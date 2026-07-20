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
#define PG_U		0x004UL		/* user-accessible	*/
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
walk_in(__u64 pml4, __u64 va, int stoplvl, __u64 iflags)
{
	__u64 table = pml4;
	int lvl;

	for (lvl = 3; lvl > stoplvl; lvl--) {
		__u64 *pte = p2v(table) + PIDX(va, lvl);

		if ((*pte & PG_P) == 0)
			*pte = pmm_alloc() | PG_P | PG_W | iflags;
		table = *pte & ~0xfffUL;
	}
	return p2v(table) + PIDX(va, stoplvl);
}

static __u64 *
walk(__u64 va, int stoplvl)
{
	return walk_in(kpml4, va, stoplvl, 0);
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

/* ---- user address spaces (M5) ---- */

__u64
pmap_kernel_pml4(void)
{
	return kpml4;
}

/*
 * Build a fresh user address space: a new PML4 that shares the kernel's
 * higher-half entries (256..511), so kernel code/stack/direct-map remain
 * mapped while user code runs and during syscalls/interrupts.  The lower
 * half (user space) starts empty.
 */
__u64
pmap_new_user_as(void)
{
	__u64 upml4 = pmm_alloc();
	__u64 *k = p2v(kpml4);
	__u64 *u = p2v(upml4);
	int i;

	for (i = 256; i < 512; i++)		/* share kernel half */
		u[i] = k[i];
	return upml4;
}

/* map one user page (PG_U); writable if requested */
void
pmap_map_user(__u64 pml4, __u64 va, __u64 pa, int writable)
{
	__u64 *pte = walk_in(pml4, va, 0, PG_U);
	__u64 flags = PG_P | PG_U | (writable ? PG_W : 0);

	*pte = (pa & ~0xfffUL) | flags;
}

/* allocate + map a zeroed user page, returning its physical address */
__u64
pmap_alloc_user_page(__u64 pml4, __u64 va, int writable)
{
	__u64 pa = pmm_alloc();
	pmap_map_user(pml4, va, pa, writable);
	return pa;
}

void
pmap_switch(__u64 pml4_phys)
{
	load_cr3(pml4_phys);
}

/* kernel virtual address of a physical page (for filling user pages) */
void *
pmap_phys_to_kv(__u64 pa)
{
	return p2v(pa);
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

	/*
	 * IRIX fixed-VA window: the kernel accesses its per-processor
	 * data area (sys/immu.h PDAPAGE — 0xffffa000 or 0xffffc000
	 * sign-extended, config-dependent) and the kernel-stack page
	 * through fixed top-of-VA addresses.  MIPS wired TLB entries;
	 * we back the whole 32K top window with real pages for cpu 0.
	 */
	for (va = 0xffffffffffff8000UL; va >= 0xffffffffffff8000UL &&
	    va != 0; va += PAGESZ)
		map_4k(va, pmm_alloc());

	load_cr3(kpml4);
}
