/*
    comedi/drivers/ni_labpc.c
    Driver for National Instruments Lab-PC series boards and compatibles
    Copyright (C) 2001, 2002, 2003 Frank Mori Hess <fmhess@users.sourceforge.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

************************************************************************
*/
/*
Driver: ni_labpc
Description: National Instruments Lab-PC (& compatibles)
Author: Frank Mori Hess <fmhess@users.sourceforge.net>
Devices: [National Instruments] Lab-PC-1200 (labpc-1200),
  Lab-PC-1200AI (labpc-1200ai), Lab-PC+ (lab-pc+), PCI-1200 (ni_labpc)
Status: works

Tested with lab-pc-1200.  For the older Lab-PC+, not all input ranges
and analog references will work, the available ranges/arefs will
depend on how you have configured the jumpers on your board
(see your owner's manual).

Kernel-level ISA plug-and-play support for the lab-pc-1200
boards has not
yet been added to the driver, mainly due to the fact that
I don't know the device id numbers.  If you have one
of these boards,
please file a bug report at http://comedi.org/ 
so I can get the necessary information from you.

The 1200 series boards have onboard calibration dacs for correcting
analog input/output offsets and gains.  The proper settings for these
caldacs are stored on the board's eeprom.  To read the caldac values
from the eeprom and store them into a file that can be then be used by
comedilib, use the comedi_calibrate program.

Configuration options - ISA boards:
  [0] - I/O port base address
  [1] - IRQ (optional, required for timed or externally triggered conversions)
  [2] - DMA channel (optional)

Configuration options - PCI boards:
  [0] - bus (optional)
  [1] - slot (optional)

The Lab-pc+ has quirky chanlist requirements
when scanning multiple channels.  Multiple channel scan
sequence must start at highest channel, then decrement down to
channel 0.  The rest of the cards can scan down like lab-pc+ or scan
up from channel zero.  Chanlists consisting of all one channel
are also legal, and allow you to pace conversions in bursts.

*/

/*

NI manuals:
341309a (labpc-1200 register manual)
340914a (pci-1200)
320502b (lab-pc+)

*/

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>

#include "../comedidev.h"

#include <asm/dma.h>

#include "8253.h"
#include "8255.h"
#include "mite.h"
#include "comedi_fc.h"
#include "ni_labpc.h"

#define DRV_NAME "ni_labpc"

/* size of io region used by board */
#define LABPC_SIZE           32
/* 2 MHz master clock */
#define LABPC_TIMER_BASE            500

/* Registers for the lab-pc+ */

/* write-only registers */
#define COMMAND1_REG	0x0
#define   ADC_GAIN_MASK	(0x7 << 4)
#define   ADC_CHAN_BITS(x)	((x) & 0x7)
/* enables multi channel scans */
#define   ADC_SCAN_EN_BIT	0x80
#define COMMAND2_REG	0x1
/* enable pretriggering (used in conjunction with SWTRIG) */
#define   PRETRIG_BIT	0x1
/* enable paced conversions on external trigger */
#define   HWTRIG_BIT	0x2
/* enable paced conversions */
#define   SWTRIG_BIT	0x4
/* use two cascaded counters for pacing */
#define   CASCADE_BIT	0x8
#define   DAC_PACED_BIT(channel)	(0x40 << ((channel) & 0x1))
#define COMMAND3_REG	0x2
/* enable dma transfers */
#define   DMA_EN_BIT	0x1
/* enable interrupts for 8255 */
#define   DIO_INTR_EN_BIT	0x2
/* enable dma terminal count interrupt */
#define   DMATC_INTR_EN_BIT	0x4
/* enable timer interrupt */
#define   TIMER_INTR_EN_BIT	0x8
/* enable error interrupt */
#define   ERR_INTR_EN_BIT	0x10
/* enable fifo not empty interrupt */
#define   ADC_FNE_INTR_EN_BIT	0x20
#define ADC_CONVERT_REG	0x3
#define DAC_LSB_REG(channel)	(0x4 + 2 * ((channel) & 0x1))
#define DAC_MSB_REG(channel)	(0x5 + 2 * ((channel) & 0x1))
#define ADC_CLEAR_REG	0x8
#define DMATC_CLEAR_REG	0xa
#define TIMER_CLEAR_REG	0xc
/* 1200 boards only */
#define COMMAND6_REG	0xe
/* select ground or common-mode reference */
#define   ADC_COMMON_BIT	0x1
/*  adc unipolar */
#define   ADC_UNIP_BIT	0x2
/*  dac unipolar */
#define   DAC_UNIP_BIT(channel)	(0x4 << ((channel) & 0x1))
/* enable fifo half full interrupt */
#define   ADC_FHF_INTR_EN_BIT	0x20
/* enable interrupt on end of hardware count */
#define   A1_INTR_EN_BIT	0x40
/* scan up from channel zero instead of down to zero */
#define   ADC_SCAN_UP_BIT 0x80
#define COMMAND4_REG	0xf
/* enables 'interval' scanning */
#define   INTERVAL_SCAN_EN_BIT	0x1
/* enables external signal on counter b1 output to trigger scan */
#define   EXT_SCAN_EN_BIT	0x2
/* chooses direction (output or input) for EXTCONV* line */
#define   EXT_CONVERT_OUT_BIT	0x4
/* chooses differential inputs for adc (in conjunction with board jumper) */
#define   ADC_DIFF_BIT	0x8
#define   EXT_CONVERT_DISABLE_BIT	0x10
/* 1200 boards only, calibration stuff */
#define COMMAND5_REG	0x1c
/* enable eeprom for write */
#define   EEPROM_WRITE_UNPROTECT_BIT	0x4
/* enable dithering */
#define   DITHER_EN_BIT	0x8
/* load calibration dac */
#define   CALDAC_LOAD_BIT	0x10
/* serial clock - rising edge writes, falling edge reads */
#define   SCLOCK_BIT	0x20
/* serial data bit for writing to eeprom or calibration dacs */
#define   SDATA_BIT	0x40
/* enable eeprom for read/write */
#define   EEPROM_EN_BIT	0x80
#define INTERVAL_COUNT_REG	0x1e
#define INTERVAL_LOAD_REG	0x1f
#define   INTERVAL_LOAD_BITS	0x1

/* read-only registers */
#define STATUS1_REG	0x0
/* data is available in fifo */
#define   DATA_AVAIL_BIT	0x1
/* overrun has occurred */
#define   OVERRUN_BIT	0x2
/* fifo overflow */
#define   OVERFLOW_BIT	0x4
/* timer interrupt has occurred */
#define   TIMER_BIT	0x8
/* dma terminal count has occurred */
#define   DMATC_BIT	0x10
/* external trigger has occurred */
#define   EXT_TRIG_BIT	0x40
/* 1200 boards only */
#define STATUS2_REG	0x1d
/* programmable eeprom serial output */
#define   EEPROM_OUT_BIT	0x1
/* counter A1 terminal count */
#define   A1_TC_BIT	0x2
/* fifo not half full */
#define   FNHF_BIT	0x4
#define ADC_FIFO_REG	0xa

#define DIO_BASE_REG	0x10
#define COUNTER_A_BASE_REG	0x14
#define COUNTER_A_CONTROL_REG	(COUNTER_A_BASE_REG + 0x3)
/* check modes put conversion pacer output in harmless state (a0 mode 2) */
#define   INIT_A0_BITS	0x14
/* put hardware conversion counter output in harmless state (a1 mode 0) */
#define   INIT_A1_BITS	0x70
#define COUNTER_B_BASE_REG	0x18

enum scan_mode {
	MODE_SINGLE_CHAN,
	MODE_SINGLE_CHAN_INTERVAL,
	MODE_MULT_CHAN_UP,
	MODE_MULT_CHAN_DOWN,
};

static const int labpc_plus_ai_gain_bits[] = {
	0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
};

static const struct comedi_lrange range_labpc_plus_ai = {
	16, {
		BIP_RANGE(5),
		BIP_RANGE(4),
		BIP_RANGE(2.5),
		BIP_RANGE(1),
		BIP_RANGE(0.5),
		BIP_RANGE(0.25),
		BIP_RANGE(0.1),
		BIP_RANGE(0.05),
		UNI_RANGE(10),
		UNI_RANGE(8),
		UNI_RANGE(5),
		UNI_RANGE(2),
		UNI_RANGE(1),
		UNI_RANGE(0.5),
		UNI_RANGE(0.2),
		UNI_RANGE(0.1)
	}
};

const int labpc_1200_ai_gain_bits[] = {
	0x00, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x00, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
};
EXPORT_SYMBOL_GPL(labpc_1200_ai_gain_bits);

