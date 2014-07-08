/*

  Broadcom B43 wireless driver
  IEEE 802.11n 2057 radio device data tables

  Copyright (c) 2010 Rafał Miłecki <zajec5@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include "b43.h"
#include "radio_2057.h"
#include "phy_common.h"

static u16 r2057_rev4_init[][2] = {
	{ 0x0E, 0x20 }, { 0x31, 0x00 }, { 0x32, 0x00 }, { 0x33, 0x00 },
	{ 0x35, 0x26 }, { 0x3C, 0xff }, { 0x3D, 0xff }, { 0x3E, 0xff },
	{ 0x3F, 0xff }, { 0x62, 0x33 }, { 0x8A, 0xf0 }, { 0x8B, 0x10 },
	{ 0x8C, 0xf0 }, { 0x91, 0x3f }, { 0x92, 0x36 }, { 0xA4, 0x8c },
	{ 0xA8, 0x55 }, { 0xAF, 0x01 }, { 0x10F, 0xf0 }, { 0x110, 0x10 },
	{ 0x111, 0xf0 }, { 0x116, 0x3f }, { 0x117, 0x36 }, { 0x129, 0x8c },
	{ 0x12D, 0x55 }, { 0x134, 0x01 }, { 0x15E, 0x00 }, { 0x15F, 0x00 },
	{ 0x160, 0x00 }, { 0x161, 0x00 }, { 0x162, 0x00 }, { 0x163, 0x00 },
	{ 0x169, 0x02 }, { 0x16A, 0x00 }, { 0x16B, 0x00 }, { 0x16C, 0x00 },
	{ 0x1A4, 0x00 }, { 0x1A5, 0x00 }, { 0x1A6, 0x00 }, { 0x1AA, 0x00 },
	{ 0x1AB, 0x00 }, { 0x1AC, 0x00 },
};

static u16 r2057_rev5_init[][2] = {
	{ 0x00, 0x00 }, { 0x01, 0x57 }, { 0x02, 0x20 }, { 0x23, 0x6 },
	{ 0x31, 0x00 }, { 0x32, 0x00 }, { 0x33, 0x00 }, { 0x51, 0x70 },
	{ 0x59, 0x88 }, { 0x5C, 0x20 }, { 0x62, 0x33 }, { 0x63, 0x0f },
	{ 0x64, 0x0f }, { 0x81, 0x01 }, { 0x91, 0x3f }, { 0x92, 0x36 },
	{ 0xA1, 0x20 }, { 0xD6, 0x70 }, { 0xDE, 0x88 }, { 0xE1, 0x20 },
	{ 0xE8, 0x0f }, { 0xE9, 0x0f }, { 0x106, 0x01 }, { 0x116, 0x3f },
	{ 0x117, 0x36 }, { 0x126, 0x20 }, { 0x15E, 0x00 }, { 0x15F, 0x00 },
	{ 0x160, 0x00 }, { 0x161, 0x00 }, { 0x162, 0x00 }, { 0x163, 0x00 },
	{ 0x16A, 0x00 }, { 0x16B, 0x00 }, { 0x16C, 0x00 }, { 0x1A4, 0x00 },
	{ 0x1A5, 0x00 }, { 0x1A6, 0x00 }, { 0x1AA, 0x00 }, { 0x1AB, 0x00 },
	{ 0x1AC, 0x00 }, { 0x1B7, 0x0c }, { 0x1C1, 0x01 }, { 0x1C2, 0x80 },
};

static u16 r2057_rev5a_init[][2] = {
	{ 0x00, 0x15 }, { 0x01, 0x57 }, { 0x02, 0x20 }, { 0x23, 0x6 },
	{ 0x31, 0x00 }, { 0x32, 0x00 }, { 0x33, 0x00 }, { 0x51, 0x70 },
	{ 0x59, 0x88 }, { 0x5C, 0x20 }, { 0x62, 0x33 }, { 0x63, 0x0f },
	{ 0x64, 0x0f }, { 0x81, 0x01 }, { 0x91, 0x3f }, { 0x92, 0x36 },
	{ 0xC9, 0x01 }, { 0xD6, 0x70 }, { 0xDE, 0x88 }, { 0xE1, 0x20 },
	{ 0xE8, 0x0f }, { 0xE9, 0x0f }, { 0x106, 0x01 }, { 0x116, 0x3f },
	{ 0x117, 0x36 }, { 0x126, 0x20 }, { 0x14E, 0x01 }, { 0x15E, 0x00 },
	{ 0x15F, 0x00 }, { 0x160, 0x00 }, { 0x161, 0x00 }, { 0x162, 0x00 },
	{ 0x163, 0x00 }, { 0x16A, 0x00 }, { 0x16B, 0x00 }, { 0x16C, 0x00 },
	{ 0x1A4, 0x00 }, { 0x1A5, 0x00 }, { 0x1A6, 0x00 }, { 0x1AA, 0x00 },
	{ 0x1AB, 0x00 }, { 0x1AC, 0x00 }, { 0x1B7, 0x0c }, { 0x1C1, 0x01 },
	{ 0x1C2, 0x80 },
};

static u16 r2057_rev7_init[][2] = {
	{ 0x00, 0x00 }, { 0x01, 0x57 }, { 0x02, 0x20 }, { 0x31, 0x00 },
	{ 0x32, 0x00 }, { 0x33, 0x00 }, { 0x51, 0x70 }, { 0x59, 0x88 },
	{ 0x5C, 0x20 }, { 0x62, 0x33 }, { 0x63, 0x0f }, { 0x64, 0x13 },
	{ 0x66, 0xee }, { 0x6E, 0x58 }, { 0x75, 0x13 }, { 0x7B, 0x13 },
	{ 0x7C, 0x14 }, { 0x7D, 0xee }, { 0x81, 0x01 }, { 0x91, 0x3f },
	{ 0x92, 0x36 }, { 0xA1, 0x20 }, { 0xD6, 0x70 }, { 0xDE, 0x88 },
	{ 0xE1, 0x20 }, { 0xE8, 0x0f }, { 0xE9, 0x13 }, { 0xEB, 0xee },
	{ 0xF3, 0x58 }, { 0xFA, 0x13 }, { 0x100, 0x13 }, { 0x101, 0x14 },
	{ 0x102, 0xee }, { 0x106, 0x01 }, { 0x116, 0x3f }, { 0x117, 0x36 },
	{ 0x126, 0x20 }, { 0x15E, 0x00 }, { 0x15F, 0x00 }, { 0x160, 0x00 },
	{ 0x161, 0x00 }, { 0x162, 0x00 }, { 0x163, 0x00 }, { 0x16A, 0x00 },
	{ 0x16B, 0x00 }, { 0x16C, 0x00 }, { 0x1A4, 0x00 }, { 0x1A5, 0x00 },
	{ 0x1A6, 0x00 }, { 0x1AA, 0x00 }, { 0x1AB, 0x00 }, { 0x1AC, 0x00 },
	{ 0x1B7, 0x05 }, { 0x1C2, 0xa0 },
};

/* TODO: Which devices should use it?
static u16 r2057_rev8_init[][2] = {
	{ 0x00, 0x08 }, { 0x01, 0x57 }, { 0x02, 0x20 }, { 0x31, 0x00 },
	{ 0x32, 0x00 }, { 0x33, 0x00 }, { 0x51, 0x70 }, { 0x59, 0x88 },
	{ 0x5C, 0x20 }, { 0x62, 0x33 }, { 0x63, 0x0f }, { 0x64, 0x0f },
	{ 0x6E, 0x58 }, { 0x75, 0x13 }, { 0x7B, 0x13 }, { 0x7C, 0x0f },
	{ 0x7D, 0xee }, { 0x81, 0x01 }, { 0x91, 0x3f }, { 0x92, 0x36 },
	{ 0xA1, 0x20 }, { 0xC9, 0x01 }, { 0xD6, 0x70 }, { 0xDE, 0x88 },
	{ 0xE1, 0x20 }, { 0xE8, 0x0f }, { 0xE9, 0x0f }, { 0xF3, 0x58 },
	{ 0xFA, 0x13 }, { 0x100, 0x13 }, { 0x101, 0x0f }, { 0x102, 0xee },
	{ 0x106, 0x01 }, { 0x116, 0x3f }, { 0x117, 0x36 }, { 0x126, 0x20 },
	{ 0x14E, 0x01 }, { 0x15E, 0x00 }, { 0x15F, 0x00 }, { 0x160, 0x00 },
	{ 0x161, 0x00 }, { 0x162, 0x00 }, { 0x163, 0x00 }, { 0x16A, 0x00 },
	{ 0x16B, 0x00 }, { 0x16C, 0x00 }, { 0x1A4, 0x00 }, { 0x1A5, 0x00 },
	{ 0x1A6, 0x00 }, { 0x1AA, 0x00 }, { 0x1AB, 0x00 }, { 0x1AC, 0x00 },
	{ 0x1B7, 0x05 }, { 0x1C2, 0xa0 },
};
*/

