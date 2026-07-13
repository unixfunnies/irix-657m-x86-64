/*
 * irix/kern/ml/X86_64/start.c
 *
 * Kernel entry point for the IRIX x86-64 port, booted via the
 * Limine protocol.  This is the analog of ml/csu.s on MIPS: the
 * bootloader has already put us in 64-bit long mode with paging
 * enabled and the kernel mapped in the higher half.
 *
 * M0 scope: bring up the serial console, prove we are alive, report
 * what the bootloader handed us, and halt.
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
static volatile LIMINE_REQUESTS_END_MARKER;

extern char __kernel_start[], __kernel_end[];

void
kmain(void)
{
	serial_early_init();

	kprintf("\n");
	kprintf("IRIX Release 6.5.7m x86-64 port -- first light (M0)\n");
	kprintf("Copyright 1987-2000 Silicon Graphics, Inc.\n");
	kprintf("All Rights Reserved.\n");
	kprintf("\n");

	if (LIMINE_BASE_REVISION_SUPPORTED == 0) {
		kprintf("PANIC: Limine base revision not supported\n");
		cpu_halt();
	}

	if (bootloader_info_req.response != 0)
		kprintf("Booted by %s %s (Limine protocol)\n",
		    bootloader_info_req.response->name,
		    bootloader_info_req.response->version);

	kprintf("Kernel text: %p .. %p\n", __kernel_start, __kernel_end);

	if (memmap_req.response != 0) {
		struct limine_memmap_response *mm = memmap_req.response;
		__u64 usable = 0;
		__u64 i;

		for (i = 0; i < mm->entry_count; i++) {
			if (mm->entries[i]->type == LIMINE_MEMMAP_USABLE)
				usable += mm->entries[i]->length;
		}
		kprintf("Physical memory: %llu entries, %llu KB usable\n",
		    mm->entry_count, usable / 1024);
	}

	kprintf("\n");
	kprintf("M0 checkpoint reached: kernel alive on x86-64, halting.\n");

	cpu_halt();
}
