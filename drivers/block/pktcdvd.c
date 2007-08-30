/*
 * Copyright (C) 2000 Jens Axboe <axboe@suse.de>
 * Copyright (C) 2001-2004 Peter Osterlund <petero2@telia.com>
 * Copyright (C) 2006 Thomas Maier <balagi@justmail.de>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Packet writing layer for ATAPI and SCSI CD-RW, DVD+RW, DVD-RW and
 * DVD-RAM devices.
 *
 * Theory of operation:
 *
 * At the lowest level, there is the standard driver for the CD/DVD device,
 * typically ide-cd.c or sr.c. This driver can handle read and write requests,
 * but it doesn't know anything about the special restrictions that apply to
 * packet writing. One restriction is that write requests must be aligned to
 * packet boundaries on the physical media, and the size of a write request
 * must be equal to the packet size. Another restriction is that a
 * GPCMD_FLUSH_CACHE command has to be issued to the drive before a read
 * command, if the previous command was a write.
 *
 * The purpose of the packet writing driver is to hide these restrictions from
 * higher layers, such as file systems, and present a block device that can be
 * randomly read and written using 2kB-sized blocks.
 *
 * The lowest layer in the packet writing driver is the packet I/O scheduler.
 * Its data is defined by the struct packet_iosched and includes two bio
 * queues with pending read and write requests. These queues are processed
 * by the pkt_iosched_process_queue() function. The write requests in this
 * queue are already properly aligned and sized. This layer is responsible for
 * issuing the flush cache commands and scheduling the I/O in a good order.
 *
 * The next layer transforms unaligned write requests to aligned writes. This
 * transformation requires reading missing pieces of data from the underlying
 * block device, assembling the pieces to full packets and queuing them to the
 * packet I/O scheduler.
 *
 * At the top layer there is a custom make_request_fn function that forwards
 * read requests directly to the iosched queue and puts write requests in the
 * unaligned write queue. A kernel thread performs the necessary read
 * gathering to convert the unaligned writes to aligned writes and then feeds
 * them to the packet I/O scheduler.
 *
 *************************************************************************/

#include <linux/pktcdvd.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/freezer.h>
#include <linux/mutex.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsi.h>
#include <linux/debugfs.h>
#include <linux/device.h>

#include <asm/uaccess.h>

#define DRIVER_NAME	"pktcdvd"

#if PACKET_DEBUG
#define DPRINTK(fmt, args...) printk(KERN_NOTICE fmt, ##args)
#else
#define DPRINTK(fmt, args...)
#endif

#if PACKET_DEBUG > 1
#define VPRINTK(fmt, args...) printk(KERN_NOTICE fmt, ##args)
#else
#define VPRINTK(fmt, args...)
#endif

#define MAX_SPEED 0xffff

#define ZONE(sector, pd) (((sector) + (pd)->offset) & ~((pd)->settings.size - 1))

static struct pktcdvd_device *pkt_devs[MAX_WRITERS];
static struct proc_dir_entry *pkt_proc;
static int pktdev_major;
static int write_congestion_on  = PKT_WRITE_CONGESTION_ON;
static int write_congestion_off = PKT_WRITE_CONGESTION_OFF;
static struct mutex ctl_mutex;	/* Serialize open/close/setup/teardown */
static mempool_t *psd_pool;

static struct class	*class_pktcdvd = NULL;    /* /sys/class/pktcdvd */
static struct dentry	*pkt_debugfs_root = NULL; /* /debug/pktcdvd */

/* forward declaration */
static int pkt_setup_dev(dev_t dev, dev_t* pkt_dev);
static int pkt_remove_dev(dev_t pkt_dev);
static int pkt_seq_show(struct seq_file *m, void *p);



/*
 * create and register a pktcdvd kernel object.
 */
static struct pktcdvd_kobj* pkt_kobj_create(struct pktcdvd_device *pd,
					const char* name,
					struct kobject* parent,
					struct kobj_type* ktype)
{
	struct pktcdvd_kobj *p;
	int error;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;
	p->pd = pd;
	error = kobject_init_and_add(&p->kobj, ktype, parent, "%s", name);
	if (error) {
		kobject_put(&p->kobj);
		return NULL;
	}
	kobject_uevent(&p->kobj, KOBJ_ADD);
	return p;
}
/*
 * remove a pktcdvd kernel object.
 */
static void pkt_kobj_remove(struct pktcdvd_kobj *p)
{
	if (p)
		kobject_put(&p->kobj);
}
/*
 * default release function for pktcdvd kernel objects.
 */
static void pkt_kobj_release(struct kobject *kobj)
{
	kfree(to_pktcdvdkobj(kobj));
}


/**********************************************************
 *
 * sysfs interface for pktcdvd
 * by (C) 2006  Thomas Maier <balagi@justmail.de>
 *
 **********************************************************/

#define DEF_ATTR(_obj,_name,_mode) \
	static struct attribute _obj = { .name = _name, .mode = _mode }

/**********************************************************
  /sys/class/pktcdvd/pktcdvd[0-7]/
                     stat/reset
                     stat/packets_started
                     stat/packets_finished
                     stat/kb_written
                     stat/kb_read
                     stat/kb_read_gather
                     write_queue/size
                     write_queue/congestion_off
                     write_queue/congestion_on
 **********************************************************/

DEF_ATTR(kobj_pkt_attr_st1, "reset", 0200);
DEF_ATTR(kobj_pkt_attr_st2, "packets_started", 0444);
DEF_ATTR(kobj_pkt_attr_st3, "packets_finished", 0444);
DEF_ATTR(kobj_pkt_attr_st4, "kb_written", 0444);
DEF_ATTR(kobj_pkt_attr_st5, "kb_read", 0444);
DEF_ATTR(kobj_pkt_attr_st6, "kb_read_gather", 0444);

static struct attribute *kobj_pkt_attrs_stat[] = {
	&kobj_pkt_attr_st1,
	&kobj_pkt_attr_st2,
	&kobj_pkt_attr_st3,
	&kobj_pkt_attr_st4,
	&kobj_pkt_attr_st5,
	&kobj_pkt_attr_st6,
	NULL
};

DEF_ATTR(kobj_pkt_attr_wq1, "size", 0444);
DEF_ATTR(kobj_pkt_attr_wq2, "congestion_off", 0644);
DEF_ATTR(kobj_pkt_attr_wq3, "congestion_on",  0644);

static struct attribute *kobj_pkt_attrs_wqueue[] = {
	&kobj_pkt_attr_wq1,
	&kobj_pkt_attr_wq2,
	&kobj_pkt_attr_wq3,
	NULL
};

static ssize_t kobj_pkt_show(struct kobject *kobj,
			struct attribute *attr, char *data)
{
	struct pktcdvd_device *pd = to_pktcdvdkobj(kobj)->pd;
	int n = 0;
	int v;
	if (strcmp(attr->name, "packets_started") == 0) {
		n = sprintf(data, "%lu\n", pd->stats.pkt_started);

	} else if (strcmp(attr->name, "packets_finished") == 0) {
		n = sprintf(data, "%lu\n", pd->stats.pkt_ended);

	} else if (strcmp(attr->name, "kb_written") == 0) {
		n = sprintf(data, "%lu\n", pd->stats.secs_w >> 1);

	} else if (strcmp(attr->name, "kb_read") == 0) {
		n = sprintf(data, "%lu\n", pd->stats.secs_r >> 1);

	} else if (strcmp(attr->name, "kb_read_gather") == 0) {
		n = sprintf(data, "%lu\n", pd->stats.secs_rg >> 1);

	} else if (strcmp(attr->name, "size") == 0) {
		spin_lock(&pd->lock);
		v = pd->bio_queue_size;
		spin_unlock(&pd->lock);
		n = sprintf(data, "%d\n", v);

	} else if (strcmp(attr->name, "congestion_off") == 0) {
		spin_lock(&pd->lock);
		v = pd->write_congestion_off;
		spin_unlock(&pd->lock);
		n = sprintf(data, "%d\n", v);

	} else if (strcmp(attr->name, "congestion_on") == 0) {
		spin_lock(&pd->lock);
		v = pd->write_congestion_on;
		spin_unlock(&pd->lock);
		n = sprintf(data, "%d\n", v);
	}
	return n;
}

static void init_write_congestion_marks(int* lo, int* hi)
{
	if (*hi > 0) {
		*hi = max(*hi, 500);
		*hi = min(*hi, 1000000);
		if (*lo <= 0)
			*lo = *hi - 100;
		else {
			*lo = min(*lo, *hi - 100);
			*lo = max(*lo, 100);
		}
	} else {
		*hi = -1;
		*lo = -1;
	}
}

static ssize_t kobj_pkt_store(struct kobject *kobj,
			struct attribute *attr,
			const char *data, size_t len)
{
	struct pktcdvd_device *pd = to_pktcdvdkobj(kobj)->pd;
	int val;

	if (strcmp(attr->name, "reset") == 0 && len > 0) {
		pd->stats.pkt_started = 0;
		pd->stats.pkt_ended = 0;
		pd->stats.secs_w = 0;
		pd->stats.secs_rg = 0;
		pd->stats.secs_r = 0;

	} else if (strcmp(attr->name, "congestion_off") == 0
		   && sscanf(data, "%d", &val) == 1) {
		spin_lock(&pd->lock);
		pd->write_congestion_off = val;
		init_write_congestion_marks(&pd->write_congestion_off,
					&pd->write_congestion_on);
		spin_unlock(&pd->lock);

	} else if (strcmp(attr->name, "congestion_on") == 0
		   && sscanf(data, "%d", &val) == 1) {
		spin_lock(&pd->lock);
		pd->write_congestion_on = val;
		init_write_congestion_marks(&pd->write_congestion_off,
					&pd->write_congestion_on);
		spin_unlock(&pd->lock);
	}
	return len;
}

static struct sysfs_ops kobj_pkt_ops = {
	.show = kobj_pkt_show,
	.store = kobj_pkt_store
};
static struct kobj_type kobj_pkt_type_stat = {
	.release = pkt_kobj_release,
	.sysfs_ops = &kobj_pkt_ops,
	.default_attrs = kobj_pkt_attrs_stat
};
static struct kobj_type kobj_pkt_type_wqueue = {
	.release = pkt_kobj_release,
	.sysfs_ops = &kobj_pkt_ops,
	.default_attrs = kobj_pkt_attrs_wqueue
};

static void pkt_sysfs_dev_new(struct pktcdvd_device *pd)
{
	if (class_pktcdvd) {
		pd->dev = device_create(class_pktcdvd, NULL, pd->pkt_dev, NULL,
					"%s", pd->name);
		if (IS_ERR(pd->dev))
			pd->dev = NULL;
	}
	if (pd->dev) {
		pd->kobj_stat = pkt_kobj_create(pd, "stat",
					&pd->dev->kobj,
					&kobj_pkt_type_stat);
		pd->kobj_wqueue = pkt_kobj_create(pd, "write_queue",
					&pd->dev->kobj,
					&kobj_pkt_type_wqueue);
	}
}

static void pkt_sysfs_dev_remove(struct pktcdvd_device *pd)
{
	pkt_kobj_remove(pd->kobj_stat);
	pkt_kobj_remove(pd->kobj_wqueue);
	if (class_pktcdvd)
		device_destroy(class_pktcdvd, pd->pkt_dev);
}


/********************************************************************
  /sys/class/pktcdvd/
                     add            map block device
                     remove         unmap packet dev
                     device_map     show mappings
 *******************************************************************/

static void class_pktcdvd_release(struct class *cls)
{
	kfree(cls);
}
static ssize_t class_pktcdvd_show_map(struct class *c, char *data)
{
	int n = 0;
	int idx;
	mutex_lock_nested(&ctl_mutex, SINGLE_DEPTH_NESTING);
	for (idx = 0; idx < MAX_WRITERS; idx++) {
		struct pktcdvd_device *pd = pkt_devs[idx];
		if (!pd)
			continue;
		n += sprintf(data+n, "%s %u:%u %u:%u\n",
			pd->name,
			MAJOR(pd->pkt_dev), MINOR(pd->pkt_dev),
			MAJOR(pd->bdev->bd_dev),
			MINOR(pd->bdev->bd_dev));
	}
	mutex_unlock(&ctl_mutex);
	return n;
}

static ssize_t class_pktcdvd_store_add(struct class *c, const char *buf,
					size_t count)
{
	unsigned int major, minor;

	if (sscanf(buf, "%u:%u", &major, &minor) == 2) {
		/* pkt_setup_dev() expects caller to hold reference to self */
		if (!try_module_get(THIS_MODULE))
			return -ENODEV;

		pkt_setup_dev(MKDEV(major, minor), NULL);

		module_put(THIS_MODULE);

		return count;
	}

	return -EINVAL;
}

static ssize_t class_pktcdvd_store_remove(struct class *c, const char *buf,
					size_t count)
{
	unsigned int major, minor;
	if (sscanf(buf, "%u:%u", &major, &minor) == 2) {
		pkt_remove_dev(MKDEV(major, minor));
		return count;
	}
	return -EINVAL;
}

