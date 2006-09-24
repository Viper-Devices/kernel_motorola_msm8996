/*
 *  linux/drivers/net/ehea/ehea_phyp.c
 *
 *  eHEA ethernet device driver for IBM eServer System p
 *
 *  (C) Copyright IBM Corp. 2006
 *
 *  Authors:
 *       Christoph Raisch <raisch@de.ibm.com>
 *       Jan-Bernd Themann <themann@de.ibm.com>
 *       Thomas Klein <tklein@de.ibm.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ehea_phyp.h"


static inline u16 get_order_of_qentries(u16 queue_entries)
{
	u8 ld = 1;		/*  logarithmus dualis */
	while (((1U << ld) - 1) < queue_entries)
		ld++;
	return ld - 1;
}

/* Defines for H_CALL H_ALLOC_RESOURCE */
#define H_ALL_RES_TYPE_QP        1
#define H_ALL_RES_TYPE_CQ        2
#define H_ALL_RES_TYPE_EQ        3
#define H_ALL_RES_TYPE_MR        5
#define H_ALL_RES_TYPE_MW        6

static long ehea_hcall_9arg_9ret(unsigned long opcode,
         			 unsigned long arg1, unsigned long arg2,
         			 unsigned long arg3, unsigned long arg4,
         			 unsigned long arg5, unsigned long arg6,
         			 unsigned long arg7, unsigned long arg8,
         			 unsigned long arg9, unsigned long *out1,
         			 unsigned long *out2,unsigned long *out3,
         			 unsigned long *out4,unsigned long *out5,
         			 unsigned long *out6,unsigned long *out7,
         			 unsigned long *out8,unsigned long *out9)
{
	long hret;
	int i, sleep_msecs;

	for (i = 0; i < 5; i++) {
		hret = plpar_hcall_9arg_9ret(opcode,arg1, arg2, arg3, arg4,
					     arg5, arg6, arg7, arg8, arg9, out1,
					     out2, out3, out4, out5, out6, out7,
					     out8, out9);
		if (H_IS_LONG_BUSY(hret)) {
			sleep_msecs = get_longbusy_msecs(hret);
			msleep_interruptible(sleep_msecs);
			continue;
		}

		if (hret < H_SUCCESS)
			ehea_error("op=%lx hret=%lx "
				   "i1=%lx i2=%lx i3=%lx i4=%lx i5=%lx i6=%lx "
				   "i7=%lx i8=%lx i9=%lx "
				   "o1=%lx o2=%lx o3=%lx o4=%lx o5=%lx o6=%lx "
				   "o7=%lx o8=%lx o9=%lx",
				   opcode, hret, arg1, arg2, arg3, arg4, arg5,
				   arg6, arg7, arg8, arg9, *out1, *out2, *out3,
				   *out4, *out5, *out6, *out7, *out8, *out9);
		return hret;
	}
	return H_BUSY;
}

u64 ehea_h_query_ehea_qp(const u64 adapter_handle, const u8 qp_category,
			 const u64 qp_handle, const u64 sel_mask, void *cb_addr)
{
	u64 dummy;

	if ((((u64)cb_addr) & (PAGE_SIZE - 1)) != 0) {
		ehea_error("not on pageboundary");
		return H_PARAMETER;
	}

	return ehea_hcall_9arg_9ret(H_QUERY_HEA_QP,
				    adapter_handle,	        /* R4 */
				    qp_category,	        /* R5 */
				    qp_handle,	                /* R6 */
				    sel_mask,	                /* R7 */
				    virt_to_abs(cb_addr),	/* R8 */
				    0, 0, 0, 0,	                /* R9-R12 */
				    &dummy,                     /* R4 */
				    &dummy,                     /* R5 */
				    &dummy,	                /* R6 */
				    &dummy,	                /* R7 */
				    &dummy,	                /* R8 */
				    &dummy,	                /* R9 */
				    &dummy,	                /* R10 */
				    &dummy,	                /* R11 */
				    &dummy);	                /* R12 */
}