const struct comedi_lrange range_labpc_1200_ai = {
	14, {
		BIP_RANGE(5),
		BIP_RANGE(2.5),
		BIP_RANGE(1),
		BIP_RANGE(0.5),
		BIP_RANGE(0.25),
		BIP_RANGE(0.1),
		BIP_RANGE(0.05),
		UNI_RANGE(10),
		UNI_RANGE(5),
		UNI_RANGE(2),
		UNI_RANGE(1),
		UNI_RANGE(0.5),
		UNI_RANGE(0.2),
		UNI_RANGE(0.1)
	}
};
EXPORT_SYMBOL_GPL(range_labpc_1200_ai);

static const struct comedi_lrange range_labpc_ao = {
	2, {
		BIP_RANGE(5),
		UNI_RANGE(10)
	}
};

/* functions that do inb/outb and readb/writeb so we can use
 * function pointers to decide which to use */
static inline unsigned int labpc_inb(unsigned long address)
{
	return inb(address);
}

static inline void labpc_outb(unsigned int byte, unsigned long address)
{
	outb(byte, address);
}

static inline unsigned int labpc_readb(unsigned long address)
{
	return readb((void __iomem *)address);
}

static inline void labpc_writeb(unsigned int byte, unsigned long address)
{
	writeb(byte, (void __iomem *)address);
}

static const struct labpc_boardinfo labpc_boards[] = {
	{
		.name			= "lab-pc-1200",
		.ai_speed		= 10000,
		.bustype		= isa_bustype,
		.register_layout	= labpc_1200_layout,
		.has_ao			= 1,
		.ai_range_table		= &range_labpc_1200_ai,
		.ai_range_code		= labpc_1200_ai_gain_bits,
		.ai_scan_up		= 1,
	}, {
		.name			= "lab-pc-1200ai",
		.ai_speed		= 10000,
		.bustype		= isa_bustype,
		.register_layout	= labpc_1200_layout,
		.ai_range_table		= &range_labpc_1200_ai,
		.ai_range_code		= labpc_1200_ai_gain_bits,
		.ai_scan_up		= 1,
	}, {
		.name			= "lab-pc+",
		.ai_speed		= 12000,
		.bustype		= isa_bustype,
		.register_layout	= labpc_plus_layout,
		.has_ao			= 1,
		.ai_range_table		= &range_labpc_plus_ai,
		.ai_range_code		= labpc_plus_ai_gain_bits,
	},
#ifdef CONFIG_COMEDI_PCI_DRIVERS
	{
		.name			= "pci-1200",
		.device_id		= 0x161,
		.ai_speed		= 10000,
		.bustype		= pci_bustype,
		.register_layout	= labpc_1200_layout,
		.has_ao			= 1,
		.ai_range_table		= &range_labpc_1200_ai,
		.ai_range_code		= labpc_1200_ai_gain_bits,
		.ai_scan_up		= 1,
		.has_mmio		= 1,
	},
#endif
};

/* size in bytes of dma buffer */
static const int dma_buffer_size = 0xff00;
/* 2 bytes per sample */
static const int sample_size = 2;

static bool labpc_range_is_unipolar(struct comedi_subdevice *s,
				    unsigned int range)
{
	const struct comedi_lrange *lrange = s->range_table;
	const struct comedi_krange *krange = &lrange->range[range];

	if (krange->min < 0)
		return false;
	else
		return true;
}

static void labpc_clear_adc_fifo(const struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;

	devpriv->write_byte(0x1, dev->iobase + ADC_CLEAR_REG);
	devpriv->read_byte(dev->iobase + ADC_FIFO_REG);
	devpriv->read_byte(dev->iobase + ADC_FIFO_REG);
}

static int labpc_ai_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn,
			      unsigned int *data)
{
	const struct labpc_boardinfo *board = comedi_board(dev);
	struct labpc_private *devpriv = dev->private;
	int i, n;
	int chan, range;
	int lsb, msb;
	int timeout = 1000;
	unsigned long flags;

	/*  disable timed conversions */
	spin_lock_irqsave(&dev->spinlock, flags);
	devpriv->cmd2 &= ~SWTRIG_BIT & ~HWTRIG_BIT & ~PRETRIG_BIT;
	devpriv->write_byte(devpriv->cmd2, dev->iobase + COMMAND2_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	/*  disable interrupt generation and dma */
	devpriv->cmd3 = 0;
	devpriv->write_byte(devpriv->cmd3, dev->iobase + COMMAND3_REG);

	/* set gain and channel */
	devpriv->cmd1 = 0;
	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);
	devpriv->cmd1 |= board->ai_range_code[range];
	/* munge channel bits for differential/scan disabled mode */
	if (CR_AREF(insn->chanspec) == AREF_DIFF)
		chan *= 2;
	devpriv->cmd1 |= ADC_CHAN_BITS(chan);
	devpriv->write_byte(devpriv->cmd1, dev->iobase + COMMAND1_REG);

	/* setup cmd6 register for 1200 boards */
	if (board->register_layout == labpc_1200_layout) {
		/*  reference inputs to ground or common? */
		if (CR_AREF(insn->chanspec) != AREF_GROUND)
			devpriv->cmd6 |= ADC_COMMON_BIT;
		else
			devpriv->cmd6 &= ~ADC_COMMON_BIT;
		/* bipolar or unipolar range? */
		if (labpc_range_is_unipolar(s, range))
			devpriv->cmd6 |= ADC_UNIP_BIT;
		else
			devpriv->cmd6 &= ~ADC_UNIP_BIT;
		/* don't interrupt on fifo half full */
		devpriv->cmd6 &= ~ADC_FHF_INTR_EN_BIT;
		/* don't enable interrupt on counter a1 terminal count? */
		devpriv->cmd6 &= ~A1_INTR_EN_BIT;
		/* write to register */
		devpriv->write_byte(devpriv->cmd6, dev->iobase + COMMAND6_REG);
	}
	/* setup cmd4 register */
	devpriv->cmd4 = 0;
	devpriv->cmd4 |= EXT_CONVERT_DISABLE_BIT;
	/* single-ended/differential */
	if (CR_AREF(insn->chanspec) == AREF_DIFF)
		devpriv->cmd4 |= ADC_DIFF_BIT;
	devpriv->write_byte(devpriv->cmd4, dev->iobase + COMMAND4_REG);

	/*
	 * initialize pacer counter output to make sure it doesn't
	 * cause any problems
	 */
	devpriv->write_byte(INIT_A0_BITS, dev->iobase + COUNTER_A_CONTROL_REG);

	labpc_clear_adc_fifo(dev);

	for (n = 0; n < insn->n; n++) {
		/* trigger conversion */
		devpriv->write_byte(0x1, dev->iobase + ADC_CONVERT_REG);

		for (i = 0; i < timeout; i++) {
			if (devpriv->read_byte(dev->iobase +
					       STATUS1_REG) & DATA_AVAIL_BIT)
				break;
			udelay(1);
		}
		if (i == timeout) {
			comedi_error(dev, "timeout");
			return -ETIME;
		}
		lsb = devpriv->read_byte(dev->iobase + ADC_FIFO_REG);
		msb = devpriv->read_byte(dev->iobase + ADC_FIFO_REG);
		data[n] = (msb << 8) | lsb;
	}

	return n;
}

#ifdef CONFIG_ISA_DMA_API
/* utility function that suggests a dma transfer size in bytes */
static unsigned int labpc_suggest_transfer_size(const struct comedi_cmd *cmd)
{
	unsigned int size;
	unsigned int freq;

	if (cmd->convert_src == TRIG_TIMER)
		freq = 1000000000 / cmd->convert_arg;
	/* return some default value */
	else
		freq = 0xffffffff;

	/* make buffer fill in no more than 1/3 second */
	size = (freq / 3) * sample_size;

	/* set a minimum and maximum size allowed */
	if (size > dma_buffer_size)
		size = dma_buffer_size - dma_buffer_size % sample_size;
	else if (size < sample_size)
		size = sample_size;

	return size;
}
#endif

static int labpc_use_continuous_mode(const struct comedi_cmd *cmd,
				     enum scan_mode mode)
{
	if (mode == MODE_SINGLE_CHAN)
		return 1;

	if (cmd->scan_begin_src == TRIG_FOLLOW)
		return 1;

	return 0;
}

static unsigned int labpc_ai_convert_period(const struct comedi_cmd *cmd,
					    enum scan_mode mode)
{
	if (cmd->convert_src != TRIG_TIMER)
		return 0;

	if (mode == MODE_SINGLE_CHAN && cmd->scan_begin_src == TRIG_TIMER)
		return cmd->scan_begin_arg;

	return cmd->convert_arg;
}

