/*
 *  prepare to run common code
 *
 *  Copyright (C) 2000 Andrea Arcangeli <andrea@suse.de> SuSE
 */

#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/percpu.h>
#include <linux/start_kernel.h>

#include <asm/processor.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/setup.h>
#include <asm/desc.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/kdebug.h>
#include <asm/e820.h>

static void __init zap_identity_mappings(void)
{
	pgd_t *pgd = pgd_offset_k(0UL);
	pgd_clear(pgd);
	__flush_tlb();
}

/* Don't add a printk in there. printk relies on the PDA which is not initialized 
   yet. */
static void __init clear_bss(void)
{
	memset(__bss_start, 0,
	       (unsigned long) __bss_stop - (unsigned long) __bss_start);
}

static void __init copy_bootdata(char *real_mode_data)
{
	char * command_line;

	memcpy(&boot_params, real_mode_data, sizeof boot_params);
	if (boot_params.hdr.cmd_line_ptr) {
		command_line = __va(boot_params.hdr.cmd_line_ptr);
		memcpy(boot_command_line, command_line, COMMAND_LINE_SIZE);
	}
}

#define EBDA_ADDR_POINTER 0x40E

static __init void reserve_ebda(void)
{
	unsigned ebda_addr, ebda_size;

	/*
	 * there is a real-mode segmented pointer pointing to the
	 * 4K EBDA area at 0x40E
	 */
	ebda_addr = *(unsigned short *)__va(EBDA_ADDR_POINTER);
	ebda_addr <<= 4;

	if (!ebda_addr)
		return;

	ebda_size = *(unsigned short *)__va(ebda_addr);

	/* Round EBDA up to pages */
	if (ebda_size == 0)
		ebda_size = 1;
	ebda_size <<= 10;
	ebda_size = round_up(ebda_size + (ebda_addr & ~PAGE_MASK), PAGE_SIZE);
	if (ebda_size > 64*1024)
		ebda_size = 64*1024;

	reserve_early(ebda_addr, ebda_addr + ebda_size);
}

void __init x86_64_start_kernel(char * real_mode_data)
{
	int i;

	/* clear bss before set_intr_gate with early_idt_handler */
	clear_bss();

	/* Make NULL pointers segfault */
	zap_identity_mappings();

	for (i = 0; i < IDT_ENTRIES; i++) {
#ifdef CONFIG_EARLY_PRINTK
		set_intr_gate(i, &early_idt_handlers[i]);
#else
		set_intr_gate(i, early_idt_handler);
#endif
	}
	load_idt((const struct desc_ptr *)&idt_descr);

	early_printk("Kernel alive\n");

 	for (i = 0; i < NR_CPUS; i++)
 		cpu_pda(i) = &boot_cpu_pda[i];

	pda_init(0);
	copy_bootdata(__va(real_mode_data));

	reserve_early(__pa_symbol(&_text), __pa_symbol(&_end));

	/* Reserve INITRD */
	if (boot_params.hdr.type_of_loader && boot_params.hdr.ramdisk_image) {
		unsigned long ramdisk_image = boot_params.hdr.ramdisk_image;
		unsigned long ramdisk_size  = boot_params.hdr.ramdisk_size;
		unsigned long ramdisk_end   = ramdisk_image + ramdisk_size;
		reserve_early(ramdisk_image, ramdisk_end);
	}

	reserve_ebda();

	/*
	 * At this point everything still needed from the boot loader
	 * or BIOS or kernel text should be early reserved or marked not
	 * RAM in e820. All other memory is free game.
	 */

	start_kernel();
}
