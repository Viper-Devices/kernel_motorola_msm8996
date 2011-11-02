/*
 * Copyright 2001, 2007-2008 MontaVista Software Inc.
 * Author: MontaVista Software, Inc. <source@mvista.com>
 *
 * Copyright (C) 2007 Ralf Baechle (ralf@linux-mips.org)
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/mach-au1x00/au1000.h>

/* Interrupt Controller register offsets */
#define IC_CFG0RD	0x40
#define IC_CFG0SET	0x40
#define IC_CFG0CLR	0x44
#define IC_CFG1RD	0x48
#define IC_CFG1SET	0x48
#define IC_CFG1CLR	0x4C
#define IC_CFG2RD	0x50
#define IC_CFG2SET	0x50
#define IC_CFG2CLR	0x54
#define IC_REQ0INT	0x54
#define IC_SRCRD	0x58
#define IC_SRCSET	0x58
#define IC_SRCCLR	0x5C
#define IC_REQ1INT	0x5C
#define IC_ASSIGNRD	0x60
#define IC_ASSIGNSET	0x60
#define IC_ASSIGNCLR	0x64
#define IC_WAKERD	0x68
#define IC_WAKESET	0x68
#define IC_WAKECLR	0x6C
#define IC_MASKRD	0x70
#define IC_MASKSET	0x70
#define IC_MASKCLR	0x74
#define IC_RISINGRD	0x78
#define IC_RISINGCLR	0x78
#define IC_FALLINGRD	0x7C
#define IC_FALLINGCLR	0x7C
#define IC_TESTBIT	0x80

static int au1x_ic_settype(struct irq_data *d, unsigned int flow_type);

/* NOTE on interrupt priorities: The original writers of this code said:
 *
 * Because of the tight timing of SETUP token to reply transactions,
 * the USB devices-side packet complete interrupt (USB_DEV_REQ_INT)
 * needs the highest priority.
 */

/* per-processor fixed function irqs */
struct au1xxx_irqmap {
	int im_irq;
	int im_type;
	int im_request;		/* set 1 to get higher priority */
};

struct au1xxx_irqmap au1000_irqmap[] __initdata = {
	{ AU1000_UART0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_UART1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_UART2_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_UART3_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_SSI0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_SSI1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE+1,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE+2,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE+3,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE+4,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE+5,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE+6,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_DMA_INT_BASE+7,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_TOY_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_TOY_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_TOY_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_TOY_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_RTC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_RTC_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_RTC_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_RTC_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 1 },
	{ AU1000_IRDA_TX_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_IRDA_RX_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_USB_DEV_REQ_INT, IRQ_TYPE_LEVEL_HIGH,  1 },
	{ AU1000_USB_DEV_SUS_INT, IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_USB_HOST_INT,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1000_ACSYNC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1000_MAC0_DMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_MAC1_DMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1000_AC97C_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ -1, },
};

struct au1xxx_irqmap au1500_irqmap[] __initdata = {
	{ AU1500_UART0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_PCI_INTA,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1500_PCI_INTB,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1500_UART3_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_PCI_INTC,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1500_PCI_INTD,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1500_DMA_INT_BASE,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_DMA_INT_BASE+1,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_DMA_INT_BASE+2,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_DMA_INT_BASE+3,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_DMA_INT_BASE+4,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_DMA_INT_BASE+5,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_DMA_INT_BASE+6,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_DMA_INT_BASE+7,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_TOY_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_TOY_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_TOY_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_TOY_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_RTC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_RTC_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_RTC_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_RTC_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 1 },
	{ AU1500_USB_DEV_REQ_INT, IRQ_TYPE_LEVEL_HIGH,  1 },
	{ AU1500_USB_DEV_SUS_INT, IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_USB_HOST_INT,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1500_ACSYNC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1500_MAC0_DMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_MAC1_DMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1500_AC97C_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ -1, },
};

