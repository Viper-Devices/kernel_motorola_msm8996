#ifndef _ASM_X86_IO_H
#define _ASM_X86_IO_H

#define ARCH_HAS_IOREMAP_WC

#include <linux/compiler.h>

/*
 * early_ioremap() and early_iounmap() are for temporary early boot-time
 * mappings, before the real ioremap() is functional.
 * A boot-time mapping is currently limited to at most 16 pages.
 */
#ifndef __ASSEMBLY__
extern void early_ioremap_init(void);
extern void early_ioremap_clear(void);
extern void early_ioremap_reset(void);
extern void *early_ioremap(unsigned long offset, unsigned long size);
extern void early_iounmap(void *addr, unsigned long size);
extern void __iomem *fix_ioremap(unsigned idx, unsigned long phys);
#endif

#define build_mmio_read(name, size, type, reg, barrier) \
static inline type name(const volatile void __iomem *addr) \
{ type ret; asm volatile("mov" size " %1,%0":reg (ret) \
:"m" (*(volatile type __force *)addr) barrier); return ret; }

#define build_mmio_write(name, size, type, reg, barrier) \
static inline void name(type val, volatile void __iomem *addr) \
{ asm volatile("mov" size " %0,%1": :reg (val), \
"m" (*(volatile type __force *)addr) barrier); }

build_mmio_read(readb, "b", unsigned char, "=q", :"memory")
build_mmio_read(readw, "w", unsigned short, "=r", :"memory")
build_mmio_read(readl, "l", unsigned int, "=r", :"memory")

build_mmio_read(__readb, "b", unsigned char, "=q", )
build_mmio_read(__readw, "w", unsigned short, "=r", )
build_mmio_read(__readl, "l", unsigned int, "=r", )

build_mmio_write(writeb, "b", unsigned char, "q", :"memory")
build_mmio_write(writew, "w", unsigned short, "r", :"memory")
build_mmio_write(writel, "l", unsigned int, "r", :"memory")

build_mmio_write(__writeb, "b", unsigned char, "q", )
build_mmio_write(__writew, "w", unsigned short, "r", )
build_mmio_write(__writel, "l", unsigned int, "r", )

#define readb_relaxed(a) __readb(a)
#define readw_relaxed(a) __readw(a)
#define readl_relaxed(a) __readl(a)
#define __raw_readb __readb
#define __raw_readw __readw
#define __raw_readl __readl

#define __raw_writeb __writeb
#define __raw_writew __writew
#define __raw_writel __writel

#define mmiowb() barrier()

#ifdef CONFIG_X86_64
build_mmio_read(readq, "q", unsigned long, "=r", :"memory")
build_mmio_read(__readq, "q", unsigned long, "=r", )
build_mmio_write(writeq, "q", unsigned long, "r", :"memory")
build_mmio_write(__writeq, "q", unsigned long, "r", )

#define readq_relaxed(a) __readq(a)
#define __raw_readq __readq
#define __raw_writeq writeq

/* Let people know we have them */
#define readq readq
#define writeq writeq
#endif

#ifdef CONFIG_X86_32
# include "io_32.h"
#else
# include "io_64.h"
#endif

extern void *xlate_dev_mem_ptr(unsigned long phys);
extern void unxlate_dev_mem_ptr(unsigned long phys, void *addr);

extern int ioremap_change_attr(unsigned long vaddr, unsigned long size,
				unsigned long prot_val);
extern void __iomem *ioremap_wc(unsigned long offset, unsigned long size);

/*
 * early_ioremap() and early_iounmap() are for temporary early boot-time
 * mappings, before the real ioremap() is functional.
 * A boot-time mapping is currently limited to at most 16 pages.
 */
extern void early_ioremap_init(void);
extern void early_ioremap_clear(void);
extern void early_ioremap_reset(void);
extern void *early_ioremap(unsigned long offset, unsigned long size);
extern void early_iounmap(void *addr, unsigned long size);
extern void __iomem *fix_ioremap(unsigned idx, unsigned long phys);


#endif /* _ASM_X86_IO_H */
