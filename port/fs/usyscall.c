/*
 * port/fs/usyscall.c
 *
 * File-backed system calls for the x86-64 port (M8): open/read/close/
 * lseek/fstat, implemented against the real VFS/vnode layer (the same
 * VFS_ROOT/VOP_LOOKUP/VOP_READ/VOP_GETATTR path the ELF loader uses).
 * usermode.c's int-0x80 dispatcher routes the file syscalls here; the
 * ml layer provides copyin/copyout so all user-memory access is checked.
 *
 * A single flat descriptor table (no per-process state yet) — sufficient
 * for the one M8 user process.  Real per-process fd tables arrive with
 * the process model (M9).  Compiled in the SGI header environment.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/pathname.h>

/* ml layer */
extern int	uva_copyin(unsigned long uaddr, void *kdst, unsigned long len);
extern int	uva_copyout(const void *ksrc, unsigned long uaddr, unsigned long len);
extern int	uva_copyinstr(unsigned long uaddr, char *kdst, unsigned long max);

extern struct vfs	*rootvfs;

#define UFD_BASE	3			/* 0,1,2 = stdio (handled in ml) */
#define MAXUFD		16

static struct ufile {
	vnode_t	*vp;
	off_t	off;
	int	inuse;
} ufd[MAXUFD];

/* the port's user-visible stat, mirrored in the libc (port/user) */
struct pstat {
	long		st_size;
	unsigned int	st_mode;
	unsigned int	st_ino;
};

static struct ufile *
fd_to_file(int fd)
{
	int i = fd - UFD_BASE;

	if (i < 0 || i >= MAXUFD || !ufd[i].inuse)
		return 0;
	return &ufd[i];
}

/* read len bytes at off from vp into kbuf; returns bytes read or <0 */
static int
vn_pread(vnode_t *vp, void *kbuf, size_t len, off_t off)
{
	struct iovec iov;
	struct uio uio;
	int error;

	iov.iov_base = kbuf;
	iov.iov_len = len;
	bzero(&uio, sizeof(uio));
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = off;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = len;
	uio.uio_limit = 0x7fffffffffffffffL;

	VOP_READ(vp, &uio, 0, NULL, NULL, error);
	if (error)
		return -1;
	return (int)(len - uio.uio_resid);
}

long
usys_open(unsigned long upath, int flags)
{
	char path[128], *name;
	vnode_t *rootvp, *vp;
	int error, i;

	if (uva_copyinstr(upath, path, sizeof(path)) < 0)
		return -1;

	VFS_ROOT(rootvfs, &rootvp, error);
	if (error)
		return -1;

	name = path;
	while (*name == '/')			/* memfs is flat: strip slashes */
		name++;

	VOP_LOOKUP(rootvp, name, &vp, NULL, 0, NULL, NULL, error);
	if (error)
		return -1;

	for (i = 0; i < MAXUFD; i++)
		if (!ufd[i].inuse)
			break;
	if (i == MAXUFD)
		return -1;

	ufd[i].vp = vp;
	ufd[i].off = 0;
	ufd[i].inuse = 1;
	return UFD_BASE + i;
}

long
usys_read(int fd, unsigned long ubuf, unsigned long len)
{
	struct ufile *f = fd_to_file(fd);
	char kbuf[512];
	unsigned long done = 0;

	if (f == 0)
		return -1;
	while (done < len) {
		unsigned long chunk = len - done;
		int got;

		if (chunk > sizeof(kbuf))
			chunk = sizeof(kbuf);
		got = vn_pread(f->vp, kbuf, chunk, f->off);
		if (got < 0)
			return -1;
		if (got == 0)
			break;			/* EOF */
		if (uva_copyout(kbuf, ubuf + done, got) != 0)
			return -1;
		f->off += got;
		done += got;
		if ((unsigned long)got < chunk)
			break;			/* short read == EOF */
	}
	return (long)done;
}

long
usys_close(int fd)
{
	struct ufile *f = fd_to_file(fd);

	if (f == 0)
		return -1;
	f->inuse = 0;
	f->vp = 0;
	return 0;
}

long
usys_lseek(int fd, long off, int whence)
{
	struct ufile *f = fd_to_file(fd);

	if (f == 0)
		return -1;
	switch (whence) {
	case 0:	f->off = off; break;			/* SEEK_SET */
	case 1:	f->off += off; break;			/* SEEK_CUR */
	default: return -1;
	}
	return (long)f->off;
}

long
usys_fstat(int fd, unsigned long ubuf)
{
	struct ufile *f = fd_to_file(fd);
	struct pstat ps;
	struct vattr va;
	int error;

	if (f == 0)
		return -1;
	bzero(&va, sizeof(va));
	VOP_GETATTR(f->vp, &va, 0, NULL, error);
	if (error)
		return -1;
	ps.st_size = (long)va.va_size;
	ps.st_mode = (unsigned int)va.va_mode;
	ps.st_ino = (unsigned int)f->vp->v_number;
	if (uva_copyout(&ps, ubuf, sizeof(ps)) != 0)
		return -1;
	return 0;
}
