/*
 * port/user/crt0.S — x86-64 user program entry.
 *
 * The kernel enters here (ELF e_entry) in ring 3 with a fresh stack.
 * argv/envp setup on the initial stack is the real exec.c's job (M9); for
 * now call main(void) and turn its return into _exit(status).
 */
	.text
	.globl	_start
_start:
	xorq	%rbp, %rbp		/* end of frame chain	*/
	call	main
	movl	%eax, %edi		/* status = main() return */
	call	_exit			/* never returns	*/
	hlt
