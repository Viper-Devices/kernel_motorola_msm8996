/*
 * IDE ATAPI floppy driver.
 *
 * Copyright (C) 1996-1999  Gadi Oxman <gadio@netvision.net.il>
 * Copyright (C) 2000-2002  Paul Bristow <paul@paulbristow.net>
 * Copyright (C) 2005       Bartlomiej Zolnierkiewicz
 *
 * This driver supports the following IDE floppy drives:
 *
 * LS-120/240 SuperDisk
 * Iomega Zip 100/250
 * Iomega PC Card Clik!/PocketZip
 *
 * For a historical changelog see
 * Documentation/ide/ChangeLog.ide-floppy.1996-2002
 */

#define DRV_NAME "ide-floppy"
#define PFX DRV_NAME ": "

#define IDEFLOPPY_VERSION "1.00"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/cdrom.h>
#include <linux/ide.h>
#include <linux/hdreg.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>

#include <scsi/scsi_ioctl.h>

#include <asm/byteorder.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/unaligned.h>

#include "ide-floppy.h"

/* module parameters */
static unsigned long debug_mask;
module_param(debug_mask, ulong, 0644);

/* define to see debug info */
#define IDEFLOPPY_DEBUG_LOG	0

#if IDEFLOPPY_DEBUG_LOG
#define ide_debug_log(lvl, fmt, args...) __ide_debug_log(lvl, fmt, args)
#else
#define ide_debug_log(lvl, fmt, args...) do {} while (0)
#endif

/*
 * After each failed packet command we issue a request sense command and retry
 * the packet command IDEFLOPPY_MAX_PC_RETRIES times.
 */
#define IDEFLOPPY_MAX_PC_RETRIES	3

/* format capacities descriptor codes */
#define CAPACITY_INVALID	0x00
#define CAPACITY_UNFORMATTED	0x01
#define CAPACITY_CURRENT	0x02
#define CAPACITY_NO_CARTRIDGE	0x03

/*
 * The following delay solves a problem with ATAPI Zip 100 drive where BSY bit
 * was apparently being deasserted before the unit was ready to receive data.
 */
#define IDEFLOPPY_PC_DELAY	(HZ/20)	/* default delay for ZIP 100 (50ms) */

/* Error code returned in rq->errors to the higher part of the driver. */
#define	IDEFLOPPY_ERROR_GENERAL		101

static DEFINE_MUTEX(idefloppy_ref_mutex);

static void idefloppy_cleanup_obj(struct kref *);

static struct ide_floppy_obj *ide_floppy_get(struct gendisk *disk)
{
	struct ide_floppy_obj *floppy = NULL;

	mutex_lock(&idefloppy_ref_mutex);
	floppy = ide_drv_g(disk, ide_floppy_obj);
	if (floppy) {
		if (ide_device_get(floppy->drive))
			floppy = NULL;
		else
			kref_get(&floppy->kref);
	}
	mutex_unlock(&idefloppy_ref_mutex);
	return floppy;
}

static void ide_floppy_put(struct ide_floppy_obj *floppy)
{
	ide_drive_t *drive = floppy->drive;

	mutex_lock(&idefloppy_ref_mutex);
	kref_put(&floppy->kref, idefloppy_cleanup_obj);
	ide_device_put(drive);
	mutex_unlock(&idefloppy_ref_mutex);
}

/*
 * Used to finish servicing a request. For read/write requests, we will call
 * ide_end_request to pass to the next buffer.
 */
static int idefloppy_end_request(ide_drive_t *drive, int uptodate, int nsecs)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct request *rq = HWGROUP(drive)->rq;
	int error;

	ide_debug_log(IDE_DBG_FUNC, "Call %s\n", __func__);

	switch (uptodate) {
	case 0:
		error = IDEFLOPPY_ERROR_GENERAL;
		break;

	case 1:
		error = 0;
		break;

	default:
		error = uptodate;
	}

	if (error)
		floppy->failed_pc = NULL;
	/* Why does this happen? */
	if (!rq)
		return 0;
	if (!blk_special_request(rq)) {
		/* our real local end request function */
		ide_end_request(drive, uptodate, nsecs);
		return 0;
	}
	rq->errors = error;
	/* fixme: need to move this local also */
	ide_end_drive_cmd(drive, 0, 0);
	return 0;
}

