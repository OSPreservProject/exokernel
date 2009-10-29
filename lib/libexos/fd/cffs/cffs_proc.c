
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


#include "cffs_buffer.h"
#include "cffs.h"
#include "cffs_inode.h"
#include "cffs_path.h"
#include "cffs_rdwr.h"
#include "cffs_alloc.h"
#include "cffs_embdir.h"
#include "cffs_xntypes.h"

#ifdef CFFSD
#include <exos/bufcache.h>
#include <exos/vm.h>
#include <xok/micropart.h>
#endif

#include <xok/pctr.h>

#include "sugar.h"

#include <ubb/xn.h>
#include <ubb/root-catalogue.h>

#ifdef JJ
#include "virtual-disk.h"
#include <kernel.h>
#include <ubb/lib/demand.h>
#else
#include <xok/sysinfo.h>
#include <exos/ubc.h>
#include <exos/uwk.h>
#include <exos/cap.h>
#include <exos/pager.h>
#include <assert.h>
#define	fatal(a)	demand(0,a)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cffs_proc.h"

#include "cffs_defaultcache.h"

#ifdef JJ
struct poo {
   int cffs_mountdev;
} poo;
struct poo *global_ftable = &poo;
#endif

#if 1
#define DPRINTF(a,b)	if (a < 0) { printf b; }
#else
#define DPRINTF(a,b)
#endif

int cffs_printstuff = 0;

int cffs_softupdates = 0;

Bit_T free_map;

/* unix-like file i/o interface */

static inline mode_t cffs_make_st_mode(int mode,int type) {
    return ((mode & ~S_IFMT) | (type&S_IFMT));
}

/* cffs_proc_getSuperblock

   Take a device number and read the default superblock 
   from it and return a pointer to it.
*/

cffs_t *cffs_proc_getSuperblock (u_int fsdev, u_int superblk, buffer_t **sb_buf) {
  *sb_buf = cffs_get_superblock (fsdev, superblk, 0);
  if (!*sb_buf)
    return NULL;
  return ((cffs_t *)((*sb_buf)->buffer));
}

static void cffs_fdInitDinode (dinode_t *dinode, mode_t mask)
{
   cffs_dinode_setUid (dinode, geteuid());
   cffs_dinode_setGid (dinode, getegid());
   cffs_dinode_setAccTime (dinode, time(NULL));
   cffs_dinode_setModTime (dinode, time(NULL));
   cffs_dinode_setCreTime (dinode, time(NULL));
   cffs_dinode_setMask (dinode, mask);
   cffs_dinode_setOpenCount (dinode, 0);
   cffs_dinode_setGroupstart (dinode, 0);
   cffs_dinode_setGroupsize (dinode, 0);
   cffs_dinode_setLength (dinode, 0);
}

/* create a file system on a disk */

int cffs_initFS (char *devname, off_t size)
{
   dinode_t *dinode;
   db_t sb = CFFS_SUPERBLKNO;
#ifdef XN
   int err;
   xn_cnt_t cnt;
   da_t da;
#else /* XN */
#ifdef CFFS_PROTECTED
   da_t da;
#endif /* CFFS_PROTECTED */
#endif /* XN */
   u_int dev;
   cffs_t *superblk;
   buffer_t *sb_buf;

   /* check size! */
   dev = atoi(devname);

#ifndef JJ
   if ((dev < 0) || (dev >= __sysinfo.si_ndisks)) {
      printf ("cffs_initFS: Invalid device number (%s)\n", devname);
      errno = ENODEV;
      return (-1);
   }

   if (roundup(size, ((off_t)BLOCK_SIZE)) > (__sysinfo.si_disks[dev].d_size * __sysinfo.si_disks[dev].d_bsize)) {
      printf ("cffs_initFS: cannot fit %qd bytes onto disk with %qu %u-byte sectors\n", size, __sysinfo.si_disks[dev].d_size, __sysinfo.si_disks[dev].d_bsize);
      errno = EFBIG;
      return (-1);
   }
#endif /* JJ */

   if (global_ftable->mounted_disks[dev] == 1) {
     printf ("cffs_initFS: Device already mounted\n");
     errno = EBUSY;
     return -1;
   }

#ifdef XN
   if (roundup(size, ((off_t)BLOCK_SIZE)) > ((off_t)(XN_NBLOCKS * XN_BLOCK_SIZE))) {
      printf ("cffs_initFS: cannot fit %qd bytes onto disk with %d %d-byte sectors\n", size, (u_int)XN_NBLOCKS, XN_BLOCK_SIZE);
      errno = EFBIG;
      return (-1);
   }
#endif /* XN */

   cffs_cache_init ();

   ENTERCRITICAL;

#ifdef XN
      /* Install our types. */
   if (specify_cffs_types() < 0) {
      fatal(Failed);
   }
   //printf("CFFS TYPES SPECIFIED\n");

   /* Install cffs root (superblock for now) */
   sb = install_root(devname, (cap_t)CAP_ROOT, cffs_super_t);
   //printf("CFFS SUPERBLOCK INSTALLED\n");

   /* Allocate backing store for superblock. */
   if ((err = sys_xn_bind(sb, -1, (cap_t)CAP_ROOT, XN_ZERO_FILL, 1)) < 0) {
     printf ("cffs_proc: err = %d\n", err);
     fatal(Could not bind);
   }
   //printf("ROOT BOUND (%d)\n", err);

   free_map = Bit_build (XN_NBLOCKS, (char *)&__xn_free_map);
   assert(Bit_length(free_map) == XN_NBLOCKS);
#endif /* XN */

   sb_buf = cffs_get_superblock (dev, sb, BUFFER_WITM);
   superblk = (cffs_t *) sb_buf->buffer;
#ifndef XN
#ifdef CFFS_PROTECTED
#ifdef CFFSD   
   cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_INIT, superblk, 0, 0);
#else
   sys_fsupdate_superblock(CFFS_SUPERBLOCK_INIT, superblk, 0, 0);
#endif /* CFFSD */

#else /* CFFS_PROTECTED */
   bzero (superblk, BLOCK_SIZE);
#endif /* CFFS_PROTECTED */
#endif /* !XN */

   cffs_superblock_setFSname (sb_buf, CFFS_FSNAME);
   cffs_superblock_setFSdev (sb_buf, dev);
   cffs_superblock_setSize (sb_buf, size);
   cffs_superblock_setBlk (sb_buf, sb);

#ifndef XN
   cffs_superblock_setAllocMap (sb_buf, (sb+1));
   cffs_alloc_initDisk (sb_buf, size, (sb+1));
#else /* XN */
   cffs_fill_xntypes (sb_buf);
#endif /* XN */

   /* must be done after the numblocks field has been set */
   cffs_superblock_setQuota (sb_buf, 0);

#if defined(XN) || defined(CFFS_PROTECTED)
   da = da_compose(sb, offsetof(cffs_t, extradirDinode));
#ifdef CFFS_PROTECTED
   dinode = &superblk->extradirDinode;
#ifdef CFFSD 
   {int ret = cffsd_fsupdate_dinode(CFFS_DINODE_INITDINODE, dinode, (u_int)S_IFDIR, (u_int)((sb << 5) + 2), 0); assert(ret == 0);}
#else
   {int ret = sys_fsupdate_dinode(CFFS_DINODE_INITDINODE, dinode, (u_int)S_IFDIR, (u_int)((sb << 5) + 2), 0); assert(ret == 0);}
#endif /* CFFSD */
#else /* CFFS_PROTECTED */
   dinode = cffs_dinode_create(&superblk->extradirDinode, da);
   cffs_dinode_setMemorySanity(dinode, CFFS_DINODE_MEMORYSANITY);
   cffs_dinode_setType(dinode, S_IFDIR);
   cffs_dinode_setLinkCount(dinode, 2);

#endif /* CFFS_PROTECTED */
#else /* XN || CFFS_PROTECTED */
   dinode = &superblk->extradirDinode;
   dinode->dinodeNum = (sb << 5) + 2;
   cffs_dinode_initDInode (dinode, dinode->dinodeNum, S_IFDIR);
#endif /* XN || CFFS_PROTECTED */
   cffs_fdInitDinode (dinode, (S_IRWXU | S_IRWXG | S_IRWXO));

#if defined(XN) || defined(CFFS_PROTECTED)
   da = da_compose(sb, offsetof(cffs_t, rootDinode));
#ifdef CFFS_PROTECTED
   dinode = &superblk->rootDinode;
#ifdef CFFSD
   {int ret = cffsd_fsupdate_dinode(CFFS_DINODE_INITDINODE, dinode, (u_int)S_IFDIR, (u_int)((sb << 5) + 1), (u_int)((sb << 5) + 1)); assert(ret == 0);}
#else /* CFFSD */
   {int ret = sys_fsupdate_dinode(CFFS_DINODE_INITDINODE, dinode, (u_int)S_IFDIR, (u_int)((sb << 5) + 1), (u_int)((sb << 5) + 1)); assert(ret == 0);}   
