/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This file is released under the GPL.
 */

#include "dm-cache-metadata.h"

#include "persistent-data/dm-array.h"
#include "persistent-data/dm-bitset.h"
#include "persistent-data/dm-space-map.h"
#include "persistent-data/dm-space-map-disk.h"
#include "persistent-data/dm-transaction-manager.h"

#include <linux/device-mapper.h>

/*----------------------------------------------------------------*/

#define DM_MSG_PREFIX   "cache metadata"

#define CACHE_SUPERBLOCK_MAGIC 06142003
#define CACHE_SUPERBLOCK_LOCATION 0

/*
 * defines a range of metadata versions that this module can handle.
 */
#define MIN_CACHE_VERSION 1
#define MAX_CACHE_VERSION 1

#define CACHE_METADATA_CACHE_SIZE 64

/*
 *  3 for btree insert +
 *  2 for btree lookup used within space map
 */
#define CACHE_MAX_CONCURRENT_LOCKS 5
#define SPACE_MAP_ROOT_SIZE 128

enum superblock_flag_bits {
	/* for spotting crashes that would invalidate the dirty bitset */
	CLEAN_SHUTDOWN,
};

/*
 * Each mapping from cache block -> origin block carries a set of flags.
 */
enum mapping_bits {
	/*
	 * A valid mapping.  Because we're using an array we clear this
	 * flag for an non existant mapping.
	 */
	M_VALID = 1,

	/*
	 * The data on the cache is different from that on the origin.
	 */
	M_DIRTY = 2
};

struct cache_disk_superblock {
	__le32 csum;
	__le32 flags;
	__le64 blocknr;

	__u8 uuid[16];
	__le64 magic;
	__le32 version;

	__u8 policy_name[CACHE_POLICY_NAME_SIZE];
	__le32 policy_hint_size;

	__u8 metadata_space_map_root[SPACE_MAP_ROOT_SIZE];
	__le64 mapping_root;
	__le64 hint_root;

	__le64 discard_root;
	__le64 discard_block_size;
	__le64 discard_nr_blocks;

	__le32 data_block_size;
	__le32 metadata_block_size;
	__le32 cache_blocks;

	__le32 compat_flags;
	__le32 compat_ro_flags;
	__le32 incompat_flags;

	__le32 read_hits;
	__le32 read_misses;
	__le32 write_hits;
	__le32 write_misses;

	__le32 policy_version[CACHE_POLICY_VERSION_SIZE];
} __packed;

struct dm_cache_metadata {
	struct block_device *bdev;
	struct dm_block_manager *bm;
	struct dm_space_map *metadata_sm;
	struct dm_transaction_manager *tm;

	struct dm_array_info info;
	struct dm_array_info hint_info;
	struct dm_disk_bitset discard_info;

	struct rw_semaphore root_lock;
	dm_block_t root;
	dm_block_t hint_root;
	dm_block_t discard_root;

	sector_t discard_block_size;
	dm_oblock_t discard_nr_blocks;

	sector_t data_block_size;
	dm_cblock_t cache_blocks;
	bool changed:1;
	bool clean_when_opened:1;

	char policy_name[CACHE_POLICY_NAME_SIZE];
	unsigned policy_version[CACHE_POLICY_VERSION_SIZE];
	size_t policy_hint_size;
	struct dm_cache_statistics stats;

	/*
	 * Reading the space map root can fail, so we read it into this
	 * buffer before the superblock is locked and updated.
	 */
	__u8 metadata_space_map_root[SPACE_MAP_ROOT_SIZE];
};

/*-------------------------------------------------------------------
 * superblock validator
 *-----------------------------------------------------------------*/

#define SUPERBLOCK_CSUM_XOR 9031977

static void sb_prepare_for_write(struct dm_block_validator *v,
				 struct dm_block *b,
				 size_t sb_block_size)
{
	struct cache_disk_superblock *disk_super = dm_block_data(b);

	disk_super->blocknr = cpu_to_le64(dm_block_location(b));
	disk_super->csum = cpu_to_le32(dm_bm_checksum(&disk_super->flags,
						      sb_block_size - sizeof(__le32),
						      SUPERBLOCK_CSUM_XOR));
}

static int check_metadata_version(struct cache_disk_superblock *disk_super)
{
	uint32_t metadata_version = le32_to_cpu(disk_super->version);
	if (metadata_version < MIN_CACHE_VERSION || metadata_version > MAX_CACHE_VERSION) {
		DMERR("Cache metadata version %u found, but only versions between %u and %u supported.",
		      metadata_version, MIN_CACHE_VERSION, MAX_CACHE_VERSION);
		return -EINVAL;
	}

	return 0;
}

