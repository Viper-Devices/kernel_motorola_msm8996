/*
 * Main bcache entry point - handle a read or a write request and decide what to
 * do with it; the make_request functions are called by the block layer.
 *
 * Copyright 2010, 2011 Kent Overstreet <kent.overstreet@gmail.com>
 * Copyright 2012 Google, Inc.
 */

#include "bcache.h"
#include "btree.h"
#include "debug.h"
#include "request.h"
#include "writeback.h"

#include <linux/cgroup.h>
#include <linux/module.h>
#include <linux/hash.h>
#include <linux/random.h>
#include "blk-cgroup.h"

#include <trace/events/bcache.h>

#define CUTOFF_CACHE_ADD	95
#define CUTOFF_CACHE_READA	90

struct kmem_cache *bch_search_cache;

static void bch_data_insert_start(struct closure *);

/* Cgroup interface */

#ifdef CONFIG_CGROUP_BCACHE
static struct bch_cgroup bcache_default_cgroup = { .cache_mode = -1 };

static struct bch_cgroup *cgroup_to_bcache(struct cgroup *cgroup)
{
	struct cgroup_subsys_state *css;
	return cgroup &&
		(css = cgroup_subsys_state(cgroup, bcache_subsys_id))
		? container_of(css, struct bch_cgroup, css)
		: &bcache_default_cgroup;
}

struct bch_cgroup *bch_bio_to_cgroup(struct bio *bio)
{
	struct cgroup_subsys_state *css = bio->bi_css
		? cgroup_subsys_state(bio->bi_css->cgroup, bcache_subsys_id)
		: task_subsys_state(current, bcache_subsys_id);

	return css
		? container_of(css, struct bch_cgroup, css)
		: &bcache_default_cgroup;
}

static ssize_t cache_mode_read(struct cgroup *cgrp, struct cftype *cft,
			struct file *file,
			char __user *buf, size_t nbytes, loff_t *ppos)
{
	char tmp[1024];
	int len = bch_snprint_string_list(tmp, PAGE_SIZE, bch_cache_modes,
					  cgroup_to_bcache(cgrp)->cache_mode + 1);

	if (len < 0)
		return len;

	return simple_read_from_buffer(buf, nbytes, ppos, tmp, len);
}

static int cache_mode_write(struct cgroup *cgrp, struct cftype *cft,
			    const char *buf)
{
	int v = bch_read_string_list(buf, bch_cache_modes);
	if (v < 0)
		return v;

	cgroup_to_bcache(cgrp)->cache_mode = v - 1;
	return 0;
}

static u64 bch_verify_read(struct cgroup *cgrp, struct cftype *cft)
{
	return cgroup_to_bcache(cgrp)->verify;
}

static int bch_verify_write(struct cgroup *cgrp, struct cftype *cft, u64 val)
{
	cgroup_to_bcache(cgrp)->verify = val;
	return 0;
}

static u64 bch_cache_hits_read(struct cgroup *cgrp, struct cftype *cft)
{
	struct bch_cgroup *bcachecg = cgroup_to_bcache(cgrp);
	return atomic_read(&bcachecg->stats.cache_hits);
}

static u64 bch_cache_misses_read(struct cgroup *cgrp, struct cftype *cft)
{
	struct bch_cgroup *bcachecg = cgroup_to_bcache(cgrp);
	return atomic_read(&bcachecg->stats.cache_misses);
}

static u64 bch_cache_bypass_hits_read(struct cgroup *cgrp,
					 struct cftype *cft)
{
	struct bch_cgroup *bcachecg = cgroup_to_bcache(cgrp);
	return atomic_read(&bcachecg->stats.cache_bypass_hits);
}

static u64 bch_cache_bypass_misses_read(struct cgroup *cgrp,
					   struct cftype *cft)
{
	struct bch_cgroup *bcachecg = cgroup_to_bcache(cgrp);
	return atomic_read(&bcachecg->stats.cache_bypass_misses);
}

static struct cftype bch_files[] = {
	{
		.name		= "cache_mode",
		.read		= cache_mode_read,
		.write_string	= cache_mode_write,
	},
	{
		.name		= "verify",
		.read_u64	= bch_verify_read,
		.write_u64	= bch_verify_write,
	},
	{
		.name		= "cache_hits",
		.read_u64	= bch_cache_hits_read,
	},
	{
		.name		= "cache_misses",
		.read_u64	= bch_cache_misses_read,
	},
	{
		.name		= "cache_bypass_hits",
		.read_u64	= bch_cache_bypass_hits_read,
	},
	{
		.name		= "cache_bypass_misses",
		.read_u64	= bch_cache_bypass_misses_read,
	},
	{ }	/* terminate */
};

static void init_bch_cgroup(struct bch_cgroup *cg)
{
	cg->cache_mode = -1;
}

static struct cgroup_subsys_state *bcachecg_create(struct cgroup *cgroup)
{
	struct bch_cgroup *cg;

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);
	init_bch_cgroup(cg);
	return &cg->css;
}

static void bcachecg_destroy(struct cgroup *cgroup)
{
	struct bch_cgroup *cg = cgroup_to_bcache(cgroup);
	free_css_id(&bcache_subsys, &cg->css);
	kfree(cg);
}

