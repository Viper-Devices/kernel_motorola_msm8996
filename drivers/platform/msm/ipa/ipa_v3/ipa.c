/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/fs.h>
#include <linux/genalloc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/rbtree.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include "ipa_i.h"
#include "ipa_rm_i.h"

#define IPA_SUMMING_THRESHOLD (0x10)
#define IPA_PIPE_MEM_START_OFST (0x0)
#define IPA_PIPE_MEM_SIZE (0x0)
#define IPA_MOBILE_AP_MODE(x) (x == IPA_MODE_MOBILE_AP_ETH || \
			       x == IPA_MODE_MOBILE_AP_WAN || \
			       x == IPA_MODE_MOBILE_AP_WLAN)
#define IPA_CNOC_CLK_RATE (75 * 1000 * 1000UL)
#define IPA_A5_MUX_HEADER_LENGTH (8)

#define IPA_AGGR_MAX_STR_LENGTH (10)

#define CLEANUP_TAG_PROCESS_TIMEOUT 150

#define IPA_AGGR_STR_IN_BYTES(str) \
	(strnlen((str), IPA_AGGR_MAX_STR_LENGTH - 1) + 1)

#define IPA_SPS_PROD_TIMEOUT_MSEC 100

#ifdef CONFIG_COMPAT
#define IPA_IOC_ADD_HDR32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_HDR, \
					compat_uptr_t)
#define IPA_IOC_DEL_HDR32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_HDR, \
					compat_uptr_t)
#define IPA_IOC_ADD_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_RT_RULE, \
					compat_uptr_t)
#define IPA_IOC_DEL_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_RT_RULE, \
					compat_uptr_t)
#define IPA_IOC_ADD_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_FLT_RULE, \
					compat_uptr_t)
#define IPA_IOC_DEL_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_FLT_RULE, \
					compat_uptr_t)
#define IPA_IOC_GET_RT_TBL32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_RT_TBL, \
				compat_uptr_t)
#define IPA_IOC_COPY_HDR32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_COPY_HDR, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF_TX_PROPS32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF_TX_PROPS, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF_RX_PROPS32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_RX_PROPS, \
					compat_uptr_t)
#define IPA_IOC_QUERY_INTF_EXT_PROPS32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_EXT_PROPS, \
					compat_uptr_t)
#define IPA_IOC_GET_HDR32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_HDR, \
				compat_uptr_t)
#define IPA_IOC_ALLOC_NAT_MEM32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ALLOC_NAT_MEM, \
				compat_uptr_t)
#define IPA_IOC_V4_INIT_NAT32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_INIT_NAT, \
				compat_uptr_t)
#define IPA_IOC_NAT_DMA32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NAT_DMA, \
				compat_uptr_t)
#define IPA_IOC_V4_DEL_NAT32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_DEL_NAT, \
				compat_uptr_t)
#define IPA_IOC_GET_NAT_OFFSET32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_NAT_OFFSET, \
				compat_uptr_t)
#define IPA_IOC_PULL_MSG32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_PULL_MSG, \
				compat_uptr_t)
#define IPA_IOC_RM_ADD_DEPENDENCY32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_ADD_DEPENDENCY, \
				compat_uptr_t)
#define IPA_IOC_RM_DEL_DEPENDENCY32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_DEL_DEPENDENCY, \
				compat_uptr_t)
#define IPA_IOC_GENERATE_FLT_EQ32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GENERATE_FLT_EQ, \
				compat_uptr_t)
#define IPA_IOC_QUERY_RT_TBL_INDEX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_RT_TBL_INDEX, \
				compat_uptr_t)
#define IPA_IOC_WRITE_QMAPID32  _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_WRITE_QMAPID, \
				compat_uptr_t)
#define IPA_IOC_MDFY_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_MDFY_FLT_RULE, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_ADD, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_DEL, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_NOTIFY_WAN_EMBMS_CONNECTED, \
					compat_uptr_t)
#define IPA_IOC_ADD_HDR_PROC_CTX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ADD_HDR_PROC_CTX, \
				compat_uptr_t)
#define IPA_IOC_DEL_HDR_PROC_CTX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_DEL_HDR_PROC_CTX, \
				compat_uptr_t)
#define IPA_IOC_MDFY_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_MDFY_RT_RULE, \
				compat_uptr_t)

/**
 * struct ipa3_ioc_nat_alloc_mem32 - nat table memory allocation
 * properties
 * @dev_name: input parameter, the name of table
 * @size: input parameter, size of table in bytes
 * @offset: output parameter, offset into page in case of system memory
 */
struct ipa3_ioc_nat_alloc_mem32 {
	char dev_name[IPA_RESOURCE_NAME_MAX];
	compat_size_t size;
	compat_off_t offset;
};
#endif

static void ipa3_start_tag_process(struct work_struct *work);
static DECLARE_WORK(ipa3_tag_work, ipa3_start_tag_process);

static void ipa3_sps_process_irq(struct work_struct *work);
static DECLARE_WORK(ipa3_sps_process_irq_work, ipa3_sps_process_irq);

static void ipa3_sps_release_resource(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_sps_release_resource_work,
	ipa3_sps_release_resource);

static struct ipa3_plat_drv_res ipa3_res = {0, };
struct msm_bus_scale_pdata *ipa3_bus_scale_table;

static struct clk *ipa3_clk;
static struct clk *smmu_clk;

struct ipa3_context *ipa3_ctx;
static struct device *master_dev;
struct platform_device *ipa3_pdev;
static bool smmu_present;
static bool arm_smmu;
static bool smmu_disable_htw;

enum ipa_smmu_cb_type {
	IPA_SMMU_CB_AP,
	IPA_SMMU_CB_WLAN,
	IPA_SMMU_CB_UC,
	IPA_SMMU_CB_MAX

};

static struct ipa_smmu_cb_ctx smmu_cb[IPA_SMMU_CB_MAX];

#if !defined(CONFIG_ARM_DMA_USE_IOMMU) && !defined(CONFIG_ARM64_DMA_USE_IOMMU)
struct dma_iommu_mapping *ipa3_arm_iommu_create_mapping(struct bus_type *bus,
		dma_addr_t base, size_t size)
{
	return NULL;
}

void ipa3_arm_iommu_release_mapping(struct dma_iommu_mapping *mapping) { }

int ipa3_arm_iommu_attach_device(struct device *dev,
		struct dma_iommu_mapping *mapping)
{
	return 0;
}

void ipa3_arm_iommu_detach_device(struct device *dev) { }
#endif


struct iommu_domain *ipa3_get_smmu_domain(void)
{
	if (smmu_cb[IPA_SMMU_CB_AP].valid)
		return smmu_cb[IPA_SMMU_CB_AP].mapping->domain;

	IPAERR("CB not valid\n");

	return NULL;
}

struct iommu_domain *ipa3_get_uc_smmu_domain(void)
{
	struct iommu_domain *domain = NULL;

	if (smmu_cb[IPA_SMMU_CB_UC].valid)
		domain = smmu_cb[IPA_SMMU_CB_UC].mapping->domain;
	else
		IPAERR("CB not valid\n");

	return domain;
}

struct device *ipa3_get_dma_dev(void)
{
	return ipa3_ctx->pdev;
}

/**
 * ipa3_get_wlan_smmu_ctx()- Return the wlan smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa3_get_wlan_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_WLAN];
}

/**
 * ipa3_get_uc_smmu_ctx()- Return the uc smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa3_get_uc_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_UC];
}

static int ipa3_open(struct inode *inode, struct file *filp)
{
	struct ipa3_context *ctx = NULL;

	IPADBG("ENTER\n");
	ctx = container_of(inode->i_cdev, struct ipa3_context, cdev);
	filp->private_data = ctx;

	return 0;
}

static void ipa3_wan_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff) {
		IPAERR("Null buffer\n");
		return;
	}

	if (type != WAN_UPSTREAM_ROUTE_ADD &&
	    type != WAN_UPSTREAM_ROUTE_DEL &&
	    type != WAN_EMBMS_CONNECT) {
		IPAERR("Wrong type given. buff %p type %d\n", buff, type);
		return;
	}

	kfree(buff);
}

static int ipa3_send_wan_msg(unsigned long usr_param, uint8_t msg_type)
{
	int retval;
	struct ipa_wan_msg *wan_msg;
	struct ipa_msg_meta msg_meta;

	wan_msg = kzalloc(sizeof(struct ipa_wan_msg), GFP_KERNEL);
	if (!wan_msg) {
		IPAERR("no memory\n");
		return -ENOMEM;
	}

	if (copy_from_user((u8 *)wan_msg, (u8 *)usr_param,
		sizeof(struct ipa_wan_msg))) {
		kfree(wan_msg);
		return -EFAULT;
	}

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = msg_type;
	msg_meta.msg_len = sizeof(struct ipa_wan_msg);
	retval = ipa3_send_msg(&msg_meta, wan_msg, ipa3_wan_msg_free_cb);
	if (retval) {
		IPAERR("ipa3_send_msg failed: %d\n", retval);
		kfree(wan_msg);
		return retval;
	}

	return 0;
}


static long ipa3_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	u32 pyld_sz;
	u8 header[128] = { 0 };
	u8 *param = NULL;
	struct ipa_ioc_nat_alloc_mem nat_mem;
	struct ipa_ioc_v4_nat_init nat_init;
	struct ipa_ioc_v4_nat_del nat_del;
	struct ipa_ioc_rm_dependency rm_depend;
	size_t sz;

	IPADBG("cmd=%x nr=%d\n", cmd, _IOC_NR(cmd));

	if (_IOC_TYPE(cmd) != IPA_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= IPA_IOCTL_MAX)
		return -ENOTTY;

	ipa3_inc_client_enable_clks();

	switch (cmd) {
	case IPA_IOC_ALLOC_NAT_MEM:
		if (copy_from_user((u8 *)&nat_mem, (u8 *)arg,
					sizeof(struct ipa_ioc_nat_alloc_mem))) {
			retval = -EFAULT;
			break;
		}
		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		if (ipa3_allocate_nat_device(&nat_mem)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, (u8 *)&nat_mem,
					sizeof(struct ipa_ioc_nat_alloc_mem))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_V4_INIT_NAT:
		if (copy_from_user((u8 *)&nat_init, (u8 *)arg,
					sizeof(struct ipa_ioc_v4_nat_init))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_nat_init_cmd(&nat_init)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_NAT_DMA:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_nat_dma_cmd))) {
			retval = -EFAULT;
			break;
		}

		pyld_sz =
		   sizeof(struct ipa_ioc_nat_dma_cmd) +
		   ((struct ipa_ioc_nat_dma_cmd *)header)->entries *
		   sizeof(struct ipa_ioc_nat_dma_one);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}

		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}

		if (ipa3_nat_dma_cmd((struct ipa_ioc_nat_dma_cmd *)param)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_V4_DEL_NAT:
		if (copy_from_user((u8 *)&nat_del, (u8 *)arg,
					sizeof(struct ipa_ioc_v4_nat_del))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_nat_del_cmd(&nat_del)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_add_hdr))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_add_hdr) +
		   ((struct ipa_ioc_add_hdr *)header)->num_hdrs *
		   sizeof(struct ipa_hdr_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_hdr((struct ipa_ioc_add_hdr *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_del_hdr))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_del_hdr) +
		   ((struct ipa_ioc_del_hdr *)header)->num_hdls *
		   sizeof(struct ipa_hdr_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_hdr((struct ipa_ioc_del_hdr *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_RT_RULE:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_add_rt_rule))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_add_rt_rule) +
		   ((struct ipa_ioc_add_rt_rule *)header)->num_rules *
		   sizeof(struct ipa_rt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_rt_rule((struct ipa_ioc_add_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_MDFY_RT_RULE:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_mdfy_rt_rule))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_mdfy_rt_rule) +
		   ((struct ipa_ioc_mdfy_rt_rule *)header)->num_rules *
		   sizeof(struct ipa_rt_rule_mdfy);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_mdfy_rt_rule((struct ipa_ioc_mdfy_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_RT_RULE:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_del_rt_rule))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_del_rt_rule) +
		   ((struct ipa_ioc_del_rt_rule *)header)->num_hdls *
		   sizeof(struct ipa_rt_rule_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_rt_rule((struct ipa_ioc_del_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_FLT_RULE:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_add_flt_rule))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_add_flt_rule) +
		   ((struct ipa_ioc_add_flt_rule *)header)->num_rules *
		   sizeof(struct ipa_flt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_flt_rule((struct ipa_ioc_add_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_FLT_RULE:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_del_flt_rule))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_del_flt_rule) +
		   ((struct ipa_ioc_del_flt_rule *)header)->num_hdls *
		   sizeof(struct ipa_flt_rule_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_flt_rule((struct ipa_ioc_del_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_MDFY_FLT_RULE:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_mdfy_flt_rule))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_mdfy_flt_rule) +
		   ((struct ipa_ioc_mdfy_flt_rule *)header)->num_rules *
		   sizeof(struct ipa_flt_rule_mdfy);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_mdfy_flt_rule((struct ipa_ioc_mdfy_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_COMMIT_HDR:
		retval = ipa3_commit_hdr();
		break;
	case IPA_IOC_RESET_HDR:
		retval = ipa3_reset_hdr();
		break;
	case IPA_IOC_COMMIT_RT:
		retval = ipa3_commit_rt(arg);
		break;
	case IPA_IOC_RESET_RT:
		retval = ipa3_reset_rt(arg);
		break;
	case IPA_IOC_COMMIT_FLT:
		retval = ipa3_commit_flt(arg);
		break;
	case IPA_IOC_RESET_FLT:
		retval = ipa3_reset_flt(arg);
		break;
	case IPA_IOC_GET_RT_TBL:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_get_rt_tbl((struct ipa_ioc_get_rt_tbl *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PUT_RT_TBL:
		retval = ipa3_put_rt_tbl(arg);
		break;
	case IPA_IOC_GET_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_get_hdr((struct ipa_ioc_get_hdr *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PUT_HDR:
		retval = ipa3_put_hdr(arg);
		break;
	case IPA_IOC_SET_FLT:
		retval = ipa3_cfg_filter(arg);
		break;
	case IPA_IOC_COPY_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_copy_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_copy_hdr((struct ipa_ioc_copy_hdr *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_copy_hdr))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_query_intf))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf((struct ipa_ioc_query_intf *)header)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_query_intf))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_TX_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_tx_props);
		if (copy_from_user(header, (u8 *)arg, sz)) {
			retval = -EFAULT;
			break;
		}

		if (((struct ipa_ioc_query_intf_tx_props *)header)->num_tx_props
				> IPA_NUM_PROPS_MAX) {
			retval = -EFAULT;
			break;
		}

		pyld_sz = sz + ((struct ipa_ioc_query_intf_tx_props *)
				header)->num_tx_props *
			sizeof(struct ipa_ioc_tx_intf_prop);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf_tx_props(
				(struct ipa_ioc_query_intf_tx_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_RX_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_rx_props);
		if (copy_from_user(header, (u8 *)arg, sz)) {
			retval = -EFAULT;
			break;
		}

		if (((struct ipa_ioc_query_intf_rx_props *)header)->num_rx_props
				> IPA_NUM_PROPS_MAX) {
			retval = -EFAULT;
			break;
		}

		pyld_sz = sz + ((struct ipa_ioc_query_intf_rx_props *)
				header)->num_rx_props *
			sizeof(struct ipa_ioc_rx_intf_prop);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf_rx_props(
				(struct ipa_ioc_query_intf_rx_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_EXT_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_ext_props);
		if (copy_from_user(header, (u8 *)arg, sz)) {
			retval = -EFAULT;
			break;
		}

		if (((struct ipa_ioc_query_intf_ext_props *)
				header)->num_ext_props > IPA_NUM_PROPS_MAX) {
			retval = -EFAULT;
			break;
		}

		pyld_sz = sz + ((struct ipa_ioc_query_intf_ext_props *)
				header)->num_ext_props *
			sizeof(struct ipa_ioc_ext_intf_prop);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf_ext_props(
				(struct ipa_ioc_query_intf_ext_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PULL_MSG:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_msg_meta))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz = sizeof(struct ipa_msg_meta) +
		   ((struct ipa_msg_meta *)header)->msg_len;
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_pull_msg((struct ipa_msg_meta *)param,
				 (char *)param + sizeof(struct ipa_msg_meta),
				 ((struct ipa_msg_meta *)param)->msg_len) !=
		       ((struct ipa_msg_meta *)param)->msg_len) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_RM_ADD_DEPENDENCY:
		if (copy_from_user((u8 *)&rm_depend, (u8 *)arg,
				sizeof(struct ipa_ioc_rm_dependency))) {
			retval = -EFAULT;
			break;
		}
		retval = ipa3_rm_add_dependency(rm_depend.resource_name,
						rm_depend.depends_on_name);
		break;
	case IPA_IOC_RM_DEL_DEPENDENCY:
		if (copy_from_user((u8 *)&rm_depend, (u8 *)arg,
				sizeof(struct ipa_ioc_rm_dependency))) {
			retval = -EFAULT;
			break;
		}
		retval = ipa3_rm_delete_dependency(rm_depend.resource_name,
						rm_depend.depends_on_name);
		break;
	case IPA_IOC_GENERATE_FLT_EQ:
		{
			struct ipa_ioc_generate_flt_eq flt_eq;

			if (copy_from_user(&flt_eq, (u8 *)arg,
				sizeof(struct ipa_ioc_generate_flt_eq))) {
				retval = -EFAULT;
				break;
			}
			if (ipa3_generate_flt_eq(flt_eq.ip, &flt_eq.attrib,
						&flt_eq.eq_attrib)) {
				retval = -EFAULT;
				break;
			}
			if (copy_to_user((u8 *)arg, &flt_eq,
				sizeof(struct ipa_ioc_generate_flt_eq))) {
				retval = -EFAULT;
				break;
			}
			break;
		}
	case IPA_IOC_QUERY_EP_MAPPING:
		{
			retval = ipa3_get_ep_mapping(arg);
			break;
		}
	case IPA_IOC_QUERY_RT_TBL_INDEX:
		if (copy_from_user(header, (u8 *)arg,
				sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_rt_index(
			 (struct ipa_ioc_get_rt_tbl_indx *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
				sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_WRITE_QMAPID:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_write_qmapid))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_write_qmap_id((struct ipa_ioc_write_qmapid *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_write_qmapid))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD:
		retval = ipa3_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_ADD);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL:
		retval = ipa3_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_DEL);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED:
		retval = ipa3_send_wan_msg(arg, WAN_EMBMS_CONNECT);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_ADD_HDR_PROC_CTX:
		if (copy_from_user(header, (u8 *)arg,
			sizeof(struct ipa_ioc_add_hdr_proc_ctx))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_add_hdr_proc_ctx) +
		   ((struct ipa_ioc_add_hdr_proc_ctx *)header)->num_proc_ctxs *
		   sizeof(struct ipa_hdr_proc_ctx_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_hdr_proc_ctx(
			(struct ipa_ioc_add_hdr_proc_ctx *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_DEL_HDR_PROC_CTX:
		if (copy_from_user(header, (u8 *)arg,
			sizeof(struct ipa_ioc_del_hdr_proc_ctx))) {
			retval = -EFAULT;
			break;
		}
		pyld_sz =
		   sizeof(struct ipa_ioc_del_hdr_proc_ctx) +
		   ((struct ipa_ioc_del_hdr_proc_ctx *)header)->num_hdls *
		   sizeof(struct ipa_hdr_proc_ctx_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_hdr_proc_ctx(
			(struct ipa_ioc_del_hdr_proc_ctx *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	default:        /* redundant, as cmd was checked against MAXNR */
		ipa3_dec_client_disable_clks();
		return -ENOTTY;
	}
	kfree(param);

	ipa3_dec_client_disable_clks();

	return retval;
}