static void idefloppy_update_buffers(ide_drive_t *drive,
				struct ide_atapi_pc *pc)
{
	struct request *rq = pc->rq;
	struct bio *bio = rq->bio;

	while ((bio = rq->bio) != NULL)
		idefloppy_end_request(drive, 1, 0);
}

static void ide_floppy_callback(ide_drive_t *drive, int dsc)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct ide_atapi_pc *pc = drive->pc;
	int uptodate = pc->error ? 0 : 1;

	ide_debug_log(IDE_DBG_FUNC, "Call %s\n", __func__);

	if (floppy->failed_pc == pc)
		floppy->failed_pc = NULL;

	if (pc->c[0] == GPCMD_READ_10 || pc->c[0] == GPCMD_WRITE_10 ||
	    (pc->rq && blk_pc_request(pc->rq)))
		uptodate = 1; /* FIXME */
	else if (pc->c[0] == GPCMD_REQUEST_SENSE) {
		u8 *buf = pc->buf;

		if (!pc->error) {
			floppy->sense_key = buf[2] & 0x0F;
			floppy->asc = buf[12];
			floppy->ascq = buf[13];
			floppy->progress_indication = buf[15] & 0x80 ?
				(u16)get_unaligned((u16 *)&buf[16]) : 0x10000;

			if (floppy->failed_pc)
				ide_debug_log(IDE_DBG_PC, "pc = %x, ",
					      floppy->failed_pc->c[0]);

			ide_debug_log(IDE_DBG_SENSE, "sense key = %x, asc = %x,"
				      "ascq = %x\n", floppy->sense_key,
				      floppy->asc, floppy->ascq);
		} else
			printk(KERN_ERR PFX "Error in REQUEST SENSE itself - "
			       "Aborting request!\n");
	}

	idefloppy_end_request(drive, uptodate, 0);
}

static void ide_floppy_report_error(idefloppy_floppy_t *floppy,
				    struct ide_atapi_pc *pc)
{
	/* supress error messages resulting from Medium not present */
	if (floppy->sense_key == 0x02 &&
	    floppy->asc       == 0x3a &&
	    floppy->ascq      == 0x00)
		return;

	printk(KERN_ERR PFX "%s: I/O error, pc = %2x, key = %2x, "
			"asc = %2x, ascq = %2x\n",
			floppy->drive->name, pc->c[0], floppy->sense_key,
			floppy->asc, floppy->ascq);

}

static ide_startstop_t idefloppy_issue_pc(ide_drive_t *drive,
		struct ide_atapi_pc *pc)
{
	idefloppy_floppy_t *floppy = drive->driver_data;

	if (floppy->failed_pc == NULL &&
	    pc->c[0] != GPCMD_REQUEST_SENSE)
		floppy->failed_pc = pc;

	/* Set the current packet command */
	drive->pc = pc;

	if (pc->retries > IDEFLOPPY_MAX_PC_RETRIES) {
		if (!(pc->flags & PC_FLAG_SUPPRESS_ERROR))
			ide_floppy_report_error(floppy, pc);
		/* Giving up */
		pc->error = IDEFLOPPY_ERROR_GENERAL;

		floppy->failed_pc = NULL;
		drive->pc_callback(drive, 0);
		return ide_stopped;
	}

	ide_debug_log(IDE_DBG_FUNC, "%s: Retry #%d\n", __func__, pc->retries);

	pc->retries++;

	return ide_issue_pc(drive, WAIT_FLOPPY_CMD, NULL);
}

void ide_floppy_create_read_capacity_cmd(struct ide_atapi_pc *pc)
{
	ide_init_pc(pc);
	pc->c[0] = GPCMD_READ_FORMAT_CAPACITIES;
	pc->c[7] = 255;
	pc->c[8] = 255;
	pc->req_xfer = 255;
}