/* input param R5 */
#define H_ALL_RES_QP_EQPO         EHEA_BMASK_IBM(9, 11)
#define H_ALL_RES_QP_QPP          EHEA_BMASK_IBM(12, 12)
#define H_ALL_RES_QP_RQR          EHEA_BMASK_IBM(13, 15)
#define H_ALL_RES_QP_EQEG         EHEA_BMASK_IBM(16, 16)
#define H_ALL_RES_QP_LL_QP        EHEA_BMASK_IBM(17, 17)
#define H_ALL_RES_QP_DMA128       EHEA_BMASK_IBM(19, 19)
#define H_ALL_RES_QP_HSM          EHEA_BMASK_IBM(20, 21)
#define H_ALL_RES_QP_SIGT         EHEA_BMASK_IBM(22, 23)
#define H_ALL_RES_QP_TENURE       EHEA_BMASK_IBM(48, 55)
#define H_ALL_RES_QP_RES_TYP      EHEA_BMASK_IBM(56, 63)

/* input param R9  */
#define H_ALL_RES_QP_TOKEN        EHEA_BMASK_IBM(0, 31)
#define H_ALL_RES_QP_PD           EHEA_BMASK_IBM(32,63)

/* input param R10 */
#define H_ALL_RES_QP_MAX_SWQE     EHEA_BMASK_IBM(4, 7)
#define H_ALL_RES_QP_MAX_R1WQE    EHEA_BMASK_IBM(12, 15)
#define H_ALL_RES_QP_MAX_R2WQE    EHEA_BMASK_IBM(20, 23)
#define H_ALL_RES_QP_MAX_R3WQE    EHEA_BMASK_IBM(28, 31)
/* Max Send Scatter Gather Elements */
#define H_ALL_RES_QP_MAX_SSGE     EHEA_BMASK_IBM(37, 39)
#define H_ALL_RES_QP_MAX_R1SGE    EHEA_BMASK_IBM(45, 47)
/* Max Receive SG Elements RQ1 */
#define H_ALL_RES_QP_MAX_R2SGE    EHEA_BMASK_IBM(53, 55)
#define H_ALL_RES_QP_MAX_R3SGE    EHEA_BMASK_IBM(61, 63)

/* input param R11 */
#define H_ALL_RES_QP_SWQE_IDL     EHEA_BMASK_IBM(0, 7)
/* max swqe immediate data length */
#define H_ALL_RES_QP_PORT_NUM     EHEA_BMASK_IBM(48, 63)

/* input param R12 */
#define H_ALL_RES_QP_TH_RQ2       EHEA_BMASK_IBM(0, 15)
/* Threshold RQ2 */
#define H_ALL_RES_QP_TH_RQ3       EHEA_BMASK_IBM(16, 31)
/* Threshold RQ3 */

/* output param R6 */
#define H_ALL_RES_QP_ACT_SWQE     EHEA_BMASK_IBM(0, 15)
#define H_ALL_RES_QP_ACT_R1WQE    EHEA_BMASK_IBM(16, 31)
#define H_ALL_RES_QP_ACT_R2WQE    EHEA_BMASK_IBM(32, 47)
#define H_ALL_RES_QP_ACT_R3WQE    EHEA_BMASK_IBM(48, 63)

/* output param, R7 */
#define H_ALL_RES_QP_ACT_SSGE     EHEA_BMASK_IBM(0, 7)
#define H_ALL_RES_QP_ACT_R1SGE    EHEA_BMASK_IBM(8, 15)
#define H_ALL_RES_QP_ACT_R2SGE    EHEA_BMASK_IBM(16, 23)
#define H_ALL_RES_QP_ACT_R3SGE    EHEA_BMASK_IBM(24, 31)
#define H_ALL_RES_QP_ACT_SWQE_IDL EHEA_BMASK_IBM(32, 39)

/* output param R8,R9 */
#define H_ALL_RES_QP_SIZE_SQ      EHEA_BMASK_IBM(0, 31)
#define H_ALL_RES_QP_SIZE_RQ1     EHEA_BMASK_IBM(32, 63)
#define H_ALL_RES_QP_SIZE_RQ2     EHEA_BMASK_IBM(0, 31)
#define H_ALL_RES_QP_SIZE_RQ3     EHEA_BMASK_IBM(32, 63)

