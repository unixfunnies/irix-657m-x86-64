/*
 * irix/kern/ml/X86_64/kprintf.c
 *
 * Minimal freestanding kprintf for early boot.  Understands
 * %s %c %d %i %u %x %p %% with optional 'l'/'ll' length modifiers.
 * Will be subsumed by the real IRIX cmn_err() machinery at M3.
 */

#include <stdarg.h>
#include "x86_64.h"

static void
prnum(unsigned long long v, unsigned base, int is_signed)
{
	static const char digits[] = "0123456789abcdef";
	char buf[24];
	int i = 0;

	if (is_signed && (long long)v < 0) {
		serial_putc('-');
		v = (unsigned long long)(-(long long)v);
	}
	do {
		buf[i++] = digits[v % base];
		v /= base;
	} while (v != 0);
	while (i > 0)
		serial_putc(buf[--i]);
}

void
kprintf(const char *fmt, ...)
{
	va_list ap;
	const char *p;

	va_start(ap, fmt);
	for (p = fmt; *p; p++) {
		int lcnt = 0;

		if (*p != '%') {
			serial_putc(*p);
			continue;
		}
		p++;
		while (*p == 'l') {
			lcnt++;
			p++;
		}
		switch (*p) {
		case 's': {
			const char *s = va_arg(ap, const char *);
			serial_puts(s != 0 ? s : "(null)");
			break;
		}
		case 'c':
			serial_putc((char)va_arg(ap, int));
			break;
		case 'd':
		case 'i':
			if (lcnt)
				prnum((unsigned long long)va_arg(ap, long long), 10, 1);
			else
				prnum((unsigned long long)(long long)va_arg(ap, int), 10, 1);
			break;
		case 'u':
			if (lcnt)
				prnum(va_arg(ap, unsigned long long), 10, 0);
			else
				prnum(va_arg(ap, unsigned int), 10, 0);
			break;
		case 'x':
			if (lcnt)
				prnum(va_arg(ap, unsigned long long), 16, 0);
			else
				prnum(va_arg(ap, unsigned int), 16, 0);
			break;
		case 'p':
			serial_puts("0x");
			prnum((unsigned long long)(__u64)va_arg(ap, void *), 16, 0);
			break;
		case '%':
			serial_putc('%');
			break;
		default:
			serial_putc('%');
			serial_putc(*p);
			break;
		}
	}
	va_end(ap);
}