struct cgroup_subsys bcache_subsys = {
	.create		= bcachecg_create,
	.destroy	= bcachecg_destroy,
	.subsys_id	= bcache_subsys_id,
	.name		= "bcache",
	.module		= THIS_MODULE,
};
EXPORT_SYMBOL_GPL(bcache_subsys);
#endif

static unsigned cache_mode(struct cached_dev *dc, struct bio *bio)
{
#ifdef CONFIG_CGROUP_BCACHE
	int r = bch_bio_to_cgroup(bio)->cache_mode;
	if (r >= 0)
		return r;
#endif
	return BDEV_CACHE_MODE(&dc->sb);
}

static bool verify(struct cached_dev *dc, struct bio *bio)
{
#ifdef CONFIG_CGROUP_BCACHE
	if (bch_bio_to_cgroup(bio)->verify)
		return true;
#endif
	return dc->verify;
}

static void bio_csum(struct bio *bio, struct bkey *k)
{
	struct bio_vec *bv;
	uint64_t csum = 0;
	int i;

	bio_for_each_segment(bv, bio, i) {
		void *d = kmap(bv->bv_page) + bv->bv_offset;
		csum = bch_crc64_update(csum, d, bv->bv_len);
		kunmap(bv->bv_page);
	}

	k->ptr[KEY_PTRS(k)] = csum & (~0ULL >> 1);
}

/* Insert data into cache */

static void bch_data_insert_keys(struct closure *cl)
{
	struct data_insert_op *op = container_of(cl, struct data_insert_op, cl);
	atomic_t *journal_ref = NULL;
	struct bkey *replace_key = op->replace ? &op->replace_key : NULL;
	int ret;

	/*
	 * If we're looping, might already be waiting on
	 * another journal write - can't wait on more than one journal write at
	 * a time
	 *
	 * XXX: this looks wrong
	 */
#if 0
	while (atomic_read(&s->cl.remaining) & CLOSURE_WAITING)
		closure_sync(&s->cl);
#endif

	if (!op->replace)
		journal_ref = bch_journal(op->c, &op->insert_keys,
					  op->flush_journal ? cl : NULL);

	ret = bch_btree_insert(op->c, &op->insert_keys,
			       journal_ref, replace_key);
	if (ret == -ESRCH) {
		op->replace_collision = true;
	} else if (ret) {
		op->error		= -ENOMEM;
		op->insert_data_done	= true;
	}

	if (journal_ref)
		atomic_dec_bug(journal_ref);

	if (!op->insert_data_done)
		continue_at(cl, bch_data_insert_start, bcache_wq);

	bch_keylist_free(&op->insert_keys);
	closure_return(cl);
}

static void bch_data_invalidate(struct closure *cl)
{
	struct data_insert_op *op = container_of(cl, struct data_insert_op, cl);
	struct bio *bio = op->bio;

	pr_debug("invalidating %i sectors from %llu",
		 bio_sectors(bio), (uint64_t) bio->bi_iter.bi_sector);

	while (bio_sectors(bio)) {
		unsigned sectors = min(bio_sectors(bio),
				       1U << (KEY_SIZE_BITS - 1));

		if (bch_keylist_realloc(&op->insert_keys, 0, op->c))
			goto out;

		bio->bi_iter.bi_sector	+= sectors;
		bio->bi_iter.bi_size	-= sectors << 9;

		bch_keylist_add(&op->insert_keys,
				&KEY(op->inode, bio->bi_iter.bi_sector, sectors));
	}

	op->insert_data_done = true;
	bio_put(bio);
out:
	continue_at(cl, bch_data_insert_keys, bcache_wq);
}

static void bch_data_insert_error(struct closure *cl)
{
	struct data_insert_op *op = container_of(cl, struct data_insert_op, cl);

	/*
	 * Our data write just errored, which means we've got a bunch of keys to
	 * insert that point to data that wasn't succesfully written.
	 *
	 * We don't have to insert those keys but we still have to invalidate
	 * that region of the cache - so, if we just strip off all the pointers
	 * from the keys we'll accomplish just that.
	 */

	struct bkey *src = op->insert_keys.keys, *dst = op->insert_keys.keys;

	while (src != op->insert_keys.top) {
		struct bkey *n = bkey_next(src);

		SET_KEY_PTRS(src, 0);
		memmove(dst, src, bkey_bytes(src));

		dst = bkey_next(dst);
		src = n;
	}

	op->insert_keys.top = dst;

	bch_data_insert_keys(cl);
}

static void bch_data_insert_endio(struct bio *bio, int error)
{
	struct closure *cl = bio->bi_private;
	struct data_insert_op *op = container_of(cl, struct data_insert_op, cl);

	if (error) {
		/* TODO: We could try to recover from this. */
		if (op->writeback)
			op->error = error;
		else if (!op->replace)
			set_closure_fn(cl, bch_data_insert_error, bcache_wq);
		else
			set_closure_fn(cl, NULL, NULL);
	}

	bch_bbio_endio(op->c, bio, error, "writing data to cache");
}