static struct class_attribute class_pktcdvd_attrs[] = {
 __ATTR(add,            0200, NULL, class_pktcdvd_store_add),
 __ATTR(remove,         0200, NULL, class_pktcdvd_store_remove),
 __ATTR(device_map,     0444, class_pktcdvd_show_map, NULL),
 __ATTR_NULL
};


static int pkt_sysfs_init(void)
{
	int ret = 0;

	/*
	 * create control files in sysfs
	 * /sys/class/pktcdvd/...
	 */
	class_pktcdvd = kzalloc(sizeof(*class_pktcdvd), GFP_KERNEL);
	if (!class_pktcdvd)
		return -ENOMEM;
	class_pktcdvd->name = DRIVER_NAME;
	class_pktcdvd->owner = THIS_MODULE;
	class_pktcdvd->class_release = class_pktcdvd_release;
	class_pktcdvd->class_attrs = class_pktcdvd_attrs;
	ret = class_register(class_pktcdvd);
	if (ret) {
		kfree(class_pktcdvd);
		class_pktcdvd = NULL;
		printk(DRIVER_NAME": failed to create class pktcdvd\n");
		return ret;
	}
	return 0;
}

static void pkt_sysfs_cleanup(void)
{
	if (class_pktcdvd)
		class_destroy(class_pktcdvd);
	class_pktcdvd = NULL;
}

/********************************************************************
  entries in debugfs

  /debugfs/pktcdvd[0-7]/
			info

 *******************************************************************/

static int pkt_debugfs_seq_show(struct seq_file *m, void *p)
{
	return pkt_seq_show(m, p);
}

static int pkt_debugfs_fops_open(struct inode *inode, struct file *file)
{
	return single_open(file, pkt_debugfs_seq_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= pkt_debugfs_fops_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};

static void pkt_debugfs_dev_new(struct pktcdvd_device *pd)
{
	if (!pkt_debugfs_root)
		return;
	pd->dfs_f_info = NULL;
	pd->dfs_d_root = debugfs_create_dir(pd->name, pkt_debugfs_root);
	if (IS_ERR(pd->dfs_d_root)) {
		pd->dfs_d_root = NULL;
		return;
	}
	pd->dfs_f_info = debugfs_create_file("info", S_IRUGO,
				pd->dfs_d_root, pd, &debug_fops);
	if (IS_ERR(pd->dfs_f_info)) {
		pd->dfs_f_info = NULL;
		return;
	}
}

static void pkt_debugfs_dev_remove(struct pktcdvd_device *pd)
{
	if (!pkt_debugfs_root)
		return;
	if (pd->dfs_f_info)
		debugfs_remove(pd->dfs_f_info);
	pd->dfs_f_info = NULL;
	if (pd->dfs_d_root)
		debugfs_remove(pd->dfs_d_root);
	pd->dfs_d_root = NULL;
}

static void pkt_debugfs_init(void)
{
	pkt_debugfs_root = debugfs_create_dir(DRIVER_NAME, NULL);
	if (IS_ERR(pkt_debugfs_root)) {
		pkt_debugfs_root = NULL;
		return;
	}
}

static void pkt_debugfs_cleanup(void)
{
	if (!pkt_debugfs_root)
		return;
	debugfs_remove(pkt_debugfs_root);
	pkt_debugfs_root = NULL;
}

/* ----------------------------------------------------------*/


static void pkt_bio_finished(struct pktcdvd_device *pd)
{
	BUG_ON(atomic_read(&pd->cdrw.pending_bios) <= 0);
	if (atomic_dec_and_test(&pd->cdrw.pending_bios)) {
		VPRINTK(DRIVER_NAME": queue empty\n");
		atomic_set(&pd->iosched.attention, 1);
		wake_up(&pd->wqueue);
	}
}

static void pkt_bio_destructor(struct bio *bio)
{
	kfree(bio->bi_io_vec);
	kfree(bio);
}

static struct bio *pkt_bio_alloc(int nr_iovecs)
{
	struct bio_vec *bvl = NULL;
	struct bio *bio;

	bio = kmalloc(sizeof(struct bio), GFP_KERNEL);
	if (!bio)
		goto no_bio;
	bio_init(bio);

	bvl = kcalloc(nr_iovecs, sizeof(struct bio_vec), GFP_KERNEL);
	if (!bvl)
		goto no_bvl;

	bio->bi_max_vecs = nr_iovecs;
	bio->bi_io_vec = bvl;
	bio->bi_destructor = pkt_bio_destructor;

	return bio;

 no_bvl:
	kfree(bio);
 no_bio:
	return NULL;
}

/*
 * Allocate a packet_data struct
 */
static struct packet_data *pkt_alloc_packet_data(int frames)
{
	int i;
	struct packet_data *pkt;

	pkt = kzalloc(sizeof(struct packet_data), GFP_KERNEL);
	if (!pkt)
		goto no_pkt;

	pkt->frames = frames;
	pkt->w_bio = pkt_bio_alloc(frames);
	if (!pkt->w_bio)
		goto no_bio;

	for (i = 0; i < frames / FRAMES_PER_PAGE; i++) {
		pkt->pages[i] = alloc_page(GFP_KERNEL|__GFP_ZERO);
		if (!pkt->pages[i])
			goto no_page;
	}

	spin_lock_init(&pkt->lock);

	for (i = 0; i < frames; i++) {
		struct bio *bio = pkt_bio_alloc(1);
		if (!bio)
			goto no_rd_bio;
		pkt->r_bios[i] = bio;
	}

	return pkt;

no_rd_bio:
	for (i = 0; i < frames; i++) {
		struct bio *bio = pkt->r_bios[i];
		if (bio)
			bio_put(bio);
	}

no_page:
	for (i = 0; i < frames / FRAMES_PER_PAGE; i++)
		if (pkt->pages[i])
			__free_page(pkt->pages[i]);
	bio_put(pkt->w_bio);
no_bio:
	kfree(pkt);
no_pkt:
	return NULL;
}

/*
 * Free a packet_data struct
 */
static void pkt_free_packet_data(struct packet_data *pkt)
{
	int i;

	for (i = 0; i < pkt->frames; i++) {
		struct bio *bio = pkt->r_bios[i];
		if (bio)
			bio_put(bio);
	}
	for (i = 0; i < pkt->frames / FRAMES_PER_PAGE; i++)
		__free_page(pkt->pages[i]);
	bio_put(pkt->w_bio);
	kfree(pkt);
}

static void pkt_shrink_pktlist(struct pktcdvd_device *pd)
{
	struct packet_data *pkt, *next;

	BUG_ON(!list_empty(&pd->cdrw.pkt_active_list));

	list_for_each_entry_safe(pkt, next, &pd->cdrw.pkt_free_list, list) {
		pkt_free_packet_data(pkt);
	}
	INIT_LIST_HEAD(&pd->cdrw.pkt_free_list);
}

static int pkt_grow_pktlist(struct pktcdvd_device *pd, int nr_packets)
{
	struct packet_data *pkt;

	BUG_ON(!list_empty(&pd->cdrw.pkt_free_list));

	while (nr_packets > 0) {
		pkt = pkt_alloc_packet_data(pd->settings.size >> 2);
		if (!pkt) {
			pkt_shrink_pktlist(pd);
			return 0;
		}
		pkt->id = nr_packets;
		pkt->pd = pd;
		list_add(&pkt->list, &pd->cdrw.pkt_free_list);
		nr_packets--;
	}
	return 1;
}

static inline struct pkt_rb_node *pkt_rbtree_next(struct pkt_rb_node *node)
{
	struct rb_node *n = rb_next(&node->rb_node);
	if (!n)
		return NULL;
	return rb_entry(n, struct pkt_rb_node, rb_node);
}

static void pkt_rbtree_erase(struct pktcdvd_device *pd, struct pkt_rb_node *node)
{
	rb_erase(&node->rb_node, &pd->bio_queue);
	mempool_free(node, pd->rb_pool);
	pd->bio_queue_size--;
	BUG_ON(pd->bio_queue_size < 0);
}

/*
 * Find the first node in the pd->bio_queue rb tree with a starting sector >= s.
 */
static struct pkt_rb_node *pkt_rbtree_find(struct pktcdvd_device *pd, sector_t s)
{
	struct rb_node *n = pd->bio_queue.rb_node;
	struct rb_node *next;
	struct pkt_rb_node *tmp;

	if (!n) {
		BUG_ON(pd->bio_queue_size > 0);
		return NULL;
	}

	for (;;) {
		tmp = rb_entry(n, struct pkt_rb_node, rb_node);
		if (s <= tmp->bio->bi_sector)
			next = n->rb_left;
		else
			next = n->rb_right;
		if (!next)
			break;
		n = next;
	}

	if (s > tmp->bio->bi_sector) {
		tmp = pkt_rbtree_next(tmp);
		if (!tmp)
			return NULL;
	}
	BUG_ON(s > tmp->bio->bi_sector);
	return tmp;
}

/*
 * Insert a node into the pd->bio_queue rb tree.
 */
static void pkt_rbtree_insert(struct pktcdvd_device *pd, struct pkt_rb_node *node)
{
	struct rb_node **p = &pd->bio_queue.rb_node;
	struct rb_node *parent = NULL;
	sector_t s = node->bio->bi_sector;
	struct pkt_rb_node *tmp;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct pkt_rb_node, rb_node);
		if (s < tmp->bio->bi_sector)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}
	rb_link_node(&node->rb_node, parent, p);
	rb_insert_color(&node->rb_node, &pd->bio_queue);
	pd->bio_queue_size++;
}

/*
 * Add a bio to a single linked list defined by its head and tail pointers.
 */
static void pkt_add_list_last(struct bio *bio, struct bio **list_head, struct bio **list_tail)
{
	bio->bi_next = NULL;
	if (*list_tail) {
		BUG_ON((*list_head) == NULL);
		(*list_tail)->bi_next = bio;
		(*list_tail) = bio;
	} else {
		BUG_ON((*list_head) != NULL);
		(*list_head) = bio;
		(*list_tail) = bio;
	}
}

/*
 * Remove and return the first bio from a single linked list defined by its
 * head and tail pointers.
 */
static inline struct bio *pkt_get_list_first(struct bio **list_head, struct bio **list_tail)
{
	struct bio *bio;

	if (*list_head == NULL)
		return NULL;

	bio = *list_head;
	*list_head = bio->bi_next;
	if (*list_head == NULL)
		*list_tail = NULL;

	bio->bi_next = NULL;
	return bio;
}

/*
 * Send a packet_command to the underlying block device and
 * wait for completion.
 */
static int pkt_generic_packet(struct pktcdvd_device *pd, struct packet_command *cgc)
{
	struct request_queue *q = bdev_get_queue(pd->bdev);
	struct request *rq;
	int ret = 0;

	rq = blk_get_request(q, (cgc->data_direction == CGC_DATA_WRITE) ?
			     WRITE : READ, __GFP_WAIT);

	if (cgc->buflen) {
		if (blk_rq_map_kern(q, rq, cgc->buffer, cgc->buflen, __GFP_WAIT))
			goto out;
	}

	rq->cmd_len = COMMAND_SIZE(cgc->cmd[0]);
	memcpy(rq->cmd, cgc->cmd, CDROM_PACKET_SIZE);

	rq->timeout = 60*HZ;
	rq->cmd_type = REQ_TYPE_BLOCK_PC;
	rq->cmd_flags |= REQ_HARDBARRIER;
	if (cgc->quiet)
		rq->cmd_flags |= REQ_QUIET;

	blk_execute_rq(rq->q, pd->bdev->bd_disk, rq, 0);
	if (rq->errors)
		ret = -EIO;
out:
	blk_put_request(rq);
	return ret;
}

/*
 * A generic sense dump / resolve mechanism should be implemented across
 * all ATAPI + SCSI devices.
 */
static void pkt_dump_sense(struct packet_command *cgc)
{
	static char *info[9] = { "No sense", "Recovered error", "Not ready",
				 "Medium error", "Hardware error", "Illegal request",
				 "Unit attention", "Data protect", "Blank check" };
	int i;
	struct request_sense *sense = cgc->sense;

	printk(DRIVER_NAME":");
	for (i = 0; i < CDROM_PACKET_SIZE; i++)
		printk(" %02x", cgc->cmd[i]);
	printk(" - ");

	if (sense == NULL) {
		printk("no sense\n");
		return;
	}

	printk("sense %02x.%02x.%02x", sense->sense_key, sense->asc, sense->ascq);

	if (sense->sense_key > 8) {
		printk(" (INVALID)\n");
		return;
	}

	printk(" (%s)\n", info[sense->sense_key]);
}

/*
 * flush the drive cache to media
 */
