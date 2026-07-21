/*
 * port/user/ulibc.h — the port's tiny x86-64 user-space C library.
 *
 * Just enough libc for a ring-3 program to reach the M8 kernel syscall
 * surface (int 0x80): the syscall wrappers, a minimal printf/puts, and a
 * couple of string helpers.  The eventual real userland links IRIX's own
 * libc; this is the bootstrap analog of the kernel's port/stubs libkern.
 */

#ifndef __ULIBC_H__
#define __ULIBC_H__

typedef unsigned long	size_t;
typedef long		ssize_t;
typedef long		off_t;

/* syscall numbers — must match usermode.c's dispatcher */
#define SYS_write	1
#define SYS_exit	2
#define SYS_read	3
#define SYS_open	4
#define SYS_close	5
#define SYS_lseek	6
#define SYS_fstat	7
#define SYS_getpid	9

/* the port's user-visible stat, mirrored from usyscall.c */
struct pstat {
	long		st_size;
	unsigned int	st_mode;
	unsigned int	st_ino;
};

ssize_t	write(int fd, const void *buf, size_t len);
ssize_t	read(int fd, void *buf, size_t len);
int	open(const char *path, int flags);
int	close(int fd);
off_t	lseek(int fd, off_t off, int whence);
int	fstat(int fd, struct pstat *st);
int	getpid(void);
void	_exit(int status) __attribute__((noreturn));

size_t	strlen(const char *s);
void	puts(const char *s);
void	printf(const char *fmt, ...);

#endif /* __ULIBC_H__ */
