/* x86-64 "icode" — the minimal user-mode program for the port's M5.
 * Mapped at USER_CODE (0x400000). Uses RIP-relative addressing so it is
 * position-independent. Syscalls via int 0x80: rax=nr, rdi/rsi/rdx=args.
 *   SYS_write = 1, SYS_exit = 2  (port ABI; see usermode.c)
 */
.code64
.globl _start
_start:
    movq  $1, %rax              /* SYS_write */
    movq  $1, %rdi             /* fd = 1 (console) */
    leaq  msg(%rip), %rsi       /* buf */
    movq  $msglen, %rdx         /* len */
    int   $0x80
    movq  $2, %rax              /* SYS_exit */
    xorq  %rdi, %rdi            /* status 0 */
    int   $0x80
1:  jmp   1b                    /* should never reach */
msg:
    .ascii "Hello from x86-64 user mode (ring 3), via int 0x80!\n"
    .set msglen, . - msg