/* output param R11,R12 */
#define H_ALL_RES_QP_LIOBN_SQ     EHEA_BMASK_IBM(0, 31)
#define H_ALL_RES_QP_LIOBN_RQ1    EHEA_BMASK_IBM(32, 63)
#define H_ALL_RES_QP_LIOBN_RQ2    EHEA_BMASK_IBM(0, 31)
#define H_ALL_RES_QP_LIOBN_RQ3    EHEA_BMASK_IBM(32, 63)

u64 ehea_h_alloc_resource_qp(const u64 adapter_handle,
			     struct ehea_qp_init_attr *init_attr, const u32 pd,
			     u64 *qp_handle, struct h_epas *h_epas)
{
	u64 hret;

	u64 allocate_controls =
	    EHEA_BMASK_SET(H_ALL_RES_QP_EQPO, init_attr->low_lat_rq1 ? 1 : 0)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_QPP, 0)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_RQR, 6)	/* rq1 & rq2 & rq3 */
	    | EHEA_BMASK_SET(H_ALL_RES_QP_EQEG, 0)	/* EQE gen. disabled */
	    | EHEA_BMASK_SET(H_ALL_RES_QP_LL_QP, init_attr->low_lat_rq1)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_DMA128, 0)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_HSM, 0)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_SIGT, init_attr->signalingtype)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_RES_TYP, H_ALL_RES_TYPE_QP);

	u64 r9_reg = EHEA_BMASK_SET(H_ALL_RES_QP_PD, pd)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_TOKEN, init_attr->qp_token);

	u64 max_r10_reg =
	    EHEA_BMASK_SET(H_ALL_RES_QP_MAX_SWQE,
			   get_order_of_qentries(init_attr->max_nr_send_wqes))
	    | EHEA_BMASK_SET(H_ALL_RES_QP_MAX_R1WQE,
			     get_order_of_qentries(init_attr->max_nr_rwqes_rq1))
	    | EHEA_BMASK_SET(H_ALL_RES_QP_MAX_R2WQE,
			     get_order_of_qentries(init_attr->max_nr_rwqes_rq2))
	    | EHEA_BMASK_SET(H_ALL_RES_QP_MAX_R3WQE,
			     get_order_of_qentries(init_attr->max_nr_rwqes_rq3))
	    | EHEA_BMASK_SET(H_ALL_RES_QP_MAX_SSGE, init_attr->wqe_size_enc_sq)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_MAX_R1SGE,
			     init_attr->wqe_size_enc_rq1)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_MAX_R2SGE,
			     init_attr->wqe_size_enc_rq2)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_MAX_R3SGE,
			     init_attr->wqe_size_enc_rq3);

	u64 r11_in =
	    EHEA_BMASK_SET(H_ALL_RES_QP_SWQE_IDL, init_attr->swqe_imm_data_len)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_PORT_NUM, init_attr->port_nr);
	u64 threshold =
	    EHEA_BMASK_SET(H_ALL_RES_QP_TH_RQ2, init_attr->rq2_threshold)
	    | EHEA_BMASK_SET(H_ALL_RES_QP_TH_RQ3, init_attr->rq3_threshold);

	u64 r5_out = 0;
	u64 r6_out = 0;
	u64 r7_out = 0;
	u64 r8_out = 0;
	u64 r9_out = 0;
	u64 g_la_user_out = 0;
	u64 r11_out = 0;
	u64 r12_out = 0;

	hret = ehea_hcall_9arg_9ret(H_ALLOC_HEA_RESOURCE,
				    adapter_handle,		/* R4 */
				    allocate_controls,		/* R5 */
				    init_attr->send_cq_handle,	/* R6 */
				    init_attr->recv_cq_handle,	/* R7 */
				    init_attr->aff_eq_handle,	/* R8 */
				    r9_reg,			/* R9 */
				    max_r10_reg,		/* R10 */
				    r11_in,			/* R11 */
				    threshold,			/* R12 */
				    qp_handle,			/* R4 */
				    &r5_out,			/* R5 */
				    &r6_out,			/* R6 */
				    &r7_out,			/* R7 */
				    &r8_out,			/* R8 */
				    &r9_out,			/* R9 */
				    &g_la_user_out,		/* R10 */
				    &r11_out,			/* R11 */
				    &r12_out);			/* R12 */

	init_attr->qp_nr = (u32)r5_out;

	init_attr->act_nr_send_wqes =
	    (u16)EHEA_BMASK_GET(H_ALL_RES_QP_ACT_SWQE, r6_out);
	init_attr->act_nr_rwqes_rq1 =
	    (u16)EHEA_BMASK_GET(H_ALL_RES_QP_ACT_R1WQE, r6_out);
	init_attr->act_nr_rwqes_rq2 =
	    (u16)EHEA_BMASK_GET(H_ALL_RES_QP_ACT_R2WQE, r6_out);
	init_attr->act_nr_rwqes_rq3 =
	    (u16)EHEA_BMASK_GET(H_ALL_RES_QP_ACT_R3WQE, r6_out);

	init_attr->act_wqe_size_enc_sq = init_attr->wqe_size_enc_sq;
	init_attr->act_wqe_size_enc_rq1 = init_attr->wqe_size_enc_rq1;
	init_attr->act_wqe_size_enc_rq2 = init_attr->wqe_size_enc_rq2;
	init_attr->act_wqe_size_enc_rq3 = init_attr->wqe_size_enc_rq3;

	init_attr->nr_sq_pages =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_SIZE_SQ, r8_out);
	init_attr->nr_rq1_pages =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_SIZE_RQ1, r8_out);
	init_attr->nr_rq2_pages =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_SIZE_RQ2, r9_out);
	init_attr->nr_rq3_pages =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_SIZE_RQ3, r9_out);

	init_attr->liobn_sq =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_LIOBN_SQ, r11_out);
	init_attr->liobn_rq1 =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_LIOBN_RQ1, r11_out);
	init_attr->liobn_rq2 =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_LIOBN_RQ2, r12_out);
	init_attr->liobn_rq3 =
	    (u32)EHEA_BMASK_GET(H_ALL_RES_QP_LIOBN_RQ3, r12_out);

	if (!hret)
		hcp_epas_ctor(h_epas, g_la_user_out, g_la_user_out);

	return hret;
}

