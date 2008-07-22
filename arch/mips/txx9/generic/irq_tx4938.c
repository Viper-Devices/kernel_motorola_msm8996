/*
 * linux/arch/mips/tx4938/common/irq.c
 *
 * Common tx4938 irq handler
 * Copyright (C) 2000-2001 Toshiba Corporation
 *
 * 2003-2005 (c) MontaVista Software, Inc. This file is licensed under the
 * terms of the GNU General Public License version 2. This program is
 * licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * Support for TX4938 in 2.6 - Manish Lachwani (mlachwani@mvista.com)
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/irq_cpu.h>
#include <asm/txx9/tx4938.h>

void __init tx4938_irq_init(void)
{
	mips_cpu_irq_init();
	txx9_irq_init(TX4938_IRC_REG & 0xfffffffffULL);
	set_irq_chained_handler(MIPS_CPU_IRQ_BASE + TX4938_IRC_INT,
				handle_simple_irq);
}
