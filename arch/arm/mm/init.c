/*
 *  linux/arch/arm/mm/init.c
 *
 *  Copyright (C) 1995-2005 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/initrd.h>

#include <asm/mach-types.h>
#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/tlb.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#define TABLE_SIZE	(2 * PTRS_PER_PTE * sizeof(pte_t))

DEFINE_PER_CPU(struct mmu_gather, mmu_gathers);

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern void _stext, _text, _etext, __data_start, _end, __init_begin, __init_end;
extern unsigned long phys_initrd_start;
extern unsigned long phys_initrd_size;

/*
 * The sole use of this is to pass memory configuration
 * data from paging_init to mem_init.
 */
static struct meminfo meminfo __initdata = { 0, };

/*
 * empty_zero_page is a special page that is used for
 * zero-initialized data and COW.
 */
struct page *empty_zero_page;

void show_mem(void)
{
	int free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0, slab = 0, node;

	printk("Mem-info:\n");
	show_free_areas();
	printk("Free swap:       %6ldkB\n", nr_swap_pages<<(PAGE_SHIFT-10));

	for_each_online_node(node) {
		struct page *page, *end;

		page = NODE_MEM_MAP(node);
		end  = page + NODE_DATA(node)->node_spanned_pages;

		do {
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (PageSlab(page))
				slab++;
			else if (!page_count(page))
				free++;
			else
				shared += page_count(page) - 1;
			page++;
		} while (page < end);
	}

	printk("%d pages of RAM\n", total);
	printk("%d free pages\n", free);
	printk("%d reserved pages\n", reserved);
	printk("%d slab pages\n", slab);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
}

static inline pmd_t *pmd_off(pgd_t *pgd, unsigned long virt)
{
	return pmd_offset(pgd, virt);
}

static inline pmd_t *pmd_off_k(unsigned long virt)
{
	return pmd_off(pgd_offset_k(virt), virt);
}

#define for_each_nodebank(iter,mi,no)			\
	for (iter = 0; iter < mi->nr_banks; iter++)	\
		if (mi->bank[iter].node == no)

/*
 * FIXME: We really want to avoid allocating the bootmap bitmap
 * over the top of the initrd.  Hopefully, this is located towards
 * the start of a bank, so if we allocate the bootmap bitmap at
 * the end, we won't clash.
 */
static unsigned int __init
find_bootmap_pfn(int node, struct meminfo *mi, unsigned int bootmap_pages)
{
	unsigned int start_pfn, bank, bootmap_pfn;

	start_pfn   = PAGE_ALIGN(__pa(&_end)) >> PAGE_SHIFT;
	bootmap_pfn = 0;

	for_each_nodebank(bank, mi, node) {
		unsigned int start, end;

		start = mi->bank[bank].start >> PAGE_SHIFT;
		end   = (mi->bank[bank].size +
			 mi->bank[bank].start) >> PAGE_SHIFT;

		if (end < start_pfn)
			continue;

		if (start < start_pfn)
			start = start_pfn;

		if (end <= start)
			continue;

		if (end - start >= bootmap_pages) {
			bootmap_pfn = start;
			break;
		}
	}

	if (bootmap_pfn == 0)
		BUG();

	return bootmap_pfn;
}

static int __init check_initrd(struct meminfo *mi)
{
	int initrd_node = -2;
#ifdef CONFIG_BLK_DEV_INITRD
	unsigned long end = phys_initrd_start + phys_initrd_size;

	/*
	 * Make sure that the initrd is within a valid area of
	 * memory.
	 */
	if (phys_initrd_size) {
		unsigned int i;

		initrd_node = -1;

		for (i = 0; i < mi->nr_banks; i++) {
			unsigned long bank_end;

			bank_end = mi->bank[i].start + mi->bank[i].size;

			if (mi->bank[i].start <= phys_initrd_start &&
			    end <= bank_end)
				initrd_node = mi->bank[i].node;
		}
	}

	if (initrd_node == -1) {
		printk(KERN_ERR "initrd (0x%08lx - 0x%08lx) extends beyond "
		       "physical memory - disabling initrd\n",
		       phys_initrd_start, end);
		phys_initrd_start = phys_initrd_size = 0;
	}
#endif

	return initrd_node;
}

