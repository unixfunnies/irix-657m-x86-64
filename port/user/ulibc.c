/*
 * port/user/ulibc.c — implementation of the port's tiny user libc.
 * All kernel interaction is through the int 0x80 gate.
 */

#include "ulibc.h"
#include <stdarg.h>

static long
syscall3(long n, long a, long b, long c)
{
	long ret;

	__asm__ __volatile__(
	    "int $0x80"
	    : "=a"(ret)
	    : "a"(n), "D"(a), "S"(b), "d"(c)
	    : "memory");
	return ret;
}

ssize_t
write(int fd, const void *buf, size_t len)
{
	return syscall3(SYS_write, fd, (long)buf, len);
}

ssize_t
read(int fd, void *buf, size_t len)
{
	return syscall3(SYS_read, fd, (long)buf, len);
}

int
open(const char *path, int flags)
{
	return (int)syscall3(SYS_open, (long)path, flags, 0);
}

int
close(int fd)
{
	return (int)syscall3(SYS_close, fd, 0, 0);
}

off_t
lseek(int fd, off_t off, int whence)
{
	return syscall3(SYS_lseek, fd, off, whence);
}

int
fstat(int fd, struct pstat *st)
{
	return (int)syscall3(SYS_fstat, fd, (long)st, 0);
}

int
getpid(void)
{
	return (int)syscall3(SYS_getpid, 0, 0, 0);
}

void
_exit(int status)
{
	syscall3(SYS_exit, status, 0, 0);
	for (;;)
		;
}

size_t
strlen(const char *s)
{
	const char *p = s;

	while (*p)
		p++;
	return (size_t)(p - s);
}

void
puts(const char *s)
{
	write(1, s, strlen(s));
}

/* minimal printf: %s %d %u %c %x %% -> fd 1 */
static void
putint(unsigned long v, int base, int sgn)
{
	char buf[24];
	int i = 0;
	static const char d[] = "0123456789abcdef";

	if (sgn && (long)v < 0) {
		write(1, "-", 1);
		v = (unsigned long)(-(long)v);
	}
	do {
		buf[i++] = d[v % base];
		v /= base;
	} while (v);
	while (i--)
		write(1, &buf[i], 1);
}

void
printf(const char *fmt, ...)
{
	va_list ap;
	const char *p;

	va_start(ap, fmt);
	for (p = fmt; *p; p++) {
		if (*p != '%') {
			write(1, p, 1);
			continue;
		}
		switch (*++p) {
		case 's': puts(va_arg(ap, const char *)); break;
		case 'd': putint((unsigned long)(long)va_arg(ap, int), 10, 1); break;
		case 'u': putint(va_arg(ap, unsigned int), 10, 0); break;
		case 'x': putint(va_arg(ap, unsigned int), 16, 0); break;
		case 'c': { char c = (char)va_arg(ap, int); write(1, &c, 1); break; }
		case '%': write(1, "%", 1); break;
		default:  write(1, p, 1); break;
		}
	}
	va_end(ap);
}
