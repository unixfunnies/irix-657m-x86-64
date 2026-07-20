/*
 * irix/kern/ml/X86_64/gdt.c
 *
 * GDT + TSS.  Long mode barely uses segmentation, but ring transitions
 * still need well-defined selectors and a TSS: the CPU loads RSP0 from
 * the TSS when an interrupt/int-gate raises privilege from ring 3 to
 * ring 0, so user-mode traps land on a known kernel stack.
 *
 * Selector layout (see also STAR-free int 0x80 path in trap.c):
 *   0x08 kernel code   0x10 kernel data
 *   0x18 user code     0x20 user data
 *   0x28 TSS (16 bytes, two GDT slots)
 */

#include "x86_64.h"

#define KCODE_SEL	0x08
#define KDATA_SEL	0x10
#define TSS_SEL		0x28

#define GDT_NENT	7	/* null,kcode,kdata,ucode,udata,tss_lo,tss_hi */

static __u64 gdt[GDT_NENT] = {
	0,			/* 0x00: null				*/
	0x00af9a000000ffff,	/* 0x08: kernel code, DPL0, 64-bit	*/
	0x00cf92000000ffff,	/* 0x10: kernel data, DPL0		*/
	0x00affa000000ffff,	/* 0x18: user code, DPL3, 64-bit	*/
	0x00cff2000000ffff,	/* 0x20: user data, DPL3		*/
	0, 0,			/* 0x28: TSS descriptor (filled below)	*/
};

/* 64-bit Task State Segment (only rsp0 + iomap base matter here) */
struct tss {
	__u32	reserved0;
	__u64	rsp0;		/* stack loaded on ring3 -> ring0	*/
	__u64	rsp1;
	__u64	rsp2;
	__u64	reserved1;
	__u64	ist[7];
	__u64	reserved2;
	__u16	reserved3;
	__u16	iomap_base;
} __attribute__((packed));

static struct tss tss;

struct gdtr {
	__u16	limit;
	__u64	base;
} __attribute__((packed));

void
tss_set_rsp0(__u64 rsp0)
{
	tss.rsp0 = rsp0;
}

void
gdt_init(void)
{
	static struct gdtr gdtr;
	__u64 base = (__u64)&tss;
	__u64 limit = sizeof(tss) - 1;

	/* system (TSS) descriptor spans gdt[5..6] */
	gdt[5] = (limit & 0xffff) |
		 ((base & 0xffffff) << 16) |
		 (0x9ULL << 40) |		/* type = available 64-bit TSS */
		 (1ULL << 47) |			/* present			*/
		 (((limit >> 16) & 0xf) << 48) |
		 (((base >> 24) & 0xff) << 56);
	gdt[6] = (base >> 32) & 0xffffffff;

	tss.iomap_base = sizeof(tss);		/* no I/O bitmap		*/

	gdtr.limit = sizeof(gdt) - 1;
	gdtr.base = (__u64)gdt;

	__asm__ __volatile__(
	    "lgdt	%0\n\t"
	    "pushq	%1\n\t"			/* reload CS via far return */
	    "leaq	1f(%%rip), %%rax\n\t"
	    "pushq	%%rax\n\t"
	    "lretq\n\t"
	    "1:\n\t"
	    "movl	%2, %%eax\n\t"
	    "movl	%%eax, %%ds\n\t"
	    "movl	%%eax, %%es\n\t"
	    "movl	%%eax, %%ss\n\t"
	    "movl	%%eax, %%fs\n\t"
	    "movl	%%eax, %%gs\n\t"
	    : : "m"(gdtr), "i"((__u64)KCODE_SEL), "i"(KDATA_SEL)
	    : "rax", "memory");

	__asm__ __volatile__("ltr %%ax" : : "a"((__u16)TSS_SEL));
}
