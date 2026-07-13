/*
 * port/include/values.h
 *
 * Minimal SysV <values.h> for the x86-64 port.  On real IRIX this came
 * from /usr/include (not present in the kernel source tree); kernel code
 * uses only the classic integer-limit names.  LP64 values.
 */

#ifndef __VALUES_H__
#define __VALUES_H__

#define BITSPERBYTE	8
#define BITS(type)	(BITSPERBYTE * (int)sizeof(type))

#define HIBITS		((short)(1 << (BITS(short) - 1)))
#define HIBITI		(1U << (BITS(int) - 1))
#define HIBITL		(1UL << (BITS(long) - 1))

#define MAXSHORT	((short)~HIBITS)
#define MAXINT		((int)(~HIBITI))
#define MAXLONG		((long)(~HIBITL))

#endif /* __VALUES_H__ */
