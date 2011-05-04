/*
 * Copyright 2011 IBM Corporation.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/spinlock.h>

#include <asm/prom.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/irq.h>
#include <asm/errno.h>
#include <asm/xics.h>

struct icp_ipl {
	union {
		u32 word;
		u8 bytes[4];
	} xirr_poll;
	union {
		u32 word;
		u8 bytes[4];
	} xirr;
	u32 dummy;
	union {
		u32 word;
		u8 bytes[4];
	} qirr;
	u32 link_a;
	u32 link_b;
	u32 link_c;
};

static struct icp_ipl __iomem *icp_native_regs[NR_CPUS];

static inline unsigned int icp_native_get_xirr(void)
{
	int cpu = smp_processor_id();

	return in_be32(&icp_native_regs[cpu]->xirr.word);
}

static inline void icp_native_set_xirr(unsigned int value)
{
	int cpu = smp_processor_id();

	out_be32(&icp_native_regs[cpu]->xirr.word, value);
}

static inline void icp_native_set_cppr(u8 value)
{
	int cpu = smp_processor_id();

	out_8(&icp_native_regs[cpu]->xirr.bytes[0], value);
}

static inline void icp_native_set_qirr(int n_cpu, u8 value)
{
	out_8(&icp_native_regs[n_cpu]->qirr.bytes[0], value);
}

static void icp_native_set_cpu_priority(unsigned char cppr)
{
	xics_set_base_cppr(cppr);
	icp_native_set_cppr(cppr);
	iosync();
}

static void icp_native_eoi(struct irq_data *d)
{
	unsigned int hw_irq = (unsigned int)irqd_to_hwirq(d);

	iosync();
	icp_native_set_xirr((xics_pop_cppr() << 24) | hw_irq);
}

static void icp_native_teardown_cpu(void)
{
	int cpu = smp_processor_id();

	/* Clear any pending IPI */
	icp_native_set_qirr(cpu, 0xff);
}

static void icp_native_flush_ipi(void)
{
	/* We take the ipi irq but and never return so we
	 * need to EOI the IPI, but want to leave our priority 0
	 *
	 * should we check all the other interrupts too?
	 * should we be flagging idle loop instead?
	 * or creating some task to be scheduled?
	 */

	icp_native_set_xirr((0x00 << 24) | XICS_IPI);
}

static unsigned int icp_native_get_irq(void)
{
	unsigned int xirr = icp_native_get_xirr();
	unsigned int vec = xirr & 0x00ffffff;
	unsigned int irq;

	if (vec == XICS_IRQ_SPURIOUS)
		return NO_IRQ;

	irq = irq_radix_revmap_lookup(xics_host, vec);
	if (likely(irq != NO_IRQ)) {
		xics_push_cppr(vec);
		return irq;
	}

	/* We don't have a linux mapping, so have rtas mask it. */
	xics_mask_unknown_vec(vec);

	/* We might learn about it later, so EOI it */
	icp_native_set_xirr(xirr);

	return NO_IRQ;
}

#ifdef CONFIG_SMP

static inline void icp_native_do_message(int cpu, int msg)
{
	unsigned long *tgt = &per_cpu(xics_ipi_message, cpu);

	set_bit(msg, tgt);
	mb();
	icp_native_set_qirr(cpu, IPI_PRIORITY);
}

static void icp_native_message_pass(int target, int msg)
{
	unsigned int i;

	if (target < NR_CPUS) {
		icp_native_do_message(target, msg);
	} else {
		for_each_online_cpu(i) {
			if (target == MSG_ALL_BUT_SELF
			    && i == smp_processor_id())
				continue;
			icp_native_do_message(i, msg);
		}
	}
}

static irqreturn_t icp_native_ipi_action(int irq, void *dev_id)
{
	int cpu = smp_processor_id();

	icp_native_set_qirr(cpu, 0xff);

	return xics_ipi_dispatch(cpu);
}

#endif /* CONFIG_SMP */