static int sb_check(struct dm_block_validator *v,
		    struct dm_block *b,
		    size_t sb_block_size)
{
	struct cache_disk_superblock *disk_super = dm_block_data(b);
	__le32 csum_le;

	if (dm_block_location(b) != le64_to_cpu(disk_super->blocknr)) {
		DMERR("sb_check failed: blocknr %llu: wanted %llu",
		      le64_to_cpu(disk_super->blocknr),
		      (unsigned long long)dm_block_location(b));
		return -ENOTBLK;
	}

	if (le64_to_cpu(disk_super->magic) != CACHE_SUPERBLOCK_MAGIC) {
		DMERR("sb_check failed: magic %llu: wanted %llu",
		      le64_to_cpu(disk_super->magic),
		      (unsigned long long)CACHE_SUPERBLOCK_MAGIC);
		return -EILSEQ;
	}

	csum_le = cpu_to_le32(dm_bm_checksum(&disk_super->flags,
					     sb_block_size - sizeof(__le32),
					     SUPERBLOCK_CSUM_XOR));
	if (csum_le != disk_super->csum) {
		DMERR("sb_check failed: csum %u: wanted %u",
		      le32_to_cpu(csum_le), le32_to_cpu(disk_super->csum));
		return -EILSEQ;
	}

	return check_metadata_version(disk_super);
}

static struct dm_block_validator sb_validator = {
	.name = "superblock",
	.prepare_for_write = sb_prepare_for_write,
	.check = sb_check
};

/*----------------------------------------------------------------*/

static int superblock_read_lock(struct dm_cache_metadata *cmd,
				struct dm_block **sblock)
{
	return dm_bm_read_lock(cmd->bm, CACHE_SUPERBLOCK_LOCATION,
			       &sb_validator, sblock);
}

static int superblock_lock_zero(struct dm_cache_metadata *cmd,
				struct dm_block **sblock)
{
	return dm_bm_write_lock_zero(cmd->bm, CACHE_SUPERBLOCK_LOCATION,
				     &sb_validator, sblock);
}

static int superblock_lock(struct dm_cache_metadata *cmd,
			   struct dm_block **sblock)
{
	return dm_bm_write_lock(cmd->bm, CACHE_SUPERBLOCK_LOCATION,
				&sb_validator, sblock);
}

/*----------------------------------------------------------------*/

static int __superblock_all_zeroes(struct dm_block_manager *bm, bool *result)
{
	int r;
	unsigned i;
	struct dm_block *b;
	__le64 *data_le, zero = cpu_to_le64(0);
	unsigned sb_block_size = dm_bm_block_size(bm) / sizeof(__le64);

	/*
	 * We can't use a validator here - it may be all zeroes.
	 */
	r = dm_bm_read_lock(bm, CACHE_SUPERBLOCK_LOCATION, NULL, &b);
	if (r)
		return r;

	data_le = dm_block_data(b);
	*result = true;
	for (i = 0; i < sb_block_size; i++) {
		if (data_le[i] != zero) {
			*result = false;
			break;
		}
	}

	return dm_bm_unlock(b);
}

static void __setup_mapping_info(struct dm_cache_metadata *cmd)
{
	struct dm_btree_value_type vt;

	vt.context = NULL;
	vt.size = sizeof(__le64);
	vt.inc = NULL;
	vt.dec = NULL;
	vt.equal = NULL;
	dm_array_info_init(&cmd->info, cmd->tm, &vt);

	if (cmd->policy_hint_size) {
		vt.size = sizeof(__le32);
		dm_array_info_init(&cmd->hint_info, cmd->tm, &vt);
	}
}

static int __save_sm_root(struct dm_cache_metadata *cmd)
{
	int r;
	size_t metadata_len;

	r = dm_sm_root_size(cmd->metadata_sm, &metadata_len);
	if (r < 0)
		return r;

	return dm_sm_copy_root(cmd->metadata_sm, &cmd->metadata_space_map_root,
			       metadata_len);
}

static void __copy_sm_root(struct dm_cache_metadata *cmd,
			   struct cache_disk_superblock *disk_super)
{
	memcpy(&disk_super->metadata_space_map_root,
	       &cmd->metadata_space_map_root,
	       sizeof(cmd->metadata_space_map_root));
}

