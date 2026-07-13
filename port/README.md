# IRIX x86-64 Port

Staged port of IRIX 6.5.7m to x86-64, booting via Limine in QEMU.
Plan/roadmap: see the milestone tags (m0, m1, ...).

## Layout
- `port/` — build system, boot config, scripts (this dir; new code)
- `irix/kern/ml/X86_64/` — the new x86-64 machine-dependent layer
- Original SGI sources are never modified; MD headers that must
  differ get x86-64 variants under `port/include/` ahead of the
  originals on the include path.

## Prereqs
clang, lld, make, xorriso, qemu-system-x86_64, and the Limine
binary release cloned into `port/boot/limine`:

    git clone --depth 1 --branch v8.6.0-binary \
        https://github.com/limine-bootloader/limine.git port/boot/limine
    make -C port/boot/limine

## Build & run
    make -C port          # kernel ELF (obj/unix) + bootable ISO
    make -C port run      # boot in QEMU, serial on stdio
    make -C port debug    # QEMU halted with gdb stub on :1234
    make -C port smoke    # regression: verify milestone checkpoints