#endif /* CFFSD */
#else /* CFFS_PROTECTED */
   dinode = cffs_dinode_create(&superblk->rootDinode, da);
   cffs_dinode_setMemorySanity(dinode, CFFS_DINODE_MEMORYSANITY);
   cffs_dinode_setType(dinode, S_IFDIR);
   cffs_dinode_setLinkCount(dinode, 2);
#endif /* CFFS_PROTECTED */
#else /* XN || CFFS_PROTECTED */
   dinode = &superblk->rootDinode;
   dinode->dinodeNum = (sb << 5) + 1;
   cffs_dinode_initDInode (dinode, dinode->dinodeNum, S_IFDIR);
#endif /* XN || CFFS_PROTECTED */
   cffs_fdInitDinode (dinode, (S_IRWXU | S_IRWXG | S_IRWXO));

   cffs_superblock_setRootDInodeNum (sb_buf, dinode->dinodeNum);
   cffs_superblock_setDirty (sb_buf, 0);
/*
printf ("size %d, rootINO %d (%x)\n", cffs->size, cffs->rootDInodeNum, cffs->rootDInodeNum);
*/

#ifndef CFFS_PROTECTED
   cffs_dinode_setDirNum (dinode, dinode->dinodeNum);
#endif

   cffs_buffer_releaseBlock (sb_buf, BUFFER_WRITE);

   /* Shut down the private (per-app) caches in order to initiate */
   /* write-backs of the newly-initialized blocks.                */

   //   _exos_pager_unregister (default_buffer_revoker);
   cffs_cache_shutdown ();

#ifdef XN
   cnt = 0;
   if ((err = sys_xn_writeback(sb, 1, &cnt)) < 0) {
      assert(0);
   }

   wk_waitfor_value (&cnt, -1, 0);

/*
   if ((err = sys_xn_unbind(sb, -1, CAP_ROOT)) < 0) {
      assert(0);
   }
*/
#endif /* XN */

   RETURNCRITICAL (0);
}

/* 
 * Make a new "light weight" filesystem on a disk. This just creates a
 * new superblock that shares allocation maps with an existing
 * superblock.
 * 
 */

int cffs_init_superblock (u_int dev, block_num_t *superblockno) {
   dinode_t *dinode;
   cffs_t *superblk;
   buffer_t *sb_buf;
   buffer_t *default_sb_buf;
   cffs_t *default_superblk;
   int ret;

   /* 
    * Right now, this only works with micropartitions as the protection scheme.
    *
    */

#ifdef XN
   assert (0);
#endif

#ifndef CFFS_PROTECTED
   assert (0);
#endif

   if ((dev < 0) || (dev >= __sysinfo.si_ndisks)) {
     errno = ENODEV;
     return (-1);
   }

   ENTERCRITICAL;

   CFFS_CHECK_SETUP (dev);

   /* 
    * Allocate a block (using the default superblock) for
    * this new superblock.
    *
    */

   default_sb_buf = cffs_get_superblock (dev, CFFS_SUPERBLKNO, 0);
   default_superblk = (cffs_t *)default_sb_buf->buffer;
   while (1) {

     /* 
      * This results in the block being charged to the default superblock
      * which is probably an ok enough thing to do.
      *
      * We have to loop since there is a race between finding a free block
      * and actually allocating it.
      *
      */

     *superblockno = cffs_findFree (default_sb_buf, 0);
     if (cffs_alloc_protWrapper (CFFS_SETPTR_EXTERNALALLOC, default_sb_buf, NULL, 
				 NULL, *superblockno) != -1)
       break;
   }

   /* 
    * Now get this new superblock-to-be that we just allocated.
    *
    */

   sb_buf = cffs_get_superblock (dev, *superblockno, BUFFER_WITM);
   superblk = (cffs_t *) sb_buf->buffer;

   /*
    * Fill in the superblock
    *
    */

   cffsd_fsupdate_superblock(CFFS_SUPERBLOCK_INIT, superblk, 0, 0);
   cffs_superblock_setFSname (sb_buf, CFFS_FSNAME);
   cffs_superblock_setFSdev (sb_buf, dev);
   cffs_superblock_setSize (sb_buf, -1); /* XXX */
   cffs_superblock_setBlk (sb_buf, *superblockno);
   cffs_superblock_setNumalloced (sb_buf, 0);
   cffs_superblock_setNumblocks (sb_buf, default_superblk->numblocks);
   cffs_superblock_setQuota (sb_buf, 0); /* must be done after the numblocks field is set */

   /*
    * All superblocks share the same allocation map. Point to
    * the default superblock's allocation map.
    *
    */

   cffs_superblock_setAllocMap (sb_buf, default_superblk->allocMap);

   /*
    * Build the "extra" directory.
    *
    */

   dinode = &superblk->extradirDinode;
   ret = cffsd_fsupdate_dinode(CFFS_DINODE_INITDINODE, dinode, (u_int)S_IFDIR, 
			       (u_int)((*superblockno << 5) + 2), 0); 
   assert(ret == 0);
   cffs_fdInitDinode (dinode, (S_IRWXU | S_IRWXG | S_IRWXO));

   /* 
    * Build the root directory.
    *
    */

   dinode = &superblk->rootDinode;
   ret = cffsd_fsupdate_dinode(CFFS_DINODE_INITDINODE, dinode, (u_int)S_IFDIR, 
			       (u_int)((*superblockno << 5) + 1), 
			       (u_int)((*superblockno << 5) + 1)); 
   assert(ret == 0);
   cffs_fdInitDinode (dinode, (S_IRWXU | S_IRWXG | S_IRWXO));
   cffs_superblock_setRootDInodeNum (sb_buf, dinode->dinodeNum);

   /* 
    * We're done with the various superblocks.
    * Write out the new one.
    *
    */

   cffs_superblock_setDirty (sb_buf, 0);
   cffs_buffer_releaseBlock (sb_buf, BUFFER_WRITE);
   cffs_buffer_releaseBlock (default_sb_buf, 0);

   RETURNCRITICAL (0);
}

void cffs_exithandler ()
{
DPRINTF (1, ("%d: cffs_exithandler\n", getpid()));

#ifndef JJ
   UAREA.u_in_critical = 0;
#endif
}

/*
 * Enable the buffer and inode caches for this process. Thist must
 * be called before calls to such things as cffs_buffer_getBlock. It
 * may be called an arbitrary number of times. It will simply return
 * on the second and subsequent invocations.
 *
 */

static int already_inited = 0;

void cffs_cache_init () {
   StaticAssert (sizeof(cffs_t) == BLOCK_SIZE);
   StaticAssert (offsetof(cffs_t,rootDinode) == 128);
   StaticAssert (offsetof(cffs_t,extradirDinode) == 256);


   if (already_inited) return;
   already_inited = 1;

   cffs_buffer_initBufferCache (128, CFFS_BUFCACHE_SIZE/BLOCK_SIZE, default_buffer_handleMiss, 
				default_buffer_flushEntry, default_buffer_writeBack,
				default_buffer_writeBack_withlock);
   cffs_inode_initInodeCache ();
   _exos_pager_register (default_buffer_revoker);
}

/*
 * Shutdown caches. May be called any number of times before
 * or after calls to cffs_cache_init ().
 *
 */

void cffs_cache_shutdown () {
  if (!already_inited)
    return;

  already_inited = 0;
  cffs_inode_shutdownInodeCache ();
  cffs_buffer_shutdownBufferCache ();
}
  
/*
 * Initialize any disk protection state. firstmount is set
 * to true when this is called by the mount syscall and is
 * set to false if this is just a "per-process" initialization.
 *
 */

