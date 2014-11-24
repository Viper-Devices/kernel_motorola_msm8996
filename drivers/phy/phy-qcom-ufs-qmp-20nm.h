/*
 * Copyright (c) 2013-2014, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef UFS_QCOM_PHY_QMP_20NM_H_
#define UFS_QCOM_PHY_QMP_20NM_H_

#include <linux/phy/phy-qcom-ufs.h>

/* QCOM UFS PHY control registers */

#define COM_OFF(x)     (0x000 + x)
#define PHY_OFF(x)     (0xC00 + x)
#define TX_OFF(n, x)   (0x400 + (0x400 * n) + x)
#define RX_OFF(n, x)   (0x600 + (0x400 * n) + x)

/* UFS PHY PLL block registers */
#define QSERDES_COM_SYS_CLK_CTRL		COM_OFF(0x0)
#define QSERDES_COM_PLL_VCOTAIL_EN		COM_OFF(0x04)
#define QSERDES_COM_PLL_CNTRL			COM_OFF(0x14)
#define QSERDES_COM_PLL_IP_SETI			COM_OFF(0x24)
#define QSERDES_COM_CORE_CLK_IN_SYNC_SEL	COM_OFF(0x28)
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN		COM_OFF(0x30)
#define QSERDES_COM_PLL_CP_SETI			COM_OFF(0x34)
#define QSERDES_COM_PLL_IP_SETP			COM_OFF(0x38)
#define QSERDES_COM_PLL_CP_SETP			COM_OFF(0x3C)
#define QSERDES_COM_SYSCLK_EN_SEL_TXBAND	COM_OFF(0x48)
#define QSERDES_COM_RESETSM_CNTRL		COM_OFF(0x4C)
#define QSERDES_COM_RESETSM_CNTRL2		COM_OFF(0x50)
#define QSERDES_COM_PLLLOCK_CMP1		COM_OFF(0x90)
#define QSERDES_COM_PLLLOCK_CMP2		COM_OFF(0x94)
#define QSERDES_COM_PLLLOCK_CMP3		COM_OFF(0x98)
#define QSERDES_COM_PLLLOCK_CMP_EN		COM_OFF(0x9C)
#define QSERDES_COM_BGTC			COM_OFF(0xA0)
#define QSERDES_COM_DEC_START1			COM_OFF(0xAC)
#define QSERDES_COM_PLL_AMP_OS			COM_OFF(0xB0)
#define QSERDES_COM_RES_CODE_UP_OFFSET		COM_OFF(0xD8)
#define QSERDES_COM_RES_CODE_DN_OFFSET		COM_OFF(0xDC)
#define QSERDES_COM_DIV_FRAC_START1		COM_OFF(0x100)
#define QSERDES_COM_DIV_FRAC_START2		COM_OFF(0x104)
#define QSERDES_COM_DIV_FRAC_START3		COM_OFF(0x108)
#define QSERDES_COM_DEC_START2			COM_OFF(0x10C)
#define QSERDES_COM_PLL_RXTXEPCLK_EN		COM_OFF(0x110)
#define QSERDES_COM_PLL_CRCTRL			COM_OFF(0x114)
#define QSERDES_COM_PLL_CLKEPDIV		COM_OFF(0x118)

/* TX LANE n (0, 1) registers */
#define QSERDES_TX_EMP_POST1_LVL(n)		TX_OFF(n, 0x08)
#define QSERDES_TX_DRV_LVL(n)			TX_OFF(n, 0x0C)
#define QSERDES_TX_LANE_MODE(n)			TX_OFF(n, 0x54)

/* RX LANE n (0, 1) registers */
#define QSERDES_RX_CDR_CONTROL1(n)		RX_OFF(n, 0x0)
#define QSERDES_RX_CDR_CONTROL_HALF(n)		RX_OFF(n, 0x8)
#define QSERDES_RX_RX_EQ_GAIN1_LSB(n)		RX_OFF(n, 0xA8)
#define QSERDES_RX_RX_EQ_GAIN1_MSB(n)		RX_OFF(n, 0xAC)
#define QSERDES_RX_RX_EQ_GAIN2_LSB(n)		RX_OFF(n, 0xB0)
#define QSERDES_RX_RX_EQ_GAIN2_MSB(n)		RX_OFF(n, 0xB4)
#define QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2(n)	RX_OFF(n, 0xBC)
#define QSERDES_RX_CDR_CONTROL_QUARTER(n)	RX_OFF(n, 0xC)
#define QSERDES_RX_SIGDET_CNTRL(n)		RX_OFF(n, 0x100)