#define RADIOREGS7(r00, r01, r02, r03, r04, r05, r06, r07, r08, r09, \
		   r10, r11, r12, r13, r14, r15, r16, r17, r18, r19, \
		   r20, r21, r22, r23, r24, r25, r26, r27) \
	.radio_vcocal_countval0			= r00,	\
	.radio_vcocal_countval1			= r01,	\
	.radio_rfpll_refmaster_sparextalsize	= r02,	\
	.radio_rfpll_loopfilter_r1		= r03,	\
	.radio_rfpll_loopfilter_c2		= r04,	\
	.radio_rfpll_loopfilter_c1		= r05,	\
	.radio_cp_kpd_idac			= r06,	\
	.radio_rfpll_mmd0			= r07,	\
	.radio_rfpll_mmd1			= r08,	\
	.radio_vcobuf_tune			= r09,	\
	.radio_logen_mx2g_tune			= r10,	\
	.radio_logen_mx5g_tune			= r11,	\
	.radio_logen_indbuf2g_tune		= r12,	\
	.radio_logen_indbuf5g_tune		= r13,	\
	.radio_txmix2g_tune_boost_pu_core0	= r14,	\
	.radio_pad2g_tune_pus_core0		= r15,	\
	.radio_pga_boost_tune_core0		= r16,	\
	.radio_txmix5g_boost_tune_core0		= r17,	\
	.radio_pad5g_tune_misc_pus_core0	= r18,	\
	.radio_lna2g_tune_core0			= r19,	\
	.radio_lna5g_tune_core0			= r20,	\
	.radio_txmix2g_tune_boost_pu_core1	= r21,	\
	.radio_pad2g_tune_pus_core1		= r22,	\
	.radio_pga_boost_tune_core1		= r23,	\
	.radio_txmix5g_boost_tune_core1		= r24,	\
	.radio_pad5g_tune_misc_pus_core1	= r25,	\
	.radio_lna2g_tune_core1			= r26,	\
	.radio_lna5g_tune_core1			= r27