static int __write_initial_superblock(struct dm_cache_metadata *cmd)
{
	int r;
	struct dm_block *sblock;
	struct cache_disk_superblock *disk_super;
	sector_t bdev_size = i_size_read(cmd->bdev->bd_inode) >> SECTOR_SHIFT;

	/* FIXME: see if we can lose the max sectors limit */
	if (bdev_size > DM_CACHE_METADATA_MAX_SECTORS)
		bdev_size = DM_CACHE_METADATA_MAX_SECTORS;

	r = dm_tm_pre_commit(cmd->tm);
	if (r < 0)
		return r;

	/*
	 * dm_sm_copy_root() can fail.  So we need to do it before we start
	 * updating the superblock.
	 */
	r = __save_sm_root(cmd);
	if (r)
		return r;

	r = superblock_lock_zero(cmd, &sblock);
	if (r)
		return r;

	disk_super = dm_block_data(sblock);
	disk_super->flags = 0;
	memset(disk_super->uuid, 0, sizeof(disk_super->uuid));
	disk_super->magic = cpu_to_le64(CACHE_SUPERBLOCK_MAGIC);
	disk_super->version = cpu_to_le32(MAX_CACHE_VERSION);
	memset(disk_super->policy_name, 0, sizeof(disk_super->policy_name));
	memset(disk_super->policy_version, 0, sizeof(disk_super->policy_version));
	disk_super->policy_hint_size = 0;

	__copy_sm_root(cmd, disk_super);

	disk_super->mapping_root = cpu_to_le64(cmd->root);
	disk_super->hint_root = cpu_to_le64(cmd->hint_root);
	disk_super->discard_root = cpu_to_le64(cmd->discard_root);
	disk_super->discard_block_size = cpu_to_le64(cmd->discard_block_size);
	disk_super->discard_nr_blocks = cpu_to_le64(from_oblock(cmd->discard_nr_blocks));
	disk_super->metadata_block_size = cpu_to_le32(DM_CACHE_METADATA_BLOCK_SIZE >> SECTOR_SHIFT);
	disk_super->data_block_size = cpu_to_le32(cmd->data_block_size);
	disk_super->cache_blocks = cpu_to_le32(0);

	disk_super->read_hits = cpu_to_le32(0);
	disk_super->read_misses = cpu_to_le32(0);
	disk_super->write_hits = cpu_to_le32(0);
	disk_super->write_misses = cpu_to_le32(0);

	return dm_tm_commit(cmd->tm, sblock);
}

static int __format_metadata(struct dm_cache_metadata *cmd)
{
	int r;

	r = dm_tm_create_with_sm(cmd->bm, CACHE_SUPERBLOCK_LOCATION,
				 &cmd->tm, &cmd->metadata_sm);
	if (r < 0) {
		DMERR("tm_create_with_sm failed");
		return r;
	}

	__setup_mapping_info(cmd);

	r = dm_array_empty(&cmd->info, &cmd->root);
	if (r < 0)
		goto bad;

	dm_disk_bitset_init(cmd->tm, &cmd->discard_info);

	r = dm_bitset_empty(&cmd->discard_info, &cmd->discard_root);
	if (r < 0)
		goto bad;

	cmd->discard_block_size = 0;
	cmd->discard_nr_blocks = 0;

	r = __write_initial_superblock(cmd);
	if (r)
		goto bad;

	cmd->clean_when_opened = true;
	return 0;

bad:
	dm_tm_destroy(cmd->tm);
	dm_sm_destroy(cmd->metadata_sm);

	return r;
}

static int __check_incompat_features(struct cache_disk_superblock *disk_super,
				     struct dm_cache_metadata *cmd)
{
	uint32_t features;

	features = le32_to_cpu(disk_super->incompat_flags) & ~DM_CACHE_FEATURE_INCOMPAT_SUPP;
	if (features) {
		DMERR("could not access metadata due to unsupported optional features (%lx).",
		      (unsigned long)features);
		return -EINVAL;
	}

	/*
	 * Check for read-only metadata to skip the following RDWR checks.
	 */
	if (get_disk_ro(cmd->bdev->bd_disk))
		return 0;

	features = le32_to_cpu(disk_super->compat_ro_flags) & ~DM_CACHE_FEATURE_COMPAT_RO_SUPP;
	if (features) {
		DMERR("could not access metadata RDWR due to unsupported optional features (%lx).",
		      (unsigned long)features);
		return -EINVAL;
	}

	return 0;
}

static int __open_metadata(struct dm_cache_metadata *cmd)
{
	int r;
	struct dm_block *sblock;
	struct cache_disk_superblock *disk_super;
	unsigned long sb_flags;

	r = superblock_read_lock(cmd, &sblock);
	if (r < 0) {
		DMERR("couldn't read lock superblock");
		return r;
	}

	disk_super = dm_block_data(sblock);

	/* Verify the data block size hasn't changed */
	if (le32_to_cpu(disk_super->data_block_size) != cmd->data_block_size) {
		DMERR("changing the data block size (from %u to %llu) is not supported",
		      le32_to_cpu(disk_super->data_block_size),
		      (unsigned long long)cmd->data_block_size);
		r = -EINVAL;
		goto bad;
	}

	r = __check_incompat_features(disk_super, cmd);
	if (r < 0)
		goto bad;

	r = dm_tm_open_with_sm(cmd->bm, CACHE_SUPERBLOCK_LOCATION,
			       disk_super->metadata_space_map_root,
			       sizeof(disk_super->metadata_space_map_root),
			       &cmd->tm, &cmd->metadata_sm);
	if (r < 0) {
		DMERR("tm_open_with_sm failed");
		goto bad;
	}

	__setup_mapping_info(cmd);
	dm_disk_bitset_init(cmd->tm, &cmd->discard_info);
	sb_flags = le32_to_cpu(disk_super->flags);
	cmd->clean_when_opened = test_bit(CLEAN_SHUTDOWN, &sb_flags);
	return dm_bm_unlock(sblock);

bad:
	dm_bm_unlock(sblock);
	return r;
}