/*
 * Reserve the various regions of node 0
 */
static __init void reserve_node_zero(pg_data_t *pgdat)
{
	unsigned long res_size = 0;

	/*
	 * Register the kernel text and data with bootmem.
	 * Note that this can only be in node 0.
	 */
#ifdef CONFIG_XIP_KERNEL
	reserve_bootmem_node(pgdat, __pa(&__data_start), &_end - &__data_start);
#else
	reserve_bootmem_node(pgdat, __pa(&_stext), &_end - &_stext);
#endif

	/*
	 * Reserve the page tables.  These are already in use,
	 * and can only be in node 0.
	 */
	reserve_bootmem_node(pgdat, __pa(swapper_pg_dir),
			     PTRS_PER_PGD * sizeof(pgd_t));

	/*
	 * Hmm... This should go elsewhere, but we really really need to
	 * stop things allocating the low memory; ideally we need a better
	 * implementation of GFP_DMA which does not assume that DMA-able
	 * memory starts at zero.
	 */
	if (machine_is_integrator() || machine_is_cintegrator())
		res_size = __pa(swapper_pg_dir) - PHYS_OFFSET;

	/*
	 * These should likewise go elsewhere.  They pre-reserve the
	 * screen memory region at the start of main system memory.
	 */
	if (machine_is_edb7211())
		res_size = 0x00020000;
	if (machine_is_p720t())
		res_size = 0x00014000;

#ifdef CONFIG_SA1111
	/*
	 * Because of the SA1111 DMA bug, we want to preserve our
	 * precious DMA-able memory...
	 */
	res_size = __pa(swapper_pg_dir) - PHYS_OFFSET;
#endif
	if (res_size)
		reserve_bootmem_node(pgdat, PHYS_OFFSET, res_size);
}

void __init build_mem_type_table(void);
void __init create_mapping(struct map_desc *md);

static unsigned long __init
bootmem_init_node(int node, int initrd_node, struct meminfo *mi)
{
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	unsigned long start_pfn, end_pfn, boot_pfn;
	unsigned int boot_pages;
	pg_data_t *pgdat;
	int i;

	start_pfn = -1UL;
	end_pfn = 0;

	/*
	 * Calculate the pfn range, and map the memory banks for this node.
	 */
	for_each_nodebank(i, mi, node) {
		unsigned long start, end;
		struct map_desc map;

		start = mi->bank[i].start >> PAGE_SHIFT;
		end = (mi->bank[i].start + mi->bank[i].size) >> PAGE_SHIFT;

		if (start_pfn > start)
			start_pfn = start;
		if (end_pfn < end)
			end_pfn = end;

		map.pfn = __phys_to_pfn(mi->bank[i].start);
		map.virtual = __phys_to_virt(mi->bank[i].start);
		map.length = mi->bank[i].size;
		map.type = MT_MEMORY;

		create_mapping(&map);
	}

	/*
	 * If there is no memory in this node, ignore it.
	 */
	if (end_pfn == 0)
		return end_pfn;

	/*
	 * Allocate the bootmem bitmap page.
	 */
	boot_pages = bootmem_bootmap_pages(end_pfn - start_pfn);
	boot_pfn = find_bootmap_pfn(node, mi, boot_pages);

	/*
	 * Initialise the bootmem allocator for this node, handing the
	 * memory banks over to bootmem.
	 */
	node_set_online(node);
	pgdat = NODE_DATA(node);
	init_bootmem_node(pgdat, boot_pfn, start_pfn, end_pfn);

	for_each_nodebank(i, mi, node)
		free_bootmem_node(pgdat, mi->bank[i].start, mi->bank[i].size);

	/*
	 * Reserve the bootmem bitmap for this node.
	 */
	reserve_bootmem_node(pgdat, boot_pfn << PAGE_SHIFT,
			     boot_pages << PAGE_SHIFT);

#ifdef CONFIG_BLK_DEV_INITRD
	/*
	 * If the initrd is in this node, reserve its memory.
	 */
	if (node == initrd_node) {
		reserve_bootmem_node(pgdat, phys_initrd_start,
				     phys_initrd_size);
		initrd_start = __phys_to_virt(phys_initrd_start);
		initrd_end = initrd_start + phys_initrd_size;
	}
#endif

	/*
	 * Finally, reserve any node zero regions.
	 */
	if (node == 0)
		reserve_node_zero(pgdat);

	/*
	 * initialise the zones within this node.
	 */
	memset(zone_size, 0, sizeof(zone_size));
	memset(zhole_size, 0, sizeof(zhole_size));

	/*
	 * The size of this node has already been determined.  If we need
	 * to do anything fancy with the allocation of this memory to the
	 * zones, now is the time to do it.
	 */
	zone_size[0] = end_pfn - start_pfn;

	/*
	 * For each bank in this node, calculate the size of the holes.
	 *  holes = node_size - sum(bank_sizes_in_node)
	 */
	zhole_size[0] = zone_size[0];
	for_each_nodebank(i, mi, node)
		zhole_size[0] -= mi->bank[i].size >> PAGE_SHIFT;

	/*
	 * Adjust the sizes according to any special requirements for
	 * this machine type.
	 */
	arch_adjust_zones(node, zone_size, zhole_size);

	free_area_init_node(node, pgdat, zone_size, start_pfn, zhole_size);

	return end_pfn;
}

