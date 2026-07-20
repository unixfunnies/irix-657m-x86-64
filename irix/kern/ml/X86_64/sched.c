/*
 * irix/kern/ml/X86_64/sched.c
 *
 * Minimal x86-64 kernel-thread scheduler — the machine-dependent base
 * the real IRIX scheduler (os/swtch.c + os/sthread.c) will later be
 * grafted onto (a dedicated post-M5 milestone; see the port plan).
 *
 * A round-robin run queue over ctx_switch() (ctxsw.S).  Threads run in
 * kernel context on their own kmem-allocated stacks; they yield
 * cooperatively (sched_yield) and are also preempted by the LAPIC timer
 * (sched_tick, called from the timer ISR) when their slice expires.
 * This is the analog of the sthread layer: fn(arg) kernel threads with
 * no user address space (that arrives with M5).
 */

#include "x86_64.h"

#define MAXTHREADS	16
#define STACKSIZE	(16 * 1024)	/* 16 KB kernel stack per thread	*/
#define SLICE_TICKS	5		/* preempt after 5 timer ticks (~50ms)	*/

enum tstate { T_UNUSED = 0, T_RUNNABLE, T_RUNNING, T_DEAD };

typedef struct kthread_ctx {
	unsigned long	sp;		/* saved stack pointer (ctx_switch)	*/
	enum tstate	state;
	int		id;
	const char	*name;
	char		*stack;		/* base of kmem stack allocation	*/
} kthread_ctx_t;

extern void	ctx_switch(unsigned long *old_sp, unsigned long new_sp);
extern void	thread_trampoline(void);

static kthread_ctx_t	threads[MAXTHREADS];
static kthread_ctx_t	sched_ctx;	/* the boot context (scheduler root)	*/
static kthread_ctx_t	*cur;
static int		nthreads;
static int		nrunnable;
static volatile int	slice;		/* timer ticks left in current slice	*/
static int		sched_active;

/*
 * Plant an initial stack frame so the first ctx_switch into this thread
 * pops entry into r12, arg into r13, and "returns" into thread_trampoline.
 * Must mirror ctxsw.S's pop order: r15,r14,r13,r12,rbp,rbx, then ret.
 */
static void
kthread_setup(kthread_ctx_t *t, void (*entry)(void *), void *arg)
{
	unsigned long *sp;

	sp = (unsigned long *)(t->stack + STACKSIZE);
	*--sp = (unsigned long)thread_trampoline;	/* ret target	*/
	*--sp = 0;					/* rbx		*/
	*--sp = 0;					/* rbp		*/
	*--sp = (unsigned long)entry;			/* r12 = entry	*/
	*--sp = (unsigned long)arg;			/* r13 = arg	*/
	*--sp = 0;					/* r14		*/
	*--sp = 0;					/* r15		*/
	t->sp = (unsigned long)sp;
}

int
kthread_spawn(const char *name, void (*entry)(void *), void *arg)
{
	kthread_ctx_t *t;
	int i;

	for (i = 0; i < MAXTHREADS; i++)
		if (threads[i].state == T_UNUSED)
			break;
	if (i == MAXTHREADS)
		return -1;

	t = &threads[i];
	t->id = i;
	t->name = name;
	t->stack = (char *)kmem_alloc(STACKSIZE, 0);
	if (t->stack == 0)
		return -1;
	kthread_setup(t, entry, arg);
	t->state = T_RUNNABLE;
	nthreads++;
	nrunnable++;
	return i;
}

/* pick the next runnable thread after 'from' (round robin); NULL if none */
static kthread_ctx_t *
pick_next(kthread_ctx_t *from)
{
	int start = (from >= threads && from < threads + MAXTHREADS)
	    ? from->id : -1;
	int i, j;

	for (j = 1; j <= MAXTHREADS; j++) {
		i = (start + j) % MAXTHREADS;
		if (threads[i].state == T_RUNNABLE)
			return &threads[i];
	}
	return 0;
}

/*
 * Switch from cur into the next runnable thread.  If none remain, switch
 * back to the scheduler root (boot context), which ends sched_run().
 */
