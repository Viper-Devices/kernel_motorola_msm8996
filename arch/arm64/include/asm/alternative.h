#ifndef __ASM_ALTERNATIVE_H
#define __ASM_ALTERNATIVE_H

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/stringify.h>

struct alt_instr {
	s32 orig_offset;	/* offset to original instruction */
	s32 alt_offset;		/* offset to replacement instruction */
	u16 cpufeature;		/* cpufeature bit set for replacement */
	u8  orig_len;		/* size of original instruction(s) */
	u8  alt_len;		/* size of new instruction(s), <= orig_len */
};

void apply_alternatives(void);
void free_alternatives_memory(void);

#define ALTINSTR_ENTRY(feature)						      \
	" .word 661b - .\n"				/* label           */ \
	" .word 663f - .\n"				/* new instruction */ \
	" .hword " __stringify(feature) "\n"		/* feature bit     */ \
	" .byte 662b-661b\n"				/* source len      */ \
	" .byte 664f-663f\n"				/* replacement len */

/* alternative assembly primitive: */
#define ALTERNATIVE(oldinstr, newinstr, feature)			\
	"661:\n\t"							\
	oldinstr "\n"							\
	"662:\n"							\
	".pushsection .altinstructions,\"a\"\n"				\
	ALTINSTR_ENTRY(feature)						\
	".popsection\n"							\
	".pushsection .altinstr_replacement, \"a\"\n"			\
	"663:\n\t"							\
	newinstr "\n"							\
	"664:\n\t"							\
	".popsection\n\t"						\
	".if ((664b-663b) != (662b-661b))\n\t"				\
	"	.error \"Alternatives instruction length mismatch\"\n\t"\
	".endif\n"

#else

.macro altinstruction_entry orig_offset alt_offset feature orig_len alt_len
	.word \orig_offset - .
	.word \alt_offset - .
	.hword \feature
	.byte \orig_len
	.byte \alt_len
.endm

.macro alternative_insn insn1 insn2 cap
661:	\insn1
662:	.pushsection .altinstructions, "a"
	altinstruction_entry 661b, 663f, \cap, 662b-661b, 664f-663f
	.popsection
	.pushsection .altinstr_replacement, "ax"
663:	\insn2
664:	.popsection
	.if ((664b-663b) != (662b-661b))
		.error "Alternatives instruction length mismatch"
	.endif
.endm

#endif  /*  __ASSEMBLY__  */

#endif /* __ASM_ALTERNATIVE_H */
