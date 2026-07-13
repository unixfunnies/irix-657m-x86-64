/*
 * irix/kern/ml/X86_64/gdt.c
 *
 * GDT setup.  Long mode barely uses segmentation, but we still need
 * well-defined kernel code/data selectors (and, come M5, user
 * selectors + a TSS — slots are reserved for them here).
 */

#include "x86_64.h"

#define GDT_NENT	7	/* null, kcode, kdata, ucode, udata, tss lo/hi */

/* selectors */
#define KCODE_SEL	0x08
#define KDATA_SEL	0x10

static __u64 gdt[GDT_NENT] = {
	0,			/* 0x00: null			*/
	0x00af9a000000ffff,	/* 0x08: kernel code, 64-bit	*/
	0x00cf92000000ffff,	/* 0x10: kernel data		*/
	0,			/* 0x18: user code (M5)		*/
	0,			/* 0x20: user data (M5)		*/
	0, 0,			/* 0x28: TSS (M5)		*/
};

struct gdtr {
	__u16	limit;
	__u64	base;
} __attribute__((packed));

void
gdt_init(void)
{
	static struct gdtr gdtr;

	gdtr.limit = sizeof(gdt) - 1;
	gdtr.base = (__u64)gdt;

	__asm__ __volatile__(
	    "lgdt	%0\n\t"
	    /* reload CS via far return */
	    "pushq	%1\n\t"
	    "leaq	1f(%%rip), %%rax\n\t"
	    "pushq	%%rax\n\t"
	    "lretq\n\t"
	    "1:\n\t"
	    /* reload data segment registers */
	    "movl	%2, %%eax\n\t"
	    "movl	%%eax, %%ds\n\t"
	    "movl	%%eax, %%es\n\t"
	    "movl	%%eax, %%ss\n\t"
	    "movl	%%eax, %%fs\n\t"
	    "movl	%%eax, %%gs\n\t"
	    : : "m"(gdtr), "i"((__u64)KCODE_SEL), "i"(KDATA_SEL)
	    : "rax", "memory");
}