static void bch_data_insert_start(struct closure *cl)
{
	struct data_insert_op *op = container_of(cl, struct data_insert_op, cl);
	struct bio *bio = op->bio, *n;

	if (op->bypass)
		return bch_data_invalidate(cl);

	if (atomic_sub_return(bio_sectors(bio), &op->c->sectors_to_gc) < 0) {
		set_gc_sectors(op->c);
		wake_up_gc(op->c);
	}

	/*
	 * Journal writes are marked REQ_FLUSH; if the original write was a
	 * flush, it'll wait on the journal write.
	 */
	bio->bi_rw &= ~(REQ_FLUSH|REQ_FUA);

	do {
		unsigned i;
		struct bkey *k;
		struct bio_set *split = op->c->bio_split;

		/* 1 for the device pointer and 1 for the chksum */
		if (bch_keylist_realloc(&op->insert_keys,
					1 + (op->csum ? 1 : 0),
					op->c))
			continue_at(cl, bch_data_insert_keys, bcache_wq);

		k = op->insert_keys.top;
		bkey_init(k);
		SET_KEY_INODE(k, op->inode);
		SET_KEY_OFFSET(k, bio->bi_iter.bi_sector);

		if (!bch_alloc_sectors(op->c, k, bio_sectors(bio),
				       op->write_point, op->write_prio,
				       op->writeback))
			goto err;

		n = bch_bio_split(bio, KEY_SIZE(k), GFP_NOIO, split);

		n->bi_end_io	= bch_data_insert_endio;
		n->bi_private	= cl;

		if (op->writeback) {
			SET_KEY_DIRTY(k, true);

			for (i = 0; i < KEY_PTRS(k); i++)
				SET_GC_MARK(PTR_BUCKET(op->c, k, i),
					    GC_MARK_DIRTY);
		}

		SET_KEY_CSUM(k, op->csum);
		if (KEY_CSUM(k))
			bio_csum(n, k);

		trace_bcache_cache_insert(k);
		bch_keylist_push(&op->insert_keys);

		n->bi_rw |= REQ_WRITE;
		bch_submit_bbio(n, op->c, k, 0);
	} while (n != bio);

	op->insert_data_done = true;
	continue_at(cl, bch_data_insert_keys, bcache_wq);
err:
	/* bch_alloc_sectors() blocks if s->writeback = true */
	BUG_ON(op->writeback);

	/*
	 * But if it's not a writeback write we'd rather just bail out if
	 * there aren't any buckets ready to write to - it might take awhile and
	 * we might be starving btree writes for gc or something.
	 */

	if (!op->replace) {
		/*
		 * Writethrough write: We can't complete the write until we've
		 * updated the index. But we don't want to delay the write while
		 * we wait for buckets to be freed up, so just invalidate the
		 * rest of the write.
		 */
		op->bypass = true;
		return bch_data_invalidate(cl);
	} else {
		/*
		 * From a cache miss, we can just insert the keys for the data
		 * we have written or bail out if we didn't do anything.
		 */
		op->insert_data_done = true;
		bio_put(bio);

		if (!bch_keylist_empty(&op->insert_keys))
			continue_at(cl, bch_data_insert_keys, bcache_wq);
		else
			closure_return(cl);
	}
}

/**
 * bch_data_insert - stick some data in the cache
 *
 * This is the starting point for any data to end up in a cache device; it could
 * be from a normal write, or a writeback write, or a write to a flash only
 * volume - it's also used by the moving garbage collector to compact data in
 * mostly empty buckets.
 *
 * It first writes the data to the cache, creating a list of keys to be inserted
 * (if the data had to be fragmented there will be multiple keys); after the
 * data is written it calls bch_journal, and after the keys have been added to
 * the next journal write they're inserted into the btree.
 *
 * It inserts the data in s->cache_bio; bi_sector is used for the key offset,
 * and op->inode is used for the key inode.
 *
 * If s->bypass is true, instead of inserting the data it invalidates the
 * region of the cache represented by s->cache_bio and op->inode.
 */
void bch_data_insert(struct closure *cl)
{
	struct data_insert_op *op = container_of(cl, struct data_insert_op, cl);

	trace_bcache_write(op->bio, op->writeback, op->bypass);

	bch_keylist_init(&op->insert_keys);
	bio_get(op->bio);
	bch_data_insert_start(cl);
}

/* Congested? */

unsigned bch_get_congested(struct cache_set *c)
{
	int i;
	long rand;

	if (!c->congested_read_threshold_us &&
	    !c->congested_write_threshold_us)
		return 0;

	i = (local_clock_us() - c->congested_last_us) / 1024;
	if (i < 0)
		return 0;

	i += atomic_read(&c->congested);
	if (i >= 0)
		return 0;

	i += CONGESTED_MAX;

	if (i > 0)
		i = fract_exp_two(i, 6);

	rand = get_random_int();
	i -= bitmap_weight(&rand, BITS_PER_LONG);

	return i > 0 ? i : 1;
}

static void add_sequential(struct task_struct *t)
{
	ewma_add(t->sequential_io_avg,
		 t->sequential_io, 8, 0);

	t->sequential_io = 0;
}

static struct hlist_head *iohash(struct cached_dev *dc, uint64_t k)
{
	return &dc->io_hash[hash_64(k, RECENT_IO_BITS)];
}

