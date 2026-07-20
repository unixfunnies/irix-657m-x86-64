/*
 * port/fs/exec_elf.c
 *
 * Minimal ELF64 loader for the x86-64 port (M7).  This is the "read a
 * binary from a vnode and lay it out in a fresh address space" core of
 * exec — the step M5 skipped by hard-coding icode.  It reads the program
 * through the REAL VFS/vnode path (VFS_ROOT -> VOP_LOOKUP -> VOP_READ,
 * dispatched via the behavior chain into memfs's ops), parses the ELF
 * header + program headers, and maps each PT_LOAD segment into the user
 * PML4 via the ml pmap.  The caller (usermode.c) then enters ring 3 at
 * the ELF entry point.
 *
 * IRIX's full exec (os/exec.c: argv/envp, stack setup, vproc, as_*) is
 * grafted on later with the real process model; this is the MD/VFS slice.
 *
 * Compiled with the SGI header environment so VFS/vnode/uio layouts match.
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

/* ml-layer primitives (freestanding side); declared by hand to avoid
 * pulling the ml header into the SGI compile environment. */
extern unsigned long	pmap_alloc_user_page(unsigned long pml4,
			    unsigned long va, int writable);
extern void		*pmap_phys_to_kv(unsigned long pa);

extern vnode_t		*rootdir;
extern struct vfs	*rootvfs;

/* --- minimal ELF64 types (avoid depending on sys/elf.h specifics) --- */
typedef struct {
	unsigned char	e_ident[16];
	unsigned short	e_type;
	unsigned short	e_machine;
	unsigned int	e_version;
	unsigned long	e_entry;
	unsigned long	e_phoff;
	unsigned long	e_shoff;
	unsigned int	e_flags;
	unsigned short	e_ehsize;
	unsigned short	e_phentsize;
	unsigned short	e_phnum;
	unsigned short	e_shentsize;
	unsigned short	e_shnum;
	unsigned short	e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	unsigned int	p_type;
	unsigned int	p_flags;
	unsigned long	p_offset;
	unsigned long	p_vaddr;
	unsigned long	p_paddr;
	unsigned long	p_filesz;
	unsigned long	p_memsz;
	unsigned long	p_align;
} Elf64_Phdr;

#define PT_LOAD		1
#define PF_W		2
#define EM_X86_64	62
#define ET_EXEC		2
#define PAGE		4096UL

/* read len bytes at file offset off from vp into a kernel buffer */
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
		return error;
	return (int)(len - uio.uio_resid);	/* bytes actually read */
}

/*
 * Load one PT_LOAD segment: map its pages into the user AS and read the
 * file portion in, zeroing the bss tail.  Contents are written through
 * each page's kernel (direct-map) alias, so read-only user segments load
 * fine.
 */
static int
load_segment(vnode_t *vp, unsigned long upml4, Elf64_Phdr *ph)
{
	unsigned long vstart = ph->p_vaddr;
	unsigned long vend = ph->p_vaddr + ph->p_memsz;
	unsigned long pstart = vstart & ~(PAGE - 1);
	unsigned long va;
	int writable = (ph->p_flags & PF_W) != 0;

	for (va = pstart; va < vend; va += PAGE) {
		unsigned long pa = pmap_alloc_user_page(upml4, va, writable);
		char *kva = pmap_phys_to_kv(pa);	/* zeroed page */
		unsigned long pg_lo = va, pg_hi = va + PAGE;
		unsigned long copy_lo, copy_hi;
		unsigned long fstart = vstart;
		unsigned long fend = vstart + ph->p_filesz;

		/* overlap of this page with the file-backed part */
		copy_lo = pg_lo > fstart ? pg_lo : fstart;
		copy_hi = pg_hi < fend ? pg_hi : fend;
		if (copy_hi > copy_lo) {
			off_t foff = ph->p_offset + (copy_lo - vstart);
			int n = vn_pread(vp, kva + (copy_lo - pg_lo),
			    copy_hi - copy_lo, foff);
			if (n < 0)
				return EIO;
		}
	}
	return 0;
}

/*
 * elf_load_init: look up "init" in the root fs, parse it, and map it into
 * the given user address space.  Returns 0 and *entry_out on success.
 */
int
elf_load_init(unsigned long upml4, unsigned long *entry_out)
{
	vnode_t *rootvp, *filevp;
	Elf64_Ehdr eh;
	Elf64_Phdr ph;
	int error, i, n;

	VFS_ROOT(rootvfs, &rootvp, error);
	if (error) {
		cmn_err(CE_CONT, "exec: VFS_ROOT failed (%d)\n", error);
		return error;
	}

	VOP_LOOKUP(rootvp, "init", &filevp, NULL, 0, NULL, NULL, error);
	if (error) {
		cmn_err(CE_CONT, "exec: lookup /init failed (%d)\n", error);
		return error;
	}
	cmn_err(CE_CONT, "exec: /init found, reading ELF via VOP_READ\n");

	n = vn_pread(filevp, &eh, sizeof(eh), 0);
	if (n < (int)sizeof(eh))
		return EIO;
	if (eh.e_ident[0] != 0x7f || eh.e_ident[1] != 'E' ||
	    eh.e_ident[2] != 'L' || eh.e_ident[3] != 'F' ||
	    eh.e_ident[4] != 2 /* ELFCLASS64 */) {
		cmn_err(CE_CONT, "exec: /init is not an ELF64 file\n");
		return ENOEXEC;
	}
	if (eh.e_machine != EM_X86_64 || eh.e_type != ET_EXEC) {
		cmn_err(CE_CONT, "exec: /init wrong machine/type\n");
		return ENOEXEC;
	}

	for (i = 0; i < eh.e_phnum; i++) {
		n = vn_pread(filevp, &ph, sizeof(ph),
		    eh.e_phoff + (off_t)i * eh.e_phentsize);
		if (n < (int)sizeof(ph))
			return EIO;
		if (ph.p_type != PT_LOAD)
			continue;
		cmn_err(CE_CONT,
		    "exec: PT_LOAD vaddr=0x%lx filesz=%lu memsz=%lu %s\n",
		    ph.p_vaddr, (unsigned long)ph.p_filesz,
		    (unsigned long)ph.p_memsz,
		    (ph.p_flags & PF_W) ? "rw-" : "r-x");
		if ((error = load_segment(filevp, upml4, &ph)) != 0)
			return error;
	}

	*entry_out = eh.e_entry;
	cmn_err(CE_CONT, "exec: /init loaded, entry 0x%lx\n", eh.e_entry);
	return 0;
}
