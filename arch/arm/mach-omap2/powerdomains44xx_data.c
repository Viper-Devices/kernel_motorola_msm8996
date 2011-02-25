/*
 * OMAP4 Power domains framework
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 * Copyright (C) 2009-2010 Nokia Corporation
 *
 * Abhijit Pagare (abhijitpagare@ti.com)
 * Benoit Cousson (b-cousson@ti.com)
 * Paul Walmsley (paul@pwsan.com)
 *
 * This file is automatically generated from the OMAP hardware databases.
 * We respectfully ask that any modifications to this file be coordinated
 * with the public linux-omap@vger.kernel.org mailing list and the
 * authors above to ensure that the autogeneration scripts are kept
 * up-to-date with the file contents.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>

#include "powerdomain.h"

#include "prcm-common.h"
#include "prcm44xx.h"
#include "prm-regbits-44xx.h"
#include "prm44xx.h"
#include "prcm_mpu44xx.h"

/* core_44xx_pwrdm: CORE power domain */
static struct powerdomain core_44xx_pwrdm = {
	.name		  = "core_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_CORE_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 5,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* core_nret_bank */
		[1] = PWRSTS_OFF_RET,	/* core_ocmram */
		[2] = PWRDM_POWER_RET,	/* core_other_bank */
		[3] = PWRSTS_OFF_RET,	/* ducati_l2ram */
		[4] = PWRSTS_OFF_RET,	/* ducati_unicache */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* core_nret_bank */
		[1] = PWRSTS_OFF_RET,	/* core_ocmram */
		[2] = PWRDM_POWER_ON,	/* core_other_bank */
		[3] = PWRDM_POWER_ON,	/* ducati_l2ram */
		[4] = PWRDM_POWER_ON,	/* ducati_unicache */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* gfx_44xx_pwrdm: 3D accelerator power domain */
static struct powerdomain gfx_44xx_pwrdm = {
	.name		  = "gfx_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_GFX_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_ON,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* gfx_mem */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* gfx_mem */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* abe_44xx_pwrdm: Audio back end power domain */
static struct powerdomain abe_44xx_pwrdm = {
	.name		  = "abe_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_ABE_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF,
	.banks		  = 2,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_RET,	/* aessmem */
		[1] = PWRDM_POWER_OFF,	/* periphmem */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* aessmem */
		[1] = PWRDM_POWER_ON,	/* periphmem */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* dss_44xx_pwrdm: Display subsystem power domain */
static struct powerdomain dss_44xx_pwrdm = {
	.name		  = "dss_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_DSS_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* dss_mem */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* dss_mem */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* tesla_44xx_pwrdm: Tesla processor power domain */
static struct powerdomain tesla_44xx_pwrdm = {
	.name		  = "tesla_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_TESLA_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 3,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_RET,	/* tesla_edma */
		[1] = PWRSTS_OFF_RET,	/* tesla_l1 */
		[2] = PWRSTS_OFF_RET,	/* tesla_l2 */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* tesla_edma */
		[1] = PWRDM_POWER_ON,	/* tesla_l1 */
		[2] = PWRDM_POWER_ON,	/* tesla_l2 */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* wkup_44xx_pwrdm: Wake-up power domain */
static struct powerdomain wkup_44xx_pwrdm = {
	.name		  = "wkup_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_WKUP_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_ON,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* wkup_bank */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* wkup_bank */
	},
};

/* cpu0_44xx_pwrdm: MPU0 processor and Neon coprocessor power domain */
static struct powerdomain cpu0_44xx_pwrdm = {
	.name		  = "cpu0_pwrdm",
	.prcm_offs	  = OMAP4430_PRCM_MPU_CPU0_INST,
	.prcm_partition	  = OMAP4430_PRCM_MPU_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRSTS_OFF_RET,	/* cpu0_l1 */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* cpu0_l1 */
	},
};

/* cpu1_44xx_pwrdm: MPU1 processor and Neon coprocessor power domain */
static struct powerdomain cpu1_44xx_pwrdm = {
	.name		  = "cpu1_pwrdm",
	.prcm_offs	  = OMAP4430_PRCM_MPU_CPU1_INST,
	.prcm_partition	  = OMAP4430_PRCM_MPU_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRSTS_OFF_RET,	/* cpu1_l1 */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* cpu1_l1 */
	},
};

/* emu_44xx_pwrdm: Emulation power domain */
static struct powerdomain emu_44xx_pwrdm = {
	.name		  = "emu_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_EMU_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_ON,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* emu_bank */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* emu_bank */
	},
};

/* mpu_44xx_pwrdm: Modena processor and the Neon coprocessor power domain */
static struct powerdomain mpu_44xx_pwrdm = {
	.name		  = "mpu_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_MPU_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 3,
	.pwrsts_mem_ret	= {
		[0] = PWRSTS_OFF_RET,	/* mpu_l1 */
		[1] = PWRSTS_OFF_RET,	/* mpu_l2 */
		[2] = PWRDM_POWER_RET,	/* mpu_ram */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* mpu_l1 */
		[1] = PWRDM_POWER_ON,	/* mpu_l2 */
		[2] = PWRDM_POWER_ON,	/* mpu_ram */
	},
};

/* ivahd_44xx_pwrdm: IVA-HD power domain */
static struct powerdomain ivahd_44xx_pwrdm = {
	.name		  = "ivahd_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_IVAHD_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF,
	.banks		  = 4,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* hwa_mem */
		[1] = PWRSTS_OFF_RET,	/* sl2_mem */
		[2] = PWRSTS_OFF_RET,	/* tcm1_mem */
		[3] = PWRSTS_OFF_RET,	/* tcm2_mem */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* hwa_mem */
		[1] = PWRDM_POWER_ON,	/* sl2_mem */
		[2] = PWRDM_POWER_ON,	/* tcm1_mem */
		[3] = PWRDM_POWER_ON,	/* tcm2_mem */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* cam_44xx_pwrdm: Camera subsystem power domain */
static struct powerdomain cam_44xx_pwrdm = {
	.name		  = "cam_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_CAM_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_ON,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* cam_mem */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* cam_mem */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* l3init_44xx_pwrdm: L3 initators pheripherals power domain  */
static struct powerdomain l3init_44xx_pwrdm = {
	.name		  = "l3init_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_L3INIT_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 1,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* l3init_bank1 */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* l3init_bank1 */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/* l4per_44xx_pwrdm: Target peripherals power domain */
static struct powerdomain l4per_44xx_pwrdm = {
	.name		  = "l4per_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_L4PER_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_RET_ON,
	.pwrsts_logic_ret = PWRSTS_OFF_RET,
	.banks		  = 2,
	.pwrsts_mem_ret	= {
		[0] = PWRDM_POWER_OFF,	/* nonretained_bank */
		[1] = PWRDM_POWER_RET,	/* retained_bank */
	},
	.pwrsts_mem_on	= {
		[0] = PWRDM_POWER_ON,	/* nonretained_bank */
		[1] = PWRDM_POWER_ON,	/* retained_bank */
	},
	.flags		= PWRDM_HAS_LOWPOWERSTATECHANGE,
};

/*
 * always_on_core_44xx_pwrdm: Always ON logic that sits in VDD_CORE voltage
 * domain
 */
static struct powerdomain always_on_core_44xx_pwrdm = {
	.name		  = "always_on_core_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_ALWAYS_ON_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_ON,
};

/* cefuse_44xx_pwrdm: Customer efuse controller power domain */
static struct powerdomain cefuse_44xx_pwrdm = {
	.name		  = "cefuse_pwrdm",
	.prcm_offs	  = OMAP4430_PRM_CEFUSE_INST,
	.prcm_partition	  = OMAP4430_PRM_PARTITION,
	.omap_chip	  = OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
	.pwrsts		  = PWRSTS_OFF_ON,
};

/*
 * The following power domains are not under SW control
 *
 * always_on_iva
 * always_on_mpu
 * stdefuse
 */

/* As powerdomains are added or removed above, this list must also be changed */
static struct powerdomain *powerdomains_omap44xx[] __initdata = {
	&core_44xx_pwrdm,
	&gfx_44xx_pwrdm,
	&abe_44xx_pwrdm,
	&dss_44xx_pwrdm,
	&tesla_44xx_pwrdm,
	&wkup_44xx_pwrdm,
	&cpu0_44xx_pwrdm,
	&cpu1_44xx_pwrdm,
	&emu_44xx_pwrdm,
	&mpu_44xx_pwrdm,
	&ivahd_44xx_pwrdm,
	&cam_44xx_pwrdm,
	&l3init_44xx_pwrdm,
	&l4per_44xx_pwrdm,
	&always_on_core_44xx_pwrdm,
	&cefuse_44xx_pwrdm,
	NULL
};

void __init omap44xx_powerdomains_init(void)
{
	pwrdm_init(powerdomains_omap44xx, &omap4_pwrdm_operations);
}