/* A mode sense command is used to "sense" floppy parameters. */
void ide_floppy_create_mode_sense_cmd(struct ide_atapi_pc *pc, u8 page_code)
{
	u16 length = 8; /* sizeof(Mode Parameter Header) = 8 Bytes */

	ide_init_pc(pc);
	pc->c[0] = GPCMD_MODE_SENSE_10;
	pc->c[1] = 0;
	pc->c[2] = page_code;

	switch (page_code) {
	case IDEFLOPPY_CAPABILITIES_PAGE:
		length += 12;
		break;
	case IDEFLOPPY_FLEXIBLE_DISK_PAGE:
		length += 32;
		break;
	default:
		printk(KERN_ERR PFX "unsupported page code in %s\n", __func__);
	}
	put_unaligned(cpu_to_be16(length), (u16 *) &pc->c[7]);
	pc->req_xfer = length;
}

static void idefloppy_create_rw_cmd(ide_drive_t *drive,
				    struct ide_atapi_pc *pc, struct request *rq,
				    unsigned long sector)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	int block = sector / floppy->bs_factor;
	int blocks = rq->nr_sectors / floppy->bs_factor;
	int cmd = rq_data_dir(rq);

	ide_debug_log(IDE_DBG_FUNC, "%s: block: %d, blocks: %d\n", __func__,
		      block, blocks);

	ide_init_pc(pc);
	pc->c[0] = cmd == READ ? GPCMD_READ_10 : GPCMD_WRITE_10;
	put_unaligned(cpu_to_be16(blocks), (unsigned short *)&pc->c[7]);
	put_unaligned(cpu_to_be32(block), (unsigned int *) &pc->c[2]);

	memcpy(rq->cmd, pc->c, 12);

	pc->rq = rq;
	pc->b_count = 0;
	if (rq->cmd_flags & REQ_RW)
		pc->flags |= PC_FLAG_WRITING;
	pc->buf = NULL;
	pc->req_xfer = pc->buf_size = blocks * floppy->block_size;
	pc->flags |= PC_FLAG_DMA_OK;
}

static void idefloppy_blockpc_cmd(idefloppy_floppy_t *floppy,
		struct ide_atapi_pc *pc, struct request *rq)
{
	ide_init_pc(pc);
	memcpy(pc->c, rq->cmd, sizeof(pc->c));
	pc->rq = rq;
	pc->b_count = 0;
	if (rq->data_len && rq_data_dir(rq) == WRITE)
		pc->flags |= PC_FLAG_WRITING;
	pc->buf = rq->data;
	if (rq->bio)
		pc->flags |= PC_FLAG_DMA_OK;
	/*
	 * possibly problematic, doesn't look like ide-floppy correctly
	 * handled scattered requests if dma fails...
	 */
	pc->req_xfer = pc->buf_size = rq->data_len;
}

static ide_startstop_t idefloppy_do_request(ide_drive_t *drive,
		struct request *rq, sector_t block_s)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	ide_hwif_t *hwif = drive->hwif;
	struct ide_atapi_pc *pc;
	unsigned long block = (unsigned long)block_s;

	ide_debug_log(IDE_DBG_FUNC, "%s: dev: %s, cmd: 0x%x, cmd_type: %x, "
		      "errors: %d\n",
		      __func__, rq->rq_disk ? rq->rq_disk->disk_name : "?",
		      rq->cmd[0], rq->cmd_type, rq->errors);

	ide_debug_log(IDE_DBG_FUNC, "%s: sector: %ld, nr_sectors: %ld, "
		      "current_nr_sectors: %d\n",
		      __func__, (long)rq->sector, rq->nr_sectors,
		      rq->current_nr_sectors);

	if (rq->errors >= ERROR_MAX) {
		if (floppy->failed_pc)
			ide_floppy_report_error(floppy, floppy->failed_pc);
		else
			printk(KERN_ERR PFX "%s: I/O error\n", drive->name);

		idefloppy_end_request(drive, 0, 0);
		return ide_stopped;
	}
	if (blk_fs_request(rq)) {
		if (((long)rq->sector % floppy->bs_factor) ||
		    (rq->nr_sectors % floppy->bs_factor)) {
			printk(KERN_ERR PFX "%s: unsupported r/w rq size\n",
				drive->name);
			idefloppy_end_request(drive, 0, 0);
			return ide_stopped;
		}
		pc = &floppy->queued_pc;
		idefloppy_create_rw_cmd(drive, pc, rq, block);
	} else if (blk_special_request(rq)) {
		pc = (struct ide_atapi_pc *) rq->buffer;
	} else if (blk_pc_request(rq)) {
		pc = &floppy->queued_pc;
		idefloppy_blockpc_cmd(floppy, pc, rq);
	} else {
		blk_dump_rq_flags(rq, PFX "unsupported command in queue");
		idefloppy_end_request(drive, 0, 0);
		return ide_stopped;
	}

	ide_init_sg_cmd(drive, rq);
	ide_map_sg(drive, rq);

	pc->sg = hwif->sg_table;
	pc->sg_cnt = hwif->sg_nents;

	pc->rq = rq;

	return idefloppy_issue_pc(drive, pc);
}