u64 ehea_h_alloc_resource_cq(const u64 adapter_handle,
			     struct ehea_cq_attr *cq_attr,
			     u64 *cq_handle, struct h_epas *epas)
{
	u64 hret, dummy, act_nr_of_cqes_out, act_pages_out;
	u64 g_la_privileged_out, g_la_user_out;

	hret = ehea_hcall_9arg_9ret(H_ALLOC_HEA_RESOURCE,
				    adapter_handle,		/* R4 */
				    H_ALL_RES_TYPE_CQ,		/* R5 */
				    cq_attr->eq_handle,		/* R6 */
				    cq_attr->cq_token,		/* R7 */
				    cq_attr->max_nr_of_cqes,	/* R8 */
				    0, 0, 0, 0,			/* R9-R12 */
				    cq_handle,			/* R4 */
				    &dummy,			/* R5 */
				    &dummy,			/* R6 */
				    &act_nr_of_cqes_out,	/* R7 */
				    &act_pages_out,		/* R8 */
				    &g_la_privileged_out,	/* R9 */
				    &g_la_user_out,		/* R10 */
				    &dummy,	                /* R11 */
				    &dummy);	                /* R12 */

	cq_attr->act_nr_of_cqes = act_nr_of_cqes_out;
	cq_attr->nr_pages = act_pages_out;

	if (!hret)
		hcp_epas_ctor(epas, g_la_privileged_out, g_la_user_out);

	return hret;
}

/* Defines for H_CALL H_ALLOC_RESOURCE */
#define H_ALL_RES_TYPE_QP        1
#define H_ALL_RES_TYPE_CQ        2
#define H_ALL_RES_TYPE_EQ        3
#define H_ALL_RES_TYPE_MR        5
#define H_ALL_RES_TYPE_MW        6