static int pkt_flush_cache(struct pktcdvd_device *pd)
{
	struct packet_command cgc;

	init_cdrom_command(&cgc, NULL, 0, CGC_DATA_NONE);
	cgc.cmd[0] = GPCMD_FLUSH_CACHE;
	cgc.quiet = 1;

	/*
	 * the IMMED bit -- we default to not setting it, although that
	 * would allow a much faster close, this is safer
	 */
#if 0
	cgc.cmd[1] = 1 << 1;
#endif
	return pkt_generic_packet(pd, &cgc);
}

/*
 * speed is given as the normal factor, e.g. 4 for 4x
 */
static noinline_for_stack int pkt_set_speed(struct pktcdvd_device *pd,
				unsigned write_speed, unsigned read_speed)
{
	struct packet_command cgc;
	struct request_sense sense;
	int ret;

	init_cdrom_command(&cgc, NULL, 0, CGC_DATA_NONE);
	cgc.sense = &sense;
	cgc.cmd[0] = GPCMD_SET_SPEED;
	cgc.cmd[2] = (read_speed >> 8) & 0xff;
	cgc.cmd[3] = read_speed & 0xff;
	cgc.cmd[4] = (write_speed >> 8) & 0xff;
	cgc.cmd[5] = write_speed & 0xff;

	if ((ret = pkt_generic_packet(pd, &cgc)))
		pkt_dump_sense(&cgc);

	return ret;
}

/*
 * Queue a bio for processing by the low-level CD device. Must be called
 * from process context.
 */
static void pkt_queue_bio(struct pktcdvd_device *pd, struct bio *bio)
{
	spin_lock(&pd->iosched.lock);
	if (bio_data_dir(bio) == READ) {
		pkt_add_list_last(bio, &pd->iosched.read_queue,
				  &pd->iosched.read_queue_tail);
	} else {
		pkt_add_list_last(bio, &pd->iosched.write_queue,
				  &pd->iosched.write_queue_tail);
	}
	spin_unlock(&pd->iosched.lock);

	atomic_set(&pd->iosched.attention, 1);
	wake_up(&pd->wqueue);
}

/*
 * Process the queued read/write requests. This function handles special
 * requirements for CDRW drives:
 * - A cache flush command must be inserted before a read request if the
 *   previous request was a write.
 * - Switching between reading and writing is slow, so don't do it more often
 *   than necessary.
 * - Optimize for throughput at the expense of latency. This means that streaming
 *   writes will never be interrupted by a read, but if the drive has to seek
 *   before the next write, switch to reading instead if there are any pending
 *   read requests.
 * - Set the read speed according to current usage pattern. When only reading
 *   from the device, it's best to use the highest possible read speed, but
 *   when switching often between reading and writing, it's better to have the
 *   same read and write speeds.
 */
static void pkt_iosched_process_queue(struct pktcdvd_device *pd)
{

	if (atomic_read(&pd->iosched.attention) == 0)
		return;
	atomic_set(&pd->iosched.attention, 0);

	for (;;) {
		struct bio *bio;
		int reads_queued, writes_queued;

		spin_lock(&pd->iosched.lock);
		reads_queued = (pd->iosched.read_queue != NULL);
		writes_queued = (pd->iosched.write_queue != NULL);
		spin_unlock(&pd->iosched.lock);

		if (!reads_queued && !writes_queued)
			break;

		if (pd->iosched.writing) {
			int need_write_seek = 1;
			spin_lock(&pd->iosched.lock);
			bio = pd->iosched.write_queue;
			spin_unlock(&pd->iosched.lock);
			if (bio && (bio->bi_sector == pd->iosched.last_write))
				need_write_seek = 0;
			if (need_write_seek && reads_queued) {
				if (atomic_read(&pd->cdrw.pending_bios) > 0) {
					VPRINTK(DRIVER_NAME": write, waiting\n");
					break;
				}
				pkt_flush_cache(pd);
				pd->iosched.writing = 0;
			}
		} else {
			if (!reads_queued && writes_queued) {
				if (atomic_read(&pd->cdrw.pending_bios) > 0) {
					VPRINTK(DRIVER_NAME": read, waiting\n");
					break;
				}
				pd->iosched.writing = 1;
			}
		}

		spin_lock(&pd->iosched.lock);
		if (pd->iosched.writing) {
			bio = pkt_get_list_first(&pd->iosched.write_queue,
						 &pd->iosched.write_queue_tail);
		} else {
			bio = pkt_get_list_first(&pd->iosched.read_queue,
						 &pd->iosched.read_queue_tail);
		}
		spin_unlock(&pd->iosched.lock);

		if (!bio)
			continue;

		if (bio_data_dir(bio) == READ)
			pd->iosched.successive_reads += bio->bi_size >> 10;
		else {
			pd->iosched.successive_reads = 0;
			pd->iosched.last_write = bio->bi_sector + bio_sectors(bio);
		}
		if (pd->iosched.successive_reads >= HI_SPEED_SWITCH) {
			if (pd->read_speed == pd->write_speed) {
				pd->read_speed = MAX_SPEED;
				pkt_set_speed(pd, pd->write_speed, pd->read_speed);
			}
		} else {
			if (pd->read_speed != pd->write_speed) {
				pd->read_speed = pd->write_speed;
				pkt_set_speed(pd, pd->write_speed, pd->read_speed);
			}
		}

		atomic_inc(&pd->cdrw.pending_bios);
		generic_make_request(bio);
	}
}

/*
 * Special care is needed if the underlying block device has a small
 * max_phys_segments value.
 */
static int pkt_set_segment_merging(struct pktcdvd_device *pd, struct request_queue *q)
{
	if ((pd->settings.size << 9) / CD_FRAMESIZE <= q->max_phys_segments) {
		/*
		 * The cdrom device can handle one segment/frame
		 */
		clear_bit(PACKET_MERGE_SEGS, &pd->flags);
		return 0;
	} else if ((pd->settings.size << 9) / PAGE_SIZE <= q->max_phys_segments) {
		/*
		 * We can handle this case at the expense of some extra memory
		 * copies during write operations
		 */
		set_bit(PACKET_MERGE_SEGS, &pd->flags);
		return 0;
	} else {
		printk(DRIVER_NAME": cdrom max_phys_segments too small\n");
		return -EIO;
	}
}

/*
 * Copy CD_FRAMESIZE bytes from src_bio into a destination page
 */
static void pkt_copy_bio_data(struct bio *src_bio, int seg, int offs, struct page *dst_page, int dst_offs)
{
	unsigned int copy_size = CD_FRAMESIZE;

	while (copy_size > 0) {
		struct bio_vec *src_bvl = bio_iovec_idx(src_bio, seg);
		void *vfrom = kmap_atomic(src_bvl->bv_page, KM_USER0) +
			src_bvl->bv_offset + offs;
		void *vto = page_address(dst_page) + dst_offs;
		int len = min_t(int, copy_size, src_bvl->bv_len - offs);

		BUG_ON(len < 0);
		memcpy(vto, vfrom, len);
		kunmap_atomic(vfrom, KM_USER0);

		seg++;
		offs = 0;
		dst_offs += len;
		copy_size -= len;
	}
}

/*
 * Copy all data for this packet to pkt->pages[], so that
 * a) The number of required segments for the write bio is minimized, which
 *    is necessary for some scsi controllers.
 * b) The data can be used as cache to avoid read requests if we receive a
 *    new write request for the same zone.
 */
static void pkt_make_local_copy(struct packet_data *pkt, struct bio_vec *bvec)
{
	int f, p, offs;

	/* Copy all data to pkt->pages[] */
	p = 0;
	offs = 0;
	for (f = 0; f < pkt->frames; f++) {
		if (bvec[f].bv_page != pkt->pages[p]) {
			void *vfrom = kmap_atomic(bvec[f].bv_page, KM_USER0) + bvec[f].bv_offset;
			void *vto = page_address(pkt->pages[p]) + offs;
			memcpy(vto, vfrom, CD_FRAMESIZE);
			kunmap_atomic(vfrom, KM_USER0);
			bvec[f].bv_page = pkt->pages[p];
			bvec[f].bv_offset = offs;
		} else {
			BUG_ON(bvec[f].bv_offset != offs);
		}
		offs += CD_FRAMESIZE;
		if (offs >= PAGE_SIZE) {
			offs = 0;
			p++;
		}
	}
}

static void pkt_end_io_read(struct bio *bio, int err)
{
	struct packet_data *pkt = bio->bi_private;
	struct pktcdvd_device *pd = pkt->pd;
	BUG_ON(!pd);

	VPRINTK("pkt_end_io_read: bio=%p sec0=%llx sec=%llx err=%d\n", bio,
		(unsigned long long)pkt->sector, (unsigned long long)bio->bi_sector, err);

	if (err)
		atomic_inc(&pkt->io_errors);
	if (atomic_dec_and_test(&pkt->io_wait)) {
		atomic_inc(&pkt->run_sm);
		wake_up(&pd->wqueue);
	}
	pkt_bio_finished(pd);
}

static void pkt_end_io_packet_write(struct bio *bio, int err)
{
	struct packet_data *pkt = bio->bi_private;
	struct pktcdvd_device *pd = pkt->pd;
	BUG_ON(!pd);

	VPRINTK("pkt_end_io_packet_write: id=%d, err=%d\n", pkt->id, err);

	pd->stats.pkt_ended++;

	pkt_bio_finished(pd);
	atomic_dec(&pkt->io_wait);
	atomic_inc(&pkt->run_sm);
	wake_up(&pd->wqueue);
}

/*
 * Schedule reads for the holes in a packet
 */
static void pkt_gather_data(struct pktcdvd_device *pd, struct packet_data *pkt)
{
	int frames_read = 0;
	struct bio *bio;
	int f;
	char written[PACKET_MAX_SIZE];

	BUG_ON(!pkt->orig_bios);

	atomic_set(&pkt->io_wait, 0);
	atomic_set(&pkt->io_errors, 0);

	/*
	 * Figure out which frames we need to read before we can write.
	 */
	memset(written, 0, sizeof(written));
	spin_lock(&pkt->lock);
	for (bio = pkt->orig_bios; bio; bio = bio->bi_next) {
		int first_frame = (bio->bi_sector - pkt->sector) / (CD_FRAMESIZE >> 9);
		int num_frames = bio->bi_size / CD_FRAMESIZE;
		pd->stats.secs_w += num_frames * (CD_FRAMESIZE >> 9);
		BUG_ON(first_frame < 0);
		BUG_ON(first_frame + num_frames > pkt->frames);
		for (f = first_frame; f < first_frame + num_frames; f++)
			written[f] = 1;
	}
	spin_unlock(&pkt->lock);

	if (pkt->cache_valid) {
		VPRINTK("pkt_gather_data: zone %llx cached\n",
			(unsigned long long)pkt->sector);
		goto out_account;
	}

	/*
	 * Schedule reads for missing parts of the packet.
	 */
	for (f = 0; f < pkt->frames; f++) {
		struct bio_vec *vec;

		int p, offset;
		if (written[f])
			continue;
		bio = pkt->r_bios[f];
		vec = bio->bi_io_vec;
		bio_init(bio);
		bio->bi_max_vecs = 1;
		bio->bi_sector = pkt->sector + f * (CD_FRAMESIZE >> 9);
		bio->bi_bdev = pd->bdev;
		bio->bi_end_io = pkt_end_io_read;
		bio->bi_private = pkt;
		bio->bi_io_vec = vec;
		bio->bi_destructor = pkt_bio_destructor;

		p = (f * CD_FRAMESIZE) / PAGE_SIZE;
		offset = (f * CD_FRAMESIZE) % PAGE_SIZE;
		VPRINTK("pkt_gather_data: Adding frame %d, page:%p offs:%d\n",
			f, pkt->pages[p], offset);
		if (!bio_add_page(bio, pkt->pages[p], CD_FRAMESIZE, offset))
			BUG();

		atomic_inc(&pkt->io_wait);
		bio->bi_rw = READ;
		pkt_queue_bio(pd, bio);
		frames_read++;
	}

out_account:
	VPRINTK("pkt_gather_data: need %d frames for zone %llx\n",
		frames_read, (unsigned long long)pkt->sector);
	pd->stats.pkt_started++;
	pd->stats.secs_rg += frames_read * (CD_FRAMESIZE >> 9);
}

/*
 * Find a packet matching zone, or the least recently used packet if
 * there is no match.
 */
static struct packet_data *pkt_get_packet_data(struct pktcdvd_device *pd, int zone)
{
	struct packet_data *pkt;

	list_for_each_entry(pkt, &pd->cdrw.pkt_free_list, list) {
		if (pkt->sector == zone || pkt->list.next == &pd->cdrw.pkt_free_list) {
			list_del_init(&pkt->list);
			if (pkt->sector != zone)
				pkt->cache_valid = 0;
			return pkt;
		}
	}
	BUG();
	return NULL;
}