static void labpc_set_ai_convert_period(struct comedi_cmd *cmd,
					enum scan_mode mode, unsigned int ns)
{
	if (cmd->convert_src != TRIG_TIMER)
		return;

	if (mode == MODE_SINGLE_CHAN &&
	    cmd->scan_begin_src == TRIG_TIMER) {
		cmd->scan_begin_arg = ns;
		if (cmd->convert_arg > cmd->scan_begin_arg)
			cmd->convert_arg = cmd->scan_begin_arg;
	} else
		cmd->convert_arg = ns;
}

static unsigned int labpc_ai_scan_period(const struct comedi_cmd *cmd,
					enum scan_mode mode)
{
	if (cmd->scan_begin_src != TRIG_TIMER)
		return 0;

	if (mode == MODE_SINGLE_CHAN && cmd->convert_src == TRIG_TIMER)
		return 0;

	return cmd->scan_begin_arg;
}

static void labpc_set_ai_scan_period(struct comedi_cmd *cmd,
				     enum scan_mode mode, unsigned int ns)
{
	if (cmd->scan_begin_src != TRIG_TIMER)
		return;

	if (mode == MODE_SINGLE_CHAN && cmd->convert_src == TRIG_TIMER)
		return;

	cmd->scan_begin_arg = ns;
}

/* figures out what counter values to use based on command */
static void labpc_adc_timing(struct comedi_device *dev, struct comedi_cmd *cmd,
			     enum scan_mode mode)
{
	struct labpc_private *devpriv = dev->private;
	/* max value for 16 bit counter in mode 2 */
	const int max_counter_value = 0x10000;
	/* min value for 16 bit counter in mode 2 */
	const int min_counter_value = 2;
	unsigned int base_period;
	unsigned int scan_period;
	unsigned int convert_period;

	/*
	 * if both convert and scan triggers are TRIG_TIMER, then they
	 * both rely on counter b0
	 */
	convert_period = labpc_ai_convert_period(cmd, mode);
	scan_period = labpc_ai_scan_period(cmd, mode);
	if (convert_period && scan_period) {
		/*
		 * pick the lowest b0 divisor value we can (for maximum input
		 * clock speed on convert and scan counters)
		 */
		devpriv->divisor_b0 = (scan_period - 1) /
		    (LABPC_TIMER_BASE * max_counter_value) + 1;
		if (devpriv->divisor_b0 < min_counter_value)
			devpriv->divisor_b0 = min_counter_value;
		if (devpriv->divisor_b0 > max_counter_value)
			devpriv->divisor_b0 = max_counter_value;

		base_period = LABPC_TIMER_BASE * devpriv->divisor_b0;

		/*  set a0 for conversion frequency and b1 for scan frequency */
		switch (cmd->flags & TRIG_ROUND_MASK) {
		default:
		case TRIG_ROUND_NEAREST:
			devpriv->divisor_a0 =
			    (convert_period + (base_period / 2)) / base_period;
			devpriv->divisor_b1 =
			    (scan_period + (base_period / 2)) / base_period;
			break;
		case TRIG_ROUND_UP:
			devpriv->divisor_a0 =
			    (convert_period + (base_period - 1)) / base_period;
			devpriv->divisor_b1 =
			    (scan_period + (base_period - 1)) / base_period;
			break;
		case TRIG_ROUND_DOWN:
			devpriv->divisor_a0 = convert_period / base_period;
			devpriv->divisor_b1 = scan_period / base_period;
			break;
		}
		/*  make sure a0 and b1 values are acceptable */
		if (devpriv->divisor_a0 < min_counter_value)
			devpriv->divisor_a0 = min_counter_value;
		if (devpriv->divisor_a0 > max_counter_value)
			devpriv->divisor_a0 = max_counter_value;
		if (devpriv->divisor_b1 < min_counter_value)
			devpriv->divisor_b1 = min_counter_value;
		if (devpriv->divisor_b1 > max_counter_value)
			devpriv->divisor_b1 = max_counter_value;
		/*  write corrected timings to command */
		labpc_set_ai_convert_period(cmd, mode,
					    base_period * devpriv->divisor_a0);
		labpc_set_ai_scan_period(cmd, mode,
					 base_period * devpriv->divisor_b1);
		/*
		 * if only one TRIG_TIMER is used, we can employ the generic
		 * cascaded timing functions
		 */
	} else if (scan_period) {
		/*
		 * calculate cascaded counter values
		 * that give desired scan timing
		 */
		i8253_cascade_ns_to_timer_2div(LABPC_TIMER_BASE,
					       &(devpriv->divisor_b1),
					       &(devpriv->divisor_b0),
					       &scan_period,
					       cmd->flags & TRIG_ROUND_MASK);
		labpc_set_ai_scan_period(cmd, mode, scan_period);
	} else if (convert_period) {
		/*
		 * calculate cascaded counter values
		 * that give desired conversion timing
		 */
		i8253_cascade_ns_to_timer_2div(LABPC_TIMER_BASE,
					       &(devpriv->divisor_a0),
					       &(devpriv->divisor_b0),
					       &convert_period,
					       cmd->flags & TRIG_ROUND_MASK);
		labpc_set_ai_convert_period(cmd, mode, convert_period);
	}
}

static enum scan_mode labpc_ai_scan_mode(const struct comedi_cmd *cmd)
{
	if (cmd->chanlist_len == 1)
		return MODE_SINGLE_CHAN;

	/* chanlist may be NULL during cmdtest. */
	if (cmd->chanlist == NULL)
		return MODE_MULT_CHAN_UP;

	if (CR_CHAN(cmd->chanlist[0]) == CR_CHAN(cmd->chanlist[1]))
		return MODE_SINGLE_CHAN_INTERVAL;

	if (CR_CHAN(cmd->chanlist[0]) < CR_CHAN(cmd->chanlist[1]))
		return MODE_MULT_CHAN_UP;

	if (CR_CHAN(cmd->chanlist[0]) > CR_CHAN(cmd->chanlist[1]))
		return MODE_MULT_CHAN_DOWN;

	pr_err("ni_labpc: bug! cannot determine AI scan mode\n");
	return 0;
}

static int labpc_ai_chanlist_invalid(const struct comedi_device *dev,
				     const struct comedi_cmd *cmd,
				     enum scan_mode mode)
{
	int channel, range, aref, i;

	if (cmd->chanlist == NULL)
		return 0;

	if (mode == MODE_SINGLE_CHAN)
		return 0;

	if (mode == MODE_SINGLE_CHAN_INTERVAL) {
		if (cmd->chanlist_len > 0xff) {
			comedi_error(dev,
				     "ni_labpc: chanlist too long for single channel interval mode\n");
			return 1;
		}
	}

	channel = CR_CHAN(cmd->chanlist[0]);
	range = CR_RANGE(cmd->chanlist[0]);
	aref = CR_AREF(cmd->chanlist[0]);

	for (i = 0; i < cmd->chanlist_len; i++) {

		switch (mode) {
		case MODE_SINGLE_CHAN_INTERVAL:
			if (CR_CHAN(cmd->chanlist[i]) != channel) {
				comedi_error(dev,
					     "channel scanning order specified in chanlist is not supported by hardware.\n");
				return 1;
			}
			break;
		case MODE_MULT_CHAN_UP:
			if (CR_CHAN(cmd->chanlist[i]) != i) {
				comedi_error(dev,
					     "channel scanning order specified in chanlist is not supported by hardware.\n");
				return 1;
			}
			break;
		case MODE_MULT_CHAN_DOWN:
			if (CR_CHAN(cmd->chanlist[i]) !=
			    cmd->chanlist_len - i - 1) {
				comedi_error(dev,
					     "channel scanning order specified in chanlist is not supported by hardware.\n");
				return 1;
			}
			break;
		default:
			dev_err(dev->class_dev,
				"ni_labpc: bug! in chanlist check\n");
			return 1;
			break;
		}

		if (CR_RANGE(cmd->chanlist[i]) != range) {
			comedi_error(dev,
				     "entries in chanlist must all have the same range\n");
			return 1;
		}

		if (CR_AREF(cmd->chanlist[i]) != aref) {
			comedi_error(dev,
				     "entries in chanlist must all have the same reference\n");
			return 1;
		}
	}

	return 0;
}