/**
* ipa3_setup_dflt_rt_tables() - Setup default routing tables
*
* Return codes:
* 0: success
* -ENOMEM: failed to allocate memory
* -EPERM: failed to add the tables
*/
int ipa3_setup_dflt_rt_tables(void)
{
	struct ipa_ioc_add_rt_rule *rt_rule;
	struct ipa_rt_rule_add *rt_rule_entry;

	rt_rule =
	   kzalloc(sizeof(struct ipa_ioc_add_rt_rule) + 1 *
			   sizeof(struct ipa_rt_rule_add), GFP_KERNEL);
	if (!rt_rule) {
		IPAERR("fail to alloc mem\n");
		return -ENOMEM;
	}
	/* setup a default v4 route to point to Apps */
	rt_rule->num_rules = 1;
	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	strlcpy(rt_rule->rt_tbl_name, IPA_DFLT_RT_TBL_NAME,
			IPA_RESOURCE_NAME_MAX);

	rt_rule_entry = &rt_rule->rules[0];
	rt_rule_entry->at_rear = 1;
	rt_rule_entry->rule.dst = IPA_CLIENT_APPS_LAN_CONS;
	rt_rule_entry->rule.hdr_hdl = ipa3_ctx->excp_hdr_hdl;
	rt_rule_entry->rule.retain_hdr = 1;

	if (ipa3_add_rt_rule(rt_rule)) {
		IPAERR("fail to add dflt v4 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPADBG("dflt v4 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	ipa3_ctx->dflt_v4_rt_rule_hdl = rt_rule_entry->rt_rule_hdl;

	/* setup a default v6 route to point to A5 */
	rt_rule->ip = IPA_IP_v6;
	if (ipa3_add_rt_rule(rt_rule)) {
		IPAERR("fail to add dflt v6 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPADBG("dflt v6 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	ipa3_ctx->dflt_v6_rt_rule_hdl = rt_rule_entry->rt_rule_hdl;

	/*
	 * because these tables are the very first to be added, they will both
	 * have the same index (0) which is essential for programming the
	 * "route" end-point config
	 */

	kfree(rt_rule);

	return 0;
}

static int ipa3_setup_exception_path(void)
{
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_hdr_add *hdr_entry;
	struct ipa3_route route = { 0 };
	int ret;

	/* install the basic exception header */
	hdr = kzalloc(sizeof(struct ipa_ioc_add_hdr) + 1 *
		      sizeof(struct ipa_hdr_add), GFP_KERNEL);
	if (!hdr) {
		IPAERR("fail to alloc exception hdr\n");
		return -ENOMEM;
	}
	hdr->num_hdrs = 1;
	hdr->commit = 1;
	hdr_entry = &hdr->hdr[0];

	strlcpy(hdr_entry->name, IPA_LAN_RX_HDR_NAME, IPA_RESOURCE_NAME_MAX);
	hdr_entry->hdr_len = IPA_LAN_RX_HEADER_LENGTH;

	if (ipa3_add_hdr(hdr)) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	if (hdr_entry->status) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	ipa3_ctx->excp_hdr_hdl = hdr_entry->hdr_hdl;

	/* set the route register to pass exception packets to Apps */
	route.route_def_pipe = ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	route.route_frag_def_pipe = ipa3_get_ep_mapping(
		IPA_CLIENT_APPS_LAN_CONS);
	route.route_def_hdr_table = !ipa3_ctx->hdr_tbl_lcl;

	if (ipa3_cfg_route(&route)) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	ret = 0;
bail:
	kfree(hdr);
	return ret;
}

static int ipa3_init_smem_region(int memory_region_size,
				int memory_region_offset)
{
	struct ipa3_hw_imm_cmd_dma_shared_mem cmd;
	struct ipa3_desc desc;
	struct ipa3_mem_buffer mem;
	int rc;

	if (memory_region_size == 0)
		return 0;

	memset(&desc, 0, sizeof(desc));
	memset(&cmd, 0, sizeof(cmd));
	memset(&mem, 0, sizeof(mem));

	mem.size = memory_region_size;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size,
		&mem.phys_base, GFP_KERNEL);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	memset(mem.base, 0, mem.size);
	cmd.skip_pipeline_clear = 0;
	cmd.pipeline_clear_options = IPA_HPS_CLEAR;
	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		memory_region_offset;
	desc.opcode = IPA_DMA_SHARED_MEM;
	desc.pyld = &cmd;
	desc.len = sizeof(cmd);
	desc.type = IPA_IMM_CMD_DESC;

	rc = ipa3_send_cmd(1, &desc);
	if (rc) {
		IPAERR("failed to send immediate command (error %d)\n", rc);
		rc = -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base,
		mem.phys_base);

	return rc;
}

/**
* ipa3_init_q6_smem() - Initialize Q6 general memory and
*                      header memory regions in IPA.
*
* Return codes:
* 0: success
* -ENOMEM: failed to allocate dma memory
* -EFAULT: failed to send IPA command to initialize the memory
*/
int ipa3_init_q6_smem(void)
{
	int rc;

	ipa3_inc_client_enable_clks();

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_size),
		IPA_MEM_PART(modem_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem RAM memory\n");
		ipa3_dec_client_disable_clks();
		return rc;
	}

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_hdr_size),
		IPA_MEM_PART(modem_hdr_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem HDRs RAM memory\n");
		ipa3_dec_client_disable_clks();
		return rc;
	}

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_hdr_proc_ctx_size),
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem proc ctx RAM memory\n");
		ipa3_dec_client_disable_clks();
		return rc;
	}

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_comp_decomp_size),
		IPA_MEM_PART(modem_comp_decomp_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem Comp/Decomp RAM memory\n");
		ipa3_dec_client_disable_clks();
		return rc;
	}

	ipa3_dec_client_disable_clks();

	return rc;
}

static void ipa3_free_buffer(void *user1, int user2)
{
	kfree(user1);
}

static int ipa3_q6_pipe_delay(void)
{
	u32 reg_val = 0;
	int client_idx;
	int ep_idx;

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_PROD(client_idx)) {
			ep_idx = ipa3_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			IPA_SETFIELD_IN_REG(reg_val, 1,
				IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_SHFT,
				IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_BMSK);

			ipa_write_reg(ipa3_ctx->mmio,
				IPA_ENDP_INIT_CTRL_N_OFST(ep_idx), reg_val);
		}
	}

	return 0;
}

static int ipa3_q6_avoid_holb(void)
{
	u32 reg_val;
	int ep_idx;
	int client_idx;
	struct ipa_ep_cfg_ctrl avoid_holb;

	memset(&avoid_holb, 0, sizeof(avoid_holb));
	avoid_holb.ipa_ep_suspend = true;

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_CONS(client_idx)) {
			ep_idx = ipa3_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			/*
			 * ipa3_cfg_ep_holb is not used here because we are
			 * setting HOLB on Q6 pipes, and from APPS perspective
			 * they are not valid, therefore, the above function
			 * will fail.
			 */
			reg_val = 0;
			IPA_SETFIELD_IN_REG(reg_val, 0,
				IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_TIMER_SHFT,
				IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_TIMER_BMSK);

			ipa_write_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v3_0(ep_idx),
				reg_val);

			reg_val = 0;
			IPA_SETFIELD_IN_REG(reg_val, 1,
				IPA_ENDP_INIT_HOL_BLOCK_EN_N_EN_SHFT,
				IPA_ENDP_INIT_HOL_BLOCK_EN_N_EN_BMSK);

			ipa_write_reg(ipa3_ctx->mmio,
				IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v3_0(ep_idx),
				reg_val);

			ipa3_cfg_ep_ctrl(ep_idx, &avoid_holb);
		}
	}

	return 0;
}

static u32 ipa3_get_max_flt_rt_cmds(u32 num_pipes)
{
	u32 max_cmds = 0;

	/*
	 * As many filter tables as there are filtering pipes,
	 *	x4 for IPv4/IPv6 and hashable/non-hashable combinations
	 */
	max_cmds += ipa3_ctx->ep_flt_num * 4;

	/* For each of the Modem routing tables - x2 for hash/non-hash */
	max_cmds += ((IPA_MEM_PART(v4_modem_rt_index_hi) -
		     IPA_MEM_PART(v4_modem_rt_index_lo) + 1) * 2);

	max_cmds += ((IPA_MEM_PART(v6_modem_rt_index_hi) -
		     IPA_MEM_PART(v6_modem_rt_index_lo) + 1) * 2);

	return max_cmds;
}

