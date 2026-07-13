#!/bin/sh
# Boot the IRIX x86-64 kernel in QEMU, serial console on stdio.
cd "$(dirname "$0")/.."
exec qemu-system-x86_64 \
	-M q35 -m 512M \
	-cdrom obj/irix-x86_64.iso \
	-serial stdio \
	-display none \
	-no-reboot -no-shutdown \
	"$@"
