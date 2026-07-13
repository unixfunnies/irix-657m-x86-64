/*
 * port/include/sys/x86_64_pte.h
 *
 * x86-64 page-table constants: the hardware PTE flag bits and the KPTE
 * recursive-mapping window.  Pulled in from the x86-64 mips_addrspace.h
 * shadow, so it lands wherever <sys/immu.h> is used.  Uses only builtin
 * types, since immu.h pulls in mips_addrspace.h before <sys/types.h>.
 *
 * Deliberately does NOT define `struct pte`: with no MIPS board selected,
 * immu.h supplies its opaque cross-compilation pte (`struct pte { uint_t
 * opaque_pte; }`), which is exactly right for machine-independent code —
 * MI code never inspects PTE fields.  The code that DOES read real PTE
 * fields (os/as/pmap.c, the kern_heap TLB-context shadow paths) is MIPS
 * software-TLB machinery replaced wholesale by the x86-64 pmap (hazard #2),
 * not compiled against a faked layout.  These constants serve that new
 * hardware-page-table pmap.
 */

#ifndef __SYS_X86_64_PTE_H
#define __SYS_X86_64_PTE_H

/* hardware flag masks, for code that manipulates raw PTE words */
#define	PG_VR		0x001UL		/* present ("valid")		*/
#define	PG_M		0x002UL		/* writable			*/
#define	PG_U		0x004UL		/* user				*/
#define	PG_PWT		0x008UL
#define	PG_PCD		0x010UL		/* cache-inhibit (MMIO)		*/
#define	PG_ACC		0x020UL		/* accessed			*/
#define	PG_DIRTY	0x040UL		/* dirty			*/
#define	PG_PS		0x080UL		/* large page			*/
#define	PG_G		0x100UL		/* global			*/
#define	PG_NX		0x8000000000000000UL
#define	PG_PFNMASK	0x000ffffffffff000UL

/*
 * KPTE recursive page-table window.  The port maps the kernel PML4 into
 * its own slot 510, so the entire page-table hierarchy is visible as a
 * linear array based here — the x86-64 stand-in for the MIPS KPTEBASE
 * segment-table window.
 */
#define	KPTEBASE	0xffffff0000000000UL
#define	PDESHFT		3		/* 8-byte PTEs			*/

#endif /* __SYS_X86_64_PTE_H */