static int ipa3_q6_clean_q6_tables(void)
{
	struct ipa3_desc *desc;
	struct ipa3_hw_imm_cmd_dma_shared_mem *cmd = NULL;
	int pipe_idx;
	int num_cmds = 0;
	int index;
	int retval;
	struct ipa3_mem_buffer mem = { 0 };
	u64 *entry;
	u32 max_cmds = ipa3_get_max_flt_rt_cmds(ipa3_ctx->ipa_num_pipes);
	int flt_idx = 0;

	mem.size = IPA_HW_TBL_HDR_WIDTH;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size,
		&mem.phys_base, GFP_KERNEL);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;
	*entry = ipa3_ctx->empty_rt_tbl_mem.phys_base;

	desc = kcalloc(max_cmds, sizeof(struct ipa3_desc), GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		retval = -ENOMEM;
		goto bail_dma;
	}

	cmd = kcalloc(max_cmds, sizeof(struct ipa3_hw_imm_cmd_dma_shared_mem),
		GFP_KERNEL);
	if (!cmd) {
		IPAERR("failed to allocate memory\n");
		retval = -ENOMEM;
		goto bail_desc;
	}

	/*
	 * Iterating over all the filtering pipes which are
	 * either invalid but connected or connected but not configured by AP.
	 */
	for (pipe_idx = 0; pipe_idx < ipa3_ctx->ipa_num_pipes; pipe_idx++) {
		if (!ipa_is_ep_support_flt(pipe_idx))
			continue;

		if (!ipa3_ctx->ep[pipe_idx].valid ||
		    ipa3_ctx->ep[pipe_idx].skip_ep_cfg) {
			/*
			 * Need to point v4 and v6 hash fltr tables to an
			 * empty table
			 */
			cmd[num_cmds].skip_pipeline_clear = 0;
			cmd[num_cmds].pipeline_clear_options =
				IPA_FULL_PIPELINE_CLEAR;
			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr = mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_flt_hash_ofst) +
				IPA_HW_TBL_HDR_WIDTH +
				flt_idx * IPA_HW_TBL_HDR_WIDTH;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;

			cmd[num_cmds].skip_pipeline_clear = 0;
			cmd[num_cmds].pipeline_clear_options =
				IPA_FULL_PIPELINE_CLEAR;
			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr =  mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_flt_hash_ofst) +
				IPA_HW_TBL_HDR_WIDTH +
				flt_idx * IPA_HW_TBL_HDR_WIDTH;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;

			/*
			 * Need to point v4 and v6 non-hash fltr tables to an
			 * empty table
			 */
			cmd[num_cmds].skip_pipeline_clear = 0;
			cmd[num_cmds].pipeline_clear_options =
				IPA_FULL_PIPELINE_CLEAR;
			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr = mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_flt_nhash_ofst) +
				IPA_HW_TBL_HDR_WIDTH +
				flt_idx * IPA_HW_TBL_HDR_WIDTH;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;

			cmd[num_cmds].skip_pipeline_clear = 0;
			cmd[num_cmds].pipeline_clear_options =
				IPA_FULL_PIPELINE_CLEAR;
			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr =  mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_flt_nhash_ofst) +
				IPA_HW_TBL_HDR_WIDTH +
				flt_idx * IPA_HW_TBL_HDR_WIDTH;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;
		}

		flt_idx++;
	}

	/* Need to point v4/v6 modem routing tables to an empty table */
	for (index = IPA_MEM_PART(v4_modem_rt_index_lo);
		 index <= IPA_MEM_PART(v4_modem_rt_index_hi);
		 index++) {
		cmd[num_cmds].skip_pipeline_clear = 0;
		cmd[num_cmds].pipeline_clear_options =
			IPA_FULL_PIPELINE_CLEAR;
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_hash_ofst) +
			index * IPA_HW_TBL_HDR_WIDTH;

		desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
		desc[num_cmds].pyld = &cmd[num_cmds];
		desc[num_cmds].len = sizeof(*cmd);
		desc[num_cmds].type = IPA_IMM_CMD_DESC;
		num_cmds++;

		cmd[num_cmds].skip_pipeline_clear = 0;
		cmd[num_cmds].pipeline_clear_options =
			IPA_FULL_PIPELINE_CLEAR;
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_nhash_ofst) +
			index * IPA_HW_TBL_HDR_WIDTH;

		desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
		desc[num_cmds].pyld = &cmd[num_cmds];
		desc[num_cmds].len = sizeof(*cmd);
		desc[num_cmds].type = IPA_IMM_CMD_DESC;
		num_cmds++;
	}

	for (index = IPA_MEM_PART(v6_modem_rt_index_lo);
		 index <= IPA_MEM_PART(v6_modem_rt_index_hi);
		 index++) {
		cmd[num_cmds].skip_pipeline_clear = 0;
		cmd[num_cmds].pipeline_clear_options =
			IPA_FULL_PIPELINE_CLEAR;
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_hash_ofst) +
			index * IPA_HW_TBL_HDR_WIDTH;

		desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
		desc[num_cmds].pyld = &cmd[num_cmds];
		desc[num_cmds].len = sizeof(*cmd);
		desc[num_cmds].type = IPA_IMM_CMD_DESC;
		num_cmds++;

		cmd[num_cmds].skip_pipeline_clear = 0;
		cmd[num_cmds].pipeline_clear_options =
			IPA_FULL_PIPELINE_CLEAR;
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_nhash_ofst) +
			index * IPA_HW_TBL_HDR_WIDTH;

		desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
		desc[num_cmds].pyld = &cmd[num_cmds];
		desc[num_cmds].len = sizeof(*cmd);
		desc[num_cmds].type = IPA_IMM_CMD_DESC;
		num_cmds++;
	}

	retval = ipa3_send_cmd(num_cmds, desc);
	if (retval) {
		IPAERR("failed to send immediate command (error %d)\n", retval);
		retval = -EFAULT;
	}

	kfree(cmd);

bail_desc:
	kfree(desc);

bail_dma:
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);

	return retval;
}

static void ipa3_q6_disable_agg_reg(struct ipa3_register_write *reg_write,
				   int ep_idx)
{
	reg_write->skip_pipeline_clear = 0;
	reg_write->pipeline_clear_options = IPA_FULL_PIPELINE_CLEAR;

	reg_write->offset = IPA_ENDP_INIT_AGGR_N_OFST_v3_0(ep_idx);
	reg_write->value =
		(1 & IPA_ENDP_INIT_AGGR_N_AGGR_FORCE_CLOSE_BMSK) <<
		IPA_ENDP_INIT_AGGR_N_AGGR_FORCE_CLOSE_SHFT;
	reg_write->value_mask =
		IPA_ENDP_INIT_AGGR_N_AGGR_FORCE_CLOSE_BMSK <<
		IPA_ENDP_INIT_AGGR_N_AGGR_FORCE_CLOSE_SHFT;

	reg_write->value |=
		((0 & IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK) <<
		IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT);
	reg_write->value_mask |=
		((IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK <<
		IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT));
}

static int ipa3_q6_set_ex_path_dis_agg(void)
{
	int ep_idx;
	int client_idx;
	struct ipa3_desc *desc;
	int num_descs = 0;
	int index;
	struct ipa3_register_write *reg_write;
	int retval;

	desc = kcalloc(ipa3_ctx->ipa_num_pipes, sizeof(struct ipa3_desc),
			GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Set the exception path to AP */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		ep_idx = ipa3_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;

		if (ipa3_ctx->ep[ep_idx].valid &&
			ipa3_ctx->ep[ep_idx].skip_ep_cfg) {
			BUG_ON(num_descs >= ipa3_ctx->ipa_num_pipes);
			reg_write = kzalloc(sizeof(*reg_write), GFP_KERNEL);

			if (!reg_write) {
				IPAERR("failed to allocate memory\n");
				BUG();
			}
			reg_write->skip_pipeline_clear = 0;
			reg_write->pipeline_clear_options =
				IPA_FULL_PIPELINE_CLEAR;
			reg_write->offset = IPA_ENDP_STATUS_n_OFST(ep_idx);
			reg_write->value =
				(ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS) &
				IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK) <<
				IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT;
			reg_write->value_mask =
				IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK <<
				IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT;

			desc[num_descs].opcode = IPA_REGISTER_WRITE;
			desc[num_descs].pyld = reg_write;
			desc[num_descs].len = sizeof(*reg_write);
			desc[num_descs].type = IPA_IMM_CMD_DESC;
			desc[num_descs].callback = ipa3_free_buffer;
			desc[num_descs].user1 = reg_write;
			num_descs++;
		}
	}

	/* Disable AGGR on IPA->Q6 pipes */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_CONS(client_idx)) {
			reg_write = kzalloc(sizeof(*reg_write), GFP_KERNEL);

			if (!reg_write) {
				IPAERR("failed to allocate memory\n");
				BUG();
			}

			ipa3_q6_disable_agg_reg(reg_write,
					       ipa3_get_ep_mapping(client_idx));

			desc[num_descs].opcode = IPA_REGISTER_WRITE;
			desc[num_descs].pyld = reg_write;
			desc[num_descs].len = sizeof(*reg_write);
			desc[num_descs].type = IPA_IMM_CMD_DESC;
			desc[num_descs].callback = ipa3_free_buffer;
			desc[num_descs].user1 = reg_write;
			num_descs++;
		}
	}

	/* Will wait 150msecs for IPA tag process completion */
	retval = ipa3_tag_process(desc, num_descs,
				 msecs_to_jiffies(CLEANUP_TAG_PROCESS_TIMEOUT));
	if (retval) {
		IPAERR("TAG process failed! (error %d)\n", retval);
		/* For timeout error ipa3_free_buffer cb will free user1 */
		if (retval != -ETIME) {
			for (index = 0; index < num_descs; index++)
				kfree(desc[index].user1);
			retval = -EINVAL;
		}
	}

	kfree(desc);

	return retval;
}

/**
* ipa3_q6_cleanup() - A cleanup for all Q6 related configuration
*                    in IPA HW. This is performed in case of SSR.
*
* Return codes:
* 0: success
* This is a mandatory procedure, in case one of the steps fails, the
* AP needs to restart.
*/
int ipa3_q6_cleanup(void)
{
	int client_idx;
	int res;

	/* If uC has notified the APPS upon a ZIP engine error,
	 * APPS need to assert (This is a non recoverable error).
	 */
	if (ipa3_ctx->uc_ctx.uc_zip_error)
		BUG();

	ipa3_inc_client_enable_clks();

	if (ipa3_q6_pipe_delay()) {
		IPAERR("Failed to delay Q6 pipes\n");
		BUG();
	}
	if (ipa3_q6_avoid_holb()) {
		IPAERR("Failed to set HOLB on Q6 pipes\n");
		BUG();
	}
	if (ipa3_q6_clean_q6_tables()) {
		IPAERR("Failed to clean Q6 tables\n");
		BUG();
	}
	if (ipa3_q6_set_ex_path_dis_agg()) {
		IPAERR("Failed to disable aggregation on Q6 pipes\n");
		BUG();
	}

	/*
	 * Q6 relies on the AP to reset all Q6 IPA pipes.
	 * In case the uC is not loaded, or upon any failure in the pipe reset
	 * sequence, we have to assert.
	 */
	if (!ipa3_ctx->uc_ctx.uc_loaded) {
		IPAERR("uC is not loaded, can't reset Q6 pipes\n");
		BUG();
	}

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++)
		if (IPA_CLIENT_IS_Q6_CONS(client_idx) ||
		    IPA_CLIENT_IS_Q6_PROD(client_idx)) {
			res = ipa3_uc_reset_pipe(client_idx);
			if (res)
				BUG();
		}

	ipa3_ctx->q6_proxy_clk_vote_valid = true;

	return 0;
}

static inline void ipa3_sram_set_canary(u32 *sram_mmio, int offset)
{
	/* Set 4 bytes of CANARY before the offset */
	sram_mmio[(offset - 4) / 4] = IPA_MEM_CANARY_VAL;
}

/**
 * _ipa_init_sram_v3_0() - Initialize IPA local SRAM.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_sram_v3_0(void)
{
	u32 *ipa_sram_mmio;
	unsigned long phys_addr;

	phys_addr = ipa3_ctx->ipa_wrapper_base +
		ipa3_ctx->ctrl->ipa_reg_base_ofst +
		IPA_SRAM_DIRECT_ACCESS_N_OFST_v3_0(
			ipa3_ctx->smem_restricted_bytes / 4);

	ipa_sram_mmio = ioremap(phys_addr, ipa3_ctx->smem_sz);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

	/* Consult with ipa_ram_mmap.h on the location of the CANARY values */
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(v4_flt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(v6_flt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_hdr_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_hdr_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(end_ofst));

	iounmap(ipa_sram_mmio);

	return 0;
}

