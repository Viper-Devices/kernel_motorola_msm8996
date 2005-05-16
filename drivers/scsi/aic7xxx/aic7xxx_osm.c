/*
 * Adaptec AIC7xxx device driver for Linux.
 *
 * $Id: //depot/aic7xxx/linux/drivers/scsi/aic7xxx/aic7xxx_osm.c#235 $
 *
 * Copyright (c) 1994 John Aycock
 *   The University of Calgary Department of Computer Science.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Sources include the Adaptec 1740 driver (aha1740.c), the Ultrastor 24F
 * driver (ultrastor.c), various Linux kernel source, the Adaptec EISA
 * config file (!adp7771.cfg), the Adaptec AHA-2740A Series User's Guide,
 * the Linux Kernel Hacker's Guide, Writing a SCSI Device Driver for Linux,
 * the Adaptec 1542 driver (aha1542.c), the Adaptec EISA overlay file
 * (adp7770.ovl), the Adaptec AHA-2740 Series Technical Reference Manual,
 * the Adaptec AIC-7770 Data Book, the ANSI SCSI specification, the
 * ANSI SCSI-2 specification (draft 10c), ...
 *
 * --------------------------------------------------------------------------
 *
 *  Modifications by Daniel M. Eischen (deischen@iworks.InterWorks.org):
 *
 *  Substantially modified to include support for wide and twin bus
 *  adapters, DMAing of SCBs, tagged queueing, IRQ sharing, bug fixes,
 *  SCB paging, and other rework of the code.
 *
 * --------------------------------------------------------------------------
 * Copyright (c) 1994-2000 Justin T. Gibbs.
 * Copyright (c) 2000-2001 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 *---------------------------------------------------------------------------
 *
 *  Thanks also go to (in alphabetical order) the following:
 *
 *    Rory Bolt     - Sequencer bug fixes
 *    Jay Estabrook - Initial DEC Alpha support
 *    Doug Ledford  - Much needed abort/reset bug fixes
 *    Kai Makisara  - DMAing of SCBs
 *
 *  A Boot time option was also added for not resetting the scsi bus.
 *
 *    Form:  aic7xxx=extended
 *           aic7xxx=no_reset
 *           aic7xxx=verbose
 *
 *  Daniel M. Eischen, deischen@iworks.InterWorks.org, 1/23/97
 *
 *  Id: aic7xxx.c,v 4.1 1997/06/12 08:23:42 deang Exp
 */

/*
 * Further driver modifications made by Doug Ledford <dledford@redhat.com>
 *
 * Copyright (c) 1997-1999 Doug Ledford
 *
 * These changes are released under the same licensing terms as the FreeBSD
 * driver written by Justin Gibbs.  Please see his Copyright notice above
 * for the exact terms and conditions covering my changes as well as the
 * warranty statement.
 *
 * Modifications made to the aic7xxx.c,v 4.1 driver from Dan Eischen include
 * but are not limited to:
 *
 *  1: Import of the latest FreeBSD sequencer code for this driver
 *  2: Modification of kernel code to accommodate different sequencer semantics
 *  3: Extensive changes throughout kernel portion of driver to improve
 *     abort/reset processing and error hanndling
 *  4: Other work contributed by various people on the Internet
 *  5: Changes to printk information and verbosity selection code
 *  6: General reliability related changes, especially in IRQ management
 *  7: Modifications to the default probe/attach order for supported cards
 *  8: SMP friendliness has been improved
 *
 */

#include "aic7xxx_osm.h"
#include "aic7xxx_inline.h"
#include <scsi/scsicam.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_spi.h>

static struct scsi_transport_template *ahc_linux_transport_template = NULL;

/*
 * Include aiclib.c as part of our
 * "module dependencies are hard" work around.
 */
#include "aiclib.c"

#include <linux/init.h>		/* __setup */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include "sd.h"			/* For geometry detection */
#endif

#include <linux/mm.h>		/* For fetching system memory size */
#include <linux/blkdev.h>		/* For block_size() */
#include <linux/delay.h>	/* For ssleep/msleep */

/*
 * Lock protecting manipulation of the ahc softc list.
 */
spinlock_t ahc_list_spinlock;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/* For dynamic sglist size calculation. */
u_int ahc_linux_nseg;
#endif

/*
 * Set this to the delay in seconds after SCSI bus reset.
 * Note, we honor this only for the initial bus reset.
 * The scsi error recovery code performs its own bus settle
 * delay handling for error recovery actions.
 */
#ifdef CONFIG_AIC7XXX_RESET_DELAY_MS
#define AIC7XXX_RESET_DELAY CONFIG_AIC7XXX_RESET_DELAY_MS
#else
#define AIC7XXX_RESET_DELAY 5000
#endif

/*
 * Control collection of SCSI transfer statistics for the /proc filesystem.
 *
 * NOTE: Do NOT enable this when running on kernels version 1.2.x and below.
 * NOTE: This does affect performance since it has to maintain statistics.
 */
#ifdef CONFIG_AIC7XXX_PROC_STATS
#define AIC7XXX_PROC_STATS
#endif

/*
 * To change the default number of tagged transactions allowed per-device,
 * add a line to the lilo.conf file like:
 * append="aic7xxx=verbose,tag_info:{{32,32,32,32},{32,32,32,32}}"
 * which will result in the first four devices on the first two
 * controllers being set to a tagged queue depth of 32.
 *
 * The tag_commands is an array of 16 to allow for wide and twin adapters.
 * Twin adapters will use indexes 0-7 for channel 0, and indexes 8-15
 * for channel 1.
 */
typedef struct {
	uint8_t tag_commands[16];	/* Allow for wide/twin adapters. */
} adapter_tag_info_t;

/*
 * Modify this as you see fit for your system.
 *
 * 0			tagged queuing disabled
 * 1 <= n <= 253	n == max tags ever dispatched.
 *
 * The driver will throttle the number of commands dispatched to a
 * device if it returns queue full.  For devices with a fixed maximum
 * queue depth, the driver will eventually determine this depth and
 * lock it in (a console message is printed to indicate that a lock
 * has occurred).  On some devices, queue full is returned for a temporary
 * resource shortage.  These devices will return queue full at varying
 * depths.  The driver will throttle back when the queue fulls occur and
 * attempt to slowly increase the depth over time as the device recovers
 * from the resource shortage.
 *
 * In this example, the first line will disable tagged queueing for all
 * the devices on the first probed aic7xxx adapter.
 *
 * The second line enables tagged queueing with 4 commands/LUN for IDs
 * (0, 2-11, 13-15), disables tagged queueing for ID 12, and tells the
 * driver to attempt to use up to 64 tags for ID 1.
 *
 * The third line is the same as the first line.
 *
 * The fourth line disables tagged queueing for devices 0 and 3.  It
 * enables tagged queueing for the other IDs, with 16 commands/LUN
 * for IDs 1 and 4, 127 commands/LUN for ID 8, and 4 commands/LUN for
 * IDs 2, 5-7, and 9-15.
 */

/*
 * NOTE: The below structure is for reference only, the actual structure
 *       to modify in order to change things is just below this comment block.
adapter_tag_info_t aic7xxx_tag_info[] =
{
	{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	{{4, 64, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4}},
	{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	{{0, 16, 4, 0, 16, 4, 4, 4, 127, 4, 4, 4, 4, 4, 4, 4}}
};
*/

#ifdef CONFIG_AIC7XXX_CMDS_PER_DEVICE
#define AIC7XXX_CMDS_PER_DEVICE CONFIG_AIC7XXX_CMDS_PER_DEVICE
#else
#define AIC7XXX_CMDS_PER_DEVICE AHC_MAX_QUEUE
#endif

#define AIC7XXX_CONFIGED_TAG_COMMANDS {					\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE,		\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE,		\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE,		\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE,		\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE,		\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE,		\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE,		\
	AIC7XXX_CMDS_PER_DEVICE, AIC7XXX_CMDS_PER_DEVICE		\
}

/*
 * By default, use the number of commands specified by
 * the users kernel configuration.
 */
static adapter_tag_info_t aic7xxx_tag_info[] =
{
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS},
	{AIC7XXX_CONFIGED_TAG_COMMANDS}
};

/*
 * There should be a specific return value for this in scsi.h, but
 * it seems that most drivers ignore it.
 */
#define DID_UNDERFLOW   DID_ERROR

void
ahc_print_path(struct ahc_softc *ahc, struct scb *scb)
{
	printk("(scsi%d:%c:%d:%d): ",
	       ahc->platform_data->host->host_no,
	       scb != NULL ? SCB_GET_CHANNEL(ahc, scb) : 'X',
	       scb != NULL ? SCB_GET_TARGET(ahc, scb) : -1,
	       scb != NULL ? SCB_GET_LUN(scb) : -1);
}

/*
 * XXX - these options apply unilaterally to _all_ 274x/284x/294x
 *       cards in the system.  This should be fixed.  Exceptions to this
 *       rule are noted in the comments.
 */

/*
 * Skip the scsi bus reset.  Non 0 make us skip the reset at startup.  This
 * has no effect on any later resets that might occur due to things like
 * SCSI bus timeouts.
 */
static uint32_t aic7xxx_no_reset;

/*
 * Certain PCI motherboards will scan PCI devices from highest to lowest,
 * others scan from lowest to highest, and they tend to do all kinds of
 * strange things when they come into contact with PCI bridge chips.  The
 * net result of all this is that the PCI card that is actually used to boot
 * the machine is very hard to detect.  Most motherboards go from lowest
 * PCI slot number to highest, and the first SCSI controller found is the
 * one you boot from.  The only exceptions to this are when a controller
 * has its BIOS disabled.  So, we by default sort all of our SCSI controllers
 * from lowest PCI slot number to highest PCI slot number.  We also force
 * all controllers with their BIOS disabled to the end of the list.  This
 * works on *almost* all computers.  Where it doesn't work, we have this
 * option.  Setting this option to non-0 will reverse the order of the sort
 * to highest first, then lowest, but will still leave cards with their BIOS
 * disabled at the very end.  That should fix everyone up unless there are
 * really strange cirumstances.
 */
static uint32_t aic7xxx_reverse_scan;

/*
 * Should we force EXTENDED translation on a controller.
 *     0 == Use whatever is in the SEEPROM or default to off
 *     1 == Use whatever is in the SEEPROM or default to on
 */
static uint32_t aic7xxx_extended;

/*
 * PCI bus parity checking of the Adaptec controllers.  This is somewhat
 * dubious at best.  To my knowledge, this option has never actually
 * solved a PCI parity problem, but on certain machines with broken PCI
 * chipset configurations where stray PCI transactions with bad parity are
 * the norm rather than the exception, the error messages can be overwelming.
 * It's included in the driver for completeness.
 *   0	   = Shut off PCI parity check
 *   non-0 = reverse polarity pci parity checking
 */
static uint32_t aic7xxx_pci_parity = ~0;

/*
 * Certain newer motherboards have put new PCI based devices into the
 * IO spaces that used to typically be occupied by VLB or EISA cards.
 * This overlap can cause these newer motherboards to lock up when scanned
 * for older EISA and VLB devices.  Setting this option to non-0 will
 * cause the driver to skip scanning for any VLB or EISA controllers and
 * only support the PCI controllers.  NOTE: this means that if the kernel
 * os compiled with PCI support disabled, then setting this to non-0
 * would result in never finding any devices :)
 */
#ifndef CONFIG_AIC7XXX_PROBE_EISA_VL
uint32_t aic7xxx_probe_eisa_vl;
#else
uint32_t aic7xxx_probe_eisa_vl = ~0;
#endif

/*
 * There are lots of broken chipsets in the world.  Some of them will
 * violate the PCI spec when we issue byte sized memory writes to our
 * controller.  I/O mapped register access, if allowed by the given
 * platform, will work in almost all cases.
 */
uint32_t aic7xxx_allow_memio = ~0;

/*
 * aic7xxx_detect() has been run, so register all device arrivals
 * immediately with the system rather than deferring to the sorted
 * attachment performed by aic7xxx_detect().
 */
int aic7xxx_detect_complete;

/*
 * So that we can set how long each device is given as a selection timeout.
 * The table of values goes like this:
 *   0 - 256ms
 *   1 - 128ms
 *   2 - 64ms
 *   3 - 32ms
 * We default to 256ms because some older devices need a longer time
 * to respond to initial selection.
 */
static uint32_t aic7xxx_seltime;

/*
 * Certain devices do not perform any aging on commands.  Should the
 * device be saturated by commands in one portion of the disk, it is
 * possible for transactions on far away sectors to never be serviced.
 * To handle these devices, we can periodically send an ordered tag to
 * force all outstanding transactions to be serviced prior to a new
 * transaction.
 */
uint32_t aic7xxx_periodic_otag;

/*
 * Module information and settable options.
 */
static char *aic7xxx = NULL;