static int __open_or_format_metadata(struct dm_cache_metadata *cmd,
				     bool format_device)
{
	int r;
	bool unformatted = false;

	r = __superblock_all_zeroes(cmd->bm, &unformatted);
	if (r)
		return r;

	if (unformatted)
		return format_device ? __format_metadata(cmd) : -EPERM;

	return __open_metadata(cmd);
}

static int __create_persistent_data_objects(struct dm_cache_metadata *cmd,
					    bool may_format_device)
{
	int r;
	cmd->bm = dm_block_manager_create(cmd->bdev, DM_CACHE_METADATA_BLOCK_SIZE,
					  CACHE_METADATA_CACHE_SIZE,
					  CACHE_MAX_CONCURRENT_LOCKS);
	if (IS_ERR(cmd->bm)) {
		DMERR("could not create block manager");
		return PTR_ERR(cmd->bm);
	}

	r = __open_or_format_metadata(cmd, may_format_device);
	if (r)
		dm_block_manager_destroy(cmd->bm);

	return r;
}

static void __destroy_persistent_data_objects(struct dm_cache_metadata *cmd)
{
	dm_sm_destroy(cmd->metadata_sm);
	dm_tm_destroy(cmd->tm);
	dm_block_manager_destroy(cmd->bm);
}

typedef unsigned long (*flags_mutator)(unsigned long);

static void update_flags(struct cache_disk_superblock *disk_super,
			 flags_mutator mutator)
{
	uint32_t sb_flags = mutator(le32_to_cpu(disk_super->flags));
	disk_super->flags = cpu_to_le32(sb_flags);
}

static unsigned long set_clean_shutdown(unsigned long flags)
{
	set_bit(CLEAN_SHUTDOWN, &flags);
	return flags;
}

static unsigned long clear_clean_shutdown(unsigned long flags)
{
	clear_bit(CLEAN_SHUTDOWN, &flags);
	return flags;
}

static void read_superblock_fields(struct dm_cache_metadata *cmd,
				   struct cache_disk_superblock *disk_super)
{
	cmd->root = le64_to_cpu(disk_super->mapping_root);
	cmd->hint_root = le64_to_cpu(disk_super->hint_root);
	cmd->discard_root = le64_to_cpu(disk_super->discard_root);
	cmd->discard_block_size = le64_to_cpu(disk_super->discard_block_size);
	cmd->discard_nr_blocks = to_oblock(le64_to_cpu(disk_super->discard_nr_blocks));
	cmd->data_block_size = le32_to_cpu(disk_super->data_block_size);
	cmd->cache_blocks = to_cblock(le32_to_cpu(disk_super->cache_blocks));
	strncpy(cmd->policy_name, disk_super->policy_name, sizeof(cmd->policy_name));
	cmd->policy_version[0] = le32_to_cpu(disk_super->policy_version[0]);
	cmd->policy_version[1] = le32_to_cpu(disk_super->policy_version[1]);
	cmd->policy_version[2] = le32_to_cpu(disk_super->policy_version[2]);
	cmd->policy_hint_size = le32_to_cpu(disk_super->policy_hint_size);

	cmd->stats.read_hits = le32_to_cpu(disk_super->read_hits);
	cmd->stats.read_misses = le32_to_cpu(disk_super->read_misses);
	cmd->stats.write_hits = le32_to_cpu(disk_super->write_hits);
	cmd->stats.write_misses = le32_to_cpu(disk_super->write_misses);

	cmd->changed = false;
}

/*
 * The mutator updates the superblock flags.
 */
static int __begin_transaction_flags(struct dm_cache_metadata *cmd,
				     flags_mutator mutator)
{
	int r;
	struct cache_disk_superblock *disk_super;
	struct dm_block *sblock;

	r = superblock_lock(cmd, &sblock);
	if (r)
		return r;

	disk_super = dm_block_data(sblock);
	update_flags(disk_super, mutator);
	read_superblock_fields(cmd, disk_super);
	dm_bm_unlock(sblock);

	return dm_bm_flush(cmd->bm);
}

static int __begin_transaction(struct dm_cache_metadata *cmd)
{
	int r;
	struct cache_disk_superblock *disk_super;
	struct dm_block *sblock;

	/*
	 * We re-read the superblock every time.  Shouldn't need to do this
	 * really.
	 */
	r = superblock_read_lock(cmd, &sblock);
	if (r)
		return r;

	disk_super = dm_block_data(sblock);
	read_superblock_fields(cmd, disk_super);
	dm_bm_unlock(sblock);

	return 0;
}

