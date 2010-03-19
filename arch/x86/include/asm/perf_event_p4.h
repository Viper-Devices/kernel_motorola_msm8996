/*
 * Netburst Perfomance Events (P4, old Xeon)
 */

#ifndef PERF_EVENT_P4_H
#define PERF_EVENT_P4_H

#include <linux/cpu.h>
#include <linux/bitops.h>

/*
 * NetBurst has perfomance MSRs shared between
 * threads if HT is turned on, ie for both logical
 * processors (mem: in turn in Atom with HT support
 * perf-MSRs are not shared and every thread has its
 * own perf-MSRs set)
 */
#define ARCH_P4_TOTAL_ESCR		(46)
#define ARCH_P4_RESERVED_ESCR		(2) /* IQ_ESCR(0,1) not always present */
#define ARCH_P4_MAX_ESCR		(ARCH_P4_TOTAL_ESCR - ARCH_P4_RESERVED_ESCR)
#define ARCH_P4_MAX_CCCR		(18)
#define ARCH_P4_MAX_COUNTER		(ARCH_P4_MAX_CCCR / 2)

#define P4_EVNTSEL_EVENT_MASK		0x7e000000U
#define P4_EVNTSEL_EVENT_SHIFT		25
#define P4_EVNTSEL_EVENTMASK_MASK	0x01fffe00U
#define P4_EVNTSEL_EVENTMASK_SHIFT	9
#define P4_EVNTSEL_TAG_MASK		0x000001e0U
#define P4_EVNTSEL_TAG_SHIFT		5
#define P4_EVNTSEL_TAG_ENABLE		0x00000010U
#define P4_EVNTSEL_T0_OS		0x00000008U
#define P4_EVNTSEL_T0_USR		0x00000004U
#define P4_EVNTSEL_T1_OS		0x00000002U
#define P4_EVNTSEL_T1_USR		0x00000001U

/* Non HT mask */
#define P4_EVNTSEL_MASK				\
	(P4_EVNTSEL_EVENT_MASK		|	\
	P4_EVNTSEL_EVENTMASK_MASK	|	\
	P4_EVNTSEL_TAG_MASK		|	\
	P4_EVNTSEL_TAG_ENABLE		|	\
	P4_EVNTSEL_T0_OS		|	\
	P4_EVNTSEL_T0_USR)

/* HT mask */
#define P4_EVNTSEL_MASK_HT			\
	(P4_EVNTSEL_MASK		|	\
	P4_EVNTSEL_T1_OS		|	\
	P4_EVNTSEL_T1_USR)

#define P4_CCCR_OVF			0x80000000U
#define P4_CCCR_CASCADE			0x40000000U
#define P4_CCCR_OVF_PMI_T0		0x04000000U
#define P4_CCCR_OVF_PMI_T1		0x08000000U
#define P4_CCCR_FORCE_OVF		0x02000000U
#define P4_CCCR_EDGE			0x01000000U
#define P4_CCCR_THRESHOLD_MASK		0x00f00000U
#define P4_CCCR_THRESHOLD_SHIFT		20
#define P4_CCCR_THRESHOLD(v)		((v) << P4_CCCR_THRESHOLD_SHIFT)
#define P4_CCCR_COMPLEMENT		0x00080000U
#define P4_CCCR_COMPARE			0x00040000U
#define P4_CCCR_ESCR_SELECT_MASK	0x0000e000U
#define P4_CCCR_ESCR_SELECT_SHIFT	13
#define P4_CCCR_ENABLE			0x00001000U
#define P4_CCCR_THREAD_SINGLE		0x00010000U
#define P4_CCCR_THREAD_BOTH		0x00020000U
#define P4_CCCR_THREAD_ANY		0x00030000U
#define P4_CCCR_RESERVED		0x00000fffU