void cffs_protection_init (u_int devno, int firstmount) {
#ifdef XN
   struct root_entry r;
   int err;
   char buf[16];
   db_t sb;
   buffer_t *sb_buf;
   cffs_t *superblk;
#endif
#ifdef CFFSD
   int ret;
   struct bc_entry *bc;
#endif

   if ((devno < 0) || (devno >= __sysinfo.si_ndisks)) {
     assert (0);
   }

#ifdef XN
   sprintf(buf, "%d", devno);

   if (firstmount) {
	/* Install our types. */
      if (specify_cffs_types() < 0) {
         fatal(Failed);
      }
	/* JJJ Fix this char*, int mess */
      if ((err = sys_xn_mount(&r, buf, (cap_t)CAP_ROOT)) < 0) {
         fatal(Could not mount);
      }
      sb = r.db;

#if 0	/* GROK -- new XN approach eliminates this crap */
      /* Allocate backing store for superblock. */
      if ((err = sys_xn_bind(sb, -1, CAP_ROOT, XN_FORCE_READIN, 1)) < 0) {
         fatal(Could not bind);
      }
      printf("Root Bound in mount (%d)\n", err);
#endif /* 0 */
   } else {
      sb = gross_get_superblock_db (buf);
      assert (sb != 0);
   }
   free_map = Bit_build (XN_NBLOCKS, (char *)&__xn_free_map);
   assert(Bit_length(free_map) == XN_NBLOCKS);

   sb_buf = cffs_get_superblock (devno, sb, BUFFER_WITM);
   superblk = (cffs_t *) sb_buf->buffer;

   cffs_pull_xntypes(superblk);

   if (firstmount) {
      db_t freemap;
      size_t f_nbytes;
      sys_xn_info(NULL, NULL, &freemap, &f_nbytes);
      cffs_superblock_setAllocMap(sb_buf, freemap);
      cffs_superblock_setNumblocks(sb_buf, f_nbytes*8);
   }

   cffs_buffer_writeBlock (sb_buf, BUFFER_WRITE);

#endif /* XN */

#ifdef CFFSD

   if (firstmount) {
     /* first read in the micropartition metadata block */

     if (!__bc_lookup (devno, MICROPART_METADATA_PAGE)) {
       ret = _exos_bc_read_and_insert (devno, MICROPART_METADATA_PAGE, 1,
				       NULL);
       if (ret < 0 && ret != -E_EXISTS) {
	 fprintf (stderr, "cffs_mountFS_setcache couldn't read micropart metadata (ret = %d)\n", ret);
	 assert (0);
       }
     }

     bc = __bc_lookup (devno, MICROPART_METADATA_PAGE);
     if (bc->buf_state != BC_VALID) {
       exos_bufcache_waitforio (bc);
     }

     /* now actually call cffsd to load in the micropart metadata and
	create a pxn for CFFS (this will return -E_EXISTS if this has
	already happend). */

     ret = cffsd_boot (devno);
     if (ret < 0 && ret != -E_EXISTS) {
       fprintf (stderr, "cffs_mountFS_setcache: couldn't boot cffsd partition\n");
       assert (0);
     }
   }
#endif /* CFFSD */
}

/* 
 * Perform a minimal "mount"--verify the specified block is likely
 * to be a real superblock and mark it as dirty. If weak_dirty is set
 * then dirty filesystems can be mounted and the superblock is not
 * scheduled to be written out to disk after being marked dirty.
 *
 * Requires that cffs_cache_init and cffs_protection_init have already
 * been properly called (the latter on the same device being
 * passed into cffs_mount_superblock).
 *
 */

int cffs_mount_superblock (int devno, u_int superblock, int weak_dirty) {
  cffs_t *superblk;
  buffer_t *sb_buf;

  CFFS_CHECK_SETUP (devno);

  sb_buf = cffs_get_superblock (devno, superblock, 0);
  superblk = (cffs_t *) sb_buf->buffer;

  if (strcmp(superblk->fsname,CFFS_FSNAME) != 0) {
    printf ("Not an exokernel CFFS file system\n");
    errno = ENXIO;
    return -1;
  }
  if (!weak_dirty && superblk->dirty != 0) {
    printf ("File system is dirty: run fsck before mounting\n");
    errno = ENXIO;
    return -1;
  }

  if (weak_dirty) {
    cffs_buffer_releaseBlock (sb_buf, BUFFER_DIRTY);
  } else {
    cffs_buffer_releaseBlock (sb_buf, BUFFER_WRITE);
  }

  return 0;
}

/*
 * Peform CFFS specific processing when mounting a disk. Called by
 * the higher level mount code.
 *
 */

int cffs_mountFS (int devno) {
  int ret;

  if ((devno < 0) || (devno >= __sysinfo.si_ndisks)) {
    return -1;
  }

  CFFS_CHECK_SETUP (devno);

  ret = cffs_mount_superblock (devno, CFFS_SUPERBLKNO, 0);

  return ret;
}

/*
 * Perform a minial unmount. Mark the superblock as clean. If weak_unmount
 * is set, just leave the superblock buffer marked dirty otherwise write
 * it and all buffers (on the system!?!?) out.
 *
 */

void cffs_unmount_superblock (u_int dev, block_num_t sb_num, int weak_unmount) {
   cffs_t *sb;
   buffer_t *sb_buf;

   CFFS_CHECK_SETUP (dev);

   sb_buf = cffs_get_superblock (dev, sb_num, 0);
   sb = (cffs_t *)(sb_buf->buffer);
   if (weak_unmount) {
     cffs_buffer_releaseBlock (sb_buf, BUFFER_DIRTY);
   } else {
     cffs_buffer_releaseBlock (sb_buf, BUFFER_WRITE);
     cffs_buffer_affectAll (BUFFER_WRITE);
   }
}

/* 
 * Performn CFFS specific processing for an unmount operation. This is
 * invoked by the higher level unmount code. Doesn't do much--just
 * call the cffs_unmount_superblock code.
 *
 */

int cffs_unmountFS (u_int dev) {

/*
printf ("unmountFS: global_unmount %d\n", global_unmount);
*/

   if ((dev < 0) || (dev >= __sysinfo.si_ndisks)) {
      printf ("cffs_unmount: Invalid device number (%d)\n", dev);
      assert (0);
   }

   CFFS_CHECK_SETUP (dev);

   cffs_unmount_superblock (dev, CFFS_SUPERBLKNO, 0);

   /* Note that the private per-app caches (inode and buffer) are not   */
   /* flushed here -- because globally shared dirty bits are used, this */
   /* is not necessary (or sufficient)...                               */

   return 0;
}

int cffs_device_usage (u_int dev, int *count) {
  buffer_t *sb_buf, *alloc_buf;
  cffs_t *sb;
  u_int allocMapBlocks;
  int i, j;

   if ((dev < 0) || (dev >= __sysinfo.si_ndisks)) {
     errno = ENODEV;
     return -1;
   }

  CFFS_CHECK_SETUP (dev);

  sb = cffs_proc_getSuperblock (dev, CFFS_SUPERBLKNO, &sb_buf);
  if (!sb) {
    errno = EIO;
    return -1;
  }
  
  *count = 0;
  allocMapBlocks = sb->numblocks/BLOCK_SIZE;
  for (i = 0; i < allocMapBlocks; i++) {
    alloc_buf = cffs_buffer_getBlock (dev, 0, 1, sb->allocMap+i, BUFFER_READ, NULL);
    if (!alloc_buf) {
      errno = EIO;
      return -1;
    }
    for (j = 0; j < BLOCK_SIZE; j++) {
      if (alloc_buf->buffer[j] == CFFS_ALLOCMAP_INUSE)
	(*count)++;
    }
    cffs_buffer_releaseBlock (alloc_buf, 0);
  }
  
  return 0;
}

/* apply a function to all inodes in a directory subtree rooted at dirInode 
   and named dirname. */

int cffs_map_directory (inode_t *dirInode, char *dirname, int f (inode_t *, dinode_t *, int *))
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   int currPos = 0;
   int i, j;
   inode_t *inode;
   int dirlength = strlen(dirname);
   int ret;
   dinode_t *dinode;
   int dirtied;

   CFFS_CHECK_SETUP (dirInode->fsdev);

   while (currPos < cffs_inode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode->dinode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = ((cffs_inode_getLength(dirInode) - currPos) < BLOCK_SIZE) ? (cffs_inode_getLength(dirInode) - currPos) : BLOCK_SIZE;
      dirBlockBuffer = cffs_buffer_getBlock (dirInode->fsdev, dirInode->sprblk, -dirInode->inodeNum, physicalDirBlockNumber, BUFFER_READ, NULL);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      dirtied = 0;

      for (i=0; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
         for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
            dirent = (embdirent_t *) (dirBlock + i + j);
            if (dirent->type == (char) 0) {
		/* dead directory entry -- if not start of sector, then */
               continue;    
            } else if (cffs_inode_blkno(dirent->inodeNum) == dirBlockBuffer->header.diskBlock) {
               dinode = (dinode_t *) (cffs_inode_offsetInBlock(dirent->inodeNum) + dirBlockBuffer->buffer);
               assert (dinode->dinodeNum != 0);
               inode = cffs_inode_makeInode (dirInode->fsdev, dirInode->sprblk, dinode->dinodeNum, dinode, dirBlockBuffer);
               cffs_buffer_bumpRefCount (dirBlockBuffer);
            } else {
	      printf ("warning, skipping external something or another\n");
	      continue;
            }

		/* recurse depth-first on directories (also update link cnt) */
            if (cffs_inode_isDir(inode)) {
               sprintf (dirname, "%s%s/", dirname, dirent->name);
               ret = cffs_map_directory (inode, dirname, f);
               dirname[dirlength] = (char) 0;
	       if (ret < 0)
		 goto error;
            }

	    ret = f (inode, dinode, &dirtied);
	    if (ret < 0)
	      return ret;

            if (cffs_inode_blkno(dirent->inodeNum) == dirBlockBuffer->header.diskBlock) {
               inode->dinode = NULL;
               cffs_inode_tossInode (inode);
            } else {
	      assert (0);
               cffs_inode_releaseInode (inode, 0);
            }
         }
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, dirtied ? 0 : BUFFER_DIRTY);
      currPos += dirBlockLength;
   }

   dirname[dirlength] = (char) 0;
   return 0;

 error:
   cffs_buffer_releaseBlock (dirBlockBuffer, 0);
   return ret;
}

