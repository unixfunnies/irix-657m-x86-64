/*
 * port/include/mipspro_compat.h
 *
 * MIPSpro-compiler compatibility shims for building original IRIX
 * sources with Clang on x86-64.  Force-included (-include) into
 * every SGI translation unit.
 *
 * The port compiles SGI code with the preprocessor masquerading as
 * a little-endian MIPS n64 compiler (_MIPSEL, _MIPS_SZLONG=64...):
 * x86-64 is LP64 little-endian, exactly the shape of a hypothetical
 * MIPSEL64, so the original headers' type and byte-order decisions
 * come out correct.  What the masquerade cannot cover — MIPSpro
 * intrinsics and pragmas — is shimmed here.
 */

#ifndef __MIPSPRO_COMPAT_H__
#define __MIPSPRO_COMPAT_H__

/* MIPSpro intrinsic: address the current function will return to */
#define __return_address	__builtin_return_address(0)

/* MIPSpro memory-ordering intrinsic (MIPS 'sync' instruction) */
#define __synchronize()		__sync_synchronize()

/*
 * sys/sbd.h only defines the TLB-size constants under a MIPS CPU select
 * (R4000/R10000), which the port deliberately never sets.  The constant
 * only sizes MIPS TLB-dump debug structures (sys/tlbdump.h); pin a value
 * so those structs compile.  x86-64 has no software-visible TLB to dump.
 */
#define NTLBENTRIES		64

#endif /* __MIPSPRO_COMPAT_H__ */