struct au1xxx_irqmap au1100_irqmap[] __initdata = {
	{ AU1100_UART0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_UART1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_SD_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_UART3_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_SSI0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_SSI1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE+1,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE+2,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE+3,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE+4,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE+5,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE+6,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_DMA_INT_BASE+7,  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_TOY_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_TOY_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_TOY_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_TOY_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_RTC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_RTC_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_RTC_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_RTC_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 1 },
	{ AU1100_IRDA_TX_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_IRDA_RX_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_USB_DEV_REQ_INT, IRQ_TYPE_LEVEL_HIGH,  1 },
	{ AU1100_USB_DEV_SUS_INT, IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_USB_HOST_INT,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1100_ACSYNC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1100_MAC0_DMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_LCD_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1100_AC97C_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ -1, },
};

struct au1xxx_irqmap au1550_irqmap[] __initdata = {
	{ AU1550_UART0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_PCI_INTA,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1550_PCI_INTB,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1550_DDMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_CRYPTO_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_PCI_INTC,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1550_PCI_INTD,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1550_PCI_RST_INT,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1550_UART1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_UART3_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_PSC0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_PSC1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_PSC2_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_PSC3_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_TOY_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_TOY_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_TOY_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_TOY_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_RTC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_RTC_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_RTC_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_RTC_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 1 },
	{ AU1550_NAND_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_USB_DEV_REQ_INT, IRQ_TYPE_LEVEL_HIGH,  1 },
	{ AU1550_USB_DEV_SUS_INT, IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1550_USB_HOST_INT,	  IRQ_TYPE_LEVEL_LOW,   0 },
	{ AU1550_MAC0_DMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1550_MAC1_DMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ -1, },
};

struct au1xxx_irqmap au1200_irqmap[] __initdata = {
	{ AU1200_UART0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_SWT_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_SD_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_DDMA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_MAE_BE_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_UART1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_MAE_FE_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_PSC0_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_PSC1_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_AES_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_CAMERA_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_TOY_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_TOY_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_TOY_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_TOY_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_RTC_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_RTC_MATCH0_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_RTC_MATCH1_INT,  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_RTC_MATCH2_INT,  IRQ_TYPE_EDGE_RISING, 1 },
	{ AU1200_NAND_INT,	  IRQ_TYPE_EDGE_RISING, 0 },
	{ AU1200_USB_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_LCD_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ AU1200_MAE_BOTH_INT,	  IRQ_TYPE_LEVEL_HIGH,  0 },
	{ -1, },
};


static void au1x_ic0_unmask(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC0_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR);

	__raw_writel(1 << bit, base + IC_MASKSET);
	__raw_writel(1 << bit, base + IC_WAKESET);
	wmb();
}

static void au1x_ic1_unmask(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC1_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR);

	__raw_writel(1 << bit, base + IC_MASKSET);
	__raw_writel(1 << bit, base + IC_WAKESET);
	wmb();
}

static void au1x_ic0_mask(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC0_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR);

	__raw_writel(1 << bit, base + IC_MASKCLR);
	__raw_writel(1 << bit, base + IC_WAKECLR);
	wmb();
}

static void au1x_ic1_mask(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC1_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR);

	__raw_writel(1 << bit, base + IC_MASKCLR);
	__raw_writel(1 << bit, base + IC_WAKECLR);
	wmb();
}

static void au1x_ic0_ack(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC0_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR);

	/*
	 * This may assume that we don't get interrupts from
	 * both edges at once, or if we do, that we don't care.
	 */
	__raw_writel(1 << bit, base + IC_FALLINGCLR);
	__raw_writel(1 << bit, base + IC_RISINGCLR);
	wmb();
}

static void au1x_ic1_ack(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC1_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR);

	/*
	 * This may assume that we don't get interrupts from
	 * both edges at once, or if we do, that we don't care.
	 */
	__raw_writel(1 << bit, base + IC_FALLINGCLR);
	__raw_writel(1 << bit, base + IC_RISINGCLR);
	wmb();
}