int cffs_printstats ()
{
   int blocks;
   int i;
   cffs_t *sb;
   buffer_t *sb_buf;

   for (i = 0; i < MAX_DISKS; i++) {

     if (global_ftable->mounted_disks[i] == 0)
       continue;

     CFFS_CHECK_SETUP(i);

     sb = cffs_proc_getSuperblock (i, CFFS_SUPERBLKNO, &sb_buf);

     printf ("Stats for the mounted exokernel C-FFS file system on device %d:\n", i);

     printf ("Total size: %qd (%d %d-byte blocks)\n", sb->size, sb->numblocks, BLOCK_SIZE);
     blocks = sb->numblocks - sb->numalloced;

#ifdef XN
     printf ("Available size: allocation is via XN.  See XN stats.\n");
#else
     printf ("Available size: %qd (%d blocks, %qd percent)\n", ((off_t)blocks * (off_t) BLOCK_SIZE), blocks, (((off_t) blocks * (off_t) 100) / (off_t)sb->numblocks));
#endif

     cffs_buffer_releaseBlock (sb_buf, 0);

   }

   return (0);
}

/*
 * Build a file * that contains information about the root director of 
 * a filesystem. cffs_grabRoot is called by the higher level mount
 * code in order to record the file * that namei should use when crossing
 * into this filesystem. 
 *
 * XXX -- no reason to have this a seperate function...this should really
 * be merged into cffs_mountFS.
 *
 */

void cffs_grabRoot (u_int dev, block_num_t sb_num, struct file *dirp)
{
   inode_t *inode;
   cffs_t *sb;
   buffer_t *sb_buf;

DPRINTF (1, ("%d: cffs_grabRoot\n", getpid()));

   demand (dirp, bogus filp);

   sb_buf = cffs_get_superblock (dev, sb_num, 0);
   sb = (cffs_t *)(sb_buf->buffer);
   inode = cffs_inode_getInode (sb->rootDInodeNum, dev, sb->blk, BUFFER_READ);
   assert ((inode) && (inode->dinode));
   cffs_buffer_releaseBlock (sb_buf, 0);

   dirp->f_dev = dev;
   dirp->f_ino = inode->inodeNum;
   dirp->f_pos = 0;
   filp_refcount_init(dirp);
   filp_refcount_inc(dirp);
   dirp->f_owner = geteuid();
   dirp->op_type = CFFS_TYPE;
   dirp->f_mode = cffs_make_st_mode (0755, S_IFDIR);
   dirp->f_flags = 0;

   assert (inode->inodeNum);
   FILEP_SETINODENUM(dirp,inode->inodeNum);
   FILEP_SETSUPERBLOCK(dirp, sb_num);

   cffs_inode_releaseInode (inode, 0);
}

/* cffs_open

   Open the file named by "name" in the directory described by "dirp".
   Upon return, "filp" will be filled in to describe the opened file and can
   be used for subsequent cffs_* calls.  The flags and mode correspond to
   the same fields of an open() call, which this correctly emulates (we hope).
*/

int cffs_open (struct file *dirp, struct file *filp, char *name, int flags, mode_t mode) {
   inode_t *inode;
   inode_t *dirInode;
   int ret = 0;
   int createdfile = 0;
   cffs_t *sb;
   buffer_t *sb_buf;
   int error;

DPRINTF (1, ("%d: cffs_open: %s (flags %x), filp %p\n", getpid(), name, flags, filp));

   if (strlen(name) > CFFS_EMBDIR_MAX_FILENAME_LENGTH) {
      errno = ENAMETOOLONG;
      return (-1);
   }

   demand (dirp, bogus dirp);
   demand (filp, bogus filp);

   CFFS_CHECK_SETUP (dirp->f_dev);

   ENTERCRITICAL

   if (flags & (O_SHLOCK | O_EXLOCK)) {
      errno = EOPNOTSUPP;
      RETURNCRITICAL (-1);
   }

   sb = cffs_proc_getSuperblock (dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), &sb_buf);
   dirInode = cffs_inode_getInode ((FILEP_GETINODENUM(dirp)), dirp->f_dev, sb->blk, (BUFFER_READ|BUFFER_WRITE));
   assert (dirInode);

   /* first thing to do is figure out if we're creating a file or not */
   if (flags & O_CREAT) {

      /* verify that we can write to the parent directory. */

      if (!verifyWritePermission (cffs_inode_getMask(dirInode), geteuid(), cffs_inode_getUid(dirInode), getegid(), cffs_inode_getGid(dirInode))) {
         errno = EACCES;
	 goto error;
      }

      /* prepare the directory for the new entry and check for collision */
      /* this also returns an "in-core" inode structure for the new file */

      inode = NULL;
      error = 0;
      ret = cffs_path_makeNewFile (sb_buf, dirInode, name, strlen(name), &inode, 0, 1, S_IFREG, &error);
      if (error) {
	if (inode) {
	  cffs_inode_releaseInode (inode, 0);
	}
	cffs_inode_releaseInode (dirInode, 0);
	goto error;
      }
      assert (inode != NULL);
      if (ret != 0) {

         if (flags & O_EXCL) {
            cffs_inode_releaseInode (inode, 0);
            cffs_inode_releaseInode (dirInode, 0);
            errno = EEXIST;
	    goto error;
         }

         if (cffs_inode_isDir(inode)) {
            cffs_inode_releaseInode (inode, 0);
            cffs_inode_releaseInode (dirInode, 0);
            errno = EISDIR;
	    goto error;
         }

         if (flags & O_TRUNC) {
            if (!verifyWritePermission (cffs_inode_getMask(inode), geteuid(), cffs_inode_getUid(inode), getegid(), cffs_inode_getGid(inode))) {
               cffs_inode_releaseInode (inode, 0);
               errno = EACCES;
	       goto error;
            }
            ret = cffs_inode_truncInode (inode, 0);
            assert (ret == 0);
            cffs_buffer_affectFile (dirp->f_dev, inode->inodeNum, BUFFER_INVALID, 1);
            cffs_inode_setModTime (inode, time(NULL));
         }

      } else {
         cffs_fdInitDinode (cffs_inode_dinode(inode), mode);
         createdfile = 1;
      }

		/* assume that inode has changed in an important way */
      cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);
   } else {
	  /* lookup the file's inode */
/*
printf ("going to translate Name to Inode\n");
*/
      inode = cffs_path_translateNameToInode (sb_buf, name, dirInode, (BUFFER_WITM | BUFFER_READ));
/*
printf ("back from translate Name to Inode: inode %p\n", inode);
*/
      cffs_inode_releaseInode (dirInode, 0);
      if (inode == NULL) {
         errno = ENOENT;
	 goto error;
      }

      if (flags & O_TRUNC) {
         if (!verifyWritePermission (cffs_inode_getMask(inode), geteuid(), cffs_inode_getUid(inode), getegid(), cffs_inode_getGid(inode))) {
            cffs_inode_releaseInode (inode, 0);
            errno = EACCES;
	    goto error;
         }
         ret = cffs_inode_truncInode (inode, 0);
         assert (ret == 0);
         cffs_buffer_affectFile (dirp->f_dev, inode->inodeNum, BUFFER_INVALID, 1);
         cffs_inode_setModTime (inode, time(NULL));
      }
   }
/*
printf("sys_open: path %s ==> inodeNum %d (%x)\n", name, inode->inodeNum, inode->inodeNum);
*/

   /* check access permissions */
   if (((flags & O_ACCMODE) == O_RDONLY) || ((flags & O_ACCMODE) == O_RDWR)) {
      if (!createdfile && !verifyReadPermission (cffs_inode_getMask(inode), geteuid(), cffs_inode_getUid(inode), getegid(), cffs_inode_getGid(inode))) {
/*
printf ("read permission check failed: inodeNum %d (%x), mask %x, uid %d, gid %d\n", inode->inodeNum, inode->dinode->dinodeNum, cffs_inode_getMask(inode), cffs_inode_getUid(inode), cffs_inode_getGid(inode));
*/
         cffs_inode_releaseInode (inode, 0);
         errno = EACCES;
	 goto error;
      }
   }
   if (((flags & O_ACCMODE) == O_WRONLY) || ((flags & O_ACCMODE) == O_RDWR)) {
      /* make sure this is really a data file and not a dir */
      if (cffs_inode_isDir(inode)) {
         cffs_inode_releaseInode (inode, 0);
         errno = EISDIR;
	 goto error;
      }
      if (!createdfile && !verifyWritePermission (cffs_inode_getMask(inode), geteuid(), cffs_inode_getUid(inode), getegid(), cffs_inode_getGid(inode))){
         cffs_inode_releaseInode (inode, 0);
         errno = EACCES;
	 goto error;
      }
   }

   cffs_inode_setOpenCount (inode, (cffs_inode_getOpenCount(inode) + 1));

   /* now initialize the struct file * stuff */
   filp->f_dev = dirp->f_dev;
   filp->f_ino = inode->inodeNum;
   filp->f_mode = cffs_make_st_mode (mode, (cffs_inode_getType(inode)));
   filp->f_pos = 0;
   filp->f_flags = flags;
   filp_refcount_init(filp);
   filp_refcount_inc(filp);
   filp->f_owner = geteuid();
   filp->op_type = CFFS_TYPE;

   FILEP_SETINODENUM(filp,inode->inodeNum);
   FILEP_SETSUPERBLOCK(filp, FILEP_GETSUPERBLOCK (dirp));

   cffs_inode_releaseInode (inode, BUFFER_DIRTY);

   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (0);
   
 error:
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (-1);
}