static int labpc_ai_cmdtest(struct comedi_device *dev,
			    struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	const struct labpc_boardinfo *board = comedi_board(dev);
	int err = 0;
	int tmp, tmp2;
	unsigned int stop_mask;
	enum scan_mode mode;

	/* Step 1 : check if triggers are trivially valid */

	err |= cfc_check_trigger_src(&cmd->start_src, TRIG_NOW | TRIG_EXT);
	err |= cfc_check_trigger_src(&cmd->scan_begin_src,
					TRIG_TIMER | TRIG_FOLLOW | TRIG_EXT);
	err |= cfc_check_trigger_src(&cmd->convert_src, TRIG_TIMER | TRIG_EXT);
	err |= cfc_check_trigger_src(&cmd->scan_end_src, TRIG_COUNT);

	stop_mask = TRIG_COUNT | TRIG_NONE;
	if (board->register_layout == labpc_1200_layout)
		stop_mask |= TRIG_EXT;
	err |= cfc_check_trigger_src(&cmd->stop_src, stop_mask);

	if (err)
		return 1;

	/* Step 2a : make sure trigger sources are unique */

	err |= cfc_check_trigger_is_unique(cmd->start_src);
	err |= cfc_check_trigger_is_unique(cmd->scan_begin_src);
	err |= cfc_check_trigger_is_unique(cmd->convert_src);
	err |= cfc_check_trigger_is_unique(cmd->stop_src);

	/* Step 2b : and mutually compatible */

	/* can't have external stop and start triggers at once */
	if (cmd->start_src == TRIG_EXT && cmd->stop_src == TRIG_EXT)
		err++;

	if (err)
		return 2;

	/* Step 3: check if arguments are trivially valid */

	if (cmd->start_arg == TRIG_NOW)
		err |= cfc_check_trigger_arg_is(&cmd->start_arg, 0);

	if (!cmd->chanlist_len)
		err |= -EINVAL;
	err |= cfc_check_trigger_arg_is(&cmd->scan_end_arg, cmd->chanlist_len);

	if (cmd->convert_src == TRIG_TIMER)
		err |= cfc_check_trigger_arg_min(&cmd->convert_arg,
						 board->ai_speed);

	/* make sure scan timing is not too fast */
	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->convert_src == TRIG_TIMER)
			err |= cfc_check_trigger_arg_min(&cmd->scan_begin_arg,
					cmd->convert_arg * cmd->chanlist_len);
		err |= cfc_check_trigger_arg_min(&cmd->scan_begin_arg,
				board->ai_speed * cmd->chanlist_len);
	}

	switch (cmd->stop_src) {
	case TRIG_COUNT:
		err |= cfc_check_trigger_arg_min(&cmd->stop_arg, 1);
		break;
	case TRIG_NONE:
		err |= cfc_check_trigger_arg_is(&cmd->stop_arg, 0);
		break;
		/*
		 * TRIG_EXT doesn't care since it doesn't
		 * trigger off a numbered channel
		 */
	default:
		break;
	}

	if (err)
		return 3;

	/* step 4: fix up any arguments */

	tmp = cmd->convert_arg;
	tmp2 = cmd->scan_begin_arg;
	mode = labpc_ai_scan_mode(cmd);
	labpc_adc_timing(dev, cmd, mode);
	if (tmp != cmd->convert_arg || tmp2 != cmd->scan_begin_arg)
		err++;

	if (err)
		return 4;

	if (labpc_ai_chanlist_invalid(dev, cmd, mode))
		return 5;

	return 0;
}

static inline int labpc_counter_load(struct comedi_device *dev,
				     unsigned long base_address,
				     unsigned int counter_number,
				     unsigned int count, unsigned int mode)
{
	const struct labpc_boardinfo *board = comedi_board(dev);

	if (board->has_mmio)
		return i8254_mm_load((void __iomem *)base_address, 0,
				     counter_number, count, mode);
	else
		return i8254_load(base_address, 0, counter_number, count, mode);
}

static int labpc_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	const struct labpc_boardinfo *board = comedi_board(dev);
	struct labpc_private *devpriv = dev->private;
	int channel, range, aref;
#ifdef CONFIG_ISA_DMA_API
	unsigned long irq_flags;
