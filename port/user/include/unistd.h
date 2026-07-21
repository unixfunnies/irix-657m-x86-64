/* port/user minimal <unistd.h> — the raw syscall surface */
#ifndef _UNISTD_H
#define _UNISTD_H
#include <sys/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1

ssize_t	write(int fd, const void *buf, size_t len);
ssize_t	read(int fd, void *buf, size_t len);
int	open(const char *path, int flags);
int	close(int fd);
off_t	lseek(int fd, off_t off, int whence);
int	getpid(void);

struct pstat { long st_size; unsigned int st_mode; unsigned int st_ino; };
int	fstat(int fd, struct pstat *st);

#endif
