
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */


#ifndef __CFFS_H__
#define __CFFS_H__

#ifndef KERNEL
#include <exos/conf.h>

#include "cffs_general.h"
#include "cffs_dinode.h"
#include "fd/proc.h"
#include <exos/locks.h>
#include <string.h>
#include <errno.h>

#ifdef JJ
#define RETURNCRITICAL return
#define ENTERCRITICAL
#define EXITCRITICAL

#else /* JJ */
#include "exos/process.h"
#include "exos/critical.h"
#define RETURNCRITICAL(x) exos_lock_release(&global_ftable->cffs_lock);return(x);
#define ENTERCRITICAL   exos_lock_get_nb(&global_ftable->cffs_lock);
#define EXITCRITICAL    exos_lock_release(&global_ftable->cffs_lock);
#endif /* JJ */

#include <ubb/lib/bit.h>

#include <stdio.h>		/* for verifyWrite */

#ifdef CFFSD 
#include "exos/cffs_prot.h"
#endif

#endif /* KERNEL */

/* ioctls supported by CFFS */

#define CFFS_IOCTL_PRINTDIRINFO		0xCFF00001

extern int cffs_softupdates;
extern int cffs_usegrouping;
extern int cffs_embedinodes;

#define CFFS_BUFCACHE_SIZE	(1024 * BLOCK_SIZE)

/* file system structure */

#define CFFS_FSNAME	"This is an exokernel CFFS file system"
#define CFFS_SUPERBLKNO	100

/*
 * This structure has to be padded as it is so that the rootDinode
 * and extradirDinode come at 128 and 256 bytes from the start, respectively.
 * This is required because the DInodeNum's of these two structs are set
 * so that their disk block address falls on the superblock.
 *
 */

typedef struct superblock {
   char		fsname[64];
   u_int32_t	fsdev;
   u_int32_t	rootDInodeNum;
   off_t	size;
   int		dirty;
   u_int32_t	numblocks;
   u_int32_t	numalloced;
   u_int32_t    quota;
   u_int32_t	allocMap;
   u_int32_t    blk; 
   char	space1[(128 - (7*sizeof(u_int32_t)) - sizeof(off_t) - sizeof(int) - 64)];
   dinode_t     rootDinode;
   dinode_t	extradirDinode;
#define CFFS_MAX_XNTYPES 16
   u_int32_t	xntypes[CFFS_MAX_XNTYPES];
   char	space2[(BLOCK_SIZE - 128 - (2*sizeof(dinode_t)) - (CFFS_MAX_XNTYPES*sizeof(u_int32_t)))];
} cffs_t;

#ifndef CFFS_PROTECTED
#define cffs_superblock_setRootDInodeNum(superblock,val) \
		((cffs_t*)superblock->buffer)->rootDInodeNum = (val)
#define cffs_superblock_setDirty(superblock,val) \
		((cffs_t*)superblock->buffer)->dirty = (val)
#define cffs_superblock_setFSname(superblock,val) \
		strcpy(((cffs_t*)superblock->buffer)->fsname, (val))
#define cffs_superblock_setFSdev(superblock,val) \
		((cffs_t*)superblock->buffer)->fsdev = (val)
#define cffs_superblock_setAllocMap(superblock,val) \
		((cffs_t*)superblock->buffer)->allocMap = (val)
#define cffs_superblock_setNumblocks(superblock,val) \
		((cffs_t*)superblock->buffer)->numblocks = (val)
#define cffs_superblock_setSize(superblock,val) \
		((cffs_t*)superblock->buffer)->size = (val)
#define cffs_superblock_setNumalloced(superblock,val) \
		((cffs_t*)superblock->buffer)->numalloced = (val)
#define cffs_superblock_setXNtype(superblock,typeno,val) \
		((cffs_t*)superblock->buffer)->xntypes[(typeno)] = (val)
#define cffs_superblock_setBlk(superblock,val) \
                ((cffs_t*)superblock->buffer)->blk = (val)
#define cffs_superblock_setQuota(superblock,val) \
                ((cffs_t*)superblock->buffer)->quota = (val)