/**
 * _ipa_init_hdr_v3_0() - Initialize IPA header block.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_hdr_v3_0(void)
{
	struct ipa3_desc desc = { 0 };
	struct ipa3_mem_buffer mem;
	struct ipa3_hdr_init_local cmd = { 0 };
	struct ipa3_hw_imm_cmd_dma_shared_mem dma_cmd = { 0 };

	mem.size = IPA_MEM_PART(modem_hdr_size) + IPA_MEM_PART(apps_hdr_size);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	cmd.hdr_table_src_addr = mem.phys_base;
	cmd.size_hdr_table = mem.size;
	cmd.hdr_table_dst_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(modem_hdr_ofst);

	desc.opcode = IPA_HDR_INIT_LOCAL;
	desc.pyld = &cmd;
	desc.len = sizeof(struct ipa3_hdr_init_local);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		dma_free_coherent(ipa3_ctx->pdev,
			mem.size, mem.base,
			mem.phys_base);
		return -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);

	mem.size = IPA_MEM_PART(modem_hdr_proc_ctx_size) +
		IPA_MEM_PART(apps_hdr_proc_ctx_size);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);
	memset(&desc, 0, sizeof(desc));

	dma_cmd.skip_pipeline_clear = 0;
	dma_cmd.pipeline_clear_options = IPA_FULL_PIPELINE_CLEAR;
	dma_cmd.system_addr = mem.phys_base;
	dma_cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst);
	dma_cmd.size = mem.size;
	desc.opcode = IPA_DMA_SHARED_MEM;
	desc.pyld = &dma_cmd;
	desc.len = sizeof(struct ipa3_hw_imm_cmd_dma_shared_mem);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		dma_free_coherent(ipa3_ctx->pdev,
			mem.size,
			mem.base,
			mem.phys_base);
		return -EFAULT;
	}

	ipa_write_reg(ipa3_ctx->mmio,
		IPA_LOCAL_PKT_PROC_CNTXT_BASE_OFST,
		dma_cmd.local_addr);

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);

	return 0;
}

/**
 * _ipa_init_rt4_v3() - Initialize IPA routing block for IPv4.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_rt4_v3(void)
{
	struct ipa3_desc desc = { 0 };
	struct ipa3_mem_buffer mem;
	struct ipa3_ip_v4_routing_init v4_cmd;
	u64 *entry;
	int i;
	int rc = 0;

	for (i = IPA_MEM_PART(v4_modem_rt_index_lo);
		i <= IPA_MEM_PART(v4_modem_rt_index_hi);
		i++)
		ipa3_ctx->rt_idx_bitmap[IPA_IP_v4] |= (1 << i);
	IPADBG("v4 rt bitmap 0x%lx\n", ipa3_ctx->rt_idx_bitmap[IPA_IP_v4]);

	mem.size = IPA_MEM_PART(v4_rt_nhash_size);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;
	for (i = 0; i < IPA_MEM_PART(v4_rt_num_index); i++) {
		*entry = ipa3_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V4_ROUTING_INIT;
	v4_cmd.hash_rules_addr = mem.phys_base;
	v4_cmd.hash_rules_size = mem.size;
	v4_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_rt_hash_ofst);
	v4_cmd.nhash_rules_addr = mem.phys_base;
	v4_cmd.nhash_rules_size = mem.size;
	v4_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_rt_nhash_ofst);
	IPADBG("putting hashable routing IPv4 rules to phys 0x%x\n",
				v4_cmd.hash_local_addr);
	IPADBG("putting non-hashable routing IPv4 rules to phys 0x%x\n",
				v4_cmd.nhash_local_addr);

	desc.pyld = &v4_cmd;
	desc.len = sizeof(struct ipa3_ip_v4_routing_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

/**
 * _ipa_init_rt6_v3() - Initialize IPA routing block for IPv6.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_rt6_v3(void)
{
	struct ipa3_desc desc = { 0 };
	struct ipa3_mem_buffer mem;
	struct ipa3_ip_v6_routing_init v6_cmd;
	u64 *entry;
	int i;
	int rc = 0;

	for (i = IPA_MEM_PART(v6_modem_rt_index_lo);
		i <= IPA_MEM_PART(v6_modem_rt_index_hi);
		i++)
		ipa3_ctx->rt_idx_bitmap[IPA_IP_v6] |= (1 << i);
	IPADBG("v6 rt bitmap 0x%lx\n", ipa3_ctx->rt_idx_bitmap[IPA_IP_v6]);

	mem.size = IPA_MEM_PART(v6_rt_nhash_size);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;
	for (i = 0; i < IPA_MEM_PART(v6_rt_num_index); i++) {
		*entry = ipa3_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V6_ROUTING_INIT;
	v6_cmd.hash_rules_addr = mem.phys_base;
	v6_cmd.hash_rules_size = mem.size;
	v6_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_rt_hash_ofst);
	v6_cmd.nhash_rules_addr = mem.phys_base;
	v6_cmd.nhash_rules_size = mem.size;
	v6_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_rt_nhash_ofst);
	IPADBG("putting hashable routing IPv6 rules to phys 0x%x\n",
				v6_cmd.hash_local_addr);
	IPADBG("putting non-hashable routing IPv6 rules to phys 0x%x\n",
				v6_cmd.nhash_local_addr);

	desc.pyld = &v6_cmd;
	desc.len = sizeof(struct ipa3_ip_v6_routing_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

/**
 * _ipa_init_flt4_v3() - Initialize IPA filtering block for IPv4.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_flt4_v3(void)
{
	struct ipa3_desc desc = { 0 };
	struct ipa3_mem_buffer mem;
	struct ipa3_ip_v4_filter_init v4_cmd;
	u64 *entry;
	int i;
	int rc = 0;
	int flt_spc;

	flt_spc = IPA_MEM_PART(v4_flt_hash_size);
	/* bitmap word */
	flt_spc -= IPA_HW_TBL_HDR_WIDTH;
	flt_spc /= IPA_HW_TBL_HDR_WIDTH;
	if (ipa3_ctx->ep_flt_num > flt_spc) {
		IPAERR("space for v4 hash flt hdr is too small\n");
		WARN_ON(1);
		return -EPERM;
	}
	flt_spc = IPA_MEM_PART(v4_flt_nhash_size);
	/* bitmap word */
	flt_spc -= IPA_HW_TBL_HDR_WIDTH;
	flt_spc /= IPA_HW_TBL_HDR_WIDTH;
	if (ipa3_ctx->ep_flt_num > flt_spc) {
		IPAERR("space for v4 non-hash flt hdr is too small\n");
		WARN_ON(1);
		return -EPERM;
	}

	/* +1 for filtering header bitmap */
	mem.size = (ipa3_ctx->ep_flt_num + 1) * IPA_HW_TBL_HDR_WIDTH;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;

	*entry = ((u64)ipa3_ctx->ep_flt_bitmap) << 1;
	IPADBG("v4 flt bitmap 0x%llx\n", *entry);
	entry++;

	for (i = 0; i <= ipa3_ctx->ep_flt_num; i++) {
		*entry = ipa3_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V4_FILTER_INIT;
	v4_cmd.hash_rules_addr = mem.phys_base;
	v4_cmd.hash_rules_size = mem.size;
	v4_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_flt_hash_ofst);
	v4_cmd.nhash_rules_addr = mem.phys_base;
	v4_cmd.nhash_rules_size = mem.size;
	v4_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_flt_nhash_ofst);
	IPADBG("putting hashable filtering IPv4 rules to phys 0x%x\n",
				v4_cmd.hash_local_addr);
	IPADBG("putting non-hashable filtering IPv4 rules to phys 0x%x\n",
				v4_cmd.nhash_local_addr);

	desc.pyld = &v4_cmd;
	desc.len = sizeof(struct ipa3_ip_v4_filter_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

/**
 * _ipa_init_flt6_v3() - Initialize IPA filtering block for IPv6.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_flt6_v3(void)
{
	struct ipa3_desc desc = { 0 };
	struct ipa3_mem_buffer mem;
	struct ipa3_ip_v6_filter_init v6_cmd;
	u64 *entry;
	int i;
	int rc = 0;
	int flt_spc;

	flt_spc = IPA_MEM_PART(v6_flt_hash_size);
	/* bitmap word */
	flt_spc -= IPA_HW_TBL_HDR_WIDTH;
	flt_spc /= IPA_HW_TBL_HDR_WIDTH;
	if (ipa3_ctx->ep_flt_num > flt_spc) {
		IPAERR("space for v6 hash flt hdr is too small\n");
		WARN_ON(1);
		return -EPERM;
	}
	flt_spc = IPA_MEM_PART(v6_flt_nhash_size);
	/* bitmap word */
	flt_spc -= IPA_HW_TBL_HDR_WIDTH;
	flt_spc /= IPA_HW_TBL_HDR_WIDTH;
	if (ipa3_ctx->ep_flt_num > flt_spc) {
		IPAERR("space for v6 non-hash flt hdr is too small\n");
		WARN_ON(1);
		return -EPERM;
	}

	/* +1 for filtering header bitmap */
	mem.size = (ipa3_ctx->ep_flt_num + 1) * IPA_HW_TBL_HDR_WIDTH;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;

	*entry = ((u64)ipa3_ctx->ep_flt_bitmap) << 1;
	IPADBG("v6 flt bitmap 0x%llx\n", *entry);
	entry++;

	for (i = 0; i <= ipa3_ctx->ep_flt_num; i++) {
		*entry = ipa3_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V6_FILTER_INIT;
	v6_cmd.hash_rules_addr = mem.phys_base;
	v6_cmd.hash_rules_size = mem.size;
	v6_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_flt_hash_ofst);
	v6_cmd.nhash_rules_addr = mem.phys_base;
	v6_cmd.nhash_rules_size = mem.size;
	v6_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_flt_nhash_ofst);
	IPADBG("putting hashable filtering IPv6 rules to phys 0x%x\n",
				v6_cmd.hash_local_addr);
	IPADBG("putting non-hashable filtering IPv6 rules to phys 0x%x\n",
				v6_cmd.nhash_local_addr);

	desc.pyld = &v6_cmd;
	desc.len = sizeof(struct ipa3_ip_v6_filter_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

static int ipa3_setup_flt_hash_tuple(void)
{
	int pipe_idx;
	struct ipa3_hash_tuple tuple;

	memset(&tuple, 0, sizeof(struct ipa3_hash_tuple));

	for (pipe_idx = 0; pipe_idx < ipa3_ctx->ipa_num_pipes ; pipe_idx++) {
		if (!ipa_is_ep_support_flt(pipe_idx))
			continue;

		if (ipa_is_modem_pipe(pipe_idx))
			continue;

		if (ipa3_set_flt_tuple_mask(pipe_idx, &tuple)) {
			IPAERR("failed to setup pipe %d flt tuple\n", pipe_idx);
			return -EFAULT;
		}
	}

	return 0;
}

static int ipa3_setup_rt_hash_tuple(void)
{
	int tbl_idx;
	struct ipa3_hash_tuple tuple;

	memset(&tuple, 0, sizeof(struct ipa3_hash_tuple));

	for (tbl_idx = 0;
		tbl_idx < max(IPA_MEM_PART(v6_rt_num_index),
		IPA_MEM_PART(v4_rt_num_index));
		tbl_idx++) {

		if (tbl_idx >= IPA_MEM_PART(v4_modem_rt_index_lo) &&
			tbl_idx <= IPA_MEM_PART(v4_modem_rt_index_hi))
			continue;

		if (tbl_idx >= IPA_MEM_PART(v6_modem_rt_index_lo) &&
			tbl_idx <= IPA_MEM_PART(v6_modem_rt_index_hi))
			continue;

		if (ipa3_set_rt_tuple_mask(tbl_idx, &tuple)) {
			IPAERR("failed to setup tbl %d rt tuple\n", tbl_idx);
			return -EFAULT;
		}
	}

	return 0;
}

static int ipa3_setup_apps_pipes(void)
{
	struct ipa_sys_connect_params sys_in;
	int result = 0;

	/* CMD OUT (AP->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_CMD_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_APPS_LAN_CONS;
	sys_in.skip_ep_cfg = true;
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_cmd)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_cmd;
	}
	IPADBG("Apps to IPA cmd pipe is connected\n");

	ipa3_ctx->ctrl->ipa_init_sram();
	IPADBG("SRAM initialized\n");

	ipa3_ctx->ctrl->ipa_init_hdr();
	IPADBG("HDR initialized\n");

	ipa3_ctx->ctrl->ipa_init_rt4();
	IPADBG("V4 RT initialized\n");

	ipa3_ctx->ctrl->ipa_init_rt6();
	IPADBG("V6 RT initialized\n");

	ipa3_ctx->ctrl->ipa_init_flt4();
	IPADBG("V4 FLT initialized\n");

	ipa3_ctx->ctrl->ipa_init_flt6();
	IPADBG("V6 FLT initialized\n");

	if (ipa3_setup_flt_hash_tuple()) {
		IPAERR(":fail to configure flt hash tuple\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("flt hash tuple is configured\n");

	if (ipa3_setup_rt_hash_tuple()) {
		IPAERR(":fail to configure rt hash tuple\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("rt hash tuple is configured\n");

	if (ipa3_setup_exception_path()) {
		IPAERR(":fail to setup excp path\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("Exception path was successfully set");

	if (ipa3_setup_dflt_rt_tables()) {
		IPAERR(":fail to setup dflt routes\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("default routing was set\n");

	/* LAN IN (IPA->A5) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_LAN_CONS;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.notify = ipa3_lan_rx_cb;
	sys_in.priv = NULL;
	sys_in.ipa_ep_cfg.hdr.hdr_len = IPA_LAN_RX_HEADER_LENGTH;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_little_endian = false;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid = true;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad = IPA_HDR_PAD;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding = false;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset = 0;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_pad_to_alignment = 2;
	sys_in.ipa_ep_cfg.cfg.cs_offload_en = IPA_ENABLE_CS_OFFLOAD_DL;

	/**
	 * ipa_lan_rx_cb() intended to notify the source EP about packet
	 * being received on the LAN_CONS via calling the source EP call-back.
	 * There could be a race condition with calling this call-back. Other
	 * thread may nullify it - e.g. on EP disconnect.
	 * This lock intended to protect the access to the source EP call-back
	 */
	spin_lock_init(&ipa3_ctx->lan_rx_clnt_notify_lock);
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_data_in)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}

	/* LAN-WAN OUT (AP->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_LAN_WAN_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_TX_DATA_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_data_out)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_data_out;
	}

	return 0;

fail_data_out:
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_in);
fail_schedule_delayed_work:
	if (ipa3_ctx->dflt_v6_rt_rule_hdl)
		__ipa3_del_rt_rule(ipa3_ctx->dflt_v6_rt_rule_hdl);
	if (ipa3_ctx->dflt_v4_rt_rule_hdl)
		__ipa3_del_rt_rule(ipa3_ctx->dflt_v4_rt_rule_hdl);
	if (ipa3_ctx->excp_hdr_hdl)
		__ipa3_del_hdr(ipa3_ctx->excp_hdr_hdl);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_cmd);
fail_cmd:
	return result;
}

static void ipa3_teardown_apps_pipes(void)
{
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_out);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_in);
	__ipa3_del_rt_rule(ipa3_ctx->dflt_v6_rt_rule_hdl);
	__ipa3_del_rt_rule(ipa3_ctx->dflt_v4_rt_rule_hdl);
	__ipa3_del_hdr(ipa3_ctx->excp_hdr_hdl);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_cmd);
}

#ifdef CONFIG_COMPAT
long compat_ipa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct ipa3_ioc_nat_alloc_mem32 nat_mem32;
	struct ipa_ioc_nat_alloc_mem nat_mem;

	switch (cmd) {
	case IPA_IOC_ADD_HDR32:
		cmd = IPA_IOC_ADD_HDR;
		break;
	case IPA_IOC_DEL_HDR32:
		cmd = IPA_IOC_DEL_HDR;
		break;
	case IPA_IOC_ADD_RT_RULE32:
		cmd = IPA_IOC_ADD_RT_RULE;
		break;
	case IPA_IOC_DEL_RT_RULE32:
		cmd = IPA_IOC_DEL_RT_RULE;
		break;
	case IPA_IOC_ADD_FLT_RULE32:
		cmd = IPA_IOC_ADD_FLT_RULE;
		break;
	case IPA_IOC_DEL_FLT_RULE32:
		cmd = IPA_IOC_DEL_FLT_RULE;
		break;
	case IPA_IOC_GET_RT_TBL32:
		cmd = IPA_IOC_GET_RT_TBL;
		break;
	case IPA_IOC_COPY_HDR32:
		cmd = IPA_IOC_COPY_HDR;
		break;
	case IPA_IOC_QUERY_INTF32:
		cmd = IPA_IOC_QUERY_INTF;
		break;
	case IPA_IOC_QUERY_INTF_TX_PROPS32:
		cmd = IPA_IOC_QUERY_INTF_TX_PROPS;
		break;
	case IPA_IOC_QUERY_INTF_RX_PROPS32:
		cmd = IPA_IOC_QUERY_INTF_RX_PROPS;
		break;
	case IPA_IOC_QUERY_INTF_EXT_PROPS32:
		cmd = IPA_IOC_QUERY_INTF_EXT_PROPS;
		break;
	case IPA_IOC_GET_HDR32:
		cmd = IPA_IOC_GET_HDR;
		break;
	case IPA_IOC_ALLOC_NAT_MEM32:
		if (copy_from_user((u8 *)&nat_mem32, (u8 *)arg,
			sizeof(struct ipa3_ioc_nat_alloc_mem32))) {
			retval = -EFAULT;
			goto ret;
		}
		memcpy(nat_mem.dev_name, nat_mem32.dev_name,
				IPA_RESOURCE_NAME_MAX);
		nat_mem.size = (size_t)nat_mem32.size;
		nat_mem.offset = (off_t)nat_mem32.offset;

		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		if (ipa3_allocate_nat_device(&nat_mem)) {
			retval = -EFAULT;
			goto ret;
		}
		nat_mem32.offset = (compat_off_t)nat_mem.offset;
		if (copy_to_user((u8 *)arg, (u8 *)&nat_mem32,
			sizeof(struct ipa3_ioc_nat_alloc_mem32))) {
			retval = -EFAULT;
		}
ret:
		return retval;
	case IPA_IOC_V4_INIT_NAT32:
		cmd = IPA_IOC_V4_INIT_NAT;
		break;
	case IPA_IOC_NAT_DMA32:
		cmd = IPA_IOC_NAT_DMA;
		break;
	case IPA_IOC_V4_DEL_NAT32:
		cmd = IPA_IOC_V4_DEL_NAT;
		break;
	case IPA_IOC_GET_NAT_OFFSET32:
		cmd = IPA_IOC_GET_NAT_OFFSET;
		break;
	case IPA_IOC_PULL_MSG32:
		cmd = IPA_IOC_PULL_MSG;
		break;
	case IPA_IOC_RM_ADD_DEPENDENCY32:
		cmd = IPA_IOC_RM_ADD_DEPENDENCY;
		break;
	case IPA_IOC_RM_DEL_DEPENDENCY32:
		cmd = IPA_IOC_RM_DEL_DEPENDENCY;
		break;
	case IPA_IOC_GENERATE_FLT_EQ32:
		cmd = IPA_IOC_GENERATE_FLT_EQ;
		break;
	case IPA_IOC_QUERY_RT_TBL_INDEX32:
		cmd = IPA_IOC_QUERY_RT_TBL_INDEX;
		break;
	case IPA_IOC_WRITE_QMAPID32:
		cmd = IPA_IOC_WRITE_QMAPID;
		break;
	case IPA_IOC_MDFY_FLT_RULE32:
		cmd = IPA_IOC_MDFY_FLT_RULE;
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD32:
		cmd = IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD;
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL32:
		cmd = IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL;
		break;
	case IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED32:
		cmd = IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED;
		break;
	case IPA_IOC_MDFY_RT_RULE32:
		cmd = IPA_IOC_MDFY_RT_RULE;
		break;
	case IPA_IOC_COMMIT_HDR:
	case IPA_IOC_RESET_HDR:
	case IPA_IOC_COMMIT_RT:
	case IPA_IOC_RESET_RT:
	case IPA_IOC_COMMIT_FLT:
	case IPA_IOC_RESET_FLT:
	case IPA_IOC_DUMP:
	case IPA_IOC_PUT_RT_TBL:
	case IPA_IOC_PUT_HDR:
	case IPA_IOC_SET_FLT:
	case IPA_IOC_QUERY_EP_MAPPING:
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return ipa3_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct file_operations ipa3_drv_fops = {
	.owner = THIS_MODULE,
	.open = ipa3_open,
	.read = ipa3_read,
	.unlocked_ioctl = ipa3_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ipa_ioctl,
#endif
};

static int ipa3_get_clks(struct device *dev)
{
	ipa3_clk = clk_get(dev, "core_clk");
	if (IS_ERR(ipa3_clk)) {
		if (ipa3_clk != ERR_PTR(-EPROBE_DEFER))
			IPAERR("fail to get ipa clk\n");
		return PTR_ERR(ipa3_clk);
	}

	if (smmu_present && arm_smmu) {
		smmu_clk = clk_get(dev, "smmu_clk");
		if (IS_ERR(smmu_clk)) {
			if (smmu_clk != ERR_PTR(-EPROBE_DEFER))
				IPAERR("fail to get smmu clk\n");
			return PTR_ERR(smmu_clk);
		}

		if (clk_get_rate(smmu_clk) == 0) {
			long rate = clk_round_rate(smmu_clk, 1000);

			clk_set_rate(smmu_clk, rate);
		}
	}

	return 0;
}

/**
 * _ipa_enable_clks_v3_0() - Enable IPA clocks.
 */
