/* init — a real ELF64 executable loaded from memfs via the VFS for M7.
 * Distinct from M5's flat icode: this is a linked ELF with PT_LOAD
 * segments that the port's ELF loader parses. write() + exit() via int 0x80.
 */
.code64
.section .text
.globl _start
_start:
    movq  $1, %rax              /* SYS_write */
    movq  $1, %rdi              /* fd 1 */
    leaq  msg(%rip), %rsi
    movq  $msglen, %rdx
    int   $0x80
    movq  $2, %rax              /* SYS_exit */
    movq  $7, %rdi              /* status 7 (distinctive) */
    int   $0x80
    hlt
.section .rodata
msg: .ascii "init: real ELF loaded from memfs via the VFS, running in ring 3!\n"
.set msglen, . - msg