static int __commit_transaction(struct dm_cache_metadata *cmd,
				flags_mutator mutator)
{
	int r;
	struct cache_disk_superblock *disk_super;
	struct dm_block *sblock;

	/*
	 * We need to know if the cache_disk_superblock exceeds a 512-byte sector.
	 */
	BUILD_BUG_ON(sizeof(struct cache_disk_superblock) > 512);

	r = dm_bitset_flush(&cmd->discard_info, cmd->discard_root,
			    &cmd->discard_root);
	if (r)
		return r;

	r = dm_tm_pre_commit(cmd->tm);
	if (r < 0)
		return r;

	r = __save_sm_root(cmd);
	if (r)
		return r;

	r = superblock_lock(cmd, &sblock);
	if (r)
		return r;

	disk_super = dm_block_data(sblock);

	if (mutator)
		update_flags(disk_super, mutator);

	disk_super->mapping_root = cpu_to_le64(cmd->root);
	disk_super->hint_root = cpu_to_le64(cmd->hint_root);
	disk_super->discard_root = cpu_to_le64(cmd->discard_root);
	disk_super->discard_block_size = cpu_to_le64(cmd->discard_block_size);
	disk_super->discard_nr_blocks = cpu_to_le64(from_oblock(cmd->discard_nr_blocks));
	disk_super->cache_blocks = cpu_to_le32(from_cblock(cmd->cache_blocks));
	strncpy(disk_super->policy_name, cmd->policy_name, sizeof(disk_super->policy_name));
	disk_super->policy_version[0] = cpu_to_le32(cmd->policy_version[0]);
	disk_super->policy_version[1] = cpu_to_le32(cmd->policy_version[1]);
	disk_super->policy_version[2] = cpu_to_le32(cmd->policy_version[2]);

	disk_super->read_hits = cpu_to_le32(cmd->stats.read_hits);
	disk_super->read_misses = cpu_to_le32(cmd->stats.read_misses);
	disk_super->write_hits = cpu_to_le32(cmd->stats.write_hits);
	disk_super->write_misses = cpu_to_le32(cmd->stats.write_misses);
	__copy_sm_root(cmd, disk_super);

	return dm_tm_commit(cmd->tm, sblock);
}

/*----------------------------------------------------------------*/

/*
 * The mappings are held in a dm-array that has 64-bit values stored in
 * little-endian format.  The index is the cblock, the high 48bits of the
 * value are the oblock and the low 16 bit the flags.
 */
#define FLAGS_MASK ((1 << 16) - 1)

static __le64 pack_value(dm_oblock_t block, unsigned flags)
{
	uint64_t value = from_oblock(block);
	value <<= 16;
	value = value | (flags & FLAGS_MASK);
	return cpu_to_le64(value);
}

static void unpack_value(__le64 value_le, dm_oblock_t *block, unsigned *flags)
{
	uint64_t value = le64_to_cpu(value_le);
	uint64_t b = value >> 16;
	*block = to_oblock(b);
	*flags = value & FLAGS_MASK;
}

/*----------------------------------------------------------------*/

struct dm_cache_metadata *dm_cache_metadata_open(struct block_device *bdev,
						 sector_t data_block_size,
						 bool may_format_device,
						 size_t policy_hint_size)
{
	int r;
	struct dm_cache_metadata *cmd;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		DMERR("could not allocate metadata struct");
		return NULL;
	}

	init_rwsem(&cmd->root_lock);
	cmd->bdev = bdev;
	cmd->data_block_size = data_block_size;
	cmd->cache_blocks = 0;
	cmd->policy_hint_size = policy_hint_size;
	cmd->changed = true;

	r = __create_persistent_data_objects(cmd, may_format_device);
	if (r) {
		kfree(cmd);
		return ERR_PTR(r);
	}

	r = __begin_transaction_flags(cmd, clear_clean_shutdown);
	if (r < 0) {
		dm_cache_metadata_close(cmd);
		return ERR_PTR(r);
	}

	return cmd;
}

void dm_cache_metadata_close(struct dm_cache_metadata *cmd)
{
	__destroy_persistent_data_objects(cmd);
	kfree(cmd);
}

/*
 * Checks that the given cache block is either unmapped or clean.
 */
static int block_unmapped_or_clean(struct dm_cache_metadata *cmd, dm_cblock_t b,
				   bool *result)
{
	int r;
	__le64 value;
	dm_oblock_t ob;
	unsigned flags;

	r = dm_array_get_value(&cmd->info, cmd->root, from_cblock(b), &value);
	if (r) {
		DMERR("block_unmapped_or_clean failed");
		return r;
	}

	unpack_value(value, &ob, &flags);
	*result = !((flags & M_VALID) && (flags & M_DIRTY));

	return 0;
}

static int blocks_are_unmapped_or_clean(struct dm_cache_metadata *cmd,
					dm_cblock_t begin, dm_cblock_t end,
					bool *result)
{
	int r;
	*result = true;

	while (begin != end) {
		r = block_unmapped_or_clean(cmd, begin, result);
		if (r)
			return r;

		if (!*result) {
			DMERR("cache block %llu is dirty",
			      (unsigned long long) from_cblock(begin));
			return 0;
		}

		begin = to_cblock(from_cblock(begin) + 1);
	}

	return 0;
}