void _ipa_enable_clks_v3_0(void)
{
	IPADBG("enabling gcc_ipa_clk\n");
	if (ipa3_clk) {
		clk_prepare(ipa3_clk);
		clk_enable(ipa3_clk);
		IPADBG("curr_ipa_clk_rate=%d", ipa3_ctx->curr_ipa_clk_rate);
		clk_set_rate(ipa3_clk, ipa3_ctx->curr_ipa_clk_rate);
		ipa3_uc_notify_clk_state(true);
	} else {
		WARN_ON(1);
	}

	if (smmu_clk)
		clk_prepare_enable(smmu_clk);
}

static unsigned int ipa3_get_bus_vote(void)
{
	unsigned int idx = 1;

	if (ipa3_ctx->curr_ipa_clk_rate == ipa3_ctx->ctrl->ipa_clk_rate_svs) {
		idx = 1;
	} else if (ipa3_ctx->curr_ipa_clk_rate ==
			ipa3_ctx->ctrl->ipa_clk_rate_nominal) {
		if (ipa3_ctx->ctrl->msm_bus_data_ptr->num_usecases <= 2)
			idx = 1;
		else
			idx = 2;
	} else if (ipa3_ctx->curr_ipa_clk_rate ==
			ipa3_ctx->ctrl->ipa_clk_rate_turbo) {
		idx = ipa3_ctx->ctrl->msm_bus_data_ptr->num_usecases - 1;
	} else {
		WARN_ON(1);
	}

	IPADBG("curr %d idx %d\n", ipa3_ctx->curr_ipa_clk_rate, idx);

	return idx;
}

/**
* ipa3_enable_clks() - Turn on IPA clocks
*
* Return codes:
* None
*/
void ipa3_enable_clks(void)
{
	IPADBG("enabling IPA clocks and bus voting\n");

	ipa3_ctx->ctrl->ipa3_enable_clks();

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
		if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl,
		    ipa3_get_bus_vote()))
			WARN_ON(1);
}


/**
 * _ipa_disable_clks_v3_0() - Disable IPA clocks.
 */
void _ipa_disable_clks_v3_0(void)
{
	IPADBG("disabling gcc_ipa_clk\n");
	ipa3_uc_notify_clk_state(false);
	if (ipa3_clk)
		clk_disable_unprepare(ipa3_clk);
	else
		WARN_ON(1);

	if (smmu_clk)
		clk_disable_unprepare(smmu_clk);
}

/**
* ipa3_disable_clks() - Turn off IPA clocks
*
* Return codes:
* None
*/
void ipa3_disable_clks(void)
{
	IPADBG("disabling IPA clocks and bus voting\n");

	ipa3_ctx->ctrl->ipa3_disable_clks();

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
		if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl,
		    0))
			WARN_ON(1);
}

/**
 * ipa3_start_tag_process() - Send TAG packet and wait for it to come back
 *
 * This function is called prior to clock gating when active client counter
 * is 1. TAG process ensures that there are no packets inside IPA HW that
 * were not submitted to peer's BAM. During TAG process all aggregation frames
 * are (force) closed.
 *
 * Return codes:
 * None
 */
static void ipa3_start_tag_process(struct work_struct *work)
{
	int res;

	IPADBG("starting TAG process\n");
	/* close aggregation frames on all pipes */
	res = ipa3_tag_aggr_force_close(-1);
	if (res)
		IPAERR("ipa3_tag_aggr_force_close failed %d\n", res);

	ipa3_dec_client_disable_clks();

	IPADBG("TAG process done\n");
}

/**
* ipa3_inc_client_enable_clks() - Increase active clients counter, and
* enable ipa clocks if necessary
*
* Return codes:
* None
*/
void ipa3_inc_client_enable_clks(void)
{
	ipa3_active_clients_lock();
	ipa3_ctx->ipa3_active_clients.cnt++;
	if (ipa3_ctx->ipa3_active_clients.cnt == 1)
		ipa3_enable_clks();
	IPADBG("active clients = %d\n", ipa3_ctx->ipa3_active_clients.cnt);
	ipa3_active_clients_unlock();
}

/**
* ipa3_inc_client_enable_clks_no_block() - Only increment the number of active
* clients if no asynchronous actions should be done. Asynchronous actions are
* locking a mutex and waking up IPA HW.
*
* Return codes: 0 for success
*		-EPERM if an asynchronous action should have been done
*/
int ipa3_inc_client_enable_clks_no_block(void)
{
	int res = 0;
	unsigned long flags;

	if (ipa3_active_clients_trylock(&flags) == 0)
		return -EPERM;

	if (ipa3_ctx->ipa3_active_clients.cnt == 0) {
		res = -EPERM;
		goto bail;
	}

	ipa3_ctx->ipa3_active_clients.cnt++;
	IPADBG("active clients = %d\n", ipa3_ctx->ipa3_active_clients.cnt);
bail:
	ipa3_active_clients_trylock_unlock(&flags);

	return res;
}

/**
 * ipa3_dec_client_disable_clks() - Decrease active clients counter
 *
 * In case that there are no active clients this function also starts
 * TAG process. When TAG progress ends ipa clocks will be gated.
 * start_tag_process_again flag is set during this function to signal TAG
 * process to start again as there was another client that may send data to ipa
 *
 * Return codes:
 * None
 */
void ipa3_dec_client_disable_clks(void)
{
	ipa3_active_clients_lock();
	ipa3_ctx->ipa3_active_clients.cnt--;
	IPADBG("active clients = %d\n", ipa3_ctx->ipa3_active_clients.cnt);
	if (ipa3_ctx->ipa3_active_clients.cnt == 0) {
		if (ipa3_ctx->tag_process_before_gating) {
			ipa3_ctx->tag_process_before_gating = false;
			/*
			 * When TAG process ends, active clients will be
			 * decreased
			 */
			ipa3_ctx->ipa3_active_clients.cnt = 1;
			queue_work(ipa3_ctx->power_mgmt_wq, &ipa3_tag_work);
		} else {
			ipa3_disable_clks();
		}
	}
	ipa3_active_clients_unlock();
}

int ipa3_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
				  u32 bandwidth_mbps)
{
	enum ipa_voltage_level needed_voltage;
	u32 clk_rate;

	IPADBG("floor_voltage=%d, bandwidth_mbps=%u",
					floor_voltage, bandwidth_mbps);

	if (floor_voltage < IPA_VOLTAGE_UNSPECIFIED ||
		floor_voltage >= IPA_VOLTAGE_MAX) {
		IPAERR("bad voltage\n");
		return -EINVAL;
	}

	if (ipa3_ctx->enable_clock_scaling) {
		IPADBG("Clock scaling is enabled\n");
		if (bandwidth_mbps >=
			ipa3_ctx->ctrl->clock_scaling_bw_threshold_turbo)
			needed_voltage = IPA_VOLTAGE_TURBO;
		else if (bandwidth_mbps >=
			ipa3_ctx->ctrl->clock_scaling_bw_threshold_nominal)
			needed_voltage = IPA_VOLTAGE_NOMINAL;
		else
			needed_voltage = IPA_VOLTAGE_SVS;
	} else {
		IPADBG("Clock scaling is disabled\n");
		needed_voltage = IPA_VOLTAGE_NOMINAL;
	}

	needed_voltage = max(needed_voltage, floor_voltage);
	switch (needed_voltage) {
	case IPA_VOLTAGE_SVS:
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_svs;
		break;
	case IPA_VOLTAGE_NOMINAL:
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_nominal;
		break;
	case IPA_VOLTAGE_TURBO:
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_turbo;
		break;
	default:
		IPAERR("bad voltage\n");
		WARN_ON(1);
		return -EFAULT;
	}

	if (clk_rate == ipa3_ctx->curr_ipa_clk_rate) {
		IPADBG("Same voltage\n");
		return 0;
	}

	ipa3_active_clients_lock();
	ipa3_ctx->curr_ipa_clk_rate = clk_rate;
	IPADBG("setting clock rate to %u\n", ipa3_ctx->curr_ipa_clk_rate);
	if (ipa3_ctx->ipa3_active_clients.cnt > 0) {
		clk_set_rate(ipa3_clk, ipa3_ctx->curr_ipa_clk_rate);
		if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
			if (msm_bus_scale_client_update_request(
			    ipa3_ctx->ipa_bus_hdl, ipa3_get_bus_vote()))
				WARN_ON(1);
	} else {
		IPADBG("clocks are gated, not setting rate\n");
	}
	ipa3_active_clients_unlock();
	IPADBG("Done\n");
	return 0;
}

/**
* ipa3_suspend_handler() - Handles the suspend interrupt:
* wakes up the suspended peripheral by requesting its consumer
* @interrupt:		Interrupt type
* @private_data:	The client's private data
* @interrupt_data:	Interrupt specific information data
*/
void ipa3_suspend_handler(enum ipa_irq_type interrupt,
				void *private_data,
				void *interrupt_data)
{
	enum ipa_rm_resource_name resource;
	u32 suspend_data =
		((struct ipa_tx_suspend_irq_data *)interrupt_data)->endpoints;
	u32 bmsk = 1;
	u32 i = 0;

	IPADBG("interrupt=%d, interrupt_data=%u\n", interrupt, suspend_data);
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if ((suspend_data & bmsk) && (ipa3_ctx->ep[i].valid)) {
			resource = ipa3_get_rm_resource_from_ep(i);
			ipa3_rm_request_resource_with_timer(resource);
		}
		bmsk = bmsk << 1;
	}
}

