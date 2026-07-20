/*
 * irix/kern/ml/X86_64/vectors.S
 *
 * Exception and interrupt entry stubs — the x86-64 replacement for
 * the MIPS ml/LOCORE .s vectors.  Vectors 0-47 (CPU exceptions +
 * 16 legacy IRQ slots) plus 255 (APIC spurious) are generated here;
 * each pushes a normalized frame and funnels into trap_dispatch().
 */

.altmacro

.macro TRAPVEC n
	.align 16
vec\n:
.if !((\n == 8) | (\n == 10) | (\n == 11) | (\n == 12) | (\n == 13) | (\n == 14) | (\n == 17) | (\n == 21))
	pushq	$0		/* no hw error code: push dummy */
.endif
	pushq	$\n
	jmp	trap_entry
.endm

.macro VECPTR n
	.quad	vec\n
.endm

	.text

	.set	i, 0
	.rept	48
	TRAPVEC	%i
	.set	i, i+1
	.endr

	TRAPVEC	255

	/* int 0x80 system-call gate (installed at DPL 3 in idt_init) */
	TRAPVEC	128

	.align 16
trap_entry:
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	pushq	%rbx
	pushq	%rbp
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movq	%rsp, %rdi	/* eframe_t * */
	call	trap_dispatch

	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rbp
	popq	%rbx
	popq	%rdx
	popq	%rcx
	popq	%rax
	addq	$16, %rsp	/* drop vec + err */
	iretq

	.section .rodata
	.globl	trap_vectbl
trap_vectbl:
	.set	i, 0
	.rept	48
	VECPTR	%i
	.set	i, i+1
	.endr

	.globl	trap_vec_spurious
trap_vec_spurious:
	VECPTR	255

	.globl	trap_vec_syscall
trap_vec_syscall:
	VECPTR	128
