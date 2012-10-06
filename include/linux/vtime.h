#ifndef _LINUX_KERNEL_VTIME_H
#define _LINUX_KERNEL_VTIME_H

struct task_struct;

#ifdef CONFIG_VIRT_CPU_ACCOUNTING
extern void vtime_task_switch(struct task_struct *prev);
extern void __vtime_account_system(struct task_struct *tsk);
extern void vtime_account_system(struct task_struct *tsk);
extern void __vtime_account_idle(struct task_struct *tsk);
#else
static inline void vtime_task_switch(struct task_struct *prev) { }
static inline void vtime_account_system(struct task_struct *tsk) { }
#endif

#if !defined(CONFIG_VIRT_CPU_ACCOUNTING) && !defined(CONFIG_IRQ_TIME_ACCOUNTING)
static inline void vtime_account(struct task_struct *tsk)
{
}
#else
extern void vtime_account(struct task_struct *tsk);
#endif

static inline void vtime_account_irq_enter(struct task_struct *tsk)
{
	/*
	 * Hardirq can interrupt idle task anytime. So we need vtime_account()
	 * that performs the idle check in CONFIG_VIRT_CPU_ACCOUNTING.
	 * Softirq can also interrupt idle task directly if it calls
	 * local_bh_enable(). Such case probably don't exist but we never know.
	 * Ksoftirqd is not concerned because idle time is flushed on context
	 * switch. Softirqs in the end of hardirqs are also not a problem because
	 * the idle time is flushed on hardirq time already.
	 */
	vtime_account(tsk);
}

static inline void vtime_account_irq_exit(struct task_struct *tsk)
{
#ifdef CONFIG_VIRT_CPU_ACCOUNTING
	/* On hard|softirq exit we always account to hard|softirq cputime */
	__vtime_account_system(tsk);
#endif
#ifdef CONFIG_IRQ_TIME_ACCOUNTING
	vtime_account(tsk);
#endif
}

#endif /* _LINUX_KERNEL_VTIME_H */
