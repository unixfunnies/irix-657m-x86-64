/*
 * irix/kern/ml/X86_64/start.c
 *
 * Kernel entry point for the IRIX x86-64 port, booted via the
 * Limine protocol.  This is the analog of ml/csu.s on MIPS: the
 * bootloader has already put us in 64-bit long mode with paging
 * enabled and the kernel mapped in the higher half.
 *
 * M1 scope: own GDT/IDT, exception handling, physical allocator,
 * our own page tables, and a calibrated LAPIC timer delivering
 * interrupts — the primitives the MI IRIX code will sit on.
 */

#include <limine.h>
#include "x86_64.h"

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_req = {
	.id = LIMINE_BOOTLOADER_INFO_REQUEST,
	.revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_req = {
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_req = {
	.id = LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_req = {
	.id = LIMINE_HHDM_REQUEST,
	.revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kaddr_req = {
	.id = LIMINE_KERNEL_ADDRESS_REQUEST,
	.revision = 0,
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

extern char __kernel_start[], __kernel_end[];

void
kmain(void)
{
	struct limine_memmap_response *mm;
	__u64 hhdm, usable, phys_top;
	__u64 i;

	serial_early_init();
	if (fb_req.response != 0 && fb_req.response->framebuffer_count > 0)
		fb_init(fb_req.response->framebuffers[0]);

	kprintf("\n");
	kprintf("IRIX Release 6.5.7m x86-64 port -- first light (M0)\n");
	kprintf("Copyright 1987-2000 Silicon Graphics, Inc.\n");
	kprintf("All Rights Reserved.\n");
	kprintf("Copyright 2026 Mallorie G. & Contributors.\n");
	kprintf("\n");

	if (LIMINE_BASE_REVISION_SUPPORTED == 0) {
		kprintf("PANIC: Limine base revision not supported\n");
		cpu_halt();
	}
	if (memmap_req.response == 0 || hhdm_req.response == 0 ||
	    kaddr_req.response == 0) {
		kprintf("PANIC: missing Limine responses\n");
		cpu_halt();
	}

	if (bootloader_info_req.response != 0)
		kprintf("Booted by %s %s (Limine protocol)\n",
		    bootloader_info_req.response->name,
		    bootloader_info_req.response->version);

	mm = memmap_req.response;
	hhdm = hhdm_req.response->offset;

	usable = 0;
	phys_top = 0;
	for (i = 0; i < mm->entry_count; i++) {
		struct limine_memmap_entry *e = mm->entries[i];

		if (e->type == LIMINE_MEMMAP_USABLE) {
			usable += e->length;
			if (e->base + e->length > phys_top)
				phys_top = e->base + e->length;
		}
	}
	kprintf("Physical memory: %llu entries, %llu KB usable\n",
	    mm->entry_count, usable / 1024);
	kprintf("M0 checkpoint reached: kernel alive on x86-64.\n\n");

	/* ---- M1: CPU bringup ---- */

	gdt_init();
	kprintf("gdt: kernel selectors loaded\n");

	idt_init();
	__asm__ __volatile__("int3");	/* IDT self-test (recoverable) */

	/* carve a contiguous kmem arena, then give the rest to the PMM */
	{
		__u64 kbase, ksize;

		kmem_reserve(mm, hhdm, 64UL * 1024 * 1024, &kbase, &ksize);
		pmm_init(mm, hhdm, kbase, ksize);
		kprintf("pmm: %lu pages (%lu KB) on freelist\n",
		    pmm_free_pages(), pmm_free_pages() * 4);
		kprintf("kmem: %lu KB arena at phys 0x%lx\n",
		    kmem_arena_size() / 1024, kbase);
	}

	pmap_bootstrap(hhdm, kaddr_req.response->physical_base,
	    (__u64)__kernel_start,
	    (__u64)(__kernel_end - __kernel_start), phys_top);
	kprintf("pmap: kernel page tables active (kphys=0x%lx, hhdm=0x%lx)\n",
	    kaddr_req.response->physical_base, hhdm);

	apic_init(hhdm);
	sti();

	while (timer_ticks < TIMER_HZ / 2)	/* half a second */
		cpu_wait();
	kprintf("timer: %lu ticks at %d Hz, interrupts live\n",
	    timer_ticks, TIMER_HZ);

	kprintf("\nM1 checkpoint reached: CPU bringup complete.\n\n");

	/* ---- M2: exercise the IRIX kmem_alloc contract ---- */
	{
		void *a, *b, *c;

		a = kmem_alloc(64, 0);
		b = kmem_zalloc(4096, 0);
		c = kmem_alloc(1UL << 20, 0);		/* 1 MB contiguous */
		kprintf("kmem_alloc: a=%p b=%p c=%p (used %lu KB)\n",
		    a, b, c, kmem_arena_used() / 1024);
		kprintf("kmem_zalloc zeroed: b[0]=%d b[4095]=%d\n",
		    (int)((char *)b)[0], (int)((char *)b)[4095]);

		kmem_free(b, 4096);			/* free middle block */
		{
			void *b2 = kmem_alloc(4096, 0);	/* should reuse it  */
			kprintf("kmem_free/realloc: freed b, realloc=%p %s\n",
			    b2, b2 == b ? "(reused, coalesce ok)" :
			    "(new block)");
			kmem_free(b2, 4096);
		}
		kmem_free(a, 64);
		kmem_free(c, 1UL << 20);
		kprintf("kmem: all freed, used %lu KB\n",
		    kmem_arena_used() / 1024);
	}

	kprintf("\nM2 checkpoint reached: kmem_alloc live.\n\n");

	/* ---- M3: hand off to IRIX's own main() ---- */
	{
		extern void port_pda_init(void);
		extern void main(void);

		port_pda_init();
		kprintf("pda: mapped at PDAPAGE, thread0 installed\n");
		kprintf("Entering irix/kern/os/main.c:main()...\n\n");
		main();
		kprintf("\nmain() returned (init handoff is stubbed).\n\n");
	}

	/* ---- M4: demonstrate the x86-64 kernel-thread scheduler ---- */
	sched_demo();
	kprintf("\nM4 checkpoint reached: scheduler live.\n\n");

	/* ---- M5: demonstrate ring-3 user mode + syscalls ---- */
	usermode_demo();

	kprintf("\nM5 checkpoint reached: user mode live, halting.\n");
	cpu_halt();
}
