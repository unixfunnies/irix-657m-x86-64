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

/* additional syscall numbers (SYS_write=1, SYS_exit=2 from the header) */
#define SYS_read	3
#define SYS_open	4
#define SYS_close	5
#define SYS_lseek	6
#define SYS_fstat	7
#define SYS_getpid	9

/* file syscalls that need the VFS live in usyscall.c (SGI env) */
extern long	usys_open(__u64 upath, int flags);
extern long	usys_read(int fd, __u64 ubuf, __u64 len);
extern long	usys_close(int fd);
extern long	usys_lseek(int fd, long off, int whence);
extern long	usys_fstat(int fd, __u64 ubuf);

/* pmap_boot.c: validate a user range in the active address space */
extern int	pmap_user_range_ok(__u64 va, __u64 len, int need_write);

/*
 * copyin/copyout — the only sanctioned way for the kernel to touch user
 * memory.  During a syscall CR3 is the user address space, so after
 * validating the range we can move bytes directly; the validation is what
 * keeps a bad user pointer from faulting the kernel.
 */
int
uva_copyin(__u64 uaddr, void *kdst, __u64 len)
{
	const char *s = (const char *)uaddr;
	char *d = kdst;
	__u64 i;

	if (!pmap_user_range_ok(uaddr, len, 0))
		return -1;
	for (i = 0; i < len; i++)
		d[i] = s[i];
	return 0;
}

int
uva_copyout(const void *ksrc, __u64 uaddr, __u64 len)
{
	const char *s = ksrc;
	char *d = (char *)uaddr;
	__u64 i;

	if (!pmap_user_range_ok(uaddr, len, 1))
		return -1;
	for (i = 0; i < len; i++)
		d[i] = s[i];
	return 0;
}

/* copy a NUL-terminated string in from user space, bounded by max */
int
uva_copyinstr(__u64 uaddr, char *kdst, __u64 max)
{
	__u64 i;

	for (i = 0; i < max; i++) {
		if (!pmap_user_range_ok(uaddr + i, 1, 0))
			return -1;
		kdst[i] = ((const char *)uaddr)[i];
		if (kdst[i] == '\0')
			return 0;
	}
	return -1;			/* not terminated within max */
}

/*
 * System-call dispatcher, called from trap.c on int 0x80.  Args are the
 * user's rdi/rsi/rdx/r10/r8; the return value goes back in rax.
 */
long
syscall_dispatch(__u64 nr, __u64 a0, __u64 a1, __u64 a2, __u64 a3, __u64 a4)
{
	(void)a3;
	(void)a4;
	switch (nr) {
	case SYS_write: {			/* write(fd, buf, len)	*/
		char kbuf[256];
		__u64 off = 0, len = a2;

		if (a0 != 1 && a0 != 2)		/* only stdout/stderr	*/
			return -1;
		while (off < len) {		/* stream in bounded chunks */
			__u64 n = len - off, i;
			if (n > sizeof(kbuf))
				n = sizeof(kbuf);
			if (uva_copyin(a1 + off, kbuf, n) != 0)
				return -1;
			for (i = 0; i < n; i++)
				console_putc(kbuf[i]);
			off += n;
		}
		return (long)len;
	}
	case SYS_exit:				/* exit(status)		*/
		user_exit_status = (int)a0;
		user_exited = 1;
		leave_usermode(user_ksp_save, kern_pml4);
		/* NOTREACHED */
		return 0;
	case SYS_open:				/* open(path, flags)	*/
		return usys_open(a0, (int)a1);
	case SYS_read:				/* read(fd, buf, len)	*/
		return usys_read((int)a0, a1, a2);
	case SYS_close:				/* close(fd)		*/
		return usys_close((int)a0);
	case SYS_lseek:				/* lseek(fd, off, whence) */
		return usys_lseek((int)a0, (long)a1, (int)a2);
	case SYS_fstat:				/* fstat(fd, statbuf)	*/
		return usys_fstat((int)a0, a1);
	case SYS_getpid:			/* getpid()		*/
		return 1;			/* single process for now */
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

/* M7: read a real ELF ("/init") from memfs through the VFS and exec it */
extern int	elf_load_init(__u64 upml4, __u64 *entry_out);

void
exec_init_demo(void)
{
	__u64 upml4, entry;
	char *kstack;

	kprintf("M7/M8: exec /init -- a real compiled C program "
	    "(crt0 + libc) loaded from memfs\n\n");

	kstack = kmem_alloc(KSTACK_SIZE, 0);
	tss_set_rsp0((__u64)kstack + KSTACK_SIZE);
	kern_pml4 = pmap_kernel_pml4();

	/* fresh address space; the ELF loader maps its PT_LOAD segments */
	upml4 = pmap_new_user_as();
	if (elf_load_init(upml4, &entry) != 0) {
		kprintf("exec: could not load /init\n");
		return;
	}
	/* user stack */
	pmap_alloc_user_page(upml4, USER_STACK_TOP - 4096, 1);

	user_exited = 0;
	kprintf("\nentering ring 3 at ELF entry 0x%lx\n\n", entry);
	enter_usermode(entry, USER_STACK_TOP - 16, upml4, &user_ksp_save);

	kprintf("\nback in kernel: /init exited (status %d)\n",
	    user_exit_status);
	kprintf("exec of a real ELF from the filesystem ok\n");
}