#else
#ifdef CFFSD
#define cffs_superblock_setRootDInodeNum(superblock,val) \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETROOTDINODENUM, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setDirty(superblock,val) \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETDIRTY, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setFSname(superblock,val) \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETFSNAME, ((cffs_t*)(superblock)->buffer), (u_int)(val), 0)
#define cffs_superblock_delete(superblock) \
        cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_DELETE, ((cffs_t*)(superblock)->buffer), 0, 0);
#define cffs_superblock_setFSdev(superblock,val) \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETFSDEV, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setAllocMap(superblock,val) \
        cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETALLOCMAP, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setNumblocks(superblock,val) \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETNUMBLOCKS, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setSize(superblock,val) \
	assert ((val) <= (u_int) 0xFFFFFFFF); \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETSIZE, ((cffs_t*)(superblock)->buffer), (u_int)(val), 0)
#define cffs_superblock_setNumalloced(superblock,val) \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETNUMALLOCED, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setXNtype(superblock,typeno,val) \
	cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETXNTYPE, ((cffs_t*)(superblock)->buffer), (typeno), (val))
#define cffs_superblock_setBlk(superblock,val)\
        cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETBLK, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setQuota(superblock,val) \
        cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_SETQUOTA, ((cffs_t*)(superblock)->buffer), (val), 0)
#else /* CFFSD */
#define cffs_superblock_setRootDInodeNum(superblock,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETROOTDINODENUM, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setDirty(superblock,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETDIRTY, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setFSname(superblock,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETFSNAME, ((cffs_t*)(superblock)->buffer), (u_int)(val), 0)
#define cffs_superblock_delete(superblock) \
        assert (0);
#define cffs_superblock_setFSdev(superblock,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETFSDEV, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setAllocMap(superblock,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETALLOCMAP, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setNumblocks(superblock,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETNUMBLOCKS, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setSize(superblock,val) \
	assert ((val) <= (u_int) 0xFFFFFFFF); \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETSIZE, ((cffs_t*)(superblock)->buffer), (u_int)(val), 0)
#define cffs_superblock_setNumalloced(superblock,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETNUMALLOCED, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setXNtype(superblock,typeno,val) \
	sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETXNTYPE, ((cffs_t*)(superblock)->buffer), (typeno), (val))
#define cffs_superblock_setBlk(superblock,val)\
        sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETBLK, ((cffs_t*)(superblock)->buffer), (val), 0)
#define cffs_superblock_setQuota(superblock,val) \
        sys_fsupdate_superblock(CFFS_SUPERBLOCK_SETQUOTA, ((cffs_t*)(superblock)->buffer), (val), 0)
#endif /* CFFSD */
#endif /* CFFS_PROTECTED */

#ifndef KERNEL
extern Bit_T  free_map;
#endif

/* syscalls.c */

int cffs_initFS (char *devname, off_t size);
int cffs_mountFS (int devno);
int cffs_unmountFS (u_int dev);
int cffs_fsck (u_int devno, u_int sb, int force, int nukeAllocMap);
int cffs_printstats ();
int cffs_flush (u_int dev);

/* UNIX-like file permission checks (for now) */

#define verifyReadPermission(mode, uid, owner, gid, group)		\
	((mode & S_IROTH) || ((mode & S_IRUSR) && (uid == owner)) ||	\
 	 ((mode & S_IRGRP) && ((gid == group) || __is_in_group (group))))

#define verifyWritePermission(mode, uid, owner, gid, group)		\
	((mode & S_IWOTH) || ((mode & S_IWUSR) && (uid == owner)) ||	\
 	 ((mode & S_IWGRP) && ((gid == group) || __is_in_group (group))))

#define verifySearchOrExecutePermission(mode, uid, owner, gid, group)  \
	((mode & S_IXOTH) || ((mode & S_IXUSR) && (uid == owner)) ||	\
 	 ((mode & S_IXGRP) && ((gid == group) || __is_in_group (group))))

#endif  /* __CFFS_H__ */