/* UFS PHY registers */
#define UFS_PHY_PHY_START			PHY_OFF(0x00)
#define UFS_PHY_POWER_DOWN_CONTROL		PHY_OFF(0x4)
#define UFS_PHY_TX_LANE_ENABLE			PHY_OFF(0x44)
#define UFS_PHY_PWM_G1_CLK_DIVIDER		PHY_OFF(0x08)
#define UFS_PHY_PWM_G2_CLK_DIVIDER		PHY_OFF(0x0C)
#define UFS_PHY_PWM_G3_CLK_DIVIDER		PHY_OFF(0x10)
#define UFS_PHY_PWM_G4_CLK_DIVIDER		PHY_OFF(0x14)
#define UFS_PHY_CORECLK_PWM_G1_CLK_DIVIDER	PHY_OFF(0x34)
#define UFS_PHY_CORECLK_PWM_G2_CLK_DIVIDER	PHY_OFF(0x38)
#define UFS_PHY_CORECLK_PWM_G3_CLK_DIVIDER	PHY_OFF(0x3C)
#define UFS_PHY_CORECLK_PWM_G4_CLK_DIVIDER	PHY_OFF(0x40)
#define UFS_PHY_OMC_STATUS_RDVAL		PHY_OFF(0x68)
#define UFS_PHY_LINE_RESET_TIME			PHY_OFF(0x28)
#define UFS_PHY_LINE_RESET_GRANULARITY		PHY_OFF(0x2C)
#define UFS_PHY_TSYNC_RSYNC_CNTL		PHY_OFF(0x48)
#define UFS_PHY_PLL_CNTL			PHY_OFF(0x50)
#define UFS_PHY_TX_LARGE_AMP_DRV_LVL		PHY_OFF(0x54)
#define UFS_PHY_TX_SMALL_AMP_DRV_LVL		PHY_OFF(0x5C)
#define UFS_PHY_TX_LARGE_AMP_POST_EMP_LVL	PHY_OFF(0x58)
#define UFS_PHY_TX_SMALL_AMP_POST_EMP_LVL	PHY_OFF(0x60)
#define UFS_PHY_CFG_CHANGE_CNT_VAL		PHY_OFF(0x64)
#define UFS_PHY_RX_SYNC_WAIT_TIME		PHY_OFF(0x6C)
#define UFS_PHY_TX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY	PHY_OFF(0xB4)
#define UFS_PHY_RX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY	PHY_OFF(0xE0)
#define UFS_PHY_TX_MIN_STALL_NOCONFIG_TIME_CAPABILITY	PHY_OFF(0xB8)
#define UFS_PHY_RX_MIN_STALL_NOCONFIG_TIME_CAPABILITY	PHY_OFF(0xE4)
#define UFS_PHY_TX_MIN_SAVE_CONFIG_TIME_CAPABILITY	PHY_OFF(0xBC)
#define UFS_PHY_RX_MIN_SAVE_CONFIG_TIME_CAPABILITY	PHY_OFF(0xE8)
#define UFS_PHY_RX_PWM_BURST_CLOSURE_LENGTH_CAPABILITY	PHY_OFF(0xFC)
#define UFS_PHY_RX_MIN_ACTIVATETIME_CAPABILITY		PHY_OFF(0x100)
#define UFS_PHY_RX_SIGDET_CTRL3				PHY_OFF(0x14c)
#define UFS_PHY_RMMI_ATTR_CTRL			PHY_OFF(0x160)
#define UFS_PHY_RMMI_RX_CFGUPDT_L1	(1 << 7)
#define UFS_PHY_RMMI_TX_CFGUPDT_L1	(1 << 6)
#define UFS_PHY_RMMI_CFGWR_L1		(1 << 5)
#define UFS_PHY_RMMI_CFGRD_L1		(1 << 4)
#define UFS_PHY_RMMI_RX_CFGUPDT_L0	(1 << 3)
#define UFS_PHY_RMMI_TX_CFGUPDT_L0	(1 << 2)
#define UFS_PHY_RMMI_CFGWR_L0		(1 << 1)
#define UFS_PHY_RMMI_CFGRD_L0		(1 << 0)
#define UFS_PHY_RMMI_ATTRID			PHY_OFF(0x164)
#define UFS_PHY_RMMI_ATTRWRVAL			PHY_OFF(0x168)
#define UFS_PHY_RMMI_ATTRRDVAL_L0_STATUS	PHY_OFF(0x16C)
#define UFS_PHY_RMMI_ATTRRDVAL_L1_STATUS	PHY_OFF(0x170)
#define UFS_PHY_PCS_READY_STATUS		PHY_OFF(0x174)