static void ipa3_sps_process_irq_schedule_rel(void)
{
	ipa3_ctx->sps_pm.res_rel_in_prog = true;
	queue_delayed_work(ipa3_ctx->sps_power_mgmt_wq,
			   &ipa3_sps_release_resource_work,
			   msecs_to_jiffies(IPA_SPS_PROD_TIMEOUT_MSEC));
}

static void ipa3_sps_process_irq(struct work_struct *work)
{
	unsigned long flags;
	int ret;

	/* request IPA clocks */
	ipa3_inc_client_enable_clks();

	/* mark SPS resource as granted */
	spin_lock_irqsave(&ipa3_ctx->sps_pm.lock, flags);
	ipa3_ctx->sps_pm.res_granted = true;
	IPADBG("IPA is ON, calling sps driver\n");

	/* process bam irq */
	ret = sps_bam_process_irq(ipa3_ctx->bam_handle);
	if (ret)
		IPAERR("sps_process_eot_event failed %d\n", ret);

	/* release IPA clocks */
	ipa3_sps_process_irq_schedule_rel();
	spin_unlock_irqrestore(&ipa3_ctx->sps_pm.lock, flags);
}

static int ipa3_apps_cons_release_resource(void)
{
	return 0;
}

static int ipa3_apps_cons_request_resource(void)
{
	return 0;
}

static void ipa3_sps_release_resource(struct work_struct *work)
{
	unsigned long flags;
	bool dec_clients = false;

	spin_lock_irqsave(&ipa3_ctx->sps_pm.lock, flags);
	/* check whether still need to decrease client usage */
	if (ipa3_ctx->sps_pm.res_rel_in_prog) {
		dec_clients = true;
		ipa3_ctx->sps_pm.res_rel_in_prog = false;
		ipa3_ctx->sps_pm.res_granted = false;
	}
	spin_unlock_irqrestore(&ipa3_ctx->sps_pm.lock, flags);
	if (dec_clients)
		ipa3_dec_client_disable_clks();
}

int ipa3_create_apps_resource(void)
{
	struct ipa_rm_create_params apps_cons_create_params;
	struct ipa_rm_perf_profile profile;
	int result = 0;

	memset(&apps_cons_create_params, 0,
				sizeof(apps_cons_create_params));
	apps_cons_create_params.name = IPA_RM_RESOURCE_APPS_CONS;
	apps_cons_create_params.request_resource =
		ipa3_apps_cons_request_resource;
	apps_cons_create_params.release_resource =
		ipa3_apps_cons_release_resource;
	result = ipa3_rm_create_resource(&apps_cons_create_params);
	if (result) {
		IPAERR("ipa3_rm_create_resource failed\n");
		return result;
	}

	profile.max_supported_bandwidth_mbps = IPA_APPS_MAX_BW_IN_MBPS;
	ipa3_rm_set_perf_profile(IPA_RM_RESOURCE_APPS_CONS, &profile);

	return result;
}

/**
 * ipa3_sps_event_cb() - Handles SPS events
 * @event: event to handle
 * @param: event-specific paramer
 *
 * This callback support the following events:
 *	- SPS_CALLBACK_BAM_RES_REQ: request resource
 *		Try to increase IPA active client counter.
 *		In case this can be done synchronously then
 *		return in *param true. Otherwise return false in *param
 *		and request IPA clocks. Later call to
 *		sps_bam_process_irq to process the pending irq.
 *	- SPS_CALLBACK_BAM_RES_REL: release resource
 *		schedule a delayed work for decreasing IPA active client
 *		counter. In case that during this time another request arrives,
 *		this work will be canceled.
 */
static void ipa3_sps_event_cb(enum sps_callback_case event, void *param)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->sps_pm.lock, flags);

	switch (event) {
	case SPS_CALLBACK_BAM_RES_REQ:
	{
		bool *ready = (bool *)param;

		/* make sure no release will happen */
		cancel_delayed_work(&ipa3_sps_release_resource_work);
		ipa3_ctx->sps_pm.res_rel_in_prog = false;

		if (ipa3_ctx->sps_pm.res_granted) {
			*ready = true;
		} else {
			if (ipa3_inc_client_enable_clks_no_block() == 0) {
				ipa3_ctx->sps_pm.res_granted = true;
				*ready = true;
			} else {
				queue_work(ipa3_ctx->sps_power_mgmt_wq,
					   &ipa3_sps_process_irq_work);
				*ready = false;
			}
		}
		break;
	}

	case SPS_CALLBACK_BAM_RES_REL:
		ipa3_sps_process_irq_schedule_rel();
		break;
	default:
		IPADBG("unsupported event %d\n", event);
	}

	spin_unlock_irqrestore(&ipa3_ctx->sps_pm.lock, flags);
}

/**
 * ipa3_destroy_flt_tbl_idrs() - destroy the idr structure for flt tables
 *  The idr strcuture per filtering table is intended for rule id generation
 *  per filtering rule.
 */
static void ipa3_destroy_flt_tbl_idrs(void)
{
	int i;
	struct ipa3_flt_tbl *flt_tbl;

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v4];
		idr_destroy(&flt_tbl->rule_ids);
		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v6];
		idr_destroy(&flt_tbl->rule_ids);
	}
}

/**
* ipa3_init() - Initialize the IPA Driver
* @resource_p:	contain platform specific values from DST file
* @pdev:	The platform device structure representing the IPA driver
*
* Function initialization process:
* - Allocate memory for the driver context data struct
* - Initializing the ipa3_ctx with:
*    1)parsed values from the dts file
*    2)parameters passed to the module initialization
*    3)read HW values(such as core memory size)
* - Map IPA core registers to CPU memory
* - Restart IPA core(HW reset)
* - Register IPA BAM to SPS driver and get a BAM handler
* - Set configuration for IPA BAM via BAM_CNFG_BITS
* - Initialize the look-aside caches(kmem_cache/slab) for filter,
*   routing and IPA-tree
* - Create memory pool with 4 objects for DMA operations(each object
*   is 512Bytes long), this object will be use for tx(A5->IPA)
* - Initialize lists head(routing,filter,hdr,system pipes)
* - Initialize mutexes (for ipa_ctx and NAT memory mutexes)
* - Initialize spinlocks (for list related to A5<->IPA pipes)
* - Initialize 2 single-threaded work-queue named "ipa rx wq" and "ipa tx wq"
* - Initialize Red-Black-Tree(s) for handles of header,routing rule,
*   routing table ,filtering rule
* - Setup all A5<->IPA pipes by calling to ipa_setup_a5_pipes
* - Preparing the descriptors for System pipes
* - Initialize the filter block by committing IPV4 and IPV6 default rules
* - Create empty routing table in system memory(no committing)
* - Initialize pipes memory pool with ipa3_pipe_mem_init for supported platforms
* - Create a char-device for IPA
* - Initialize IPA RM (resource manager)
*/
static int ipa3_init(const struct ipa3_plat_drv_res *resource_p,
		struct device *ipa_dev)
{
	int result = 0;
	int i;
	struct sps_bam_props bam_props = { 0 };
	struct ipa3_flt_tbl *flt_tbl;
	struct ipa3_rt_tbl_set *rset;

	IPADBG("IPA Driver initialization started\n");

	/*
	 * since structure alignment is implementation dependent, add test to
	 * avoid different and incompatible data layouts
	 */
	BUILD_BUG_ON(sizeof(struct ipa3_hw_pkt_status) != IPA_PKT_STATUS_SIZE);

	ipa3_ctx = kzalloc(sizeof(*ipa3_ctx), GFP_KERNEL);
	if (!ipa3_ctx) {
		IPAERR(":kzalloc err.\n");
		result = -ENOMEM;
		goto fail_mem_ctx;
	}

	ipa3_ctx->pdev = ipa_dev;
	ipa3_ctx->uc_pdev = ipa_dev;
	ipa3_ctx->smmu_present = smmu_present;
	ipa3_ctx->ipa_wrapper_base = resource_p->ipa_mem_base;
	ipa3_ctx->ipa_hw_type = resource_p->ipa_hw_type;
	ipa3_ctx->ipa3_hw_mode = resource_p->ipa3_hw_mode;
	ipa3_ctx->use_ipa_teth_bridge = resource_p->use_ipa_teth_bridge;
	ipa3_ctx->ipa_bam_remote_mode = resource_p->ipa_bam_remote_mode;
	ipa3_ctx->modem_cfg_emb_pipe_flt = resource_p->modem_cfg_emb_pipe_flt;
	ipa3_ctx->wan_rx_ring_size = resource_p->wan_rx_ring_size;
	ipa3_ctx->skip_uc_pipe_reset = resource_p->skip_uc_pipe_reset;

	/* default aggregation parameters */
	ipa3_ctx->aggregation_type = IPA_MBIM_16;
	ipa3_ctx->aggregation_byte_limit = 1;
	ipa3_ctx->aggregation_time_limit = 0;

	ipa3_ctx->ctrl = kzalloc(sizeof(*ipa3_ctx->ctrl), GFP_KERNEL);
	if (!ipa3_ctx->ctrl) {
		IPAERR("memory allocation error for ctrl\n");
		result = -ENOMEM;
		goto fail_mem_ctrl;
	}
	result = ipa3_controller_static_bind(ipa3_ctx->ctrl,
			ipa3_ctx->ipa_hw_type);
	if (result) {
		IPAERR("fail to static bind IPA ctrl.\n");
		result = -EFAULT;
		goto fail_bind;
	}

