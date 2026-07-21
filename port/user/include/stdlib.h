/* port/user minimal <stdlib.h> */
#ifndef _STDLIB_H
#define _STDLIB_H
#ifndef NULL
#define NULL ((void *)0)
#endif
void	exit(int status) __attribute__((noreturn));
char	*getenv(const char *name);
int	atoi(const char *s);
#endif
