/*
 * irix/kern/ml/X86_64/usermode.c
 *
 * Ring-3 user mode — the machine-dependent pieces IRIX's exec/syscall
 * path will sit on: a per-process address space (its own PML4 sharing
 * the kernel higher half), the ring-0<->ring-3 transition, and a system
 * call gate (int 0x80).  This is the x86-64 analog of what main() did on
 * MIPS with the embedded "icode" that execs /etc/init: here a tiny
 * x86-64 icode is loaded into a fresh user address space and run in
 * ring 3, where it issues write(1,...) and exit(0) through int 0x80.
 *
 * The real IRIX exec (ELF load from a vnode) and syscallsw dispatch are
 * grafted on later, together with the real scheduler/vproc (post-M5).
 */

#include "x86_64.h"

/* port syscall numbers (the icode agrees on these) */
#define SYS_write	1
#define SYS_exit	2

#define USER_CODE	0x0000000000400000UL	/* icode load address	*/
#define USER_STACK_TOP	0x0000000000501000UL	/* one page of stack	*/
#define KSTACK_SIZE	(16 * 1024)

extern void	enter_usermode(__u64 entry, __u64 ustack, __u64 upml4,
		    __u64 *ksp_save);
extern void	leave_usermode(__u64 ksp_saved, __u64 kpml4);

/*
 * x86-64 icode — assembled from irix/kern/ml/X86_64/icode.S (kept in the
 * tree for provenance): write(1,msg,len); exit(0); via int 0x80.
 */
static const unsigned char icode[] = {
	72,199,192,1,0,0,0,72,199,199,1,0,
	0,0,72,141,53,23,0,0,0,72,199,194,
	52,0,0,0,205,128,72,199,192,2,0,0,
	0,72,49,255,205,128,235,254,72,101,108,108,
	111,32,102,114,111,109,32,120,56,54,45,54,
	52,32,117,115,101,114,32,109,111,100,101,32,
	40,114,105,110,103,32,51,41,44,32,118,105,
	97,32,105,110,116,32,48,120,56,48,33,10,
};

static __u64	user_ksp_save;		/* kernel rsp saved by enter_usermode */
static __u64	kern_pml4;
static int	user_exited;
static int	user_exit_status;
__u64		syscall_caller_cs;	/* CS at last int 0x80 (trap.c sets) */

/*
 * System-call dispatcher, called from trap.c on int 0x80.  Args are the
 * user's rdi/rsi/rdx/...; during the trap CR3 is still the user address
 * space, so user pointers are directly readable (no copyin machinery yet).
 */
long
syscall_dispatch(__u64 nr, __u64 a0, __u64 a1, __u64 a2, __u64 a3, __u64 a4)
{
	(void)a3;
	(void)a4;
	switch (nr) {
	case SYS_write: {			/* write(fd, buf, len)	*/
		const char *buf = (const char *)a1;
		__u64 len = a2, i;

		for (i = 0; i < len; i++)
			console_putc(buf[i]);
		return (long)len;
	}
	case SYS_exit:				/* exit(status)		*/
		user_exit_status = (int)a0;
		user_exited = 1;
		leave_usermode(user_ksp_save, kern_pml4);
		/* NOTREACHED */
		return 0;
	default:
		kprintf("syscall: unknown nr %lu\n", nr);
		return -1;
	}
}

void
usermode_demo(void)
{
	__u64 upml4, code_pa;
	char *kstack;
	unsigned i;

	kprintf("M5: user mode (ring 3) demo\n\n");

	/* dedicated kernel stack for ring3 -> ring0 (int 0x80) traps */
	kstack = kmem_alloc(KSTACK_SIZE, 0);
	tss_set_rsp0((__u64)kstack + KSTACK_SIZE);

	kern_pml4 = pmap_kernel_pml4();

	/* build the user address space and load icode into it */
	upml4 = pmap_new_user_as();
	code_pa = pmap_alloc_user_page(upml4, USER_CODE, 0 /* r-x */);
	{
		unsigned char *dst = pmap_phys_to_kv(code_pa);
		for (i = 0; i < sizeof(icode); i++)
			dst[i] = icode[i];
	}
	/* one user stack page just below USER_STACK_TOP */
	pmap_alloc_user_page(upml4, USER_STACK_TOP - 4096, 1 /* rw */);

	kprintf("entering ring 3: code@0x%lx stack@0x%lx\n\n",
	    USER_CODE, USER_STACK_TOP);

	enter_usermode(USER_CODE, USER_STACK_TOP - 16, upml4, &user_ksp_save);

	/* back in the kernel via leave_usermode() from SYS_exit */
	kprintf("\nback in kernel: user process exited (status %d)\n",
	    user_exit_status);
	kprintf("syscall caller CS=0x%lx (CPL %lu -> %s)\n",
	    syscall_caller_cs, syscall_caller_cs & 3,
	    (syscall_caller_cs & 3) == 3 ? "ring 3 confirmed" : "NOT ring 3");
	kprintf("user->kernel syscall round trip ok\n");
}
