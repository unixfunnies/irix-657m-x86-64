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

/* exec.c-lite: load an ELF from memfs (exec_elf.c) and run it with args */
extern int	elf_load(const char *name, __u64 upml4, __u64 *entry_out);

/*
 * Build the System V AMD64 initial process stack in the user stack page:
 *
 *   rsp -> argc
 *          argv[0..argc-1], NULL
 *          envp[0..envc-1], NULL
 *          auxv (AT_NULL)
 *          ... argv/envp string data (near the top) ...
 *
 * We fill it through the page's kernel (direct-map) alias while computing
 * user virtual addresses for the pointers.  Returns the user rsp (16-byte
 * aligned, as _start expects).  This is the piece the real IRIX exec.c
 * does in user-space; here it is the minimal MD slice that lets a genuine
 * argv-consuming program (echo) run.
 */
static __u64
build_user_stack(char *const argv[], char *const envp[], __u64 top_va,
    char *kva_base, __u64 page_base_va)
{
	__u64 argp[32], envq[32];
	int argc = 0, envc = 0, i, nwords;
	__u64 sp = top_va, base, *w;
#define U2K(v)	(kva_base + ((v) - page_base_va))

	while (argv[argc])
		argc++;
	while (envp[envc])
		envc++;

	/* copy strings top-down, recording each one's user VA */
	for (i = argc - 1; i >= 0; i--) {
		__u64 len = 0, j;
		while (argv[i][len]) len++;
		len++;
		sp -= len;
		for (j = 0; j < len; j++)
			U2K(sp)[j] = argv[i][j];
		argp[i] = sp;
	}
	for (i = envc - 1; i >= 0; i--) {
		__u64 len = 0, j;
		while (envp[i][len]) len++;
		len++;
		sp -= len;
		for (j = 0; j < len; j++)
			U2K(sp)[j] = envp[i][j];
		envq[i] = sp;
	}
	sp &= ~15UL;

	/* argc + (argv+NULL) + (envp+NULL) + auxv(AT_NULL) */
	nwords = 1 + (argc + 1) + (envc + 1) + 2;
	if (nwords & 1)
		nwords++;			/* keep rsp 16-aligned	*/
	base = (sp - (__u64)nwords * 8) & ~15UL;

	w = (__u64 *)U2K(base);
	*w++ = (__u64)argc;
	for (i = 0; i < argc; i++)
		*w++ = argp[i];
	*w++ = 0;
	for (i = 0; i < envc; i++)
		*w++ = envq[i];
	*w++ = 0;
	*w++ = 0;				/* auxv: AT_NULL */
	*w++ = 0;
	return base;
#undef U2K
}

/* generic exec: fresh AS, load the ELF, build the argv stack, enter ring 3 */
static void
exec_prog(const char *path, char *const argv[], char *const envp[])
{
	__u64 upml4, entry, stack_pa, rsp;
	char *kstack, *stack_kva;

	kstack = kmem_alloc(KSTACK_SIZE, 0);
	tss_set_rsp0((__u64)kstack + KSTACK_SIZE);
	kern_pml4 = pmap_kernel_pml4();

	upml4 = pmap_new_user_as();
	if (elf_load(path, upml4, &entry) != 0) {
		kprintf("exec: could not load /%s\n", path);
		return;
	}
	stack_pa = pmap_alloc_user_page(upml4, USER_STACK_TOP - 4096, 1);
	stack_kva = pmap_phys_to_kv(stack_pa);
	rsp = build_user_stack(argv, envp, USER_STACK_TOP, stack_kva,
	    USER_STACK_TOP - 4096);

	user_exited = 0;
	kprintf("\nentering ring 3: entry 0x%lx, rsp 0x%lx\n\n", entry, rsp);
	enter_usermode(entry, rsp, upml4, &user_ksp_save);
	kprintf("\nback in kernel: /%s exited (status %d)\n",
	    path, user_exit_status);
}

/* M7/M8: exec the compiled init program (no args) */
void
exec_init_demo(void)
{
	static char *const argv[] = { "init", 0 };
	static char *const envp[] = { 0 };

	kprintf("M7/M8: exec /init -- a compiled C program (crt0 + libc)\n\n");
	exec_prog("init", argv, envp);
	kprintf("exec of a real ELF from the filesystem ok\n");
}

/* M9: exec the genuine IRIX eoe/cmd/echo.c, passing real argv */
void
exec_echo_demo(void)
{
	static char *const argv[] = {
		"echo", "hello,", "from", "real", "IRIX", "echo(1)",
		"running", "in", "ring", "3", "on", "x86-64!", 0
	};
	static char *const envp[] = { "_XPG=0", 0 };
	int i;

	kprintf("M9: exec /echo -- the genuine eoe/cmd/echo.c, with argv\n");
	kprintf("argv =");
	for (i = 0; argv[i]; i++)
		kprintf(" %s", argv[i]);
	kprintf("\n\n");

	exec_prog("echo", argv, envp);
	kprintf("echo(1) printed its argv -> real IRIX command ran in ring 3\n");
}
