/*
 * port/fs/memfs.c
 *
 * memfs — a minimal in-memory root filesystem for the x86-64 port.
 *
 * IRIX's real root filesystems (efs, xfs, nfs) mount a block device or
 * network share through the buffer cache / device layer, none of which
 * exists yet on this port.  memfs instead satisfies just enough of the
 * VFS contract for the REAL irix/kern/os/vfs.c:vfs_mountroot() to run to
 * completion: it registers in vfssw[], provides a vfsops whose rootinit
 * attaches a behavior to the root vfs and builds a single root directory
 * vnode, and sets rootdir/rootdev/rootfstype.  This carries main() past
 * the "Root on device" boundary it currently panics at.
 *
 * As of M7 memfs is a genuinely readable (if tiny) filesystem: it serves
 * one regular file, "init" (a real ELF64 executable), through real vnode
 * ops.  VOP_LOOKUP on the root returns the file vnode; VOP_READ streams
 * its bytes via uiomove; VOP_GETATTR reports type/size.  The port ELF
 * loader (exec_elf.c) reads the binary through exactly these ops -- the
 * real IRIX VFS dispatch path (VN_BHV -> bd_ops) -- to exec it in ring 3.
 *
 * Compiled with the SGI header environment (scripts/tryc.sh) so every
 * struct layout matches the kernel it links against.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <sys/fs_subr.h>
#include <ksys/behavior.h>
#include "init_elf.h"			/* the "init" ELF file contents	*/

#define MEMFS_DEV	makedev(0, 1)	/* synthetic root device */
#define MEMFS_BSIZE	4096
#define MEMFS_FILENAME	"init"

extern vnodeops_t	memfs_vnodeops;
extern vfsops_t		memfs_vfsops;

/*
 * memfs holds one directory (root) and one file ("init").  Each vnode
 * carries a behavior descriptor bound to memfs_vnodeops; the file's bytes
 * are the embedded init_elf[].  A real memfs would key a table of these.
 */
static struct memfs_mount {
	bhv_desc_t	m_vfsbhv;	/* behavior on the vfs chain	*/
	bhv_desc_t	m_rootbhv;	/* behavior on the root vnode	*/
	bhv_desc_t	m_filebhv;	/* behavior on the file vnode	*/
	vnode_t		m_rootvp;	/* the root directory vnode	*/
	vnode_t		m_filevp;	/* the "init" file vnode	*/
} memfs;

/* ---- vfs ops ---- */

static int
memfs_rootinit(struct vfs *vfsp)
{
	vnode_t *rvp = &memfs.m_rootvp;
	vnode_t *fvp = &memfs.m_filevp;

	/* attach memfs behavior/ops to the root vfs behavior chain */
	vfs_insertbhv(vfsp, &memfs.m_vfsbhv, &memfs_vfsops, &memfs);

	vfsp->vfs_flag |= VFS_RDONLY | VFS_NODEV;
	vfsp->vfs_dev = MEMFS_DEV;
	vfsp->vfs_bsize = MEMFS_BSIZE;

	/* build the root directory vnode by hand (no vnode cache yet) */
	bzero(rvp, sizeof(*rvp));
	rvp->v_vfsp = vfsp;
	rvp->v_type = VDIR;
	rvp->v_count = 1;
	rvp->v_number = 1;
	rvp->v_flag = VROOT;
	vn_bhv_head_init(VN_BHV_HEAD(rvp), "memfs");
	bhv_desc_init(&memfs.m_rootbhv, &memfs, rvp, &memfs_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(rvp), &memfs.m_rootbhv);

	/* build the "init" regular-file vnode */
	bzero(fvp, sizeof(*fvp));
	fvp->v_vfsp = vfsp;
	fvp->v_type = VREG;
	fvp->v_count = 1;
	fvp->v_number = 2;
	vn_bhv_head_init(VN_BHV_HEAD(fvp), "memfs");
	bhv_desc_init(&memfs.m_filebhv, &memfs, fvp, &memfs_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(fvp), &memfs.m_filebhv);

	rootdir = rvp;
	rootdev = MEMFS_DEV;
	strcpy(rootfstype, "memfs");

	cmn_err(CE_CONT, "memfs: synthetic root mounted (1 file: %s, %d bytes)\n",
	    MEMFS_FILENAME, (int)sizeof(init_elf));
	return 0;
}