MODULE_AUTHOR("Maintainer: Justin T. Gibbs <gibbs@scsiguy.com>");
MODULE_DESCRIPTION("Adaptec Aic77XX/78XX SCSI Host Bus Adapter driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(AIC7XXX_DRIVER_VERSION);
module_param(aic7xxx, charp, 0444);
MODULE_PARM_DESC(aic7xxx,
"period delimited, options string.\n"
"	verbose			Enable verbose/diagnostic logging\n"
"	allow_memio		Allow device registers to be memory mapped\n"
"	debug			Bitmask of debug values to enable\n"
"	no_probe		Toggle EISA/VLB controller probing\n"
"	probe_eisa_vl		Toggle EISA/VLB controller probing\n"
"	no_reset		Supress initial bus resets\n"
"	extended		Enable extended geometry on all controllers\n"
"	periodic_otag		Send an ordered tagged transaction\n"
"				periodically to prevent tag starvation.\n"
"				This may be required by some older disk\n"
"				drives or RAID arrays.\n"
"	reverse_scan		Sort PCI devices highest Bus/Slot to lowest\n"
"	tag_info:<tag_str>	Set per-target tag depth\n"
"	global_tag_depth:<int>	Global tag depth for every target\n"
"				on every bus\n"
"	seltime:<int>		Selection Timeout\n"
"				(0/256ms,1/128ms,2/64ms,3/32ms)\n"
"\n"
"	Sample /etc/modprobe.conf line:\n"
"		Toggle EISA/VLB probing\n"
"		Set tag depth on Controller 1/Target 1 to 10 tags\n"
"		Shorten the selection timeout to 128ms\n"
"\n"
"	options aic7xxx 'aic7xxx=probe_eisa_vl.tag_info:{{}.{.10}}.seltime:1'\n"
);

static void ahc_linux_handle_scsi_status(struct ahc_softc *,
					 struct ahc_linux_device *,
					 struct scb *);
static void ahc_linux_queue_cmd_complete(struct ahc_softc *ahc,
					 Scsi_Cmnd *cmd);
static void ahc_linux_sem_timeout(u_long arg);
static void ahc_linux_freeze_simq(struct ahc_softc *ahc);
static void ahc_linux_release_simq(u_long arg);
static void ahc_linux_dev_timed_unfreeze(u_long arg);
static int  ahc_linux_queue_recovery_cmd(Scsi_Cmnd *cmd, scb_flag flag);
static void ahc_linux_initialize_scsi_bus(struct ahc_softc *ahc);
static void ahc_linux_size_nseg(void);
static void ahc_linux_thread_run_complete_queue(struct ahc_softc *ahc);
static u_int ahc_linux_user_tagdepth(struct ahc_softc *ahc,
				     struct ahc_devinfo *devinfo);
static void ahc_linux_device_queue_depth(struct ahc_softc *ahc,
					 struct ahc_linux_device *dev);
static struct ahc_linux_target*	ahc_linux_alloc_target(struct ahc_softc*,
						       u_int, u_int);
static void			ahc_linux_free_target(struct ahc_softc*,
						      struct ahc_linux_target*);
static struct ahc_linux_device*	ahc_linux_alloc_device(struct ahc_softc*,
						       struct ahc_linux_target*,
						       u_int);
static void			ahc_linux_free_device(struct ahc_softc*,
						      struct ahc_linux_device*);
static int ahc_linux_run_command(struct ahc_softc*,
				 struct ahc_linux_device *,
				 struct scsi_cmnd *);
static void ahc_linux_setup_tag_info_global(char *p);
static aic_option_callback_t ahc_linux_setup_tag_info;
static int  aic7xxx_setup(char *s);
static int  ahc_linux_next_unit(void);
static struct ahc_cmd *ahc_linux_run_complete_queue(struct ahc_softc *ahc);

/********************************* Inlines ************************************/
static __inline struct ahc_linux_device*
		     ahc_linux_get_device(struct ahc_softc *ahc, u_int channel,
					  u_int target, u_int lun, int alloc);
static __inline void ahc_schedule_completeq(struct ahc_softc *ahc);
static __inline void ahc_linux_unmap_scb(struct ahc_softc*, struct scb*);

static __inline int ahc_linux_map_seg(struct ahc_softc *ahc, struct scb *scb,
		 		      struct ahc_dma_seg *sg,
				      dma_addr_t addr, bus_size_t len);

static __inline void
ahc_schedule_completeq(struct ahc_softc *ahc)
{
	if ((ahc->platform_data->flags & AHC_RUN_CMPLT_Q_TIMER) == 0) {
		ahc->platform_data->flags |= AHC_RUN_CMPLT_Q_TIMER;
		ahc->platform_data->completeq_timer.expires = jiffies;
		add_timer(&ahc->platform_data->completeq_timer);
	}
}

static __inline struct ahc_linux_device*
ahc_linux_get_device(struct ahc_softc *ahc, u_int channel, u_int target,
		     u_int lun, int alloc)
{
	struct ahc_linux_target *targ;
	struct ahc_linux_device *dev;
	u_int target_offset;

	target_offset = target;
	if (channel != 0)
		target_offset += 8;
	targ = ahc->platform_data->targets[target_offset];
	if (targ == NULL) {
		if (alloc != 0) {
			targ = ahc_linux_alloc_target(ahc, channel, target);
			if (targ == NULL)
				return (NULL);
		} else
			return (NULL);
	}
	dev = targ->devices[lun];
	if (dev == NULL && alloc != 0)
		dev = ahc_linux_alloc_device(ahc, targ, lun);
	return (dev);
}

#define AHC_LINUX_MAX_RETURNED_ERRORS 4
static struct ahc_cmd *
ahc_linux_run_complete_queue(struct ahc_softc *ahc)
{
	struct	ahc_cmd *acmd;
	u_long	done_flags;
	int	with_errors;

	with_errors = 0;
	ahc_done_lock(ahc, &done_flags);
	while ((acmd = TAILQ_FIRST(&ahc->platform_data->completeq)) != NULL) {
		Scsi_Cmnd *cmd;

		if (with_errors > AHC_LINUX_MAX_RETURNED_ERRORS) {
			/*
			 * Linux uses stack recursion to requeue
			 * commands that need to be retried.  Avoid
			 * blowing out the stack by "spoon feeding"
			 * commands that completed with error back
			 * the operating system in case they are going
			 * to be retried. "ick"
			 */
			ahc_schedule_completeq(ahc);
			break;
		}
		TAILQ_REMOVE(&ahc->platform_data->completeq,
			     acmd, acmd_links.tqe);
		cmd = &acmd_scsi_cmd(acmd);
		cmd->host_scribble = NULL;
		if (ahc_cmd_get_transaction_status(cmd) != DID_OK
		 || (cmd->result & 0xFF) != SCSI_STATUS_OK)
			with_errors++;

		cmd->scsi_done(cmd);
	}
	ahc_done_unlock(ahc, &done_flags);
	return (acmd);
}

static __inline void
ahc_linux_unmap_scb(struct ahc_softc *ahc, struct scb *scb)
{
	Scsi_Cmnd *cmd;

	cmd = scb->io_ctx;
	ahc_sync_sglist(ahc, scb, BUS_DMASYNC_POSTWRITE);
	if (cmd->use_sg != 0) {
		struct scatterlist *sg;

		sg = (struct scatterlist *)cmd->request_buffer;
		pci_unmap_sg(ahc->dev_softc, sg, cmd->use_sg,
			     cmd->sc_data_direction);
	} else if (cmd->request_bufflen != 0) {
		pci_unmap_single(ahc->dev_softc,
				 scb->platform_data->buf_busaddr,
				 cmd->request_bufflen,
				 cmd->sc_data_direction);
	}
}

static __inline int
ahc_linux_map_seg(struct ahc_softc *ahc, struct scb *scb,
		  struct ahc_dma_seg *sg, dma_addr_t addr, bus_size_t len)
{
	int	 consumed;

	if ((scb->sg_count + 1) > AHC_NSEG)
		panic("Too few segs for dma mapping.  "
		      "Increase AHC_NSEG\n");

	consumed = 1;
	sg->addr = ahc_htole32(addr & 0xFFFFFFFF);
	scb->platform_data->xfer_len += len;

	if (sizeof(dma_addr_t) > 4
	 && (ahc->flags & AHC_39BIT_ADDRESSING) != 0)
		len |= (addr >> 8) & AHC_SG_HIGH_ADDR_MASK;

	sg->len = ahc_htole32(len);
	return (consumed);
}

/************************  Host template entry points *************************/
static int	   ahc_linux_detect(Scsi_Host_Template *);
static int	   ahc_linux_queue(Scsi_Cmnd *, void (*)(Scsi_Cmnd *));
static const char *ahc_linux_info(struct Scsi_Host *);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int	   ahc_linux_slave_alloc(Scsi_Device *);
static int	   ahc_linux_slave_configure(Scsi_Device *);
static void	   ahc_linux_slave_destroy(Scsi_Device *);
#if defined(__i386__)
static int	   ahc_linux_biosparam(struct scsi_device*,
				       struct block_device*,
				       sector_t, int[]);
#endif
#else
static int	   ahc_linux_release(struct Scsi_Host *);
static void	   ahc_linux_select_queue_depth(struct Scsi_Host *host,
						Scsi_Device *scsi_devs);
#if defined(__i386__)
static int	   ahc_linux_biosparam(Disk *, kdev_t, int[]);
#endif
#endif
static int	   ahc_linux_bus_reset(Scsi_Cmnd *);
static int	   ahc_linux_dev_reset(Scsi_Cmnd *);
static int	   ahc_linux_abort(Scsi_Cmnd *);

/*
 * Calculate a safe value for AHC_NSEG (as expressed through ahc_linux_nseg).
 *
 * In pre-2.5.X...
 * The midlayer allocates an S/G array dynamically when a command is issued
 * using SCSI malloc.  This array, which is in an OS dependent format that
 * must later be copied to our private S/G list, is sized to house just the
 * number of segments needed for the current transfer.  Since the code that
 * sizes the SCSI malloc pool does not take into consideration fragmentation
 * of the pool, executing transactions numbering just a fraction of our
 * concurrent transaction limit with list lengths aproaching AHC_NSEG will
 * quickly depleat the SCSI malloc pool of usable space.  Unfortunately, the
 * mid-layer does not properly handle this scsi malloc failures for the S/G
 * array and the result can be a lockup of the I/O subsystem.  We try to size
 * our S/G list so that it satisfies our drivers allocation requirements in
 * addition to avoiding fragmentation of the SCSI malloc pool.
 */
static void
ahc_linux_size_nseg(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	u_int cur_size;
	u_int best_size;

	/*
	 * The SCSI allocator rounds to the nearest 512 bytes
	 * an cannot allocate across a page boundary.  Our algorithm
	 * is to start at 1K of scsi malloc space per-command and
	 * loop through all factors of the PAGE_SIZE and pick the best.
	 */
	best_size = 0;
	for (cur_size = 1024; cur_size <= PAGE_SIZE; cur_size *= 2) {
		u_int nseg;

		nseg = cur_size / sizeof(struct scatterlist);
		if (nseg < AHC_LINUX_MIN_NSEG)
			continue;

		if (best_size == 0) {
			best_size = cur_size;
			ahc_linux_nseg = nseg;
		} else {
			u_int best_rem;
			u_int cur_rem;

			/*
			 * Compare the traits of the current "best_size"
			 * with the current size to determine if the
			 * current size is a better size.
			 */
			best_rem = best_size % sizeof(struct scatterlist);
			cur_rem = cur_size % sizeof(struct scatterlist);
			if (cur_rem < best_rem) {
				best_size = cur_size;
				ahc_linux_nseg = nseg;
			}
		}
	}
#endif
}

/*
 * Try to detect an Adaptec 7XXX controller.
 */
static int
ahc_linux_detect(Scsi_Host_Template *template)
{
	struct	ahc_softc *ahc;
	int     found = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/*
	 * It is a bug that the upper layer takes
	 * this lock just prior to calling us.
	 */
	spin_unlock_irq(&io_request_lock);
#endif

	/*
	 * Sanity checking of Linux SCSI data structures so
	 * that some of our hacks^H^H^H^H^Hassumptions aren't
	 * violated.
	 */
	if (offsetof(struct ahc_cmd_internal, end)
	  > offsetof(struct scsi_cmnd, host_scribble)) {
		printf("ahc_linux_detect: SCSI data structures changed.\n");
		printf("ahc_linux_detect: Unable to attach\n");
		return (0);
	}
	ahc_linux_size_nseg();
	/*
	 * If we've been passed any parameters, process them now.
	 */
	if (aic7xxx)
		aic7xxx_setup(aic7xxx);

	template->proc_name = "aic7xxx";

	/*
	 * Initialize our softc list lock prior to
	 * probing for any adapters.
	 */
	ahc_list_lockinit();

	found = ahc_linux_pci_init();
	if (!ahc_linux_eisa_init())
		found++;
	
	/*
	 * Register with the SCSI layer all
	 * controllers we've found.
	 */
	TAILQ_FOREACH(ahc, &ahc_tailq, links) {

		if (ahc_linux_register_host(ahc, template) == 0)
			found++;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	spin_lock_irq(&io_request_lock);
#endif
	aic7xxx_detect_complete++;

	return (found);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/*
 * Free the passed in Scsi_Host memory structures prior to unloading the
 * module.
 */
int
ahc_linux_release(struct Scsi_Host * host)
{
	struct ahc_softc *ahc;
	u_long l;

	ahc_list_lock(&l);
	if (host != NULL) {

		/*
		 * We should be able to just perform
		 * the free directly, but check our
		 * list for extra sanity.
		 */
		ahc = ahc_find_softc(*(struct ahc_softc **)host->hostdata);
		if (ahc != NULL) {
			u_long s;

			ahc_lock(ahc, &s);
			ahc_intr_enable(ahc, FALSE);
			ahc_unlock(ahc, &s);
			ahc_free(ahc);
		}
	}
	ahc_list_unlock(&l);
	return (0);
}
#endif

/*
 * Return a string describing the driver.
 */
static const char *
ahc_linux_info(struct Scsi_Host *host)
{
	static char buffer[512];
	char	ahc_info[256];
	char   *bp;
	struct ahc_softc *ahc;

	bp = &buffer[0];
	ahc = *(struct ahc_softc **)host->hostdata;
	memset(bp, 0, sizeof(buffer));
	strcpy(bp, "Adaptec AIC7XXX EISA/VLB/PCI SCSI HBA DRIVER, Rev ");
	strcat(bp, AIC7XXX_DRIVER_VERSION);
	strcat(bp, "\n");
	strcat(bp, "        <");
	strcat(bp, ahc->description);
	strcat(bp, ">\n");
	strcat(bp, "        ");
	ahc_controller_info(ahc, ahc_info);
	strcat(bp, ahc_info);
	strcat(bp, "\n");

	return (bp);
}

/*
 * Queue an SCB to the controller.
 */
static int
ahc_linux_queue(Scsi_Cmnd * cmd, void (*scsi_done) (Scsi_Cmnd *))
{
	struct	 ahc_softc *ahc;
	struct	 ahc_linux_device *dev;

	ahc = *(struct ahc_softc **)cmd->device->host->hostdata;

	/*
	 * Save the callback on completion function.
	 */
	cmd->scsi_done = scsi_done;

	/*
	 * Close the race of a command that was in the process of
	 * being queued to us just as our simq was frozen.  Let
	 * DV commands through so long as we are only frozen to
	 * perform DV.
	 */
	if (ahc->platform_data->qfrozen != 0)
		return SCSI_MLQUEUE_HOST_BUSY;

	dev = ahc_linux_get_device(ahc, cmd->device->channel, cmd->device->id,
				   cmd->device->lun, /*alloc*/TRUE);
	BUG_ON(dev == NULL);

	cmd->result = CAM_REQ_INPROG << 16;

	return ahc_linux_run_command(ahc, dev, cmd);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int
ahc_linux_slave_alloc(Scsi_Device *device)
{
	struct	ahc_softc *ahc;

	ahc = *((struct ahc_softc **)device->host->hostdata);
	if (bootverbose)
		printf("%s: Slave Alloc %d\n", ahc_name(ahc), device->id);
	return (0);
}

static int
ahc_linux_slave_configure(Scsi_Device *device)
{
	struct	ahc_softc *ahc;
	struct	ahc_linux_device *dev;
	u_long	flags;

	ahc = *((struct ahc_softc **)device->host->hostdata);
	if (bootverbose)
		printf("%s: Slave Configure %d\n", ahc_name(ahc), device->id);
	ahc_midlayer_entrypoint_lock(ahc, &flags);
	/*
	 * Since Linux has attached to the device, configure
	 * it so we don't free and allocate the device
	 * structure on every command.
	 */
	dev = ahc_linux_get_device(ahc, device->channel,
				   device->id, device->lun,
				   /*alloc*/TRUE);
	if (dev != NULL) {
		dev->flags &= ~AHC_DEV_UNCONFIGURED;
		dev->scsi_device = device;
		ahc_linux_device_queue_depth(ahc, dev);
	}
	ahc_midlayer_entrypoint_unlock(ahc, &flags);

	/* Initial Domain Validation */
	if (!spi_initial_dv(device->sdev_target))
		spi_dv_device(device);

	return (0);
}

static void
ahc_linux_slave_destroy(Scsi_Device *device)
{
	struct	ahc_softc *ahc;
	struct	ahc_linux_device *dev;
	u_long	flags;

	ahc = *((struct ahc_softc **)device->host->hostdata);
	if (bootverbose)
		printf("%s: Slave Destroy %d\n", ahc_name(ahc), device->id);
	ahc_midlayer_entrypoint_lock(ahc, &flags);
	dev = ahc_linux_get_device(ahc, device->channel,
				   device->id, device->lun,
					   /*alloc*/FALSE);
	/*
	 * Filter out "silly" deletions of real devices by only
	 * deleting devices that have had slave_configure()
	 * called on them.  All other devices that have not
	 * been configured will automatically be deleted by
	 * the refcounting process.
	 */
	if (dev != NULL
	 && (dev->flags & AHC_DEV_SLAVE_CONFIGURED) != 0) {
		dev->flags |= AHC_DEV_UNCONFIGURED;
		if (dev->active == 0
	 	 && (dev->flags & AHC_DEV_TIMER_ACTIVE) == 0)
			ahc_linux_free_device(ahc, dev);
	}
	ahc_midlayer_entrypoint_unlock(ahc, &flags);
}
#else
/*
 * Sets the queue depth for each SCSI device hanging
 * off the input host adapter.
 */
static void
ahc_linux_select_queue_depth(struct Scsi_Host *host, Scsi_Device *scsi_devs)
{
	Scsi_Device *device;
	Scsi_Device *ldev;
	struct	ahc_softc *ahc;
	u_long	flags;

	ahc = *((struct ahc_softc **)host->hostdata);
	ahc_lock(ahc, &flags);
	for (device = scsi_devs; device != NULL; device = device->next) {

		/*
		 * Watch out for duplicate devices.  This works around
		 * some quirks in how the SCSI scanning code does its
		 * device management.
		 */
		for (ldev = scsi_devs; ldev != device; ldev = ldev->next) {
			if (ldev->host == device->host
			 && ldev->channel == device->channel
			 && ldev->id == device->id
			 && ldev->lun == device->lun)
				break;
		}
		/* Skip duplicate. */
		if (ldev != device)
			continue;

		if (device->host == host) {
			struct	 ahc_linux_device *dev;

			/*
			 * Since Linux has attached to the device, configure
			 * it so we don't free and allocate the device
			 * structure on every command.
			 */
			dev = ahc_linux_get_device(ahc, device->channel,
						   device->id, device->lun,
						   /*alloc*/TRUE);
			if (dev != NULL) {
				dev->flags &= ~AHC_DEV_UNCONFIGURED;
				dev->scsi_device = device;
				ahc_linux_device_queue_depth(ahc, dev);
				device->queue_depth = dev->openings
						    + dev->active;
				if ((dev->flags & (AHC_DEV_Q_BASIC
						| AHC_DEV_Q_TAGGED)) == 0) {
					/*
					 * We allow the OS to queue 2 untagged
					 * transactions to us at any time even
					 * though we can only execute them
					 * serially on the controller/device.
					 * This should remove some latency.
					 */
					device->queue_depth = 2;
				}
			}
		}
	}
	ahc_unlock(ahc, &flags);
}
#endif

#if defined(__i386__)
/*
 * Return the disk geometry for the given SCSI device.
 */
static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
ahc_linux_biosparam(struct scsi_device *sdev, struct block_device *bdev,
		    sector_t capacity, int geom[])
{
	uint8_t *bh;
#else
ahc_linux_biosparam(Disk *disk, kdev_t dev, int geom[])
{
	struct	scsi_device *sdev = disk->device;
	u_long	capacity = disk->capacity;
	struct	buffer_head *bh;
#endif
	int	 heads;
	int	 sectors;
	int	 cylinders;
	int	 ret;
	int	 extended;
	struct	 ahc_softc *ahc;
	u_int	 channel;

	ahc = *((struct ahc_softc **)sdev->host->hostdata);
	channel = sdev->channel;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	bh = scsi_bios_ptable(bdev);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,17)
	bh = bread(MKDEV(MAJOR(dev), MINOR(dev) & ~0xf), 0, block_size(dev));
#else
	bh = bread(MKDEV(MAJOR(dev), MINOR(dev) & ~0xf), 0, 1024);
#endif

	if (bh) {
		ret = scsi_partsize(bh, capacity,
				    &geom[2], &geom[0], &geom[1]);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
		kfree(bh);
#else
		brelse(bh);
#endif
		if (ret != -1)
			return (ret);
	}
	heads = 64;
	sectors = 32;
	cylinders = aic_sector_div(capacity, heads, sectors);

	if (aic7xxx_extended != 0)
		extended = 1;
	else if (channel == 0)
		extended = (ahc->flags & AHC_EXTENDED_TRANS_A) != 0;
	else
		extended = (ahc->flags & AHC_EXTENDED_TRANS_B) != 0;
	if (extended && cylinders >= 1024) {
		heads = 255;
		sectors = 63;
		cylinders = aic_sector_div(capacity, heads, sectors);
	}
	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;
	return (0);
}
#endif

/*
 * Abort the current SCSI command(s).
 */
static int
ahc_linux_abort(Scsi_Cmnd *cmd)
{
	int error;

	error = ahc_linux_queue_recovery_cmd(cmd, SCB_ABORT);
	if (error != 0)
		printf("aic7xxx_abort returns 0x%x\n", error);
	return (error);
}

/*
 * Attempt to send a target reset message to the device that timed out.
 */
static int
ahc_linux_dev_reset(Scsi_Cmnd *cmd)
{
	int error;

	error = ahc_linux_queue_recovery_cmd(cmd, SCB_DEVICE_RESET);
	if (error != 0)
		printf("aic7xxx_dev_reset returns 0x%x\n", error);
	return (error);
}

/*
 * Reset the SCSI bus.
 */
static int
ahc_linux_bus_reset(Scsi_Cmnd *cmd)
{
	struct ahc_softc *ahc;
	u_long s;
	int    found;

	ahc = *(struct ahc_softc **)cmd->device->host->hostdata;
	ahc_midlayer_entrypoint_lock(ahc, &s);
	found = ahc_reset_channel(ahc, cmd->device->channel + 'A',
				  /*initiate reset*/TRUE);
	ahc_linux_run_complete_queue(ahc);
	ahc_midlayer_entrypoint_unlock(ahc, &s);

	if (bootverbose)
		printf("%s: SCSI bus reset delivered. "
		       "%d SCBs aborted.\n", ahc_name(ahc), found);

	return SUCCESS;
}

Scsi_Host_Template aic7xxx_driver_template = {
	.module			= THIS_MODULE,
	.name			= "aic7xxx",
	.proc_info		= ahc_linux_proc_info,
	.info			= ahc_linux_info,
	.queuecommand		= ahc_linux_queue,
	.eh_abort_handler	= ahc_linux_abort,
	.eh_device_reset_handler = ahc_linux_dev_reset,
	.eh_bus_reset_handler	= ahc_linux_bus_reset,
#if defined(__i386__)
	.bios_param		= ahc_linux_biosparam,
#endif
	.can_queue		= AHC_MAX_QUEUE,
	.this_id		= -1,
	.cmd_per_lun		= 2,
	.use_clustering		= ENABLE_CLUSTERING,
	.slave_alloc		= ahc_linux_slave_alloc,
	.slave_configure	= ahc_linux_slave_configure,
	.slave_destroy		= ahc_linux_slave_destroy,
};

/**************************** Tasklet Handler *********************************/

/******************************** Macros **************************************/
#define BUILD_SCSIID(ahc, cmd)						    \
	((((cmd)->device->id << TID_SHIFT) & TID)			    \
	| (((cmd)->device->channel == 0) ? (ahc)->our_id : (ahc)->our_id_b) \
	| (((cmd)->device->channel == 0) ? 0 : TWIN_CHNLB))

/******************************** Bus DMA *************************************/
int
ahc_dma_tag_create(struct ahc_softc *ahc, bus_dma_tag_t parent,
		   bus_size_t alignment, bus_size_t boundary,
		   dma_addr_t lowaddr, dma_addr_t highaddr,
		   bus_dma_filter_t *filter, void *filterarg,
		   bus_size_t maxsize, int nsegments,
		   bus_size_t maxsegsz, int flags, bus_dma_tag_t *ret_tag)
{
	bus_dma_tag_t dmat;

	dmat = malloc(sizeof(*dmat), M_DEVBUF, M_NOWAIT);
	if (dmat == NULL)
		return (ENOMEM);

	/*
	 * Linux is very simplistic about DMA memory.  For now don't
	 * maintain all specification information.  Once Linux supplies
	 * better facilities for doing these operations, or the
	 * needs of this particular driver change, we might need to do
	 * more here.
	 */
	dmat->alignment = alignment;
	dmat->boundary = boundary;
	dmat->maxsize = maxsize;
	*ret_tag = dmat;
	return (0);
}

void
ahc_dma_tag_destroy(struct ahc_softc *ahc, bus_dma_tag_t dmat)
{
	free(dmat, M_DEVBUF);
}

int
ahc_dmamem_alloc(struct ahc_softc *ahc, bus_dma_tag_t dmat, void** vaddr,
		 int flags, bus_dmamap_t *mapp)
{
	bus_dmamap_t map;

	map = malloc(sizeof(*map), M_DEVBUF, M_NOWAIT);
	if (map == NULL)
		return (ENOMEM);
	/*
	 * Although we can dma data above 4GB, our
	 * "consistent" memory is below 4GB for
	 * space efficiency reasons (only need a 4byte
	 * address).  For this reason, we have to reset
	 * our dma mask when doing allocations.
	 */
	if (ahc->dev_softc != NULL)
		if (pci_set_dma_mask(ahc->dev_softc, 0xFFFFFFFF)) {
			printk(KERN_WARNING "aic7xxx: No suitable DMA available.\n");
			kfree(map);
			return (ENODEV);
		}
	*vaddr = pci_alloc_consistent(ahc->dev_softc,
				      dmat->maxsize, &map->bus_addr);
	if (ahc->dev_softc != NULL)
		if (pci_set_dma_mask(ahc->dev_softc,
				     ahc->platform_data->hw_dma_mask)) {
			printk(KERN_WARNING "aic7xxx: No suitable DMA available.\n");
			kfree(map);
			return (ENODEV);
		}
	if (*vaddr == NULL)
		return (ENOMEM);
	*mapp = map;
	return(0);
}

void
ahc_dmamem_free(struct ahc_softc *ahc, bus_dma_tag_t dmat,
		void* vaddr, bus_dmamap_t map)
{
	pci_free_consistent(ahc->dev_softc, dmat->maxsize,
			    vaddr, map->bus_addr);
}

int
ahc_dmamap_load(struct ahc_softc *ahc, bus_dma_tag_t dmat, bus_dmamap_t map,
		void *buf, bus_size_t buflen, bus_dmamap_callback_t *cb,
		void *cb_arg, int flags)
{
	/*
	 * Assume for now that this will only be used during
	 * initialization and not for per-transaction buffer mapping.
	 */
	bus_dma_segment_t stack_sg;

	stack_sg.ds_addr = map->bus_addr;
	stack_sg.ds_len = dmat->maxsize;
	cb(cb_arg, &stack_sg, /*nseg*/1, /*error*/0);
	return (0);
}

void
ahc_dmamap_destroy(struct ahc_softc *ahc, bus_dma_tag_t dmat, bus_dmamap_t map)
{
	/*
	 * The map may is NULL in our < 2.3.X implementation.
	 * Now it's 2.6.5, but just in case...
	 */
	BUG_ON(map == NULL);
	free(map, M_DEVBUF);
}

int
ahc_dmamap_unload(struct ahc_softc *ahc, bus_dma_tag_t dmat, bus_dmamap_t map)
{
	/* Nothing to do */
	return (0);
}

/********************* Platform Dependent Functions ***************************/
/*
 * Compare "left hand" softc with "right hand" softc, returning:
 * < 0 - lahc has a lower priority than rahc
 *   0 - Softcs are equal
 * > 0 - lahc has a higher priority than rahc
 */
int
ahc_softc_comp(struct ahc_softc *lahc, struct ahc_softc *rahc)
{
	int	value;
	int	rvalue;
	int	lvalue;

	/*
	 * Under Linux, cards are ordered as follows:
	 *	1) VLB/EISA BIOS enabled devices sorted by BIOS address.
	 *	2) PCI devices with BIOS enabled sorted by bus/slot/func.
	 *	3) All remaining VLB/EISA devices sorted by ioport.
	 *	4) All remaining PCI devices sorted by bus/slot/func.
	 */
	value = (lahc->flags & AHC_BIOS_ENABLED)
	      - (rahc->flags & AHC_BIOS_ENABLED);
	if (value != 0)
		/* Controllers with BIOS enabled have a *higher* priority */
		return (value);

	/*
	 * Same BIOS setting, now sort based on bus type.
	 * EISA and VL controllers sort together.  EISA/VL
	 * have higher priority than PCI.
	 */
	rvalue = (rahc->chip & AHC_BUS_MASK);
 	if (rvalue == AHC_VL)
		rvalue = AHC_EISA;
	lvalue = (lahc->chip & AHC_BUS_MASK);
 	if (lvalue == AHC_VL)
		lvalue = AHC_EISA;
	value = rvalue - lvalue;
	if (value != 0)
		return (value);

	/* Still equal.  Sort by BIOS address, ioport, or bus/slot/func. */
	switch (rvalue) {
#ifdef CONFIG_PCI
	case AHC_PCI:
	{
		char primary_channel;

		if (aic7xxx_reverse_scan != 0)
			value = ahc_get_pci_bus(lahc->dev_softc)
			      - ahc_get_pci_bus(rahc->dev_softc);
		else
			value = ahc_get_pci_bus(rahc->dev_softc)
			      - ahc_get_pci_bus(lahc->dev_softc);
		if (value != 0)
			break;
		if (aic7xxx_reverse_scan != 0)
			value = ahc_get_pci_slot(lahc->dev_softc)
			      - ahc_get_pci_slot(rahc->dev_softc);
		else
			value = ahc_get_pci_slot(rahc->dev_softc)
			      - ahc_get_pci_slot(lahc->dev_softc);
		if (value != 0)
			break;
		/*
		 * On multi-function devices, the user can choose
		 * to have function 1 probed before function 0.
		 * Give whichever channel is the primary channel
		 * the highest priority.
		 */
		primary_channel = (lahc->flags & AHC_PRIMARY_CHANNEL) + 'A';
		value = -1;
		if (lahc->channel == primary_channel)
			value = 1;
		break;
	}
#endif
	case AHC_EISA:
		if ((rahc->flags & AHC_BIOS_ENABLED) != 0) {
			value = rahc->platform_data->bios_address
			      - lahc->platform_data->bios_address; 
		} else {
			value = rahc->bsh.ioport
			      - lahc->bsh.ioport; 
		}
		break;
	default:
		panic("ahc_softc_sort: invalid bus type");
	}
	return (value);
}

static void
ahc_linux_setup_tag_info_global(char *p)
{
	int tags, i, j;

	tags = simple_strtoul(p + 1, NULL, 0) & 0xff;
	printf("Setting Global Tags= %d\n", tags);

	for (i = 0; i < NUM_ELEMENTS(aic7xxx_tag_info); i++) {
		for (j = 0; j < AHC_NUM_TARGETS; j++) {
			aic7xxx_tag_info[i].tag_commands[j] = tags;
		}
	}
}

static void
ahc_linux_setup_tag_info(u_long arg, int instance, int targ, int32_t value)
{

	if ((instance >= 0) && (targ >= 0)
	 && (instance < NUM_ELEMENTS(aic7xxx_tag_info))
	 && (targ < AHC_NUM_TARGETS)) {
		aic7xxx_tag_info[instance].tag_commands[targ] = value & 0xff;
		if (bootverbose)
			printf("tag_info[%d:%d] = %d\n", instance, targ, value);
	}
}

/*
 * Handle Linux boot parameters. This routine allows for assigning a value
 * to a parameter with a ':' between the parameter and the value.
 * ie. aic7xxx=stpwlev:1,extended
 */
static int
aic7xxx_setup(char *s)
{
	int	i, n;
	char   *p;
	char   *end;

	static struct {
		const char *name;
		uint32_t *flag;
	} options[] = {
		{ "extended", &aic7xxx_extended },
		{ "no_reset", &aic7xxx_no_reset },
		{ "verbose", &aic7xxx_verbose },
		{ "allow_memio", &aic7xxx_allow_memio},
#ifdef AHC_DEBUG
		{ "debug", &ahc_debug },
#endif
		{ "reverse_scan", &aic7xxx_reverse_scan },
		{ "no_probe", &aic7xxx_probe_eisa_vl },
		{ "probe_eisa_vl", &aic7xxx_probe_eisa_vl },
		{ "periodic_otag", &aic7xxx_periodic_otag },
		{ "pci_parity", &aic7xxx_pci_parity },
		{ "seltime", &aic7xxx_seltime },
		{ "tag_info", NULL },
		{ "global_tag_depth", NULL },
		{ "dv", NULL }
	};

	end = strchr(s, '\0');

	/*
	 * XXX ia64 gcc isn't smart enough to know that NUM_ELEMENTS
	 * will never be 0 in this case.
	 */
	n = 0;

	while ((p = strsep(&s, ",.")) != NULL) {
		if (*p == '\0')
			continue;
		for (i = 0; i < NUM_ELEMENTS(options); i++) {

			n = strlen(options[i].name);
			if (strncmp(options[i].name, p, n) == 0)
				break;
		}
		if (i == NUM_ELEMENTS(options))
			continue;

		if (strncmp(p, "global_tag_depth", n) == 0) {
			ahc_linux_setup_tag_info_global(p + n);
		} else if (strncmp(p, "tag_info", n) == 0) {
			s = aic_parse_brace_option("tag_info", p + n, end,
			    2, ahc_linux_setup_tag_info, 0);
		} else if (p[n] == ':') {
			*(options[i].flag) = simple_strtoul(p + n + 1, NULL, 0);
		} else if (strncmp(p, "verbose", n) == 0) {
			*(options[i].flag) = 1;
		} else {
			*(options[i].flag) ^= 0xFFFFFFFF;
		}
	}
	return 1;
}

__setup("aic7xxx=", aic7xxx_setup);

uint32_t aic7xxx_verbose;

int
ahc_linux_register_host(struct ahc_softc *ahc, Scsi_Host_Template *template)
{
	char	 buf[80];
	struct	 Scsi_Host *host;
	char	*new_name;
	u_long	 s;

	template->name = ahc->description;
	host = scsi_host_alloc(template, sizeof(struct ahc_softc *));
	if (host == NULL)
		return (ENOMEM);

	*((struct ahc_softc **)host->hostdata) = ahc;
	ahc_lock(ahc, &s);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	scsi_assign_lock(host, &ahc->platform_data->spin_lock);
#elif AHC_SCSI_HAS_HOST_LOCK != 0
	host->lock = &ahc->platform_data->spin_lock;
#endif
	ahc->platform_data->host = host;
	host->can_queue = AHC_MAX_QUEUE;
	host->cmd_per_lun = 2;
	/* XXX No way to communicate the ID for multiple channels */
	host->this_id = ahc->our_id;
	host->irq = ahc->platform_data->irq;
	host->max_id = (ahc->features & AHC_WIDE) ? 16 : 8;
	host->max_lun = AHC_NUM_LUNS;
	host->max_channel = (ahc->features & AHC_TWIN) ? 1 : 0;
	host->sg_tablesize = AHC_NSEG;
	ahc_set_unit(ahc, ahc_linux_next_unit());
	sprintf(buf, "scsi%d", host->host_no);
	new_name = malloc(strlen(buf) + 1, M_DEVBUF, M_NOWAIT);
	if (new_name != NULL) {
		strcpy(new_name, buf);
		ahc_set_name(ahc, new_name);
	}
	host->unique_id = ahc->unit;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	scsi_set_pci_device(host, ahc->dev_softc);
#endif
	ahc_linux_initialize_scsi_bus(ahc);
	ahc_intr_enable(ahc, TRUE);
	ahc_unlock(ahc, &s);

	host->transportt = ahc_linux_transport_template;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	scsi_add_host(host, (ahc->dev_softc ? &ahc->dev_softc->dev : NULL)); /* XXX handle failure */
	scsi_scan_host(host);
#endif
	return (0);
}

uint64_t
ahc_linux_get_memsize(void)
{
	struct sysinfo si;

	si_meminfo(&si);
	return ((uint64_t)si.totalram << PAGE_SHIFT);
}

/*
 * Find the smallest available unit number to use
 * for a new device.  We don't just use a static
 * count to handle the "repeated hot-(un)plug"
 * scenario.
 */
static int
ahc_linux_next_unit(void)
{
	struct ahc_softc *ahc;
	int unit;

	unit = 0;
retry:
	TAILQ_FOREACH(ahc, &ahc_tailq, links) {
		if (ahc->unit == unit) {
			unit++;
			goto retry;
		}
	}
	return (unit);
}

/*
 * Place the SCSI bus into a known state by either resetting it,
 * or forcing transfer negotiations on the next command to any
 * target.
 */
void
ahc_linux_initialize_scsi_bus(struct ahc_softc *ahc)
{
	int i;
	int numtarg;

	i = 0;
	numtarg = 0;

	if (aic7xxx_no_reset != 0)
		ahc->flags &= ~(AHC_RESET_BUS_A|AHC_RESET_BUS_B);

	if ((ahc->flags & AHC_RESET_BUS_A) != 0)
		ahc_reset_channel(ahc, 'A', /*initiate_reset*/TRUE);
	else
		numtarg = (ahc->features & AHC_WIDE) ? 16 : 8;

	if ((ahc->features & AHC_TWIN) != 0) {

		if ((ahc->flags & AHC_RESET_BUS_B) != 0) {
			ahc_reset_channel(ahc, 'B', /*initiate_reset*/TRUE);
		} else {
			if (numtarg == 0)
				i = 8;
			numtarg += 8;
		}
	}

	/*
	 * Force negotiation to async for all targets that
	 * will not see an initial bus reset.
	 */
	for (; i < numtarg; i++) {
		struct ahc_devinfo devinfo;
		struct ahc_initiator_tinfo *tinfo;
		struct ahc_tmode_tstate *tstate;
		u_int our_id;
		u_int target_id;
		char channel;

		channel = 'A';
		our_id = ahc->our_id;
		target_id = i;
		if (i > 7 && (ahc->features & AHC_TWIN) != 0) {
			channel = 'B';
			our_id = ahc->our_id_b;
			target_id = i % 8;
		}
		tinfo = ahc_fetch_transinfo(ahc, channel, our_id,
					    target_id, &tstate);
		ahc_compile_devinfo(&devinfo, our_id, target_id,
				    CAM_LUN_WILDCARD, channel, ROLE_INITIATOR);
		ahc_update_neg_request(ahc, &devinfo, tstate,
				       tinfo, AHC_NEG_ALWAYS);
	}
	/* Give the bus some time to recover */
	if ((ahc->flags & (AHC_RESET_BUS_A|AHC_RESET_BUS_B)) != 0) {
		ahc_linux_freeze_simq(ahc);
		init_timer(&ahc->platform_data->reset_timer);
		ahc->platform_data->reset_timer.data = (u_long)ahc;
		ahc->platform_data->reset_timer.expires =
		    jiffies + (AIC7XXX_RESET_DELAY * HZ)/1000;
		ahc->platform_data->reset_timer.function =
		    ahc_linux_release_simq;
		add_timer(&ahc->platform_data->reset_timer);
	}
}

int
ahc_platform_alloc(struct ahc_softc *ahc, void *platform_arg)
{

	ahc->platform_data =
	    malloc(sizeof(struct ahc_platform_data), M_DEVBUF, M_NOWAIT);
	if (ahc->platform_data == NULL)
		return (ENOMEM);
	memset(ahc->platform_data, 0, sizeof(struct ahc_platform_data));
	TAILQ_INIT(&ahc->platform_data->completeq);
	TAILQ_INIT(&ahc->platform_data->device_runq);
	ahc->platform_data->irq = AHC_LINUX_NOIRQ;
	ahc->platform_data->hw_dma_mask = 0xFFFFFFFF;
	ahc_lockinit(ahc);
	ahc_done_lockinit(ahc);
	init_timer(&ahc->platform_data->completeq_timer);
	ahc->platform_data->completeq_timer.data = (u_long)ahc;
	ahc->platform_data->completeq_timer.function =
	    (ahc_linux_callback_t *)ahc_linux_thread_run_complete_queue;
	init_MUTEX_LOCKED(&ahc->platform_data->eh_sem);
	ahc->seltime = (aic7xxx_seltime & 0x3) << 4;
	ahc->seltime_b = (aic7xxx_seltime & 0x3) << 4;
	if (aic7xxx_pci_parity == 0)
		ahc->flags |= AHC_DISABLE_PCI_PERR;

	return (0);
}

void
ahc_platform_free(struct ahc_softc *ahc)
{
	struct ahc_linux_target *targ;
	struct ahc_linux_device *dev;
	int i, j;

	if (ahc->platform_data != NULL) {
		del_timer_sync(&ahc->platform_data->completeq_timer);
		if (ahc->platform_data->host != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
			scsi_remove_host(ahc->platform_data->host);
#endif
			scsi_host_put(ahc->platform_data->host);
		}

		/* destroy all of the device and target objects */
		for (i = 0; i < AHC_NUM_TARGETS; i++) {
			targ = ahc->platform_data->targets[i];
			if (targ != NULL) {
				/* Keep target around through the loop. */
				targ->refcount++;
				for (j = 0; j < AHC_NUM_LUNS; j++) {

					if (targ->devices[j] == NULL)
						continue;
					dev = targ->devices[j];
					ahc_linux_free_device(ahc, dev);
				}
				/*
				 * Forcibly free the target now that
				 * all devices are gone.
				 */
				ahc_linux_free_target(ahc, targ);
 			}
 		}

		if (ahc->platform_data->irq != AHC_LINUX_NOIRQ)
			free_irq(ahc->platform_data->irq, ahc);
		if (ahc->tag == BUS_SPACE_PIO
		 && ahc->bsh.ioport != 0)
			release_region(ahc->bsh.ioport, 256);
		if (ahc->tag == BUS_SPACE_MEMIO
		 && ahc->bsh.maddr != NULL) {
			iounmap(ahc->bsh.maddr);
			release_mem_region(ahc->platform_data->mem_busaddr,
					   0x1000);
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
		/*
		 * In 2.4 we detach from the scsi midlayer before the PCI
		 * layer invokes our remove callback.  No per-instance
		 * detach is provided, so we must reach inside the PCI
		 * subsystem's internals and detach our driver manually.
		 */
		if (ahc->dev_softc != NULL)
			ahc->dev_softc->driver = NULL;
#endif
		free(ahc->platform_data, M_DEVBUF);
	}
}

void
ahc_platform_freeze_devq(struct ahc_softc *ahc, struct scb *scb)
{
	ahc_platform_abort_scbs(ahc, SCB_GET_TARGET(ahc, scb),
				SCB_GET_CHANNEL(ahc, scb),
				SCB_GET_LUN(scb), SCB_LIST_NULL,
				ROLE_UNKNOWN, CAM_REQUEUE_REQ);
}

void
ahc_platform_set_tags(struct ahc_softc *ahc, struct ahc_devinfo *devinfo,
		      ahc_queue_alg alg)
{
	struct ahc_linux_device *dev;
	int was_queuing;
	int now_queuing;

	dev = ahc_linux_get_device(ahc, devinfo->channel - 'A',
				   devinfo->target,
				   devinfo->lun, /*alloc*/FALSE);
	if (dev == NULL)
		return;
	was_queuing = dev->flags & (AHC_DEV_Q_BASIC|AHC_DEV_Q_TAGGED);
	switch (alg) {
	default:
	case AHC_QUEUE_NONE:
		now_queuing = 0;
		break; 
	case AHC_QUEUE_BASIC:
		now_queuing = AHC_DEV_Q_BASIC;
		break;
	case AHC_QUEUE_TAGGED:
		now_queuing = AHC_DEV_Q_TAGGED;
		break;
	}
	if ((dev->flags & AHC_DEV_FREEZE_TIL_EMPTY) == 0
	 && (was_queuing != now_queuing)
	 && (dev->active != 0)) {
		dev->flags |= AHC_DEV_FREEZE_TIL_EMPTY;
		dev->qfrozen++;
	}

	dev->flags &= ~(AHC_DEV_Q_BASIC|AHC_DEV_Q_TAGGED|AHC_DEV_PERIODIC_OTAG);
	if (now_queuing) {
		u_int usertags;

		usertags = ahc_linux_user_tagdepth(ahc, devinfo);
		if (!was_queuing) {
			/*
			 * Start out agressively and allow our
			 * dynamic queue depth algorithm to take
			 * care of the rest.
			 */
			dev->maxtags = usertags;
			dev->openings = dev->maxtags - dev->active;
		}
		if (dev->maxtags == 0) {
			/*
			 * Queueing is disabled by the user.
			 */
			dev->openings = 1;
		} else if (alg == AHC_QUEUE_TAGGED) {
			dev->flags |= AHC_DEV_Q_TAGGED;
			if (aic7xxx_periodic_otag != 0)
				dev->flags |= AHC_DEV_PERIODIC_OTAG;
		} else
			dev->flags |= AHC_DEV_Q_BASIC;
	} else {
		/* We can only have one opening. */
		dev->maxtags = 0;
		dev->openings =  1 - dev->active;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	if (dev->scsi_device != NULL) {
		switch ((dev->flags & (AHC_DEV_Q_BASIC|AHC_DEV_Q_TAGGED))) {
		case AHC_DEV_Q_BASIC:
			scsi_adjust_queue_depth(dev->scsi_device,
						MSG_SIMPLE_TASK,
						dev->openings + dev->active);
			break;
		case AHC_DEV_Q_TAGGED:
			scsi_adjust_queue_depth(dev->scsi_device,
						MSG_ORDERED_TASK,
						dev->openings + dev->active);
			break;
		default:
			/*
			 * We allow the OS to queue 2 untagged transactions to
			 * us at any time even though we can only execute them
			 * serially on the controller/device.  This should
			 * remove some latency.
			 */
			scsi_adjust_queue_depth(dev->scsi_device,
						/*NON-TAGGED*/0,
						/*queue depth*/2);
			break;
		}
	}
#endif
}

int
ahc_platform_abort_scbs(struct ahc_softc *ahc, int target, char channel,
			int lun, u_int tag, role_t role, uint32_t status)
{
	return 0;
}

static void
ahc_linux_thread_run_complete_queue(struct ahc_softc *ahc)
{
	u_long flags;

	ahc_lock(ahc, &flags);
	del_timer(&ahc->platform_data->completeq_timer);
	ahc->platform_data->flags &= ~AHC_RUN_CMPLT_Q_TIMER;
	ahc_linux_run_complete_queue(ahc);
	ahc_unlock(ahc, &flags);
}

static u_int
ahc_linux_user_tagdepth(struct ahc_softc *ahc, struct ahc_devinfo *devinfo)
{
	static int warned_user;
	u_int tags;

	tags = 0;
	if ((ahc->user_discenable & devinfo->target_mask) != 0) {
		if (ahc->unit >= NUM_ELEMENTS(aic7xxx_tag_info)) {
			if (warned_user == 0) {

				printf(KERN_WARNING
"aic7xxx: WARNING: Insufficient tag_info instances\n"
"aic7xxx: for installed controllers. Using defaults\n"
"aic7xxx: Please update the aic7xxx_tag_info array in\n"
"aic7xxx: the aic7xxx_osm..c source file.\n");
				warned_user++;
			}
			tags = AHC_MAX_QUEUE;
		} else {
			adapter_tag_info_t *tag_info;

			tag_info = &aic7xxx_tag_info[ahc->unit];
			tags = tag_info->tag_commands[devinfo->target_offset];
			if (tags > AHC_MAX_QUEUE)
				tags = AHC_MAX_QUEUE;
		}
	}
	return (tags);
}

/*
 * Determines the queue depth for a given device.
 */
static void
ahc_linux_device_queue_depth(struct ahc_softc *ahc,
			     struct ahc_linux_device *dev)
{
	struct	ahc_devinfo devinfo;
	u_int	tags;

	ahc_compile_devinfo(&devinfo,
			    dev->target->channel == 0
			  ? ahc->our_id : ahc->our_id_b,
			    dev->target->target, dev->lun,
			    dev->target->channel == 0 ? 'A' : 'B',
			    ROLE_INITIATOR);
	tags = ahc_linux_user_tagdepth(ahc, &devinfo);
	if (tags != 0
	 && dev->scsi_device != NULL
	 && dev->scsi_device->tagged_supported != 0) {

		ahc_set_tags(ahc, &devinfo, AHC_QUEUE_TAGGED);
		ahc_print_devinfo(ahc, &devinfo);
		printf("Tagged Queuing enabled.  Depth %d\n", tags);
	} else {
		ahc_set_tags(ahc, &devinfo, AHC_QUEUE_NONE);
	}
}

static int
ahc_linux_run_command(struct ahc_softc *ahc, struct ahc_linux_device *dev,
		      struct scsi_cmnd *cmd)
{
	struct	 scb *scb;
	struct	 hardware_scb *hscb;
	struct	 ahc_initiator_tinfo *tinfo;
	struct	 ahc_tmode_tstate *tstate;
	uint16_t mask;
	struct scb_tailq *untagged_q = NULL;

	/*
	 * Schedule us to run later.  The only reason we are not
	 * running is because the whole controller Q is frozen.
	 */
	if (ahc->platform_data->qfrozen != 0)
		return SCSI_MLQUEUE_HOST_BUSY;

	/*
	 * We only allow one untagged transaction
	 * per target in the initiator role unless
	 * we are storing a full busy target *lun*
	 * table in SCB space.
	 */
	if (!blk_rq_tagged(cmd->request)
	    && (ahc->features & AHC_SCB_BTT) == 0) {
		int target_offset;

		target_offset = cmd->device->id + cmd->device->channel * 8;
		untagged_q = &(ahc->untagged_queues[target_offset]);
		if (!TAILQ_EMPTY(untagged_q))
			/* if we're already executing an untagged command
			 * we're busy to another */
			return SCSI_MLQUEUE_DEVICE_BUSY;
	}

	/*
	 * Get an scb to use.
	 */
	if ((scb = ahc_get_scb(ahc)) == NULL) {
			ahc->flags |= AHC_RESOURCE_SHORTAGE;
			return SCSI_MLQUEUE_HOST_BUSY;
	}

	scb->io_ctx = cmd;
	scb->platform_data->dev = dev;
	hscb = scb->hscb;
	cmd->host_scribble = (char *)scb;

	/*
	 * Fill out basics of the HSCB.
	 */
	hscb->control = 0;
	hscb->scsiid = BUILD_SCSIID(ahc, cmd);
	hscb->lun = cmd->device->lun;
	mask = SCB_GET_TARGET_MASK(ahc, scb);
	tinfo = ahc_fetch_transinfo(ahc, SCB_GET_CHANNEL(ahc, scb),
				    SCB_GET_OUR_ID(scb),
				    SCB_GET_TARGET(ahc, scb), &tstate);
	hscb->scsirate = tinfo->scsirate;
	hscb->scsioffset = tinfo->curr.offset;
	if ((tstate->ultraenb & mask) != 0)
		hscb->control |= ULTRAENB;
	
	if ((ahc->user_discenable & mask) != 0)
		hscb->control |= DISCENB;
	
	if ((tstate->auto_negotiate & mask) != 0) {
		scb->flags |= SCB_AUTO_NEGOTIATE;
		scb->hscb->control |= MK_MESSAGE;
	}

	if ((dev->flags & (AHC_DEV_Q_TAGGED|AHC_DEV_Q_BASIC)) != 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
		int	msg_bytes;
		uint8_t tag_msgs[2];
		
		msg_bytes = scsi_populate_tag_msg(cmd, tag_msgs);
		if (msg_bytes && tag_msgs[0] != MSG_SIMPLE_TASK) {
			hscb->control |= tag_msgs[0];
			if (tag_msgs[0] == MSG_ORDERED_TASK)
				dev->commands_since_idle_or_otag = 0;
		} else
#endif
		if (dev->commands_since_idle_or_otag == AHC_OTAG_THRESH
		    && (dev->flags & AHC_DEV_Q_TAGGED) != 0) {
			hscb->control |= MSG_ORDERED_TASK;
			dev->commands_since_idle_or_otag = 0;
		} else {
			hscb->control |= MSG_SIMPLE_TASK;
		}
	}

	hscb->cdb_len = cmd->cmd_len;
	if (hscb->cdb_len <= 12) {
		memcpy(hscb->shared_data.cdb, cmd->cmnd, hscb->cdb_len);
	} else {
		memcpy(hscb->cdb32, cmd->cmnd, hscb->cdb_len);
		scb->flags |= SCB_CDB32_PTR;
	}

	scb->platform_data->xfer_len = 0;
	ahc_set_residual(scb, 0);
	ahc_set_sense_residual(scb, 0);
	scb->sg_count = 0;
	if (cmd->use_sg != 0) {
		struct	ahc_dma_seg *sg;
		struct	scatterlist *cur_seg;
		struct	scatterlist *end_seg;
		int	nseg;

		cur_seg = (struct scatterlist *)cmd->request_buffer;
		nseg = pci_map_sg(ahc->dev_softc, cur_seg, cmd->use_sg,
				  cmd->sc_data_direction);
		end_seg = cur_seg + nseg;
		/* Copy the segments into the SG list. */
		sg = scb->sg_list;
		/*
		 * The sg_count may be larger than nseg if
		 * a transfer crosses a 32bit page.
		 */ 
		while (cur_seg < end_seg) {
			dma_addr_t addr;
			bus_size_t len;
			int consumed;

			addr = sg_dma_address(cur_seg);
			len = sg_dma_len(cur_seg);
			consumed = ahc_linux_map_seg(ahc, scb,
						     sg, addr, len);
			sg += consumed;
			scb->sg_count += consumed;
			cur_seg++;
		}
		sg--;
		sg->len |= ahc_htole32(AHC_DMA_LAST_SEG);

		/*
		 * Reset the sg list pointer.
		 */
		scb->hscb->sgptr =
			ahc_htole32(scb->sg_list_phys | SG_FULL_RESID);
		
		/*
		 * Copy the first SG into the "current"
		 * data pointer area.
		 */
		scb->hscb->dataptr = scb->sg_list->addr;
		scb->hscb->datacnt = scb->sg_list->len;
	} else if (cmd->request_bufflen != 0) {
		struct	 ahc_dma_seg *sg;
		dma_addr_t addr;

		sg = scb->sg_list;
		addr = pci_map_single(ahc->dev_softc,
				      cmd->request_buffer,
				      cmd->request_bufflen,
				      cmd->sc_data_direction);
		scb->platform_data->buf_busaddr = addr;
		scb->sg_count = ahc_linux_map_seg(ahc, scb,
						  sg, addr,
						  cmd->request_bufflen);
		sg->len |= ahc_htole32(AHC_DMA_LAST_SEG);

		/*
		 * Reset the sg list pointer.
		 */
		scb->hscb->sgptr =
			ahc_htole32(scb->sg_list_phys | SG_FULL_RESID);

		/*
		 * Copy the first SG into the "current"
		 * data pointer area.
		 */
		scb->hscb->dataptr = sg->addr;
		scb->hscb->datacnt = sg->len;
	} else {
		scb->hscb->sgptr = ahc_htole32(SG_LIST_NULL);
		scb->hscb->dataptr = 0;
		scb->hscb->datacnt = 0;
		scb->sg_count = 0;
	}

	LIST_INSERT_HEAD(&ahc->pending_scbs, scb, pending_links);
	dev->openings--;
	dev->active++;
	dev->commands_issued++;
	if ((dev->flags & AHC_DEV_PERIODIC_OTAG) != 0)
		dev->commands_since_idle_or_otag++;
	
	scb->flags |= SCB_ACTIVE;
	if (untagged_q) {
		TAILQ_INSERT_TAIL(untagged_q, scb, links.tqe);
		scb->flags |= SCB_UNTAGGEDQ;
	}
	ahc_queue_scb(ahc, scb);
	return 0;
}

/*
 * SCSI controller interrupt handler.
 */
irqreturn_t
ahc_linux_isr(int irq, void *dev_id, struct pt_regs * regs)
{
	struct	ahc_softc *ahc;
	u_long	flags;
	int	ours;

	ahc = (struct ahc_softc *) dev_id;
	ahc_lock(ahc, &flags); 
	ours = ahc_intr(ahc);
	ahc_linux_run_complete_queue(ahc);
	ahc_unlock(ahc, &flags);
	return IRQ_RETVAL(ours);
}

void
ahc_platform_flushwork(struct ahc_softc *ahc)
{

	while (ahc_linux_run_complete_queue(ahc) != NULL)
		;
}

static struct ahc_linux_target*
ahc_linux_alloc_target(struct ahc_softc *ahc, u_int channel, u_int target)
{
	struct ahc_linux_target *targ;
	u_int target_offset;

	target_offset = target;
	if (channel != 0)
		target_offset += 8;

	targ = malloc(sizeof(*targ), M_DEVBUG, M_NOWAIT);
	if (targ == NULL)
		return (NULL);
	memset(targ, 0, sizeof(*targ));
	targ->channel = channel;
	targ->target = target;
	targ->ahc = ahc;
	ahc->platform_data->targets[target_offset] = targ;
	return (targ);
}

static void
ahc_linux_free_target(struct ahc_softc *ahc, struct ahc_linux_target *targ)
{
	struct ahc_devinfo devinfo;
	struct ahc_initiator_tinfo *tinfo;
	struct ahc_tmode_tstate *tstate;
	u_int our_id;
	u_int target_offset;
	char channel;

	/*
	 * Force a negotiation to async/narrow on any
	 * future command to this device unless a bus
	 * reset occurs between now and that command.
	 */
	channel = 'A' + targ->channel;
	our_id = ahc->our_id;
	target_offset = targ->target;
	if (targ->channel != 0) {
		target_offset += 8;
		our_id = ahc->our_id_b;
	}
	tinfo = ahc_fetch_transinfo(ahc, channel, our_id,
				    targ->target, &tstate);
	ahc_compile_devinfo(&devinfo, our_id, targ->target, CAM_LUN_WILDCARD,
			    channel, ROLE_INITIATOR);
	ahc_set_syncrate(ahc, &devinfo, NULL, 0, 0, 0,
			 AHC_TRANS_GOAL, /*paused*/FALSE);
	ahc_set_width(ahc, &devinfo, MSG_EXT_WDTR_BUS_8_BIT,
		      AHC_TRANS_GOAL, /*paused*/FALSE);
	ahc_update_neg_request(ahc, &devinfo, tstate, tinfo, AHC_NEG_ALWAYS);
	ahc->platform_data->targets[target_offset] = NULL;
	free(targ, M_DEVBUF);
}

static struct ahc_linux_device*
ahc_linux_alloc_device(struct ahc_softc *ahc,
		 struct ahc_linux_target *targ, u_int lun)
{
	struct ahc_linux_device *dev;

	dev = malloc(sizeof(*dev), M_DEVBUG, M_NOWAIT);
	if (dev == NULL)
		return (NULL);
	memset(dev, 0, sizeof(*dev));
	init_timer(&dev->timer);
	dev->flags = AHC_DEV_UNCONFIGURED;
	dev->lun = lun;
	dev->target = targ;

	/*
	 * We start out life using untagged
	 * transactions of which we allow one.
	 */
	dev->openings = 1;

	/*
	 * Set maxtags to 0.  This will be changed if we
	 * later determine that we are dealing with
	 * a tagged queuing capable device.
	 */
	dev->maxtags = 0;
	
	targ->refcount++;
	targ->devices[lun] = dev;
	return (dev);
}

static void
__ahc_linux_free_device(struct ahc_softc *ahc, struct ahc_linux_device *dev)
{
	struct ahc_linux_target *targ;

	targ = dev->target;
	targ->devices[dev->lun] = NULL;
	free(dev, M_DEVBUF);
	targ->refcount--;
	if (targ->refcount == 0)
		ahc_linux_free_target(ahc, targ);
}

static void
ahc_linux_free_device(struct ahc_softc *ahc, struct ahc_linux_device *dev)
{
	del_timer_sync(&dev->timer);
	__ahc_linux_free_device(ahc, dev);
}

void
ahc_send_async(struct ahc_softc *ahc, char channel,
	       u_int target, u_int lun, ac_code code, void *arg)
{
	switch (code) {
	case AC_TRANSFER_NEG:
	{
		char	buf[80];
		struct	ahc_linux_target *targ;
		struct	info_str info;
		struct	ahc_initiator_tinfo *tinfo;
		struct	ahc_tmode_tstate *tstate;
		int	target_offset;

		info.buffer = buf;
		info.length = sizeof(buf);
		info.offset = 0;
		info.pos = 0;
		tinfo = ahc_fetch_transinfo(ahc, channel,
						channel == 'A' ? ahc->our_id
							       : ahc->our_id_b,
						target, &tstate);

		/*
		 * Don't bother reporting results while
		 * negotiations are still pending.
		 */
		if (tinfo->curr.period != tinfo->goal.period
		 || tinfo->curr.width != tinfo->goal.width
		 || tinfo->curr.offset != tinfo->goal.offset
		 || tinfo->curr.ppr_options != tinfo->goal.ppr_options)
			if (bootverbose == 0)
				break;

		/*
		 * Don't bother reporting results that
		 * are identical to those last reported.
		 */
		target_offset = target;
		if (channel == 'B')
			target_offset += 8;
		targ = ahc->platform_data->targets[target_offset];
		if (targ == NULL)
			break;
		if (tinfo->curr.period == targ->last_tinfo.period
		 && tinfo->curr.width == targ->last_tinfo.width
		 && tinfo->curr.offset == targ->last_tinfo.offset
		 && tinfo->curr.ppr_options == targ->last_tinfo.ppr_options)
			if (bootverbose == 0)
				break;

		targ->last_tinfo.period = tinfo->curr.period;
		targ->last_tinfo.width = tinfo->curr.width;
		targ->last_tinfo.offset = tinfo->curr.offset;
		targ->last_tinfo.ppr_options = tinfo->curr.ppr_options;

		printf("(%s:%c:", ahc_name(ahc), channel);
		if (target == CAM_TARGET_WILDCARD)
			printf("*): ");
		else
			printf("%d): ", target);
		ahc_format_transinfo(&info, &tinfo->curr);
		if (info.pos < info.length)
			*info.buffer = '\0';
		else
			buf[info.length - 1] = '\0';
		printf("%s", buf);
		break;
	}
        case AC_SENT_BDR:
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
		WARN_ON(lun != CAM_LUN_WILDCARD);
		scsi_report_device_reset(ahc->platform_data->host,
					 channel - 'A', target);
#else
		Scsi_Device *scsi_dev;

		/*
		 * Find the SCSI device associated with this
		 * request and indicate that a UA is expected.
		 */
		for (scsi_dev = ahc->platform_data->host->host_queue;
		     scsi_dev != NULL; scsi_dev = scsi_dev->next) {
			if (channel - 'A' == scsi_dev->channel
			 && target == scsi_dev->id
			 && (lun == CAM_LUN_WILDCARD
			  || lun == scsi_dev->lun)) {
				scsi_dev->was_reset = 1;
				scsi_dev->expecting_cc_ua = 1;
			}
		}
#endif
		break;
	}
        case AC_BUS_RESET:
		if (ahc->platform_data->host != NULL) {
			scsi_report_bus_reset(ahc->platform_data->host,
					      channel - 'A');
		}
                break;
        default:
                panic("ahc_send_async: Unexpected async event");
        }
}

/*
 * Calls the higher level scsi done function and frees the scb.
 */
void
ahc_done(struct ahc_softc *ahc, struct scb *scb)
{
	Scsi_Cmnd *cmd;
	struct	   ahc_linux_device *dev;

	LIST_REMOVE(scb, pending_links);
	if ((scb->flags & SCB_UNTAGGEDQ) != 0) {
		struct scb_tailq *untagged_q;
		int target_offset;

		target_offset = SCB_GET_TARGET_OFFSET(ahc, scb);
		untagged_q = &(ahc->untagged_queues[target_offset]);
		TAILQ_REMOVE(untagged_q, scb, links.tqe);
		BUG_ON(!TAILQ_EMPTY(untagged_q));
	}

	if ((scb->flags & SCB_ACTIVE) == 0) {
		printf("SCB %d done'd twice\n", scb->hscb->tag);
		ahc_dump_card_state(ahc);
		panic("Stopping for safety");
	}
	cmd = scb->io_ctx;
	dev = scb->platform_data->dev;
	dev->active--;
	dev->openings++;
	if ((cmd->result & (CAM_DEV_QFRZN << 16)) != 0) {
		cmd->result &= ~(CAM_DEV_QFRZN << 16);
		dev->qfrozen--;
	}
	ahc_linux_unmap_scb(ahc, scb);

	/*
	 * Guard against stale sense data.
	 * The Linux mid-layer assumes that sense
	 * was retrieved anytime the first byte of
	 * the sense buffer looks "sane".
	 */
	cmd->sense_buffer[0] = 0;
	if (ahc_get_transaction_status(scb) == CAM_REQ_INPROG) {
		uint32_t amount_xferred;

		amount_xferred =
		    ahc_get_transfer_length(scb) - ahc_get_residual(scb);
		if ((scb->flags & SCB_TRANSMISSION_ERROR) != 0) {
#ifdef AHC_DEBUG
			if ((ahc_debug & AHC_SHOW_MISC) != 0) {
				ahc_print_path(ahc, scb);
				printf("Set CAM_UNCOR_PARITY\n");
			}
#endif
			ahc_set_transaction_status(scb, CAM_UNCOR_PARITY);
#ifdef AHC_REPORT_UNDERFLOWS
		/*
		 * This code is disabled by default as some
		 * clients of the SCSI system do not properly
		 * initialize the underflow parameter.  This
		 * results in spurious termination of commands
		 * that complete as expected (e.g. underflow is
		 * allowed as command can return variable amounts
		 * of data.
		 */
		} else if (amount_xferred < scb->io_ctx->underflow) {
			u_int i;

			ahc_print_path(ahc, scb);
			printf("CDB:");
			for (i = 0; i < scb->io_ctx->cmd_len; i++)
				printf(" 0x%x", scb->io_ctx->cmnd[i]);
			printf("\n");
			ahc_print_path(ahc, scb);
			printf("Saw underflow (%ld of %ld bytes). "
			       "Treated as error\n",
				ahc_get_residual(scb),
				ahc_get_transfer_length(scb));
			ahc_set_transaction_status(scb, CAM_DATA_RUN_ERR);
#endif
		} else {
			ahc_set_transaction_status(scb, CAM_REQ_CMP);
		}
	} else if (ahc_get_transaction_status(scb) == CAM_SCSI_STATUS_ERROR) {
		ahc_linux_handle_scsi_status(ahc, dev, scb);
	} else if (ahc_get_transaction_status(scb) == CAM_SEL_TIMEOUT) {
		dev->flags |= AHC_DEV_UNCONFIGURED;
	}

	if (dev->openings == 1
	 && ahc_get_transaction_status(scb) == CAM_REQ_CMP
	 && ahc_get_scsi_status(scb) != SCSI_STATUS_QUEUE_FULL)
		dev->tag_success_count++;
	/*
	 * Some devices deal with temporary internal resource
	 * shortages by returning queue full.  When the queue
	 * full occurrs, we throttle back.  Slowly try to get
	 * back to our previous queue depth.
	 */
	if ((dev->openings + dev->active) < dev->maxtags
	 && dev->tag_success_count > AHC_TAG_SUCCESS_INTERVAL) {
		dev->tag_success_count = 0;
		dev->openings++;
	}

	if (dev->active == 0)
		dev->commands_since_idle_or_otag = 0;

	if ((dev->flags & AHC_DEV_UNCONFIGURED) != 0
	    && dev->active == 0
	    && (dev->flags & AHC_DEV_TIMER_ACTIVE) == 0)
		ahc_linux_free_device(ahc, dev);
	else if ((dev->flags & AHC_DEV_ON_RUN_LIST) == 0) {
		TAILQ_INSERT_TAIL(&ahc->platform_data->device_runq, dev, links);
		dev->flags |= AHC_DEV_ON_RUN_LIST;
	}

	if ((scb->flags & SCB_RECOVERY_SCB) != 0) {
		printf("Recovery SCB completes\n");
		if (ahc_get_transaction_status(scb) == CAM_BDR_SENT
		 || ahc_get_transaction_status(scb) == CAM_REQ_ABORTED)
			ahc_set_transaction_status(scb, CAM_CMD_TIMEOUT);
		if ((ahc->platform_data->flags & AHC_UP_EH_SEMAPHORE) != 0) {
			ahc->platform_data->flags &= ~AHC_UP_EH_SEMAPHORE;
			up(&ahc->platform_data->eh_sem);
		}
	}

	ahc_free_scb(ahc, scb);
	ahc_linux_queue_cmd_complete(ahc, cmd);
}

static void
ahc_linux_handle_scsi_status(struct ahc_softc *ahc,
			     struct ahc_linux_device *dev, struct scb *scb)
{
	struct	ahc_devinfo devinfo;

	ahc_compile_devinfo(&devinfo,
			    ahc->our_id,
			    dev->target->target, dev->lun,
			    dev->target->channel == 0 ? 'A' : 'B',
			    ROLE_INITIATOR);
	
	/*
	 * We don't currently trust the mid-layer to
	 * properly deal with queue full or busy.  So,
	 * when one occurs, we tell the mid-layer to
	 * unconditionally requeue the command to us
	 * so that we can retry it ourselves.  We also
	 * implement our own throttling mechanism so
	 * we don't clobber the device with too many
	 * commands.
	 */
	switch (ahc_get_scsi_status(scb)) {
	default:
		break;
	case SCSI_STATUS_CHECK_COND:
	case SCSI_STATUS_CMD_TERMINATED:
	{
		Scsi_Cmnd *cmd;

		/*
		 * Copy sense information to the OS's cmd
		 * structure if it is available.
		 */
		cmd = scb->io_ctx;
		if (scb->flags & SCB_SENSE) {
			u_int sense_size;

			sense_size = MIN(sizeof(struct scsi_sense_data)
				       - ahc_get_sense_residual(scb),
					 sizeof(cmd->sense_buffer));
			memcpy(cmd->sense_buffer,
			       ahc_get_sense_buf(ahc, scb), sense_size);
			if (sense_size < sizeof(cmd->sense_buffer))
				memset(&cmd->sense_buffer[sense_size], 0,
				       sizeof(cmd->sense_buffer) - sense_size);
			cmd->result |= (DRIVER_SENSE << 24);
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOW_SENSE) {
				int i;

				printf("Copied %d bytes of sense data:",
				       sense_size);
				for (i = 0; i < sense_size; i++) {
					if ((i & 0xF) == 0)
						printf("\n");
					printf("0x%x ", cmd->sense_buffer[i]);
				}
				printf("\n");
			}
#endif
		}
		break;
	}
	case SCSI_STATUS_QUEUE_FULL:
	{
		/*
		 * By the time the core driver has returned this
		 * command, all other commands that were queued
		 * to us but not the device have been returned.
		 * This ensures that dev->active is equal to
		 * the number of commands actually queued to
		 * the device.
		 */
		dev->tag_success_count = 0;
		if (dev->active != 0) {
			/*
			 * Drop our opening count to the number
			 * of commands currently outstanding.
			 */
			dev->openings = 0;
/*
			ahc_print_path(ahc, scb);
			printf("Dropping tag count to %d\n", dev->active);
 */
			if (dev->active == dev->tags_on_last_queuefull) {

				dev->last_queuefull_same_count++;
				/*
				 * If we repeatedly see a queue full
				 * at the same queue depth, this
				 * device has a fixed number of tag
				 * slots.  Lock in this tag depth
				 * so we stop seeing queue fulls from
				 * this device.
				 */
				if (dev->last_queuefull_same_count
				 == AHC_LOCK_TAGS_COUNT) {
					dev->maxtags = dev->active;
					ahc_print_path(ahc, scb);
					printf("Locking max tag count at %d\n",
					       dev->active);
				}
			} else {
				dev->tags_on_last_queuefull = dev->active;
				dev->last_queuefull_same_count = 0;
			}
			ahc_set_transaction_status(scb, CAM_REQUEUE_REQ);
			ahc_set_scsi_status(scb, SCSI_STATUS_OK);
			ahc_platform_set_tags(ahc, &devinfo,
				     (dev->flags & AHC_DEV_Q_BASIC)
				   ? AHC_QUEUE_BASIC : AHC_QUEUE_TAGGED);
			break;
		}
		/*
		 * Drop down to a single opening, and treat this
		 * as if the target returned BUSY SCSI status.
		 */
		dev->openings = 1;
		ahc_set_scsi_status(scb, SCSI_STATUS_BUSY);
		ahc_platform_set_tags(ahc, &devinfo,
			     (dev->flags & AHC_DEV_Q_BASIC)
			   ? AHC_QUEUE_BASIC : AHC_QUEUE_TAGGED);
		/* FALLTHROUGH */
	}
	case SCSI_STATUS_BUSY:
	{
		/*
		 * Set a short timer to defer sending commands for
		 * a bit since Linux will not delay in this case.
		 */
		if ((dev->flags & AHC_DEV_TIMER_ACTIVE) != 0) {
			printf("%s:%c:%d: Device Timer still active during "
			       "busy processing\n", ahc_name(ahc),
				dev->target->channel, dev->target->target);
			break;
		}
		dev->flags |= AHC_DEV_TIMER_ACTIVE;
		dev->qfrozen++;
		init_timer(&dev->timer);
		dev->timer.data = (u_long)dev;
		dev->timer.expires = jiffies + (HZ/2);
		dev->timer.function = ahc_linux_dev_timed_unfreeze;
		add_timer(&dev->timer);
		break;
	}
	}
}

static void
ahc_linux_queue_cmd_complete(struct ahc_softc *ahc, Scsi_Cmnd *cmd)
{
	/*
	 * Typically, the complete queue has very few entries
	 * queued to it before the queue is emptied by
	 * ahc_linux_run_complete_queue, so sorting the entries
	 * by generation number should be inexpensive.
	 * We perform the sort so that commands that complete
	 * with an error are retuned in the order origionally
	 * queued to the controller so that any subsequent retries
	 * are performed in order.  The underlying ahc routines do
	 * not guarantee the order that aborted commands will be
	 * returned to us.
	 */
	struct ahc_completeq *completeq;
	struct ahc_cmd *list_cmd;
	struct ahc_cmd *acmd;

	/*
	 * Map CAM error codes into Linux Error codes.  We
	 * avoid the conversion so that the DV code has the
	 * full error information available when making
	 * state change decisions.
	 */
	{
		u_int new_status;

		switch (ahc_cmd_get_transaction_status(cmd)) {
		case CAM_REQ_INPROG:
		case CAM_REQ_CMP:
		case CAM_SCSI_STATUS_ERROR:
			new_status = DID_OK;
			break;
		case CAM_REQ_ABORTED:
			new_status = DID_ABORT;
			break;
		case CAM_BUSY:
			new_status = DID_BUS_BUSY;
			break;
		case CAM_REQ_INVALID:
		case CAM_PATH_INVALID:
			new_status = DID_BAD_TARGET;
			break;
		case CAM_SEL_TIMEOUT:
			new_status = DID_NO_CONNECT;
			break;
		case CAM_SCSI_BUS_RESET:
		case CAM_BDR_SENT:
			new_status = DID_RESET;
			break;
		case CAM_UNCOR_PARITY:
			new_status = DID_PARITY;
			break;
		case CAM_CMD_TIMEOUT:
			new_status = DID_TIME_OUT;
			break;
		case CAM_UA_ABORT:
		case CAM_REQ_CMP_ERR:
		case CAM_AUTOSENSE_FAIL:
		case CAM_NO_HBA:
		case CAM_DATA_RUN_ERR:
		case CAM_UNEXP_BUSFREE:
		case CAM_SEQUENCE_FAIL:
		case CAM_CCB_LEN_ERR:
		case CAM_PROVIDE_FAIL:
		case CAM_REQ_TERMIO:
		case CAM_UNREC_HBA_ERROR:
		case CAM_REQ_TOO_BIG:
			new_status = DID_ERROR;
			break;
		case CAM_REQUEUE_REQ:
			/*
			 * If we want the request requeued, make sure there
			 * are sufficent retries.  In the old scsi error code,
			 * we used to be able to specify a result code that
			 * bypassed the retry count.  Now we must use this
			 * hack.  We also "fake" a check condition with
			 * a sense code of ABORTED COMMAND.  This seems to
			 * evoke a retry even if this command is being sent
			 * via the eh thread.  Ick!  Ick!  Ick!
			 */
			if (cmd->retries > 0)
				cmd->retries--;
			new_status = DID_OK;
			ahc_cmd_set_scsi_status(cmd, SCSI_STATUS_CHECK_COND);
			cmd->result |= (DRIVER_SENSE << 24);
			memset(cmd->sense_buffer, 0,
			       sizeof(cmd->sense_buffer));
			cmd->sense_buffer[0] = SSD_ERRCODE_VALID
					     | SSD_CURRENT_ERROR;
			cmd->sense_buffer[2] = SSD_KEY_ABORTED_COMMAND;
			break;
		default:
			/* We should never get here */
			new_status = DID_ERROR;
			break;
		}

		ahc_cmd_set_transaction_status(cmd, new_status);
	}

	completeq = &ahc->platform_data->completeq;
	list_cmd = TAILQ_FIRST(completeq);
	acmd = (struct ahc_cmd *)cmd;
	while (list_cmd != NULL
	    && acmd_scsi_cmd(list_cmd).serial_number
	     < acmd_scsi_cmd(acmd).serial_number)
		list_cmd = TAILQ_NEXT(list_cmd, acmd_links.tqe);
	if (list_cmd != NULL)
		TAILQ_INSERT_BEFORE(list_cmd, acmd, acmd_links.tqe);
	else
		TAILQ_INSERT_TAIL(completeq, acmd, acmd_links.tqe);
}

static void
ahc_linux_sem_timeout(u_long arg)
{
	struct	ahc_softc *ahc;
	u_long	s;

	ahc = (struct ahc_softc *)arg;

	ahc_lock(ahc, &s);
	if ((ahc->platform_data->flags & AHC_UP_EH_SEMAPHORE) != 0) {
		ahc->platform_data->flags &= ~AHC_UP_EH_SEMAPHORE;
		up(&ahc->platform_data->eh_sem);
	}
	ahc_unlock(ahc, &s);
}

static void
ahc_linux_freeze_simq(struct ahc_softc *ahc)
{
	ahc->platform_data->qfrozen++;
	if (ahc->platform_data->qfrozen == 1) {
		scsi_block_requests(ahc->platform_data->host);

		/* XXX What about Twin channels? */
		ahc_platform_abort_scbs(ahc, CAM_TARGET_WILDCARD, ALL_CHANNELS,
					CAM_LUN_WILDCARD, SCB_LIST_NULL,
					ROLE_INITIATOR, CAM_REQUEUE_REQ);
	}
}

static void
ahc_linux_release_simq(u_long arg)
{
	struct ahc_softc *ahc;
	u_long s;
	int    unblock_reqs;

	ahc = (struct ahc_softc *)arg;

	unblock_reqs = 0;
	ahc_lock(ahc, &s);
	if (ahc->platform_data->qfrozen > 0)
		ahc->platform_data->qfrozen--;
	if (ahc->platform_data->qfrozen == 0)
		unblock_reqs = 1;
	ahc_unlock(ahc, &s);
	/*
	 * There is still a race here.  The mid-layer
	 * should keep its own freeze count and use
	 * a bottom half handler to run the queues
	 * so we can unblock with our own lock held.
	 */
	if (unblock_reqs)
		scsi_unblock_requests(ahc->platform_data->host);
}

static void
ahc_linux_dev_timed_unfreeze(u_long arg)
{
	struct ahc_linux_device *dev;
	struct ahc_softc *ahc;
	u_long s;

	dev = (struct ahc_linux_device *)arg;
	ahc = dev->target->ahc;
	ahc_lock(ahc, &s);
	dev->flags &= ~AHC_DEV_TIMER_ACTIVE;
	if (dev->qfrozen > 0)
		dev->qfrozen--;
	if (dev->active == 0)
		__ahc_linux_free_device(ahc, dev);
	ahc_unlock(ahc, &s);
}

static int
ahc_linux_queue_recovery_cmd(Scsi_Cmnd *cmd, scb_flag flag)
{
	struct ahc_softc *ahc;
	struct ahc_linux_device *dev;
	struct scb *pending_scb;
	u_long s;
	u_int  saved_scbptr;
	u_int  active_scb_index;
	u_int  last_phase;
	u_int  saved_scsiid;
	u_int  cdb_byte;
	int    retval;
	int    was_paused;
	int    paused;
	int    wait;
	int    disconnected;

	pending_scb = NULL;
	paused = FALSE;
	wait = FALSE;
	ahc = *(struct ahc_softc **)cmd->device->host->hostdata;

	printf("%s:%d:%d:%d: Attempting to queue a%s message\n",
	       ahc_name(ahc), cmd->device->channel,
	       cmd->device->id, cmd->device->lun,
	       flag == SCB_ABORT ? "n ABORT" : " TARGET RESET");

	printf("CDB:");
	for (cdb_byte = 0; cdb_byte < cmd->cmd_len; cdb_byte++)
		printf(" 0x%x", cmd->cmnd[cdb_byte]);
	printf("\n");

	/*
	 * In all versions of Linux, we have to work around
	 * a major flaw in how the mid-layer is locked down
	 * if we are to sleep successfully in our error handler
	 * while allowing our interrupt handler to run.  Since
	 * the midlayer acquires either the io_request_lock or
	 * our lock prior to calling us, we must use the
	 * spin_unlock_irq() method for unlocking our lock.
	 * This will force interrupts to be enabled on the
	 * current CPU.  Since the EH thread should not have
	 * been running with CPU interrupts disabled other than
	 * by acquiring either the io_request_lock or our own
	 * lock, this *should* be safe.
	 */
	ahc_midlayer_entrypoint_lock(ahc, &s);

	/*
	 * First determine if we currently own this command.
	 * Start by searching the device queue.  If not found
	 * there, check the pending_scb list.  If not found
	 * at all, and the system wanted us to just abort the
	 * command, return success.
	 */
	dev = ahc_linux_get_device(ahc, cmd->device->channel, cmd->device->id,
				   cmd->device->lun, /*alloc*/FALSE);

	if (dev == NULL) {
		/*
		 * No target device for this command exists,
		 * so we must not still own the command.
		 */
		printf("%s:%d:%d:%d: Is not an active device\n",
		       ahc_name(ahc), cmd->device->channel, cmd->device->id,
		       cmd->device->lun);
		retval = SUCCESS;
		goto no_cmd;
	}

	if ((dev->flags & (AHC_DEV_Q_BASIC|AHC_DEV_Q_TAGGED)) == 0
	 && ahc_search_untagged_queues(ahc, cmd, cmd->device->id,
				       cmd->device->channel + 'A',
				       cmd->device->lun,
				       CAM_REQ_ABORTED, SEARCH_COMPLETE) != 0) {
		printf("%s:%d:%d:%d: Command found on untagged queue\n",
		       ahc_name(ahc), cmd->device->channel, cmd->device->id,
		       cmd->device->lun);
		retval = SUCCESS;
		goto done;
	}

	/*
	 * See if we can find a matching cmd in the pending list.
	 */
	LIST_FOREACH(pending_scb, &ahc->pending_scbs, pending_links) {
		if (pending_scb->io_ctx == cmd)
			break;
	}

	if (pending_scb == NULL && flag == SCB_DEVICE_RESET) {

		/* Any SCB for this device will do for a target reset */
		LIST_FOREACH(pending_scb, &ahc->pending_scbs, pending_links) {
		  	if (ahc_match_scb(ahc, pending_scb, cmd->device->id,
					  cmd->device->channel + 'A',
					  CAM_LUN_WILDCARD,
					  SCB_LIST_NULL, ROLE_INITIATOR) == 0)
				break;
		}
	}

	if (pending_scb == NULL) {
		printf("%s:%d:%d:%d: Command not found\n",
		       ahc_name(ahc), cmd->device->channel, cmd->device->id,
		       cmd->device->lun);
		goto no_cmd;
	}

	if ((pending_scb->flags & SCB_RECOVERY_SCB) != 0) {
		/*
		 * We can't queue two recovery actions using the same SCB
		 */
		retval = FAILED;
		goto  done;
	}

	/*
	 * Ensure that the card doesn't do anything
	 * behind our back and that we didn't "just" miss
	 * an interrupt that would affect this cmd.
	 */
	was_paused = ahc_is_paused(ahc);
	ahc_pause_and_flushwork(ahc);
	paused = TRUE;

	if ((pending_scb->flags & SCB_ACTIVE) == 0) {
		printf("%s:%d:%d:%d: Command already completed\n",
		       ahc_name(ahc), cmd->device->channel, cmd->device->id,
		       cmd->device->lun);
		goto no_cmd;
	}

	printf("%s: At time of recovery, card was %spaused\n",
	       ahc_name(ahc), was_paused ? "" : "not ");
	ahc_dump_card_state(ahc);

	disconnected = TRUE;
	if (flag == SCB_ABORT) {
		if (ahc_search_qinfifo(ahc, cmd->device->id,
				       cmd->device->channel + 'A',
				       cmd->device->lun,
				       pending_scb->hscb->tag,
				       ROLE_INITIATOR, CAM_REQ_ABORTED,
				       SEARCH_COMPLETE) > 0) {
			printf("%s:%d:%d:%d: Cmd aborted from QINFIFO\n",
			       ahc_name(ahc), cmd->device->channel,
					cmd->device->id, cmd->device->lun);
			retval = SUCCESS;
			goto done;
		}
	} else if (ahc_search_qinfifo(ahc, cmd->device->id,
				      cmd->device->channel + 'A',
				      cmd->device->lun, pending_scb->hscb->tag,
				      ROLE_INITIATOR, /*status*/0,
				      SEARCH_COUNT) > 0) {
		disconnected = FALSE;
	}

	if (disconnected && (ahc_inb(ahc, SEQ_FLAGS) & NOT_IDENTIFIED) == 0) {
		struct scb *bus_scb;

		bus_scb = ahc_lookup_scb(ahc, ahc_inb(ahc, SCB_TAG));
		if (bus_scb == pending_scb)
			disconnected = FALSE;
		else if (flag != SCB_ABORT
		      && ahc_inb(ahc, SAVED_SCSIID) == pending_scb->hscb->scsiid
		      && ahc_inb(ahc, SAVED_LUN) == SCB_GET_LUN(pending_scb))
			disconnected = FALSE;
	}

	/*
	 * At this point, pending_scb is the scb associated with the
	 * passed in command.  That command is currently active on the
	 * bus, is in the disconnected state, or we're hoping to find
	 * a command for the same target active on the bus to abuse to
	 * send a BDR.  Queue the appropriate message based on which of
	 * these states we are in.
	 */
	last_phase = ahc_inb(ahc, LASTPHASE);
	saved_scbptr = ahc_inb(ahc, SCBPTR);
	active_scb_index = ahc_inb(ahc, SCB_TAG);
	saved_scsiid = ahc_inb(ahc, SAVED_SCSIID);
	if (last_phase != P_BUSFREE
	 && (pending_scb->hscb->tag == active_scb_index
	  || (flag == SCB_DEVICE_RESET
	   && SCSIID_TARGET(ahc, saved_scsiid) == cmd->device->id))) {

		/*
		 * We're active on the bus, so assert ATN
		 * and hope that the target responds.
		 */
		pending_scb = ahc_lookup_scb(ahc, active_scb_index);
		pending_scb->flags |= SCB_RECOVERY_SCB|flag;
		ahc_outb(ahc, MSG_OUT, HOST_MSG);
		ahc_outb(ahc, SCSISIGO, last_phase|ATNO);
		printf("%s:%d:%d:%d: Device is active, asserting ATN\n",
		       ahc_name(ahc), cmd->device->channel, cmd->device->id,
		       cmd->device->lun);
		wait = TRUE;
	} else if (disconnected) {

		/*
		 * Actually re-queue this SCB in an attempt
		 * to select the device before it reconnects.
		 * In either case (selection or reselection),
		 * we will now issue the approprate message
		 * to the timed-out device.
		 *
		 * Set the MK_MESSAGE control bit indicating
		 * that we desire to send a message.  We
		 * also set the disconnected flag since
		 * in the paging case there is no guarantee
		 * that our SCB control byte matches the
		 * version on the card.  We don't want the
		 * sequencer to abort the command thinking
		 * an unsolicited reselection occurred.
		 */
		pending_scb->hscb->control |= MK_MESSAGE|DISCONNECTED;
		pending_scb->flags |= SCB_RECOVERY_SCB|flag;

		/*
		 * Remove any cached copy of this SCB in the
		 * disconnected list in preparation for the
		 * queuing of our abort SCB.  We use the
		 * same element in the SCB, SCB_NEXT, for
		 * both the qinfifo and the disconnected list.
		 */
		ahc_search_disc_list(ahc, cmd->device->id,
				     cmd->device->channel + 'A',
				     cmd->device->lun, pending_scb->hscb->tag,
				     /*stop_on_first*/TRUE,
				     /*remove*/TRUE,
				     /*save_state*/FALSE);

		/*
		 * In the non-paging case, the sequencer will
		 * never re-reference the in-core SCB.
		 * To make sure we are notified during
		 * reslection, set the MK_MESSAGE flag in
		 * the card's copy of the SCB.
		 */
		if ((ahc->flags & AHC_PAGESCBS) == 0) {
			ahc_outb(ahc, SCBPTR, pending_scb->hscb->tag);
			ahc_outb(ahc, SCB_CONTROL,
				 ahc_inb(ahc, SCB_CONTROL)|MK_MESSAGE);
		}

		/*
		 * Clear out any entries in the QINFIFO first
		 * so we are the next SCB for this target
		 * to run.
		 */
		ahc_search_qinfifo(ahc, cmd->device->id,
				   cmd->device->channel + 'A',
				   cmd->device->lun, SCB_LIST_NULL,
				   ROLE_INITIATOR, CAM_REQUEUE_REQ,
				   SEARCH_COMPLETE);
		ahc_qinfifo_requeue_tail(ahc, pending_scb);
		ahc_outb(ahc, SCBPTR, saved_scbptr);
		ahc_print_path(ahc, pending_scb);
		printf("Device is disconnected, re-queuing SCB\n");
		wait = TRUE;
	} else {
		printf("%s:%d:%d:%d: Unable to deliver message\n",
		       ahc_name(ahc), cmd->device->channel, cmd->device->id,
		       cmd->device->lun);
		retval = FAILED;
		goto done;
	}

no_cmd:
	/*
	 * Our assumption is that if we don't have the command, no
	 * recovery action was required, so we return success.  Again,
	 * the semantics of the mid-layer recovery engine are not
	 * well defined, so this may change in time.
	 */
	retval = SUCCESS;
done:
	if (paused)
		ahc_unpause(ahc);
	if (wait) {
		struct timer_list timer;
		int ret;

		ahc->platform_data->flags |= AHC_UP_EH_SEMAPHORE;
		spin_unlock_irq(&ahc->platform_data->spin_lock);
		init_timer(&timer);
		timer.data = (u_long)ahc;
		timer.expires = jiffies + (5 * HZ);
		timer.function = ahc_linux_sem_timeout;
		add_timer(&timer);
		printf("Recovery code sleeping\n");
		down(&ahc->platform_data->eh_sem);
		printf("Recovery code awake\n");
        	ret = del_timer_sync(&timer);
		if (ret == 0) {
			printf("Timer Expired\n");
			retval = FAILED;
		}
		spin_lock_irq(&ahc->platform_data->spin_lock);
	}
	ahc_linux_run_complete_queue(ahc);
	ahc_midlayer_entrypoint_unlock(ahc, &s);
	return (retval);
}

void
ahc_platform_dump_card_state(struct ahc_softc *ahc)
{
}

static void ahc_linux_exit(void);

static void ahc_linux_get_width(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	spi_width(starget) = tinfo->curr.width;
}

static void ahc_linux_set_width(struct scsi_target *starget, int width)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_devinfo devinfo;
	unsigned long flags;

	ahc_compile_devinfo(&devinfo, shost->this_id, starget->id, 0,
			    starget->channel + 'A', ROLE_INITIATOR);
	ahc_lock(ahc, &flags);
	ahc_set_width(ahc, &devinfo, width, AHC_TRANS_GOAL, FALSE);
	ahc_unlock(ahc, &flags);
}

static void ahc_linux_get_period(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	spi_period(starget) = tinfo->curr.period;
}

static void ahc_linux_set_period(struct scsi_target *starget, int period)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	struct ahc_devinfo devinfo;
	unsigned int ppr_options = tinfo->curr.ppr_options;
	unsigned long flags;
	unsigned long offset = tinfo->curr.offset;
	struct ahc_syncrate *syncrate;

	if (offset == 0)
		offset = MAX_OFFSET;

	ahc_compile_devinfo(&devinfo, shost->this_id, starget->id, 0,
			    starget->channel + 'A', ROLE_INITIATOR);

	/* all PPR requests apart from QAS require wide transfers */
	if (ppr_options & ~MSG_EXT_PPR_QAS_REQ) {
		ahc_linux_get_width(starget);
		if (spi_width(starget) == 0)
			ppr_options &= MSG_EXT_PPR_QAS_REQ;
	}

	syncrate = ahc_find_syncrate(ahc, &period, &ppr_options, AHC_SYNCRATE_DT);
	ahc_lock(ahc, &flags);
	ahc_set_syncrate(ahc, &devinfo, syncrate, period, offset,
			 ppr_options, AHC_TRANS_GOAL, FALSE);
	ahc_unlock(ahc, &flags);
}

static void ahc_linux_get_offset(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	spi_offset(starget) = tinfo->curr.offset;
}

static void ahc_linux_set_offset(struct scsi_target *starget, int offset)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	struct ahc_devinfo devinfo;
	unsigned int ppr_options = 0;
	unsigned int period = 0;
	unsigned long flags;
	struct ahc_syncrate *syncrate = NULL;

	ahc_compile_devinfo(&devinfo, shost->this_id, starget->id, 0,
			    starget->channel + 'A', ROLE_INITIATOR);
	if (offset != 0) {
		syncrate = ahc_find_syncrate(ahc, &period, &ppr_options, AHC_SYNCRATE_DT);
		period = tinfo->curr.period;
		ppr_options = tinfo->curr.ppr_options;
	}
	ahc_lock(ahc, &flags);
	ahc_set_syncrate(ahc, &devinfo, syncrate, period, offset,
			 ppr_options, AHC_TRANS_GOAL, FALSE);
	ahc_unlock(ahc, &flags);
}

static void ahc_linux_get_dt(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	spi_dt(starget) = tinfo->curr.ppr_options & MSG_EXT_PPR_DT_REQ;
}

static void ahc_linux_set_dt(struct scsi_target *starget, int dt)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	struct ahc_devinfo devinfo;
	unsigned int ppr_options = tinfo->curr.ppr_options
		& ~MSG_EXT_PPR_DT_REQ;
	unsigned int period = tinfo->curr.period;
	unsigned long flags;
	struct ahc_syncrate *syncrate;

	ahc_compile_devinfo(&devinfo, shost->this_id, starget->id, 0,
			    starget->channel + 'A', ROLE_INITIATOR);
	syncrate = ahc_find_syncrate(ahc, &period, &ppr_options,AHC_SYNCRATE_DT);
	ahc_lock(ahc, &flags);
	ahc_set_syncrate(ahc, &devinfo, syncrate, period, tinfo->curr.offset,
			 ppr_options, AHC_TRANS_GOAL, FALSE);
	ahc_unlock(ahc, &flags);
}

