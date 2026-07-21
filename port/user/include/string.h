/* port/user minimal <string.h> */
#ifndef _STRING_H
#define _STRING_H
#include <sys/types.h>
size_t	strlen(const char *s);
int	strcmp(const char *a, const char *b);
char	*strerror(int errnum);
#endif