static int __init icp_native_map_one_cpu(int hw_id, unsigned long addr,
					 unsigned long size)
{
	char *rname;
	int i, cpu = -1;

	/* This may look gross but it's good enough for now, we don't quite
	 * have a hard -> linux processor id matching.
	 */
	for_each_possible_cpu(i) {
		if (!cpu_present(i))
			continue;
		if (hw_id == get_hard_smp_processor_id(i)) {
			cpu = i;
			break;
		}
	}

	/* Fail, skip that CPU. Don't print, it's normal, some XICS come up
	 * with way more entries in there than you have CPUs
	 */
	if (cpu == -1)
		return 0;

	rname = kasprintf(GFP_KERNEL, "CPU %d [0x%x] Interrupt Presentation",
			  cpu, hw_id);

	if (!request_mem_region(addr, size, rname)) {
		pr_warning("icp_native: Could not reserve ICP MMIO"
			   " for CPU %d, interrupt server #0x%x\n",
			   cpu, hw_id);
		return -EBUSY;
	}

	icp_native_regs[cpu] = ioremap(addr, size);
	if (!icp_native_regs[cpu]) {
		pr_warning("icp_native: Failed ioremap for CPU %d, "
			   "interrupt server #0x%x, addr %#lx\n",
			   cpu, hw_id, addr);
		release_mem_region(addr, size);
		return -ENOMEM;
	}
	return 0;
}

static int __init icp_native_init_one_node(struct device_node *np,
					   unsigned int *indx)
{
	unsigned int ilen;
	const u32 *ireg;
	int i;
	int reg_tuple_size;
	int num_servers = 0;

	/* This code does the theorically broken assumption that the interrupt
	 * server numbers are the same as the hard CPU numbers.
	 * This happens to be the case so far but we are playing with fire...
	 * should be fixed one of these days. -BenH.
	 */
	ireg = of_get_property(np, "ibm,interrupt-server-ranges", &ilen);

	/* Do that ever happen ? we'll know soon enough... but even good'old
	 * f80 does have that property ..
	 */
	WARN_ON((ireg == NULL) || (ilen != 2*sizeof(u32)));

	if (ireg) {
		*indx = of_read_number(ireg, 1);
		if (ilen >= 2*sizeof(u32))
			num_servers = of_read_number(ireg + 1, 1);
	}

	ireg = of_get_property(np, "reg", &ilen);
	if (!ireg) {
		pr_err("icp_native: Can't find interrupt reg property");
		return -1;
	}

	reg_tuple_size = (of_n_addr_cells(np) + of_n_size_cells(np)) * 4;
	if (((ilen % reg_tuple_size) != 0)
	    || (num_servers && (num_servers != (ilen / reg_tuple_size)))) {
		pr_err("icp_native: ICP reg len (%d) != num servers (%d)",
		       ilen / reg_tuple_size, num_servers);
		return -1;
	}

	for (i = 0; i < (ilen / reg_tuple_size); i++) {
		struct resource r;
		int err;

		err = of_address_to_resource(np, i, &r);
		if (err) {
			pr_err("icp_native: Could not translate ICP MMIO"
			       " for interrupt server 0x%x (%d)\n", *indx, err);
			return -1;
		}

		if (icp_native_map_one_cpu(*indx, r.start, r.end - r.start))
			return -1;

		(*indx)++;
	}
	return 0;
}

static const struct icp_ops icp_native_ops = {
	.get_irq	= icp_native_get_irq,
	.eoi		= icp_native_eoi,
	.set_priority	= icp_native_set_cpu_priority,
	.teardown_cpu	= icp_native_teardown_cpu,
	.flush_ipi	= icp_native_flush_ipi,
#ifdef CONFIG_SMP
	.ipi_action	= icp_native_ipi_action,
	.message_pass	= icp_native_message_pass,
#endif
};

int icp_native_init(void)
{
	struct device_node *np;
	u32 indx = 0;
	int found = 0;

	for_each_compatible_node(np, NULL, "ibm,ppc-xicp")
		if (icp_native_init_one_node(np, &indx) == 0)
			found = 1;
	if (!found) {
		for_each_node_by_type(np,
			"PowerPC-External-Interrupt-Presentation") {
				if (icp_native_init_one_node(np, &indx) == 0)
					found = 1;
		}
	}

	if (found == 0)
		return -ENODEV;

	icp_ops = &icp_native_ops;

	return 0;
}