static void pkt_put_packet_data(struct pktcdvd_device *pd, struct packet_data *pkt)
{
	if (pkt->cache_valid) {
		list_add(&pkt->list, &pd->cdrw.pkt_free_list);
	} else {
		list_add_tail(&pkt->list, &pd->cdrw.pkt_free_list);
	}
}

/*
 * recover a failed write, query for relocation if possible
 *
 * returns 1 if recovery is possible, or 0 if not
 *
 */
static int pkt_start_recovery(struct packet_data *pkt)
{
	/*
	 * FIXME. We need help from the file system to implement
	 * recovery handling.
	 */
	return 0;
#if 0
	struct request *rq = pkt->rq;
	struct pktcdvd_device *pd = rq->rq_disk->private_data;
	struct block_device *pkt_bdev;
	struct super_block *sb = NULL;
	unsigned long old_block, new_block;
	sector_t new_sector;

	pkt_bdev = bdget(kdev_t_to_nr(pd->pkt_dev));
	if (pkt_bdev) {
		sb = get_super(pkt_bdev);
		bdput(pkt_bdev);
	}

	if (!sb)
		return 0;

	if (!sb->s_op || !sb->s_op->relocate_blocks)
		goto out;

	old_block = pkt->sector / (CD_FRAMESIZE >> 9);
	if (sb->s_op->relocate_blocks(sb, old_block, &new_block))
		goto out;

	new_sector = new_block * (CD_FRAMESIZE >> 9);
	pkt->sector = new_sector;

	pkt->bio->bi_sector = new_sector;
	pkt->bio->bi_next = NULL;
	pkt->bio->bi_flags = 1 << BIO_UPTODATE;
	pkt->bio->bi_idx = 0;

	BUG_ON(pkt->bio->bi_rw != (1 << BIO_RW));
	BUG_ON(pkt->bio->bi_vcnt != pkt->frames);
	BUG_ON(pkt->bio->bi_size != pkt->frames * CD_FRAMESIZE);
	BUG_ON(pkt->bio->bi_end_io != pkt_end_io_packet_write);
	BUG_ON(pkt->bio->bi_private != pkt);

	drop_super(sb);
	return 1;

out:
	drop_super(sb);
	return 0;
#endif
}

static inline void pkt_set_state(struct packet_data *pkt, enum packet_data_state state)
{
#if PACKET_DEBUG > 1
	static const char *state_name[] = {
		"IDLE", "WAITING", "READ_WAIT", "WRITE_WAIT", "RECOVERY", "FINISHED"
	};
	enum packet_data_state old_state = pkt->state;
	VPRINTK("pkt %2d : s=%6llx %s -> %s\n", pkt->id, (unsigned long long)pkt->sector,
		state_name[old_state], state_name[state]);
#endif
	pkt->state = state;
}

/*
 * Scan the work queue to see if we can start a new packet.
 * returns non-zero if any work was done.
 */
static int pkt_handle_queue(struct pktcdvd_device *pd)
{
	struct packet_data *pkt, *p;
	struct bio *bio = NULL;
	sector_t zone = 0; /* Suppress gcc warning */
	struct pkt_rb_node *node, *first_node;
	struct rb_node *n;
	int wakeup;

	VPRINTK("handle_queue\n");

	atomic_set(&pd->scan_queue, 0);

	if (list_empty(&pd->cdrw.pkt_free_list)) {
		VPRINTK("handle_queue: no pkt\n");
		return 0;
	}

	/*
	 * Try to find a zone we are not already working on.
	 */
	spin_lock(&pd->lock);
	first_node = pkt_rbtree_find(pd, pd->current_sector);
	if (!first_node) {
		n = rb_first(&pd->bio_queue);
		if (n)
			first_node = rb_entry(n, struct pkt_rb_node, rb_node);
	}
	node = first_node;
	while (node) {
		bio = node->bio;
		zone = ZONE(bio->bi_sector, pd);
		list_for_each_entry(p, &pd->cdrw.pkt_active_list, list) {
			if (p->sector == zone) {
				bio = NULL;
				goto try_next_bio;
			}
		}
		break;
try_next_bio:
		node = pkt_rbtree_next(node);
		if (!node) {
			n = rb_first(&pd->bio_queue);
			if (n)
				node = rb_entry(n, struct pkt_rb_node, rb_node);
		}
		if (node == first_node)
			node = NULL;
	}
	spin_unlock(&pd->lock);
	if (!bio) {
		VPRINTK("handle_queue: no bio\n");
		return 0;
	}

	pkt = pkt_get_packet_data(pd, zone);

	pd->current_sector = zone + pd->settings.size;
	pkt->sector = zone;
	BUG_ON(pkt->frames != pd->settings.size >> 2);
	pkt->write_size = 0;

	/*
	 * Scan work queue for bios in the same zone and link them
	 * to this packet.
	 */
	spin_lock(&pd->lock);
	VPRINTK("pkt_handle_queue: looking for zone %llx\n", (unsigned long long)zone);
	while ((node = pkt_rbtree_find(pd, zone)) != NULL) {
		bio = node->bio;
		VPRINTK("pkt_handle_queue: found zone=%llx\n",
			(unsigned long long)ZONE(bio->bi_sector, pd));
		if (ZONE(bio->bi_sector, pd) != zone)
			break;
		pkt_rbtree_erase(pd, node);
		spin_lock(&pkt->lock);
		pkt_add_list_last(bio, &pkt->orig_bios, &pkt->orig_bios_tail);
		pkt->write_size += bio->bi_size / CD_FRAMESIZE;
		spin_unlock(&pkt->lock);
	}
	/* check write congestion marks, and if bio_queue_size is
	   below, wake up any waiters */
	wakeup = (pd->write_congestion_on > 0
	 		&& pd->bio_queue_size <= pd->write_congestion_off);
	spin_unlock(&pd->lock);
	if (wakeup)
		clear_bdi_congested(&pd->disk->queue->backing_dev_info, WRITE);

	pkt->sleep_time = max(PACKET_WAIT_TIME, 1);
	pkt_set_state(pkt, PACKET_WAITING_STATE);
	atomic_set(&pkt->run_sm, 1);

	spin_lock(&pd->cdrw.active_list_lock);
	list_add(&pkt->list, &pd->cdrw.pkt_active_list);
	spin_unlock(&pd->cdrw.active_list_lock);

	return 1;
}

/*
 * Assemble a bio to write one packet and queue the bio for processing
 * by the underlying block device.
 */
static void pkt_start_write(struct pktcdvd_device *pd, struct packet_data *pkt)
{
	struct bio *bio;
	int f;
	int frames_write;
	struct bio_vec *bvec = pkt->w_bio->bi_io_vec;

	for (f = 0; f < pkt->frames; f++) {
		bvec[f].bv_page = pkt->pages[(f * CD_FRAMESIZE) / PAGE_SIZE];
		bvec[f].bv_offset = (f * CD_FRAMESIZE) % PAGE_SIZE;
	}

	/*
	 * Fill-in bvec with data from orig_bios.
	 */
	frames_write = 0;
	spin_lock(&pkt->lock);
	for (bio = pkt->orig_bios; bio; bio = bio->bi_next) {
		int segment = bio->bi_idx;
		int src_offs = 0;
		int first_frame = (bio->bi_sector - pkt->sector) / (CD_FRAMESIZE >> 9);
		int num_frames = bio->bi_size / CD_FRAMESIZE;
		BUG_ON(first_frame < 0);
		BUG_ON(first_frame + num_frames > pkt->frames);
		for (f = first_frame; f < first_frame + num_frames; f++) {
			struct bio_vec *src_bvl = bio_iovec_idx(bio, segment);

			while (src_offs >= src_bvl->bv_len) {
				src_offs -= src_bvl->bv_len;
				segment++;
				BUG_ON(segment >= bio->bi_vcnt);
				src_bvl = bio_iovec_idx(bio, segment);
			}

			if (src_bvl->bv_len - src_offs >= CD_FRAMESIZE) {
				bvec[f].bv_page = src_bvl->bv_page;
				bvec[f].bv_offset = src_bvl->bv_offset + src_offs;
			} else {
				pkt_copy_bio_data(bio, segment, src_offs,
						  bvec[f].bv_page, bvec[f].bv_offset);
			}
			src_offs += CD_FRAMESIZE;
			frames_write++;
		}
	}
	pkt_set_state(pkt, PACKET_WRITE_WAIT_STATE);
	spin_unlock(&pkt->lock);

	VPRINTK("pkt_start_write: Writing %d frames for zone %llx\n",
		frames_write, (unsigned long long)pkt->sector);
	BUG_ON(frames_write != pkt->write_size);

	if (test_bit(PACKET_MERGE_SEGS, &pd->flags) || (pkt->write_size < pkt->frames)) {
		pkt_make_local_copy(pkt, bvec);
		pkt->cache_valid = 1;
	} else {
		pkt->cache_valid = 0;
	}

	/* Start the write request */
	bio_init(pkt->w_bio);
	pkt->w_bio->bi_max_vecs = PACKET_MAX_SIZE;
	pkt->w_bio->bi_sector = pkt->sector;
	pkt->w_bio->bi_bdev = pd->bdev;
	pkt->w_bio->bi_end_io = pkt_end_io_packet_write;
	pkt->w_bio->bi_private = pkt;
	pkt->w_bio->bi_io_vec = bvec;
	pkt->w_bio->bi_destructor = pkt_bio_destructor;
	for (f = 0; f < pkt->frames; f++)
		if (!bio_add_page(pkt->w_bio, bvec[f].bv_page, CD_FRAMESIZE, bvec[f].bv_offset))
			BUG();
	VPRINTK(DRIVER_NAME": vcnt=%d\n", pkt->w_bio->bi_vcnt);

	atomic_set(&pkt->io_wait, 1);
	pkt->w_bio->bi_rw = WRITE;
	pkt_queue_bio(pd, pkt->w_bio);
}

static void pkt_finish_packet(struct packet_data *pkt, int uptodate)
{
	struct bio *bio, *next;

	if (!uptodate)
		pkt->cache_valid = 0;

	/* Finish all bios corresponding to this packet */
	bio = pkt->orig_bios;
	while (bio) {
		next = bio->bi_next;
		bio->bi_next = NULL;
		bio_endio(bio, uptodate ? 0 : -EIO);
		bio = next;
	}
	pkt->orig_bios = pkt->orig_bios_tail = NULL;
}

static void pkt_run_state_machine(struct pktcdvd_device *pd, struct packet_data *pkt)
{
	int uptodate;

	VPRINTK("run_state_machine: pkt %d\n", pkt->id);

	for (;;) {
		switch (pkt->state) {
		case PACKET_WAITING_STATE:
			if ((pkt->write_size < pkt->frames) && (pkt->sleep_time > 0))
				return;

			pkt->sleep_time = 0;
			pkt_gather_data(pd, pkt);
			pkt_set_state(pkt, PACKET_READ_WAIT_STATE);
			break;

		case PACKET_READ_WAIT_STATE:
			if (atomic_read(&pkt->io_wait) > 0)
				return;

			if (atomic_read(&pkt->io_errors) > 0) {
				pkt_set_state(pkt, PACKET_RECOVERY_STATE);
			} else {
				pkt_start_write(pd, pkt);
			}
			break;

		case PACKET_WRITE_WAIT_STATE:
			if (atomic_read(&pkt->io_wait) > 0)
				return;

			if (test_bit(BIO_UPTODATE, &pkt->w_bio->bi_flags)) {
				pkt_set_state(pkt, PACKET_FINISHED_STATE);
			} else {
				pkt_set_state(pkt, PACKET_RECOVERY_STATE);
			}
			break;

		case PACKET_RECOVERY_STATE:
			if (pkt_start_recovery(pkt)) {
				pkt_start_write(pd, pkt);
			} else {
				VPRINTK("No recovery possible\n");
				pkt_set_state(pkt, PACKET_FINISHED_STATE);
			}
			break;

		case PACKET_FINISHED_STATE:
			uptodate = test_bit(BIO_UPTODATE, &pkt->w_bio->bi_flags);
			pkt_finish_packet(pkt, uptodate);
			return;

		default:
			BUG();
			break;
		}
	}
}

static void pkt_handle_packets(struct pktcdvd_device *pd)
{
	struct packet_data *pkt, *next;

	VPRINTK("pkt_handle_packets\n");

	/*
	 * Run state machine for active packets
	 */
	list_for_each_entry(pkt, &pd->cdrw.pkt_active_list, list) {
		if (atomic_read(&pkt->run_sm) > 0) {
			atomic_set(&pkt->run_sm, 0);
			pkt_run_state_machine(pd, pkt);
		}
	}

	/*
	 * Move no longer active packets to the free list
	 */
	spin_lock(&pd->cdrw.active_list_lock);
	list_for_each_entry_safe(pkt, next, &pd->cdrw.pkt_active_list, list) {
		if (pkt->state == PACKET_FINISHED_STATE) {
			list_del(&pkt->list);
			pkt_put_packet_data(pd, pkt);
			pkt_set_state(pkt, PACKET_IDLE_STATE);
			atomic_set(&pd->scan_queue, 1);
		}
	}
	spin_unlock(&pd->cdrw.active_list_lock);
}

