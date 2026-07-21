/*
 * port/user/ulibc.c — the port's tiny x86-64 user C library.
 *
 * Just enough libc for real programs (our init.c and the genuine IRIX
 * eoe/cmd/echo.c) to run in ring 3: the int-0x80 syscall wrappers, an
 * unbuffered stdio, and the handful of stdlib/string functions echo uses.
 * Bootstrap analog of the kernel's port/stubs libkern.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

/* syscall numbers — must match usermode.c's dispatcher */
#define SYS_write	1
#define SYS_exit	2
#define SYS_read	3
#define SYS_open	4
#define SYS_close	5
#define SYS_lseek	6
#define SYS_fstat	7
#define SYS_getpid	9

int	errno;
char	**environ;			/* set by crt0 from the initial stack */

static FILE __stdout = { 1 };
static FILE __stderr = { 2 };
FILE	*stdout = &__stdout;
FILE	*stderr = &__stderr;

static long
syscall3(long n, long a, long b, long c)
{
	long ret;

	__asm__ __volatile__("int $0x80"
	    : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "memory");
	return ret;
}

/* ---- syscalls ---- */

ssize_t write(int fd, const void *buf, size_t len)
	{ return syscall3(SYS_write, fd, (long)buf, len); }
ssize_t read(int fd, void *buf, size_t len)
	{ return syscall3(SYS_read, fd, (long)buf, len); }
int open(const char *path, int flags)
	{ return (int)syscall3(SYS_open, (long)path, flags, 0); }
int close(int fd)
	{ return (int)syscall3(SYS_close, fd, 0, 0); }
off_t lseek(int fd, off_t off, int whence)
	{ return syscall3(SYS_lseek, fd, off, whence); }
int fstat(int fd, struct pstat *st)
	{ return (int)syscall3(SYS_fstat, fd, (long)st, 0); }
int getpid(void)
	{ return (int)syscall3(SYS_getpid, 0, 0, 0); }

void
exit(int status)
{
	syscall3(SYS_exit, status, 0, 0);
	for (;;)
		;
}

/* ---- string / stdlib ---- */

size_t
strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return (size_t)(p - s);
}

int
strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (unsigned char)*a - (unsigned char)*b;
}

char *
strerror(int e)
{
	static char buf[16];
	char *p = buf + sizeof(buf) - 1;
	int v = e;

	*p = '\0';
	if (v == 0) { *--p = '0'; return p; }
	if (v < 0) v = -v;
	while (v) { *--p = '0' + v % 10; v /= 10; }
	if (e < 0) *--p = '-';
	return p;
}

int
atoi(const char *s)
{
	int n = 0, neg = 0;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '-') { neg = 1; s++; }
	else if (*s == '+') s++;
	while (*s >= '0' && *s <= '9')
		n = n * 10 + (*s++ - '0');
	return neg ? -n : n;
}

char *
getenv(const char *name)
{
	size_t nl = strlen(name);
	char **e;

	if (!environ)
		return 0;
	for (e = environ; *e; e++) {
		char *v = *e;
		size_t i;
		for (i = 0; i < nl; i++)
			if (v[i] != name[i])
				break;
		if (i == nl && v[i] == '=')
			return v + nl + 1;
	}
	return 0;
}

/* ---- stdio (unbuffered) ---- */

int
fputc(int c, FILE *fp)
{
	char ch = (char)c;
	write(fp->fd, &ch, 1);
	return (unsigned char)c;
}

int
putchar(int c)
{
	return fputc(c, stdout);
}

int
fputs(const char *s, FILE *fp)
{
	write(fp->fd, s, strlen(s));
	return 0;
}

int
puts(const char *s)
{
	fputs(s, stdout);
	putchar('\n');
	return 0;
}

int
fclose(FILE *fp)
{
	(void)fp;			/* unbuffered: nothing to flush */
	return 0;
}

static void
prnum(FILE *fp, unsigned long v, int base, int sgn)
{
	char buf[24];
	int i = 0;
	static const char d[] = "0123456789abcdef";

	if (sgn && (long)v < 0) { fputc('-', fp); v = (unsigned long)(-(long)v); }
	do { buf[i++] = d[v % base]; v /= base; } while (v);
	while (i--)
		fputc(buf[i], fp);
}

static int
vfprintf_(FILE *fp, const char *fmt, va_list ap)
{
	const char *p;

	for (p = fmt; *p; p++) {
		if (*p != '%') { fputc(*p, fp); continue; }
		switch (*++p) {
		case 's': fputs(va_arg(ap, const char *), fp); break;
		case 'd': prnum(fp, (unsigned long)(long)va_arg(ap, int), 10, 1); break;
		case 'u': prnum(fp, va_arg(ap, unsigned int), 10, 0); break;
		case 'x': prnum(fp, va_arg(ap, unsigned int), 16, 0); break;
		case 'c': fputc(va_arg(ap, int), fp); break;
		case '%': fputc('%', fp); break;
		default:  fputc(*p, fp); break;
		}
	}
	return 0;
}

int
fprintf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf_(fp, fmt, ap);
	va_end(ap);
	return 0;
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf_(stdout, fmt, ap);
	va_end(ap);
	return 0;
}