static bool check_should_bypass(struct cached_dev *dc, struct bio *bio)
{
	struct cache_set *c = dc->disk.c;
	unsigned mode = cache_mode(dc, bio);
	unsigned sectors, congested = bch_get_congested(c);
	struct task_struct *task = current;
	struct io *i;

	if (test_bit(BCACHE_DEV_DETACHING, &dc->disk.flags) ||
	    c->gc_stats.in_use > CUTOFF_CACHE_ADD ||
	    (bio->bi_rw & REQ_DISCARD))
		goto skip;

	if (mode == CACHE_MODE_NONE ||
	    (mode == CACHE_MODE_WRITEAROUND &&
	     (bio->bi_rw & REQ_WRITE)))
		goto skip;

	if (bio->bi_iter.bi_sector & (c->sb.block_size - 1) ||
	    bio_sectors(bio) & (c->sb.block_size - 1)) {
		pr_debug("skipping unaligned io");
		goto skip;
	}

	if (bypass_torture_test(dc)) {
		if ((get_random_int() & 3) == 3)
			goto skip;
		else
			goto rescale;
	}

	if (!congested && !dc->sequential_cutoff)
		goto rescale;

	if (!congested &&
	    mode == CACHE_MODE_WRITEBACK &&
	    (bio->bi_rw & REQ_WRITE) &&
	    (bio->bi_rw & REQ_SYNC))
		goto rescale;

	spin_lock(&dc->io_lock);

	hlist_for_each_entry(i, iohash(dc, bio->bi_iter.bi_sector), hash)
		if (i->last == bio->bi_iter.bi_sector &&
		    time_before(jiffies, i->jiffies))
			goto found;

	i = list_first_entry(&dc->io_lru, struct io, lru);

	add_sequential(task);
	i->sequential = 0;
found:
	if (i->sequential + bio->bi_iter.bi_size > i->sequential)
		i->sequential	+= bio->bi_iter.bi_size;

	i->last			 = bio_end_sector(bio);
	i->jiffies		 = jiffies + msecs_to_jiffies(5000);
	task->sequential_io	 = i->sequential;

	hlist_del(&i->hash);
	hlist_add_head(&i->hash, iohash(dc, i->last));
	list_move_tail(&i->lru, &dc->io_lru);

	spin_unlock(&dc->io_lock);

	sectors = max(task->sequential_io,
		      task->sequential_io_avg) >> 9;

	if (dc->sequential_cutoff &&
	    sectors >= dc->sequential_cutoff >> 9) {
		trace_bcache_bypass_sequential(bio);
		goto skip;
	}

	if (congested && sectors >= congested) {
		trace_bcache_bypass_congested(bio);
		goto skip;
	}

rescale:
	bch_rescale_priorities(c, bio_sectors(bio));
	return false;
skip:
	bch_mark_sectors_bypassed(c, dc, bio_sectors(bio));
	return true;
}

/* Cache lookup */

struct search {
	/* Stack frame for bio_complete */
	struct closure		cl;

	struct bcache_device	*d;

	struct bbio		bio;
	struct bio		*orig_bio;
	struct bio		*cache_miss;

	unsigned		insert_bio_sectors;

	unsigned		recoverable:1;
	unsigned		write:1;
	unsigned		read_dirty_data:1;

	unsigned long		start_time;

	struct btree_op		op;
	struct data_insert_op	iop;
	struct bio_vec		bv[BIO_MAX_PAGES];
};

static void bch_cache_read_endio(struct bio *bio, int error)
{
	struct bbio *b = container_of(bio, struct bbio, bio);
	struct closure *cl = bio->bi_private;
	struct search *s = container_of(cl, struct search, cl);

	/*
	 * If the bucket was reused while our bio was in flight, we might have
	 * read the wrong data. Set s->error but not error so it doesn't get
	 * counted against the cache device, but we'll still reread the data
	 * from the backing device.
	 */

	if (error)
		s->iop.error = error;
	else if (ptr_stale(s->iop.c, &b->key, 0)) {
		atomic_long_inc(&s->iop.c->cache_read_races);
		s->iop.error = -EINTR;
	}

	bch_bbio_endio(s->iop.c, bio, error, "reading from cache");
}

/*
 * Read from a single key, handling the initial cache miss if the key starts in
 * the middle of the bio
 */