static void __init bootmem_init(struct meminfo *mi)
{
	unsigned long addr, memend_pfn = 0;
	int node, initrd_node, i;

	/*
	 * Invalidate the node number for empty or invalid memory banks
	 */
	for (i = 0; i < mi->nr_banks; i++)
		if (mi->bank[i].size == 0 || mi->bank[i].node >= MAX_NUMNODES)
			mi->bank[i].node = -1;

	memcpy(&meminfo, mi, sizeof(meminfo));

	/*
	 * Clear out all the mappings below the kernel image.
	 */
	for (addr = 0; addr < MODULE_START; addr += PGDIR_SIZE)
		pmd_clear(pmd_off_k(addr));
#ifdef CONFIG_XIP_KERNEL
	/* The XIP kernel is mapped in the module area -- skip over it */
	addr = ((unsigned long)&_etext + PGDIR_SIZE - 1) & PGDIR_MASK;
#endif
	for ( ; addr < PAGE_OFFSET; addr += PGDIR_SIZE)
		pmd_clear(pmd_off_k(addr));

	/*
	 * Clear out all the kernel space mappings, except for the first
	 * memory bank, up to the end of the vmalloc region.
	 */
	for (addr = __phys_to_virt(mi->bank[0].start + mi->bank[0].size);
	     addr < VMALLOC_END; addr += PGDIR_SIZE)
		pmd_clear(pmd_off_k(addr));

	/*
	 * Locate which node contains the ramdisk image, if any.
	 */
	initrd_node = check_initrd(mi);

	/*
	 * Run through each node initialising the bootmem allocator.
	 */
	for_each_node(node) {
		unsigned long end_pfn;

		end_pfn = bootmem_init_node(node, initrd_node, mi);

		/*
		 * Remember the highest memory PFN.
		 */
		if (end_pfn > memend_pfn)
			memend_pfn = end_pfn;
	}

	high_memory = __va(memend_pfn << PAGE_SHIFT);

	/*
	 * This doesn't seem to be used by the Linux memory manager any
	 * more, but is used by ll_rw_block.  If we can get rid of it, we
	 * also get rid of some of the stuff above as well.
	 *
	 * Note: max_low_pfn and max_pfn reflect the number of _pages_ in
	 * the system, not the maximum PFN.
	 */
	max_pfn = max_low_pfn = memend_pfn - PHYS_PFN_OFFSET;
}

