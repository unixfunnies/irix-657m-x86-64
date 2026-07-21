/*
 * port/user/crt0.S — x86-64 user program entry.
 *
 * The kernel enters here (ELF e_entry) in ring 3 with the System V initial
 * stack: rsp -> argc, then argv[], NULL, envp[], NULL, auxv[] (built by the
 * kernel's exec path).  Unpack argc/argv/envp, stash environ for getenv(),
 * call main(argc, argv), and turn its return into _exit(status).
 */
	.text
	.globl	_start
_start:
	xorq	%rbp, %rbp
	movq	(%rsp), %rdi		/* argc			*/
	leaq	8(%rsp), %rsi		/* argv = &stack[1]	*/
	leaq	8(%rsi,%rdi,8), %rdx	/* envp = argv + (argc+1)*8 */
	movq	%rdx, environ(%rip)	/* environ = envp	*/
	call	main
	movl	%eax, %edi		/* status = main() return */
	call	exit			/* never returns	*/
	hlt