/*
 * Look at the flexible disk page parameters. We ignore the CHS capacity
 * parameters and use the LBA parameters instead.
 */
static int ide_floppy_get_flexible_disk_page(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct gendisk *disk = floppy->disk;
	struct ide_atapi_pc pc;
	u8 *page;
	int capacity, lba_capacity;
	u16 transfer_rate, sector_size, cyls, rpm;
	u8 heads, sectors;

	ide_floppy_create_mode_sense_cmd(&pc, IDEFLOPPY_FLEXIBLE_DISK_PAGE);

	if (ide_queue_pc_tail(drive, disk, &pc)) {
		printk(KERN_ERR PFX "Can't get flexible disk page params\n");
		return 1;
	}

	if (pc.buf[3] & 0x80)
		drive->dev_flags |= IDE_DFLAG_WP;
	else
		drive->dev_flags &= ~IDE_DFLAG_WP;

	set_disk_ro(disk, !!(drive->dev_flags & IDE_DFLAG_WP));

	page = &pc.buf[8];

	transfer_rate = be16_to_cpup((__be16 *)&pc.buf[8 + 2]);
	sector_size   = be16_to_cpup((__be16 *)&pc.buf[8 + 6]);
	cyls          = be16_to_cpup((__be16 *)&pc.buf[8 + 8]);
	rpm           = be16_to_cpup((__be16 *)&pc.buf[8 + 28]);
	heads         = pc.buf[8 + 4];
	sectors       = pc.buf[8 + 5];

	capacity = cyls * heads * sectors * sector_size;

	if (memcmp(page, &floppy->flexible_disk_page, 32))
		printk(KERN_INFO PFX "%s: %dkB, %d/%d/%d CHS, %d kBps, "
				"%d sector size, %d rpm\n",
				drive->name, capacity / 1024, cyls, heads,
				sectors, transfer_rate / 8, sector_size, rpm);

	memcpy(&floppy->flexible_disk_page, page, 32);
	drive->bios_cyl = cyls;
	drive->bios_head = heads;
	drive->bios_sect = sectors;
	lba_capacity = floppy->blocks * floppy->block_size;

	if (capacity < lba_capacity) {
		printk(KERN_NOTICE PFX "%s: The disk reports a capacity of %d "
			"bytes, but the drive only handles %d\n",
			drive->name, lba_capacity, capacity);
		floppy->blocks = floppy->block_size ?
			capacity / floppy->block_size : 0;
		drive->capacity64 = floppy->blocks * floppy->bs_factor;
	}

	return 0;
}

/*
 * Determine if a media is present in the floppy drive, and if so, its LBA
 * capacity.
 */