/* Non HT mask */
#define P4_CCCR_MASK				\
	(P4_CCCR_OVF			|	\
	P4_CCCR_CASCADE			|	\
	P4_CCCR_OVF_PMI_T0		|	\
	P4_CCCR_FORCE_OVF		|	\
	P4_CCCR_EDGE			|	\
	P4_CCCR_THRESHOLD_MASK		|	\
	P4_CCCR_COMPLEMENT		|	\
	P4_CCCR_COMPARE			|	\
	P4_CCCR_ESCR_SELECT_MASK	|	\
	P4_CCCR_ENABLE)

/* HT mask */
#define P4_CCCR_MASK_HT				\
	(P4_CCCR_MASK			|	\
	P4_CCCR_THREAD_ANY)

/*
 * format is 32 bit: ee ss aa aa
 * where
 *	ee - 8 bit event
 *	ss - 8 bit selector
 *	aa aa - 16 bits reserved for tags/attributes
 */
#define P4_EVENT_PACK(event, selector)		(((event) << 24) | ((selector) << 16))
#define P4_EVENT_UNPACK_EVENT(packed)		(((packed) >> 24) & 0xff)
#define P4_EVENT_UNPACK_SELECTOR(packed)	(((packed) >> 16) & 0xff)
#define P4_EVENT_PACK_ATTR(attr)		((attr))
#define P4_EVENT_UNPACK_ATTR(packed)		((packed) & 0xffff)
#define P4_MAKE_EVENT_ATTR(class, name, bit)	class##_##name = (1 << bit)
#define P4_EVENT_ATTR(class, name)		class##_##name
#define P4_EVENT_ATTR_STR(class, name)		__stringify(class##_##name)

/*
 * config field is 64bit width and consists of
 * HT << 63 | ESCR << 32 | CCCR
 * where HT is HyperThreading bit (since ESCR
 * has it reserved we may use it for own purpose)
 *
 * note that this is NOT the addresses of respective
 * ESCR and CCCR but rather an only packed value should
 * be unpacked and written to a proper addresses
 *
 * the base idea is to pack as much info as
 * possible
 */
#define p4_config_pack_escr(v)		(((u64)(v)) << 32)
#define p4_config_pack_cccr(v)		(((u64)(v)) & 0xffffffffULL)
#define p4_config_unpack_escr(v)	(((u64)(v)) >> 32)
#define p4_config_unpack_cccr(v)	(((u64)(v)) & 0xfffff000ULL)

#define p4_config_unpack_emask(v)			\
	({						\
		u32 t = p4_config_unpack_escr((v));	\
		t  &= P4_EVNTSEL_EVENTMASK_MASK;	\
		t >>= P4_EVNTSEL_EVENTMASK_SHIFT;	\
		t;					\
	})

#define p4_config_unpack_key(v)		(((u64)(v)) & P4_CCCR_RESERVED)

#define P4_CONFIG_HT_SHIFT		63
#define P4_CONFIG_HT			(1ULL << P4_CONFIG_HT_SHIFT)

static inline u32 p4_config_unpack_opcode(u64 config)
{
	u32 e, s;

	/*
	 * we don't care about HT presence here since
	 * event opcode doesn't depend on it
	 */
	e = (p4_config_unpack_escr(config) & P4_EVNTSEL_EVENT_MASK) >> P4_EVNTSEL_EVENT_SHIFT;
	s = (p4_config_unpack_cccr(config) & P4_CCCR_ESCR_SELECT_MASK) >> P4_CCCR_ESCR_SELECT_SHIFT;

	return P4_EVENT_PACK(e, s);
}

static inline bool p4_is_event_cascaded(u64 config)
{
	u32 cccr = p4_config_unpack_cccr(config);
	return !!(cccr & P4_CCCR_CASCADE);
}

static inline int p4_ht_config_thread(u64 config)
{
	return !!(config & P4_CONFIG_HT);
}

static inline u64 p4_set_ht_bit(u64 config)
{
	return config | P4_CONFIG_HT;
}

static inline u64 p4_clear_ht_bit(u64 config)
{
	return config & ~P4_CONFIG_HT;
}

static inline int p4_ht_active(void)
{
#ifdef CONFIG_SMP
	return smp_num_siblings > 1;
#endif
	return 0;
}

