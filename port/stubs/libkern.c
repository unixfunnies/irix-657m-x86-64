/*
 * port/stubs/libkern.c
 *
 * Freestanding kernel string/memory routines for the x86-64 port.
 * On MIPS these were hand-tuned assembly in irix/kern/ml and libc;
 * plain C is plenty at this stage.  Clang also lowers struct copies
 * and initializers to memcpy/memset, so those must exist.
 */

typedef unsigned long	size_t;

void *
memset(void *dst, int c, size_t n)
{
	char *d = dst;
	while (n--)
		*d++ = (char)c;
	return dst;
}

void *
memcpy(void *dst, const void *src, size_t n)
{
	char *d = dst;
	const char *s = src;
	while (n--)
		*d++ = *s++;
	return dst;
}

void *
memmove(void *dst, const void *src, size_t n)
{
	char *d = dst;
	const char *s = src;

	if (d < s) {
		while (n--)
			*d++ = *s++;
	} else {
		d += n;
		s += n;
		while (n--)
			*--d = *--s;
	}
	return dst;
}

int
memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *pa = a, *pb = b;
	for (; n--; pa++, pb++)
		if (*pa != *pb)
			return *pa - *pb;
	return 0;
}

void
bzero(void *p, size_t n)
{
	memset(p, 0, n);
}

void
bcopy(const void *src, void *dst, size_t n)
{
	memmove(dst, src, n);
}

int
bcmp(const void *a, const void *b, size_t n)
{
	return memcmp(a, b, n);
}

size_t
strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return p - s;
}

char *
strcpy(char *dst, const char *src)
{
	char *d = dst;
	while ((*d++ = *src++) != 0)
		;
	return dst;
}

char *
strncpy(char *dst, const char *src, size_t n)
{
	char *d = dst;
	while (n && (*d++ = *src++) != 0)
		n--;
	while (n--)
		*d++ = 0;
	return dst;
}

char *
strcat(char *dst, const char *src)
{
	strcpy(dst + strlen(dst), src);
	return dst;
}

char *
strncat(char *dst, const char *src, size_t n)
{
	char *d = dst + strlen(dst);
	while (n-- && *src)
		*d++ = *src++;
	*d = 0;
	return dst;
}

int
strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *(const unsigned char *)a - *(const unsigned char *)b;
}

int
strncmp(const char *a, const char *b, size_t n)
{
	while (n && *a && *a == *b)
		a++, b++, n--;
	return n ? *(const unsigned char *)a - *(const unsigned char *)b : 0;
}

char *
strchr(const char *s, int c)
{
	for (; *s; s++)
		if (*s == (char)c)
			return (char *)s;
	return c == 0 ? (char *)s : 0;
}