/* cffs_read

   Read from a file. Check that the file has been opened and that
   we have read permission. Updates the file pointer.
*/

int cffs_read (struct file *filp, char *buf, int nbytes, int blocking) {
   inode_t *inode;
   int ret = 0;
   int error;

DPRINTF (1, ("%d: cffs_read: nbytes %d, blocking %d\n", getpid(), nbytes, blocking));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);

   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));

   /* make sure we've got the file open for reading */
   if ((inode == NULL) || (((filp->f_flags & O_ACCMODE) != O_RDONLY) && ((filp->f_flags & O_ACCMODE) != O_RDWR))) {
      errno = (inode) ? EACCES : EBADF;
      ret = -1;
   }

   /* do the read */
   if (ret >= 0) {
     error = 0;
      ret = cffs_readBuffer (FILEP_GETSUPERBLOCK(filp), inode, buf, filp->f_pos, nbytes, &error);
      if (error) {
	cffs_inode_releaseInode (inode, 0);	
	RETURNCRITICAL (-1);
      }
      if (ret >= 0) {
         /* update the file position */
         filp->f_pos += ret;
      } else {
         errno = -ret;
         ret = -1;
      }
   }

   cffs_inode_releaseInode (inode, 0);

DPRINTF (2, ("%d: cffs_read done: newpos %d, ret %d, errno %d\n", getpid(), (uint)filp->f_pos, ret, errno));

   RETURNCRITICAL (ret);
}


/* cffs_write

   Write data to a file. Check that the file has been opened and that
   we have write permission. Updates the file pointer. 
*/

int cffs_write (struct file *filp, char *buf, int nbytes, int blocking) {
   inode_t *inode;
   int ret;
   int error;

DPRINTF (1, ("%d: cffs_write: nbytes %d\n", getpid(), nbytes));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));

   /* make sure we've got the file open for writing */
   if ((inode == NULL) || (((filp->f_flags & O_ACCMODE) != O_WRONLY) && ((filp->f_flags & O_ACCMODE) != O_RDWR))) {
      cffs_inode_releaseInode (inode, 0);
      errno = (inode) ? EACCES : EBADF;
      RETURNCRITICAL (-1);
   }

   if (filp->f_flags & O_APPEND) {
      filp->f_pos = cffs_inode_getLength (inode);
   }

   /* do the write */
   error = 0;
   ret = cffs_writeBuffer (FILEP_GETSUPERBLOCK(filp), inode, buf, filp->f_pos, nbytes, &error);
   if (error) {
     cffs_inode_releaseInode (inode, 0);
     RETURNCRITICAL (-1);
   }
   if (ret < 0) {
      cffs_inode_releaseInode (inode, 0);
      errno = -ret;
      RETURNCRITICAL (-1);
   }

   /* update the file pointer */
   filp->f_pos += ret;
   cffs_inode_setModTime (inode, time(NULL));
   cffs_inode_releaseInode (inode, BUFFER_DIRTY);

   RETURNCRITICAL (ret);
}


int cffs_release (struct file *filp)
{
   return (0);
}


/* cffs_close

   Close a file.
 */

int cffs_close (struct file *filp) {
   inode_t *inode;

DPRINTF (1, ("%d: cffs_close: filp %p\n", getpid(), filp));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert ((inode != NULL) && (inode->dinode != NULL));

   if (inode == NULL) {
      errno = EBADF;
      RETURNCRITICAL (-1);
   }
/*
printf ("inodeNum %d, size %d, flags %x\n", inode->inodeNum, cffs_inode_getLength (inode), filp->f_flags);
*/
   cffs_inode_setAccTime (inode, time(NULL));

   if ((filp->f_flags & O_ACCMODE) != O_RDONLY) {
      cffs_inode_setModTime (inode, time(NULL));
   }

   assert (cffs_inode_getOpenCount(inode) >= 1);
   cffs_inode_setOpenCount (inode, (cffs_inode_getOpenCount(inode) - 1));

   cffs_inode_releaseInode (inode, BUFFER_DIRTY);

   FILEP_SETINODENUM(filp,0);
/*
printf ("cffs_close: done\n");
*/
   RETURNCRITICAL (0);
}


/* cffs_lseek

   Adjust the file pointer.
 */

off_t cffs_lseek (struct file *filp, off_t offset, int whence) {
   inode_t *inode;
   off_t new;
	/* GROK - get rid of offset_t and use 64-bit off_t */

DPRINTF (1, ("%d: cffs_lseek to %qd from %d\n", getpid(), offset, whence));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode != NULL);

   if (inode == NULL) {
      errno = EBADF;
      RETURNCRITICAL (-1);
   }

   switch (whence) {
      case SEEK_SET: new = offset; break;
      case SEEK_CUR: new = filp->f_pos + offset; break;
      case SEEK_END: new = cffs_inode_getLength(inode) + offset; break;
      default: new = -1;
   }

   if (new < 0) {
      cffs_inode_releaseInode (inode, 0);
      errno = EINVAL;
      RETURNCRITICAL (-1);
   }
   filp->f_pos = new;
   cffs_inode_releaseInode (inode, 0);

   RETURNCRITICAL (filp->f_pos);
}


int cffs_lookup (struct file *dirp, const char *name, struct file *filp)
{
   inode_t *dirInode;
   inode_t *inode;
   cffs_t *sb;
   buffer_t *sb_buf;

   demand (dirp, bogus filp);
   demand (filp, bogus filp);

DPRINTF (1, ("%d: cffs_lookup: dirp %p (dirIno %x), name %s\n", getpid(), dirp, (FILEP_GETINODENUM(dirp)), name));

   if (strlen(name) > CFFS_EMBDIR_MAX_FILENAME_LENGTH) {
      errno = ENAMETOOLONG;
      return (-1);
   }

   CFFS_CHECK_SETUP (dirp->f_dev);

#ifndef JJ
	/* GROK - this is a grotesque hack... */
  /* hardwire terminal support!! */
  if (!strcmp(name,"dev") && (FILPEQ(dirp,__current->root))) {
    filp->f_ino = 599999;
    filp->f_pos = 0;
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = getuid();
#ifdef NEWPTY
    filp->op_type = CDEV_TYPE;
#else
    filp->op_type = NPTY_TYPE;
#endif
    filp->f_mode = cffs_make_st_mode (0755, S_IFDIR);
    filp->f_flags = 0;
    return (0);
  }
#endif

  ENTERCRITICAL;

   sb = cffs_proc_getSuperblock (dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), &sb_buf);
   dirInode = cffs_inode_getInode ((FILEP_GETINODENUM(dirp)), dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), (BUFFER_READ|BUFFER_WITM));
   assert (dirInode);


DPRINTF (3, ("(dirInode %p)", dirInode));
DPRINTF (3, (" %s in dir (ino 0x%x)\n", name, dirInode->inodeNum));

   inode = cffs_path_translateNameToInode (sb_buf, name, dirInode, (BUFFER_WITM|BUFFER_READ));
   cffs_inode_releaseInode (dirInode, 0);
   if (inode == NULL) {
      errno = ENOENT;
      goto error;
   }

   /* Set up the struct file * stuff */
   filp->f_dev = dirp->f_dev;
   filp->f_ino = inode->inodeNum;
   filp->f_pos = 0;
   filp_refcount_init(filp);
   filp->f_owner = geteuid();
   filp->op_type = CFFS_TYPE;
   filp->f_mode = cffs_make_st_mode (0, (int)(cffs_inode_getType(inode)));
   filp->f_flags = 0;

#if 0
  if ((S_ISDIR(filp->f_mode)) && !verifySearchOrExecutePermission (cffs_inode_getMask (inode), geteuid (), cffs_inode_getUid (inode), getegid (),
								   cffs_inode_getGid (inode))) {
    errno = EACCES;
    goto error;
  }
#endif

DPRINTF (3, ("found Ino 0x%x, linkCount %d\n", inode->inodeNum, cffs_inode_getLinkCount(inode)));

   FILEP_SETINODENUM(filp,inode->inodeNum);
   FILEP_SETSUPERBLOCK(filp, FILEP_GETSUPERBLOCK(dirp));
   cffs_inode_releaseInode (inode, 0);
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL(0);

 error:
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (-1);
}