/*
 * Set up device the mappings.  Since we clear out the page tables for all
 * mappings above VMALLOC_END, we will remove any debug device mappings.
 * This means you have to be careful how you debug this function, or any
 * called function.  This means you can't use any function or debugging
 * method which may touch any device, otherwise the kernel _will_ crash.
 */
static void __init devicemaps_init(struct machine_desc *mdesc)
{
	struct map_desc map;
	unsigned long addr;
	void *vectors;

	/*
	 * Allocate the vector page early.
	 */
	vectors = alloc_bootmem_low_pages(PAGE_SIZE);
	BUG_ON(!vectors);

	for (addr = VMALLOC_END; addr; addr += PGDIR_SIZE)
		pmd_clear(pmd_off_k(addr));

	/*
	 * Map the kernel if it is XIP.
	 * It is always first in the modulearea.
	 */
#ifdef CONFIG_XIP_KERNEL
	map.pfn = __phys_to_pfn(CONFIG_XIP_PHYS_ADDR & PGDIR_MASK);
	map.virtual = MODULE_START;
	map.length = ((unsigned long)&_etext - map.virtual + ~PGDIR_MASK) & PGDIR_MASK;
	map.type = MT_ROM;
	create_mapping(&map);
#endif

	/*
	 * Map the cache flushing regions.
	 */
#ifdef FLUSH_BASE
	map.pfn = __phys_to_pfn(FLUSH_BASE_PHYS);
	map.virtual = FLUSH_BASE;
	map.length = SZ_1M;
	map.type = MT_CACHECLEAN;
	create_mapping(&map);
#endif
#ifdef FLUSH_BASE_MINICACHE
	map.pfn = __phys_to_pfn(FLUSH_BASE_PHYS + SZ_1M);
	map.virtual = FLUSH_BASE_MINICACHE;
	map.length = SZ_1M;
	map.type = MT_MINICLEAN;
	create_mapping(&map);
#endif

	/*
	 * Create a mapping for the machine vectors at the high-vectors
	 * location (0xffff0000).  If we aren't using high-vectors, also
	 * create a mapping at the low-vectors virtual address.
	 */
	map.pfn = __phys_to_pfn(virt_to_phys(vectors));
	map.virtual = 0xffff0000;
	map.length = PAGE_SIZE;
	map.type = MT_HIGH_VECTORS;
	create_mapping(&map);

	if (!vectors_high()) {
		map.virtual = 0;
		map.type = MT_LOW_VECTORS;
		create_mapping(&map);
	}

	/*
	 * Ask the machine support to map in the statically mapped devices.
	 */
	if (mdesc->map_io)
		mdesc->map_io();

	/*
	 * Finally flush the caches and tlb to ensure that we're in a
	 * consistent state wrt the writebuffer.  This also ensures that
	 * any write-allocated cache lines in the vector page are written
	 * back.  After this point, we can start to touch devices again.
	 */
	local_flush_tlb_all();
	flush_cache_all();
}

/*
 * paging_init() sets up the page tables, initialises the zone memory
 * maps, and sets up the zero page, bad page and bad page tables.
 */
void __init paging_init(struct meminfo *mi, struct machine_desc *mdesc)
{
	void *zero_page;

	build_mem_type_table();
	bootmem_init(mi);
	devicemaps_init(mdesc);

	top_pmd = pmd_off_k(0xffff0000);

	/*
	 * allocate the zero page.  Note that we count on this going ok.
	 */
	zero_page = alloc_bootmem_low_pages(PAGE_SIZE);
	memzero(zero_page, PAGE_SIZE);
	empty_zero_page = virt_to_page(zero_page);
	flush_dcache_page(empty_zero_page);
}

static inline void free_area(unsigned long addr, unsigned long end, char *s)
{
	unsigned int size = (end - addr) >> 10;

	for (; addr < end; addr += PAGE_SIZE) {
		struct page *page = virt_to_page(addr);
		ClearPageReserved(page);
		init_page_count(page);
		free_page(addr);
		totalram_pages++;
	}

	if (size && s)
		printk(KERN_INFO "Freeing %s memory: %dK\n", s, size);
}