static inline int p4_ht_thread(int cpu)
{
#ifdef CONFIG_SMP
	if (smp_num_siblings == 2)
		return cpu != cpumask_first(__get_cpu_var(cpu_sibling_map));
#endif
	return 0;
}

static inline int p4_should_swap_ts(u64 config, int cpu)
{
	return p4_ht_config_thread(config) ^ p4_ht_thread(cpu);
}

static inline u32 p4_default_cccr_conf(int cpu)
{
	/*
	 * Note that P4_CCCR_THREAD_ANY is "required" on
	 * non-HT machines (on HT machines we count TS events
	 * regardless the state of second logical processor
	 */
	u32 cccr = P4_CCCR_THREAD_ANY;

	if (!p4_ht_thread(cpu))
		cccr |= P4_CCCR_OVF_PMI_T0;
	else
		cccr |= P4_CCCR_OVF_PMI_T1;

	return cccr;
}

static inline u32 p4_default_escr_conf(int cpu, int exclude_os, int exclude_usr)
{
	u32 escr = 0;

	if (!p4_ht_thread(cpu)) {
		if (!exclude_os)
			escr |= P4_EVNTSEL_T0_OS;
		if (!exclude_usr)
			escr |= P4_EVNTSEL_T0_USR;
	} else {
		if (!exclude_os)
			escr |= P4_EVNTSEL_T1_OS;
		if (!exclude_usr)
			escr |= P4_EVNTSEL_T1_USR;
	}

	return escr;
}

/*
 * Comments below the event represent ESCR restriction
 * for this event and counter index per ESCR
 *
 * MSR_P4_IQ_ESCR0 and MSR_P4_IQ_ESCR1 are available only on early
 * processor builds (family 0FH, models 01H-02H). These MSRs
 * are not available on later versions, so that we don't use
 * them completely
 *
 * Also note that CCCR1 do not have P4_CCCR_ENABLE bit properly
 * working so that we should not use this CCCR and respective
 * counter as result
 */
#define P4_TC_DELIVER_MODE		P4_EVENT_PACK(0x01, 0x01)
	/*
	 * MSR_P4_TC_ESCR0:	4, 5
	 * MSR_P4_TC_ESCR1:	6, 7
	 */

#define P4_BPU_FETCH_REQUEST		P4_EVENT_PACK(0x03, 0x00)
	/*
	 * MSR_P4_BPU_ESCR0:	0, 1
	 * MSR_P4_BPU_ESCR1:	2, 3
	 */

#define P4_ITLB_REFERENCE		P4_EVENT_PACK(0x18, 0x03)
	/*
	 * MSR_P4_ITLB_ESCR0:	0, 1
	 * MSR_P4_ITLB_ESCR1:	2, 3
	 */

#define P4_MEMORY_CANCEL		P4_EVENT_PACK(0x02, 0x05)
	/*
	 * MSR_P4_DAC_ESCR0:	8, 9
	 * MSR_P4_DAC_ESCR1:	10, 11
	 */

#define P4_MEMORY_COMPLETE		P4_EVENT_PACK(0x08, 0x02)
	/*
	 * MSR_P4_SAAT_ESCR0:	8, 9
	 * MSR_P4_SAAT_ESCR1:	10, 11
	 */

#define P4_LOAD_PORT_REPLAY		P4_EVENT_PACK(0x04, 0x02)
	/*
	 * MSR_P4_SAAT_ESCR0:	8, 9
	 * MSR_P4_SAAT_ESCR1:	10, 11
	 */

#define P4_STORE_PORT_REPLAY		P4_EVENT_PACK(0x05, 0x02)
	/*
	 * MSR_P4_SAAT_ESCR0:	8, 9
	 * MSR_P4_SAAT_ESCR1:	10, 11
	 */

#define P4_MOB_LOAD_REPLAY		P4_EVENT_PACK(0x03, 0x02)
	/*
	 * MSR_P4_MOB_ESCR0:	0, 1
	 * MSR_P4_MOB_ESCR1:	2, 3
	 */