int cffs_printdirinfo (struct file *filp)
{
   inode_t *inode;
   int ret;
   cffs_t *sb;
   buffer_t *sb_buf;

DPRINTF (3, ("%d: cffs_printdirinfo\n", getpid()));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   sb = cffs_proc_getSuperblock (filp->f_dev, FILEP_GETSUPERBLOCK(filp), &sb_buf);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode && inode->dinode);

   /* make sure we've got the file open for reading */
   if ((inode == NULL) || (((filp->f_flags & O_ACCMODE) != O_RDONLY) && ((filp->f_flags & O_ACCMODE) != O_RDWR))) {
      cffs_inode_releaseInode (inode, 0);
      errno = (inode) ? EACCES : EBADF;
      goto error;
   }
   if (!cffs_inode_isDir(inode)) {
      cffs_inode_releaseInode (inode, 0);
      errno = ENOTDIR;
      goto error;
   }

   /* cffs_path_getdirentries updates the file position */
   ret = cffs_path_printdirinfo (sb_buf, inode);
   cffs_inode_releaseInode (inode, 0);
   if (ret < 0) {
      errno = -ret;
      goto error;
   }

   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (0);

 error:
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (-1);
}


/* ioctl -- used for printing various stuffs */

int cffs_ioctl (struct file *filp, u_int request, char *argp)
{
   switch (request) {
      case CFFS_IOCTL_PRINTDIRINFO:
		return (cffs_printdirinfo (filp));
      default:
   }

   errno = ENODEV;	/* Supposed to say op not supported */
   return (-1);
}


/* cffs_getdirentries

*/

int cffs_getdirentries (struct file *filp, char *buf, int nbytes) {
   inode_t *inode;
   int ret;
   cffs_t *sb;
   buffer_t *sb_buf;

DPRINTF (1, ("%d: cffs_getdirentries\n", getpid()));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   sb = cffs_proc_getSuperblock (filp->f_dev, FILEP_GETSUPERBLOCK(filp), &sb_buf);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode && inode->dinode);

   /* make sure we've got the file open for reading */
   if ((inode == NULL) || (((filp->f_flags & O_ACCMODE) != O_RDONLY) && ((filp->f_flags & O_ACCMODE) != O_RDWR))) {
      errno = (inode) ? EACCES : EBADF;
      goto error;
   }
   if (!cffs_inode_isDir(inode)) {
      errno = ENOTDIR;
      goto error;
   }

   /* cffs_path_getdirentries updates the file position */
   ret = cffs_path_getdirentries (sb_buf, inode, buf, nbytes, &filp->f_pos);

   if (ret < 0) {
      errno = -ret;
      goto error;
   }

   cffs_inode_releaseInode (inode, 0);
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (ret);

 error:
   cffs_inode_releaseInode (inode, 0);
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (-1);
}


/* cffs_mkdir

 Create an empty directory and link it into the specified parent (dirp).
 */

int cffs_mkdir (struct file *dirp, const char *name, mode_t mode) {
   inode_t *dirInode;
   int ret;
   inode_t *inode;
   cffs_t *sb;
   buffer_t *sb_buf;
   int error;

DPRINTF (1, ("%d: cffs_mkdir: %s (mode %x)\n", getpid(), name, (uint)mode));

   if (strlen(name) > CFFS_EMBDIR_MAX_FILENAME_LENGTH) {
      errno = ENAMETOOLONG;
      return (-1);
   }

   CFFS_CHECK_SETUP (dirp->f_dev);

   ENTERCRITICAL

   demand (dirp, bogus dirp);
   sb = cffs_proc_getSuperblock (dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), &sb_buf);
   dirInode = cffs_inode_getInode ((FILEP_GETINODENUM(dirp)), dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), (BUFFER_READ|BUFFER_WITM));
   assert (dirInode);

   if (!cffs_inode_isDir (dirInode)) {
      cffs_inode_releaseInode (dirInode, 0);
      errno = ENOTDIR;
      goto error;
   }

   /* verify that we can write to the parent directory. */

   if (!verifyWritePermission (cffs_inode_getMask(dirInode), geteuid(), cffs_inode_getUid(dirInode), getegid(), cffs_inode_getGid(dirInode))) {
      cffs_inode_releaseInode (dirInode, 0);
      errno = EACCES;
      goto error;
   }

   /* link in the new directory */

   inode = NULL;
   error = 0;
   ret = cffs_path_makeNewFile (sb_buf, dirInode, name, strlen(name), &inode, 0, 0, S_IFDIR, &error);
   if (error) {
      if (inode) {
         cffs_inode_releaseInode (inode, 0);
      }
      cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);
      goto error;
   }

   if (ret == 0) {
      cffs_fdInitDinode (cffs_inode_dinode(inode), mode);
      cffs_inode_releaseInode (inode, BUFFER_DIRTY);
   } else {
      if (inode) {
         cffs_inode_releaseInode (inode, 0);
      }
      cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);
      errno = EEXIST;
      goto error;
   }

   cffs_inode_setModTime (dirInode, time(NULL));
   cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL  (ret);

 error:
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL  (-1);
}


int cffs_rename (struct file *dirfrom, const char *namefrom,
                 struct file *dirto,   const char *nameto)
{
   inode_t *dirInode1;
   inode_t *dirInode2;
   cffs_t *sb_from, *sb_to;
   buffer_t *sb_buf_from, *sb_buf_to;
   int ret;
   int error;

DPRINTF (1, ("%d: cffs_rename: %s to %s\n", getpid(), namefrom, nameto));

   if (strlen(nameto) > CFFS_EMBDIR_MAX_FILENAME_LENGTH) {
      errno = ENAMETOOLONG;
      return (-1);
   }

   CFFS_CHECK_SETUP (dirfrom->f_dev);
   CFFS_CHECK_SETUP (dirto->f_dev);

   ENTERCRITICAL

   demand (dirfrom, bogus dirfrom);
   demand (dirto, bogus dirto);

   sb_from = cffs_proc_getSuperblock (dirfrom->f_dev, FILEP_GETSUPERBLOCK(dirfrom), &sb_buf_from);
   sb_to = cffs_proc_getSuperblock (dirto->f_dev, FILEP_GETSUPERBLOCK(dirto), &sb_buf_to);

   dirInode1 = cffs_inode_getInode ((FILEP_GETINODENUM(dirfrom)), dirfrom->f_dev, sb_from->blk, (BUFFER_READ|BUFFER_WITM));
   assert (dirInode1);

   /* verify that we can write to the parent directory. */

   if (!verifyWritePermission (cffs_inode_getMask(dirInode1), geteuid(), cffs_inode_getUid(dirInode1), getegid(), cffs_inode_getGid(dirInode1))) {
      cffs_inode_releaseInode (dirInode1, 0);
      errno = EACCES;
      goto error;
   }

   dirInode2 = cffs_inode_getInode ((FILEP_GETINODENUM(dirto)), dirto->f_dev, sb_to->blk, (BUFFER_READ|BUFFER_WITM));
   assert (dirInode2);

   /* verify that we can write to the parent directory. */

   if (!verifyWritePermission (cffs_inode_getMask(dirInode2), geteuid(), cffs_inode_getUid(dirInode2), getegid(), cffs_inode_getGid(dirInode2))) {
      cffs_inode_releaseInode (dirInode1, 0);
      cffs_inode_releaseInode (dirInode2, 0);
      errno = EACCES;
      goto error;
   }

   error = 0;
   ret = cffs_path_renameEntry (sb_buf_from, dirInode1, namefrom, sb_buf_to, dirInode2, nameto, &error);

   cffs_inode_releaseInode (dirInode1, BUFFER_DIRTY);
   cffs_inode_releaseInode (dirInode2, BUFFER_DIRTY);

   if (error) {
     goto error;
   }

DPRINTF (2, ("rename done: ret %d\n", ret));

   if (ret < 0) {
      errno = -ret;
      goto error;
   }

   cffs_buffer_releaseBlock (sb_buf_from, 0);
   cffs_buffer_releaseBlock (sb_buf_to, 0);
   RETURNCRITICAL (0);

 error:
   cffs_buffer_releaseBlock (sb_buf_from, 0);
   cffs_buffer_releaseBlock (sb_buf_to, 0);
   RETURNCRITICAL (-1);
}


/* cffs_unlink

   Currently, we do both unlink and rmdir with this one function.  We should
   really separate the two for correct behavior (only difference is that each
   should not do the other...).
*/

