/*
 * This code largely moved from arch/i386/kernel/timer/timer_tsc.c
 * which was originally moved from arch/i386/kernel/time.c.
 * See comments there for proper credits.
 */

#include <linux/workqueue.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/init.h>

#include <asm/tsc.h>
#include <asm/io.h>

#include "mach_timer.h"

/*
 * On some systems the TSC frequency does not
 * change with the cpu frequency. So we need
 * an extra value to store the TSC freq
 */
unsigned int tsc_khz;

int tsc_disable __cpuinitdata = 0;

#ifdef CONFIG_X86_TSC
static int __init tsc_setup(char *str)
{
	printk(KERN_WARNING "notsc: Kernel compiled with CONFIG_X86_TSC, "
				"cannot disable TSC.\n");
	return 1;
}
#else
/*
 * disable flag for tsc. Takes effect by clearing the TSC cpu flag
 * in cpu/common.c
 */
static int __init tsc_setup(char *str)
{
	tsc_disable = 1;

	return 1;
}
#endif

__setup("notsc", tsc_setup);


/*
 * code to mark and check if the TSC is unstable
 * due to cpufreq or due to unsynced TSCs
 */
static int tsc_unstable;

static inline int check_tsc_unstable(void)
{
	return tsc_unstable;
}

void mark_tsc_unstable(void)
{
	tsc_unstable = 1;
}
EXPORT_SYMBOL_GPL(mark_tsc_unstable);

/* Accellerators for sched_clock()
 * convert from cycles(64bits) => nanoseconds (64bits)
 *  basic equation:
 *		ns = cycles / (freq / ns_per_sec)
 *		ns = cycles * (ns_per_sec / freq)
 *		ns = cycles * (10^9 / (cpu_khz * 10^3))
 *		ns = cycles * (10^6 / cpu_khz)
 *
 *	Then we use scaling math (suggested by george@mvista.com) to get:
 *		ns = cycles * (10^6 * SC / cpu_khz) / SC
 *		ns = cycles * cyc2ns_scale / SC
 *
 *	And since SC is a constant power of two, we can convert the div
 *  into a shift.
 *
 *  We can use khz divisor instead of mhz to keep a better percision, since
 *  cyc2ns_scale is limited to 10^6 * 2^10, which fits in 32 bits.
 *  (mathieu.desnoyers@polymtl.ca)
 *
 *			-johnstul@us.ibm.com "math is hard, lets go shopping!"
 */
static unsigned long cyc2ns_scale __read_mostly;

#define CYC2NS_SCALE_FACTOR 10 /* 2^10, carefully chosen */

static inline void set_cyc2ns_scale(unsigned long cpu_khz)
{
	cyc2ns_scale = (1000000 << CYC2NS_SCALE_FACTOR)/cpu_khz;
}

static inline unsigned long long cycles_2_ns(unsigned long long cyc)
{
	return (cyc * cyc2ns_scale) >> CYC2NS_SCALE_FACTOR;
}

/*
 * Scheduler clock - returns current time in nanosec units.
 */
unsigned long long sched_clock(void)
{
	unsigned long long this_offset;

	/*
	 * in the NUMA case we dont use the TSC as they are not
	 * synchronized across all CPUs.
	 */
#ifndef CONFIG_NUMA
	if (!cpu_khz || check_tsc_unstable())
#endif
		/* no locking but a rare wrong value is not a big deal */
		return (jiffies_64 - INITIAL_JIFFIES) * (1000000000 / HZ);

	/* read the Time Stamp Counter: */
	rdtscll(this_offset);

	/* return the value in ns */
	return cycles_2_ns(this_offset);
}

static unsigned long calculate_cpu_khz(void)
{
	unsigned long long start, end;
	unsigned long count;
	u64 delta64;
	int i;
	unsigned long flags;

	local_irq_save(flags);

	/* run 3 times to ensure the cache is warm */
	for (i = 0; i < 3; i++) {
		mach_prepare_counter();
		rdtscll(start);
		mach_countup(&count);
		rdtscll(end);
	}
	/*
	 * Error: ECTCNEVERSET
	 * The CTC wasn't reliable: we got a hit on the very first read,
	 * or the CPU was so fast/slow that the quotient wouldn't fit in
	 * 32 bits..
	 */
	if (count <= 1)
		goto err;

	delta64 = end - start;

	/* cpu freq too fast: */
	if (delta64 > (1ULL<<32))
		goto err;

	/* cpu freq too slow: */
	if (delta64 <= CALIBRATE_TIME_MSEC)
		goto err;

	delta64 += CALIBRATE_TIME_MSEC/2; /* round for do_div */
	do_div(delta64,CALIBRATE_TIME_MSEC);

	local_irq_restore(flags);
	return (unsigned long)delta64;
err:
	local_irq_restore(flags);
	return 0;
}