#define P4_PAGE_WALK_TYPE		P4_EVENT_PACK(0x01, 0x04)
	/*
	 * MSR_P4_PMH_ESCR0:	0, 1
	 * MSR_P4_PMH_ESCR1:	2, 3
	 */

#define P4_BSQ_CACHE_REFERENCE		P4_EVENT_PACK(0x0c, 0x07)
	/*
	 * MSR_P4_BSU_ESCR0:	0, 1
	 * MSR_P4_BSU_ESCR1:	2, 3
	 */

#define P4_IOQ_ALLOCATION		P4_EVENT_PACK(0x03, 0x06)
	/*
	 * MSR_P4_FSB_ESCR0:	0, 1
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_IOQ_ACTIVE_ENTRIES		P4_EVENT_PACK(0x1a, 0x06)
	/*
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_FSB_DATA_ACTIVITY		P4_EVENT_PACK(0x17, 0x06)
	/*
	 * MSR_P4_FSB_ESCR0:	0, 1
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_BSQ_ALLOCATION		P4_EVENT_PACK(0x05, 0x07)
	/*
	 * MSR_P4_BSU_ESCR0:	0, 1
	 */

#define P4_BSQ_ACTIVE_ENTRIES		P4_EVENT_PACK(0x06, 0x07)
	/*
	 * NOTE: no ESCR name in docs, it's guessed
	 * MSR_P4_BSU_ESCR1:	2, 3
	 */

#define P4_SSE_INPUT_ASSIST		P4_EVENT_PACK(0x34, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_PACKED_SP_UOP		P4_EVENT_PACK(0x08, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_PACKED_DP_UOP		P4_EVENT_PACK(0x0c, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_SCALAR_SP_UOP		P4_EVENT_PACK(0x0a, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_SCALAR_DP_UOP		P4_EVENT_PACK(0x0e, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_64BIT_MMX_UOP		P4_EVENT_PACK(0x02, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_128BIT_MMX_UOP		P4_EVENT_PACK(0x1a, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_X87_FP_UOP			P4_EVENT_PACK(0x04, 0x01)
	/*
	 * MSR_P4_FIRM_ESCR0:	8, 9
	 * MSR_P4_FIRM_ESCR1:	10, 11
	 */

#define P4_TC_MISC			P4_EVENT_PACK(0x06, 0x01)
	/*
	 * MSR_P4_TC_ESCR0:	4, 5
	 * MSR_P4_TC_ESCR1:	6, 7
	 */

#define P4_GLOBAL_POWER_EVENTS		P4_EVENT_PACK(0x13, 0x06)
	/*
	 * MSR_P4_FSB_ESCR0:	0, 1
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_TC_MS_XFER			P4_EVENT_PACK(0x05, 0x00)
	/*
	 * MSR_P4_MS_ESCR0:	4, 5
	 * MSR_P4_MS_ESCR1:	6, 7
	 */

#define P4_UOP_QUEUE_WRITES		P4_EVENT_PACK(0x09, 0x00)
	/*
	 * MSR_P4_MS_ESCR0:	4, 5
	 * MSR_P4_MS_ESCR1:	6, 7
	 */

#define P4_RETIRED_MISPRED_BRANCH_TYPE	P4_EVENT_PACK(0x05, 0x02)
	/*
	 * MSR_P4_TBPU_ESCR0:	4, 5
	 * MSR_P4_TBPU_ESCR1:	6, 7
	 */

#define P4_RETIRED_BRANCH_TYPE		P4_EVENT_PACK(0x04, 0x02)
	/*
	 * MSR_P4_TBPU_ESCR0:	4, 5
	 * MSR_P4_TBPU_ESCR1:	6, 7
	 */

#define P4_RESOURCE_STALL		P4_EVENT_PACK(0x01, 0x01)
	/*
	 * MSR_P4_ALF_ESCR0:	12, 13, 16
	 * MSR_P4_ALF_ESCR1:	14, 15, 17
	 */