int dm_cache_resize(struct dm_cache_metadata *cmd, dm_cblock_t new_cache_size)
{
	int r;
	bool clean;
	__le64 null_mapping = pack_value(0, 0);

	down_write(&cmd->root_lock);
	__dm_bless_for_disk(&null_mapping);

	if (from_cblock(new_cache_size) < from_cblock(cmd->cache_blocks)) {
		r = blocks_are_unmapped_or_clean(cmd, new_cache_size, cmd->cache_blocks, &clean);
		if (r) {
			__dm_unbless_for_disk(&null_mapping);
			goto out;
		}

		if (!clean) {
			DMERR("unable to shrink cache due to dirty blocks");
			r = -EINVAL;
			__dm_unbless_for_disk(&null_mapping);
			goto out;
		}
	}

	r = dm_array_resize(&cmd->info, cmd->root, from_cblock(cmd->cache_blocks),
			    from_cblock(new_cache_size),
			    &null_mapping, &cmd->root);
	if (!r)
		cmd->cache_blocks = new_cache_size;
	cmd->changed = true;

out:
	up_write(&cmd->root_lock);

	return r;
}

int dm_cache_discard_bitset_resize(struct dm_cache_metadata *cmd,
				   sector_t discard_block_size,
				   dm_oblock_t new_nr_entries)
{
	int r;

	down_write(&cmd->root_lock);
	r = dm_bitset_resize(&cmd->discard_info,
			     cmd->discard_root,
			     from_oblock(cmd->discard_nr_blocks),
			     from_oblock(new_nr_entries),
			     false, &cmd->discard_root);
	if (!r) {
		cmd->discard_block_size = discard_block_size;
		cmd->discard_nr_blocks = new_nr_entries;
	}

	cmd->changed = true;
	up_write(&cmd->root_lock);

	return r;
}

static int __set_discard(struct dm_cache_metadata *cmd, dm_oblock_t b)
{
	return dm_bitset_set_bit(&cmd->discard_info, cmd->discard_root,
				 from_oblock(b), &cmd->discard_root);
}

static int __clear_discard(struct dm_cache_metadata *cmd, dm_oblock_t b)
{
	return dm_bitset_clear_bit(&cmd->discard_info, cmd->discard_root,
				   from_oblock(b), &cmd->discard_root);
}

static int __is_discarded(struct dm_cache_metadata *cmd, dm_oblock_t b,
			  bool *is_discarded)
{
	return dm_bitset_test_bit(&cmd->discard_info, cmd->discard_root,
				  from_oblock(b), &cmd->discard_root,
				  is_discarded);
}

static int __discard(struct dm_cache_metadata *cmd,
		     dm_oblock_t dblock, bool discard)
{
	int r;

	r = (discard ? __set_discard : __clear_discard)(cmd, dblock);
	if (r)
		return r;

	cmd->changed = true;
	return 0;
}

int dm_cache_set_discard(struct dm_cache_metadata *cmd,
			 dm_oblock_t dblock, bool discard)
{
	int r;

	down_write(&cmd->root_lock);
	r = __discard(cmd, dblock, discard);
	up_write(&cmd->root_lock);

	return r;
}

static int __load_discards(struct dm_cache_metadata *cmd,
			   load_discard_fn fn, void *context)
{
	int r = 0;
	dm_block_t b;
	bool discard;

	for (b = 0; b < from_oblock(cmd->discard_nr_blocks); b++) {
		dm_oblock_t dblock = to_oblock(b);

		if (cmd->clean_when_opened) {
			r = __is_discarded(cmd, dblock, &discard);
			if (r)
				return r;
		} else
			discard = false;

		r = fn(context, cmd->discard_block_size, dblock, discard);
		if (r)
			break;
	}

	return r;
}

int dm_cache_load_discards(struct dm_cache_metadata *cmd,
			   load_discard_fn fn, void *context)
{
	int r;

	down_read(&cmd->root_lock);
	r = __load_discards(cmd, fn, context);
	up_read(&cmd->root_lock);

	return r;
}

dm_cblock_t dm_cache_size(struct dm_cache_metadata *cmd)
{
	dm_cblock_t r;

	down_read(&cmd->root_lock);
	r = cmd->cache_blocks;
	up_read(&cmd->root_lock);

	return r;
}

static int __remove(struct dm_cache_metadata *cmd, dm_cblock_t cblock)
{
	int r;
	__le64 value = pack_value(0, 0);

	__dm_bless_for_disk(&value);
	r = dm_array_set_value(&cmd->info, cmd->root, from_cblock(cblock),
			       &value, &cmd->root);
	if (r)
		return r;

	cmd->changed = true;
	return 0;
}

int dm_cache_remove_mapping(struct dm_cache_metadata *cmd, dm_cblock_t cblock)
{
	int r;

	down_write(&cmd->root_lock);
	r = __remove(cmd, cblock);
	up_write(&cmd->root_lock);

	return r;
}

static int __insert(struct dm_cache_metadata *cmd,
		    dm_cblock_t cblock, dm_oblock_t oblock)
{
	int r;
	__le64 value = pack_value(oblock, M_VALID);
	__dm_bless_for_disk(&value);

	r = dm_array_set_value(&cmd->info, cmd->root, from_cblock(cblock),
			       &value, &cmd->root);
	if (r)
		return r;

	cmd->changed = true;
	return 0;
}

