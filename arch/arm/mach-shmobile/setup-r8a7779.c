/*
 * r8a7779 processor support
 *
 * Copyright (C) 2011  Renesas Solutions Corp.
 * Copyright (C) 2011  Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/serial_sci.h>
#include <linux/sh_intc.h>
#include <linux/sh_timer.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/r8a7779.h>
#include <mach/common.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

static struct plat_sci_port scif0_platform_data = {
	.mapbase	= 0xffe40000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE | SCSCR_CKE1,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		= { gic_spi(88), gic_spi(88),
			    gic_spi(88), gic_spi(88) },
};

static struct platform_device scif0_device = {
	.name		= "sh-sci",
	.id		= 0,
	.dev		= {
		.platform_data	= &scif0_platform_data,
	},
};

static struct plat_sci_port scif1_platform_data = {
	.mapbase	= 0xffe41000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE | SCSCR_CKE1,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		= { gic_spi(89), gic_spi(89),
			    gic_spi(89), gic_spi(89) },
};

static struct platform_device scif1_device = {
	.name		= "sh-sci",
	.id		= 1,
	.dev		= {
		.platform_data	= &scif1_platform_data,
	},
};

static struct plat_sci_port scif2_platform_data = {
	.mapbase	= 0xffe42000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE | SCSCR_CKE1,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		= { gic_spi(90), gic_spi(90),
			    gic_spi(90), gic_spi(90) },
};

static struct platform_device scif2_device = {
	.name		= "sh-sci",
	.id		= 2,
	.dev		= {
		.platform_data	= &scif2_platform_data,
	},
};

static struct plat_sci_port scif3_platform_data = {
	.mapbase	= 0xffe43000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE | SCSCR_CKE1,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		= { gic_spi(91), gic_spi(91),
			    gic_spi(91), gic_spi(91) },
};

static struct platform_device scif3_device = {
	.name		= "sh-sci",
	.id		= 3,
	.dev		= {
		.platform_data	= &scif3_platform_data,
	},
};

static struct plat_sci_port scif4_platform_data = {
	.mapbase	= 0xffe44000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE | SCSCR_CKE1,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		= { gic_spi(92), gic_spi(92),
			    gic_spi(92), gic_spi(92) },
};

static struct platform_device scif4_device = {
	.name		= "sh-sci",
	.id		= 4,
	.dev		= {
		.platform_data	= &scif4_platform_data,
	},
};

static struct plat_sci_port scif5_platform_data = {
	.mapbase	= 0xffe45000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE | SCSCR_CKE1,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		= { gic_spi(93), gic_spi(93),
			    gic_spi(93), gic_spi(93) },
};

static struct platform_device scif5_device = {
	.name		= "sh-sci",
	.id		= 5,
	.dev		= {
		.platform_data	= &scif5_platform_data,
	},
};

/* TMU */
static struct sh_timer_config tmu00_platform_data = {
	.name = "TMU00",
	.channel_offset = 0x4,
	.timer_bit = 0,
	.clockevent_rating = 200,
};

static struct resource tmu00_resources[] = {
	[0] = {
		.name	= "TMU00",
		.start	= 0xffd80008,
		.end	= 0xffd80013,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(32),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tmu00_device = {
	.name		= "sh_tmu",
	.id		= 0,
	.dev = {
		.platform_data	= &tmu00_platform_data,
	},
	.resource	= tmu00_resources,
	.num_resources	= ARRAY_SIZE(tmu00_resources),
};

static struct sh_timer_config tmu01_platform_data = {
	.name = "TMU01",
	.channel_offset = 0x10,
	.timer_bit = 1,
	.clocksource_rating = 200,
};

static struct resource tmu01_resources[] = {
	[0] = {
		.name	= "TMU01",
		.start	= 0xffd80014,
		.end	= 0xffd8001f,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(33),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tmu01_device = {
	.name		= "sh_tmu",
	.id		= 1,
	.dev = {
		.platform_data	= &tmu01_platform_data,
	},
	.resource	= tmu01_resources,
	.num_resources	= ARRAY_SIZE(tmu01_resources),
};

static struct platform_device *r8a7779_early_devices[] __initdata = {
	&scif0_device,
	&scif1_device,
	&scif2_device,
	&scif3_device,
	&scif4_device,
	&scif5_device,
	&tmu00_device,
	&tmu01_device,
};

static struct platform_device *r8a7779_late_devices[] __initdata = {
};

void __init r8a7779_add_standard_devices(void)
{
	r8a7779_pm_init();

	r8a7779_init_pm_domain(&r8a7779_sh4a);
	r8a7779_init_pm_domain(&r8a7779_sgx);
	r8a7779_init_pm_domain(&r8a7779_vdp1);
	r8a7779_init_pm_domain(&r8a7779_impx3);

	platform_add_devices(r8a7779_early_devices,
			    ARRAY_SIZE(r8a7779_early_devices));
	platform_add_devices(r8a7779_late_devices,
			    ARRAY_SIZE(r8a7779_late_devices));
}

void __init r8a7779_add_early_devices(void)
{
	early_platform_add_devices(r8a7779_early_devices,
				   ARRAY_SIZE(r8a7779_early_devices));
}