static void pkt_count_states(struct pktcdvd_device *pd, int *states)
{
	struct packet_data *pkt;
	int i;

	for (i = 0; i < PACKET_NUM_STATES; i++)
		states[i] = 0;

	spin_lock(&pd->cdrw.active_list_lock);
	list_for_each_entry(pkt, &pd->cdrw.pkt_active_list, list) {
		states[pkt->state]++;
	}
	spin_unlock(&pd->cdrw.active_list_lock);
}

/*
 * kcdrwd is woken up when writes have been queued for one of our
 * registered devices
 */
static int kcdrwd(void *foobar)
{
	struct pktcdvd_device *pd = foobar;
	struct packet_data *pkt;
	long min_sleep_time, residue;

	set_user_nice(current, -20);
	set_freezable();

	for (;;) {
		DECLARE_WAITQUEUE(wait, current);

		/*
		 * Wait until there is something to do
		 */
		add_wait_queue(&pd->wqueue, &wait);
		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);

			/* Check if we need to run pkt_handle_queue */
			if (atomic_read(&pd->scan_queue) > 0)
				goto work_to_do;

			/* Check if we need to run the state machine for some packet */
			list_for_each_entry(pkt, &pd->cdrw.pkt_active_list, list) {
				if (atomic_read(&pkt->run_sm) > 0)
					goto work_to_do;
			}

			/* Check if we need to process the iosched queues */
			if (atomic_read(&pd->iosched.attention) != 0)
				goto work_to_do;

			/* Otherwise, go to sleep */
			if (PACKET_DEBUG > 1) {
				int states[PACKET_NUM_STATES];
				pkt_count_states(pd, states);
				VPRINTK("kcdrwd: i:%d ow:%d rw:%d ww:%d rec:%d fin:%d\n",
					states[0], states[1], states[2], states[3],
					states[4], states[5]);
			}

			min_sleep_time = MAX_SCHEDULE_TIMEOUT;
			list_for_each_entry(pkt, &pd->cdrw.pkt_active_list, list) {
				if (pkt->sleep_time && pkt->sleep_time < min_sleep_time)
					min_sleep_time = pkt->sleep_time;
			}

			generic_unplug_device(bdev_get_queue(pd->bdev));

			VPRINTK("kcdrwd: sleeping\n");
			residue = schedule_timeout(min_sleep_time);
			VPRINTK("kcdrwd: wake up\n");

			/* make swsusp happy with our thread */
			try_to_freeze();

			list_for_each_entry(pkt, &pd->cdrw.pkt_active_list, list) {
				if (!pkt->sleep_time)
					continue;
				pkt->sleep_time -= min_sleep_time - residue;
				if (pkt->sleep_time <= 0) {
					pkt->sleep_time = 0;
					atomic_inc(&pkt->run_sm);
				}
			}

			if (kthread_should_stop())
				break;
		}
work_to_do:
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&pd->wqueue, &wait);

		if (kthread_should_stop())
			break;

		/*
		 * if pkt_handle_queue returns true, we can queue
		 * another request.
		 */
		while (pkt_handle_queue(pd))
			;

		/*
		 * Handle packet state machine
		 */
		pkt_handle_packets(pd);

		/*
		 * Handle iosched queues
		 */
		pkt_iosched_process_queue(pd);
	}

	return 0;
}

static void pkt_print_settings(struct pktcdvd_device *pd)
{
	printk(DRIVER_NAME": %s packets, ", pd->settings.fp ? "Fixed" : "Variable");
	printk("%u blocks, ", pd->settings.size >> 2);
	printk("Mode-%c disc\n", pd->settings.block_mode == 8 ? '1' : '2');
}

static int pkt_mode_sense(struct pktcdvd_device *pd, struct packet_command *cgc, int page_code, int page_control)
{
	memset(cgc->cmd, 0, sizeof(cgc->cmd));

	cgc->cmd[0] = GPCMD_MODE_SENSE_10;
	cgc->cmd[2] = page_code | (page_control << 6);
	cgc->cmd[7] = cgc->buflen >> 8;
	cgc->cmd[8] = cgc->buflen & 0xff;
	cgc->data_direction = CGC_DATA_READ;
	return pkt_generic_packet(pd, cgc);
}

static int pkt_mode_select(struct pktcdvd_device *pd, struct packet_command *cgc)
{
	memset(cgc->cmd, 0, sizeof(cgc->cmd));
	memset(cgc->buffer, 0, 2);
	cgc->cmd[0] = GPCMD_MODE_SELECT_10;
	cgc->cmd[1] = 0x10;		/* PF */
	cgc->cmd[7] = cgc->buflen >> 8;
	cgc->cmd[8] = cgc->buflen & 0xff;
	cgc->data_direction = CGC_DATA_WRITE;
	return pkt_generic_packet(pd, cgc);
}

static int pkt_get_disc_info(struct pktcdvd_device *pd, disc_information *di)
{
	struct packet_command cgc;
	int ret;

	/* set up command and get the disc info */
	init_cdrom_command(&cgc, di, sizeof(*di), CGC_DATA_READ);
	cgc.cmd[0] = GPCMD_READ_DISC_INFO;
	cgc.cmd[8] = cgc.buflen = 2;
	cgc.quiet = 1;

	if ((ret = pkt_generic_packet(pd, &cgc)))
		return ret;

	/* not all drives have the same disc_info length, so requeue
	 * packet with the length the drive tells us it can supply
	 */
	cgc.buflen = be16_to_cpu(di->disc_information_length) +
		     sizeof(di->disc_information_length);

	if (cgc.buflen > sizeof(disc_information))
		cgc.buflen = sizeof(disc_information);

	cgc.cmd[8] = cgc.buflen;
	return pkt_generic_packet(pd, &cgc);
}

static int pkt_get_track_info(struct pktcdvd_device *pd, __u16 track, __u8 type, track_information *ti)
{
	struct packet_command cgc;
	int ret;

	init_cdrom_command(&cgc, ti, 8, CGC_DATA_READ);
	cgc.cmd[0] = GPCMD_READ_TRACK_RZONE_INFO;
	cgc.cmd[1] = type & 3;
	cgc.cmd[4] = (track & 0xff00) >> 8;
	cgc.cmd[5] = track & 0xff;
	cgc.cmd[8] = 8;
	cgc.quiet = 1;

	if ((ret = pkt_generic_packet(pd, &cgc)))
		return ret;

	cgc.buflen = be16_to_cpu(ti->track_information_length) +
		     sizeof(ti->track_information_length);

	if (cgc.buflen > sizeof(track_information))
		cgc.buflen = sizeof(track_information);

	cgc.cmd[8] = cgc.buflen;
	return pkt_generic_packet(pd, &cgc);
}

static noinline_for_stack int pkt_get_last_written(struct pktcdvd_device *pd,
						long *last_written)
{
	disc_information di;
	track_information ti;
	__u32 last_track;
	int ret = -1;

	if ((ret = pkt_get_disc_info(pd, &di)))
		return ret;

	last_track = (di.last_track_msb << 8) | di.last_track_lsb;
	if ((ret = pkt_get_track_info(pd, last_track, 1, &ti)))
		return ret;

	/* if this track is blank, try the previous. */
	if (ti.blank) {
		last_track--;
		if ((ret = pkt_get_track_info(pd, last_track, 1, &ti)))
			return ret;
	}

	/* if last recorded field is valid, return it. */
	if (ti.lra_v) {
		*last_written = be32_to_cpu(ti.last_rec_address);
	} else {
		/* make it up instead */
		*last_written = be32_to_cpu(ti.track_start) +
				be32_to_cpu(ti.track_size);
		if (ti.free_blocks)
			*last_written -= (be32_to_cpu(ti.free_blocks) + 7);
	}
	return 0;
}

/*
 * write mode select package based on pd->settings
 */
static noinline_for_stack int pkt_set_write_settings(struct pktcdvd_device *pd)
{
	struct packet_command cgc;
	struct request_sense sense;
	write_param_page *wp;
	char buffer[128];
	int ret, size;

	/* doesn't apply to DVD+RW or DVD-RAM */
	if ((pd->mmc3_profile == 0x1a) || (pd->mmc3_profile == 0x12))
		return 0;

	memset(buffer, 0, sizeof(buffer));
	init_cdrom_command(&cgc, buffer, sizeof(*wp), CGC_DATA_READ);
	cgc.sense = &sense;
	if ((ret = pkt_mode_sense(pd, &cgc, GPMODE_WRITE_PARMS_PAGE, 0))) {
		pkt_dump_sense(&cgc);
		return ret;
	}

	size = 2 + ((buffer[0] << 8) | (buffer[1] & 0xff));
	pd->mode_offset = (buffer[6] << 8) | (buffer[7] & 0xff);
	if (size > sizeof(buffer))
		size = sizeof(buffer);

	/*
	 * now get it all
	 */
	init_cdrom_command(&cgc, buffer, size, CGC_DATA_READ);
	cgc.sense = &sense;
	if ((ret = pkt_mode_sense(pd, &cgc, GPMODE_WRITE_PARMS_PAGE, 0))) {
		pkt_dump_sense(&cgc);
		return ret;
	}

	/*
	 * write page is offset header + block descriptor length
	 */
	wp = (write_param_page *) &buffer[sizeof(struct mode_page_header) + pd->mode_offset];

	wp->fp = pd->settings.fp;
	wp->track_mode = pd->settings.track_mode;
	wp->write_type = pd->settings.write_type;
	wp->data_block_type = pd->settings.block_mode;

	wp->multi_session = 0;

#ifdef PACKET_USE_LS
	wp->link_size = 7;
	wp->ls_v = 1;
#endif

	if (wp->data_block_type == PACKET_BLOCK_MODE1) {
		wp->session_format = 0;
		wp->subhdr2 = 0x20;
	} else if (wp->data_block_type == PACKET_BLOCK_MODE2) {
		wp->session_format = 0x20;
		wp->subhdr2 = 8;
#if 0
		wp->mcn[0] = 0x80;
		memcpy(&wp->mcn[1], PACKET_MCN, sizeof(wp->mcn) - 1);
#endif
	} else {
		/*
		 * paranoia
		 */
		printk(DRIVER_NAME": write mode wrong %d\n", wp->data_block_type);
		return 1;
	}
	wp->packet_size = cpu_to_be32(pd->settings.size >> 2);

	cgc.buflen = cgc.cmd[8] = size;
	if ((ret = pkt_mode_select(pd, &cgc))) {
		pkt_dump_sense(&cgc);
		return ret;
	}

	pkt_print_settings(pd);
	return 0;
}

/*
 * 1 -- we can write to this track, 0 -- we can't
 */
static int pkt_writable_track(struct pktcdvd_device *pd, track_information *ti)
{
	switch (pd->mmc3_profile) {
		case 0x1a: /* DVD+RW */
		case 0x12: /* DVD-RAM */
			/* The track is always writable on DVD+RW/DVD-RAM */
			return 1;
		default:
			break;
	}

	if (!ti->packet || !ti->fp)
		return 0;

	/*
	 * "good" settings as per Mt Fuji.
	 */
	if (ti->rt == 0 && ti->blank == 0)
		return 1;

	if (ti->rt == 0 && ti->blank == 1)
		return 1;

	if (ti->rt == 1 && ti->blank == 0)
		return 1;

	printk(DRIVER_NAME": bad state %d-%d-%d\n", ti->rt, ti->blank, ti->packet);
	return 0;
}

/*
 * 1 -- we can write to this disc, 0 -- we can't
 */
static int pkt_writable_disc(struct pktcdvd_device *pd, disc_information *di)
{
	switch (pd->mmc3_profile) {
		case 0x0a: /* CD-RW */
		case 0xffff: /* MMC3 not supported */
			break;
		case 0x1a: /* DVD+RW */
		case 0x13: /* DVD-RW */
		case 0x12: /* DVD-RAM */
			return 1;
		default:
			VPRINTK(DRIVER_NAME": Wrong disc profile (%x)\n", pd->mmc3_profile);
			return 0;
	}

	/*
	 * for disc type 0xff we should probably reserve a new track.
	 * but i'm not sure, should we leave this to user apps? probably.
	 */
	if (di->disc_type == 0xff) {
		printk(DRIVER_NAME": Unknown disc. No track?\n");
		return 0;
	}

	if (di->disc_type != 0x20 && di->disc_type != 0) {
		printk(DRIVER_NAME": Wrong disc type (%x)\n", di->disc_type);
		return 0;
	}

	if (di->erasable == 0) {
		printk(DRIVER_NAME": Disc not erasable\n");
		return 0;
	}

	if (di->border_status == PACKET_SESSION_RESERVED) {
		printk(DRIVER_NAME": Can't write to last track (reserved)\n");
		return 0;
	}

	return 1;
}