/*  input param R5 */
#define H_ALL_RES_EQ_NEQ             EHEA_BMASK_IBM(0, 0)
#define H_ALL_RES_EQ_NON_NEQ_ISN     EHEA_BMASK_IBM(6, 7)
#define H_ALL_RES_EQ_INH_EQE_GEN     EHEA_BMASK_IBM(16, 16)
#define H_ALL_RES_EQ_RES_TYPE        EHEA_BMASK_IBM(56, 63)
/*  input param R6 */
#define H_ALL_RES_EQ_MAX_EQE         EHEA_BMASK_IBM(32, 63)

/*  output param R6 */
#define H_ALL_RES_EQ_LIOBN           EHEA_BMASK_IBM(32, 63)

/*  output param R7 */
#define H_ALL_RES_EQ_ACT_EQE         EHEA_BMASK_IBM(32, 63)

/*  output param R8 */
#define H_ALL_RES_EQ_ACT_PS          EHEA_BMASK_IBM(32, 63)

/*  output param R9 */
#define H_ALL_RES_EQ_ACT_EQ_IST_C    EHEA_BMASK_IBM(30, 31)
#define H_ALL_RES_EQ_ACT_EQ_IST_1    EHEA_BMASK_IBM(40, 63)

/*  output param R10 */
#define H_ALL_RES_EQ_ACT_EQ_IST_2    EHEA_BMASK_IBM(40, 63)

/*  output param R11 */
#define H_ALL_RES_EQ_ACT_EQ_IST_3    EHEA_BMASK_IBM(40, 63)

/*  output param R12 */
#define H_ALL_RES_EQ_ACT_EQ_IST_4    EHEA_BMASK_IBM(40, 63)

u64 ehea_h_alloc_resource_eq(const u64 adapter_handle,
			     struct ehea_eq_attr *eq_attr, u64 *eq_handle)
{
	u64 hret, dummy, eq_liobn, allocate_controls;
	u64 ist1_out, ist2_out, ist3_out, ist4_out;
	u64 act_nr_of_eqes_out, act_pages_out;

	/* resource type */
	allocate_controls =
	    EHEA_BMASK_SET(H_ALL_RES_EQ_RES_TYPE, H_ALL_RES_TYPE_EQ)
	    | EHEA_BMASK_SET(H_ALL_RES_EQ_NEQ, eq_attr->type ? 1 : 0)
	    | EHEA_BMASK_SET(H_ALL_RES_EQ_INH_EQE_GEN, !eq_attr->eqe_gen)
	    | EHEA_BMASK_SET(H_ALL_RES_EQ_NON_NEQ_ISN, 1);

	hret = ehea_hcall_9arg_9ret(H_ALLOC_HEA_RESOURCE,
				    adapter_handle,		/* R4 */
				    allocate_controls,		/* R5 */
				    eq_attr->max_nr_of_eqes,	/* R6 */
				    0, 0, 0, 0, 0, 0,		/* R7-R10 */
				    eq_handle,			/* R4 */
				    &dummy,			/* R5 */
				    &eq_liobn,			/* R6 */
				    &act_nr_of_eqes_out,	/* R7 */
				    &act_pages_out,		/* R8 */
				    &ist1_out,			/* R9 */
				    &ist2_out,			/* R10 */
				    &ist3_out,			/* R11 */
				    &ist4_out);			/* R12 */

	eq_attr->act_nr_of_eqes = act_nr_of_eqes_out;
	eq_attr->nr_pages = act_pages_out;
	eq_attr->ist1 = ist1_out;
	eq_attr->ist2 = ist2_out;
	eq_attr->ist3 = ist3_out;
	eq_attr->ist4 = ist4_out;

	return hret;
}

