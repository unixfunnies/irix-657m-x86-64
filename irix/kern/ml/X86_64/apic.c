/*
 * irix/kern/ml/X86_64/apic.c
 *
 * Local APIC bringup and timer — the x86-64 stand-in for the MIPS
 * count/compare clock (ml/clksupport.c).  Legacy PICs are masked;
 * the LAPIC timer is calibrated against the PIT and run periodic at
 * TIMER_HZ on VEC_TIMER.
 */

#include "x86_64.h"

#define MSR_APIC_BASE	0x1b

/* LAPIC register offsets */
#define LAPIC_EOI	0x0b0
#define LAPIC_SVR	0x0f0		/* spurious vector	*/
#define LAPIC_LVT_TIMER	0x320
#define LAPIC_TIMER_ICR	0x380		/* initial count	*/
#define LAPIC_TIMER_CCR	0x390		/* current count	*/
#define LAPIC_TIMER_DCR	0x3e0		/* divide config	*/

#define LVT_PERIODIC	0x20000
#define DCR_DIV16	0x3

#define PIT_HZ		1193182

static volatile __u32 *lapic;

static __u32
lapic_rd(int reg)
{
	return lapic[reg / 4];
}

static void
lapic_wr(int reg, __u32 val)
{
	lapic[reg / 4] = val;
}

void
lapic_eoi(void)
{
	lapic_wr(LAPIC_EOI, 0);
}

/*
 * Mask the legacy 8259 PICs (after remapping them clear of the CPU
 * exception range, so any spurious legacy IRQ is identifiable).
 */
static void
pic_mask_all(void)
{
	outb(0x20, 0x11);	/* ICW1: init, cascade, ICW4	*/
	outb(0xa0, 0x11);
	outb(0x21, 0x20);	/* ICW2: master base 0x20	*/
	outb(0xa1, 0x28);	/* ICW2: slave base 0x28	*/
	outb(0x21, 0x04);	/* ICW3				*/
	outb(0xa1, 0x02);
	outb(0x21, 0x01);	/* ICW4: 8086 mode		*/
	outb(0xa1, 0x01);
	outb(0x21, 0xff);	/* mask everything		*/
	outb(0xa1, 0xff);
}

/*
 * Let the LAPIC timer free-run for 10ms (measured by PIT channel 2
 * in one-shot mode) to learn its frequency.
 */
static __u32
lapic_timer_calibrate(void)
{
	__u32 elapsed;
	__u16 pit_count = PIT_HZ / 100;		/* 10ms */
	__u8 gate;

	/* gate ch2 low, speaker off */
	gate = inb(0x61) & 0xfc;
	outb(0x61, gate);

	outb(0x43, 0xb0);			/* ch2, lo/hi, mode 0 */
	outb(0x42, pit_count & 0xff);
	outb(0x42, pit_count >> 8);

	lapic_wr(LAPIC_TIMER_DCR, DCR_DIV16);
	lapic_wr(LAPIC_TIMER_ICR, 0xffffffff);

	outb(0x61, gate | 1);			/* raise gate: start */
	while ((inb(0x61) & 0x20) == 0)		/* wait for OUT high */
		;

	elapsed = 0xffffffff - lapic_rd(LAPIC_TIMER_CCR);
	lapic_wr(LAPIC_TIMER_ICR, 0);		/* stop */
	outb(0x61, gate);

	return elapsed;				/* LAPIC ticks / 10ms */
}

void
apic_init(__u64 hhdm)
{
	__u64 base = rdmsr(MSR_APIC_BASE);
	__u32 per10ms;

	lapic = (volatile __u32 *)((base & ~0xfffUL) + hhdm);

	pic_mask_all();

	/* software-enable LAPIC, spurious vector 255 */
	lapic_wr(LAPIC_SVR, 0x100 | VEC_SPURIOUS);

	per10ms = lapic_timer_calibrate();
	kprintf("lapic: base 0x%lx, timer %u ticks/10ms (div 16)\n",
	    base & ~0xfffUL, per10ms);

	/* periodic at TIMER_HZ */
	lapic_wr(LAPIC_TIMER_DCR, DCR_DIV16);
	lapic_wr(LAPIC_LVT_TIMER, VEC_TIMER | LVT_PERIODIC);
	lapic_wr(LAPIC_TIMER_ICR, per10ms * (100 / TIMER_HZ));
}