#endif
	int ret;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	enum transfer_type xfer;
	enum scan_mode mode;
	unsigned long flags;

	if (!dev->irq) {
		comedi_error(dev, "no irq assigned, cannot perform command");
		return -1;
	}

	range = CR_RANGE(cmd->chanlist[0]);
	aref = CR_AREF(cmd->chanlist[0]);

	/* make sure board is disabled before setting up acquisition */
	spin_lock_irqsave(&dev->spinlock, flags);
	devpriv->cmd2 &= ~SWTRIG_BIT & ~HWTRIG_BIT & ~PRETRIG_BIT;
	devpriv->write_byte(devpriv->cmd2, dev->iobase + COMMAND2_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	devpriv->cmd3 = 0;
	devpriv->write_byte(devpriv->cmd3, dev->iobase + COMMAND3_REG);

	/*  initialize software conversion count */
	if (cmd->stop_src == TRIG_COUNT)
		devpriv->count = cmd->stop_arg * cmd->chanlist_len;

	/*  setup hardware conversion counter */
	if (cmd->stop_src == TRIG_EXT) {
		/*
		 * load counter a1 with count of 3
		 * (pc+ manual says this is minimum allowed) using mode 0
		 */
		ret = labpc_counter_load(dev, dev->iobase + COUNTER_A_BASE_REG,
					 1, 3, 0);
		if (ret < 0) {
			comedi_error(dev, "error loading counter a1");
			return -1;
		}
	} else			/*
				 * otherwise, just put a1 in mode 0
				 * with no count to set its output low
				 */
		devpriv->write_byte(INIT_A1_BITS,
				    dev->iobase + COUNTER_A_CONTROL_REG);

#ifdef CONFIG_ISA_DMA_API
	/*  figure out what method we will use to transfer data */
	if (devpriv->dma_chan &&	/*  need a dma channel allocated */
		/*
		 * dma unsafe at RT priority,
		 * and too much setup time for TRIG_WAKE_EOS for
		 */
	    (cmd->flags & (TRIG_WAKE_EOS | TRIG_RT)) == 0 &&
	    /*  only available on the isa boards */
	    board->bustype == isa_bustype) {
		xfer = isa_dma_transfer;
		/* pc-plus has no fifo-half full interrupt */
	} else
#endif
	if (board->register_layout == labpc_1200_layout &&
		   /*  wake-end-of-scan should interrupt on fifo not empty */
		   (cmd->flags & TRIG_WAKE_EOS) == 0 &&
		   /*  make sure we are taking more than just a few points */
		   (cmd->stop_src != TRIG_COUNT || devpriv->count > 256)) {
		xfer = fifo_half_full_transfer;
	} else
		xfer = fifo_not_empty_transfer;
	devpriv->current_transfer = xfer;
	mode = labpc_ai_scan_mode(cmd);

	/*  setup cmd6 register for 1200 boards */
	if (board->register_layout == labpc_1200_layout) {
		/*  reference inputs to ground or common? */
		if (aref != AREF_GROUND)
			devpriv->cmd6 |= ADC_COMMON_BIT;
		else
			devpriv->cmd6 &= ~ADC_COMMON_BIT;
		/*  bipolar or unipolar range? */
		if (labpc_range_is_unipolar(s, range))
			devpriv->cmd6 |= ADC_UNIP_BIT;
		else
			devpriv->cmd6 &= ~ADC_UNIP_BIT;
		/*  interrupt on fifo half full? */
		if (xfer == fifo_half_full_transfer)
			devpriv->cmd6 |= ADC_FHF_INTR_EN_BIT;
		else
			devpriv->cmd6 &= ~ADC_FHF_INTR_EN_BIT;
		/*  enable interrupt on counter a1 terminal count? */
		if (cmd->stop_src == TRIG_EXT)
			devpriv->cmd6 |= A1_INTR_EN_BIT;
		else
			devpriv->cmd6 &= ~A1_INTR_EN_BIT;
		/*  are we scanning up or down through channels? */
		if (mode == MODE_MULT_CHAN_UP)
			devpriv->cmd6 |= ADC_SCAN_UP_BIT;
		else
			devpriv->cmd6 &= ~ADC_SCAN_UP_BIT;
		/*  write to register */
		devpriv->write_byte(devpriv->cmd6, dev->iobase + COMMAND6_REG);
	}

	/* setup channel list, etc (cmd1 register) */
	devpriv->cmd1 = 0;
	if (mode == MODE_MULT_CHAN_UP)
		channel = CR_CHAN(cmd->chanlist[cmd->chanlist_len - 1]);
	else
		channel = CR_CHAN(cmd->chanlist[0]);
	/* munge channel bits for differential / scan disabled mode */
	if ((mode == MODE_SINGLE_CHAN || mode == MODE_SINGLE_CHAN_INTERVAL) &&
	    aref == AREF_DIFF)
		channel *= 2;
	devpriv->cmd1 |= ADC_CHAN_BITS(channel);
	devpriv->cmd1 |= board->ai_range_code[range];
	devpriv->write_byte(devpriv->cmd1, dev->iobase + COMMAND1_REG);
	/* manual says to set scan enable bit on second pass */
	if (mode == MODE_MULT_CHAN_UP || mode == MODE_MULT_CHAN_DOWN) {
		devpriv->cmd1 |= ADC_SCAN_EN_BIT;
		/* need a brief delay before enabling scan, or scan
		 * list will get screwed when you switch
		 * between scan up to scan down mode - dunno why */
		udelay(1);
		devpriv->write_byte(devpriv->cmd1, dev->iobase + COMMAND1_REG);
	}

	devpriv->write_byte(cmd->chanlist_len,
			    dev->iobase + INTERVAL_COUNT_REG);
	/*  load count */
	devpriv->write_byte(INTERVAL_LOAD_BITS,
			    dev->iobase + INTERVAL_LOAD_REG);

	if (cmd->convert_src == TRIG_TIMER || cmd->scan_begin_src == TRIG_TIMER) {
		/*  set up pacing */
		labpc_adc_timing(dev, cmd, mode);
		/*  load counter b0 in mode 3 */
		ret = labpc_counter_load(dev, dev->iobase + COUNTER_B_BASE_REG,
					 0, devpriv->divisor_b0, 3);
		if (ret < 0) {
			comedi_error(dev, "error loading counter b0");
			return -1;
		}
	}
	/*  set up conversion pacing */
	if (labpc_ai_convert_period(cmd, mode)) {
		/*  load counter a0 in mode 2 */
		ret = labpc_counter_load(dev, dev->iobase + COUNTER_A_BASE_REG,
					 0, devpriv->divisor_a0, 2);
		if (ret < 0) {
			comedi_error(dev, "error loading counter a0");
			return -1;
		}
	} else
		devpriv->write_byte(INIT_A0_BITS,
				    dev->iobase + COUNTER_A_CONTROL_REG);

	/*  set up scan pacing */
	if (labpc_ai_scan_period(cmd, mode)) {
		/*  load counter b1 in mode 2 */
		ret = labpc_counter_load(dev, dev->iobase + COUNTER_B_BASE_REG,
					 1, devpriv->divisor_b1, 2);
		if (ret < 0) {
			comedi_error(dev, "error loading counter b1");
			return -1;
		}
	}

	labpc_clear_adc_fifo(dev);

#ifdef CONFIG_ISA_DMA_API
	/*  set up dma transfer */
	if (xfer == isa_dma_transfer) {
		irq_flags = claim_dma_lock();
		disable_dma(devpriv->dma_chan);
		/* clear flip-flop to make sure 2-byte registers for
		 * count and address get set correctly */
		clear_dma_ff(devpriv->dma_chan);
		set_dma_addr(devpriv->dma_chan,
			     virt_to_bus(devpriv->dma_buffer));
		/*  set appropriate size of transfer */
		devpriv->dma_transfer_size = labpc_suggest_transfer_size(cmd);
		if (cmd->stop_src == TRIG_COUNT &&
		    devpriv->count * sample_size < devpriv->dma_transfer_size) {
			devpriv->dma_transfer_size =
			    devpriv->count * sample_size;
		}
		set_dma_count(devpriv->dma_chan, devpriv->dma_transfer_size);
		enable_dma(devpriv->dma_chan);
		release_dma_lock(irq_flags);
		/*  enable board's dma */
		devpriv->cmd3 |= DMA_EN_BIT | DMATC_INTR_EN_BIT;
	} else
		devpriv->cmd3 &= ~DMA_EN_BIT & ~DMATC_INTR_EN_BIT;
#endif

	/*  enable error interrupts */
	devpriv->cmd3 |= ERR_INTR_EN_BIT;
	/*  enable fifo not empty interrupt? */
	if (xfer == fifo_not_empty_transfer)
		devpriv->cmd3 |= ADC_FNE_INTR_EN_BIT;
	else
		devpriv->cmd3 &= ~ADC_FNE_INTR_EN_BIT;
	devpriv->write_byte(devpriv->cmd3, dev->iobase + COMMAND3_REG);

	/*  setup any external triggering/pacing (cmd4 register) */
	devpriv->cmd4 = 0;
	if (cmd->convert_src != TRIG_EXT)
		devpriv->cmd4 |= EXT_CONVERT_DISABLE_BIT;
	/* XXX should discard first scan when using interval scanning
	 * since manual says it is not synced with scan clock */
	if (labpc_use_continuous_mode(cmd, mode) == 0) {
		devpriv->cmd4 |= INTERVAL_SCAN_EN_BIT;
		if (cmd->scan_begin_src == TRIG_EXT)
			devpriv->cmd4 |= EXT_SCAN_EN_BIT;
	}
	/*  single-ended/differential */
	if (aref == AREF_DIFF)
		devpriv->cmd4 |= ADC_DIFF_BIT;
	devpriv->write_byte(devpriv->cmd4, dev->iobase + COMMAND4_REG);

	/*  startup acquisition */

	/*  cmd2 reg */
	/*  use 2 cascaded counters for pacing */
	spin_lock_irqsave(&dev->spinlock, flags);
	devpriv->cmd2 |= CASCADE_BIT;
	switch (cmd->start_src) {
	case TRIG_EXT:
		devpriv->cmd2 |= HWTRIG_BIT;
		devpriv->cmd2 &= ~PRETRIG_BIT & ~SWTRIG_BIT;
		break;
	case TRIG_NOW:
		devpriv->cmd2 |= SWTRIG_BIT;
		devpriv->cmd2 &= ~PRETRIG_BIT & ~HWTRIG_BIT;
		break;
	default:
		comedi_error(dev, "bug with start_src");
		spin_unlock_irqrestore(&dev->spinlock, flags);
		return -1;
		break;
	}
	switch (cmd->stop_src) {
	case TRIG_EXT:
		devpriv->cmd2 |= HWTRIG_BIT | PRETRIG_BIT;
		break;
	case TRIG_COUNT:
	case TRIG_NONE:
		break;
	default:
		comedi_error(dev, "bug with stop_src");
		spin_unlock_irqrestore(&dev->spinlock, flags);
		return -1;
	}
	devpriv->write_byte(devpriv->cmd2, dev->iobase + COMMAND2_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	return 0;
}

static int labpc_cancel(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct labpc_private *devpriv = dev->private;
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);
	devpriv->cmd2 &= ~SWTRIG_BIT & ~HWTRIG_BIT & ~PRETRIG_BIT;
	devpriv->write_byte(devpriv->cmd2, dev->iobase + COMMAND2_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	devpriv->cmd3 = 0;
	devpriv->write_byte(devpriv->cmd3, dev->iobase + COMMAND3_REG);

	return 0;
}

#ifdef CONFIG_ISA_DMA_API
static void labpc_drain_dma(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async = s->async;
	int status;
	unsigned long flags;
	unsigned int max_points, num_points, residue, leftover;
	int i;

	status = devpriv->stat1;

	flags = claim_dma_lock();
	disable_dma(devpriv->dma_chan);
	/* clear flip-flop to make sure 2-byte registers for
	 * count and address get set correctly */
	clear_dma_ff(devpriv->dma_chan);

	/*  figure out how many points to read */
	max_points = devpriv->dma_transfer_size / sample_size;
	/* residue is the number of points left to be done on the dma
	 * transfer.  It should always be zero at this point unless
	 * the stop_src is set to external triggering.
	 */
	residue = get_dma_residue(devpriv->dma_chan) / sample_size;
	num_points = max_points - residue;
	if (devpriv->count < num_points && async->cmd.stop_src == TRIG_COUNT)
		num_points = devpriv->count;

	/*  figure out how many points will be stored next time */
	leftover = 0;
	if (async->cmd.stop_src != TRIG_COUNT) {
		leftover = devpriv->dma_transfer_size / sample_size;
	} else if (devpriv->count > num_points) {
		leftover = devpriv->count - num_points;
		if (leftover > max_points)
			leftover = max_points;
	}

	/* write data to comedi buffer */
	for (i = 0; i < num_points; i++)
		cfc_write_to_buffer(s, devpriv->dma_buffer[i]);

	if (async->cmd.stop_src == TRIG_COUNT)
		devpriv->count -= num_points;

	/*  set address and count for next transfer */
	set_dma_addr(devpriv->dma_chan, virt_to_bus(devpriv->dma_buffer));
	set_dma_count(devpriv->dma_chan, leftover * sample_size);
	release_dma_lock(flags);

	async->events |= COMEDI_CB_BLOCK;
}

static void handle_isa_dma(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;

	labpc_drain_dma(dev);

	enable_dma(devpriv->dma_chan);

	/*  clear dma tc interrupt */
	devpriv->write_byte(0x1, dev->iobase + DMATC_CLEAR_REG);
}
#endif

/* read all available samples from ai fifo */
static int labpc_drain_fifo(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;
	unsigned int lsb, msb;
	short data;
	struct comedi_async *async = dev->read_subdev->async;
	const int timeout = 10000;
	unsigned int i;

	devpriv->stat1 = devpriv->read_byte(dev->iobase + STATUS1_REG);

	for (i = 0; (devpriv->stat1 & DATA_AVAIL_BIT) && i < timeout;
	     i++) {
		/*  quit if we have all the data we want */
		if (async->cmd.stop_src == TRIG_COUNT) {
			if (devpriv->count == 0)
				break;
			devpriv->count--;
		}
		lsb = devpriv->read_byte(dev->iobase + ADC_FIFO_REG);
		msb = devpriv->read_byte(dev->iobase + ADC_FIFO_REG);
		data = (msb << 8) | lsb;
		cfc_write_to_buffer(dev->read_subdev, data);
		devpriv->stat1 = devpriv->read_byte(dev->iobase + STATUS1_REG);
	}
	if (i == timeout) {
		comedi_error(dev, "ai timeout, fifo never empties");
		async->events |= COMEDI_CB_ERROR | COMEDI_CB_EOA;
		return -1;
	}

	return 0;
}

/* makes sure all data acquired by board is transferred to comedi (used
 * when acquisition is terminated by stop_src == TRIG_EXT). */
static void labpc_drain_dregs(struct comedi_device *dev)
{
#ifdef CONFIG_ISA_DMA_API
	struct labpc_private *devpriv = dev->private;

	if (devpriv->current_transfer == isa_dma_transfer)
		labpc_drain_dma(dev);
#endif

	labpc_drain_fifo(dev);
}

/* interrupt service routine */
static irqreturn_t labpc_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	const struct labpc_boardinfo *board = comedi_board(dev);
	struct labpc_private *devpriv = dev->private;
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async;
	struct comedi_cmd *cmd;

	if (!dev->attached) {
		comedi_error(dev, "premature interrupt");
		return IRQ_HANDLED;
	}

	async = s->async;
	cmd = &async->cmd;
	async->events = 0;

	/* read board status */
	devpriv->stat1 = devpriv->read_byte(dev->iobase + STATUS1_REG);
	if (board->register_layout == labpc_1200_layout)
		devpriv->stat2 = devpriv->read_byte(dev->iobase + STATUS2_REG);

	if ((devpriv->stat1 & (DMATC_BIT | TIMER_BIT | OVERFLOW_BIT |
			       OVERRUN_BIT | DATA_AVAIL_BIT)) == 0
	    && (devpriv->stat2 & A1_TC_BIT) == 0
	    && (devpriv->stat2 & FNHF_BIT)) {
		return IRQ_NONE;
	}

	if (devpriv->stat1 & OVERRUN_BIT) {
		/* clear error interrupt */
		devpriv->write_byte(0x1, dev->iobase + ADC_CLEAR_REG);
		async->events |= COMEDI_CB_ERROR | COMEDI_CB_EOA;
		comedi_event(dev, s);
		comedi_error(dev, "overrun");
		return IRQ_HANDLED;
	}

#ifdef CONFIG_ISA_DMA_API
	if (devpriv->current_transfer == isa_dma_transfer) {
		/*
		 * if a dma terminal count of external stop trigger
		 * has occurred
		 */
		if (devpriv->stat1 & DMATC_BIT ||
		    (board->register_layout == labpc_1200_layout
		     && devpriv->stat2 & A1_TC_BIT)) {
			handle_isa_dma(dev);
		}
	} else
#endif
		labpc_drain_fifo(dev);

	if (devpriv->stat1 & TIMER_BIT) {
		comedi_error(dev, "handled timer interrupt?");
		/*  clear it */
		devpriv->write_byte(0x1, dev->iobase + TIMER_CLEAR_REG);
	}

	if (devpriv->stat1 & OVERFLOW_BIT) {
		/*  clear error interrupt */
		devpriv->write_byte(0x1, dev->iobase + ADC_CLEAR_REG);
		async->events |= COMEDI_CB_ERROR | COMEDI_CB_EOA;
		comedi_event(dev, s);
		comedi_error(dev, "overflow");
		return IRQ_HANDLED;
	}
	/*  handle external stop trigger */
	if (cmd->stop_src == TRIG_EXT) {
		if (devpriv->stat2 & A1_TC_BIT) {
			labpc_drain_dregs(dev);
			labpc_cancel(dev, s);
			async->events |= COMEDI_CB_EOA;
		}
	}

	/* TRIG_COUNT end of acquisition */
	if (cmd->stop_src == TRIG_COUNT) {
		if (devpriv->count == 0) {
			labpc_cancel(dev, s);
			async->events |= COMEDI_CB_EOA;
		}
	}

	comedi_event(dev, s);
	return IRQ_HANDLED;
}

