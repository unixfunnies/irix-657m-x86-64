# IRIX 6.5.7m → x86-64 port

A staged port of the genuine IRIX 6.5.7m source (SGI's MIPS Unix) to
x86-64, booting under Limine in QEMU. The goal is a real bootable kernel
built from the actual IRIX tree — reusing the machine-independent SGI
code and writing a fresh x86-64 machine-dependent layer under it.

**Status:** boots → mounts a root filesystem → schedules kernel threads →
enters ring 3 → execs a real ELF off its filesystem → runs a compiled C
userland program against a real syscall surface. Milestones `m0`–`m8`
(git tags). `make -C port smoke` re-verifies the whole chain (M0→M8).

---

## The big picture

Two worlds meet here:

- **Machine-independent IRIX C** (`irix/kern/os`, `fs`, `bsd`, `ksys`, …)
  is reused *as-is*. `main.c`, the VFS (`vfs.c`/`vnode.c`/`behavior.c`),
  `printf.c`, `move.c`, etc. are compiled straight from `irix/`.
- **A new x86-64 machine-dependent layer** lives in
  `irix/kern/ml/X86_64/` — the successor to the MIPS `irix/kern/ml/`.
  Boot, paging, traps/IDT, APIC timer, context switch, ring-3 entry,
  pmap, kmem: all written fresh for x86-64.

Original SGI source is **never edited**. Where a MIPS header must differ,
an x86-64 variant is shadowed under `port/include/` ahead of the original
on the include path. Where a whole SGI `.c` needs a change, a copy lives
in `port/patched/` (see `port/patched/PATCHES.md`) and is compiled
instead of the original.

## The toolchain trick (how SGI code compiles without MIPSpro)

There is no MIPSpro compiler. Instead, `port/scripts/tryc.sh` compiles
original SGI sources with **Clang's preprocessor masquerading as a
little-endian MIPS n64 compiler** (`-D_MIPSEL -D_MIPS_SZLONG=64 -D__mips=3
-D_SGI_SOURCE …`). x86-64 is LP64 little-endian — structurally identical
to a hypothetical MIPSEL64 — so the original headers' type-width and
byte-order decisions resolve correctly and the code compiles to real
x86-64 objects. `port/include/mipspro_compat.h` is force-included to shim
the few MIPSpro intrinsics the masquerade can't cover.

`port/COMPILES.txt` tracks which `irix/kern/os/*.c` compile clean (94/114
at last count).

## Build & run

Prereqs: `clang`, `lld`, `make`, `xorriso`, `qemu-system-x86_64`, plus the
Limine binary release:

    git clone --depth 1 --branch v8.6.0-binary \
        https://github.com/limine-bootloader/limine.git port/boot/limine
    make -C port/boot/limine

Then:

    make -C port          # kernel ELF (obj/unix) + bootable ISO
    make -C port run      # boot in QEMU, serial on stdio
    make -C port debug    # QEMU halted with a gdb stub on :1234
    make -C port smoke    # regression: assert every milestone checkpoint

The kernel writes to **both** COM1 serial and a framebuffer console, so
`make run` shows output in the terminal and it's also visible on screen
in VirtualBox / real hardware.

## How the kernel is linked (the stub layer)

`main.c` and friends reference far more symbols than are implemented yet.
`port/scripts/genstubs.sh` iteratively links, harvests undefined symbols,
and writes tracing stubs into `port/stubs/autostubs.c` (generated;
gitignored — `make` bootstraps it on a fresh checkout). Each stub prints
`[stub] <name>` once on first call, so the boot trace is a live map of
which subsystems are real vs. still stubbed. Symbols that must be *real*
(console, PDA, the init tables, the filesystem switch, kmem_zone, …) are
hand-written in `port/stubs/sgi_glue.c` and `port/stubs/libkern.c`.

## Layout

    irix/kern/ml/X86_64/   new x86-64 MD layer (start, trap, pmm, pmap_boot,
                           apic, kmem, sched, ctxsw.S, usermode, gdt, ...)
    port/build/            linker.ld, clang flags
    port/boot/             limine.conf + the Limine binary release
    port/scripts/          tryc.sh (compile SGI src), genstubs.sh, smoke.sh,
                           run.sh, debug.sh
    port/include/          x86-64 header shadows (sys/mips_addrspace.h,
                           sys/x86_64_pte.h, values.h, mipspro_compat.h) +
                           symlinks re-exporting SGI public headers
    port/stubs/            sgi_glue.c, libkern.c, autostubs.c (generated)
    port/patched/          minimally-edited copies of SGI .c files
    port/fs/               memfs.c (in-memory root fs), exec_elf.c (ELF
                           loader), usyscall.c (file syscalls), *_elf.h/motd.h
    port/user/             the userland: crt0.S + ulibc + init.c, built by
                           build-user.sh into the ELF memfs serves as /init

## Milestone map (git tags)

- `m0` first light — Limine boots the kernel, serial + framebuffer console
- `m1` CPU bringup — GDT/IDT, paging, PMM, LAPIC timer, interrupts
- `m2` MMU header contract + `kmem_alloc` (first-fit arena)
- `m3` IRIX `main.c` executes — prints its own banner, runs init to the end
- `m4` kernel-thread scheduler — `ctx_switch`, run queue, timer preemption
- `m5` ring-3 user mode — per-process AS + CR3 switch, `int 0x80` gate
- `m6` root filesystem — real VFS core + memfs; `main()` runs to completion
- `m7` real exec — ELF loaded from the filesystem via the real VOP path
- `m8` syscall surface + real compiled userland (copyin/out, open/read/…)

**Full roadmap and the next milestone (M9: real IRIX process model —
swtch.c/sthread/vproc/exec + argv) live in the plan file:**
`~/.claude/plans/i-want-to-make-eager-rabbit.md`

## Adding a milestone (the rhythm)

1. Explore the real SGI code for the contract (`tryc.sh <file>` to see what
   compiles; grep the headers for the macros/structs).
2. Write the x86-64 MD piece under `irix/kern/ml/X86_64/` (or a `port/fs`
   SGI-env file if it needs the VFS/vnode headers).
3. Adopt real SGI `.c` files by adding them to `OS_SRCS` in the Makefile;
   re-run `genstubs.sh` to close the link.
4. Demonstrate it from `start.c`, add checks to `smoke.sh`, confirm green.
5. Commit + tag `mN`; update the plan file's progress log.

Commits/tags are authored `unixfunnies <unixfunnies@gmail.com>`.