static void ahc_linux_get_qas(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	spi_dt(starget) = tinfo->curr.ppr_options & MSG_EXT_PPR_QAS_REQ;
}

static void ahc_linux_set_qas(struct scsi_target *starget, int qas)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	struct ahc_devinfo devinfo;
	unsigned int ppr_options = tinfo->curr.ppr_options
		& ~MSG_EXT_PPR_QAS_REQ;
	unsigned int period = tinfo->curr.period;
	unsigned long flags;
	struct ahc_syncrate *syncrate;

	if (qas)
		ppr_options |= MSG_EXT_PPR_QAS_REQ;

	ahc_compile_devinfo(&devinfo, shost->this_id, starget->id, 0,
			    starget->channel + 'A', ROLE_INITIATOR);
	syncrate = ahc_find_syncrate(ahc, &period, &ppr_options, AHC_SYNCRATE_DT);
	ahc_lock(ahc, &flags);
	ahc_set_syncrate(ahc, &devinfo, syncrate, period, tinfo->curr.offset,
			 ppr_options, AHC_TRANS_GOAL, FALSE);
	ahc_unlock(ahc, &flags);
}

static void ahc_linux_get_iu(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	spi_dt(starget) = tinfo->curr.ppr_options & MSG_EXT_PPR_IU_REQ;
}

