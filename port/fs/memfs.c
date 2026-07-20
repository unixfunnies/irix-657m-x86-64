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
 * File contents come later (M6+/M7): once exec/open paths exist, memfs
 * grows real directory entries and vnode ops, or gives way to efs on a
 * ramdisk.  For now the root exists but is empty; nothing reads it before
 * the process/exec milestone.
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
#include <sys/fs_subr.h>
#include <ksys/behavior.h>

#define MEMFS_DEV	makedev(0, 1)	/* synthetic root device */
#define MEMFS_BSIZE	4096

extern vnodeops_t	memfs_vnodeops;
extern vfsops_t		memfs_vfsops;

/* the single root mount + its behavior descriptors (only one memfs) */
static struct memfs_mount {
	bhv_desc_t	m_vfsbhv;	/* behavior on the vfs chain	*/
	bhv_desc_t	m_vnbhv;	/* behavior on the root vnode	*/
	vnode_t		m_rootvp;	/* the root directory vnode	*/
} memfs;

/* ---- vfs ops ---- */

static int
memfs_rootinit(struct vfs *vfsp)
{
	vnode_t *rvp = &memfs.m_rootvp;

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
	bhv_desc_init(&memfs.m_vnbhv, &memfs, rvp, &memfs_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(rvp), &memfs.m_vnbhv);

	rootdir = rvp;
	rootdev = MEMFS_DEV;
	strcpy(rootfstype, "memfs");

	cmn_err(CE_CONT, "memfs: synthetic root mounted\n");
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

/*
 * Root vnode ops.  Left zero (all NULL) apart from the behavior-chain
 * position: on the M6 boot path nothing invokes a VOP on the root before
 * the process/exec milestone, so no op is exercised yet.  Real ops
 * (lookup/getattr/open/read...) arrive with the file layer.
 */
vnodeops_t memfs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
};