static noinline_for_stack int pkt_probe_settings(struct pktcdvd_device *pd)
{
	struct packet_command cgc;
	unsigned char buf[12];
	disc_information di;
	track_information ti;
	int ret, track;

	init_cdrom_command(&cgc, buf, sizeof(buf), CGC_DATA_READ);
	cgc.cmd[0] = GPCMD_GET_CONFIGURATION;
	cgc.cmd[8] = 8;
	ret = pkt_generic_packet(pd, &cgc);
	pd->mmc3_profile = ret ? 0xffff : buf[6] << 8 | buf[7];

	memset(&di, 0, sizeof(disc_information));
	memset(&ti, 0, sizeof(track_information));

	if ((ret = pkt_get_disc_info(pd, &di))) {
		printk("failed get_disc\n");
		return ret;
	}

	if (!pkt_writable_disc(pd, &di))
		return -EROFS;

	pd->type = di.erasable ? PACKET_CDRW : PACKET_CDR;

	track = 1; /* (di.last_track_msb << 8) | di.last_track_lsb; */
	if ((ret = pkt_get_track_info(pd, track, 1, &ti))) {
		printk(DRIVER_NAME": failed get_track\n");
		return ret;
	}

	if (!pkt_writable_track(pd, &ti)) {
		printk(DRIVER_NAME": can't write to this track\n");
		return -EROFS;
	}

	/*
	 * we keep packet size in 512 byte units, makes it easier to
	 * deal with request calculations.
	 */
	pd->settings.size = be32_to_cpu(ti.fixed_packet_size) << 2;
	if (pd->settings.size == 0) {
		printk(DRIVER_NAME": detected zero packet size!\n");
		return -ENXIO;
	}
	if (pd->settings.size > PACKET_MAX_SECTORS) {
		printk(DRIVER_NAME": packet size is too big\n");
		return -EROFS;
	}
	pd->settings.fp = ti.fp;
	pd->offset = (be32_to_cpu(ti.track_start) << 2) & (pd->settings.size - 1);

	if (ti.nwa_v) {
		pd->nwa = be32_to_cpu(ti.next_writable);
		set_bit(PACKET_NWA_VALID, &pd->flags);
	}

	/*
	 * in theory we could use lra on -RW media as well and just zero
	 * blocks that haven't been written yet, but in practice that
	 * is just a no-go. we'll use that for -R, naturally.
	 */
	if (ti.lra_v) {
		pd->lra = be32_to_cpu(ti.last_rec_address);
		set_bit(PACKET_LRA_VALID, &pd->flags);
	} else {
		pd->lra = 0xffffffff;
		set_bit(PACKET_LRA_VALID, &pd->flags);
	}

	/*
	 * fine for now
	 */
	pd->settings.link_loss = 7;
	pd->settings.write_type = 0;	/* packet */
	pd->settings.track_mode = ti.track_mode;

	/*
	 * mode1 or mode2 disc
	 */
	switch (ti.data_mode) {
		case PACKET_MODE1:
			pd->settings.block_mode = PACKET_BLOCK_MODE1;
			break;
		case PACKET_MODE2:
			pd->settings.block_mode = PACKET_BLOCK_MODE2;
			break;
		default:
			printk(DRIVER_NAME": unknown data mode\n");
			return -EROFS;
	}
	return 0;
}

/*
 * enable/disable write caching on drive
 */
static noinline_for_stack int pkt_write_caching(struct pktcdvd_device *pd,
						int set)
{
	struct packet_command cgc;
	struct request_sense sense;
	unsigned char buf[64];
	int ret;

	init_cdrom_command(&cgc, buf, sizeof(buf), CGC_DATA_READ);
	cgc.sense = &sense;
	cgc.buflen = pd->mode_offset + 12;

	/*
	 * caching mode page might not be there, so quiet this command
	 */
	cgc.quiet = 1;

	if ((ret = pkt_mode_sense(pd, &cgc, GPMODE_WCACHING_PAGE, 0)))
		return ret;

	buf[pd->mode_offset + 10] |= (!!set << 2);

	cgc.buflen = cgc.cmd[8] = 2 + ((buf[0] << 8) | (buf[1] & 0xff));
	ret = pkt_mode_select(pd, &cgc);
	if (ret) {
		printk(DRIVER_NAME": write caching control failed\n");
		pkt_dump_sense(&cgc);
	} else if (!ret && set)
		printk(DRIVER_NAME": enabled write caching on %s\n", pd->name);
	return ret;
}

static int pkt_lock_door(struct pktcdvd_device *pd, int lockflag)
{
	struct packet_command cgc;

	init_cdrom_command(&cgc, NULL, 0, CGC_DATA_NONE);
	cgc.cmd[0] = GPCMD_PREVENT_ALLOW_MEDIUM_REMOVAL;
	cgc.cmd[4] = lockflag ? 1 : 0;
	return pkt_generic_packet(pd, &cgc);
}

/*
 * Returns drive maximum write speed
 */
static noinline_for_stack int pkt_get_max_speed(struct pktcdvd_device *pd,
						unsigned *write_speed)
{
	struct packet_command cgc;
	struct request_sense sense;
	unsigned char buf[256+18];
	unsigned char *cap_buf;
	int ret, offset;

	cap_buf = &buf[sizeof(struct mode_page_header) + pd->mode_offset];
	init_cdrom_command(&cgc, buf, sizeof(buf), CGC_DATA_UNKNOWN);
	cgc.sense = &sense;

	ret = pkt_mode_sense(pd, &cgc, GPMODE_CAPABILITIES_PAGE, 0);
	if (ret) {
		cgc.buflen = pd->mode_offset + cap_buf[1] + 2 +
			     sizeof(struct mode_page_header);
		ret = pkt_mode_sense(pd, &cgc, GPMODE_CAPABILITIES_PAGE, 0);
		if (ret) {
			pkt_dump_sense(&cgc);
			return ret;
		}
	}

	offset = 20;			    /* Obsoleted field, used by older drives */
	if (cap_buf[1] >= 28)
		offset = 28;		    /* Current write speed selected */
	if (cap_buf[1] >= 30) {
		/* If the drive reports at least one "Logical Unit Write
		 * Speed Performance Descriptor Block", use the information
		 * in the first block. (contains the highest speed)
		 */
		int num_spdb = (cap_buf[30] << 8) + cap_buf[31];
		if (num_spdb > 0)
			offset = 34;
	}

	*write_speed = (cap_buf[offset] << 8) | cap_buf[offset + 1];
	return 0;
}