static void ahc_linux_set_iu(struct scsi_target *starget, int iu)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ahc_softc *ahc = *((struct ahc_softc **)shost->hostdata);
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo 
		= ahc_fetch_transinfo(ahc,
				      starget->channel + 'A',
				      shost->this_id, starget->id, &tstate);
	struct ahc_devinfo devinfo;
	unsigned int ppr_options = tinfo->curr.ppr_options
		& ~MSG_EXT_PPR_IU_REQ;
	unsigned int period = tinfo->curr.period;
	unsigned long flags;
	struct ahc_syncrate *syncrate;

	if (iu)
		ppr_options |= MSG_EXT_PPR_IU_REQ;

	ahc_compile_devinfo(&devinfo, shost->this_id, starget->id, 0,
			    starget->channel + 'A', ROLE_INITIATOR);
	syncrate = ahc_find_syncrate(ahc, &period, &ppr_options, AHC_SYNCRATE_DT);
	ahc_lock(ahc, &flags);
	ahc_set_syncrate(ahc, &devinfo, syncrate, period, tinfo->curr.offset,
			 ppr_options, AHC_TRANS_GOAL, FALSE);
	ahc_unlock(ahc, &flags);
}

static struct spi_function_template ahc_linux_transport_functions = {
	.get_offset	= ahc_linux_get_offset,
	.set_offset	= ahc_linux_set_offset,
	.show_offset	= 1,
	.get_period	= ahc_linux_get_period,
	.set_period	= ahc_linux_set_period,
	.show_period	= 1,
	.get_width	= ahc_linux_get_width,
	.set_width	= ahc_linux_set_width,
	.show_width	= 1,
	.get_dt		= ahc_linux_get_dt,
	.set_dt		= ahc_linux_set_dt,
	.show_dt	= 1,
	.get_iu		= ahc_linux_get_iu,
	.set_iu		= ahc_linux_set_iu,
	.show_iu	= 1,
	.get_qas	= ahc_linux_get_qas,
	.set_qas	= ahc_linux_set_qas,
	.show_qas	= 1,
};



static int __init
ahc_linux_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	ahc_linux_transport_template = spi_attach_transport(&ahc_linux_transport_functions);
	if (!ahc_linux_transport_template)
		return -ENODEV;
	if (ahc_linux_detect(&aic7xxx_driver_template))
		return 0;
	spi_release_transport(ahc_linux_transport_template);
	ahc_linux_exit();
	return -ENODEV;
#else
	scsi_register_module(MODULE_SCSI_HA, &aic7xxx_driver_template);
	if (aic7xxx_driver_template.present == 0) {
		scsi_unregister_module(MODULE_SCSI_HA,
				       &aic7xxx_driver_template);
		return (-ENODEV);
	}

	return (0);
#endif
}

static void
ahc_linux_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/*
	 * In 2.4 we have to unregister from the PCI core _after_
	 * unregistering from the scsi midlayer to avoid dangling
	 * references.
	 */
	scsi_unregister_module(MODULE_SCSI_HA, &aic7xxx_driver_template);
#endif
	ahc_linux_pci_exit();
	ahc_linux_eisa_exit();
	spi_release_transport(ahc_linux_transport_template);
}

module_init(ahc_linux_init);
module_exit(ahc_linux_exit);
