/*
 * port/stubs/sgi_glue.c
 *
 * Hand-written glue between original SGI kernel code and the x86-64
 * ml layer: the things that must be REAL (not auto-stubbed) for
 * main()/cmn_err() to run.  Compiled with the SGI header environment
 * (port/scripts/tryc.sh) so struct layouts match exactly.
 */

#include <sys/types.h>
#include <sys/pda.h>
#include <sys/kthread.h>
#include <sys/utsname.h>
#include <sys/cmn_err.h>

/* ---- console character sink (ml layer) ---- */
extern void	console_putc(char c);
extern void	kprintf(const char *fmt, ...);

/*
 * arcs_write: the ARCS PROM console write vector.  Early cmn_err()
 * output lands here (PRW_ARCS) until a real console driver attaches.
 * On this port the "PROM console" is the ml serial+framebuffer console.
 */
long
arcs_write(unsigned long fd, void *buf, unsigned long n, unsigned long *cnt)
{
	char *p = buf;
	unsigned long i;

	(void)fd;
	for (i = 0; i < n; i++)
		console_putc(p[i]);
	if (cnt != 0)
		*cnt = n;
	return 0;	/* ESUCCESS */
}

/* ---- printf.c's message buffers (lboot/master.c provided these) ---- */

#define PORT_PUTBUFSZ	4096
#define PORT_CONBUFSZ	1024
#define PORT_ERRBUFSZ	1024

/* (printf.c itself owns putbufndx/constrlen/errbufndx) */
char	putbuf_store[PORT_PUTBUFSZ];
char	*putbuf = putbuf_store;
int	putbufsz = PORT_PUTBUFSZ;	/* must be a power of 2 */

char	conbuf_store[PORT_CONBUFSZ];
char	*conbuf = conbuf_store;
int	conbufsz = PORT_CONBUFSZ;

char	errbuf_store[PORT_ERRBUFSZ];
char	*errbuf = errbuf_store;
int	errbufsz = PORT_ERRBUFSZ;

/* ---- PDA / current-thread plumbing ---- */

/*
 * The PDA page is mapped at PDAADDR (0xffff...ffffc000) by
 * pmap_bootstrap().  These give SGI code its usual global views.
 */
pda_t		*masterpda;
pdaindr_t	pdaindr[1];

static kthread_t thread0;		/* static kernel thread 0 */

/* boot stats main() prints (normally set by MD startup / lboot) */
extern pfn_t	physmem;		/* declared in os/main.c	*/
int		numcpus = 1;
int		showconfig = 1;

/* ml layer exports */
extern unsigned long pmm_total_pages(void);

void
port_pda_init(void)
{
	pda_t *pda = (pda_t *)PDAADDR;

	bzero(pda, sizeof(pda_t));
	pda->p_cpuid = 0;
	pda->p_cpumask = 1;
	pda->p_curkthread = &thread0;	/* curthreadp now valid */
	pda->cpufreq = 1000;		/* nominal; QEMU vCPU		*/

	masterpda = pda;
	pdaindr[0].pda = pda;

	bzero(&thread0, sizeof(thread0));

	physmem = pmm_total_pages();	/* pages -> "Total real memory"	*/
}

/* ---- utsname: what the banner prints ---- */

struct utsname utsname = {
	"IRIX64",			/* sysname  */
	"irix-x86",			/* nodename */
	"6.5.7m",			/* release  */
	"x86-64-port-M3",		/* version  */
	"IP99"				/* machine: fictional x86-64 board */
};

char uname_releasename[128];		/* DEBUG-kernel release suffix */

/* ---- interrupt level (spl) — x86-64: IF flag, one level for now ---- */

static __inline void port_sti(void) { __asm__ __volatile__("sti"); }
static __inline void port_cli(void) { __asm__ __volatile__("cli"); }

int	spl0(void)  { port_sti(); return 0; }
int	spl7(void)  { port_cli(); return 7; }
int	splhi(void) { port_cli(); return 7; }
void	splx(int s) { if (s == 0) port_sti(); else port_cli(); }
int	isspl0(void) { return 1; }

/* ---- lboot-generated init tables: the port authors these ---- */

void	(*einit_tbl[])(void) = { 0 };
void	(*postintrinit_tbl[])(void) = { 0 };
void	(*init_tbl[])(void) = { 0 };
void	(*io_init[])(void) = { 0 };
void	(*io_start[])(void) = { 0 };
void	(*io_reg[])(void) = { 0 };

/* lboot also emitted these configuration globals */
char	*bootswapfile = "";
char	*bootdumpfile = "";
unsigned char miniroot = 0;
int	maxcpus = 1;
