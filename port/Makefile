# Makefile for the IRIX x86-64 port.
#
# This deliberately does NOT resurrect smake/ISM — it is a fresh,
# minimal build system for the port.  Original SGI sources are
# compiled in place from their home directories as they are adopted.

PORT		:= $(CURDIR)
TOP		:= $(abspath $(PORT)/..)
ML		:= $(TOP)/irix/kern/ml/X86_64
BUILD		:= $(PORT)/obj
ISO_ROOT	:= $(BUILD)/iso_root
LIMINE		:= $(PORT)/boot/limine

KERNEL		:= $(BUILD)/unix
ISO		:= $(BUILD)/irix-x86_64.iso

CC		:= clang
LD		:= ld.lld

CFLAGS		:= --target=x86_64-elf -std=gnu99 -g -O2 -pipe \
		   -Wall -Wextra \
		   -ffreestanding -fno-stack-protector -fno-stack-check \
		   -fno-lto -fno-PIC \
		   -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
		   -mno-red-zone -mcmodel=kernel \
		   -I$(PORT)/include -I$(ML)

LDFLAGS		:= -nostdlib -static -z max-page-size=0x1000 \
		   -T $(PORT)/build/linker.ld

CSRCS		:= start.c serial.c kprintf.c fb.c gdt.c trap.c pmm.c \
		   pmap_boot.c apic.c kmem.c sched.c usermode.c
SSRCS		:= vectors.S ctxsw.S
OBJS		:= $(patsubst %.c,$(BUILD)/ml/%.o,$(CSRCS)) \
		   $(patsubst %.S,$(BUILD)/ml/%.o,$(SSRCS))

# Original SGI kernel sources adopted as-is, compiled from their home
# directories with the MIPSEL-n64 masquerade via scripts/tryc.sh.
# OS_SRCS name files under irix/kern/os/ (built to obj/sgi/<name>.o).
OS_SRCS		:= main.c vfs.c behavior.c fs_subr.c move.c
OS_OBJS		:= $(patsubst %.c,$(BUILD)/sgi/%.o,$(OS_SRCS))

# Port-authored / patched SGI-environment sources
PORT_SGI_OBJS	:= $(BUILD)/sgi/printf.o \
		   $(BUILD)/sgi/sgi_glue.o \
		   $(BUILD)/sgi/libkern.o \
		   $(BUILD)/sgi/memfs.o \
		   $(BUILD)/sgi/exec_elf.o
OBJS		+= $(OS_OBJS) $(PORT_SGI_OBJS) $(BUILD)/autostubs.o

.PHONY: all iso run debug smoke clean

all: $(ISO)

$(BUILD)/ml/%.o: $(ML)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ml/%.o: $(ML)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# adopted-as-is os/ sources
$(BUILD)/sgi/%.o: $(TOP)/irix/kern/os/%.c
	@mkdir -p $(dir $@)
	$(PORT)/scripts/tryc.sh $< -o $@

# patched copies win over the os/ rule (printf.c lives in both)
$(BUILD)/sgi/printf.o: $(PORT)/patched/printf.c
	@mkdir -p $(dir $@)
	$(PORT)/scripts/tryc.sh $< -o $@

# port-authored filesystem sources
$(BUILD)/sgi/%.o: $(PORT)/fs/%.c
	@mkdir -p $(dir $@)
	$(PORT)/scripts/tryc.sh $< -o $@

$(BUILD)/sgi/%.o: $(PORT)/stubs/%.c
	@mkdir -p $(dir $@)
	$(PORT)/scripts/tryc.sh $< -o $@

# autostubs.c is generated (gitignored). If absent, close the link by
# harvesting undefined symbols into it; genstubs.sh writes the .o too.
$(PORT)/stubs/autostubs.c:
	$(PORT)/scripts/genstubs.sh $(filter %.o,$(OBJS))

$(BUILD)/autostubs.o: $(PORT)/stubs/autostubs.c
	@mkdir -p $(dir $@)
	$(PORT)/scripts/tryc.sh $< -o $@

$(KERNEL): $(OBJS) $(PORT)/build/linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(ISO): $(KERNEL) $(PORT)/boot/limine.conf
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot/limine $(ISO_ROOT)/EFI/BOOT
	cp $(KERNEL) $(ISO_ROOT)/boot/unix
	cp $(PORT)/boot/limine.conf $(ISO_ROOT)/boot/limine/
	cp $(LIMINE)/limine-bios.sys $(LIMINE)/limine-bios-cd.bin \
	   $(LIMINE)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	xorriso -as mkisofs \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    $(ISO_ROOT) -o $@ 2>/dev/null
	$(LIMINE)/limine bios-install $@ 2>/dev/null

iso: $(ISO)

run: $(ISO)
	$(PORT)/scripts/run.sh

debug: $(ISO)
	$(PORT)/scripts/debug.sh

smoke: $(ISO)
	$(PORT)/scripts/smoke.sh

clean:
	rm -rf $(BUILD)