static int
memfs_vfsmountroot(bhv_desc_t *bdp, enum whymountroot why)
{
	return memfs_rootinit(bhvtovfs(bdp));
}

static int
memfs_root(bhv_desc_t *bdp, struct vnode **vpp)
{
	vnode_t *rvp = &memfs.m_rootvp;

	VN_HOLD(rvp);
	*vpp = rvp;
	return 0;
}

static int
memfs_statvfs(bhv_desc_t *bdp, struct statvfs *sp, struct vnode *vp)
{
	bzero(sp, sizeof(*sp));
	sp->f_bsize = MEMFS_BSIZE;
	sp->f_frsize = MEMFS_BSIZE;
	strcpy(sp->f_basetype, "memfs");
	return 0;
}

static int
memfs_sync(bhv_desc_t *bdp, int flags, struct cred *cr)
{
	return 0;
}

vfsops_t memfs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
	(int (*)())fs_nosys,	/* vfs_mount		*/
	memfs_rootinit,		/* vfs_rootinit		*/
	(int (*)())fs_nosys,	/* vfs_mntupdate	*/
	fs_dounmount,		/* vfs_dounmount	*/
	(int (*)())fs_nosys,	/* vfs_unmount		*/
	memfs_root,		/* vfs_root		*/
	memfs_statvfs,		/* vfs_statvfs		*/
	memfs_sync,		/* vfs_sync		*/
	(int (*)())fs_nosys,	/* vfs_vget		*/
	memfs_vfsmountroot,	/* vfs_mountroot	*/
	fs_realvfsops,		/* vfs_realvfsops	*/
	fs_import,		/* vfs_import		*/
	(int (*)())fs_nosys,	/* vfs_quotactl		*/
};

/* ---- vnode ops ---- */

/* VOP_LOOKUP on the root dir: only "init" exists */
static int
memfs_lookup(bhv_desc_t *bdp, char *name, vnode_t **vpp,
    struct pathname *pnp, int flags, vnode_t *rdir, struct cred *cr)
{
	if (strcmp(name, MEMFS_FILENAME) == 0) {
		VN_HOLD(&memfs.m_filevp);
		*vpp = &memfs.m_filevp;
		return 0;
	}
	*vpp = NULL;
	return ENOENT;
}

/* VOP_READ on the "init" file: stream init_elf[] via uiomove */
static int
memfs_read(bhv_desc_t *bdp, struct uio *uiop, int ioflag, struct cred *cr,
    struct flid *fl)
{
	off_t off = uiop->uio_offset;
	ssize_t avail;

	if (off < 0 || off > (off_t)sizeof(init_elf))
		return EINVAL;
	avail = (ssize_t)sizeof(init_elf) - off;
	if (avail <= 0)
		return 0;			/* EOF */
	if (avail > uiop->uio_resid)
		avail = uiop->uio_resid;
	return uiomove((char *)init_elf + off, avail, UIO_READ, uiop);
}

/* VOP_GETATTR: report type and size for root (dir) or file */
static int
memfs_getattr(bhv_desc_t *bdp, struct vattr *vap, int flags, struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);

	vap->va_type = vp->v_type;
	vap->va_mode = (vp->v_type == VDIR) ? 0555 : 0555;
	vap->va_nlink = 1;
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_size = (vp->v_type == VREG) ? sizeof(init_elf) : MEMFS_BSIZE;
	vap->va_blksize = MEMFS_BSIZE;
	return 0;
}

/*
 * memfs vnode ops.  Only the read path the ELF loader needs is real
 * (lookup/read/getattr); the rest stay NULL (never invoked on this path).
 * Designated initializers keep field placement correct against the large
 * vnodeops_t.
 */
vnodeops_t memfs_vnodeops = {
	.vn_position = BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	.vop_lookup  = memfs_lookup,
	.vop_read    = memfs_read,
	.vop_getattr = memfs_getattr,
};
