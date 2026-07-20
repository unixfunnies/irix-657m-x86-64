/*
 * irix/kern/ml/X86_64/ctxsw.S
 *
 * x86-64 kernel-thread context switch — the machine-dependent primitive
 * that on MIPS was ml/process.s:resume().  That version wired a per-CPU
 * TLB entry for the kernel stack and juggled cache barriers; x86-64 needs
 * none of that (kernel stacks live in the direct map), so a switch is
 * just: save the callee-saved registers on the outgoing stack, swap the
 * stack pointer, restore.  The System V ABI says rbx/rbp/r12-r15 are
 * callee-saved, so those plus rsp fully capture a cooperative kernel
 * context; everything else is already dead across the call.
 *
 *   void ctx_switch(unsigned long *old_rsp, unsigned long new_rsp);
 *     rdi = &old->sp  (where to stash the outgoing stack pointer)
 *     rsi = new->sp   (stack pointer to resume)
 */

	.text
	.globl	ctx_switch
	.align	16
ctx_switch:
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	%rsp, (%rdi)		/* *old_rsp = rsp		*/
	movq	%rsi, %rsp		/* rsp = new_rsp		*/
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret				/* resume at saved return addr	*/

/*
 * thread_trampoline — first thing a brand-new thread "returns" into.
 * ctx_switch's pops leave r12=entry, r13=arg (planted by kthread_spawn);
 * call entry(arg), then thread_exit() if it ever returns.
 *
 * ctx_switch does not carry the interrupt flag between contexts, so a
 * fresh thread inherits whatever IF happened to be set at switch-in time.
 * Enable interrupts here (the analog of MIPS sthread_launch's spl0) so
 * every thread runs preemptible regardless of who switched into it.
 */
	.globl	thread_trampoline
	.align	16
thread_trampoline:
	sti				/* run with interrupts enabled	*/
	movq	%r13, %rdi		/* arg				*/
	callq	*%r12			/* entry(arg)			*/
	callq	thread_exit		/* never returns		*/
	ud2