int recalibrate_cpu_khz(void)
{
#ifndef CONFIG_SMP
	unsigned long cpu_khz_old = cpu_khz;

	if (cpu_has_tsc) {
		cpu_khz = calculate_cpu_khz();
		tsc_khz = cpu_khz;
		cpu_data[0].loops_per_jiffy =
			cpufreq_scale(cpu_data[0].loops_per_jiffy,
					cpu_khz_old, cpu_khz);
		return 0;
	} else
		return -ENODEV;
#else
	return -ENODEV;
#endif
}

EXPORT_SYMBOL(recalibrate_cpu_khz);

void tsc_init(void)
{
	if (!cpu_has_tsc || tsc_disable)
		return;

	cpu_khz = calculate_cpu_khz();
	tsc_khz = cpu_khz;

	if (!cpu_khz)
		return;

	printk("Detected %lu.%03lu MHz processor.\n",
				(unsigned long)cpu_khz / 1000,
				(unsigned long)cpu_khz % 1000);

	set_cyc2ns_scale(cpu_khz);
}

#ifdef CONFIG_CPU_FREQ

static unsigned int cpufreq_delayed_issched = 0;
static unsigned int cpufreq_init = 0;
static struct work_struct cpufreq_delayed_get_work;

static void handle_cpufreq_delayed_get(void *v)
{
	unsigned int cpu;

	for_each_online_cpu(cpu)
		cpufreq_get(cpu);

	cpufreq_delayed_issched = 0;
}

/*
 * if we notice cpufreq oddness, schedule a call to cpufreq_get() as it tries
 * to verify the CPU frequency the timing core thinks the CPU is running
 * at is still correct.
 */
static inline void cpufreq_delayed_get(void)
{
	if (cpufreq_init && !cpufreq_delayed_issched) {
		cpufreq_delayed_issched = 1;
		printk(KERN_DEBUG "Checking if CPU frequency changed.\n");
		schedule_work(&cpufreq_delayed_get_work);
	}
}

/*
 * if the CPU frequency is scaled, TSC-based delays will need a different
 * loops_per_jiffy value to function properly.
 */
static unsigned int ref_freq = 0;
static unsigned long loops_per_jiffy_ref = 0;
static unsigned long cpu_khz_ref = 0;

static int
time_cpufreq_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;

	if (val != CPUFREQ_RESUMECHANGE && val != CPUFREQ_SUSPENDCHANGE)
		write_seqlock_irq(&xtime_lock);

	if (!ref_freq) {
		if (!freq->old){
			ref_freq = freq->new;
			goto end;
		}
		ref_freq = freq->old;
		loops_per_jiffy_ref = cpu_data[freq->cpu].loops_per_jiffy;
		cpu_khz_ref = cpu_khz;
	}

	if ((val == CPUFREQ_PRECHANGE  && freq->old < freq->new) ||
	    (val == CPUFREQ_POSTCHANGE && freq->old > freq->new) ||
	    (val == CPUFREQ_RESUMECHANGE)) {
		if (!(freq->flags & CPUFREQ_CONST_LOOPS))
			cpu_data[freq->cpu].loops_per_jiffy =
				cpufreq_scale(loops_per_jiffy_ref,
						ref_freq, freq->new);

		if (cpu_khz) {

			if (num_online_cpus() == 1)
				cpu_khz = cpufreq_scale(cpu_khz_ref,
						ref_freq, freq->new);
			if (!(freq->flags & CPUFREQ_CONST_LOOPS)) {
				tsc_khz = cpu_khz;
				set_cyc2ns_scale(cpu_khz);
				/*
				 * TSC based sched_clock turns
				 * to junk w/ cpufreq
				 */
				mark_tsc_unstable();
			}
		}
	}
end:
	if (val != CPUFREQ_RESUMECHANGE && val != CPUFREQ_SUSPENDCHANGE)
		write_sequnlock_irq(&xtime_lock);

	return 0;
}

static struct notifier_block time_cpufreq_notifier_block = {
	.notifier_call	= time_cpufreq_notifier
};

static int __init cpufreq_tsc(void)
{
	int ret;

	INIT_WORK(&cpufreq_delayed_get_work, handle_cpufreq_delayed_get, NULL);
	ret = cpufreq_register_notifier(&time_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (!ret)
		cpufreq_init = 1;

	return ret;
}

core_initcall(cpufreq_tsc);

#endif