int dm_cache_insert_mapping(struct dm_cache_metadata *cmd,
			    dm_cblock_t cblock, dm_oblock_t oblock)
{
	int r;

	down_write(&cmd->root_lock);
	r = __insert(cmd, cblock, oblock);
	up_write(&cmd->root_lock);

	return r;
}

struct thunk {
	load_mapping_fn fn;
	void *context;

	struct dm_cache_metadata *cmd;
	bool respect_dirty_flags;
	bool hints_valid;
};

static bool policy_unchanged(struct dm_cache_metadata *cmd,
			     struct dm_cache_policy *policy)
{
	const char *policy_name = dm_cache_policy_get_name(policy);
	const unsigned *policy_version = dm_cache_policy_get_version(policy);
	size_t policy_hint_size = dm_cache_policy_get_hint_size(policy);

	/*
	 * Ensure policy names match.
	 */
	if (strncmp(cmd->policy_name, policy_name, sizeof(cmd->policy_name)))
		return false;

	/*
	 * Ensure policy major versions match.
	 */
	if (cmd->policy_version[0] != policy_version[0])
		return false;

	/*
	 * Ensure policy hint sizes match.
	 */
	if (cmd->policy_hint_size != policy_hint_size)
		return false;

	return true;
}

static bool hints_array_initialized(struct dm_cache_metadata *cmd)
{
	return cmd->hint_root && cmd->policy_hint_size;
}

static bool hints_array_available(struct dm_cache_metadata *cmd,
				  struct dm_cache_policy *policy)
{
	return cmd->clean_when_opened && policy_unchanged(cmd, policy) &&
		hints_array_initialized(cmd);
}

static int __load_mapping(void *context, uint64_t cblock, void *leaf)
{
	int r = 0;
	bool dirty;
	__le64 value;
	__le32 hint_value = 0;
	dm_oblock_t oblock;
	unsigned flags;
	struct thunk *thunk = context;
	struct dm_cache_metadata *cmd = thunk->cmd;

	memcpy(&value, leaf, sizeof(value));
	unpack_value(value, &oblock, &flags);

	if (flags & M_VALID) {
		if (thunk->hints_valid) {
			r = dm_array_get_value(&cmd->hint_info, cmd->hint_root,
					       cblock, &hint_value);
			if (r && r != -ENODATA)
				return r;
		}

		dirty = thunk->respect_dirty_flags ? (flags & M_DIRTY) : true;
		r = thunk->fn(thunk->context, oblock, to_cblock(cblock),
			      dirty, le32_to_cpu(hint_value), thunk->hints_valid);
	}

	return r;
}

static int __load_mappings(struct dm_cache_metadata *cmd,
			   struct dm_cache_policy *policy,
			   load_mapping_fn fn, void *context)
{
	struct thunk thunk;

	thunk.fn = fn;
	thunk.context = context;

	thunk.cmd = cmd;
	thunk.respect_dirty_flags = cmd->clean_when_opened;
	thunk.hints_valid = hints_array_available(cmd, policy);

	return dm_array_walk(&cmd->info, cmd->root, __load_mapping, &thunk);
}

int dm_cache_load_mappings(struct dm_cache_metadata *cmd,
			   struct dm_cache_policy *policy,
			   load_mapping_fn fn, void *context)
{
	int r;

	down_read(&cmd->root_lock);
	r = __load_mappings(cmd, policy, fn, context);
	up_read(&cmd->root_lock);

	return r;
}

static int __dump_mapping(void *context, uint64_t cblock, void *leaf)
{
	int r = 0;
	__le64 value;
	dm_oblock_t oblock;
	unsigned flags;

	memcpy(&value, leaf, sizeof(value));
	unpack_value(value, &oblock, &flags);

	return r;
}

static int __dump_mappings(struct dm_cache_metadata *cmd)
{
	return dm_array_walk(&cmd->info, cmd->root, __dump_mapping, NULL);
}

void dm_cache_dump(struct dm_cache_metadata *cmd)
{
	down_read(&cmd->root_lock);
	__dump_mappings(cmd);
	up_read(&cmd->root_lock);
}

int dm_cache_changed_this_transaction(struct dm_cache_metadata *cmd)
{
	int r;

	down_read(&cmd->root_lock);
	r = cmd->changed;
	up_read(&cmd->root_lock);

	return r;
}

static int __dirty(struct dm_cache_metadata *cmd, dm_cblock_t cblock, bool dirty)
{
	int r;
	unsigned flags;
	dm_oblock_t oblock;
	__le64 value;

	r = dm_array_get_value(&cmd->info, cmd->root, from_cblock(cblock), &value);
	if (r)
		return r;

	unpack_value(value, &oblock, &flags);

	if (((flags & M_DIRTY) && dirty) || (!(flags & M_DIRTY) && !dirty))
		/* nothing to be done */
		return 0;

	value = pack_value(oblock, (flags & ~M_DIRTY) | (dirty ? M_DIRTY : 0));
	__dm_bless_for_disk(&value);

	r = dm_array_set_value(&cmd->info, cmd->root, from_cblock(cblock),
			       &value, &cmd->root);
	if (r)
		return r;

	cmd->changed = true;
	return 0;

}