static int labpc_ao_insn_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn,
			       unsigned int *data)
{
	const struct labpc_boardinfo *board = comedi_board(dev);
	struct labpc_private *devpriv = dev->private;
	int channel, range;
	unsigned long flags;
	int lsb, msb;

	channel = CR_CHAN(insn->chanspec);

	/* turn off pacing of analog output channel */
	/* note: hardware bug in daqcard-1200 means pacing cannot
	 * be independently enabled/disabled for its the two channels */
	spin_lock_irqsave(&dev->spinlock, flags);
	devpriv->cmd2 &= ~DAC_PACED_BIT(channel);
	devpriv->write_byte(devpriv->cmd2, dev->iobase + COMMAND2_REG);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	/* set range */
	if (board->register_layout == labpc_1200_layout) {
		range = CR_RANGE(insn->chanspec);
		if (labpc_range_is_unipolar(s, range))
			devpriv->cmd6 |= DAC_UNIP_BIT(channel);
		else
			devpriv->cmd6 &= ~DAC_UNIP_BIT(channel);
		/*  write to register */
		devpriv->write_byte(devpriv->cmd6, dev->iobase + COMMAND6_REG);
	}
	/* send data */
	lsb = data[0] & 0xff;
	msb = (data[0] >> 8) & 0xff;
	devpriv->write_byte(lsb, dev->iobase + DAC_LSB_REG(channel));
	devpriv->write_byte(msb, dev->iobase + DAC_MSB_REG(channel));

	/* remember value for readback */
	devpriv->ao_value[channel] = data[0];

	return 1;
}

static int labpc_ao_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn,
			      unsigned int *data)
{
	struct labpc_private *devpriv = dev->private;

	data[0] = devpriv->ao_value[CR_CHAN(insn->chanspec)];

	return 1;
}

static int labpc_8255_mmio(int dir, int port, int data, unsigned long iobase)
{
	if (dir) {
		writeb(data, (void __iomem *)(iobase + port));
		return 0;
	} else {
		return readb((void __iomem *)(iobase + port));
	}
}

/* lowlevel write to eeprom/dac */
static void labpc_serial_out(struct comedi_device *dev, unsigned int value,
			     unsigned int value_width)
{
	struct labpc_private *devpriv = dev->private;
	int i;

	for (i = 1; i <= value_width; i++) {
		/*  clear serial clock */
		devpriv->cmd5 &= ~SCLOCK_BIT;
		/*  send bits most significant bit first */
		if (value & (1 << (value_width - i)))
			devpriv->cmd5 |= SDATA_BIT;
		else
			devpriv->cmd5 &= ~SDATA_BIT;
		udelay(1);
		devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
		/*  set clock to load bit */
		devpriv->cmd5 |= SCLOCK_BIT;
		udelay(1);
		devpriv->write_byte(devpriv->cmd5,
				    dev->iobase + COMMAND5_REG);
	}
}

/* lowlevel read from eeprom */
static unsigned int labpc_serial_in(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;
	unsigned int value = 0;
	int i;
	const int value_width = 8;	/*  number of bits wide values are */

	for (i = 1; i <= value_width; i++) {
		/*  set serial clock */
		devpriv->cmd5 |= SCLOCK_BIT;
		udelay(1);
		devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
		/*  clear clock bit */
		devpriv->cmd5 &= ~SCLOCK_BIT;
		udelay(1);
		devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
		/*  read bits most significant bit first */
		udelay(1);
		devpriv->stat2 = devpriv->read_byte(dev->iobase + STATUS2_REG);
		if (devpriv->stat2 & EEPROM_OUT_BIT)
			value |= 1 << (value_width - i);
	}

	return value;
}

