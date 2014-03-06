/*
 * arch/arm/kernel/kprobes.h
 *
 * Copyright (C) 2011 Jon Medhurst <tixy@yxit.co.uk>.
 *
 * Some contents moved here from arch/arm/include/asm/kprobes.h which is
 * Copyright (C) 2006, 2007 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _ARM_KERNEL_KPROBES_H
#define _ARM_KERNEL_KPROBES_H

/*
 * These undefined instructions must be unique and
 * reserved solely for kprobes' use.
 */
#define KPROBE_ARM_BREAKPOINT_INSTRUCTION	0x07f001f8
#define KPROBE_THUMB16_BREAKPOINT_INSTRUCTION	0xde18
#define KPROBE_THUMB32_BREAKPOINT_INSTRUCTION	0xf7f0a018

struct decode_header;
union decode_action;

typedef enum probes_insn (kprobe_decode_insn_t)(probes_opcode_t,
						struct arch_specific_insn *,
						const union decode_action *);

#ifdef CONFIG_THUMB2_KERNEL

enum probes_insn thumb16_kprobe_decode_insn(probes_opcode_t,
					    struct arch_specific_insn *,
					    const union decode_action *);
enum probes_insn thumb32_kprobe_decode_insn(probes_opcode_t,
					    struct arch_specific_insn *,
					    const union decode_action *);

#else /* !CONFIG_THUMB2_KERNEL */

enum probes_insn arm_kprobe_decode_insn(probes_opcode_t,
					struct arch_specific_insn *,
					const union decode_action *);

#endif

#include "probes.h"

#endif /* _ARM_KERNEL_KPROBES_H */