static void
schedule(void)
{
	kthread_ctx_t *prev = cur;
	kthread_ctx_t *next = pick_next(cur);

	if (next == 0) {
		/* no runnable threads: return to the boot/root context */
		if (cur != &sched_ctx) {
			cur = &sched_ctx;
			slice = SLICE_TICKS;
			ctx_switch(&prev->sp, sched_ctx.sp);
		}
		return;
	}

	if (next == prev) {
		slice = SLICE_TICKS;
		return;
	}

	if (prev->state == T_RUNNING)
		prev->state = T_RUNNABLE;
	next->state = T_RUNNING;
	cur = next;
	slice = SLICE_TICKS;
	ctx_switch(&prev->sp, next->sp);
}

void
sched_yield(void)
{
	cli();
	schedule();
	sti();
}

void
thread_exit(void)
{
	cli();
	cur->state = T_DEAD;	/* stack not freed here — we're still on it;
				 * a real reaper frees dead stacks later	*/
	nrunnable--;
	schedule();		/* never returns to a dead thread */
	ml_panic("thread_exit: scheduled a dead thread", 0);
}

/*
 * Called from the LAPIC timer ISR (trap.c).  Decrements the current
 * slice; when it hits zero, forces a reschedule (preemption).  Runs in
 * interrupt context on the current thread's own stack, so ctx_switch
 * from here is safe: we resume mid-ISR when switched back.
 */
void
sched_tick(void)
{
	if (!sched_active)
		return;
	if (--slice <= 0)
		schedule();
}

/*
 * Start scheduling: runs until every spawned thread has exited, then
 * returns to the caller (the boot context).
 */
void
sched_run(void)
{
	kthread_ctx_t *first;

	cur = &sched_ctx;
	sched_ctx.name = "sched-root";
	sched_ctx.state = T_RUNNING;

	first = pick_next(&sched_ctx);
	if (first == 0)
		return;			/* nothing to run */

	slice = SLICE_TICKS;
	sched_active = 1;
	first->state = T_RUNNING;
	cur = first;
	ctx_switch(&sched_ctx.sp, first->sp);	/* returns when all exit */
	sched_active = 0;
}

/* ---------------- M4 demonstration ---------------- */

/*
 * Cooperative workers: each prints a few rounds and voluntarily yields
 * between them.  Interleaved output (A,B,C,A,B,C,...) proves round-robin
 * context switching over ctx_switch().
 */
static void
coop_worker(void *arg)
{
	const char *name = arg;
	int i;

	for (i = 0; i < 3; i++) {
		kprintf("[sched] coop %s: round %d\n", name, i);
		sched_yield();
	}
	kprintf("[sched] coop %s: done\n", name);
}

/*
 * Preemption demo: these threads NEVER yield — they burn CPU in a busy
 * loop.  If their progress lines interleave, the LAPIC timer preempted
 * one mid-loop to run the other (sched_tick -> schedule).  Under a purely
 * cooperative scheduler one would run to completion before the other
 * started.
 */
static void
spin_worker(void *arg)
{
	const char *name = arg;
	int k;

	for (k = 0; k < 3; k++) {
		volatile unsigned long i;
		for (i = 0; i < 40000000UL; i++)	/* burn, no yield */
			;
		kprintf("[sched] spin %s: progress %d\n", name, k);
	}
	kprintf("[sched] spin %s: done\n", name);
}

void
sched_demo(void)
{
	kprintf("M4: kernel-thread scheduler demo\n\n");

	kprintf("cooperative round-robin (yield):\n");
	kthread_spawn("A", coop_worker, "A");
	kthread_spawn("B", coop_worker, "B");
	kthread_spawn("C", coop_worker, "C");
	sched_run();
	kprintf("cooperative switching ok\n\n");

	kprintf("preemptive (timer, no yield):\n");
	kthread_spawn("X", spin_worker, "X");
	kthread_spawn("Y", spin_worker, "Y");
	sched_run();
	kprintf("preemptive switching ok\n");
}