int cffs_unlink (struct file *dirp, const char *name) {
    inode_t *dirInode;
    inode_t *inode;
    void *dirent;
    buffer_t *buffer;
    cffs_t *sb;
    buffer_t *sb_buf;

DPRINTF (1, ("%d: cffs_unlink: %s\n", getpid(), name));

   CFFS_CHECK_SETUP (dirp->f_dev);

   ENTERCRITICAL

   demand (dirp, bogus dirp);
   sb = cffs_proc_getSuperblock (dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), &sb_buf);
   dirInode = cffs_inode_getInode ((FILEP_GETINODENUM(dirp)), dirp->f_dev, sb->blk, (BUFFER_READ|BUFFER_WITM));
   assert (dirInode);

   /* verify that we can write to the parent directory. */

   if (!verifyWritePermission (cffs_inode_getMask(dirInode), geteuid(), cffs_inode_getUid(dirInode), getegid(), cffs_inode_getGid(dirInode))) {
      cffs_inode_releaseInode (dirInode, 0);
      errno = EACCES;
      goto error;
   }

   inode = cffs_path_grabEntry (sb_buf, dirInode, name, strlen(name), &dirent, &buffer);
   if (inode == NULL) {
      cffs_inode_releaseInode (dirInode, 0);
      errno = ENOENT;
      goto error;
   }

   if (cffs_inode_isDir(inode)) {
      if (cffs_path_isEmpty(sb_buf, inode) == 0) {
         cffs_inode_releaseInode (inode, 0);
         cffs_inode_releaseInode (dirInode, 0);
         cffs_buffer_releaseBlock (buffer, 0);
         errno = ENOTEMPTY;
	 goto error;
      }
   }

	/* removeEntry will release the inode and handle ordering properly */

   cffs_path_removeEntry (dirInode, name, strlen(name), dirent, inode, buffer, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));

   cffs_inode_setModTime (dirInode, time(NULL));
   cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);
   cffs_buffer_releaseBlock (sb_buf, 0);
     
   RETURNCRITICAL (0);

 error:
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (-1);
}


/* Not quite the right behavior (should work for directories only), but not */
/* worth repreating the code in the short term */

int cffs_rmdir (struct file *dirp, const char *name)
{
   return (cffs_unlink(dirp, name));
}


void fillStatBuf (inode_t *inode, u_int dev, struct stat *buf) {

   assert ((inode) && (inode->dinode));

   buf->st_dev = dev;
   buf->st_ino = inode->inodeNum;
   buf->st_mode = cffs_make_st_mode ((int)(cffs_inode_getMask(inode)),((int)cffs_inode_getType(inode)));
   buf->st_nlink = cffs_inode_getLinkCount(inode);
   buf->st_uid = cffs_inode_getUid(inode);
   buf->st_gid = cffs_inode_getGid(inode);
   buf->st_rdev = 0;
   buf->st_size = cffs_inode_getLength(inode);
   buf->st_atime = cffs_inode_getAccTime(inode);
   buf->st_atimensec = 0;
   buf->st_mtime = cffs_inode_getModTime(inode);
   buf->st_mtimensec = 0;
   buf->st_ctime = cffs_inode_getCreTime(inode);
   buf->st_ctimensec = 0;
   buf->st_blksize = BLOCK_SIZE;
   buf->st_blocks = ((cffs_inode_getLength(inode) + BLOCK_SIZE - 1) / BLOCK_SIZE) * (BLOCK_SIZE / 512);
}


/* cffs_stat

   Return info about a filename.
 */

int cffs_stat (struct file *filp, struct stat *buf) {
   inode_t *inode;

DPRINTF (1, ("%d: cffs_stat: filp %p, buf %p\n", getpid(), filp, buf));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode);

   fillStatBuf (inode, filp->f_dev, buf);
/*
printf ("exit cffs_stat: ret %d\n", OK);
*/
   cffs_inode_releaseInode (inode, 0);
   RETURNCRITICAL (0);
}


/* cffs_bmap

   Return diskBlock corresponding to offset in file.  Also, return a count
   of the number of logically sequential blocks that are also physically
   sequential (useful for prefetching).
 */

int cffs_bmap (struct file *filp, u_int *diskBlockHighP, u_int *diskBlockLowP, 
	       off_t offset, u_int *seqBlocksP) {
   inode_t *inode;
   u_int diskBlock;
   int error;

DPRINTF (1, ("%d: cffs_bmap: filp %p, offset %x\n", getpid(), filp, (u_int)offset));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode);

   if ((offset < 0) || (offset >= cffs_inode_getLength(inode))) {
      cffs_inode_releaseInode (inode, 0);
      errno = EINVAL;
      RETURNCRITICAL (-1);
   }

   error = 0;
   diskBlock = cffs_inode_offsetToBlock (inode, (u_int) offset, NULL, &error);
   if (error) {
     cffs_inode_releaseInode (inode, 0);
     errno = EDQUOT;
     RETURNCRITICAL (-1);
   }
   *diskBlockLowP = diskBlock;
   *diskBlockHighP = 0;
   if (seqBlocksP) {
      *seqBlocksP = cffs_inode_getNumContig (inode, (offset/BLOCK_SIZE));
   }
/*
printf ("exit cffs_bmap: <ino %d, off %d> --> diskBlock %d\n", inode->inodeNum, (u_int)offset, diskBlock);
*/
   cffs_inode_releaseInode (inode, 0);
   RETURNCRITICAL (0);
}


/* cffs_chmod 

   Change the file permissions on a file. Must be the owner of the
   file or the super-user to do this.

   XXX -- this doesn't follow unix semantics exactly: if a file
   is opened and then the file's permissions are changed the
   process with the open file see's the new permissions and not
   the old permissions.
*/

int cffs_chmod (struct file *filp, mode_t mode) {
   inode_t *inode;

DPRINTF (1, ("%d: cffs_chmod: filp %p, mode %x\n", getpid(), filp, (u_int)mode));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode);

   if (geteuid() != 0 && cffs_inode_getUid(inode) != geteuid()) {
      cffs_inode_releaseInode (inode, 0);
      errno = EACCES;
      RETURNCRITICAL (-1);
   }

   cffs_inode_setMask (inode, mode);

   cffs_inode_releaseInode (inode, BUFFER_DIRTY);
   RETURNCRITICAL (0);
}


int cffs_chown (struct file *filp, uid_t owner, gid_t group) {
   inode_t *inode;
   int noaccess = 0;

DPRINTF (1, ("%d: cffs_chown: filp %p, owner %d, group %d\n", getpid(), filp, (int)owner, (int)group));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode);

   /* if not root... */
   if (geteuid() != 0) {
     /* if you're trying to change the owner... */
     if (owner != -1) {
       /* can't change if you're not the owner */
       if (cffs_inode_getUid(inode) != geteuid())
	 noaccess = 1;
       /* can't change if we're changing! */
       else if (owner != cffs_inode_getUid(inode))
	 noaccess = 1;
     }

     /* if you're trying to change the group... */
     if (!noaccess && owner != -1) {
       /* can't change if you're not the owner */
       if (cffs_inode_getUid(inode) != geteuid())
	 noaccess = 1;
       /* can't change if you're not in the new group */
       else if (!__is_in_group(group))
	 noaccess = 1;
     }
   }

   if (noaccess) {
     cffs_inode_releaseInode (inode, 0);
     errno = EACCES;
     RETURNCRITICAL (-1);
   }

   if (owner != -1) cffs_inode_setUid (inode, owner);
   if (group != -1) cffs_inode_setGid (inode, group);

   cffs_inode_releaseInode (inode, BUFFER_DIRTY);
   RETURNCRITICAL (0);
}


int cffs_utimes (struct file *filp, const struct timeval *times)
{
   inode_t *inode;

DPRINTF (1, ("%d: cffs_utimes: filp %p\n", getpid(), filp));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode);

	/* ignoring times[0].tv_usec and times[1].tv_usec */
   cffs_inode_setAccTime (inode, times[0].tv_sec);
   cffs_inode_setModTime (inode, times[1].tv_sec);

   cffs_inode_releaseInode (inode, BUFFER_DIRTY);
   RETURNCRITICAL (0);
}


int cffs_truncate (struct file *filp, off_t length) {
   inode_t *inode;
   int ret;
   off_t originalLength;

DPRINTF (1, ("%d: cffs_truncate: filp %p\n", getpid(), filp));

   if (length < 0) {
      errno = EINVAL;
      return (-1);
   }

   if (length > MAX_FILE_SIZE) {
      errno = EFBIG;
      return (-1);
   }

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode);

   if (!verifyWritePermission (cffs_inode_getMask(inode), geteuid(), cffs_inode_getUid(inode), getegid(), cffs_inode_getGid(inode))) {
      cffs_inode_releaseInode (inode, 0);
      errno = EACCES;
      RETURNCRITICAL (-1);              /* no write permission */
   }

   originalLength = cffs_inode_getLength (inode);

   ret = cffs_inode_truncInode (inode, length);
   if (ret < 0) {
      cffs_inode_releaseInode (inode, 0);
      errno = -ret;
      RETURNCRITICAL (-1);
   }

   if (length == 0) {
     cffs_buffer_affectFile (inode->fsdev, inode->inodeNum, BUFFER_INVALID, 1);
   } else {
     cffs_buffer_affectFileTail (filp->f_dev, inode->inodeNum, length + BLOCK_SIZE, originalLength, BUFFER_INVALID);
   }

   cffs_inode_setModTime (inode, time(NULL));

   cffs_inode_releaseInode (inode, BUFFER_DIRTY);
   RETURNCRITICAL (0);
}


