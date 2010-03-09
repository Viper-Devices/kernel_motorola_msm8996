/*
 * SH-X3 SMP
 *
 *  Copyright (C) 2007 - 2008  Paul Mundt
 *  Copyright (C) 2007  Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#define STBCR_REG(phys_id) (0xfe400004 | (phys_id << 12))
#define RESET_REG(phys_id) (0xfe400008 | (phys_id << 12))

#define STBCR_MSTP	0x00000001
#define STBCR_RESET	0x00000002
#define STBCR_LTSLP	0x80000000

static irqreturn_t ipi_interrupt_handler(int irq, void *arg)
{
	unsigned int message = (unsigned int)(long)arg;
	unsigned int cpu = hard_smp_processor_id();
	unsigned int offs = 4 * cpu;
	unsigned int x;

	x = __raw_readl(0xfe410070 + offs); /* C0INITICI..CnINTICI */
	x &= (1 << (message << 2));
	__raw_writel(x, 0xfe410080 + offs); /* C0INTICICLR..CnINTICICLR */

	smp_message_recv(message);

	return IRQ_HANDLED;
}

void __init plat_smp_setup(void)
{
	unsigned int cpu = 0;
	int i, num;

	init_cpu_possible(cpumask_of(cpu));

	/* Enable light sleep for the boot CPU */
	__raw_writel(__raw_readl(STBCR_REG(cpu)) | STBCR_LTSLP, STBCR_REG(cpu));

	__cpu_number_map[0] = 0;
	__cpu_logical_map[0] = 0;

	/*
	 * Do this stupidly for now.. we don't have an easy way to probe
	 * for the total number of cores.
	 */
	for (i = 1, num = 0; i < NR_CPUS; i++) {
		set_cpu_possible(i, true);
		__cpu_number_map[i] = ++num;
		__cpu_logical_map[num] = i;
	}

        printk(KERN_INFO "Detected %i available secondary CPU(s)\n", num);
}

void __init plat_prepare_cpus(unsigned int max_cpus)
{
	int i;

	local_timer_setup(0);

	BUILD_BUG_ON(SMP_MSG_NR >= 8);

	for (i = 0; i < SMP_MSG_NR; i++)
		request_irq(104 + i, ipi_interrupt_handler, IRQF_DISABLED,
			    "IPI", (void *)(long)i);
}

void plat_start_cpu(unsigned int cpu, unsigned long entry_point)
{
	if (__in_29bit_mode())
		__raw_writel(entry_point, RESET_REG(cpu));
	else
		__raw_writel(virt_to_phys(entry_point), RESET_REG(cpu));

	if (!(__raw_readl(STBCR_REG(cpu)) & STBCR_MSTP))
		__raw_writel(STBCR_MSTP, STBCR_REG(cpu));

	while (!(__raw_readl(STBCR_REG(cpu)) & STBCR_MSTP))
		cpu_relax();

	/* Start up secondary processor by sending a reset */
	__raw_writel(STBCR_RESET | STBCR_LTSLP, STBCR_REG(cpu));
}

int plat_smp_processor_id(void)
{
	return __raw_readl(0xff000048); /* CPIDR */
}

void plat_send_ipi(unsigned int cpu, unsigned int message)
{
	unsigned long addr = 0xfe410070 + (cpu * 4);

	BUG_ON(cpu >= 4);

	__raw_writel(1 << (message << 2), addr); /* C0INTICI..CnINTICI */
}
