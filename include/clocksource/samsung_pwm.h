/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __CLOCKSOURCE_SAMSUNG_PWM_H
#define __CLOCKSOURCE_SAMSUNG_PWM_H

#include <linux/spinlock.h>

#define SAMSUNG_PWM_NUM		5

extern spinlock_t samsung_pwm_lock;

struct samsung_pwm_variant {
	u8 bits;
	u8 div_base;
	u8 tclk_mask;
	u8 output_mask;
	bool has_tint_cstat;
};

#endif /* __CLOCKSOURCE_SAMSUNG_PWM_H */