/* These tables from cdrecord - I don't have orange book */
/* standard speed CD-RW (1-4x) */
static char clv_to_speed[16] = {
	/* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	   0, 2, 4, 6, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
/* high speed CD-RW (-10x) */
static char hs_clv_to_speed[16] = {
	/* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	   0, 2, 4, 6, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
/* ultra high speed CD-RW */
static char us_clv_to_speed[16] = {
	/* 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
	   0, 2, 4, 8, 0, 0,16, 0,24,32,40,48, 0, 0, 0, 0
};

/*
 * reads the maximum media speed from ATIP
 */
static noinline_for_stack int pkt_media_speed(struct pktcdvd_device *pd,
						unsigned *speed)
{
	struct packet_command cgc;
	struct request_sense sense;
	unsigned char buf[64];
	unsigned int size, st, sp;
	int ret;

	init_cdrom_command(&cgc, buf, 2, CGC_DATA_READ);
	cgc.sense = &sense;
	cgc.cmd[0] = GPCMD_READ_TOC_PMA_ATIP;
	cgc.cmd[1] = 2;
	cgc.cmd[2] = 4; /* READ ATIP */
	cgc.cmd[8] = 2;
	ret = pkt_generic_packet(pd, &cgc);
	if (ret) {
		pkt_dump_sense(&cgc);
		return ret;
	}
	size = ((unsigned int) buf[0]<<8) + buf[1] + 2;
	if (size > sizeof(buf))
		size = sizeof(buf);

	init_cdrom_command(&cgc, buf, size, CGC_DATA_READ);
	cgc.sense = &sense;
	cgc.cmd[0] = GPCMD_READ_TOC_PMA_ATIP;
	cgc.cmd[1] = 2;
	cgc.cmd[2] = 4;
	cgc.cmd[8] = size;
	ret = pkt_generic_packet(pd, &cgc);
	if (ret) {
		pkt_dump_sense(&cgc);
		return ret;
	}

	if (!(buf[6] & 0x40)) {
		printk(DRIVER_NAME": Disc type is not CD-RW\n");
		return 1;
	}
	if (!(buf[6] & 0x4)) {
		printk(DRIVER_NAME": A1 values on media are not valid, maybe not CDRW?\n");
		return 1;
	}

	st = (buf[6] >> 3) & 0x7; /* disc sub-type */

	sp = buf[16] & 0xf; /* max speed from ATIP A1 field */

	/* Info from cdrecord */
	switch (st) {
		case 0: /* standard speed */
			*speed = clv_to_speed[sp];
			break;
		case 1: /* high speed */
			*speed = hs_clv_to_speed[sp];
			break;
		case 2: /* ultra high speed */
			*speed = us_clv_to_speed[sp];
			break;
		default:
			printk(DRIVER_NAME": Unknown disc sub-type %d\n",st);
			return 1;
	}
	if (*speed) {
		printk(DRIVER_NAME": Max. media speed: %d\n",*speed);
		return 0;
	} else {
		printk(DRIVER_NAME": Unknown speed %d for sub-type %d\n",sp,st);
		return 1;
	}
}

static noinline_for_stack int pkt_perform_opc(struct pktcdvd_device *pd)
{
	struct packet_command cgc;
	struct request_sense sense;
	int ret;

	VPRINTK(DRIVER_NAME": Performing OPC\n");

	init_cdrom_command(&cgc, NULL, 0, CGC_DATA_NONE);
	cgc.sense = &sense;
	cgc.timeout = 60*HZ;
	cgc.cmd[0] = GPCMD_SEND_OPC;
	cgc.cmd[1] = 1;
	if ((ret = pkt_generic_packet(pd, &cgc)))
		pkt_dump_sense(&cgc);
	return ret;
}

static int pkt_open_write(struct pktcdvd_device *pd)
{
	int ret;
	unsigned int write_speed, media_write_speed, read_speed;

	if ((ret = pkt_probe_settings(pd))) {
		VPRINTK(DRIVER_NAME": %s failed probe\n", pd->name);
		return ret;
	}

	if ((ret = pkt_set_write_settings(pd))) {
		DPRINTK(DRIVER_NAME": %s failed saving write settings\n", pd->name);
		return -EIO;
	}

	pkt_write_caching(pd, USE_WCACHING);

	if ((ret = pkt_get_max_speed(pd, &write_speed)))
		write_speed = 16 * 177;
	switch (pd->mmc3_profile) {
		case 0x13: /* DVD-RW */
		case 0x1a: /* DVD+RW */
		case 0x12: /* DVD-RAM */
			DPRINTK(DRIVER_NAME": write speed %ukB/s\n", write_speed);
			break;
		default:
			if ((ret = pkt_media_speed(pd, &media_write_speed)))
				media_write_speed = 16;
			write_speed = min(write_speed, media_write_speed * 177);
			DPRINTK(DRIVER_NAME": write speed %ux\n", write_speed / 176);
			break;
	}
	read_speed = write_speed;

	if ((ret = pkt_set_speed(pd, write_speed, read_speed))) {
		DPRINTK(DRIVER_NAME": %s couldn't set write speed\n", pd->name);
		return -EIO;
	}
	pd->write_speed = write_speed;
	pd->read_speed = read_speed;

	if ((ret = pkt_perform_opc(pd))) {
		DPRINTK(DRIVER_NAME": %s Optimum Power Calibration failed\n", pd->name);
	}

	return 0;
}

/*
 * called at open time.
 */
static int pkt_open_dev(struct pktcdvd_device *pd, fmode_t write)
{
	int ret;
	long lba;
	struct request_queue *q;

	/*
	 * We need to re-open the cdrom device without O_NONBLOCK to be able
	 * to read/write from/to it. It is already opened in O_NONBLOCK mode
	 * so bdget() can't fail.
	 */
	bdget(pd->bdev->bd_dev);
	if ((ret = blkdev_get(pd->bdev, FMODE_READ, O_RDONLY)))
		goto out;

	if ((ret = bd_claim(pd->bdev, pd)))
		goto out_putdev;

	if ((ret = pkt_get_last_written(pd, &lba))) {
		printk(DRIVER_NAME": pkt_get_last_written failed\n");
		goto out_unclaim;
	}

	set_capacity(pd->disk, lba << 2);
	set_capacity(pd->bdev->bd_disk, lba << 2);
	bd_set_size(pd->bdev, (loff_t)lba << 11);

	q = bdev_get_queue(pd->bdev);
	if (write) {
		if ((ret = pkt_open_write(pd)))
			goto out_unclaim;
		/*
		 * Some CDRW drives can not handle writes larger than one packet,
		 * even if the size is a multiple of the packet size.
		 */
		spin_lock_irq(q->queue_lock);
		blk_queue_max_sectors(q, pd->settings.size);
		spin_unlock_irq(q->queue_lock);
		set_bit(PACKET_WRITABLE, &pd->flags);
	} else {
		pkt_set_speed(pd, MAX_SPEED, MAX_SPEED);
		clear_bit(PACKET_WRITABLE, &pd->flags);
	}

	if ((ret = pkt_set_segment_merging(pd, q)))
		goto out_unclaim;

	if (write) {
		if (!pkt_grow_pktlist(pd, CONFIG_CDROM_PKTCDVD_BUFFERS)) {
			printk(DRIVER_NAME": not enough memory for buffers\n");
			ret = -ENOMEM;
			goto out_unclaim;
		}
		printk(DRIVER_NAME": %lukB available on disc\n", lba << 1);
	}

	return 0;

out_unclaim:
	bd_release(pd->bdev);
out_putdev:
	blkdev_put(pd->bdev);
out:
	return ret;
}

/*
 * called when the device is closed. makes sure that the device flushes
 * the internal cache before we close.
 */
static void pkt_release_dev(struct pktcdvd_device *pd, int flush)
{
	if (flush && pkt_flush_cache(pd))
		DPRINTK(DRIVER_NAME": %s not flushing cache\n", pd->name);

	pkt_lock_door(pd, 0);

	pkt_set_speed(pd, MAX_SPEED, MAX_SPEED);
	bd_release(pd->bdev);
	blkdev_put(pd->bdev);

	pkt_shrink_pktlist(pd);
}

static struct pktcdvd_device *pkt_find_dev_from_minor(int dev_minor)
{
	if (dev_minor >= MAX_WRITERS)
		return NULL;
	return pkt_devs[dev_minor];
}

static int pkt_open(struct inode *inode, struct file *file)
{
	struct pktcdvd_device *pd = NULL;
	int ret;

	VPRINTK(DRIVER_NAME": entering open\n");

	mutex_lock(&ctl_mutex);
	pd = pkt_find_dev_from_minor(iminor(inode));
	if (!pd) {
		ret = -ENODEV;
		goto out;
	}
	BUG_ON(pd->refcnt < 0);

	pd->refcnt++;
	if (pd->refcnt > 1) {
		if ((file->f_mode & FMODE_WRITE) &&
		    !test_bit(PACKET_WRITABLE, &pd->flags)) {
			ret = -EBUSY;
			goto out_dec;
		}
	} else {
		ret = pkt_open_dev(pd, file->f_mode & FMODE_WRITE);
		if (ret)
			goto out_dec;
		/*
		 * needed here as well, since ext2 (among others) may change
		 * the blocksize at mount time
		 */
		set_blocksize(inode->i_bdev, CD_FRAMESIZE);
	}

	mutex_unlock(&ctl_mutex);
	return 0;

out_dec:
	pd->refcnt--;
out:
	VPRINTK(DRIVER_NAME": failed open (%d)\n", ret);
	mutex_unlock(&ctl_mutex);
	return ret;
}

static int pkt_close(struct inode *inode, struct file *file)
{
	struct pktcdvd_device *pd = inode->i_bdev->bd_disk->private_data;
	int ret = 0;

	mutex_lock(&ctl_mutex);
	pd->refcnt--;
	BUG_ON(pd->refcnt < 0);
	if (pd->refcnt == 0) {
		int flush = test_bit(PACKET_WRITABLE, &pd->flags);
		pkt_release_dev(pd, flush);
	}
	mutex_unlock(&ctl_mutex);
	return ret;
}


static void pkt_end_io_read_cloned(struct bio *bio, int err)
{
	struct packet_stacked_data *psd = bio->bi_private;
	struct pktcdvd_device *pd = psd->pd;

	bio_put(bio);
	bio_endio(psd->bio, err);
	mempool_free(psd, psd_pool);
	pkt_bio_finished(pd);
}

static int pkt_make_request(struct request_queue *q, struct bio *bio)
{
	struct pktcdvd_device *pd;
	char b[BDEVNAME_SIZE];
	sector_t zone;
	struct packet_data *pkt;
	int was_empty, blocked_bio;
	struct pkt_rb_node *node;

	pd = q->queuedata;
	if (!pd) {
		printk(DRIVER_NAME": %s incorrect request queue\n", bdevname(bio->bi_bdev, b));
		goto end_io;
	}

	/*
	 * Clone READ bios so we can have our own bi_end_io callback.
	 */
	if (bio_data_dir(bio) == READ) {
		struct bio *cloned_bio = bio_clone(bio, GFP_NOIO);
		struct packet_stacked_data *psd = mempool_alloc(psd_pool, GFP_NOIO);

		psd->pd = pd;
		psd->bio = bio;
		cloned_bio->bi_bdev = pd->bdev;
		cloned_bio->bi_private = psd;
		cloned_bio->bi_end_io = pkt_end_io_read_cloned;
		pd->stats.secs_r += bio->bi_size >> 9;
		pkt_queue_bio(pd, cloned_bio);
		return 0;
	}

	if (!test_bit(PACKET_WRITABLE, &pd->flags)) {
		printk(DRIVER_NAME": WRITE for ro device %s (%llu)\n",
			pd->name, (unsigned long long)bio->bi_sector);
		goto end_io;
	}

	if (!bio->bi_size || (bio->bi_size % CD_FRAMESIZE)) {
		printk(DRIVER_NAME": wrong bio size\n");
		goto end_io;
	}

	blk_queue_bounce(q, &bio);

	zone = ZONE(bio->bi_sector, pd);
	VPRINTK("pkt_make_request: start = %6llx stop = %6llx\n",
		(unsigned long long)bio->bi_sector,
		(unsigned long long)(bio->bi_sector + bio_sectors(bio)));

	/* Check if we have to split the bio */
	{
		struct bio_pair *bp;
		sector_t last_zone;
		int first_sectors;

		last_zone = ZONE(bio->bi_sector + bio_sectors(bio) - 1, pd);
		if (last_zone != zone) {
			BUG_ON(last_zone != zone + pd->settings.size);
			first_sectors = last_zone - bio->bi_sector;
			bp = bio_split(bio, first_sectors);
			BUG_ON(!bp);
			pkt_make_request(q, &bp->bio1);
			pkt_make_request(q, &bp->bio2);
			bio_pair_release(bp);
			return 0;
		}
	}

	/*
	 * If we find a matching packet in state WAITING or READ_WAIT, we can
	 * just append this bio to that packet.
	 */
	spin_lock(&pd->cdrw.active_list_lock);
	blocked_bio = 0;
	list_for_each_entry(pkt, &pd->cdrw.pkt_active_list, list) {
		if (pkt->sector == zone) {
			spin_lock(&pkt->lock);
			if ((pkt->state == PACKET_WAITING_STATE) ||
			    (pkt->state == PACKET_READ_WAIT_STATE)) {
				pkt_add_list_last(bio, &pkt->orig_bios,
						  &pkt->orig_bios_tail);
				pkt->write_size += bio->bi_size / CD_FRAMESIZE;
				if ((pkt->write_size >= pkt->frames) &&
				    (pkt->state == PACKET_WAITING_STATE)) {
					atomic_inc(&pkt->run_sm);
					wake_up(&pd->wqueue);
				}
				spin_unlock(&pkt->lock);
				spin_unlock(&pd->cdrw.active_list_lock);
				return 0;
			} else {
				blocked_bio = 1;
			}
			spin_unlock(&pkt->lock);
		}
	}
	spin_unlock(&pd->cdrw.active_list_lock);

 	/*
	 * Test if there is enough room left in the bio work queue
	 * (queue size >= congestion on mark).
	 * If not, wait till the work queue size is below the congestion off mark.
	 */
	spin_lock(&pd->lock);
	if (pd->write_congestion_on > 0
	    && pd->bio_queue_size >= pd->write_congestion_on) {
		set_bdi_congested(&q->backing_dev_info, WRITE);
		do {
			spin_unlock(&pd->lock);
			congestion_wait(WRITE, HZ);
			spin_lock(&pd->lock);
		} while(pd->bio_queue_size > pd->write_congestion_off);
	}
	spin_unlock(&pd->lock);

	/*
	 * No matching packet found. Store the bio in the work queue.
	 */
	node = mempool_alloc(pd->rb_pool, GFP_NOIO);
	node->bio = bio;
	spin_lock(&pd->lock);
	BUG_ON(pd->bio_queue_size < 0);
	was_empty = (pd->bio_queue_size == 0);
	pkt_rbtree_insert(pd, node);
	spin_unlock(&pd->lock);

	/*
	 * Wake up the worker thread.
	 */
	atomic_set(&pd->scan_queue, 1);
	if (was_empty) {
		/* This wake_up is required for correct operation */
		wake_up(&pd->wqueue);
	} else if (!list_empty(&pd->cdrw.pkt_free_list) && !blocked_bio) {
		/*
		 * This wake up is not required for correct operation,
		 * but improves performance in some cases.
		 */
		wake_up(&pd->wqueue);
	}
	return 0;
end_io:
	bio_io_error(bio);
	return 0;
}



static int pkt_merge_bvec(struct request_queue *q, struct bvec_merge_data *bmd,
			  struct bio_vec *bvec)
{
	struct pktcdvd_device *pd = q->queuedata;
	sector_t zone = ZONE(bmd->bi_sector, pd);
	int used = ((bmd->bi_sector - zone) << 9) + bmd->bi_size;
	int remaining = (pd->settings.size << 9) - used;
	int remaining2;

	/*
	 * A bio <= PAGE_SIZE must be allowed. If it crosses a packet
	 * boundary, pkt_make_request() will split the bio.
	 */
	remaining2 = PAGE_SIZE - bmd->bi_size;
	remaining = max(remaining, remaining2);

	BUG_ON(remaining < 0);
	return remaining;
}

static void pkt_init_queue(struct pktcdvd_device *pd)
{
	struct request_queue *q = pd->disk->queue;

	blk_queue_make_request(q, pkt_make_request);
	blk_queue_hardsect_size(q, CD_FRAMESIZE);
	blk_queue_max_sectors(q, PACKET_MAX_SECTORS);
	blk_queue_merge_bvec(q, pkt_merge_bvec);
	q->queuedata = pd;
}

static int pkt_seq_show(struct seq_file *m, void *p)
{
	struct pktcdvd_device *pd = m->private;
	char *msg;
	char bdev_buf[BDEVNAME_SIZE];
	int states[PACKET_NUM_STATES];

	seq_printf(m, "Writer %s mapped to %s:\n", pd->name,
		   bdevname(pd->bdev, bdev_buf));

	seq_printf(m, "\nSettings:\n");
	seq_printf(m, "\tpacket size:\t\t%dkB\n", pd->settings.size / 2);

	if (pd->settings.write_type == 0)
		msg = "Packet";
	else
		msg = "Unknown";
	seq_printf(m, "\twrite type:\t\t%s\n", msg);

	seq_printf(m, "\tpacket type:\t\t%s\n", pd->settings.fp ? "Fixed" : "Variable");
	seq_printf(m, "\tlink loss:\t\t%d\n", pd->settings.link_loss);

	seq_printf(m, "\ttrack mode:\t\t%d\n", pd->settings.track_mode);

	if (pd->settings.block_mode == PACKET_BLOCK_MODE1)
		msg = "Mode 1";
	else if (pd->settings.block_mode == PACKET_BLOCK_MODE2)
		msg = "Mode 2";
	else
		msg = "Unknown";
	seq_printf(m, "\tblock mode:\t\t%s\n", msg);

	seq_printf(m, "\nStatistics:\n");
	seq_printf(m, "\tpackets started:\t%lu\n", pd->stats.pkt_started);
	seq_printf(m, "\tpackets ended:\t\t%lu\n", pd->stats.pkt_ended);
	seq_printf(m, "\twritten:\t\t%lukB\n", pd->stats.secs_w >> 1);
	seq_printf(m, "\tread gather:\t\t%lukB\n", pd->stats.secs_rg >> 1);
	seq_printf(m, "\tread:\t\t\t%lukB\n", pd->stats.secs_r >> 1);

	seq_printf(m, "\nMisc:\n");
	seq_printf(m, "\treference count:\t%d\n", pd->refcnt);
	seq_printf(m, "\tflags:\t\t\t0x%lx\n", pd->flags);
	seq_printf(m, "\tread speed:\t\t%ukB/s\n", pd->read_speed);
	seq_printf(m, "\twrite speed:\t\t%ukB/s\n", pd->write_speed);
	seq_printf(m, "\tstart offset:\t\t%lu\n", pd->offset);
	seq_printf(m, "\tmode page offset:\t%u\n", pd->mode_offset);

	seq_printf(m, "\nQueue state:\n");
	seq_printf(m, "\tbios queued:\t\t%d\n", pd->bio_queue_size);
	seq_printf(m, "\tbios pending:\t\t%d\n", atomic_read(&pd->cdrw.pending_bios));
	seq_printf(m, "\tcurrent sector:\t\t0x%llx\n", (unsigned long long)pd->current_sector);

	pkt_count_states(pd, states);
	seq_printf(m, "\tstate:\t\t\ti:%d ow:%d rw:%d ww:%d rec:%d fin:%d\n",
		   states[0], states[1], states[2], states[3], states[4], states[5]);

	seq_printf(m, "\twrite congestion marks:\toff=%d on=%d\n",
			pd->write_congestion_off,
			pd->write_congestion_on);
	return 0;
}

static int pkt_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, pkt_seq_show, PDE(inode)->data);
}

static const struct file_operations pkt_proc_fops = {
	.open	= pkt_seq_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release = single_release
};

static int pkt_new_dev(struct pktcdvd_device *pd, dev_t dev)
{
	int i;
	int ret = 0;
	char b[BDEVNAME_SIZE];
	struct block_device *bdev;

	if (pd->pkt_dev == dev) {
		printk(DRIVER_NAME": Recursive setup not allowed\n");
		return -EBUSY;
	}
	for (i = 0; i < MAX_WRITERS; i++) {
		struct pktcdvd_device *pd2 = pkt_devs[i];
		if (!pd2)
			continue;
		if (pd2->bdev->bd_dev == dev) {
			printk(DRIVER_NAME": %s already setup\n", bdevname(pd2->bdev, b));
			return -EBUSY;
		}
		if (pd2->pkt_dev == dev) {
			printk(DRIVER_NAME": Can't chain pktcdvd devices\n");
			return -EBUSY;
		}
	}

	bdev = bdget(dev);
	if (!bdev)
		return -ENOMEM;
	ret = blkdev_get(bdev, FMODE_READ, O_RDONLY | O_NONBLOCK);
	if (ret)
		return ret;

	/* This is safe, since we have a reference from open(). */
	__module_get(THIS_MODULE);

	pd->bdev = bdev;
	set_blocksize(bdev, CD_FRAMESIZE);

	pkt_init_queue(pd);

	atomic_set(&pd->cdrw.pending_bios, 0);
	pd->cdrw.thread = kthread_run(kcdrwd, pd, "%s", pd->name);
	if (IS_ERR(pd->cdrw.thread)) {
		printk(DRIVER_NAME": can't start kernel thread\n");
		ret = -ENOMEM;
		goto out_mem;
	}

	proc_create_data(pd->name, 0, pkt_proc, &pkt_proc_fops, pd);
	DPRINTK(DRIVER_NAME": writer %s mapped to %s\n", pd->name, bdevname(bdev, b));
	return 0;

out_mem:
	blkdev_put(bdev);
	/* This is safe: open() is still holding a reference. */
	module_put(THIS_MODULE);
	return ret;
}

static int pkt_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pktcdvd_device *pd = inode->i_bdev->bd_disk->private_data;

	VPRINTK("pkt_ioctl: cmd %x, dev %d:%d\n", cmd, imajor(inode), iminor(inode));

	switch (cmd) {
	case CDROMEJECT:
		/*
		 * The door gets locked when the device is opened, so we
		 * have to unlock it or else the eject command fails.
		 */
		if (pd->refcnt == 1)
			pkt_lock_door(pd, 0);
		/* fallthru */
	/*
	 * forward selected CDROM ioctls to CD-ROM, for UDF
	 */
	case CDROMMULTISESSION:
	case CDROMREADTOCENTRY:
	case CDROM_LAST_WRITTEN:
	case CDROM_SEND_PACKET:
	case SCSI_IOCTL_SEND_COMMAND:
		return __blkdev_driver_ioctl(pd->bdev, file ? file->f_mode : 0,
					cmd, arg);

	default:
		VPRINTK(DRIVER_NAME": Unknown ioctl for %s (%x)\n", pd->name, cmd);
		return -ENOTTY;
	}

	return 0;
}

