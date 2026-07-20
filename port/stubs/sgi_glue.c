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

/* ---- filesystem switch (lboot-generated; the port registers memfs) ---- */

#include <sys/vfs.h>
#include <sys/vnode.h>

extern vfsops_t	memfs_vfsops;
extern vnodeops_t memfs_vnodeops;
/* vfs_strayops is defined by the real vfs.c */

/*
 * vfssw[]: index 0 is BADVFS (filled by vfsinit), index 1 is memfs.
 * rootfstype defaults to "memfs" so vfs_mountroot() selects it directly.
 */
struct vfssw vfssw[] = {
	{ "BADVFS", 0, 0, 0, 0, 0 },
	{ "memfs",  0, &memfs_vfsops, &memfs_vnodeops, 0, 0 },
};
int	vfsmax = sizeof(vfssw) / sizeof(vfssw[0]);
int	nfstype;			/* set by vfsinit() = vfsmax	*/

dev_t	rootdev;
char	rootfstype[16] = "memfs";
vnode_t	*rootdir;			/* memfs_rootinit() sets this	*/

/*
 * kmem_zone_init: vfsinit() ASSERTs the pathname zone is non-NULL.
 * The real zone/slab allocator is future work (M2 note); for now hand
 * back a small tag object so zone-based callers get a valid handle.
 * kmem_zone_alloc/zalloc fall through to the general kmem allocator.
 */
extern void *kmem_alloc(unsigned long, int);
extern void *kmem_zalloc(unsigned long, int);

struct zone_stub { int z_size; char *z_name; };

void *
kmem_zone_init(int size, char *name)
{
	struct zone_stub *z = kmem_alloc(sizeof(*z), 0);
	z->z_size = size;
	z->z_name = name;
	return z;
}

void *
kmem_zone_alloc(void *zone, int flags)
{
	return kmem_alloc(((struct zone_stub *)zone)->z_size, flags);
}

/*
 * kvpalloc(npgs, flags, color): allocate npgs pages of kernel virtual
 * memory.  The real IRIX kvpalloc maps fresh pages into the kernel's
 * mapped region (kseg2) via the page tables; here we hand back
 * contiguous direct-mapped (HHDM/kseg0) memory from the kmem arena,
 * which is a valid kernel VA and satisfies the "want N zeroed pages"
 * contract its callers rely on.
 */
void *
kvpalloc(unsigned int npgs, int flags, int color)
{
	return kmem_zalloc((unsigned long)npgs * NBPP, 0);
}

void *
kmem_zone_zalloc(void *zone, int flags)
{
	return kmem_zalloc(((struct zone_stub *)zone)->z_size, flags);
}

/* ---- device-name rendering for cmn_err %V (Root on device ...) ---- */

char *
dev_to_name(dev_t dev, char *buf, unsigned int len)
{
	/* minimal: render "memfs" for the synthetic root, else maj/min */
	if (dev == rootdev)
		strncpy(buf, "memfs", len);
	else {
		/* "0x%x" without stdio: tiny hex formatter */
		static const char hx[] = "0123456789abcdef";
		char *p = buf;
		int i, started = 0;
		*p++ = '0'; *p++ = 'x';
		for (i = 28; i >= 0; i -= 4) {
			int nyb = (dev >> i) & 0xf;
			if (nyb || started || i == 0) { *p++ = hx[nyb]; started = 1; }
		}
		*p = 0;
	}
	return buf;
}
