/*
 * arch/sh/mach-cayman/irq.c - SH-5 Cayman Interrupt Support
 *
 * This file handles the board specific parts of the Cayman interrupt system
 *
 * Copyright (C) 2002 Stuart Menefy
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/signal.h>
#include <cpu/irq.h>
#include <asm/page.h>

/* Setup for the SMSC FDC37C935 / LAN91C100FD */
#define SMSC_IRQ         IRQ_IRL1

/* Setup for PCI Bus 2, which transmits interrupts via the EPLD */
#define PCI2_IRQ         IRQ_IRL3

unsigned long epld_virt;

#define EPLD_BASE        0x04002000
#define EPLD_STATUS_BASE (epld_virt + 0x10)
#define EPLD_MASK_BASE   (epld_virt + 0x20)

/* Note the SMSC SuperIO chip and SMSC LAN chip interrupts are all muxed onto
   the same SH-5 interrupt */

static irqreturn_t cayman_interrupt_smsc(int irq, void *dev_id)
{
        printk(KERN_INFO "CAYMAN: spurious SMSC interrupt\n");
	return IRQ_NONE;
}

static irqreturn_t cayman_interrupt_pci2(int irq, void *dev_id)
{
        printk(KERN_INFO "CAYMAN: spurious PCI interrupt, IRQ %d\n", irq);
	return IRQ_NONE;
}

static struct irqaction cayman_action_smsc = {
	.name		= "Cayman SMSC Mux",
	.handler	= cayman_interrupt_smsc,
	.flags		= IRQF_DISABLED,
};

static struct irqaction cayman_action_pci2 = {
	.name		= "Cayman PCI2 Mux",
	.handler	= cayman_interrupt_pci2,
	.flags		= IRQF_DISABLED,
};

static void enable_cayman_irq(unsigned int irq)
{
	unsigned long flags;
	unsigned long mask;
	unsigned int reg;
	unsigned char bit;

	irq -= START_EXT_IRQS;
	reg = EPLD_MASK_BASE + ((irq / 8) << 2);
	bit = 1<<(irq % 8);
	local_irq_save(flags);
	mask = ctrl_inl(reg);
	mask |= bit;
	ctrl_outl(mask, reg);
	local_irq_restore(flags);
}

void disable_cayman_irq(unsigned int irq)
{
	unsigned long flags;
	unsigned long mask;
	unsigned int reg;
	unsigned char bit;

	irq -= START_EXT_IRQS;
	reg = EPLD_MASK_BASE + ((irq / 8) << 2);
	bit = 1<<(irq % 8);
	local_irq_save(flags);
	mask = ctrl_inl(reg);
	mask &= ~bit;
	ctrl_outl(mask, reg);
	local_irq_restore(flags);
}

static void ack_cayman_irq(unsigned int irq)
{
	disable_cayman_irq(irq);
}

struct irq_chip cayman_irq_type = {
	.name		= "Cayman-IRQ",
	.unmask 	= enable_cayman_irq,
	.mask		= disable_cayman_irq,
	.mask_ack	= ack_cayman_irq,
};

int cayman_irq_demux(int evt)
{
	int irq = intc_evt_to_irq[evt];

	if (irq == SMSC_IRQ) {
		unsigned long status;
		int i;

		status = ctrl_inl(EPLD_STATUS_BASE) &
			 ctrl_inl(EPLD_MASK_BASE) & 0xff;
		if (status == 0) {
			irq = -1;
		} else {
			for (i=0; i<8; i++) {
				if (status & (1<<i))
					break;
			}
			irq = START_EXT_IRQS + i;
		}
	}

	if (irq == PCI2_IRQ) {
		unsigned long status;
		int i;

		status = ctrl_inl(EPLD_STATUS_BASE + 3 * sizeof(u32)) &
			 ctrl_inl(EPLD_MASK_BASE + 3 * sizeof(u32)) & 0xff;
		if (status == 0) {
			irq = -1;
		} else {
			for (i=0; i<8; i++) {
				if (status & (1<<i))
					break;
			}
			irq = START_EXT_IRQS + (3 * 8) + i;
		}
	}

	return irq;
}

#if defined(CONFIG_PROC_FS) && defined(CONFIG_SYSCTL)
int cayman_irq_describe(char* p, int irq)
{
	if (irq < NR_INTC_IRQS) {
		return intc_irq_describe(p, irq);
	} else if (irq < NR_INTC_IRQS + 8) {
		return sprintf(p, "(SMSC %d)", irq - NR_INTC_IRQS);
	} else if ((irq >= NR_INTC_IRQS + 24) && (irq < NR_INTC_IRQS + 32)) {
		return sprintf(p, "(PCI2 %d)", irq - (NR_INTC_IRQS + 24));
	}

	return 0;
}
#endif

void init_cayman_irq(void)
{
	int i;

	epld_virt = (unsigned long)ioremap_nocache(EPLD_BASE, 1024);
	if (!epld_virt) {
		printk(KERN_ERR "Cayman IRQ: Unable to remap EPLD\n");
		return;
	}

	for (i = 0; i < NR_EXT_IRQS; i++) {
		set_irq_chip_and_handler(START_EXT_IRQS + i, &cayman_irq_type,
					 handle_level_irq);
	}

	/* Setup the SMSC interrupt */
	setup_irq(SMSC_IRQ, &cayman_action_smsc);
	setup_irq(PCI2_IRQ, &cayman_action_pci2);
}
