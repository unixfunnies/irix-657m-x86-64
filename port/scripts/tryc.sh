#!/bin/sh
# Compile one original SGI kernel source file with the port's flags.
# Usage: tryc.sh <file.c> [extra clang args...] — errors only.
cd "$(dirname "$0")/../.."
SRC="$1"; shift
exec clang --target=x86_64-elf -std=gnu99 -g -O1 \
	-ffreestanding -nostdlibinc -fno-stack-protector -fno-stack-check \
	-fno-lto -fno-PIC -m64 -march=x86-64 \
	-mno-80387 -mno-mmx -mno-sse -mno-sse2 \
	-mno-red-zone -mcmodel=kernel \
	-Wno-everything \
	-D_KERNEL \
	-D__mips=3 -D_MIPS_ISA=3 -D_MIPS_SIM=3 \
	-D_MIPS_SZLONG=64 -D_MIPS_SZINT=32 -D_MIPS_SZPTR=64 \
	-D_MIPS_FPSET=32 -D_MIPSEL -D_LANGUAGE_C=1 -D_LONGLONG \
	-D_ABI64=3 -D_ABIN32=2 -D_ABIO32=1 \
	-D_PAGESZ=4096 -D_MIPS3_ADDRSPACE \
	-include port/include/mipspro_compat.h \
	-Iport/include -Iirix/kern -Iirix/include -Ieoe/include \
	-Iirix/kern/ml/X86_64 \
	-c "$SRC" "$@"