u64 ehea_h_modify_ehea_qp(const u64 adapter_handle, const u8 cat,
			  const u64 qp_handle, const u64 sel_mask,
			  void *cb_addr, u64 *inv_attr_id, u64 *proc_mask,
			  u16 *out_swr, u16 *out_rwr)
{
	u64 hret, dummy, act_out_swr, act_out_rwr;

	if ((((u64)cb_addr) & (PAGE_SIZE - 1)) != 0) {
		ehea_error("not on page boundary");
		return H_PARAMETER;
	}

	hret = ehea_hcall_9arg_9ret(H_MODIFY_HEA_QP,
				    adapter_handle,		/* R4 */
				    (u64) cat,			/* R5 */
				    qp_handle,			/* R6 */
				    sel_mask,			/* R7 */
				    virt_to_abs(cb_addr),	/* R8 */
				    0, 0, 0, 0,			/* R9-R12 */
				    inv_attr_id,		/* R4 */
				    &dummy,			/* R5 */
				    &dummy,			/* R6 */
				    &act_out_swr,		/* R7 */
				    &act_out_rwr,		/* R8 */
				    proc_mask,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,			/* R11 */
				    &dummy);			/* R12 */
	*out_swr = act_out_swr;
	*out_rwr = act_out_rwr;

	return hret;
}

u64 ehea_h_register_rpage(const u64 adapter_handle, const u8 pagesize,
			  const u8 queue_type, const u64 resource_handle,
			  const u64 log_pageaddr, u64 count)
{
	u64 dummy, reg_control;

	reg_control = EHEA_BMASK_SET(H_REG_RPAGE_PAGE_SIZE, pagesize)
		    | EHEA_BMASK_SET(H_REG_RPAGE_QT, queue_type);

	return ehea_hcall_9arg_9ret(H_REGISTER_HEA_RPAGES,
				    adapter_handle,		/* R4 */
				    reg_control,		/* R5 */
				    resource_handle,		/* R6 */
				    log_pageaddr,		/* R7 */
				    count,			/* R8 */
				    0, 0, 0, 0,			/* R9-R12 */
				    &dummy,			/* R4 */
				    &dummy,			/* R5 */
				    &dummy,			/* R6 */
				    &dummy,			/* R7 */
				    &dummy,			/* R8 */
				    &dummy,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,	                /* R11 */
				    &dummy);	                /* R12 */
}

u64 ehea_h_register_smr(const u64 adapter_handle, const u64 orig_mr_handle,
			const u64 vaddr_in, const u32 access_ctrl, const u32 pd,
			struct ehea_mr *mr)
{
	u64 hret, dummy, lkey_out;

	hret = ehea_hcall_9arg_9ret(H_REGISTER_SMR,
				    adapter_handle       ,          /* R4 */
				    orig_mr_handle,                 /* R5 */
				    vaddr_in,                       /* R6 */
				    (((u64)access_ctrl) << 32ULL),  /* R7 */
				    pd,                             /* R8 */
				    0, 0, 0, 0,			    /* R9-R12 */
				    &mr->handle,                    /* R4 */
				    &dummy,                         /* R5 */
				    &lkey_out,                      /* R6 */
				    &dummy,                         /* R7 */
				    &dummy,                         /* R8 */
				    &dummy,                         /* R9 */
				    &dummy,                         /* R10 */
				    &dummy,                         /* R11 */
				    &dummy);                        /* R12 */
	mr->lkey = (u32)lkey_out;

	return hret;
}

u64 ehea_h_disable_and_get_hea(const u64 adapter_handle, const u64 qp_handle)
{
	u64 hret, dummy, ladr_next_sq_wqe_out;
	u64 ladr_next_rq1_wqe_out, ladr_next_rq2_wqe_out, ladr_next_rq3_wqe_out;

	hret = ehea_hcall_9arg_9ret(H_DISABLE_AND_GET_HEA,
				    adapter_handle,		/* R4 */
				    H_DISABLE_GET_EHEA_WQE_P,	/* R5 */
				    qp_handle,			/* R6 */
				    0, 0, 0, 0, 0, 0,		/* R7-R12 */
				    &ladr_next_sq_wqe_out,	/* R4 */
				    &ladr_next_rq1_wqe_out,	/* R5 */
				    &ladr_next_rq2_wqe_out,	/* R6 */
				    &ladr_next_rq3_wqe_out,	/* R7 */
				    &dummy,			/* R8 */
				    &dummy,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,                     /* R11 */
				    &dummy);                    /* R12 */
	return hret;
}

