/*
 *  Driver for the Conexant CX25821 PCIe bridge
 *
 *  Copyright (C) 2009 Conexant Systems Inc.
 *  Authors  <shu.lin@conexant.com>, <hiep.huynh@conexant.com>
 *  Based on Steven Toth <stoth@linuxtv.org> cx23885 driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef CX25821_VIDEO_H_
#define CX25821_VIDEO_H_

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kmod.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/div64.h>

#include "cx25821.h"
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>

#define VIDEO_DEBUG 0

#define dprintk(level, fmt, arg...)					\
do {									\
	if (VIDEO_DEBUG >= level)					\
		printk(KERN_DEBUG "%s/0: " fmt, dev->name, ##arg);	\
} while (0)

/* For IOCTL to identify running upstream */
#define UPSTREAM_START_VIDEO        700
#define UPSTREAM_STOP_VIDEO         701
#define UPSTREAM_START_AUDIO        702
#define UPSTREAM_STOP_AUDIO         703
#define UPSTREAM_DUMP_REGISTERS     702
#define SET_VIDEO_STD               800
#define SET_PIXEL_FORMAT            1000
#define ENABLE_CIF_RESOLUTION       1001

#define REG_READ		    900
#define REG_WRITE		    901
#define MEDUSA_READ		    910
#define MEDUSA_WRITE		    911

#define FORMAT_FLAGS_PACKED       0x01
extern void cx25821_video_wakeup(struct cx25821_dev *dev,
				 struct cx25821_dmaqueue *q, u32 count);

extern int cx25821_res_get(struct cx25821_dev *dev, struct cx25821_fh *fh,
			   unsigned int bit);
extern int cx25821_res_check(struct cx25821_fh *fh, unsigned int bit);
extern int cx25821_res_locked(struct cx25821_fh *fh, unsigned int bit);
extern void cx25821_res_free(struct cx25821_dev *dev, struct cx25821_fh *fh,
			     unsigned int bits);
extern int cx25821_start_video_dma(struct cx25821_dev *dev,
				   struct cx25821_dmaqueue *q,
				   struct cx25821_buffer *buf,
				   const struct sram_channel *channel);

extern int cx25821_video_irq(struct cx25821_dev *dev, int chan_num, u32 status);
extern void cx25821_video_unregister(struct cx25821_dev *dev, int chan_num);
extern int cx25821_video_register(struct cx25821_dev *dev);

#endif
