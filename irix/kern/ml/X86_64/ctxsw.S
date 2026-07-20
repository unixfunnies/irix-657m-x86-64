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

/*
 * enter_usermode(entry, ustack, upml4, ksp_save) — drop to ring 3.
 *   rdi=entry rsi=ustack rdx=upml4(phys) rcx=&ksp_save
 * Saves the kernel context so SYS_exit can longjmp back via
 * leave_usermode(), switches to the user address space, and iretq's to
 * ring 3 with a fresh user stack.  Returns (in the kernel) only when
 * leave_usermode restores the saved stack.
 */
	.globl	enter_usermode
	.align	16
enter_usermode:
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	%rsp, (%rcx)		/* *ksp_save = kernel rsp	*/
	movq	%rdx, %cr3		/* switch to user address space	*/
	movw	$0x23, %ax		/* user data selector (0x20|3)	*/
	movw	%ax, %ds
	movw	%ax, %es
	pushq	$0x23			/* SS				*/
	pushq	%rsi			/* RSP (user stack)		*/
	pushq	$0x202			/* RFLAGS (IF set)		*/
	pushq	$0x1b			/* CS (user code 0x18|3)	*/
	pushq	%rdi			/* RIP (entry)			*/
	iretq				/* -> ring 3			*/

/*
 * leave_usermode(ksp_saved, kpml4) — return from ring 3 to the kernel.
 * Called from the SYS_exit handler (on the TSS ring-0 stack); switches
 * back to the kernel address space, abandons the trap frame, and resumes
 * enter_usermode's caller as if it had returned.
 */
	.globl	leave_usermode
	.align	16
leave_usermode:
	movq	%rsi, %cr3		/* back to kernel address space	*/
	movw	$0x10, %ax		/* kernel data selector		*/
	movw	%ax, %ds
	movw	%ax, %es
	movq	%rdi, %rsp		/* restore saved kernel stack	*/
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret
