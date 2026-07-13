/*
 * irix/kern/ml/X86_64/pmm.c
 *
 * Boot-time physical page allocator: a simple intrusive freelist
 * over the Limine memory map, with pages touched through the HHDM
 * linear mapping.  This will feed the real IRIX kmem/pfdat machinery
 * at M2; for now it backs the page-table builder.
 */

#include <limine.h>
#include "x86_64.h"

#define PAGESZ		4096

static __u64	hhdm_off;
static __u64	freelist;	/* phys addr of first free page	*/
static __u64	nfree, ntotal;

static void *
p2v(__u64 pa)
{
	return (void *)(pa + hhdm_off);
}

void
pmm_init(struct limine_memmap_response *mm, __u64 hhdm,
    __u64 skip_base, __u64 skip_size)
{
	__u64 i, pa;

	hhdm_off = hhdm;
	for (i = 0; i < mm->entry_count; i++) {
		struct limine_memmap_entry *e = mm->entries[i];

		if (e->type != LIMINE_MEMMAP_USABLE)
			continue;
		for (pa = e->base; pa + PAGESZ <= e->base + e->length;
		    pa += PAGESZ) {
			/* skip the range handed to the kmem arena */
			if (pa >= skip_base && pa < skip_base + skip_size)
				continue;
			*(__u64 *)p2v(pa) = freelist;
			freelist = pa;
			nfree++;
			ntotal++;
		}
	}
}

__u64
pmm_alloc(void)
{
	__u64 pa, *p;
	int i;

	if (freelist == 0)
		panic("pmm_alloc: out of physical memory", 0);
	pa = freelist;
	p = p2v(pa);
	freelist = *p;
	nfree--;

	for (i = 0; i < PAGESZ / 8; i++)
		p[i] = 0;
	return pa;
}

void
pmm_free(__u64 pa)
{
	*(__u64 *)p2v(pa) = freelist;
	freelist = pa;
	nfree++;
}

__u64
pmm_free_pages(void)
{
	return nfree;
}

__u64
pmm_total_pages(void)
{
	return ntotal;
}
