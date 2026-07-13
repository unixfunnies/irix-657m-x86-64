/*
 * irix/kern/ml/X86_64/x86_64.h
 *
 * Low-level x86-64 primitives for the IRIX x86-64 port.
 * This is new code (no SGI/MIPS heritage) — the analog of what
 * sys/asm.h + sys/reg.h provided on MIPS.
 */

#ifndef __ML_X86_64_H__
#define __ML_X86_64_H__

typedef unsigned char	__u8;
typedef unsigned short	__u16;
typedef unsigned int	__u32;
typedef unsigned long	__u64;

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

static inline void
cpu_halt(void)
{
	for (;;)
		__asm__ __volatile__("cli; hlt");
}

/* serial.c */
void	serial_early_init(void);
void	serial_putc(char c);
void	serial_puts(const char *s);

/* kprintf.c */
void	kprintf(const char *fmt, ...);

#endif /* __ML_X86_64_H__ */