static void au1x_ic0_maskack(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC0_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR);

	__raw_writel(1 << bit, base + IC_WAKECLR);
	__raw_writel(1 << bit, base + IC_MASKCLR);
	__raw_writel(1 << bit, base + IC_RISINGCLR);
	__raw_writel(1 << bit, base + IC_FALLINGCLR);
	wmb();
}

static void au1x_ic1_maskack(struct irq_data *d)
{
	unsigned int bit = d->irq - AU1000_INTC1_INT_BASE;
	void __iomem *base = (void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR);

	__raw_writel(1 << bit, base + IC_WAKECLR);
	__raw_writel(1 << bit, base + IC_MASKCLR);
	__raw_writel(1 << bit, base + IC_RISINGCLR);
	__raw_writel(1 << bit, base + IC_FALLINGCLR);
	wmb();
}

static int au1x_ic1_setwake(struct irq_data *d, unsigned int on)
{
	int bit = d->irq - AU1000_INTC1_INT_BASE;
	unsigned long wakemsk, flags;

	/* only GPIO 0-7 can act as wakeup source.  Fortunately these
	 * are wired up identically on all supported variants.
	 */
	if ((bit < 0) || (bit > 7))
		return -EINVAL;

	local_irq_save(flags);
	wakemsk = __raw_readl((void __iomem *)SYS_WAKEMSK);
	if (on)
		wakemsk |= 1 << bit;
	else
		wakemsk &= ~(1 << bit);
	__raw_writel(wakemsk, (void __iomem *)SYS_WAKEMSK);
	wmb();
	local_irq_restore(flags);

	return 0;
}

/*
 * irq_chips for both ICs; this way the mask handlers can be
 * as short as possible.
 */
static struct irq_chip au1x_ic0_chip = {
	.name		= "Alchemy-IC0",
	.irq_ack	= au1x_ic0_ack,
	.irq_mask	= au1x_ic0_mask,
	.irq_mask_ack	= au1x_ic0_maskack,
	.irq_unmask	= au1x_ic0_unmask,
	.irq_set_type	= au1x_ic_settype,
};

static struct irq_chip au1x_ic1_chip = {
	.name		= "Alchemy-IC1",
	.irq_ack	= au1x_ic1_ack,
	.irq_mask	= au1x_ic1_mask,
	.irq_mask_ack	= au1x_ic1_maskack,
	.irq_unmask	= au1x_ic1_unmask,
	.irq_set_type	= au1x_ic_settype,
	.irq_set_wake	= au1x_ic1_setwake,
};

