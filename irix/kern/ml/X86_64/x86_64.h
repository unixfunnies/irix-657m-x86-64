/*
 * irix/kern/ml/X86_64/x86_64.h
 *
 * Low-level x86-64 primitives for the IRIX x86-64 port.
 * This is new code (no SGI/MIPS heritage) — the analog of what
 * sys/asm.h + sys/reg.h + ml/spl.s provided on MIPS.
 */

#ifndef __ML_X86_64_H__
#define __ML_X86_64_H__

typedef unsigned char	__u8;
typedef unsigned short	__u16;
typedef unsigned int	__u32;
typedef unsigned long	__u64;
typedef long		__s64;

/* ---- port I/O ---- */

static inline void
outb(__u16 port, __u8 val)
{
	__asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline __u8
inb(__u16 port)
{
	__u8 val;
	__asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}

/* ---- MSRs, control registers, flags ---- */

static inline __u64
rdmsr(__u32 msr)
{
	__u32 lo, hi;
	__asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((__u64)hi << 32) | lo;
}

static inline void
wrmsr(__u32 msr, __u64 val)
{
	__asm__ __volatile__("wrmsr" : :
	    "a"((__u32)val), "d"((__u32)(val >> 32)), "c"(msr));
}

static inline void
load_cr3(__u64 pa)
{
	__asm__ __volatile__("movq %0, %%cr3" : : "r"(pa) : "memory");
}

static inline __u64
read_cr2(void)
{
	__u64 v;
	__asm__ __volatile__("movq %%cr2, %0" : "=r"(v));
	return v;
}

static inline void
sti(void)
{
	__asm__ __volatile__("sti");
}

static inline void
cli(void)
{
	__asm__ __volatile__("cli");
}

static inline void
cpu_wait(void)
{
	__asm__ __volatile__("hlt");
}

static inline void
cpu_halt(void)
{
	for (;;)
		__asm__ __volatile__("cli; hlt");
}

/*
 * Saved trap context.  Layout must match the push sequence in
 * vectors.S exactly (this is the x86-64 'eframe').
 */
typedef struct eframe {
	__u64	ef_r15, ef_r14, ef_r13, ef_r12, ef_r11, ef_r10, ef_r9, ef_r8;
	__u64	ef_rdi, ef_rsi, ef_rbp, ef_rbx, ef_rdx, ef_rcx, ef_rax;
	__u64	ef_vec;			/* vector number		*/
	__u64	ef_err;			/* hw error code or 0		*/
	__u64	ef_rip, ef_cs, ef_rflags, ef_rsp, ef_ss;
} eframe_t;

/* serial.c */
void	serial_early_init(void);
void	serial_putc(char c);
void	serial_puts(const char *s);

/* fb.c */
struct limine_framebuffer;
int	fb_init(struct limine_framebuffer *lf);
void	fb_putc(char c);

/* kprintf.c */
void	console_putc(char c);		/* fans out to serial + framebuffer */
void	kprintf(const char *fmt, ...);

/* gdt.c */
void	gdt_init(void);
void	tss_set_rsp0(__u64 rsp0);

/* usermode.c */
long	syscall_dispatch(__u64 nr, __u64 a0, __u64 a1, __u64 a2,
	    __u64 a3, __u64 a4);
void	usermode_demo(void);
void	exec_init_demo(void);
void	exec_echo_demo(void);

/* trap.c */
void	idt_init(void);
void	ml_panic(const char *msg, eframe_t *ef);

/* pmm.c */
struct limine_memmap_response;
void	pmm_init(struct limine_memmap_response *mm, __u64 hhdm,
	    __u64 skip_base, __u64 skip_size);
__u64	pmm_alloc(void);		/* phys addr of a zeroed 4K page */
void	pmm_free(__u64 pa);
__u64	pmm_free_pages(void);
__u64	pmm_total_pages(void);

/* kmem.c — IRIX kmem_alloc contract, x86-64 implementation */
void	kmem_reserve(struct limine_memmap_response *mm, __u64 hhdm,
	    __u64 cap, __u64 *out_base, __u64 *out_size);
void	*kmem_alloc(unsigned long size, int flags);
void	*kmem_zalloc(unsigned long size, int flags);
void	kmem_free(void *ptr, unsigned long size);
void	*kmem_alloc_node(unsigned long size, int flags, int node);
void	*kmem_zalloc_node(unsigned long size, int flags, int node);
__u64	kmem_arena_used(void);
__u64	kmem_arena_size(void);

/* pmap_boot.c */
void	pmap_bootstrap(__u64 hhdm, __u64 kphys, __u64 kvirt, __u64 ksize,
	    __u64 phys_top);
__u64	pmap_kernel_pml4(void);
__u64	pmap_new_user_as(void);
void	pmap_map_user(__u64 pml4, __u64 va, __u64 pa, int writable);
__u64	pmap_alloc_user_page(__u64 pml4, __u64 va, int writable);
void	pmap_switch(__u64 pml4_phys);
void	*pmap_phys_to_kv(__u64 pa);

/* apic.c */
void	apic_init(__u64 hhdm);
void	lapic_eoi(void);
extern volatile __u64 timer_ticks;

/* sched.c — minimal x86-64 kernel-thread scheduler */
int	kthread_spawn(const char *name, void (*entry)(void *), void *arg);
void	sched_yield(void);
void	thread_exit(void);
void	sched_tick(void);
void	sched_run(void);
void	sched_demo(void);

#define TIMER_HZ	100
#define VEC_TIMER	32
#define VEC_SYSCALL	128		/* int 0x80 system-call gate	*/
#define VEC_SPURIOUS	255

#endif /* __ML_X86_64_H__ */