static int cache_lookup_fn(struct btree_op *op, struct btree *b, struct bkey *k)
{
	struct search *s = container_of(op, struct search, op);
	struct bio *n, *bio = &s->bio.bio;
	struct bkey *bio_key;
	unsigned ptr;

	if (bkey_cmp(k, &KEY(s->iop.inode, bio->bi_iter.bi_sector, 0)) <= 0)
		return MAP_CONTINUE;

	if (KEY_INODE(k) != s->iop.inode ||
	    KEY_START(k) > bio->bi_iter.bi_sector) {
		unsigned bio_sectors = bio_sectors(bio);
		unsigned sectors = KEY_INODE(k) == s->iop.inode
			? min_t(uint64_t, INT_MAX,
				KEY_START(k) - bio->bi_iter.bi_sector)
			: INT_MAX;

		int ret = s->d->cache_miss(b, s, bio, sectors);
		if (ret != MAP_CONTINUE)
			return ret;

		/* if this was a complete miss we shouldn't get here */
		BUG_ON(bio_sectors <= sectors);
	}

	if (!KEY_SIZE(k))
		return MAP_CONTINUE;

	/* XXX: figure out best pointer - for multiple cache devices */
	ptr = 0;

	PTR_BUCKET(b->c, k, ptr)->prio = INITIAL_PRIO;

	if (KEY_DIRTY(k))
		s->read_dirty_data = true;

	n = bch_bio_split(bio, min_t(uint64_t, INT_MAX,
				     KEY_OFFSET(k) - bio->bi_iter.bi_sector),
			  GFP_NOIO, s->d->bio_split);

	bio_key = &container_of(n, struct bbio, bio)->key;
	bch_bkey_copy_single_ptr(bio_key, k, ptr);

	bch_cut_front(&KEY(s->iop.inode, n->bi_iter.bi_sector, 0), bio_key);
	bch_cut_back(&KEY(s->iop.inode, bio_end_sector(n), 0), bio_key);

	n->bi_end_io	= bch_cache_read_endio;
	n->bi_private	= &s->cl;

	/*
	 * The bucket we're reading from might be reused while our bio
	 * is in flight, and we could then end up reading the wrong
	 * data.
	 *
	 * We guard against this by checking (in cache_read_endio()) if
	 * the pointer is stale again; if so, we treat it as an error
	 * and reread from the backing device (but we don't pass that
	 * error up anywhere).
	 */

	__bch_submit_bbio(n, b->c);
	return n == bio ? MAP_DONE : MAP_CONTINUE;
}

static void cache_lookup(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, iop.cl);
	struct bio *bio = &s->bio.bio;

	int ret = bch_btree_map_keys(&s->op, s->iop.c,
				     &KEY(s->iop.inode, bio->bi_iter.bi_sector, 0),
				     cache_lookup_fn, MAP_END_KEY);
	if (ret == -EAGAIN)
		continue_at(cl, cache_lookup, bcache_wq);

	closure_return(cl);
}

/* Common code for the make_request functions */

static void request_endio(struct bio *bio, int error)
{
	struct closure *cl = bio->bi_private;

	if (error) {
		struct search *s = container_of(cl, struct search, cl);
		s->iop.error = error;
		/* Only cache read errors are recoverable */
		s->recoverable = false;
	}

	bio_put(bio);
	closure_put(cl);
}

static void bio_complete(struct search *s)
{
	if (s->orig_bio) {
		int cpu, rw = bio_data_dir(s->orig_bio);
		unsigned long duration = jiffies - s->start_time;

		cpu = part_stat_lock();
		part_round_stats(cpu, &s->d->disk->part0);
		part_stat_add(cpu, &s->d->disk->part0, ticks[rw], duration);
		part_stat_unlock();

		trace_bcache_request_end(s->d, s->orig_bio);
		bio_endio(s->orig_bio, s->iop.error);
		s->orig_bio = NULL;
	}
}

static void do_bio_hook(struct search *s)
{
	struct bio *bio = &s->bio.bio;

	bio_init(bio);
	bio->bi_io_vec		= s->bv;
	bio->bi_max_vecs	= BIO_MAX_PAGES;
	__bio_clone(bio, s->orig_bio);
	bio->bi_end_io		= request_endio;
	bio->bi_private		= &s->cl;

	atomic_set(&bio->bi_cnt, 3);
}

static void search_free(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);
	bio_complete(s);

	if (s->iop.bio)
		bio_put(s->iop.bio);

	closure_debug_destroy(cl);
	mempool_free(s, s->d->c->search);
}

static struct search *search_alloc(struct bio *bio, struct bcache_device *d)
{
	struct search *s;

	s = mempool_alloc(d->c->search, GFP_NOIO);
	memset(s, 0, offsetof(struct search, iop.insert_keys));

	__closure_init(&s->cl, NULL);

	s->iop.inode		= d->id;
	s->iop.c		= d->c;
	s->d			= d;
	s->op.lock		= -1;
	s->iop.write_point	= hash_long((unsigned long) current, 16);
	s->orig_bio		= bio;
	s->write		= (bio->bi_rw & REQ_WRITE) != 0;
	s->iop.flush_journal	= (bio->bi_rw & (REQ_FLUSH|REQ_FUA)) != 0;
	s->recoverable		= 1;
	s->start_time		= jiffies;
	do_bio_hook(s);

	return s;
}

/* Cached devices */

static void cached_dev_bio_complete(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);
	struct cached_dev *dc = container_of(s->d, struct cached_dev, disk);

	search_free(cl);
	cached_dev_put(dc);
}

/* Process reads */

static void cached_dev_cache_miss_done(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);

	if (s->iop.replace_collision)
		bch_mark_cache_miss_collision(s->iop.c, s->d);

	if (s->iop.bio) {
		int i;
		struct bio_vec *bv;

		bio_for_each_segment_all(bv, s->iop.bio, i)
			__free_page(bv->bv_page);
	}

	cached_dev_bio_complete(cl);
}

