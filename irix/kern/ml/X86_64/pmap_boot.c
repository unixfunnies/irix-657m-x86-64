/*
 * irix/kern/ml/X86_64/pmap_boot.c
 *
 * Boot-time page-table construction.  This is where the port departs
 * hardest from MIPS: IRIX drove a software-refilled TLB (ml/tlb.s,
 * utlbmiss.s); x86-64 walks 4-level page tables in hardware.  Here we
 * replace Limine's bootstrap tables with our own:
 *
 *   - HHDM linear map of physical memory (2MB pages) at hhdm_off,
 *     covering RAM and low-4GB MMIO (LAPIC, IOAPIC).
 *   - the kernel image mapped 4K-granular at its linked address
 *     (0xffffffff80000000 + slide).
 *
 * The real pmap (os/as/pmap.c contract) builds on this at M2.
 */

#include "x86_64.h"

#define PAGESZ		4096UL
#define PG_P		0x001UL		/* present		*/
#define PG_W		0x002UL		/* writable		*/
#define PG_PS		0x080UL		/* 2MB page (PDE)	*/

#define PIDX(va, lvl)	(((va) >> (12 + 9 * (lvl))) & 0x1ff)

static __u64	hhdm_off;
static __u64	kpml4;		/* phys addr of kernel PML4	*/

static __u64 *
p2v(__u64 pa)
{
	return (__u64 *)(pa + hhdm_off);
}

/*
 * Walk to the page-table level 'stoplvl' entry for va, allocating
 * intermediate tables as needed.  Returns a pointer to the entry.
 */
static __u64 *
walk(__u64 va, int stoplvl)
{
	__u64 table = kpml4;
	int lvl;

	for (lvl = 3; lvl > stoplvl; lvl--) {
		__u64 *pte = p2v(table) + PIDX(va, lvl);

		if ((*pte & PG_P) == 0)
			*pte = pmm_alloc() | PG_P | PG_W;
		table = *pte & ~0xfffUL;
	}
	return p2v(table) + PIDX(va, stoplvl);
}

static void
map_2m(__u64 va, __u64 pa)
{
	*walk(va, 1) = pa | PG_P | PG_W | PG_PS;
}

static void
map_4k(__u64 va, __u64 pa)
{
	*walk(va, 0) = pa | PG_P | PG_W;
}

void
pmap_bootstrap(__u64 hhdm, __u64 kphys, __u64 kvirt, __u64 ksize,
    __u64 phys_top)
{
	__u64 pa, va;

	hhdm_off = hhdm;
	kpml4 = pmm_alloc();

	/*
	 * HHDM: linear-map physical memory with 2MB pages.  Always
	 * cover at least 4GB so LAPIC/IOAPIC/PCI MMIO is reachable.
	 */
	if (phys_top < 0x100000000UL)
		phys_top = 0x100000000UL;
	for (pa = 0; pa < phys_top; pa += 0x200000UL)
		map_2m(hhdm + pa, pa);

	/* kernel image at its linked/loaded address, 4K pages */
	for (va = kvirt, pa = kphys; va < kvirt + ksize;
	    va += PAGESZ, pa += PAGESZ)
		map_4k(va, pa);

	load_cr3(kpml4);
}