	if (ipa3_bus_scale_table) {
		IPADBG("Use bus scaling info from device tree\n");
		ipa3_ctx->ctrl->msm_bus_data_ptr = ipa3_bus_scale_table;
	}

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL) {
		/* get BUS handle */
		ipa3_ctx->ipa_bus_hdl =
			msm_bus_scale_register_client(
				ipa3_ctx->ctrl->msm_bus_data_ptr);
		if (!ipa3_ctx->ipa_bus_hdl) {
			IPAERR("fail to register with bus mgr!\n");
			result = -ENODEV;
			goto fail_bus_reg;
		}
	} else {
		IPADBG("Skipping bus scaling registration on Virtual plat\n");
	}

	/* get IPA clocks */
	result = ipa3_get_clks(master_dev);
	if (result)
		goto fail_clk;

	/* Enable ipa3_ctx->enable_clock_scaling */
	ipa3_ctx->enable_clock_scaling = 1;
	ipa3_ctx->curr_ipa_clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_turbo;

	/* enable IPA clocks explicitly to allow the initialization */
	ipa3_enable_clks();

	/* setup IPA register access */
	IPADBG("Mapping 0x%x\n", resource_p->ipa_mem_base +
		ipa3_ctx->ctrl->ipa_reg_base_ofst);
	ipa3_ctx->mmio = ioremap(resource_p->ipa_mem_base +
			ipa3_ctx->ctrl->ipa_reg_base_ofst,
			resource_p->ipa_mem_size);
	if (!ipa3_ctx->mmio) {
		IPAERR(":ipa-base ioremap err.\n");
		result = -EFAULT;
		goto fail_remap;
	}

	result = ipa3_init_hw();
	if (result) {
		IPAERR(":error initializing HW.\n");
		result = -ENODEV;
		goto fail_init_hw;
	}
	IPADBG("IPA HW initialization sequence completed");

	ipa3_ctx->ipa_num_pipes = ipa3_get_num_pipes();
	if (ipa3_ctx->ipa_num_pipes > IPA3_MAX_NUM_PIPES) {
		IPAERR("IPA has more pipes then supported! has %d, max %d\n",
			ipa3_ctx->ipa_num_pipes, IPA3_MAX_NUM_PIPES);
		result = -ENODEV;
		goto fail_init_hw;
	}

	ipa_init_ep_flt_bitmap();
	IPADBG("EP with flt support bitmap 0x%x (%u pipes)\n",
		ipa3_ctx->ep_flt_bitmap, ipa3_ctx->ep_flt_num);

	ipa3_ctx->ctrl->ipa_sram_read_settings();
	IPADBG("SRAM, size: 0x%x, restricted bytes: 0x%x\n",
		ipa3_ctx->smem_sz, ipa3_ctx->smem_restricted_bytes);

	IPADBG("hdr_lcl=%u ip4_rt_hash=%u ip4_rt_nonhash=%u\n",
		ipa3_ctx->hdr_tbl_lcl, ipa3_ctx->ip4_rt_tbl_hash_lcl,
		ipa3_ctx->ip4_rt_tbl_nhash_lcl);

	IPADBG("ip6_rt_hash=%u ip6_rt_nonhash=%u\n",
		ipa3_ctx->ip6_rt_tbl_hash_lcl, ipa3_ctx->ip6_rt_tbl_nhash_lcl);

	IPADBG("ip4_flt_hash=%u ip4_flt_nonhash=%u\n",
		ipa3_ctx->ip4_flt_tbl_hash_lcl,
		ipa3_ctx->ip4_flt_tbl_nhash_lcl);

	IPADBG("ip6_flt_hash=%u ip6_flt_nonhash=%u\n",
		ipa3_ctx->ip6_flt_tbl_hash_lcl,
		ipa3_ctx->ip6_flt_tbl_nhash_lcl);

	if (ipa3_ctx->smem_reqd_sz > ipa3_ctx->smem_sz) {
		IPAERR("SW expect more core memory, needed %d, avail %d\n",
			ipa3_ctx->smem_reqd_sz, ipa3_ctx->smem_sz);
		result = -ENOMEM;
		goto fail_init_hw;
	}

	mutex_init(&ipa3_ctx->ipa3_active_clients.mutex);
	spin_lock_init(&ipa3_ctx->ipa3_active_clients.spinlock);
	ipa3_ctx->ipa3_active_clients.cnt = 1;

	/* Create workqueues for power management */
	ipa3_ctx->power_mgmt_wq =
		create_singlethread_workqueue("ipa_power_mgmt");
	if (!ipa3_ctx->power_mgmt_wq) {
		IPAERR("failed to create power mgmt wq\n");
		result = -ENOMEM;
		goto fail_init_hw;
	}

	ipa3_ctx->sps_power_mgmt_wq =
		create_singlethread_workqueue("sps_ipa_power_mgmt");
	if (!ipa3_ctx->sps_power_mgmt_wq) {
		IPAERR("failed to create sps power mgmt wq\n");
		result = -ENOMEM;
		goto fail_create_sps_wq;
	}

	spin_lock_init(&ipa3_ctx->sps_pm.lock);
	ipa3_ctx->sps_pm.res_granted = false;
	ipa3_ctx->sps_pm.res_rel_in_prog = false;

	/* register IPA with SPS driver */
	bam_props.phys_addr = resource_p->bam_mem_base;
	bam_props.virt_size = resource_p->bam_mem_size;
	bam_props.irq = resource_p->bam_irq;
	bam_props.num_pipes = ipa3_ctx->ipa_num_pipes;
	bam_props.summing_threshold = IPA_SUMMING_THRESHOLD;
	bam_props.event_threshold = IPA_EVENT_THRESHOLD;
	bam_props.options |= SPS_BAM_NO_LOCAL_CLK_GATING;
	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
		bam_props.options |= SPS_BAM_OPT_IRQ_WAKEUP;
	bam_props.options |= SPS_BAM_RES_CONFIRM;
	if (ipa3_ctx->ipa_bam_remote_mode == true)
		bam_props.manage |= SPS_BAM_MGR_DEVICE_REMOTE;
	if (ipa3_ctx->smmu_present)
		bam_props.options |= SPS_BAM_SMMU_EN;
	bam_props.ee = resource_p->ee;
	bam_props.callback = ipa3_sps_event_cb;
	bam_props.ipc_loglevel = 2;

	result = sps_register_bam_device(&bam_props, &ipa3_ctx->bam_handle);
	if (result) {
		IPAERR(":bam register err.\n");
		result = -EPROBE_DEFER;
		goto fail_register_bam_device;
	}
	IPADBG("IPA BAM is registered\n");

	/* init the lookaside cache */
	ipa3_ctx->flt_rule_cache = kmem_cache_create("IPA FLT",
			sizeof(struct ipa3_flt_entry), 0, 0, NULL);
	if (!ipa3_ctx->flt_rule_cache) {
		IPAERR(":ipa flt cache create failed\n");
		result = -ENOMEM;
		goto fail_flt_rule_cache;
	}
	ipa3_ctx->rt_rule_cache = kmem_cache_create("IPA RT",
			sizeof(struct ipa3_rt_entry), 0, 0, NULL);
	if (!ipa3_ctx->rt_rule_cache) {
		IPAERR(":ipa rt cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_rule_cache;
	}
	ipa3_ctx->hdr_cache = kmem_cache_create("IPA HDR",
			sizeof(struct ipa3_hdr_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_cache) {
		IPAERR(":ipa hdr cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_cache;
	}
	ipa3_ctx->hdr_offset_cache =
	   kmem_cache_create("IPA HDR OFFSET",
			   sizeof(struct ipa3_hdr_offset_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_offset_cache) {
		IPAERR(":ipa hdr off cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_offset_cache;
	}
	ipa3_ctx->hdr_proc_ctx_cache = kmem_cache_create("IPA HDR PROC CTX",
		sizeof(struct ipa3_hdr_proc_ctx_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_proc_ctx_cache) {
		IPAERR(":ipa hdr proc ctx cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_proc_ctx_cache;
	}
	ipa3_ctx->hdr_proc_ctx_offset_cache =
		kmem_cache_create("IPA HDR PROC CTX OFFSET",
		sizeof(struct ipa3_hdr_proc_ctx_offset_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_proc_ctx_offset_cache) {
		IPAERR(":ipa hdr proc ctx off cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_proc_ctx_offset_cache;
	}
	ipa3_ctx->rt_tbl_cache = kmem_cache_create("IPA RT TBL",
			sizeof(struct ipa3_rt_tbl), 0, 0, NULL);
	if (!ipa3_ctx->rt_tbl_cache) {
		IPAERR(":ipa rt tbl cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_tbl_cache;
	}
	ipa3_ctx->tx_pkt_wrapper_cache =
	   kmem_cache_create("IPA TX PKT WRAPPER",
			   sizeof(struct ipa3_tx_pkt_wrapper), 0, 0, NULL);
	if (!ipa3_ctx->tx_pkt_wrapper_cache) {
		IPAERR(":ipa tx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_tx_pkt_wrapper_cache;
	}
	ipa3_ctx->rx_pkt_wrapper_cache =
	   kmem_cache_create("IPA RX PKT WRAPPER",
			   sizeof(struct ipa3_rx_pkt_wrapper), 0, 0, NULL);
	if (!ipa3_ctx->rx_pkt_wrapper_cache) {
		IPAERR(":ipa rx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_rx_pkt_wrapper_cache;
	}

	/* Setup DMA pool */
	ipa3_ctx->dma_pool = dma_pool_create("ipa_tx", ipa3_ctx->pdev,
		IPA_NUM_DESC_PER_SW_TX * sizeof(struct sps_iovec),
		0, 0);
	if (!ipa3_ctx->dma_pool) {
		IPAERR("cannot alloc DMA pool.\n");
		result = -ENOMEM;
		goto fail_dma_pool;
	}

	/* init the various list heads */
	INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_hdr_entry_list);
	for (i = 0; i < IPA_HDR_BIN_MAX; i++) {
		INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa3_ctx->hdr_proc_ctx_tbl.head_proc_ctx_entry_list);
	for (i = 0; i < IPA_HDR_PROC_CTX_BIN_MAX; i++) {
		INIT_LIST_HEAD(&ipa3_ctx->hdr_proc_ctx_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(&ipa3_ctx->
				hdr_proc_ctx_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa3_ctx->rt_tbl_set[IPA_IP_v4].head_rt_tbl_list);
	INIT_LIST_HEAD(&ipa3_ctx->rt_tbl_set[IPA_IP_v6].head_rt_tbl_list);
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v4];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys[IPA_RULE_HASHABLE] =
			!ipa3_ctx->ip4_flt_tbl_hash_lcl;
		flt_tbl->in_sys[IPA_RULE_NON_HASHABLE] =
			!ipa3_ctx->ip4_flt_tbl_nhash_lcl;
		idr_init(&flt_tbl->rule_ids);

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v6];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys[IPA_RULE_HASHABLE] =
			!ipa3_ctx->ip6_flt_tbl_hash_lcl;
		flt_tbl->in_sys[IPA_RULE_NON_HASHABLE] =
			!ipa3_ctx->ip6_flt_tbl_nhash_lcl;
		idr_init(&flt_tbl->rule_ids);
	}

	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v4];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);
	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v6];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);

	INIT_LIST_HEAD(&ipa3_ctx->intf_list);
	INIT_LIST_HEAD(&ipa3_ctx->msg_list);
	INIT_LIST_HEAD(&ipa3_ctx->pull_msg_list);
	init_waitqueue_head(&ipa3_ctx->msg_waitq);
	mutex_init(&ipa3_ctx->msg_lock);

	mutex_init(&ipa3_ctx->lock);
	mutex_init(&ipa3_ctx->nat_mem.lock);

	idr_init(&ipa3_ctx->ipa_idr);
	spin_lock_init(&ipa3_ctx->idr_lock);

	/* wlan related member */
	memset(&ipa3_ctx->wc_memb, 0, sizeof(ipa3_ctx->wc_memb));
	spin_lock_init(&ipa3_ctx->wc_memb.wlan_spinlock);
	spin_lock_init(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);
	INIT_LIST_HEAD(&ipa3_ctx->wc_memb.wlan_comm_desc_list);
	/*
	 * setup an empty routing table in system memory, this will be used
	 * to delete a routing table cleanly and safely
	 */
	ipa3_ctx->empty_rt_tbl_mem.size = IPA_HW_TBL_WIDTH;

	ipa3_ctx->empty_rt_tbl_mem.base =
		dma_alloc_coherent(ipa3_ctx->pdev,
				ipa3_ctx->empty_rt_tbl_mem.size,
				    &ipa3_ctx->empty_rt_tbl_mem.phys_base,
				    GFP_KERNEL);
	if (!ipa3_ctx->empty_rt_tbl_mem.base) {
		IPAERR("DMA buff alloc fail %d bytes for empty routing tbl\n",
				ipa3_ctx->empty_rt_tbl_mem.size);
		result = -ENOMEM;
		goto fail_apps_pipes;
	}
	if (ipa3_ctx->empty_rt_tbl_mem.phys_base &
		IPA_HW_TBL_SYSADDR_ALIGNMENT) {
		IPAERR("Empty routing table buf is not address aligned 0x%x\n",
				ipa3_ctx->empty_rt_tbl_mem.phys_base);
		result = -EFAULT;
		goto fail_empty_rt_tbl;
	}
	memset(ipa3_ctx->empty_rt_tbl_mem.base, 0,
			ipa3_ctx->empty_rt_tbl_mem.size);
	IPADBG("empty routing table was allocated in system memory");

	/* setup the AP-IPA pipes */
	if (ipa3_setup_apps_pipes()) {
		IPAERR(":failed to setup IPA-Apps pipes.\n");
		result = -ENODEV;
		goto fail_empty_rt_tbl;
	}
	IPADBG("IPA System2Bam pipes were connected\n");

	/* setup the IPA pipe mem pool */
	if (resource_p->ipa_pipe_mem_size)
		ipa3_pipe_mem_init(resource_p->ipa_pipe_mem_start_ofst,
				resource_p->ipa_pipe_mem_size);

	ipa3_ctx->class = class_create(THIS_MODULE, DRV_NAME);

	result = alloc_chrdev_region(&ipa3_ctx->dev_num, 0, 1, DRV_NAME);
	if (result) {
		IPAERR("alloc_chrdev_region err.\n");
		result = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	ipa3_ctx->dev = device_create(ipa3_ctx->class, NULL, ipa3_ctx->dev_num,
			ipa3_ctx, DRV_NAME);
	if (IS_ERR(ipa3_ctx->dev)) {
		IPAERR(":device_create err.\n");
		result = -ENODEV;
		goto fail_device_create;
	}

	cdev_init(&ipa3_ctx->cdev, &ipa3_drv_fops);
	ipa3_ctx->cdev.owner = THIS_MODULE;
	ipa3_ctx->cdev.ops = &ipa3_drv_fops;  /* from LDD3 */

	result = cdev_add(&ipa3_ctx->cdev, ipa3_ctx->dev_num, 1);
	if (result) {
		IPAERR(":cdev_add err=%d\n", -result);
		result = -ENODEV;
		goto fail_cdev_add;
	}
	IPADBG("ipa cdev added successful. major:%d minor:%d\n",
			MAJOR(ipa3_ctx->dev_num),
			MINOR(ipa3_ctx->dev_num));

	if (ipa3_create_nat_device()) {
		IPAERR("unable to create nat device\n");
		result = -ENODEV;
		goto fail_nat_dev_add;
	}

	/* Create workqueue for power management */
	ipa3_ctx->power_mgmt_wq =
		create_singlethread_workqueue("ipa_power_mgmt");
	if (!ipa3_ctx->power_mgmt_wq) {
		IPAERR("failed to create wq\n");
		result = -ENOMEM;
		goto fail_init_hw;
	}

	/* Initialize IPA RM (resource manager) */
	result = ipa3_rm_initialize();
	if (result) {
		IPAERR("RM initialization failed (%d)\n", -result);
		result = -ENODEV;
		goto fail_ipa_rm_init;
	}
	IPADBG("IPA resource manager initialized");

	result = ipa3_create_apps_resource();
	if (result) {
		IPAERR("Failed to create APPS_CONS resource\n");
		result = -ENODEV;
		goto fail_create_apps_resource;
	}

	/*register IPA IRQ handler*/
	result = ipa3_interrupts_init(resource_p->ipa_irq, 0,
			master_dev);
	if (result) {
		IPAERR("ipa interrupts initialization failed\n");
		result = -ENODEV;
		goto fail_ipa_interrupts_init;
	}

	/*add handler for suspend interrupt*/
	result = ipa3_add_interrupt_handler(IPA_TX_SUSPEND_IRQ,
			ipa3_suspend_handler, true, NULL);
	if (result) {
		IPAERR("register handler for suspend interrupt failed\n");
		result = -ENODEV;
		goto fail_add_interrupt_handler;
	}

	if (ipa3_ctx->use_ipa_teth_bridge) {
		/* Initialize the tethering bridge driver */
		result = ipa3_teth_bridge_driver_init();
		if (result) {
			IPAERR(":teth_bridge init failed (%d)\n", -result);
			result = -ENODEV;
			goto fail_add_interrupt_handler;
		}
		IPADBG("teth_bridge initialized");
	}

	ipa3_debugfs_init();

	result = ipa3_uc_interface_init();
	if (result)
		IPAERR(":ipa Uc interface init failed (%d)\n", -result);
	else
		IPADBG(":ipa Uc interface init ok\n");

	result = ipa3_wdi_init();
	if (result)
		IPAERR(":wdi init failed (%d)\n", -result);
	else
		IPADBG(":wdi init ok\n");

	ipa3_ctx->q6_proxy_clk_vote_valid = true;

	ipa3_register_panic_hdlr();

	pr_info("IPA driver initialization was successful.\n");

	return 0;

fail_add_interrupt_handler:
	free_irq(resource_p->ipa_irq, master_dev);
fail_ipa_interrupts_init:
	ipa3_rm_delete_resource(IPA_RM_RESOURCE_APPS_CONS);
fail_create_apps_resource:
	ipa3_rm_exit();
fail_ipa_rm_init:
fail_nat_dev_add:
	cdev_del(&ipa3_ctx->cdev);
fail_cdev_add:
	device_destroy(ipa3_ctx->class, ipa3_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(ipa3_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	if (ipa3_ctx->pipe_mem_pool)
		gen_pool_destroy(ipa3_ctx->pipe_mem_pool);
fail_empty_rt_tbl:
	ipa3_teardown_apps_pipes();
	dma_free_coherent(ipa3_ctx->pdev,
			  ipa3_ctx->empty_rt_tbl_mem.size,
			  ipa3_ctx->empty_rt_tbl_mem.base,
			  ipa3_ctx->empty_rt_tbl_mem.phys_base);
fail_apps_pipes:
	ipa3_destroy_flt_tbl_idrs();
	idr_destroy(&ipa3_ctx->ipa_idr);
fail_dma_pool:
	kmem_cache_destroy(ipa3_ctx->rx_pkt_wrapper_cache);
fail_rx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa3_ctx->tx_pkt_wrapper_cache);
fail_tx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa3_ctx->rt_tbl_cache);
fail_rt_tbl_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_proc_ctx_offset_cache);
fail_hdr_proc_ctx_offset_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_proc_ctx_cache);
fail_hdr_proc_ctx_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_offset_cache);
fail_hdr_offset_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_cache);
fail_hdr_cache:
	kmem_cache_destroy(ipa3_ctx->rt_rule_cache);
fail_rt_rule_cache:
	kmem_cache_destroy(ipa3_ctx->flt_rule_cache);
fail_flt_rule_cache:
	sps_deregister_bam_device(ipa3_ctx->bam_handle);
fail_register_bam_device:
	destroy_workqueue(ipa3_ctx->sps_power_mgmt_wq);
fail_create_sps_wq:
	destroy_workqueue(ipa3_ctx->power_mgmt_wq);
fail_init_hw:
	iounmap(ipa3_ctx->mmio);
fail_remap:
	ipa3_disable_clks();
fail_clk:
	msm_bus_scale_unregister_client(ipa3_ctx->ipa_bus_hdl);
fail_bus_reg:
	if (ipa3_bus_scale_table) {
		msm_bus_cl_clear_pdata(ipa3_bus_scale_table);
		ipa3_bus_scale_table = NULL;
	}
fail_bind:
	kfree(ipa3_ctx->ctrl);
fail_mem_ctrl:
	kfree(ipa3_ctx);
	ipa3_ctx = NULL;
fail_mem_ctx:
	return result;
}

static int get_ipa_dts_configuration(struct platform_device *pdev,
		struct ipa3_plat_drv_res *ipa_drv_res)
{
	int result;
	struct resource *resource;

	/* initialize ipa3_res */
	ipa_drv_res->ipa_pipe_mem_start_ofst = IPA_PIPE_MEM_START_OFST;
	ipa_drv_res->ipa_pipe_mem_size = IPA_PIPE_MEM_SIZE;
	ipa_drv_res->ipa_hw_type = 0;
	ipa_drv_res->ipa3_hw_mode = 0;
	ipa_drv_res->ipa_bam_remote_mode = false;
	ipa_drv_res->modem_cfg_emb_pipe_flt = false;
	ipa_drv_res->wan_rx_ring_size = IPA_GENERIC_RX_POOL_SZ;

	smmu_disable_htw = of_property_read_bool(pdev->dev.of_node,
			"qcom,smmu-disable-htw");

	/* Get IPA HW Version */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ipa-hw-ver",
					&ipa_drv_res->ipa_hw_type);
	if ((result) || (ipa_drv_res->ipa_hw_type == 0)) {
		IPAERR(":get resource failed for ipa-hw-ver!\n");
		return -ENODEV;
	}
	IPADBG(": ipa_hw_type = %d", ipa_drv_res->ipa_hw_type);

	if (ipa_drv_res->ipa_hw_type < IPA_HW_v3_0) {
		IPAERR(":IPA version below 3.0 not supported!\n");
		return -ENODEV;
	}

	/* Get IPA HW mode */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ipa-hw-mode",
			&ipa_drv_res->ipa3_hw_mode);
	if (result)
		IPADBG("using default (IPA_MODE_NORMAL) for ipa-hw-mode\n");
	else
		IPADBG(": found ipa_drv_res->ipa3_hw_mode = %d",
				ipa_drv_res->ipa3_hw_mode);

	/* Get IPA WAN RX pool sizee */
	result = of_property_read_u32(pdev->dev.of_node,
			"qcom,wan-rx-ring-size",
			&ipa_drv_res->wan_rx_ring_size);
	if (result)
		IPADBG("using default for wan-rx-ring-size\n");
	else
		IPADBG(": found ipa_drv_res->wan-rx-ring-size = %u",
				ipa_drv_res->wan_rx_ring_size);

	ipa_drv_res->use_ipa_teth_bridge =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,use-ipa-tethering-bridge");
	IPADBG(": using TBDr = %s",
		ipa_drv_res->use_ipa_teth_bridge
		? "True" : "False");

	ipa_drv_res->ipa_bam_remote_mode =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-bam-remote-mode");
	IPADBG(": ipa bam remote mode = %s\n",
			ipa_drv_res->ipa_bam_remote_mode
			? "True" : "False");

	ipa_drv_res->modem_cfg_emb_pipe_flt =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,modem-cfg-emb-pipe-flt");
	IPADBG(": modem configure embedded pipe filtering = %s\n",
			ipa_drv_res->modem_cfg_emb_pipe_flt
			? "True" : "False");

	ipa_drv_res->skip_uc_pipe_reset =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,skip-uc-pipe-reset");
	IPADBG(": skip uC pipe reset = %s\n",
		ipa_drv_res->skip_uc_pipe_reset
		? "True" : "False");

	/* Get IPA wrapper address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"ipa-base");
	if (!resource) {
		IPAERR(":get resource failed for ipa-base!\n");
		return -ENODEV;
	}
	ipa_drv_res->ipa_mem_base = resource->start;
	ipa_drv_res->ipa_mem_size = resource_size(resource);
	IPADBG(": ipa-base = 0x%x, size = 0x%x\n",
			ipa_drv_res->ipa_mem_base,
			ipa_drv_res->ipa_mem_size);

	/* Get IPA BAM address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"bam-base");
	if (!resource) {
		IPAERR(":get resource failed for bam-base!\n");
		return -ENODEV;
	}
	ipa_drv_res->bam_mem_base = resource->start;
	ipa_drv_res->bam_mem_size = resource_size(resource);
	IPADBG(": bam-base = 0x%x, size = 0x%x\n",
			ipa_drv_res->bam_mem_base,
			ipa_drv_res->bam_mem_size);

	/* Get IPA pipe mem start ofst */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"ipa-pipe-mem");
	if (!resource) {
		IPADBG(":not using pipe memory - resource nonexisting\n");
	} else {
		ipa_drv_res->ipa_pipe_mem_start_ofst = resource->start;
		ipa_drv_res->ipa_pipe_mem_size = resource_size(resource);
		IPADBG(":using pipe memory - at 0x%x of size 0x%x\n",
				ipa_drv_res->ipa_pipe_mem_start_ofst,
				ipa_drv_res->ipa_pipe_mem_size);
	}

	/* Get IPA IRQ number */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"ipa-irq");
	if (!resource) {
		IPAERR(":get resource failed for ipa-irq!\n");
		return -ENODEV;
	}
	ipa_drv_res->ipa_irq = resource->start;
	IPADBG(":ipa-irq = %d\n", ipa_drv_res->ipa_irq);

	/* Get IPA BAM IRQ number */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"bam-irq");
	if (!resource) {
		IPAERR(":get resource failed for bam-irq!\n");
		return -ENODEV;
	}
	ipa_drv_res->bam_irq = resource->start;
	IPADBG(":ibam-irq = %d\n", ipa_drv_res->bam_irq);

	result = of_property_read_u32(pdev->dev.of_node, "qcom,ee",
			&ipa_drv_res->ee);
	if (result)
		ipa_drv_res->ee = 0;

	return 0;
}