u64 ehea_h_free_resource(const u64 adapter_handle, const u64 res_handle)
{
	u64 dummy;

	return ehea_hcall_9arg_9ret(H_FREE_RESOURCE,
				    adapter_handle,	   /* R4 */
				    res_handle,            /* R5 */
				    0, 0, 0, 0, 0, 0, 0,   /* R6-R12 */
				    &dummy,                /* R4 */
				    &dummy,                /* R5 */
				    &dummy,                /* R6 */
				    &dummy,                /* R7 */
				    &dummy,                /* R8 */
				    &dummy,                /* R9 */
				    &dummy,		   /* R10 */
				    &dummy,                /* R11 */
				    &dummy);               /* R12 */
}

u64 ehea_h_alloc_resource_mr(const u64 adapter_handle, const u64 vaddr,
			     const u64 length, const u32 access_ctrl,
			     const u32 pd, u64 *mr_handle, u32 *lkey)
{
	u64 hret, dummy, lkey_out;

	hret = ehea_hcall_9arg_9ret(H_ALLOC_HEA_RESOURCE,
				    adapter_handle,		   /* R4 */
				    5,				   /* R5 */
				    vaddr,			   /* R6 */
				    length,			   /* R7 */
				    (((u64) access_ctrl) << 32ULL),/* R8 */
				    pd,				   /* R9 */
				    0, 0, 0,			   /* R10-R12 */
				    mr_handle,			   /* R4 */
				    &dummy,			   /* R5 */
				    &lkey_out,			   /* R6 */
				    &dummy,			   /* R7 */
				    &dummy,			   /* R8 */
				    &dummy,			   /* R9 */
				    &dummy,			   /* R10 */
				    &dummy,                        /* R11 */
				    &dummy);                       /* R12 */
	*lkey = (u32) lkey_out;

	return hret;
}

u64 ehea_h_register_rpage_mr(const u64 adapter_handle, const u64 mr_handle,
			     const u8 pagesize, const u8 queue_type,
			     const u64 log_pageaddr, const u64 count)
{
	if ((count > 1) && (log_pageaddr & 0xfff)) {
		ehea_error("not on pageboundary");
		return H_PARAMETER;
	}

	return ehea_h_register_rpage(adapter_handle, pagesize,
				     queue_type, mr_handle,
				     log_pageaddr, count);
}

u64 ehea_h_query_ehea(const u64 adapter_handle, void *cb_addr)
{
	u64 hret, dummy, cb_logaddr;

	cb_logaddr = virt_to_abs(cb_addr);

	hret = ehea_hcall_9arg_9ret(H_QUERY_HEA,
				    adapter_handle,		/* R4 */
				    cb_logaddr,			/* R5 */
				    0, 0, 0, 0, 0, 0, 0,	/* R6-R12 */
				    &dummy,			/* R4 */
				    &dummy,			/* R5 */
				    &dummy,			/* R6 */
				    &dummy,			/* R7 */
				    &dummy,			/* R8 */
				    &dummy,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,             	/* R11 */
				    &dummy);            	/* R12 */
#ifdef DEBUG
	ehea_dmp(cb_addr, sizeof(struct hcp_query_ehea), "hcp_query_ehea");
#endif
	return hret;
}

u64 ehea_h_query_ehea_port(const u64 adapter_handle, const u16 port_num,
			   const u8 cb_cat, const u64 select_mask,
			   void *cb_addr)
{
	u64 port_info, dummy;
	u64 cb_logaddr = virt_to_abs(cb_addr);
	u64 arr_index = 0;

	port_info = EHEA_BMASK_SET(H_MEHEAPORT_CAT, cb_cat)
		  | EHEA_BMASK_SET(H_MEHEAPORT_PN, port_num);

	return ehea_hcall_9arg_9ret(H_QUERY_HEA_PORT,
				    adapter_handle,		/* R4 */
				    port_info,			/* R5 */
				    select_mask,		/* R6 */
				    arr_index,			/* R7 */
				    cb_logaddr,			/* R8 */
				    0, 0, 0, 0,			/* R9-R12 */
				    &dummy,			/* R4 */
				    &dummy,			/* R5 */
				    &dummy,			/* R6 */
				    &dummy,			/* R7 */
				    &dummy,			/* R8 */
				    &dummy,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,                     /* R11 */
				    &dummy);                    /* R12 */
}