static int ide_floppy_get_capacity(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct gendisk *disk = floppy->disk;
	struct ide_atapi_pc pc;
	u8 *cap_desc;
	u8 header_len, desc_cnt;
	int i, rc = 1, blocks, length;

	drive->bios_cyl = 0;
	drive->bios_head = drive->bios_sect = 0;
	floppy->blocks = 0;
	floppy->bs_factor = 1;
	drive->capacity64 = 0;

	ide_floppy_create_read_capacity_cmd(&pc);
	if (ide_queue_pc_tail(drive, disk, &pc)) {
		printk(KERN_ERR PFX "Can't get floppy parameters\n");
		return 1;
	}
	header_len = pc.buf[3];
	cap_desc = &pc.buf[4];
	desc_cnt = header_len / 8; /* capacity descriptor of 8 bytes */

	for (i = 0; i < desc_cnt; i++) {
		unsigned int desc_start = 4 + i*8;

		blocks = be32_to_cpup((__be32 *)&pc.buf[desc_start]);
		length = be16_to_cpup((__be16 *)&pc.buf[desc_start + 6]);

		ide_debug_log(IDE_DBG_PROBE, "Descriptor %d: %dkB, %d blocks, "
			      "%d sector size\n",
			      i, blocks * length / 1024, blocks, length);

		if (i)
			continue;
		/*
		 * the code below is valid only for the 1st descriptor, ie i=0
		 */

		switch (pc.buf[desc_start + 4] & 0x03) {
		/* Clik! drive returns this instead of CAPACITY_CURRENT */
		case CAPACITY_UNFORMATTED:
			if (!(drive->atapi_flags & IDE_AFLAG_CLIK_DRIVE))
				/*
				 * If it is not a clik drive, break out
				 * (maintains previous driver behaviour)
				 */
				break;
		case CAPACITY_CURRENT:
			/* Normal Zip/LS-120 disks */
			if (memcmp(cap_desc, &floppy->cap_desc, 8))
				printk(KERN_INFO PFX "%s: %dkB, %d blocks, %d "
				       "sector size\n",
				       drive->name, blocks * length / 1024,
				       blocks, length);
			memcpy(&floppy->cap_desc, cap_desc, 8);

			if (!length || length % 512) {
				printk(KERN_NOTICE PFX "%s: %d bytes block size"
				       " not supported\n", drive->name, length);
			} else {
				floppy->blocks = blocks;
				floppy->block_size = length;
				floppy->bs_factor = length / 512;
				if (floppy->bs_factor != 1)
					printk(KERN_NOTICE PFX "%s: Warning: "
					       "non 512 bytes block size not "
					       "fully supported\n",
					       drive->name);
				drive->capacity64 =
					floppy->blocks * floppy->bs_factor;
				rc = 0;
			}
			break;
		case CAPACITY_NO_CARTRIDGE:
			/*
			 * This is a KERN_ERR so it appears on screen
			 * for the user to see
			 */
			printk(KERN_ERR PFX "%s: No disk in drive\n",
			       drive->name);
			break;
		case CAPACITY_INVALID:
			printk(KERN_ERR PFX "%s: Invalid capacity for disk "
				"in drive\n", drive->name);
			break;
		}
		ide_debug_log(IDE_DBG_PROBE, "Descriptor 0 Code: %d\n",
			      pc.buf[desc_start + 4] & 0x03);
	}

	/* Clik! disk does not support get_flexible_disk_page */
	if (!(drive->atapi_flags & IDE_AFLAG_CLIK_DRIVE))
		(void) ide_floppy_get_flexible_disk_page(drive);

	return rc;
}

sector_t ide_floppy_capacity(ide_drive_t *drive)
{
	return drive->capacity64;
}

static void idefloppy_setup(ide_drive_t *drive)
{
	struct ide_floppy_obj *floppy = drive->driver_data;
	u16 *id = drive->id;

	drive->pc_callback	 = ide_floppy_callback;
	drive->pc_update_buffers = idefloppy_update_buffers;
	drive->pc_io_buffers	 = ide_io_buffers;

	/*
	 * We used to check revisions here. At this point however I'm giving up.
	 * Just assume they are all broken, its easier.
	 *
	 * The actual reason for the workarounds was likely a driver bug after
	 * all rather than a firmware bug, and the workaround below used to hide
	 * it. It should be fixed as of version 1.9, but to be on the safe side
	 * we'll leave the limitation below for the 2.2.x tree.
	 */
	if (!strncmp((char *)&id[ATA_ID_PROD], "IOMEGA ZIP 100 ATAPI", 20)) {
		drive->atapi_flags |= IDE_AFLAG_ZIP_DRIVE;
		/* This value will be visible in the /proc/ide/hdx/settings */
		drive->pc_delay = IDEFLOPPY_PC_DELAY;
		blk_queue_max_sectors(drive->queue, 64);
	}

	/*
	 * Guess what? The IOMEGA Clik! drive also needs the above fix. It makes
	 * nasty clicking noises without it, so please don't remove this.
	 */
	if (strncmp((char *)&id[ATA_ID_PROD], "IOMEGA Clik!", 11) == 0) {
		blk_queue_max_sectors(drive->queue, 64);
		drive->atapi_flags |= IDE_AFLAG_CLIK_DRIVE;
		/* IOMEGA Clik! drives do not support lock/unlock commands */
		drive->atapi_flags |= IDE_AFLAG_NO_DOORLOCK;
	}

	(void) ide_floppy_get_capacity(drive);

	ide_proc_register_driver(drive, floppy->driver);

	drive->dev_flags |= IDE_DFLAG_ATTACH;
}

