/* port/user minimal <stdio.h> — unbuffered, straight to write() */
#ifndef _STDIO_H
#define _STDIO_H
#include <sys/types.h>

#define EOF	(-1)

typedef struct __FILE { int fd; } FILE;
extern FILE *stdout;
extern FILE *stderr;

int	putchar(int c);
int	puts(const char *s);
int	fputs(const char *s, FILE *fp);
int	fputc(int c, FILE *fp);
int	printf(const char *fmt, ...);
int	fprintf(FILE *fp, const char *fmt, ...);
int	fclose(FILE *fp);

#endif