static int ipa_smmu_wlan_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = &smmu_cb[IPA_SMMU_CB_WLAN];
	int disable_htw = 1;
	int atomic_ctx = 1;
	int ret;

	IPADBG("sub pdev=%p\n", dev);

	cb->dev = dev;
	cb->iommu = iommu_domain_alloc(&platform_bus_type);
	if (!cb->iommu) {
		IPAERR("could not alloc iommu domain\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}

	if (smmu_disable_htw) {
		ret = iommu_domain_set_attr(cb->iommu,
			DOMAIN_ATTR_COHERENT_HTW_DISABLE,
			&disable_htw);
		if (ret) {
			IPAERR("couldn't disable coherent HTW\n");
			return -EIO;
		}
	}

	if (iommu_domain_set_attr(cb->iommu,
				DOMAIN_ATTR_ATOMIC,
				&atomic_ctx)) {
		IPAERR("couldn't disable coherent HTW\n");
		return -EIO;
	}

	ret = iommu_attach_device(cb->iommu, dev);
	if (ret) {
		IPAERR("could not attach device ret=%d\n", ret);
		return ret;
	}

	cb->valid = true;

	return 0;
}

static int ipa_smmu_uc_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = &smmu_cb[IPA_SMMU_CB_UC];
	int disable_htw = 1;
	int ret;

	IPADBG("sub pdev=%p\n", dev);

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
		    dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		IPAERR("DMA set mask failed\n");
		return -EOPNOTSUPP;
	}

	cb->dev = dev;
	cb->mapping = ipa3_arm_iommu_create_mapping(&platform_bus_type,
			IPA_SMMU_UC_VA_START, IPA_SMMU_UC_VA_SIZE);
	if (IS_ERR(cb->mapping)) {
		IPADBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}

	ret = ipa3_arm_iommu_attach_device(cb->dev, cb->mapping);
	if (ret) {
		IPAERR("could not attach device ret=%d\n", ret);
		return ret;
	}

	if (smmu_disable_htw) {
		if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				 &disable_htw)) {
			IPAERR("couldn't disable coherent HTW\n");
			ipa3_arm_iommu_detach_device(cb->dev);
			return -EIO;
		}
	}

	cb->valid = true;
	cb->next_addr = IPA_SMMU_UC_VA_END;
	ipa3_ctx->uc_pdev = dev;

	return 0;
}

static int ipa_smmu_ap_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = &smmu_cb[IPA_SMMU_CB_AP];
	int result;
	int disable_htw = 1;
	int atomic_ctx = 1;

	IPADBG("sub pdev=%p\n", dev);

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
		    dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		IPAERR("DMA set mask failed\n");
		return -EOPNOTSUPP;
	}

	cb->dev = dev;
	cb->mapping = ipa3_arm_iommu_create_mapping(&platform_bus_type,
			IPA_SMMU_AP_VA_START, IPA_SMMU_AP_VA_SIZE);
	if (IS_ERR(cb->mapping)) {
		IPADBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}

	if (smmu_disable_htw) {
		if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				 &disable_htw)) {
			IPAERR("couldn't disable coherent HTW\n");
			ipa3_arm_iommu_detach_device(cb->dev);
			return -EIO;
		}
	}

	if (iommu_domain_set_attr(cb->mapping->domain,
				  DOMAIN_ATTR_ATOMIC,
				  &atomic_ctx)) {
		IPAERR("couldn't set domain as atomic\n");
		ipa3_arm_iommu_detach_device(cb->dev);
		return -EIO;
	}

	result = ipa3_arm_iommu_attach_device(cb->dev, cb->mapping);
	if (result) {
		IPAERR("couldn't attach to IOMMU ret=%d\n", result);
		return result;
	}

	cb->valid = true;
	smmu_present = true;

	if (!ipa3_bus_scale_table)
		ipa3_bus_scale_table = msm_bus_cl_get_pdata(ipa3_pdev);

	/* Proceed to real initialization */
	result = ipa3_init(&ipa3_res, dev);
	if (result) {
		IPAERR("ipa_init failed\n");
		ipa3_arm_iommu_detach_device(cb->dev);
		arm_iommu_release_mapping(cb->mapping);
		return result;
	}

	return result;
}

int ipa3_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl, struct of_device_id *pdrv_match)
{
	int result;
	struct device *dev = &pdev_p->dev;

	IPADBG("IPA driver probing started\n");

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-ap-cb"))
		return ipa_smmu_ap_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-wlan-cb"))
		return ipa_smmu_wlan_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-uc-cb"))
		return ipa_smmu_uc_cb_probe(dev);

	master_dev = dev;
	if (!ipa3_pdev)
		ipa3_pdev = pdev_p;

	result = get_ipa_dts_configuration(pdev_p, &ipa3_res);
	if (result) {
		IPAERR("IPA dts parsing failed\n");
		return result;
	}

	result = ipa3_bind_api_controller(ipa3_res.ipa_hw_type, api_ctrl);
	if (result) {
		IPAERR("IPA API binding failed\n");
		return result;
	}

	if (of_property_read_bool(pdev_p->dev.of_node, "qcom,arm-smmu")) {
		arm_smmu = true;
		result = of_platform_populate(pdev_p->dev.of_node,
				pdrv_match, NULL, &pdev_p->dev);
	} else if (of_property_read_bool(pdev_p->dev.of_node,
				"qcom,msm-smmu")) {
		IPAERR("Legacy IOMMU not supported\n");
		result = -EOPNOTSUPP;
	} else {
		if (dma_set_mask(&pdev_p->dev, DMA_BIT_MASK(32)) ||
			    dma_set_coherent_mask(&pdev_p->dev,
			    DMA_BIT_MASK(32))) {
			IPAERR("DMA set mask failed\n");
			return -EOPNOTSUPP;
		}

		if (!ipa3_bus_scale_table)
			ipa3_bus_scale_table = msm_bus_cl_get_pdata(pdev_p);

		/* Proceed to real initialization */
		result = ipa3_init(&ipa3_res, dev);
		if (result) {
			IPAERR("ipa3_init failed\n");
			return result;
		}
	}

	return result;
}

/**
 * ipa3_ap_suspend() - suspend callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP suspend
 * operation is invoked, usually by pressing a suspend button.
 *
 * Returns -EAGAIN to runtime_pm framework in case IPA is in use by AP.
 * This will postpone the suspend operation until IPA is no longer used by AP.
*/
int ipa3_ap_suspend(struct device *dev)
{
	int i;

	IPADBG("Enter...\n");
	/*
	 * In case SPS requested IPA resources fail to suspend.
	 * This can happen if SPS driver is during the processing of
	 * IPA BAM interrupt
	 */
	if (ipa3_ctx->sps_pm.res_granted && !ipa3_ctx->sps_pm.res_rel_in_prog) {
		IPAERR("SPS resource is granted, do not suspend\n");
		return -EAGAIN;
	}

	/* In case there is a tx/rx handler in polling mode fail to suspend */
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (ipa3_ctx->ep[i].sys &&
			atomic_read(&ipa3_ctx->ep[i].sys->curr_polling_state)) {
			IPAERR("EP %d is in polling state, do not suspend\n",
				i);
			return -EAGAIN;
		}
	}

	/* release SPS IPA resource without waiting for inactivity timer */
	ipa3_sps_release_resource(NULL);
	IPADBG("Exit\n");

	return 0;
}

/**
* ipa3_ap_resume() - resume callback for runtime_pm
* @dev: pointer to device
*
* This callback will be invoked by the runtime_pm framework when an AP resume
* operation is invoked.
*
* Always returns 0 since resume should always succeed.
*/
int ipa3_ap_resume(struct device *dev)
{
	return 0;
}

struct ipa3_context *ipa3_get_ctx(void)
{
	return ipa3_ctx;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW device driver");
