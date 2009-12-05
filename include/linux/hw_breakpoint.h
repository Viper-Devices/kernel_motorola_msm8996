#ifndef _LINUX_HW_BREAKPOINT_H
#define _LINUX_HW_BREAKPOINT_H

enum {
	HW_BREAKPOINT_LEN_1 = 1,
	HW_BREAKPOINT_LEN_2 = 2,
	HW_BREAKPOINT_LEN_4 = 4,
	HW_BREAKPOINT_LEN_8 = 8,
};

enum {
	HW_BREAKPOINT_R = 1,
	HW_BREAKPOINT_W = 2,
	HW_BREAKPOINT_X = 4,
};

#ifdef __KERNEL__

#include <linux/perf_event.h>

#ifdef CONFIG_HAVE_HW_BREAKPOINT

/* As it's for in-kernel or ptrace use, we want it to be pinned */
#define DEFINE_BREAKPOINT_ATTR(name)	\
struct perf_event_attr name = {		\
	.type = PERF_TYPE_BREAKPOINT,	\
	.size = sizeof(name),		\
	.pinned = 1,			\
};

static inline void hw_breakpoint_init(struct perf_event_attr *attr)
{
	attr->type = PERF_TYPE_BREAKPOINT;
	attr->size = sizeof(*attr);
	attr->pinned = 1;
}

static inline unsigned long hw_breakpoint_addr(struct perf_event *bp)
{
	return bp->attr.bp_addr;
}

static inline int hw_breakpoint_type(struct perf_event *bp)
{
	return bp->attr.bp_type;
}

static inline int hw_breakpoint_len(struct perf_event *bp)
{
	return bp->attr.bp_len;
}

extern struct perf_event *
register_user_hw_breakpoint(struct perf_event_attr *attr,
			    perf_callback_t triggered,
			    struct task_struct *tsk);

/* FIXME: only change from the attr, and don't unregister */
extern struct perf_event *
modify_user_hw_breakpoint(struct perf_event *bp, struct perf_event_attr *attr);

/*
 * Kernel breakpoints are not associated with any particular thread.
 */
extern struct perf_event *
register_wide_hw_breakpoint_cpu(struct perf_event_attr *attr,
				perf_callback_t triggered,
				int cpu);

extern struct perf_event **
register_wide_hw_breakpoint(struct perf_event_attr *attr,
			    perf_callback_t triggered);

extern int register_perf_hw_breakpoint(struct perf_event *bp);
extern int __register_perf_hw_breakpoint(struct perf_event *bp);
extern void unregister_hw_breakpoint(struct perf_event *bp);
extern void unregister_wide_hw_breakpoint(struct perf_event **cpu_events);

extern int reserve_bp_slot(struct perf_event *bp);
extern void release_bp_slot(struct perf_event *bp);

extern void flush_ptrace_hw_breakpoint(struct task_struct *tsk);

static inline struct arch_hw_breakpoint *counter_arch_bp(struct perf_event *bp)
{
	return &bp->hw.info;
}

#else /* !CONFIG_HAVE_HW_BREAKPOINT */

static inline struct perf_event *
register_user_hw_breakpoint(struct perf_event_attr *attr,
			    perf_callback_t triggered,
			    struct task_struct *tsk)	{ return NULL; }
static inline struct perf_event *
modify_user_hw_breakpoint(struct perf_event *bp,
			  struct perf_event_attr *attr)	{ return NULL; }
static inline struct perf_event *
register_wide_hw_breakpoint_cpu(struct perf_event_attr *attr,
				perf_callback_t triggered,
				int cpu)		{ return NULL; }
static inline struct perf_event **
register_wide_hw_breakpoint(struct perf_event_attr *attr,
			    perf_callback_t triggered)	{ return NULL; }
static inline int
register_perf_hw_breakpoint(struct perf_event *bp)	{ return -ENOSYS; }
static inline int
__register_perf_hw_breakpoint(struct perf_event *bp) 	{ return -ENOSYS; }
static inline void unregister_hw_breakpoint(struct perf_event *bp)	{ }
static inline void
unregister_wide_hw_breakpoint(struct perf_event **cpu_events)		{ }
static inline int
reserve_bp_slot(struct perf_event *bp)			{return -ENOSYS; }
static inline void release_bp_slot(struct perf_event *bp) 		{ }

static inline void flush_ptrace_hw_breakpoint(struct task_struct *tsk)	{ }

static inline struct arch_hw_breakpoint *counter_arch_bp(struct perf_event *bp)
{
	return NULL;
}

#endif /* CONFIG_HAVE_HW_BREAKPOINT */
#endif /* __KERNEL__ */

#endif /* _LINUX_HW_BREAKPOINT_H */