#define P4_WC_BUFFER			P4_EVENT_PACK(0x05, 0x05)
	/*
	 * MSR_P4_DAC_ESCR0:	8, 9
	 * MSR_P4_DAC_ESCR1:	10, 11
	 */

#define P4_B2B_CYCLES			P4_EVENT_PACK(0x16, 0x03)
	/*
	 * MSR_P4_FSB_ESCR0:	0, 1
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_BNR				P4_EVENT_PACK(0x08, 0x03)
	/*
	 * MSR_P4_FSB_ESCR0:	0, 1
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_SNOOP			P4_EVENT_PACK(0x06, 0x03)
	/*
	 * MSR_P4_FSB_ESCR0:	0, 1
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_RESPONSE			P4_EVENT_PACK(0x04, 0x03)
	/*
	 * MSR_P4_FSB_ESCR0:	0, 1
	 * MSR_P4_FSB_ESCR1:	2, 3
	 */

#define P4_FRONT_END_EVENT		P4_EVENT_PACK(0x08, 0x05)
	/*
	 * MSR_P4_CRU_ESCR2:	12, 13, 16
	 * MSR_P4_CRU_ESCR3:	14, 15, 17
	 */

#define P4_EXECUTION_EVENT		P4_EVENT_PACK(0x0c, 0x05)
	/*
	 * MSR_P4_CRU_ESCR2:	12, 13, 16
	 * MSR_P4_CRU_ESCR3:	14, 15, 17
	 */

#define P4_REPLAY_EVENT			P4_EVENT_PACK(0x09, 0x05)
	/*
	 * MSR_P4_CRU_ESCR2:	12, 13, 16
	 * MSR_P4_CRU_ESCR3:	14, 15, 17
	 */

#define P4_INSTR_RETIRED		P4_EVENT_PACK(0x02, 0x04)
	/*
	 * MSR_P4_CRU_ESCR0:	12, 13, 16
	 * MSR_P4_CRU_ESCR1:	14, 15, 17
	 */

#define P4_UOPS_RETIRED			P4_EVENT_PACK(0x01, 0x04)
	/*
	 * MSR_P4_CRU_ESCR0:	12, 13, 16
	 * MSR_P4_CRU_ESCR1:	14, 15, 17
	 */

#define P4_UOP_TYPE			P4_EVENT_PACK(0x02, 0x02)
	/*
	 * MSR_P4_RAT_ESCR0:	12, 13, 16
	 * MSR_P4_RAT_ESCR1:	14, 15, 17
	 */

#define P4_BRANCH_RETIRED		P4_EVENT_PACK(0x06, 0x05)
	/*
	 * MSR_P4_CRU_ESCR2:	12, 13, 16
	 * MSR_P4_CRU_ESCR3:	14, 15, 17
	 */

#define P4_MISPRED_BRANCH_RETIRED	P4_EVENT_PACK(0x03, 0x04)
	/*
	 * MSR_P4_CRU_ESCR0:	12, 13, 16
	 * MSR_P4_CRU_ESCR1:	14, 15, 17
	 */

#define P4_X87_ASSIST			P4_EVENT_PACK(0x03, 0x05)
	/*
	 * MSR_P4_CRU_ESCR2:	12, 13, 16
	 * MSR_P4_CRU_ESCR3:	14, 15, 17
	 */

#define P4_MACHINE_CLEAR		P4_EVENT_PACK(0x02, 0x05)
	/*
	 * MSR_P4_CRU_ESCR2:	12, 13, 16
	 * MSR_P4_CRU_ESCR3:	14, 15, 17
	 */

#define P4_INSTR_COMPLETED		P4_EVENT_PACK(0x07, 0x04)
	/*
	 * MSR_P4_CRU_ESCR0:	12, 13, 16
	 * MSR_P4_CRU_ESCR1:	14, 15, 17
	 */

/*
 * a caller should use P4_EVENT_ATTR helper to
 * pick the attribute needed, for example
 *
 *	P4_EVENT_ATTR(P4_TC_DELIVER_MODE, DD)
 */