static int au1x_ic_settype(struct irq_data *d, unsigned int flow_type)
{
	struct irq_chip *chip;
	unsigned int bit, irq = d->irq;
	irq_flow_handler_t handler = NULL;
	unsigned char *name = NULL;
	void __iomem *base;
	int ret;

	if (irq >= AU1000_INTC1_INT_BASE) {
		bit = irq - AU1000_INTC1_INT_BASE;
		chip = &au1x_ic1_chip;
		base = (void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR);
	} else {
		bit = irq - AU1000_INTC0_INT_BASE;
		chip = &au1x_ic0_chip;
		base = (void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR);
	}

	if (bit > 31)
		return -EINVAL;

	ret = 0;

	switch (flow_type) {	/* cfgregs 2:1:0 */
	case IRQ_TYPE_EDGE_RISING:	/* 0:0:1 */
		__raw_writel(1 << bit, base + IC_CFG2CLR);
		__raw_writel(1 << bit, base + IC_CFG1CLR);
		__raw_writel(1 << bit, base + IC_CFG0SET);
		handler = handle_edge_irq;
		name = "riseedge";
		break;
	case IRQ_TYPE_EDGE_FALLING:	/* 0:1:0 */
		__raw_writel(1 << bit, base + IC_CFG2CLR);
		__raw_writel(1 << bit, base + IC_CFG1SET);
		__raw_writel(1 << bit, base + IC_CFG0CLR);
		handler = handle_edge_irq;
		name = "falledge";
		break;
	case IRQ_TYPE_EDGE_BOTH:	/* 0:1:1 */
		__raw_writel(1 << bit, base + IC_CFG2CLR);
		__raw_writel(1 << bit, base + IC_CFG1SET);
		__raw_writel(1 << bit, base + IC_CFG0SET);
		handler = handle_edge_irq;
		name = "bothedge";
		break;
	case IRQ_TYPE_LEVEL_HIGH:	/* 1:0:1 */
		__raw_writel(1 << bit, base + IC_CFG2SET);
		__raw_writel(1 << bit, base + IC_CFG1CLR);
		__raw_writel(1 << bit, base + IC_CFG0SET);
		handler = handle_level_irq;
		name = "hilevel";
		break;
	case IRQ_TYPE_LEVEL_LOW:	/* 1:1:0 */
		__raw_writel(1 << bit, base + IC_CFG2SET);
		__raw_writel(1 << bit, base + IC_CFG1SET);
		__raw_writel(1 << bit, base + IC_CFG0CLR);
		handler = handle_level_irq;
		name = "lowlevel";
		break;
	case IRQ_TYPE_NONE:		/* 0:0:0 */
		__raw_writel(1 << bit, base + IC_CFG2CLR);
		__raw_writel(1 << bit, base + IC_CFG1CLR);
		__raw_writel(1 << bit, base + IC_CFG0CLR);
		break;
	default:
		ret = -EINVAL;
	}
	__irq_set_chip_handler_name_locked(d->irq, chip, handler, name);

	wmb();

	return ret;
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_status() & read_c0_cause();
	unsigned long s, off;

	if (pending & CAUSEF_IP7) {
		off = MIPS_CPU_IRQ_BASE + 7;
		goto handle;
	} else if (pending & CAUSEF_IP2) {
		s = KSEG1ADDR(AU1000_IC0_PHYS_ADDR) + IC_REQ0INT;
		off = AU1000_INTC0_INT_BASE;
	} else if (pending & CAUSEF_IP3) {
		s = KSEG1ADDR(AU1000_IC0_PHYS_ADDR) + IC_REQ1INT;
		off = AU1000_INTC0_INT_BASE;
	} else if (pending & CAUSEF_IP4) {
		s = KSEG1ADDR(AU1000_IC1_PHYS_ADDR) + IC_REQ0INT;
		off = AU1000_INTC1_INT_BASE;
	} else if (pending & CAUSEF_IP5) {
		s = KSEG1ADDR(AU1000_IC1_PHYS_ADDR) + IC_REQ1INT;
		off = AU1000_INTC1_INT_BASE;
	} else
		goto spurious;

	s = __raw_readl((void __iomem *)s);
	if (unlikely(!s)) {
spurious:
		spurious_interrupt();
		return;
	}
	off += __ffs(s);
handle:
	do_IRQ(off);
}


static inline void ic_init(void __iomem *base)
{
	/* initialize interrupt controller to a safe state */
	__raw_writel(0xffffffff, base + IC_CFG0CLR);
	__raw_writel(0xffffffff, base + IC_CFG1CLR);
	__raw_writel(0xffffffff, base + IC_CFG2CLR);
	__raw_writel(0xffffffff, base + IC_MASKCLR);
	__raw_writel(0xffffffff, base + IC_ASSIGNCLR);
	__raw_writel(0xffffffff, base + IC_WAKECLR);
	__raw_writel(0xffffffff, base + IC_SRCSET);
	__raw_writel(0xffffffff, base + IC_FALLINGCLR);
	__raw_writel(0xffffffff, base + IC_RISINGCLR);
	__raw_writel(0x00000000, base + IC_TESTBIT);
	wmb();
}