int dm_cache_set_dirty(struct dm_cache_metadata *cmd,
		       dm_cblock_t cblock, bool dirty)
{
	int r;

	down_write(&cmd->root_lock);
	r = __dirty(cmd, cblock, dirty);
	up_write(&cmd->root_lock);

	return r;
}

void dm_cache_metadata_get_stats(struct dm_cache_metadata *cmd,
				 struct dm_cache_statistics *stats)
{
	down_read(&cmd->root_lock);
	*stats = cmd->stats;
	up_read(&cmd->root_lock);
}

void dm_cache_metadata_set_stats(struct dm_cache_metadata *cmd,
				 struct dm_cache_statistics *stats)
{
	down_write(&cmd->root_lock);
	cmd->stats = *stats;
	up_write(&cmd->root_lock);
}

int dm_cache_commit(struct dm_cache_metadata *cmd, bool clean_shutdown)
{
	int r;
	flags_mutator mutator = (clean_shutdown ? set_clean_shutdown :
				 clear_clean_shutdown);

	down_write(&cmd->root_lock);
	r = __commit_transaction(cmd, mutator);
	if (r)
		goto out;

	r = __begin_transaction(cmd);

out:
	up_write(&cmd->root_lock);
	return r;
}

int dm_cache_get_free_metadata_block_count(struct dm_cache_metadata *cmd,
					   dm_block_t *result)
{
	int r = -EINVAL;

	down_read(&cmd->root_lock);
	r = dm_sm_get_nr_free(cmd->metadata_sm, result);
	up_read(&cmd->root_lock);

	return r;
}

int dm_cache_get_metadata_dev_size(struct dm_cache_metadata *cmd,
				   dm_block_t *result)
{
	int r = -EINVAL;

	down_read(&cmd->root_lock);
	r = dm_sm_get_nr_blocks(cmd->metadata_sm, result);
	up_read(&cmd->root_lock);

	return r;
}

/*----------------------------------------------------------------*/

static int begin_hints(struct dm_cache_metadata *cmd, struct dm_cache_policy *policy)
{
	int r;
	__le32 value;
	size_t hint_size;
	const char *policy_name = dm_cache_policy_get_name(policy);
	const unsigned *policy_version = dm_cache_policy_get_version(policy);

	if (!policy_name[0] ||
	    (strlen(policy_name) > sizeof(cmd->policy_name) - 1))
		return -EINVAL;

	if (!policy_unchanged(cmd, policy)) {
		strncpy(cmd->policy_name, policy_name, sizeof(cmd->policy_name));
		memcpy(cmd->policy_version, policy_version, sizeof(cmd->policy_version));

		hint_size = dm_cache_policy_get_hint_size(policy);
		if (!hint_size)
			return 0; /* short-circuit hints initialization */
		cmd->policy_hint_size = hint_size;

		if (cmd->hint_root) {
			r = dm_array_del(&cmd->hint_info, cmd->hint_root);
			if (r)
				return r;
		}

		r = dm_array_empty(&cmd->hint_info, &cmd->hint_root);
		if (r)
			return r;

		value = cpu_to_le32(0);
		__dm_bless_for_disk(&value);
		r = dm_array_resize(&cmd->hint_info, cmd->hint_root, 0,
				    from_cblock(cmd->cache_blocks),
				    &value, &cmd->hint_root);
		if (r)
			return r;
	}

	return 0;
}

static int save_hint(void *context, dm_cblock_t cblock, dm_oblock_t oblock, uint32_t hint)
{
	struct dm_cache_metadata *cmd = context;
	__le32 value = cpu_to_le32(hint);
	int r;

	__dm_bless_for_disk(&value);

	r = dm_array_set_value(&cmd->hint_info, cmd->hint_root,
			       from_cblock(cblock), &value, &cmd->hint_root);
	cmd->changed = true;

	return r;
}

static int write_hints(struct dm_cache_metadata *cmd, struct dm_cache_policy *policy)
{
	int r;

	r = begin_hints(cmd, policy);
	if (r) {
		DMERR("begin_hints failed");
		return r;
	}

	return policy_walk_mappings(policy, save_hint, cmd);
}

int dm_cache_write_hints(struct dm_cache_metadata *cmd, struct dm_cache_policy *policy)
{
	int r;

	down_write(&cmd->root_lock);
	r = write_hints(cmd, policy);
	up_write(&cmd->root_lock);

	return r;
}

int dm_cache_metadata_all_clean(struct dm_cache_metadata *cmd, bool *result)
{
	return blocks_are_unmapped_or_clean(cmd, 0, cmd->cache_blocks, result);
}
