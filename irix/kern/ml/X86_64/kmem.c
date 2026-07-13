/*
 * irix/kern/ml/X86_64/kmem.c
 *
 * x86-64 kernel memory allocator — the machine-dependent implementation
 * of the IRIX kmem_alloc()/kmem_zalloc()/kmem_free() contract (sys/kmem.h)
 * that the machine-independent kernel is written against.
 *
 * IRIX's own heap (sgi/kern_heap.c) grows the kernel virtual address space
 * through the MIPS software-TLB page tables; that mechanism is exactly what
 * the port replaces.  Here we take the natural x86-64 route: reserve one
 * large physically-contiguous arena up front, addressed through the HHDM
 * direct map (PHYS_TO_K0), and run a first-fit free list over it.  Because
 * the direct map is linear, virtual contiguity == physical contiguity, so
 * multi-page and KM_PHYSCONTIG allocations are free.
 *
 * First-fit with an implicit block list (header per block, split on alloc,
 * forward+backward coalesce on free).  Correct and simple; a zone/slab
 * layer (kmem_zone_*) sits on top later when a consumer needs it.
 */

#include <limine.h>
#include "x86_64.h"

#define KM_ALIGN	16
#define KM_MAGIC	0x4b4d454dU	/* "KMEM"			*/

struct blk {
	__u64		size;		/* total block bytes incl header */
	__u64		inuse;		/* 0 free, KM_MAGIC when used	*/
};

static char	*arena_base;		/* HHDM virtual base		*/
static __u64	arena_size;
static __u64	arena_used;
static __u64	k0base_off;		/* PHYS_TO_K0 offset (HHDM)	*/

static __u64
roundup(__u64 v, __u64 a)
{
	return (v + a - 1) & ~(a - 1);
}

/*
 * Reserve a contiguous physical arena for the kernel heap from the Limine
 * map (the largest usable region, capped).  Returns the physical range so
 * the caller can exclude it from the page allocator.
 */
void
kmem_reserve(struct limine_memmap_response *mm, __u64 hhdm,
    __u64 cap, __u64 *out_base, __u64 *out_size)
{
	__u64 best_base = 0, best_len = 0;
	__u64 i;

	for (i = 0; i < mm->entry_count; i++) {
		if (mm->entries[i]->type != LIMINE_MEMMAP_USABLE)
			continue;
		if (mm->entries[i]->length > best_len) {
			best_len = mm->entries[i]->length;
			best_base = mm->entries[i]->base;
		}
	}
	if (best_len > cap)
		best_len = cap;
	best_len &= ~0xfffUL;

	k0base_off = hhdm;
	arena_base = (char *)(best_base + hhdm);
	arena_size = best_len;
	arena_used = 0;

	/* one free block spanning the arena */
	{
		struct blk *b = (struct blk *)arena_base;
		b->size = arena_size;
		b->inuse = 0;
	}

	*out_base = best_base;
	*out_size = best_len;
}

static struct blk *
next_blk(struct blk *b)
{
	char *n = (char *)b + b->size;
	if (n >= arena_base + arena_size)
		return 0;
	return (struct blk *)n;
}

void *
kmem_alloc(unsigned long size, int flags)
{
	__u64 need = roundup(size + sizeof(struct blk), KM_ALIGN);
	struct blk *b;

	(void)flags;			/* we never sleep; KM_* advisory here */
	if (size == 0)
		return 0;

	for (b = (struct blk *)arena_base; b != 0; b = next_blk(b)) {
		if (b->inuse || b->size < need)
			continue;

		/* split if the remainder can hold a header + min payload */
		if (b->size >= need + sizeof(struct blk) + KM_ALIGN) {
			struct blk *rest = (struct blk *)((char *)b + need);
			rest->size = b->size - need;
			rest->inuse = 0;
			b->size = need;
		}
		b->inuse = KM_MAGIC;
		arena_used += b->size;
		return (void *)(b + 1);
	}
	return 0;			/* out of arena */
}

static void
kmem_bzero(void *p, __u64 n)
{
	char *c = p;
	while (n--)
		*c++ = 0;
}

void *
kmem_zalloc(unsigned long size, int flags)
{
	void *p = kmem_alloc(size, flags);
	if (p != 0)
		kmem_bzero(p, size);
	return p;
}

void
kmem_free(void *ptr, unsigned long size)
{
	struct blk *b, *p, *prev;

	(void)size;
	if (ptr == 0)
		return;
	b = (struct blk *)ptr - 1;
	if (b->inuse != KM_MAGIC)
		panic("kmem_free: bad or double free", 0);
	b->inuse = 0;
	arena_used -= b->size;

	/* forward coalesce */
	for (;;) {
		struct blk *n = next_blk(b);
		if (n == 0 || n->inuse)
			break;
		b->size += n->size;
	}
	/* backward coalesce: find predecessor */
	prev = 0;
	for (p = (struct blk *)arena_base; p != 0 && p != b; p = next_blk(p))
		prev = p;
	if (prev != 0 && !prev->inuse)
		prev->size += b->size;
}

/* NUMA-node variants: single-node port, ignore the node argument */
void *
kmem_alloc_node(unsigned long size, int flags, int node)
{
	(void)node;
	return kmem_alloc(size, flags);
}

void *
kmem_zalloc_node(unsigned long size, int flags, int node)
{
	(void)node;
	return kmem_zalloc(size, flags);
}

__u64
kmem_arena_used(void)
{
	return arena_used;
}

__u64
kmem_arena_size(void)
{
	return arena_size;
}