static void __init au1000_init_irq(struct au1xxx_irqmap *map)
{
	unsigned int bit, irq_nr;
	void __iomem *base;

	ic_init((void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR));
	ic_init((void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR));
	mips_cpu_irq_init();

	/* register all 64 possible IC0+IC1 irq sources as type "none".
	 * Use set_irq_type() to set edge/level behaviour at runtime.
	 */
	for (irq_nr = AU1000_INTC0_INT_BASE;
	     (irq_nr < AU1000_INTC0_INT_BASE + 32); irq_nr++)
		au1x_ic_settype(irq_get_irq_data(irq_nr), IRQ_TYPE_NONE);

	for (irq_nr = AU1000_INTC1_INT_BASE;
	     (irq_nr < AU1000_INTC1_INT_BASE + 32); irq_nr++)
		au1x_ic_settype(irq_get_irq_data(irq_nr), IRQ_TYPE_NONE);

	/*
	 * Initialize IC0, which is fixed per processor.
	 */
	while (map->im_irq != -1) {
		irq_nr = map->im_irq;

		if (irq_nr >= AU1000_INTC1_INT_BASE) {
			bit = irq_nr - AU1000_INTC1_INT_BASE;
			base = (void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR);
		} else {
			bit = irq_nr - AU1000_INTC0_INT_BASE;
			base = (void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR);
		}
		if (map->im_request)
			__raw_writel(1 << bit, base + IC_ASSIGNSET);

		au1x_ic_settype(irq_get_irq_data(irq_nr), map->im_type);
		++map;
	}

	set_c0_status(IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3);
}

void __init arch_init_irq(void)
{
	switch (alchemy_get_cputype()) {
	case ALCHEMY_CPU_AU1000:
		au1000_init_irq(au1000_irqmap);
		break;
	case ALCHEMY_CPU_AU1500:
		au1000_init_irq(au1500_irqmap);
		break;
	case ALCHEMY_CPU_AU1100:
		au1000_init_irq(au1100_irqmap);
		break;
	case ALCHEMY_CPU_AU1550:
		au1000_init_irq(au1550_irqmap);
		break;
	case ALCHEMY_CPU_AU1200:
		au1000_init_irq(au1200_irqmap);
		break;
	}
}


static unsigned long alchemy_ic_pmdata[7 * 2];

static inline void alchemy_ic_suspend_one(void __iomem *base, unsigned long *d)
{
	d[0] = __raw_readl(base + IC_CFG0RD);
	d[1] = __raw_readl(base + IC_CFG1RD);
	d[2] = __raw_readl(base + IC_CFG2RD);
	d[3] = __raw_readl(base + IC_SRCRD);
	d[4] = __raw_readl(base + IC_ASSIGNRD);
	d[5] = __raw_readl(base + IC_WAKERD);
	d[6] = __raw_readl(base + IC_MASKRD);
	ic_init(base);		/* shut it up too while at it */
}

static inline void alchemy_ic_resume_one(void __iomem *base, unsigned long *d)
{
	ic_init(base);

	__raw_writel(d[0], base + IC_CFG0SET);
	__raw_writel(d[1], base + IC_CFG1SET);
	__raw_writel(d[2], base + IC_CFG2SET);
	__raw_writel(d[3], base + IC_SRCSET);
	__raw_writel(d[4], base + IC_ASSIGNSET);
	__raw_writel(d[5], base + IC_WAKESET);
	wmb();

	__raw_writel(d[6], base + IC_MASKSET);
	wmb();
}

static int alchemy_ic_suspend(void)
{
	alchemy_ic_suspend_one((void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR),
			       alchemy_ic_pmdata);
	alchemy_ic_suspend_one((void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR),
			       &alchemy_ic_pmdata[7]);
	return 0;
}

static void alchemy_ic_resume(void)
{
	alchemy_ic_resume_one((void __iomem *)KSEG1ADDR(AU1000_IC1_PHYS_ADDR),
			      &alchemy_ic_pmdata[7]);
	alchemy_ic_resume_one((void __iomem *)KSEG1ADDR(AU1000_IC0_PHYS_ADDR),
			      alchemy_ic_pmdata);
}

static struct syscore_ops alchemy_ic_syscore_ops = {
	.suspend	= alchemy_ic_suspend,
	.resume		= alchemy_ic_resume,
};

static int __init alchemy_ic_pm_init(void)
{
	register_syscore_ops(&alchemy_ic_syscore_ops);
	return 0;
}
device_initcall(alchemy_ic_pm_init);