static unsigned int labpc_eeprom_read(struct comedi_device *dev,
				      unsigned int address)
{
	struct labpc_private *devpriv = dev->private;
	unsigned int value;
	/*  bits to tell eeprom to expect a read */
	const int read_instruction = 0x3;
	/*  8 bit write lengths to eeprom */
	const int write_length = 8;

	/*  enable read/write to eeprom */
	devpriv->cmd5 &= ~EEPROM_EN_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
	devpriv->cmd5 |= EEPROM_EN_BIT | EEPROM_WRITE_UNPROTECT_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	/*  send read instruction */
	labpc_serial_out(dev, read_instruction, write_length);
	/*  send 8 bit address to read from */
	labpc_serial_out(dev, address, write_length);
	/*  read result */
	value = labpc_serial_in(dev);

	/*  disable read/write to eeprom */
	devpriv->cmd5 &= ~EEPROM_EN_BIT & ~EEPROM_WRITE_UNPROTECT_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	return value;
}

static unsigned int labpc_eeprom_read_status(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;
	unsigned int value;
	const int read_status_instruction = 0x5;
	const int write_length = 8;	/*  8 bit write lengths to eeprom */

	/*  enable read/write to eeprom */
	devpriv->cmd5 &= ~EEPROM_EN_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
	devpriv->cmd5 |= EEPROM_EN_BIT | EEPROM_WRITE_UNPROTECT_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	/*  send read status instruction */
	labpc_serial_out(dev, read_status_instruction, write_length);
	/*  read result */
	value = labpc_serial_in(dev);

	/*  disable read/write to eeprom */
	devpriv->cmd5 &= ~EEPROM_EN_BIT & ~EEPROM_WRITE_UNPROTECT_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	return value;
}

static int labpc_eeprom_write(struct comedi_device *dev,
				unsigned int address, unsigned int value)
{
	struct labpc_private *devpriv = dev->private;
	const int write_enable_instruction = 0x6;
	const int write_instruction = 0x2;
	const int write_length = 8;	/*  8 bit write lengths to eeprom */
	const int write_in_progress_bit = 0x1;
	const int timeout = 10000;
	int i;

	/*  make sure there isn't already a write in progress */
	for (i = 0; i < timeout; i++) {
		if ((labpc_eeprom_read_status(dev) & write_in_progress_bit) ==
		    0)
			break;
	}
	if (i == timeout) {
		comedi_error(dev, "eeprom write timed out");
		return -ETIME;
	}
	/*  update software copy of eeprom */
	devpriv->eeprom_data[address] = value;

	/*  enable read/write to eeprom */
	devpriv->cmd5 &= ~EEPROM_EN_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
	devpriv->cmd5 |= EEPROM_EN_BIT | EEPROM_WRITE_UNPROTECT_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	/*  send write_enable instruction */
	labpc_serial_out(dev, write_enable_instruction, write_length);
	devpriv->cmd5 &= ~EEPROM_EN_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	/*  send write instruction */
	devpriv->cmd5 |= EEPROM_EN_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
	labpc_serial_out(dev, write_instruction, write_length);
	/*  send 8 bit address to write to */
	labpc_serial_out(dev, address, write_length);
	/*  write value */
	labpc_serial_out(dev, value, write_length);
	devpriv->cmd5 &= ~EEPROM_EN_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	/*  disable read/write to eeprom */
	devpriv->cmd5 &= ~EEPROM_EN_BIT & ~EEPROM_WRITE_UNPROTECT_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	return 0;
}

/* writes to 8 bit calibration dacs */
static void write_caldac(struct comedi_device *dev, unsigned int channel,
			 unsigned int value)
{
	struct labpc_private *devpriv = dev->private;

	if (value == devpriv->caldac[channel])
		return;
	devpriv->caldac[channel] = value;

	/*  clear caldac load bit and make sure we don't write to eeprom */
	devpriv->cmd5 &=
	    ~CALDAC_LOAD_BIT & ~EEPROM_EN_BIT & ~EEPROM_WRITE_UNPROTECT_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);

	/*  write 4 bit channel */
	labpc_serial_out(dev, channel, 4);
	/*  write 8 bit caldac value */
	labpc_serial_out(dev, value, 8);

	/*  set and clear caldac bit to load caldac value */
	devpriv->cmd5 |= CALDAC_LOAD_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
	devpriv->cmd5 &= ~CALDAC_LOAD_BIT;
	udelay(1);
	devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
}

static int labpc_calib_insn_write(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	int channel = CR_CHAN(insn->chanspec);

	write_caldac(dev, channel, data[0]);
	return 1;
}

static int labpc_calib_insn_read(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data)
{
	struct labpc_private *devpriv = dev->private;

	data[0] = devpriv->caldac[CR_CHAN(insn->chanspec)];

	return 1;
}

static int labpc_eeprom_insn_write(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_insn *insn,
				   unsigned int *data)
{
	int channel = CR_CHAN(insn->chanspec);
	int ret;

	/*  only allow writes to user area of eeprom */
	if (channel < 16 || channel > 127) {
		dev_dbg(dev->class_dev,
			"eeprom writes are only allowed to channels 16 through 127 (the pointer and user areas)\n");
		return -EINVAL;
	}

	ret = labpc_eeprom_write(dev, channel, data[0]);
	if (ret < 0)
		return ret;

	return 1;
}

static int labpc_eeprom_insn_read(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	struct labpc_private *devpriv = dev->private;

	data[0] = devpriv->eeprom_data[CR_CHAN(insn->chanspec)];

	return 1;
}

