/*
 * linux/arch/arm/mach-omap2/prcm.c
 *
 * OMAP 24xx Power Reset and Clock Management (PRCM) functions
 *
 * Copyright (C) 2005 Nokia Corporation
 *
 * Written by Tony Lindgren <tony.lindgren@nokia.com>
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 *
 * Some pieces of code Copyright (C) 2005 Texas Instruments, Inc.
 * Upgraded with OMAP4 support by Abhijit Pagare <abhijitpagare@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <plat/common.h>
#include <plat/prcm.h>
#include <plat/irqs.h>

#include "clock.h"
#include "clock2xxx.h"
#include "cm2xxx_3xxx.h"
#include "cm44xx.h"
#include "prm2xxx_3xxx.h"
#include "prm44xx.h"
#include "prcm44xx.h"
#include "prm-regbits-24xx.h"
#include "prm-regbits-44xx.h"
#include "control.h"

void __iomem *prm_base;
void __iomem *cm_base;
void __iomem *cm2_base;

#define MAX_MODULE_ENABLE_WAIT		100000

u32 omap_prcm_get_reset_sources(void)
{
	/* XXX This presumably needs modification for 34XX */
	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		return prm_read_mod_reg(WKUP_MOD, OMAP2_RM_RSTST) & 0x7f;
	if (cpu_is_omap44xx())
		return prm_read_mod_reg(WKUP_MOD, OMAP4_RM_RSTST) & 0x7f;

	return 0;
}
EXPORT_SYMBOL(omap_prcm_get_reset_sources);

/* Resets clock rates and reboots the system. Only called from system.h */
void omap_prcm_arch_reset(char mode, const char *cmd)
{
	s16 prcm_offs = 0;

	if (cpu_is_omap24xx()) {
		omap2xxx_clk_prepare_for_reboot();

		prcm_offs = WKUP_MOD;
	} else if (cpu_is_omap34xx()) {
		prcm_offs = OMAP3430_GR_MOD;
		omap3_ctrl_write_boot_mode((cmd ? (u8)*cmd : 0));
	} else if (cpu_is_omap44xx())
		prcm_offs = OMAP4430_PRM_DEVICE_INST;
	else
		WARN_ON(1);

	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		prm_set_mod_reg_bits(OMAP_RST_DPLL3_MASK, prcm_offs,
						 OMAP2_RM_RSTCTRL);
	if (cpu_is_omap44xx())
		prm_set_mod_reg_bits(OMAP4430_RST_GLOBAL_WARM_SW_MASK,
				     prcm_offs, OMAP4_RM_RSTCTRL);
}

/**
 * omap2_cm_wait_idlest - wait for IDLEST bit to indicate module readiness
 * @reg: physical address of module IDLEST register
 * @mask: value to mask against to determine if the module is active
 * @idlest: idle state indicator (0 or 1) for the clock
 * @name: name of the clock (for printk)
 *
 * Returns 1 if the module indicated readiness in time, or 0 if it
 * failed to enable in roughly MAX_MODULE_ENABLE_WAIT microseconds.
 *
 * XXX This function is deprecated.  It should be removed once the
 * hwmod conversion is complete.
 */
int omap2_cm_wait_idlest(void __iomem *reg, u32 mask, u8 idlest,
				const char *name)
{
	int i = 0;
	int ena = 0;

	if (idlest)
		ena = 0;
	else
		ena = mask;

	/* Wait for lock */
	omap_test_timeout(((__raw_readl(reg) & mask) == ena),
			  MAX_MODULE_ENABLE_WAIT, i);

	if (i < MAX_MODULE_ENABLE_WAIT)
		pr_debug("cm: Module associated with clock %s ready after %d "
			 "loops\n", name, i);
	else
		pr_err("cm: Module associated with clock %s didn't enable in "
		       "%d tries\n", name, MAX_MODULE_ENABLE_WAIT);

	return (i < MAX_MODULE_ENABLE_WAIT) ? 1 : 0;
};

void __init omap2_set_globals_prcm(struct omap_globals *omap2_globals)
{
	/* Static mapping, never released */
	if (omap2_globals->prm) {
		prm_base = ioremap(omap2_globals->prm, SZ_8K);
		WARN_ON(!prm_base);
	}
	if (omap2_globals->cm) {
		cm_base = ioremap(omap2_globals->cm, SZ_8K);
		WARN_ON(!cm_base);
	}
	if (omap2_globals->cm2) {
		cm2_base = ioremap(omap2_globals->cm2, SZ_8K);
		WARN_ON(!cm2_base);
	}
}