static void ide_floppy_remove(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct gendisk *g = floppy->disk;

	ide_proc_unregister_driver(drive, floppy->driver);

	del_gendisk(g);

	ide_floppy_put(floppy);
}

static void idefloppy_cleanup_obj(struct kref *kref)
{
	struct ide_floppy_obj *floppy = to_ide_drv(kref, ide_floppy_obj);
	ide_drive_t *drive = floppy->drive;
	struct gendisk *g = floppy->disk;

	drive->driver_data = NULL;
	g->private_data = NULL;
	put_disk(g);
	kfree(floppy);
}

static int ide_floppy_probe(ide_drive_t *);

static ide_driver_t idefloppy_driver = {
	.gen_driver = {
		.owner		= THIS_MODULE,
		.name		= "ide-floppy",
		.bus		= &ide_bus_type,
	},
	.probe			= ide_floppy_probe,
	.remove			= ide_floppy_remove,
	.version		= IDEFLOPPY_VERSION,
	.do_request		= idefloppy_do_request,
	.end_request		= idefloppy_end_request,
	.error			= __ide_error,
#ifdef CONFIG_IDE_PROC_FS
	.proc			= ide_floppy_proc,
	.settings		= ide_floppy_settings,
#endif
};

static int idefloppy_open(struct inode *inode, struct file *filp)
{
	struct gendisk *disk = inode->i_bdev->bd_disk;
	struct ide_floppy_obj *floppy;
	ide_drive_t *drive;
	int ret = 0;

	floppy = ide_floppy_get(disk);
	if (!floppy)
		return -ENXIO;

	drive = floppy->drive;

	ide_debug_log(IDE_DBG_FUNC, "Call %s\n", __func__);

	floppy->openers++;

	if (floppy->openers == 1) {
		drive->atapi_flags &= ~IDE_AFLAG_FORMAT_IN_PROGRESS;
		/* Just in case */

		if (ide_do_test_unit_ready(drive, disk))
			ide_do_start_stop(drive, disk, 1);

		ret = ide_floppy_get_capacity(drive);

		set_capacity(disk, ide_floppy_capacity(drive));

		if (ret && (filp->f_flags & O_NDELAY) == 0) {
		    /*
		     * Allow O_NDELAY to open a drive without a disk, or with an
		     * unreadable disk, so that we can get the format capacity
		     * of the drive or begin the format - Sam
		     */
			ret = -EIO;
			goto out_put_floppy;
		}

		if ((drive->dev_flags & IDE_DFLAG_WP) && (filp->f_mode & 2)) {
			ret = -EROFS;
			goto out_put_floppy;
		}

		ide_set_media_lock(drive, disk, 1);
		drive->dev_flags |= IDE_DFLAG_MEDIA_CHANGED;
		check_disk_change(inode->i_bdev);
	} else if (drive->atapi_flags & IDE_AFLAG_FORMAT_IN_PROGRESS) {
		ret = -EBUSY;
		goto out_put_floppy;
	}
	return 0;

out_put_floppy:
	floppy->openers--;
	ide_floppy_put(floppy);
	return ret;
}

static int idefloppy_release(struct inode *inode, struct file *filp)
{
	struct gendisk *disk = inode->i_bdev->bd_disk;
	struct ide_floppy_obj *floppy = ide_drv_g(disk, ide_floppy_obj);
	ide_drive_t *drive = floppy->drive;

	ide_debug_log(IDE_DBG_FUNC, "Call %s\n", __func__);

	if (floppy->openers == 1) {
		ide_set_media_lock(drive, disk, 0);
		drive->atapi_flags &= ~IDE_AFLAG_FORMAT_IN_PROGRESS;
	}

	floppy->openers--;

	ide_floppy_put(floppy);

	return 0;
}