#define UFS_PHY_TX_LANE_ENABLE_MASK		0x3

/*
 * This structure represents the 20nm specific phy.
 * common_cfg MUST remain the first field in this structure
 * in case extra fields are added. This way, when calling
 * get_ufs_qcom_phy() of generic phy, we can extract the
 * common phy structure (struct ufs_qcom_phy) out of it
 * regardless of the relevant specific phy.
 */
struct ufs_qcom_phy_qmp_20nm {
	struct ufs_qcom_phy common_cfg;
};

static struct ufs_qcom_phy_calibration phy_cal_table_rate_A_1_2_0[] = {
	UFS_QCOM_PHY_CAL_ENTRY(UFS_PHY_POWER_DOWN_CONTROL, 0x01),
	UFS_QCOM_PHY_CAL_ENTRY(UFS_PHY_RX_SIGDET_CTRL3, 0x0D),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_VCOTAIL_EN, 0xe1),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CRCTRL, 0xcc),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_SYSCLK_EN_SEL_TXBAND, 0x08),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CLKEPDIV, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_RXTXEPCLK_EN, 0x10),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DEC_START1, 0x82),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DEC_START2, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START1, 0x80),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START2, 0x80),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START3, 0x40),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP1, 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP2, 0x19),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP3, 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP_EN, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RESETSM_CNTRL, 0x90),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RESETSM_CNTRL2, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL1(0), 0xf2),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_HALF(0), 0x0c),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_QUARTER(0), 0x12),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL1(1), 0xf2),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_HALF(1), 0x0c),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_QUARTER(1), 0x12),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_LSB(0), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_MSB(0), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_LSB(0), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_MSB(0), 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_LSB(1), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_MSB(1), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_LSB(1), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_MSB(1), 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CP_SETI, 0x3f),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_IP_SETP, 0x1b),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CP_SETP, 0x0f),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_IP_SETI, 0x01),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_EMP_POST1_LVL(0), 0x2F),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_DRV_LVL(0), 0x20),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_EMP_POST1_LVL(1), 0x2F),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_DRV_LVL(1), 0x20),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_LANE_MODE(0), 0x68),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_LANE_MODE(1), 0x68),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2(1), 0xdc),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2(0), 0xdc),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x3),
};

static struct ufs_qcom_phy_calibration phy_cal_table_rate_A_1_3_0[] = {
	UFS_QCOM_PHY_CAL_ENTRY(UFS_PHY_POWER_DOWN_CONTROL, 0x01),
	UFS_QCOM_PHY_CAL_ENTRY(UFS_PHY_RX_SIGDET_CTRL3, 0x0D),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_VCOTAIL_EN, 0xe1),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CRCTRL, 0xcc),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_SYSCLK_EN_SEL_TXBAND, 0x08),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CLKEPDIV, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_RXTXEPCLK_EN, 0x10),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DEC_START1, 0x82),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DEC_START2, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START1, 0x80),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START2, 0x80),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START3, 0x40),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP1, 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP2, 0x19),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP3, 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP_EN, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RESETSM_CNTRL, 0x90),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RESETSM_CNTRL2, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL1(0), 0xf2),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_HALF(0), 0x0c),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_QUARTER(0), 0x12),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL1(1), 0xf2),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_HALF(1), 0x0c),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_CDR_CONTROL_QUARTER(1), 0x12),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_LSB(0), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_MSB(0), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_LSB(0), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_MSB(0), 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_LSB(1), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN1_MSB(1), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_LSB(1), 0xff),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQ_GAIN2_MSB(1), 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CP_SETI, 0x2b),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_IP_SETP, 0x38),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CP_SETP, 0x3c),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RES_CODE_UP_OFFSET, 0x02),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RES_CODE_DN_OFFSET, 0x02),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_IP_SETI, 0x01),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CNTRL, 0x40),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_LANE_MODE(0), 0x68),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_TX_LANE_MODE(1), 0x68),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2(1), 0xdc),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2(0), 0xdc),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x3),
};

static struct ufs_qcom_phy_calibration phy_cal_table_rate_B[] = {
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DEC_START1, 0x98),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP1, 0x65),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP2, 0x1e),
};

#endif