static void cached_dev_read_error(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);
	struct bio *bio = &s->bio.bio;

	if (s->recoverable) {
		/* Retry from the backing device: */
		trace_bcache_read_retry(s->orig_bio);

		s->iop.error = 0;
		do_bio_hook(s);

		/* XXX: invalidate cache */

		closure_bio_submit(bio, cl, s->d);
	}

	continue_at(cl, cached_dev_cache_miss_done, NULL);
}

static void cached_dev_read_done(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);
	struct cached_dev *dc = container_of(s->d, struct cached_dev, disk);

	/*
	 * We had a cache miss; cache_bio now contains data ready to be inserted
	 * into the cache.
	 *
	 * First, we copy the data we just read from cache_bio's bounce buffers
	 * to the buffers the original bio pointed to:
	 */

	if (s->iop.bio) {
		bio_reset(s->iop.bio);
		s->iop.bio->bi_iter.bi_sector = s->cache_miss->bi_iter.bi_sector;
		s->iop.bio->bi_bdev = s->cache_miss->bi_bdev;
		s->iop.bio->bi_iter.bi_size = s->insert_bio_sectors << 9;
		bch_bio_map(s->iop.bio, NULL);

		bio_copy_data(s->cache_miss, s->iop.bio);

		bio_put(s->cache_miss);
		s->cache_miss = NULL;
	}

	if (verify(dc, &s->bio.bio) && s->recoverable && !s->read_dirty_data)
		bch_data_verify(dc, s->orig_bio);

	bio_complete(s);

	if (s->iop.bio &&
	    !test_bit(CACHE_SET_STOPPING, &s->iop.c->flags)) {
		BUG_ON(!s->iop.replace);
		closure_call(&s->iop.cl, bch_data_insert, NULL, cl);
	}

	continue_at(cl, cached_dev_cache_miss_done, NULL);
}

static void cached_dev_read_done_bh(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);
	struct cached_dev *dc = container_of(s->d, struct cached_dev, disk);

	bch_mark_cache_accounting(s->iop.c, s->d,
				  !s->cache_miss, s->iop.bypass);
	trace_bcache_read(s->orig_bio, !s->cache_miss, s->iop.bypass);

	if (s->iop.error)
		continue_at_nobarrier(cl, cached_dev_read_error, bcache_wq);
	else if (s->iop.bio || verify(dc, &s->bio.bio))
		continue_at_nobarrier(cl, cached_dev_read_done, bcache_wq);
	else
		continue_at_nobarrier(cl, cached_dev_bio_complete, NULL);
}

static int cached_dev_cache_miss(struct btree *b, struct search *s,
				 struct bio *bio, unsigned sectors)
{
	int ret = MAP_CONTINUE;
	unsigned reada = 0;
	struct cached_dev *dc = container_of(s->d, struct cached_dev, disk);
	struct bio *miss, *cache_bio;

	if (s->cache_miss || s->iop.bypass) {
		miss = bch_bio_split(bio, sectors, GFP_NOIO, s->d->bio_split);
		ret = miss == bio ? MAP_DONE : MAP_CONTINUE;
		goto out_submit;
	}

	if (!(bio->bi_rw & REQ_RAHEAD) &&
	    !(bio->bi_rw & REQ_META) &&
	    s->iop.c->gc_stats.in_use < CUTOFF_CACHE_READA)
		reada = min_t(sector_t, dc->readahead >> 9,
			      bdev_sectors(bio->bi_bdev) - bio_end_sector(bio));

	s->insert_bio_sectors = min(sectors, bio_sectors(bio) + reada);

	s->iop.replace_key = KEY(s->iop.inode,
				 bio->bi_iter.bi_sector + s->insert_bio_sectors,
				 s->insert_bio_sectors);

	ret = bch_btree_insert_check_key(b, &s->op, &s->iop.replace_key);
	if (ret)
		return ret;

	s->iop.replace = true;

	miss = bch_bio_split(bio, sectors, GFP_NOIO, s->d->bio_split);

	/* btree_search_recurse()'s btree iterator is no good anymore */
	ret = miss == bio ? MAP_DONE : -EINTR;

	cache_bio = bio_alloc_bioset(GFP_NOWAIT,
			DIV_ROUND_UP(s->insert_bio_sectors, PAGE_SECTORS),
			dc->disk.bio_split);
	if (!cache_bio)
		goto out_submit;

	cache_bio->bi_iter.bi_sector	= miss->bi_iter.bi_sector;
	cache_bio->bi_bdev		= miss->bi_bdev;
	cache_bio->bi_iter.bi_size	= s->insert_bio_sectors << 9;

	cache_bio->bi_end_io	= request_endio;
	cache_bio->bi_private	= &s->cl;

	bch_bio_map(cache_bio, NULL);
	if (bio_alloc_pages(cache_bio, __GFP_NOWARN|GFP_NOIO))
		goto out_put;

	if (reada)
		bch_mark_cache_readahead(s->iop.c, s->d);

	s->cache_miss	= miss;
	s->iop.bio	= cache_bio;
	bio_get(cache_bio);
	closure_bio_submit(cache_bio, &s->cl, s->d);

	return ret;
out_put:
	bio_put(cache_bio);
out_submit:
	miss->bi_end_io		= request_endio;
	miss->bi_private	= &s->cl;
	closure_bio_submit(miss, &s->cl, s->d);
	return ret;
}