enum P4_EVENTS_ATTR {
	P4_MAKE_EVENT_ATTR(P4_TC_DELIVER_MODE, DD, 0),
	P4_MAKE_EVENT_ATTR(P4_TC_DELIVER_MODE, DB, 1),
	P4_MAKE_EVENT_ATTR(P4_TC_DELIVER_MODE, DI, 2),
	P4_MAKE_EVENT_ATTR(P4_TC_DELIVER_MODE, BD, 3),
	P4_MAKE_EVENT_ATTR(P4_TC_DELIVER_MODE, BB, 4),
	P4_MAKE_EVENT_ATTR(P4_TC_DELIVER_MODE, BI, 5),
	P4_MAKE_EVENT_ATTR(P4_TC_DELIVER_MODE, ID, 6),

	P4_MAKE_EVENT_ATTR(P4_BPU_FETCH_REQUEST, TCMISS, 0),

	P4_MAKE_EVENT_ATTR(P4_ITLB_REFERENCE, HIT, 0),
	P4_MAKE_EVENT_ATTR(P4_ITLB_REFERENCE, MISS, 1),
	P4_MAKE_EVENT_ATTR(P4_ITLB_REFERENCE, HIT_UK, 2),

	P4_MAKE_EVENT_ATTR(P4_MEMORY_CANCEL, ST_RB_FULL, 2),
	P4_MAKE_EVENT_ATTR(P4_MEMORY_CANCEL, 64K_CONF, 3),

	P4_MAKE_EVENT_ATTR(P4_MEMORY_COMPLETE, LSC, 0),
	P4_MAKE_EVENT_ATTR(P4_MEMORY_COMPLETE, SSC, 1),

	P4_MAKE_EVENT_ATTR(P4_LOAD_PORT_REPLAY, SPLIT_LD, 1),

	P4_MAKE_EVENT_ATTR(P4_STORE_PORT_REPLAY, SPLIT_ST, 1),

	P4_MAKE_EVENT_ATTR(P4_MOB_LOAD_REPLAY, NO_STA, 1),
	P4_MAKE_EVENT_ATTR(P4_MOB_LOAD_REPLAY, NO_STD, 3),
	P4_MAKE_EVENT_ATTR(P4_MOB_LOAD_REPLAY, PARTIAL_DATA, 4),
	P4_MAKE_EVENT_ATTR(P4_MOB_LOAD_REPLAY, UNALGN_ADDR, 5),

