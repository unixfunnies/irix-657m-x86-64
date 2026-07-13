/*
 * port/include/sys/mips_addrspace.h  —  x86-64 variant
 *
 * Shadows irix/kern/sys/mips_addrspace.h (found first via -Iport/include).
 * It reuses that header's include guard so the MIPS original is fully
 * suppressed, and re-exports every name the original defined, but with
 * x86-64 semantics.  This is the address-space half of the M2 MD/MI
 * memory contract — the successor to the MIPS KSEG layout.
 *
 * x86-64 canonical higher-half kernel map used by the port
 * (see irix/kern/ml/X86_64/pmap_boot.c):
 *
 *   0xffff800000000000  K0BASE   direct map of all physical RAM (HHDM),
 *                                 the analog of cached kseg0.  Set up by
 *                                 Limine; matched by pmap_bootstrap().
 *   0xffffc00000000000  K2BASE   mapped (page-table-backed) kernel region
 *                                 — kernel heap / vmalloc, analog of kseg2.
 *   0xffffff0000000000  KPTEBASE  recursive page-table window (PML4 slot 510)
 *   0xffffffff80000000  kernel image (see linker.ld / COMPAT_K0BASE)
 *
 * NOTE: x86-64 has no address-bit-encoded uncached window like MIPS kseg1.
 * Cache-inhibited access is a page-attribute (PCD/PWT) here, so the K1
 * ("uncached") macros alias the direct map for now; MMIO must instead be
 * mapped with PG_PCD.  Flagged for the driver layer (post-M6).
 */

#ifndef __SYS_MIPS_ADDRSPACE_H
#define __SYS_MIPS_ADDRSPACE_H

/*
 * Direct map (kseg0 analog).  Because K0BASE's low bits are zero and a
 * physical address is far smaller than the base, (p | K0BASE) == (p +
 * K0BASE), so the original OR/AND macro forms stay valid on x86-64.
 */
#define	K0BASE			0xffff800000000000UL
#define	K0BASE_EXL_WR		K0BASE
#define	K0BASE_EXL		K0BASE
#define	K0BASE_NONCOH		K0BASE
#define	K0SIZE			0x0000400000000000UL	/* 64 TB window   */

/* kseg1 (uncached) — no x86-64 equivalent; alias direct map (see NOTE) */
#define	K1BASE			K0BASE
#define	K1SIZE			K0SIZE

/* kseg2 — mapped (paged) kernel region: kernel heap / dynamic maps      */
#define	K2BASE			0xffffc00000000000UL
#define	K2SIZE			0x0000200000000000UL

/* user region (canonical lower half) */
#define	KUBASE			0x0000000000000000UL
#define	KUSIZE			0x0000800000000000UL	/* 47-bit user VA  */
#define	KUSIZE_32		0x0000000100000000UL
#define	KUSIZE_64		KUSIZE

#define	KSEGSIZE		K2SIZE			/* max syssegsz    */

/* Kernel image / boot compatibility window (matches linker.ld ENTRY base) */
#define	COMPAT_K0BASE		0xffffffff80000000UL
#define	COMPAT_K1BASE		0xffffffff80000000UL
#define	COMPAT_K0BASE32		COMPAT_K0BASE
#define	COMPAT_K1BASE32		COMPAT_K1BASE
#define	COMPAT_KSIZE		0x0000000080000000UL

/*
 * phys <-> direct-map conversions.  Additive on x86-64 (see above), which
 * is exact for any physical address in the window.
 */
#define	TO_PHYS_MASK		0x0000ffffffffffffUL	/* 48-bit phys     */
#define	TO_K0_MASK		K0BASE
#define	TO_COMPAT_PHYS_MASK	0x000000007fffffffUL

#define	PHYS_TO_K0(x)		((__psunsigned_t)(x) + K0BASE)
#define	PHYS_TO_K1(x)		((__psunsigned_t)(x) + K1BASE)
#define	K0_TO_PHYS(x)		((__psunsigned_t)(x) - K0BASE)
#define	K1_TO_PHYS(x)		((__psunsigned_t)(x) - K1BASE)
#define	KDM_TO_PHYS(x)		((__psunsigned_t)(x) - K0BASE)
#define	KDM_TO_LOWPHYS(x)	(KDM_TO_PHYS(x))
#define	K0_TO_K1(x)		(x)			/* aliased (see NOTE) */
#define	K1_TO_K0(x)		(x)
#define	PHYS_TO_COMPATK0(x)	((__psunsigned_t)(x) + COMPAT_K0BASE)
#define	PHYS_TO_COMPATK1(x)	((__psunsigned_t)(x) + COMPAT_K1BASE)
#define	COMPAT_TO_PHYS(x)	((__psunsigned_t)(x) - COMPAT_K0BASE)
#define	XKPHYS_TO_PHYS(x)	((__psunsigned_t)(x) - K0BASE)

#define	SEXT_K0BASE		K0BASE
#define	SEXT_K1BASE		K1BASE
#define	PTR_EXT(x)		((__psunsigned_t)(x))

/* region tests */
#define	IS_KSEGDM(x)		((__psunsigned_t)(x) >= K0BASE && \
				 (__psunsigned_t)(x) < K0BASE + K0SIZE)
#define	IS_KSEG0(x)		IS_KSEGDM(x)
#define	IS_KSEG1(x)		(0)			/* no uncached seg   */
#define	IS_KSEG2(x)		((__psunsigned_t)(x) >= K2BASE && \
				 (__psunsigned_t)(x) < K2BASE + K2SIZE)
#define	IS_KPTESEG(x)		((__psunsigned_t)(x) >= 0xffffff0000000000UL && \
				 (__psunsigned_t)(x) < 0xffffff8000000000UL)
#define	IS_KPHYS(x)		IS_KSEGDM(x)
#define	IS_XKPHYS(x)		IS_KSEGDM(x)
#define	IS_KUREGION(x)		((__psunsigned_t)(x) < KUSIZE)
#define	IS_KUSEG(x)		IS_KUREGION(x)
#define	IS_KUSEG32(x)		((__psunsigned_t)(x) < KUSIZE_32)
#define	IS_KUSEG64(x)		IS_KUREGION(x)
#define	IS_COMPATK0(x)		((__psunsigned_t)(x) >= COMPAT_K0BASE)
#define	IS_COMPATK1(x)		((__psunsigned_t)(x) >= COMPAT_K1BASE)
#define	IS_COMPAT_PHYS(x)	((__psunsigned_t)(x) < COMPAT_KSIZE)

#define	K1MASK(x)		((__psunsigned_t)(x))
#define	KREGION_MASK		0xffff000000000000UL

/*
 * MIPS cache-attribute encodings (kseg1 attr field).  x86-64 expresses
 * caching via page bits, not address bits, so these are inert placeholders
 * kept only so headers that reference the names still compile.
 */
#define	CACHE_ALGO(x)		(0)
#define	CALGO_MASK		0
#define	CALGO_SHFT		0
#define	K1ATTR_MASK		0
#define	K1ATTR_SHFT		0
#define	UATTR_MASK		0
#define	UATTR_SHFT		0
#define	UNCACHED_ATTR		0

/* x86-64 PTE format + KPTE window (absent from immu.h with no MIPS board) */
#include <sys/x86_64_pte.h>

#endif /* __SYS_MIPS_ADDRSPACE_H */