static void cached_dev_read(struct cached_dev *dc, struct search *s)
{
	struct closure *cl = &s->cl;

	closure_call(&s->iop.cl, cache_lookup, NULL, cl);
	continue_at(cl, cached_dev_read_done_bh, NULL);
}

/* Process writes */

static void cached_dev_write_complete(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);
	struct cached_dev *dc = container_of(s->d, struct cached_dev, disk);

	up_read_non_owner(&dc->writeback_lock);
	cached_dev_bio_complete(cl);
}

static void cached_dev_write(struct cached_dev *dc, struct search *s)
{
	struct closure *cl = &s->cl;
	struct bio *bio = &s->bio.bio;
	struct bkey start = KEY(dc->disk.id, bio->bi_iter.bi_sector, 0);
	struct bkey end = KEY(dc->disk.id, bio_end_sector(bio), 0);

	bch_keybuf_check_overlapping(&s->iop.c->moving_gc_keys, &start, &end);

	down_read_non_owner(&dc->writeback_lock);
	if (bch_keybuf_check_overlapping(&dc->writeback_keys, &start, &end)) {
		/*
		 * We overlap with some dirty data undergoing background
		 * writeback, force this write to writeback
		 */
		s->iop.bypass = false;
		s->iop.writeback = true;
	}

	/*
	 * Discards aren't _required_ to do anything, so skipping if
	 * check_overlapping returned true is ok
	 *
	 * But check_overlapping drops dirty keys for which io hasn't started,
	 * so we still want to call it.
	 */
	if (bio->bi_rw & REQ_DISCARD)
		s->iop.bypass = true;

	if (should_writeback(dc, s->orig_bio,
			     cache_mode(dc, bio),
			     s->iop.bypass)) {
		s->iop.bypass = false;
		s->iop.writeback = true;
	}

	if (s->iop.bypass) {
		s->iop.bio = s->orig_bio;
		bio_get(s->iop.bio);

		if (!(bio->bi_rw & REQ_DISCARD) ||
		    blk_queue_discard(bdev_get_queue(dc->bdev)))
			closure_bio_submit(bio, cl, s->d);
	} else if (s->iop.writeback) {
		bch_writeback_add(dc);
		s->iop.bio = bio;

		if (bio->bi_rw & REQ_FLUSH) {
			/* Also need to send a flush to the backing device */
			struct bio *flush = bio_alloc_bioset(GFP_NOIO, 0,
							     dc->disk.bio_split);

			flush->bi_rw	= WRITE_FLUSH;
			flush->bi_bdev	= bio->bi_bdev;
			flush->bi_end_io = request_endio;
			flush->bi_private = cl;

			closure_bio_submit(flush, cl, s->d);
		}
	} else {
		s->iop.bio = bio_clone_bioset(bio, GFP_NOIO,
					      dc->disk.bio_split);

		closure_bio_submit(bio, cl, s->d);
	}

	closure_call(&s->iop.cl, bch_data_insert, NULL, cl);
	continue_at(cl, cached_dev_write_complete, NULL);
}

static void cached_dev_nodata(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);
	struct bio *bio = &s->bio.bio;

	if (s->iop.flush_journal)
		bch_journal_meta(s->iop.c, cl);

	/* If it's a flush, we send the flush to the backing device too */
	closure_bio_submit(bio, cl, s->d);

	continue_at(cl, cached_dev_bio_complete, NULL);
}

/* Cached devices - read & write stuff */

static void cached_dev_make_request(struct request_queue *q, struct bio *bio)
{
	struct search *s;
	struct bcache_device *d = bio->bi_bdev->bd_disk->private_data;
	struct cached_dev *dc = container_of(d, struct cached_dev, disk);
	int cpu, rw = bio_data_dir(bio);

	cpu = part_stat_lock();
	part_stat_inc(cpu, &d->disk->part0, ios[rw]);
	part_stat_add(cpu, &d->disk->part0, sectors[rw], bio_sectors(bio));
	part_stat_unlock();

	bio->bi_bdev = dc->bdev;
	bio->bi_iter.bi_sector += dc->sb.data_offset;

	if (cached_dev_get(dc)) {
		s = search_alloc(bio, d);
		trace_bcache_request_start(s->d, bio);

		if (!bio->bi_iter.bi_size) {
			/*
			 * can't call bch_journal_meta from under
			 * generic_make_request
			 */
			continue_at_nobarrier(&s->cl,
					      cached_dev_nodata,
					      bcache_wq);
		} else {
			s->iop.bypass = check_should_bypass(dc, bio);

			if (rw)
				cached_dev_write(dc, s);
			else
				cached_dev_read(dc, s);
		}
	} else {
		if ((bio->bi_rw & REQ_DISCARD) &&
		    !blk_queue_discard(bdev_get_queue(dc->bdev)))
			bio_endio(bio, 0);
		else
			bch_generic_make_request(bio, &d->bio_split_hook);
	}
}

static int cached_dev_ioctl(struct bcache_device *d, fmode_t mode,
			    unsigned int cmd, unsigned long arg)
{
	struct cached_dev *dc = container_of(d, struct cached_dev, disk);
	return __blkdev_driver_ioctl(dc->bdev, mode, cmd, arg);
}