	P4_MAKE_EVENT_ATTR(P4_PAGE_WALK_TYPE, DTMISS, 0),
	P4_MAKE_EVENT_ATTR(P4_PAGE_WALK_TYPE, ITMISS, 1),

	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_2ndL_HITS, 0),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_2ndL_HITE, 1),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_2ndL_HITM, 2),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_3rdL_HITS, 3),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_3rdL_HITE, 4),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_3rdL_HITM, 5),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_2ndL_MISS, 8),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, RD_3rdL_MISS, 9),
	P4_MAKE_EVENT_ATTR(P4_BSQ_CACHE_REFERENCE, WR_2ndL_MISS, 10),

	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, DEFAULT, 0),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, ALL_READ, 5),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, ALL_WRITE, 6),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, MEM_UC, 7),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, MEM_WC, 8),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, MEM_WT, 9),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, MEM_WP, 10),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, MEM_WB, 11),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, OWN, 13),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, OTHER, 14),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ALLOCATION, PREFETCH, 15),

	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, DEFAULT, 0),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, ALL_READ, 5),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, ALL_WRITE, 6),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, MEM_UC, 7),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, MEM_WC, 8),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, MEM_WT, 9),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, MEM_WP, 10),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, MEM_WB, 11),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, OWN, 13),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, OTHER, 14),
	P4_MAKE_EVENT_ATTR(P4_IOQ_ACTIVE_ENTRIES, PREFETCH, 15),

	P4_MAKE_EVENT_ATTR(P4_FSB_DATA_ACTIVITY, DRDY_DRV, 0),
	P4_MAKE_EVENT_ATTR(P4_FSB_DATA_ACTIVITY, DRDY_OWN, 1),
	P4_MAKE_EVENT_ATTR(P4_FSB_DATA_ACTIVITY, DRDY_OTHER, 2),
	P4_MAKE_EVENT_ATTR(P4_FSB_DATA_ACTIVITY, DBSY_DRV, 3),
	P4_MAKE_EVENT_ATTR(P4_FSB_DATA_ACTIVITY, DBSY_OWN, 4),
	P4_MAKE_EVENT_ATTR(P4_FSB_DATA_ACTIVITY, DBSY_OTHER, 5),

	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_TYPE0, 0),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_TYPE1, 1),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_LEN0, 2),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_LEN1, 3),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_IO_TYPE, 5),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_LOCK_TYPE, 6),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_CACHE_TYPE, 7),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_SPLIT_TYPE, 8),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_DEM_TYPE, 9),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, REQ_ORD_TYPE, 10),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, MEM_TYPE0, 11),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, MEM_TYPE1, 12),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ALLOCATION, MEM_TYPE2, 13),

	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_TYPE0, 0),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_TYPE1, 1),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_LEN0, 2),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_LEN1, 3),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_IO_TYPE, 5),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_LOCK_TYPE, 6),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_CACHE_TYPE, 7),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_SPLIT_TYPE, 8),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_DEM_TYPE, 9),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, REQ_ORD_TYPE, 10),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, MEM_TYPE0, 11),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, MEM_TYPE1, 12),
	P4_MAKE_EVENT_ATTR(P4_BSQ_ACTIVE_ENTRIES, MEM_TYPE2, 13),

	P4_MAKE_EVENT_ATTR(P4_SSE_INPUT_ASSIST, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_PACKED_SP_UOP, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_PACKED_DP_UOP, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_SCALAR_SP_UOP, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_SCALAR_DP_UOP, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_64BIT_MMX_UOP, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_128BIT_MMX_UOP, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_X87_FP_UOP, ALL, 15),

	P4_MAKE_EVENT_ATTR(P4_TC_MISC, FLUSH, 4),

	P4_MAKE_EVENT_ATTR(P4_GLOBAL_POWER_EVENTS, RUNNING, 0),

	P4_MAKE_EVENT_ATTR(P4_TC_MS_XFER, CISC, 0),

	P4_MAKE_EVENT_ATTR(P4_UOP_QUEUE_WRITES, FROM_TC_BUILD, 0),
	P4_MAKE_EVENT_ATTR(P4_UOP_QUEUE_WRITES, FROM_TC_DELIVER, 1),
	P4_MAKE_EVENT_ATTR(P4_UOP_QUEUE_WRITES, FROM_ROM, 2),

	P4_MAKE_EVENT_ATTR(P4_RETIRED_MISPRED_BRANCH_TYPE, CONDITIONAL, 1),
	P4_MAKE_EVENT_ATTR(P4_RETIRED_MISPRED_BRANCH_TYPE, CALL, 2),
	P4_MAKE_EVENT_ATTR(P4_RETIRED_MISPRED_BRANCH_TYPE, RETURN, 3),
	P4_MAKE_EVENT_ATTR(P4_RETIRED_MISPRED_BRANCH_TYPE, INDIRECT, 4),

	P4_MAKE_EVENT_ATTR(P4_RETIRED_BRANCH_TYPE, CONDITIONAL, 1),
	P4_MAKE_EVENT_ATTR(P4_RETIRED_BRANCH_TYPE, CALL, 2),
	P4_MAKE_EVENT_ATTR(P4_RETIRED_BRANCH_TYPE, RETURN, 3),
	P4_MAKE_EVENT_ATTR(P4_RETIRED_BRANCH_TYPE, INDIRECT, 4),

	P4_MAKE_EVENT_ATTR(P4_RESOURCE_STALL, SBFULL, 5),

	P4_MAKE_EVENT_ATTR(P4_WC_BUFFER, WCB_EVICTS, 0),
	P4_MAKE_EVENT_ATTR(P4_WC_BUFFER, WCB_FULL_EVICTS, 1),

	P4_MAKE_EVENT_ATTR(P4_FRONT_END_EVENT, NBOGUS, 0),
	P4_MAKE_EVENT_ATTR(P4_FRONT_END_EVENT, BOGUS, 1),

	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, NBOGUS0, 0),
	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, NBOGUS1, 1),
	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, NBOGUS2, 2),
	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, NBOGUS3, 3),
	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, BOGUS0, 4),
	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, BOGUS1, 5),
	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, BOGUS2, 6),
	P4_MAKE_EVENT_ATTR(P4_EXECUTION_EVENT, BOGUS3, 7),

	P4_MAKE_EVENT_ATTR(P4_REPLAY_EVENT, NBOGUS, 0),
	P4_MAKE_EVENT_ATTR(P4_REPLAY_EVENT, BOGUS, 1),

	P4_MAKE_EVENT_ATTR(P4_INSTR_RETIRED, NBOGUSNTAG, 0),
	P4_MAKE_EVENT_ATTR(P4_INSTR_RETIRED, NBOGUSTAG, 1),
	P4_MAKE_EVENT_ATTR(P4_INSTR_RETIRED, BOGUSNTAG, 2),
	P4_MAKE_EVENT_ATTR(P4_INSTR_RETIRED, BOGUSTAG, 3),

	P4_MAKE_EVENT_ATTR(P4_UOPS_RETIRED, NBOGUS, 0),
	P4_MAKE_EVENT_ATTR(P4_UOPS_RETIRED, BOGUS, 1),

	P4_MAKE_EVENT_ATTR(P4_UOP_TYPE, TAGLOADS, 1),
	P4_MAKE_EVENT_ATTR(P4_UOP_TYPE, TAGSTORES, 2),

	P4_MAKE_EVENT_ATTR(P4_BRANCH_RETIRED, MMNP, 0),
	P4_MAKE_EVENT_ATTR(P4_BRANCH_RETIRED, MMNM, 1),
	P4_MAKE_EVENT_ATTR(P4_BRANCH_RETIRED, MMTP, 2),
	P4_MAKE_EVENT_ATTR(P4_BRANCH_RETIRED, MMTM, 3),

	P4_MAKE_EVENT_ATTR(P4_MISPRED_BRANCH_RETIRED, NBOGUS, 0),

	P4_MAKE_EVENT_ATTR(P4_X87_ASSIST, FPSU, 0),
	P4_MAKE_EVENT_ATTR(P4_X87_ASSIST, FPSO, 1),
	P4_MAKE_EVENT_ATTR(P4_X87_ASSIST, POAO, 2),
	P4_MAKE_EVENT_ATTR(P4_X87_ASSIST, POAU, 3),
	P4_MAKE_EVENT_ATTR(P4_X87_ASSIST, PREA, 4),

	P4_MAKE_EVENT_ATTR(P4_MACHINE_CLEAR, CLEAR, 0),
	P4_MAKE_EVENT_ATTR(P4_MACHINE_CLEAR, MOCLEAR, 1),
	P4_MAKE_EVENT_ATTR(P4_MACHINE_CLEAR, SMCLEAR, 2),

	P4_MAKE_EVENT_ATTR(P4_INSTR_COMPLETED, NBOGUS, 0),
	P4_MAKE_EVENT_ATTR(P4_INSTR_COMPLETED, BOGUS, 1),
};

enum {
	KEY_P4_L1D_OP_READ_RESULT_MISS = PERF_COUNT_HW_MAX,
	KEY_P4_LL_OP_READ_RESULT_MISS,
	KEY_P4_DTLB_OP_READ_RESULT_MISS,
	KEY_P4_DTLB_OP_WRITE_RESULT_MISS,
	KEY_P4_ITLB_OP_READ_RESULT_ACCESS,
	KEY_P4_ITLB_OP_READ_RESULT_MISS,
	KEY_P4_UOP_TYPE,
};

#endif /* PERF_EVENT_P4_H */