static inline void
free_memmap(int node, unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *start_pg, *end_pg;
	unsigned long pg, pgend;

	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page(start_pfn);
	end_pg = pfn_to_page(end_pfn);

	/*
	 * Convert to physical addresses, and
	 * round start upwards and end downwards.
	 */
	pg = PAGE_ALIGN(__pa(start_pg));
	pgend = __pa(end_pg) & PAGE_MASK;

	/*
	 * If there are free pages between these,
	 * free the section of the memmap array.
	 */
	if (pg < pgend)
		free_bootmem_node(NODE_DATA(node), pg, pgend - pg);
}

/*
 * The mem_map array can get very big.  Free the unused area of the memory map.
 */
static void __init free_unused_memmap_node(int node, struct meminfo *mi)
{
	unsigned long bank_start, prev_bank_end = 0;
	unsigned int i;

	/*
	 * [FIXME] This relies on each bank being in address order.  This
	 * may not be the case, especially if the user has provided the
	 * information on the command line.
	 */
	for_each_nodebank(i, mi, node) {
		bank_start = mi->bank[i].start >> PAGE_SHIFT;
		if (bank_start < prev_bank_end) {
			printk(KERN_ERR "MEM: unordered memory banks.  "
				"Not freeing memmap.\n");
			break;
		}

		/*
		 * If we had a previous bank, and there is a space
		 * between the current bank and the previous, free it.
		 */
		if (prev_bank_end && prev_bank_end != bank_start)
			free_memmap(node, prev_bank_end, bank_start);

		prev_bank_end = (mi->bank[i].start +
				 mi->bank[i].size) >> PAGE_SHIFT;
	}
}

/*
 * mem_init() marks the free areas in the mem_map and tells us how much
 * memory is free.  This is done after various parts of the system have
 * claimed their memory after the kernel image.
 */
void __init mem_init(void)
{
	unsigned int codepages, datapages, initpages;
	int i, node;

	codepages = &_etext - &_text;
	datapages = &_end - &__data_start;
	initpages = &__init_end - &__init_begin;

#ifndef CONFIG_DISCONTIGMEM
	max_mapnr   = virt_to_page(high_memory) - mem_map;
#endif

	/* this will put all unused low memory onto the freelists */
	for_each_online_node(node) {
		pg_data_t *pgdat = NODE_DATA(node);

		free_unused_memmap_node(node, &meminfo);

		if (pgdat->node_spanned_pages != 0)
			totalram_pages += free_all_bootmem_node(pgdat);
	}

#ifdef CONFIG_SA1111
	/* now that our DMA memory is actually so designated, we can free it */
	free_area(PAGE_OFFSET, (unsigned long)swapper_pg_dir, NULL);
#endif

	/*
	 * Since our memory may not be contiguous, calculate the
	 * real number of pages we have in this system
	 */
	printk(KERN_INFO "Memory:");

	num_physpages = 0;
	for (i = 0; i < meminfo.nr_banks; i++) {
		num_physpages += meminfo.bank[i].size >> PAGE_SHIFT;
		printk(" %ldMB", meminfo.bank[i].size >> 20);
	}

	printk(" = %luMB total\n", num_physpages >> (20 - PAGE_SHIFT));
	printk(KERN_NOTICE "Memory: %luKB available (%dK code, "
		"%dK data, %dK init)\n",
		(unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
		codepages >> 10, datapages >> 10, initpages >> 10);

	if (PAGE_SIZE >= 16384 && num_physpages <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get
		 * anywhere without overcommit, so turn
		 * it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}

void free_initmem(void)
{
	if (!machine_is_integrator() && !machine_is_cintegrator()) {
		free_area((unsigned long)(&__init_begin),
			  (unsigned long)(&__init_end),
			  "init");
	}
}

#ifdef CONFIG_BLK_DEV_INITRD

static int keep_initrd;

void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (!keep_initrd)
		free_area(start, end, "initrd");
}

static int __init keepinitrd_setup(char *__unused)
{
	keep_initrd = 1;
	return 1;
}

__setup("keepinitrd", keepinitrd_setup);
#endif
