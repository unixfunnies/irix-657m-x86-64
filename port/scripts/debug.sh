#!/bin/sh
# Boot under QEMU with a gdb stub, halted at the first instruction.
# Attach with:  gdb port/obj/unix -ex 'target remote :1234'
cd "$(dirname "$0")/.."
exec qemu-system-x86_64 \
	-M q35 -m 512M \
	-cdrom obj/irix-x86_64.iso \
	-serial stdio \
	-display none \
	-no-reboot -no-shutdown \
	-s -S \
	"$@"