#define PHYREGS(r0, r1, r2, r3, r4, r5)	\
	.phy_regs.phy_bw1a	= r0,	\
	.phy_regs.phy_bw2	= r1,	\
	.phy_regs.phy_bw3	= r2,	\
	.phy_regs.phy_bw4	= r3,	\
	.phy_regs.phy_bw5	= r4,	\
	.phy_regs.phy_bw6	= r5

void r2057_upload_inittabs(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;
	u16 *table = NULL;
	u16 size, i;

	switch (phy->rev) {
	case 7:
		table = r2057_rev4_init[0];
		size = ARRAY_SIZE(r2057_rev4_init);
		break;
	case 8:
		if (phy->radio_rev == 5) {
			table = r2057_rev5_init[0];
			size = ARRAY_SIZE(r2057_rev5_init);
		} else if (phy->radio_rev == 7) {
			table = r2057_rev7_init[0];
			size = ARRAY_SIZE(r2057_rev7_init);
		}
		break;
	case 9:
		if (phy->radio_rev == 5) {
			table = r2057_rev5a_init[0];
			size = ARRAY_SIZE(r2057_rev5a_init);
		}
		break;
	}

	B43_WARN_ON(!table);

	if (table) {
		for (i = 0; i < size; i++, table += 2)
			b43_radio_write(dev, table[0], table[1]);
	}
}

void r2057_get_chantabent_rev7(struct b43_wldev *dev, u16 freq,
			       const struct b43_nphy_chantabent_rev7 **tabent_r7,
			       const struct b43_nphy_chantabent_rev7_2g **tabent_r7_2g)
{
	struct b43_phy *phy = &dev->phy;
	const struct b43_nphy_chantabent_rev7 *e_r7 = NULL;
	const struct b43_nphy_chantabent_rev7_2g *e_r7_2g = NULL;
	unsigned int len, i;

	*tabent_r7 = NULL;
	*tabent_r7_2g = NULL;

	/* TODO */
	switch (phy->rev) {
	default:
		break;
	}

	if (e_r7) {
		for (i = 0; i < len; i++, e_r7++) {
			if (e_r7->freq == freq) {
				*tabent_r7 = e_r7;
				return;
			}
		}
	} else if (e_r7_2g) {
		for (i = 0; i < len; i++, e_r7_2g++) {
			if (e_r7_2g->freq == freq) {
				*tabent_r7_2g = e_r7_2g;
				return;
			}
		}
	} else {
		B43_WARN_ON(1);
	}
}