static int cached_dev_congested(void *data, int bits)
{
	struct bcache_device *d = data;
	struct cached_dev *dc = container_of(d, struct cached_dev, disk);
	struct request_queue *q = bdev_get_queue(dc->bdev);
	int ret = 0;

	if (bdi_congested(&q->backing_dev_info, bits))
		return 1;

	if (cached_dev_get(dc)) {
		unsigned i;
		struct cache *ca;

		for_each_cache(ca, d->c, i) {
			q = bdev_get_queue(ca->bdev);
			ret |= bdi_congested(&q->backing_dev_info, bits);
		}

		cached_dev_put(dc);
	}

	return ret;
}

void bch_cached_dev_request_init(struct cached_dev *dc)
{
	struct gendisk *g = dc->disk.disk;

	g->queue->make_request_fn		= cached_dev_make_request;
	g->queue->backing_dev_info.congested_fn = cached_dev_congested;
	dc->disk.cache_miss			= cached_dev_cache_miss;
	dc->disk.ioctl				= cached_dev_ioctl;
}

/* Flash backed devices */

static int flash_dev_cache_miss(struct btree *b, struct search *s,
				struct bio *bio, unsigned sectors)
{
	struct bio_vec *bv;
	int i;

	/* Zero fill bio */

	bio_for_each_segment(bv, bio, i) {
		unsigned j = min(bv->bv_len >> 9, sectors);

		void *p = kmap(bv->bv_page);
		memset(p + bv->bv_offset, 0, j << 9);
		kunmap(bv->bv_page);

		sectors	-= j;
	}

	bio_advance(bio, min(sectors << 9, bio->bi_iter.bi_size));

	if (!bio->bi_iter.bi_size)
		return MAP_DONE;

	return MAP_CONTINUE;
}

static void flash_dev_nodata(struct closure *cl)
{
	struct search *s = container_of(cl, struct search, cl);

	if (s->iop.flush_journal)
		bch_journal_meta(s->iop.c, cl);

	continue_at(cl, search_free, NULL);
}

static void flash_dev_make_request(struct request_queue *q, struct bio *bio)
{
	struct search *s;
	struct closure *cl;
	struct bcache_device *d = bio->bi_bdev->bd_disk->private_data;
	int cpu, rw = bio_data_dir(bio);

	cpu = part_stat_lock();
	part_stat_inc(cpu, &d->disk->part0, ios[rw]);
	part_stat_add(cpu, &d->disk->part0, sectors[rw], bio_sectors(bio));
	part_stat_unlock();

	s = search_alloc(bio, d);
	cl = &s->cl;
	bio = &s->bio.bio;

	trace_bcache_request_start(s->d, bio);

	if (!bio->bi_iter.bi_size) {
		/*
		 * can't call bch_journal_meta from under
		 * generic_make_request
		 */
		continue_at_nobarrier(&s->cl,
				      flash_dev_nodata,
				      bcache_wq);
	} else if (rw) {
		bch_keybuf_check_overlapping(&s->iop.c->moving_gc_keys,
					&KEY(d->id, bio->bi_iter.bi_sector, 0),
					&KEY(d->id, bio_end_sector(bio), 0));

		s->iop.bypass		= (bio->bi_rw & REQ_DISCARD) != 0;
		s->iop.writeback	= true;
		s->iop.bio		= bio;

		closure_call(&s->iop.cl, bch_data_insert, NULL, cl);
	} else {
		closure_call(&s->iop.cl, cache_lookup, NULL, cl);
	}

	continue_at(cl, search_free, NULL);
}

static int flash_dev_ioctl(struct bcache_device *d, fmode_t mode,
			   unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static int flash_dev_congested(void *data, int bits)
{
	struct bcache_device *d = data;
	struct request_queue *q;
	struct cache *ca;
	unsigned i;
	int ret = 0;

	for_each_cache(ca, d->c, i) {
		q = bdev_get_queue(ca->bdev);
		ret |= bdi_congested(&q->backing_dev_info, bits);
	}

	return ret;
}

void bch_flash_dev_request_init(struct bcache_device *d)
{
	struct gendisk *g = d->disk;

	g->queue->make_request_fn		= flash_dev_make_request;
	g->queue->backing_dev_info.congested_fn = flash_dev_congested;
	d->cache_miss				= flash_dev_cache_miss;
	d->ioctl				= flash_dev_ioctl;
}

void bch_request_exit(void)
{
#ifdef CONFIG_CGROUP_BCACHE
	cgroup_unload_subsys(&bcache_subsys);
#endif
	if (bch_search_cache)
		kmem_cache_destroy(bch_search_cache);
}

int __init bch_request_init(void)
{
	bch_search_cache = KMEM_CACHE(search, 0);
	if (!bch_search_cache)
		return -ENOMEM;

#ifdef CONFIG_CGROUP_BCACHE
	cgroup_load_subsys(&bcache_subsys);
	init_bch_cgroup(&bcache_default_cgroup);

	cgroup_add_cftypes(&bcache_subsys, bch_files);
#endif
	return 0;
}