/* cffs_link

 Create a hard link to a file.
 */

int cffs_link (struct file *tofilp, struct file *dirp, const char *name) {
   inode_t *dirInode;
   int ret;
   inode_t *inode;
   cffs_t *sb_to, *sb_dir;
   buffer_t *sb_buf_to, *sb_buf_dir;
   int error;

DPRINTF (1, ("%d: cffs_link: %s\n", getpid(), name));

   if (strlen(name) > CFFS_EMBDIR_MAX_FILENAME_LENGTH) {
      errno = ENAMETOOLONG;
      return (-1);
   }

   ENTERCRITICAL

   demand (dirp, bogus dirp);
   demand (tofilp, bogus tofilp);

   assert (dirp->op_type == CFFS_TYPE);
   assert (tofilp->op_type == CFFS_TYPE);
   assert (tofilp->f_dev == dirp->f_dev);

   CFFS_CHECK_SETUP (tofilp->f_dev);
   CFFS_CHECK_SETUP (dirp->f_dev);

   sb_to = cffs_proc_getSuperblock (tofilp->f_dev, FILEP_GETSUPERBLOCK(tofilp), &sb_buf_to);
   sb_dir = cffs_proc_getSuperblock (dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), &sb_buf_dir);

   dirInode = cffs_inode_getInode ((FILEP_GETINODENUM(dirp)), dirp->f_dev, sb_dir->blk, (BUFFER_READ|BUFFER_WITM));
   assert (dirInode);

   if (!cffs_inode_isDir(dirInode)) {
      cffs_inode_releaseInode (dirInode, 0);
      errno = ENOTDIR;
      goto error;
   }

   /* verify that we can write to the parent directory. */

   if (!verifyWritePermission (cffs_inode_getMask(dirInode), geteuid(), cffs_inode_getUid(dirInode), getegid(), cffs_inode_getGid(dirInode))) {
      cffs_inode_releaseInode (dirInode, 0);
      goto error;
      RETURNCRITICAL (-1);              /* no write permission */
   }

   inode = cffs_inode_getInode ((FILEP_GETINODENUM(tofilp)), tofilp->f_dev, sb_to->blk, (BUFFER_READ|BUFFER_WITM));
   assert (inode);

#if 0	/* GROK -- disallow hard links to directories */
   if ((cffs_inode_isDir(inode)) && (geteuid() != 0)) {
      cffs_inode_releaseInode (dirInode, 0);
      cffs_inode_releaseInode (inode, 0);
      errno = EPERM;
      goto error;
   }
#endif

   /* add new directory entry for the link */

   /* XXX -- is this the right superblock? */
   error = 0;
   ret = cffs_path_addLink (sb_buf_dir, dirInode, name, strlen(name), inode, 0, &error);
   if (error) {
      cffs_inode_releaseInode (inode, 0);
      cffs_inode_releaseInode (dirInode, 0);
      goto error;
   }

   if (ret == 0) {
      cffs_inode_setModTime (inode, time(NULL));
      cffs_inode_releaseInode (inode, BUFFER_DIRTY);
      cffs_inode_setModTime (dirInode, time(NULL));
      cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);

   } else {
      cffs_inode_releaseInode (inode, 0);
      cffs_inode_releaseInode (dirInode, 0);
      errno = EEXIST;
      goto error;
   }
   cffs_buffer_releaseBlock (sb_buf_to, 0);
   cffs_buffer_releaseBlock (sb_buf_dir, 0);
   RETURNCRITICAL (ret);

 error:
   cffs_buffer_releaseBlock (sb_buf_to, 0);
   cffs_buffer_releaseBlock (sb_buf_dir, 0);
   RETURNCRITICAL (-1);
}


/* cffs_symlink

 Create a symbolic link.
 */

int cffs_symlink (struct file *dirp, const char *name, const char *to) 
{
   inode_t *dirInode;
   int ret;
   inode_t *inode;
   int tolen = strlen(to);
   cffs_t *sb;
   buffer_t *sb_buf;
   int error;

DPRINTF (1, ("%d: cffs_symlink: %s --> %s\n", getpid(), name, to));

   if (strlen(name) > CFFS_EMBDIR_MAX_FILENAME_LENGTH) {
      errno = ENAMETOOLONG;
      return (-1);
   }

   CFFS_CHECK_SETUP (dirp->f_dev);

   ENTERCRITICAL

   demand (dirp, bogus dirp);
   sb = cffs_proc_getSuperblock (dirp->f_dev, FILEP_GETSUPERBLOCK(dirp), &sb_buf);
   dirInode = cffs_inode_getInode ((FILEP_GETINODENUM(dirp)), dirp->f_dev, sb->blk, (BUFFER_READ|BUFFER_WITM));
   assert (dirInode);

   if (!cffs_inode_isDir(dirInode)) {
      errno = ENOTDIR;
      goto error;
   }

   /* verify that we can write to the parent directory. */

   if (!verifyWritePermission (cffs_inode_getMask(dirInode), geteuid(), cffs_inode_getUid(dirInode), getegid(), cffs_inode_getGid(dirInode))) {
     errno = EACCES;
     goto error;
   }

   /* add new directory entry for the symbolic link */

   inode = NULL;
   error = 0;
   ret = cffs_path_makeNewFile (sb_buf, dirInode, name, strlen(name), &inode, 0, 0, S_IFLNK, &error);
   if (error || ret != 0) {
     if (ret != 0) {
       errno = EEXIST;
     }
     if (inode) {
       cffs_inode_releaseInode (inode, BUFFER_DIRTY);
     }
     goto error;
   }     

   cffs_fdInitDinode (cffs_inode_dinode(inode), 0777);
   error = 0;
   ret = cffs_writeBuffer (sb->blk, inode, to, (off_t) 0, tolen, &error);
   cffs_inode_releaseInode (inode, BUFFER_DIRTY);
   if (error || ret != tolen) {
     if (ret != tolen) {
       errno = ENOSPC;
     }
     goto error2;
   }

   cffs_inode_setModTime (dirInode, time(NULL));

   cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);
   cffs_buffer_releaseBlock (sb_buf, 0);
   EXITCRITICAL;

   return 0;

 error2:
   ret = cffs_unlink (dirp, name);
   assert (ret == 0);
 error:
   cffs_inode_releaseInode (dirInode, 0);
   cffs_buffer_releaseBlock (sb_buf, 0);
   RETURNCRITICAL (-1);
}


/* cffs_readlink

   Read  a link.
*/

int cffs_readlink (struct file *filp, char *buf, int buflen) {
   inode_t *inode;
   int ret;
   int error;

DPRINTF (1, ("%d: cffs_readlink: buf %p, buflen %d\n", getpid(), buf, buflen));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));

   /* make sure it is a symlink */
   if (!cffs_inode_isSymLink(inode)) {
      cffs_inode_releaseInode (inode, 0);
      errno = EINVAL;
      RETURNCRITICAL (-1);
   }

   /* do the read */
   error = 0;
   ret = cffs_readBuffer (FILEP_GETSUPERBLOCK(filp), inode, buf, (off_t) 0, buflen, &error);
   if (error) {
      cffs_inode_releaseInode (inode, 0);
      RETURNCRITICAL (-1);
   }
   if (ret < 0) {
      cffs_inode_releaseInode (inode, 0);
      errno = -ret;
      RETURNCRITICAL (-1);
   }

   cffs_inode_releaseInode (inode, 0);

   RETURNCRITICAL (ret);
}


int cffs_fsync (struct file *filp)
{
   inode_t *inode;

DPRINTF (1, ("%d: cffs_fsync: filp %p\n", getpid(), filp));

   CFFS_CHECK_SETUP (filp->f_dev);

   ENTERCRITICAL

   demand (filp, bogus filp);
   inode = cffs_inode_getInode ((FILEP_GETINODENUM(filp)), filp->f_dev, FILEP_GETSUPERBLOCK(filp), (BUFFER_READ|BUFFER_WITM));
   assert (inode);

   cffs_buffer_affectFile (inode->fsdev, inode->inodeNum, BUFFER_WRITE, 0);

   cffs_inode_releaseInode (inode, BUFFER_WRITE);

   RETURNCRITICAL (0);
}


void cffs_sync ()
{

  cffs_cache_init ();

   ENTERCRITICAL

	/* This should be made BUFFER_ASYNC!!! */
   cffs_buffer_affectAll (BUFFER_WRITE);

   EXITCRITICAL
}


/* This is totally hacked and should be used at one's own risk... */

int cffs_flush (u_int dev)
{

  CFFS_CHECK_SETUP (dev);

   ENTERCRITICAL

   cffs_inode_flushInodeCache ();
   cffs_buffer_affectAll (BUFFER_WRITE | BUFFER_INVALID);

	/* Also need to flush the bc! */
#ifndef JJ
   sys_bc_flush (dev, BC_SYNC_ANY, BC_SYNC_ANY);
#endif

   RETURNCRITICAL (0);
}