static int pkt_media_changed(struct gendisk *disk)
{
	struct pktcdvd_device *pd = disk->private_data;
	struct gendisk *attached_disk;

	if (!pd)
		return 0;
	if (!pd->bdev)
		return 0;
	attached_disk = pd->bdev->bd_disk;
	if (!attached_disk)
		return 0;
	return attached_disk->fops->media_changed(attached_disk);
}

static struct block_device_operations pktcdvd_ops = {
	.owner =		THIS_MODULE,
	.open =			pkt_open,
	.release =		pkt_close,
	.ioctl =		pkt_ioctl,
	.media_changed =	pkt_media_changed,
};

/*
 * Set up mapping from pktcdvd device to CD-ROM device.
 */
static int pkt_setup_dev(dev_t dev, dev_t* pkt_dev)
{
	int idx;
	int ret = -ENOMEM;
	struct pktcdvd_device *pd;
	struct gendisk *disk;

	mutex_lock_nested(&ctl_mutex, SINGLE_DEPTH_NESTING);

	for (idx = 0; idx < MAX_WRITERS; idx++)
		if (!pkt_devs[idx])
			break;
	if (idx == MAX_WRITERS) {
		printk(DRIVER_NAME": max %d writers supported\n", MAX_WRITERS);
		ret = -EBUSY;
		goto out_mutex;
	}

	pd = kzalloc(sizeof(struct pktcdvd_device), GFP_KERNEL);
	if (!pd)
		goto out_mutex;

	pd->rb_pool = mempool_create_kmalloc_pool(PKT_RB_POOL_SIZE,
						  sizeof(struct pkt_rb_node));
	if (!pd->rb_pool)
		goto out_mem;

	INIT_LIST_HEAD(&pd->cdrw.pkt_free_list);
	INIT_LIST_HEAD(&pd->cdrw.pkt_active_list);
	spin_lock_init(&pd->cdrw.active_list_lock);

	spin_lock_init(&pd->lock);
	spin_lock_init(&pd->iosched.lock);
	sprintf(pd->name, DRIVER_NAME"%d", idx);
	init_waitqueue_head(&pd->wqueue);
	pd->bio_queue = RB_ROOT;

	pd->write_congestion_on  = write_congestion_on;
	pd->write_congestion_off = write_congestion_off;

	disk = alloc_disk(1);
	if (!disk)
		goto out_mem;
	pd->disk = disk;
	disk->major = pktdev_major;
	disk->first_minor = idx;
	disk->fops = &pktcdvd_ops;
	disk->flags = GENHD_FL_REMOVABLE;
	strcpy(disk->disk_name, pd->name);
	disk->private_data = pd;
	disk->queue = blk_alloc_queue(GFP_KERNEL);
	if (!disk->queue)
		goto out_mem2;

	pd->pkt_dev = MKDEV(pktdev_major, idx);
	ret = pkt_new_dev(pd, dev);
	if (ret)
		goto out_new_dev;

	add_disk(disk);

	pkt_sysfs_dev_new(pd);
	pkt_debugfs_dev_new(pd);

	pkt_devs[idx] = pd;
	if (pkt_dev)
		*pkt_dev = pd->pkt_dev;

	mutex_unlock(&ctl_mutex);
	return 0;

out_new_dev:
	blk_cleanup_queue(disk->queue);
out_mem2:
	put_disk(disk);
out_mem:
	if (pd->rb_pool)
		mempool_destroy(pd->rb_pool);
	kfree(pd);
out_mutex:
	mutex_unlock(&ctl_mutex);
	printk(DRIVER_NAME": setup of pktcdvd device failed\n");
	return ret;
}

/*
 * Tear down mapping from pktcdvd device to CD-ROM device.
 */
static int pkt_remove_dev(dev_t pkt_dev)
{
	struct pktcdvd_device *pd;
	int idx;
	int ret = 0;

	mutex_lock_nested(&ctl_mutex, SINGLE_DEPTH_NESTING);

	for (idx = 0; idx < MAX_WRITERS; idx++) {
		pd = pkt_devs[idx];
		if (pd && (pd->pkt_dev == pkt_dev))
			break;
	}
	if (idx == MAX_WRITERS) {
		DPRINTK(DRIVER_NAME": dev not setup\n");
		ret = -ENXIO;
		goto out;
	}

	if (pd->refcnt > 0) {
		ret = -EBUSY;
		goto out;
	}
	if (!IS_ERR(pd->cdrw.thread))
		kthread_stop(pd->cdrw.thread);

	pkt_devs[idx] = NULL;

	pkt_debugfs_dev_remove(pd);
	pkt_sysfs_dev_remove(pd);

	blkdev_put(pd->bdev);

	remove_proc_entry(pd->name, pkt_proc);
	DPRINTK(DRIVER_NAME": writer %s unmapped\n", pd->name);

	del_gendisk(pd->disk);
	blk_cleanup_queue(pd->disk->queue);
	put_disk(pd->disk);

	mempool_destroy(pd->rb_pool);
	kfree(pd);

	/* This is safe: open() is still holding a reference. */
	module_put(THIS_MODULE);

out:
	mutex_unlock(&ctl_mutex);
	return ret;
}

static void pkt_get_status(struct pkt_ctrl_command *ctrl_cmd)
{
	struct pktcdvd_device *pd;

	mutex_lock_nested(&ctl_mutex, SINGLE_DEPTH_NESTING);

	pd = pkt_find_dev_from_minor(ctrl_cmd->dev_index);
	if (pd) {
		ctrl_cmd->dev = new_encode_dev(pd->bdev->bd_dev);
		ctrl_cmd->pkt_dev = new_encode_dev(pd->pkt_dev);
	} else {
		ctrl_cmd->dev = 0;
		ctrl_cmd->pkt_dev = 0;
	}
	ctrl_cmd->num_devices = MAX_WRITERS;

	mutex_unlock(&ctl_mutex);
}

static int pkt_ctl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct pkt_ctrl_command ctrl_cmd;
	int ret = 0;
	dev_t pkt_dev = 0;

	if (cmd != PACKET_CTRL_CMD)
		return -ENOTTY;

	if (copy_from_user(&ctrl_cmd, argp, sizeof(struct pkt_ctrl_command)))
		return -EFAULT;

	switch (ctrl_cmd.command) {
	case PKT_CTRL_CMD_SETUP:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		ret = pkt_setup_dev(new_decode_dev(ctrl_cmd.dev), &pkt_dev);
		ctrl_cmd.pkt_dev = new_encode_dev(pkt_dev);
		break;
	case PKT_CTRL_CMD_TEARDOWN:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		ret = pkt_remove_dev(new_decode_dev(ctrl_cmd.pkt_dev));
		break;
	case PKT_CTRL_CMD_STATUS:
		pkt_get_status(&ctrl_cmd);
		break;
	default:
		return -ENOTTY;
	}

	if (copy_to_user(argp, &ctrl_cmd, sizeof(struct pkt_ctrl_command)))
		return -EFAULT;
	return ret;
}


static const struct file_operations pkt_ctl_fops = {
	.ioctl	 = pkt_ctl_ioctl,
	.owner	 = THIS_MODULE,
};

static struct miscdevice pkt_misc = {
	.minor 		= MISC_DYNAMIC_MINOR,
	.name  		= DRIVER_NAME,
	.fops  		= &pkt_ctl_fops
};

static int __init pkt_init(void)
{
	int ret;

	mutex_init(&ctl_mutex);

	psd_pool = mempool_create_kmalloc_pool(PSD_POOL_SIZE,
					sizeof(struct packet_stacked_data));
	if (!psd_pool)
		return -ENOMEM;

	ret = register_blkdev(pktdev_major, DRIVER_NAME);
	if (ret < 0) {
		printk(DRIVER_NAME": Unable to register block device\n");
		goto out2;
	}
	if (!pktdev_major)
		pktdev_major = ret;

	ret = pkt_sysfs_init();
	if (ret)
		goto out;

	pkt_debugfs_init();

	ret = misc_register(&pkt_misc);
	if (ret) {
		printk(DRIVER_NAME": Unable to register misc device\n");
		goto out_misc;
	}

	pkt_proc = proc_mkdir("driver/"DRIVER_NAME, NULL);

	return 0;

out_misc:
	pkt_debugfs_cleanup();
	pkt_sysfs_cleanup();
out:
	unregister_blkdev(pktdev_major, DRIVER_NAME);
out2:
	mempool_destroy(psd_pool);
	return ret;
}

static void __exit pkt_exit(void)
{
	remove_proc_entry("driver/"DRIVER_NAME, NULL);
	misc_deregister(&pkt_misc);

	pkt_debugfs_cleanup();
	pkt_sysfs_cleanup();

	unregister_blkdev(pktdev_major, DRIVER_NAME);
	mempool_destroy(psd_pool);
}

MODULE_DESCRIPTION("Packet writing layer for CD/DVD drives");
MODULE_AUTHOR("Jens Axboe <axboe@suse.de>");
MODULE_LICENSE("GPL");

module_init(pkt_init);
module_exit(pkt_exit);