int labpc_common_attach(struct comedi_device *dev, unsigned long iobase,
			unsigned int irq, unsigned int dma_chan)
{
	const struct labpc_boardinfo *board = comedi_board(dev);
	struct labpc_private *devpriv = dev->private;
	struct comedi_subdevice *s;
	int i;
	unsigned long isr_flags;
#ifdef CONFIG_ISA_DMA_API
	unsigned long dma_flags;
#endif
	int ret;

	dev_info(dev->class_dev, "ni_labpc: %s\n", board->name);
	if (iobase == 0) {
		dev_err(dev->class_dev, "io base address is zero!\n");
		return -EINVAL;
	}
	/*  request io regions for isa boards */
	if (board->bustype == isa_bustype) {
		/* check if io addresses are available */
		if (!request_region(iobase, LABPC_SIZE, DRV_NAME)) {
			dev_err(dev->class_dev, "I/O port conflict\n");
			return -EIO;
		}
	}
	dev->iobase = iobase;

	if (board->has_mmio) {
		devpriv->read_byte = labpc_readb;
		devpriv->write_byte = labpc_writeb;
	} else {
		devpriv->read_byte = labpc_inb;
		devpriv->write_byte = labpc_outb;
	}
	/* initialize board's command registers */
	devpriv->write_byte(devpriv->cmd1, dev->iobase + COMMAND1_REG);
	devpriv->write_byte(devpriv->cmd2, dev->iobase + COMMAND2_REG);
	devpriv->write_byte(devpriv->cmd3, dev->iobase + COMMAND3_REG);
	devpriv->write_byte(devpriv->cmd4, dev->iobase + COMMAND4_REG);
	if (board->register_layout == labpc_1200_layout) {
		devpriv->write_byte(devpriv->cmd5, dev->iobase + COMMAND5_REG);
		devpriv->write_byte(devpriv->cmd6, dev->iobase + COMMAND6_REG);
	}

	/* grab our IRQ */
	if (irq) {
		isr_flags = 0;
		if (board->bustype == pci_bustype ||
		    board->bustype == pcmcia_bustype)
			isr_flags |= IRQF_SHARED;
		if (request_irq(irq, labpc_interrupt, isr_flags,
				DRV_NAME, dev)) {
			dev_err(dev->class_dev, "unable to allocate irq %u\n",
				irq);
			return -EINVAL;
		}
	}
	dev->irq = irq;

#ifdef CONFIG_ISA_DMA_API
	/* grab dma channel */
	if (dma_chan > 3) {
		dev_err(dev->class_dev, "invalid dma channel %u\n", dma_chan);
		return -EINVAL;
	} else if (dma_chan) {
		/* allocate dma buffer */
		devpriv->dma_buffer = kmalloc(dma_buffer_size,
					      GFP_KERNEL | GFP_DMA);
		if (devpriv->dma_buffer == NULL)
			return -ENOMEM;

		if (request_dma(dma_chan, DRV_NAME)) {
			dev_err(dev->class_dev,
				"failed to allocate dma channel %u\n",
				dma_chan);
			return -EINVAL;
		}
		devpriv->dma_chan = dma_chan;
		dma_flags = claim_dma_lock();
		disable_dma(devpriv->dma_chan);
		set_dma_mode(devpriv->dma_chan, DMA_MODE_READ);
		release_dma_lock(dma_flags);
	}
#endif

	dev->board_name = board->name;

	ret = comedi_alloc_subdevices(dev, 5);
	if (ret)
		return ret;

	/* analog input subdevice */
	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_AI;
	s->subdev_flags	= SDF_READABLE | SDF_GROUND | SDF_COMMON | SDF_DIFF;
	s->n_chan	= 8;
	s->len_chanlist	= 8;
	s->maxdata	= 0x0fff;
	s->range_table	= board->ai_range_table;
	s->insn_read	= labpc_ai_insn_read;
	if (dev->irq) {
		dev->read_subdev = s;
		s->subdev_flags	|= SDF_CMD_READ;
		s->do_cmd	= labpc_ai_cmd;
		s->do_cmdtest	= labpc_ai_cmdtest;
		s->cancel	= labpc_cancel;
	}

	/* analog output */
	s = &dev->subdevices[1];
	if (board->has_ao) {
		s->type		= COMEDI_SUBD_AO;
		s->subdev_flags	= SDF_READABLE | SDF_WRITABLE | SDF_GROUND;
		s->n_chan	= NUM_AO_CHAN;
		s->maxdata	= 0x0fff;
		s->range_table	= &range_labpc_ao;
		s->insn_read	= labpc_ao_insn_read;
		s->insn_write	= labpc_ao_insn_write;

		/* initialize analog outputs to a known value */
		for (i = 0; i < s->n_chan; i++) {
			short lsb, msb;

			devpriv->ao_value[i] = s->maxdata / 2;
			lsb = devpriv->ao_value[i] & 0xff;
			msb = (devpriv->ao_value[i] >> 8) & 0xff;
			devpriv->write_byte(lsb, dev->iobase + DAC_LSB_REG(i));
			devpriv->write_byte(msb, dev->iobase + DAC_MSB_REG(i));
		}
	} else {
		s->type		= COMEDI_SUBD_UNUSED;
	}

	/* 8255 dio */
	s = &dev->subdevices[2];
	ret = subdev_8255_init(dev, s,
			       (board->has_mmio) ? labpc_8255_mmio : NULL,
			       dev->iobase + DIO_BASE_REG);
	if (ret)
		return ret;

	/*  calibration subdevices for boards that have one */
	s = &dev->subdevices[3];
	if (board->register_layout == labpc_1200_layout) {
		s->type		= COMEDI_SUBD_CALIB;
		s->subdev_flags	= SDF_READABLE | SDF_WRITABLE | SDF_INTERNAL;
		s->n_chan	= 16;
		s->maxdata	= 0xff;
		s->insn_read	= labpc_calib_insn_read;
		s->insn_write	= labpc_calib_insn_write;

		for (i = 0; i < s->n_chan; i++)
			write_caldac(dev, i, s->maxdata / 2);
	} else
		s->type		= COMEDI_SUBD_UNUSED;

	/* EEPROM */
	s = &dev->subdevices[4];
	if (board->register_layout == labpc_1200_layout) {
		s->type		= COMEDI_SUBD_MEMORY;
		s->subdev_flags	= SDF_READABLE | SDF_WRITABLE | SDF_INTERNAL;
		s->n_chan	= EEPROM_SIZE;
		s->maxdata	= 0xff;
		s->insn_read	= labpc_eeprom_insn_read;
		s->insn_write	= labpc_eeprom_insn_write;

		for (i = 0; i < s->n_chan; i++)
			devpriv->eeprom_data[i] = labpc_eeprom_read(dev, i);
	} else
		s->type		= COMEDI_SUBD_UNUSED;

	return 0;
}
EXPORT_SYMBOL_GPL(labpc_common_attach);

static int labpc_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	const struct labpc_boardinfo *board = comedi_board(dev);
	struct labpc_private *devpriv;
	unsigned long iobase = 0;
	unsigned int irq = 0;
	unsigned int dma_chan = 0;

	devpriv = kzalloc(sizeof(*devpriv), GFP_KERNEL);
	if (!devpriv)
		return -ENOMEM;
	dev->private = devpriv;

	/* get base address, irq etc. based on bustype */
	switch (board->bustype) {
	case isa_bustype:
#ifdef CONFIG_ISA_DMA_API
		iobase = it->options[0];
		irq = it->options[1];
		dma_chan = it->options[2];
#else
		dev_err(dev->class_dev,
			"ni_labpc driver has not been built with ISA DMA support.\n");
		return -EINVAL;
#endif
		break;
	case pci_bustype:
#ifdef CONFIG_COMEDI_PCI_DRIVERS
		dev_err(dev->class_dev,
			"manual configuration of PCI board '%s' is not supported\n",
			board->name);
		return -EINVAL;
#else
		dev_err(dev->class_dev,
			"ni_labpc driver has not been built with PCI support.\n");
		return -EINVAL;
#endif
		break;
	default:
		dev_err(dev->class_dev,
			"ni_labpc: bug! couldn't determine board type\n");
		return -EINVAL;
		break;
	}

	return labpc_common_attach(dev, iobase, irq, dma_chan);
}

static const struct labpc_boardinfo *
labpc_pci_find_boardinfo(struct pci_dev *pcidev)
{
	unsigned int device_id = pcidev->device;
	unsigned int n;

	for (n = 0; n < ARRAY_SIZE(labpc_boards); n++) {
		const struct labpc_boardinfo *board = &labpc_boards[n];
		if (board->bustype == pci_bustype &&
		    board->device_id == device_id)
			return board;
	}
	return NULL;
}

static int labpc_auto_attach(struct comedi_device *dev,
				       unsigned long context_unused)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct labpc_private *devpriv;
	unsigned long iobase;
	unsigned int irq;
	int ret;

	if (!IS_ENABLED(CONFIG_COMEDI_PCI_DRIVERS))
		return -ENODEV;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;

	devpriv = kzalloc(sizeof(*devpriv), GFP_KERNEL);
	if (!devpriv)
		return -ENOMEM;
	dev->private = devpriv;

	dev->board_ptr = labpc_pci_find_boardinfo(pcidev);
	if (!dev->board_ptr)
		return -ENODEV;
	devpriv->mite = mite_alloc(pcidev);
	if (!devpriv->mite)
		return -ENOMEM;
	ret = mite_setup(devpriv->mite);
	if (ret < 0)
		return ret;
	iobase = (unsigned long)devpriv->mite->daq_io_addr;
	irq = mite_irq(devpriv->mite);
	return labpc_common_attach(dev, iobase, irq, 0);
}

void labpc_common_detach(struct comedi_device *dev)
{
	const struct labpc_boardinfo *board = comedi_board(dev);
	struct labpc_private *devpriv = dev->private;
	struct comedi_subdevice *s;

	if (!board)
		return;
	if (dev->subdevices) {
		s = &dev->subdevices[2];
		subdev_8255_cleanup(dev, s);
	}
#ifdef CONFIG_ISA_DMA_API
	/* only free stuff if it has been allocated by _attach */
	kfree(devpriv->dma_buffer);
	if (devpriv->dma_chan)
		free_dma(devpriv->dma_chan);
#endif
	if (dev->irq)
		free_irq(dev->irq, dev);
	if (board->bustype == isa_bustype && dev->iobase)
		release_region(dev->iobase, LABPC_SIZE);
#ifdef CONFIG_COMEDI_PCI_DRIVERS
	if (devpriv->mite) {
		mite_unsetup(devpriv->mite);
		mite_free(devpriv->mite);
	}
	if (board->bustype == pci_bustype)
		comedi_pci_disable(dev);
#endif
}
EXPORT_SYMBOL_GPL(labpc_common_detach);

static struct comedi_driver labpc_driver = {
	.driver_name	= DRV_NAME,
	.module		= THIS_MODULE,
	.attach		= labpc_attach,
	.auto_attach	= labpc_auto_attach,
	.detach		= labpc_common_detach,
	.num_names	= ARRAY_SIZE(labpc_boards),
	.board_name	= &labpc_boards[0].name,
	.offset		= sizeof(struct labpc_boardinfo),
};

#ifdef CONFIG_COMEDI_PCI_DRIVERS
static DEFINE_PCI_DEVICE_TABLE(labpc_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_NI, 0x161) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, labpc_pci_table);

static int labpc_pci_probe(struct pci_dev *dev,
			   const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &labpc_driver, id->driver_data);
}

static struct pci_driver labpc_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= labpc_pci_table,
	.probe		= labpc_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(labpc_driver, labpc_pci_driver);
#else
module_comedi_driver(labpc_driver);
#endif


MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