u64 ehea_h_modify_ehea_port(const u64 adapter_handle, const u16 port_num,
			    const u8 cb_cat, const u64 select_mask,
			    void *cb_addr)
{
	u64 port_info, dummy, inv_attr_ident, proc_mask;
	u64 arr_index = 0;
	u64 cb_logaddr = virt_to_abs(cb_addr);

	port_info = EHEA_BMASK_SET(H_MEHEAPORT_CAT, cb_cat)
		  | EHEA_BMASK_SET(H_MEHEAPORT_PN, port_num);
#ifdef DEBUG
	ehea_dump(cb_addr, sizeof(struct hcp_ehea_port_cb0), "Before HCALL");
#endif
	return ehea_hcall_9arg_9ret(H_MODIFY_HEA_PORT,
				    adapter_handle,		/* R4 */
				    port_info,			/* R5 */
				    select_mask,		/* R6 */
				    arr_index,			/* R7 */
				    cb_logaddr,			/* R8 */
				    0, 0, 0, 0,			/* R9-R12 */
				    &inv_attr_ident,		/* R4 */
				    &proc_mask,			/* R5 */
				    &dummy,			/* R6 */
				    &dummy,			/* R7 */
				    &dummy,			/* R8 */
				    &dummy,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,                     /* R11 */
				    &dummy);                    /* R12 */
}

u64 ehea_h_reg_dereg_bcmc(const u64 adapter_handle, const u16 port_num,
			  const u8 reg_type, const u64 mc_mac_addr,
			  const u16 vlan_id, const u32 hcall_id)
{
	u64 r5_port_num, r6_reg_type, r7_mc_mac_addr, r8_vlan_id, dummy;
	u64 mac_addr = mc_mac_addr >> 16;

	r5_port_num = EHEA_BMASK_SET(H_REGBCMC_PN, port_num);
	r6_reg_type = EHEA_BMASK_SET(H_REGBCMC_REGTYPE, reg_type);
	r7_mc_mac_addr = EHEA_BMASK_SET(H_REGBCMC_MACADDR, mac_addr);
	r8_vlan_id = EHEA_BMASK_SET(H_REGBCMC_VLANID, vlan_id);

	return ehea_hcall_9arg_9ret(hcall_id,
				    adapter_handle,		/* R4 */
				    r5_port_num,		/* R5 */
				    r6_reg_type,		/* R6 */
				    r7_mc_mac_addr,		/* R7 */
				    r8_vlan_id,			/* R8 */
				    0, 0, 0, 0,			/* R9-R12 */
				    &dummy,			/* R4 */
				    &dummy,			/* R5 */
				    &dummy,			/* R6 */
				    &dummy,			/* R7 */
				    &dummy,			/* R8 */
				    &dummy,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,                     /* R11 */
				    &dummy);                    /* R12 */
}

u64 ehea_h_reset_events(const u64 adapter_handle, const u64 neq_handle,
			const u64 event_mask)
{
	u64 dummy;

	return ehea_hcall_9arg_9ret(H_RESET_EVENTS,
				    adapter_handle,		/* R4 */
				    neq_handle,			/* R5 */
				    event_mask,			/* R6 */
				    0, 0, 0, 0, 0, 0,		/* R7-R12 */
				    &dummy,			/* R4 */
				    &dummy,			/* R5 */
				    &dummy,			/* R6 */
				    &dummy,			/* R7 */
				    &dummy,			/* R8 */
				    &dummy,			/* R9 */
				    &dummy,			/* R10 */
				    &dummy,                     /* R11 */
				    &dummy);                    /* R12 */
}