static int idefloppy_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct ide_floppy_obj *floppy = ide_drv_g(bdev->bd_disk,
						     ide_floppy_obj);
	ide_drive_t *drive = floppy->drive;

	geo->heads = drive->bios_head;
	geo->sectors = drive->bios_sect;
	geo->cylinders = (u16)drive->bios_cyl; /* truncate */
	return 0;
}

static int idefloppy_media_changed(struct gendisk *disk)
{
	struct ide_floppy_obj *floppy = ide_drv_g(disk, ide_floppy_obj);
	ide_drive_t *drive = floppy->drive;
	int ret;

	/* do not scan partitions twice if this is a removable device */
	if (drive->dev_flags & IDE_DFLAG_ATTACH) {
		drive->dev_flags &= ~IDE_DFLAG_ATTACH;
		return 0;
	}
	ret = !!(drive->dev_flags & IDE_DFLAG_MEDIA_CHANGED);
	drive->dev_flags &= ~IDE_DFLAG_MEDIA_CHANGED;
	return ret;
}

static int idefloppy_revalidate_disk(struct gendisk *disk)
{
	struct ide_floppy_obj *floppy = ide_drv_g(disk, ide_floppy_obj);
	set_capacity(disk, ide_floppy_capacity(floppy->drive));
	return 0;
}

static struct block_device_operations idefloppy_ops = {
	.owner			= THIS_MODULE,
	.open			= idefloppy_open,
	.release		= idefloppy_release,
	.ioctl			= ide_floppy_ioctl,
	.getgeo			= idefloppy_getgeo,
	.media_changed		= idefloppy_media_changed,
	.revalidate_disk	= idefloppy_revalidate_disk
};

static int ide_floppy_probe(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy;
	struct gendisk *g;

	if (!strstr("ide-floppy", drive->driver_req))
		goto failed;

	if (drive->media != ide_floppy)
		goto failed;

	if (!ide_check_atapi_device(drive, DRV_NAME)) {
		printk(KERN_ERR PFX "%s: not supported by this version of "
		       DRV_NAME "\n", drive->name);
		goto failed;
	}
	floppy = kzalloc(sizeof(idefloppy_floppy_t), GFP_KERNEL);
	if (!floppy) {
		printk(KERN_ERR PFX "%s: Can't allocate a floppy structure\n",
		       drive->name);
		goto failed;
	}

	g = alloc_disk_node(1 << PARTN_BITS, hwif_to_node(drive->hwif));
	if (!g)
		goto out_free_floppy;

	ide_init_disk(g, drive);

	kref_init(&floppy->kref);

	floppy->drive = drive;
	floppy->driver = &idefloppy_driver;
	floppy->disk = g;

	g->private_data = &floppy->driver;

	drive->driver_data = floppy;

	drive->debug_mask = debug_mask;

	idefloppy_setup(drive);

	set_capacity(g, ide_floppy_capacity(drive));

	g->minors = 1 << PARTN_BITS;
	g->driverfs_dev = &drive->gendev;
	if (drive->dev_flags & IDE_DFLAG_REMOVABLE)
		g->flags = GENHD_FL_REMOVABLE;
	g->fops = &idefloppy_ops;
	add_disk(g);
	return 0;

out_free_floppy:
	kfree(floppy);
failed:
	return -ENODEV;
}

static void __exit idefloppy_exit(void)
{
	driver_unregister(&idefloppy_driver.gen_driver);
}

static int __init idefloppy_init(void)
{
	printk(KERN_INFO DRV_NAME " driver " IDEFLOPPY_VERSION "\n");
	return driver_register(&idefloppy_driver.gen_driver);
}

MODULE_ALIAS("ide:*m-floppy*");
MODULE_ALIAS("ide-floppy");
module_init(idefloppy_init);
module_exit(idefloppy_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ATAPI FLOPPY Driver");

