# port/patched — SGI sources requiring x86-64 modifications

Convention: when an original SGI source file cannot compile or run
correctly on x86-64 even with the header/compat-shim machinery, it is
COPIED here and minimally edited. Every edit is marked `X86_64 PORT:`
in a comment at the site. The original under `irix/` is never touched.
The port build compiles the copy here instead of the original.

| file | original | reason |
|---|---|---|
| printf.c | irix/kern/os/printf.c | `%r`/`%R` decode fabricated a va_list by casting `&field` — legal on MIPS ABIs (va_list is a pointer), invalid on x86-64 SysV (va_list is a struct array). Added a variadic springboard (`errprintf_1arg`) that builds a genuine va_list. 2 sites. |
