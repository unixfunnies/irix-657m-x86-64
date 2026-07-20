/*
 * irix/kern/ml/X86_64/trap.c
 *
 * IDT setup and the C-level trap/interrupt dispatcher — the x86-64
 * analog of the MIPS gen_exc.s/trap.c path.
 */

#include "x86_64.h"

struct idtent {
	__u16	off_lo;
	__u16	sel;
	__u8	ist;
	__u8	type;
	__u16	off_mid;
	__u32	off_hi;
	__u32	rsvd;
} __attribute__((packed));

struct idtr {
	__u16	limit;
	__u64	base;
} __attribute__((packed));

static struct idtent idt[256];

extern __u64 trap_vectbl[48];
extern __u64 trap_vec_spurious[1];
extern __u64 trap_vec_syscall[1];
extern __u64 syscall_caller_cs;		/* usermode.c: CS at last int 0x80 */

static const char *trapnames[] = {
	"divide error", "debug", "NMI", "breakpoint",
	"overflow", "bound range", "invalid opcode", "device not available",
	"double fault", "coproc segment overrun", "invalid TSS",
	"segment not present", "stack fault", "general protection",
	"page fault", "reserved15", "x87 FP error", "alignment check",
	"machine check", "SIMD FP", "virtualization", "control protection",
};

static void
idt_set_dpl(int vec, __u64 handler, int dpl)
{
	idt[vec].off_lo = handler & 0xffff;
	idt[vec].sel = 0x08;		/* kernel code selector	*/
	idt[vec].ist = 0;
	idt[vec].type = 0x8e | (dpl << 5);	/* present, interrupt gate */
	idt[vec].off_mid = (handler >> 16) & 0xffff;
	idt[vec].off_hi = handler >> 32;
	idt[vec].rsvd = 0;
}

static void
idt_set(int vec, __u64 handler)
{
	idt_set_dpl(vec, handler, 0);
}

void
idt_init(void)
{
	static struct idtr idtr;
	int i;

	for (i = 0; i < 48; i++)
		idt_set(i, trap_vectbl[i]);
	idt_set(VEC_SPURIOUS, trap_vec_spurious[0]);
	/* int 0x80 syscall gate: DPL 3 so ring-3 code may invoke it */
	idt_set_dpl(VEC_SYSCALL, trap_vec_syscall[0], 3);

	idtr.limit = sizeof(idt) - 1;
	idtr.base = (__u64)idt;
	__asm__ __volatile__("lidt %0" : : "m"(idtr));
}

void
ml_panic(const char *msg, eframe_t *ef)
{
	kprintf("\nPANIC: %s\n", msg);
	if (ef != 0) {
		kprintf("vec=%lu err=0x%lx\n", ef->ef_vec, ef->ef_err);
		kprintf("rip=0x%lx cs=0x%lx rflags=0x%lx\n",
		    ef->ef_rip, ef->ef_cs, ef->ef_rflags);
		kprintf("rsp=0x%lx ss=0x%lx rbp=0x%lx\n",
		    ef->ef_rsp, ef->ef_ss, ef->ef_rbp);
		kprintf("rax=0x%lx rbx=0x%lx rcx=0x%lx rdx=0x%lx\n",
		    ef->ef_rax, ef->ef_rbx, ef->ef_rcx, ef->ef_rdx);
		kprintf("rsi=0x%lx rdi=0x%lx r8=0x%lx r9=0x%lx\n",
		    ef->ef_rsi, ef->ef_rdi, ef->ef_r8, ef->ef_r9);
		if (ef->ef_vec == 14)
			kprintf("cr2=0x%lx (faulting address)\n", read_cr2());
	}
	cpu_halt();
}

volatile __u64 timer_ticks;

void trap_dispatch(eframe_t *ef);

void
trap_dispatch(eframe_t *ef)
{
	switch (ef->ef_vec) {
	case 3:				/* breakpoint: recoverable, used */
					/* as the IDT self-test		 */
		kprintf("trap: breakpoint at rip=0x%lx (IDT ok)\n",
		    ef->ef_rip);
		return;

	case VEC_TIMER:
		timer_ticks++;
		lapic_eoi();		/* EOI before any preemptive switch */
		sched_tick();		/* may ctx_switch to another thread */
		return;

	case VEC_SPURIOUS:
		return;

	case VEC_SYSCALL:		/* int 0x80 from ring 3 */
		/*
		 * Syscall args arrive in the saved GPRs; the return value
		 * goes back in ef_rax, which iretq restores.  This is the
		 * x86-64 analog of the MIPS syscall exception path; the real
		 * IRIX syscallsw dispatch is grafted on with vproc (post-M5).
		 * ef_cs records the caller's privilege (CPL 3 = ring 3).
		 */
		syscall_caller_cs = ef->ef_cs;
		ef->ef_rax = syscall_dispatch(ef->ef_rax, ef->ef_rdi,
		    ef->ef_rsi, ef->ef_rdx, ef->ef_r10, ef->ef_r8);
		return;

	default:
		ml_panic(ef->ef_vec < 22 ? trapnames[ef->ef_vec] :
		    "unexpected trap", ef);
	}
}
