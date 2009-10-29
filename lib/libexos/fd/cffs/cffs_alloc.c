
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
#include "cffs_dinode.h"
#include "cffs_alloc.h"
#include "cffs_xntypes.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#ifndef JJ
#include <xok/sysinfo.h>
#include <xok/bc.h>
#include <exos/cap.h>
#include <exos/vm-layout.h>
#include <exos/vm.h>
#else
#include "bit.h"
#include "kernel.h"
#endif

/* GROK: current assumptions of this code:
       -- currently assumes only one disk device
       -- assumes block 0 is never used */

int cffs_usegrouping = 1;

#define MAX_GROUP_SIZE	16
/*
#define FORCE_DIRECTORY_COLLOCATION
*/

/* functions to allocate disk locations for inodes and blocks */

/* per-process count of allocated/de-allocated blocks (nice for debugging FS */
int alloced = 0;

block_num_t cffs_alloc_doCollocation (buffer_t *sb_buf, dinode_t *dinode, int blockPtrNum);


block_num_t cffs_alloc_initDisk (buffer_t *sb_buf, off_t size, block_num_t blkno)
{
#ifndef XN
   int i;
   int allocMapBlocks;
   u_int blockcount = (size+BLOCK_SIZE-1) / BLOCK_SIZE;
   char *maps[CFFSD_MAX_INITALLOC_SZ];
   buffer_t *buffers[CFFSD_MAX_INITALLOC_SZ];
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

/*
printf ("cffs_alloc_initDisk: size %qd, blkno %d, blockcount %d\n", size, blkno, blockcount);
*/
   allocMapBlocks = (roundup (blockcount, BLOCK_SIZE)) / BLOCK_SIZE;
   assert ((blkno + allocMapBlocks) < BLOCK_SIZE);
   assert (allocMapBlocks <= CFFSD_MAX_INITALLOC_SZ);

   for (i=0; i<allocMapBlocks; i++) {
      buffers[i] = cffs_buffer_getBlock (sb->fsdev, 0, 1, (blkno+i), (BUFFER_READ|BUFFER_WITM), NULL);
      maps[i] = buffers[i]->buffer;
   }

   
#ifdef CFFS_PROTECTED
   {
#ifdef CFFSD
	int ret = cffsd_fsupdate_initAllocMap (maps, allocMapBlocks, blkno+allocMapBlocks, sb->fsdev);
#else
	assert (0);
	int ret = sys_fsupdate_initAllocMap (buffer->buffer, blkno+allocMapBlocks);
#endif /* CFFSD */
	assert (ret == 0);
   }
#else /* CFFS_PROTECTED */
   for (i=0; i<allocMapBlocks; i++) {
     assert (0);
      bzero (maps[i], BLOCK_SIZE);
      if (i == 0) {
         int j;
         for (j=0; j<(blkno+allocMapBlocks); j++) {
            buffer->buffer[j] = (char) 1;
         }
      }
   }
#endif /* CFFS_PROTECTED */

   for (i=0; i<allocMapBlocks; i++) {      
      cffs_buffer_releaseBlock (buffers[i], BUFFER_DIRTY);
   }

   cffs_superblock_setNumblocks (sb_buf, blockcount);
   cffs_superblock_setNumalloced (sb_buf, (blkno + allocMapBlocks));
   cffs_buffer_dirtyBlock (sb_buf);

   return (blkno + allocMapBlocks);
#else
   assert (0);
   return (blkno);
#endif
}


int cffs_alloc_allocBlock (buffer_t *sb_buf, block_num_t block)
{
#if !defined(XN) && !defined(CFFS_PROTECTED)
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
   int allocMapBlkno = sb->allocMap + (block / BLOCK_SIZE);
   u_int blockcount = sb->numblocks;
   buffer_t *buffer;

   if ((block == 0) || (block >= blockcount)) {
      printf ("block %d, blockcount %d, alloced %d\n", block, blockcount, alloced);
   }
   assert (0 < block && block < blockcount);

   buffer = cffs_buffer_getBlock (sb->fsdev, 0, 1, allocMapBlkno, (BUFFER_READ|BUFFER_WITM), NULL);
   assert ((buffer) && (buffer->buffer));

   block = block % BLOCK_SIZE;

   if (buffer->buffer[block]) {
      cffs_buffer_releaseBlock (buffer, 0);
      return (-1);
   }
/*
printf ("cffs_alloc_allocBlock: block %d\n", block);
*/
   buffer->buffer[block] = (char) 1;
   alloced++;
   sb->numalloced++;
   cffs_buffer_dirtyBlock (sb_buf);

   cffs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

#else
   assert (0);
#endif
   return (0);
}


int cffs_alloc_deallocBlock (buffer_t *sb_buf, block_num_t block)
{
#if !defined(XN) && !defined(CFFS_PROTECTED)
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
   int allocMapBlkno = sb->allocMap + (block / BLOCK_SIZE);
   u_int blockcount = sb->numblocks;
   buffer_t *buffer;
   block_num_t origblock = block;

   if ((block == 0) || (block >= blockcount)) {
      printf ("cffs_alloc_deallocBlock: bad block %d, blockcount %d\n", block, blockcount);
   }
   assert (0 < block && block < blockcount);

   buffer = cffs_buffer_getBlock (sb->fsdev, 0, 1, allocMapBlkno, (BUFFER_READ|BUFFER_WITM), NULL);
   assert ((buffer) && (buffer->buffer));

   block = block % BLOCK_SIZE;

   if (buffer->buffer[block] == (char) 0) {
printf ("cffs_alloc_deallocBlock: block %d already deallocated\n", block);
      cffs_buffer_releaseBlock (buffer, 0);
      return (-1);
   }

	/* reset the dirty bit on the cached block -- no reason to write it */
   sys_bc_set_dirty (sb->fsdev, origblock, 0);

   buffer->buffer[block] = (char) 0;
   alloced--;
   sb->numalloced--;
   cffs_buffer_dirtyBlock (sb_buf);

   cffs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

#else
   assert (0);
#endif
   return (0);
}


#ifndef XN
int cffs_alloc_protWrapper (int action, buffer_t *sb_buffer, struct dinode *dinode, u_int *ptrloc, u_int blkno)
{
#ifndef CFFS_PROTECTED
   assert (0);
   return (0);
#else
   u_int block = (blkno) ? blkno : *ptrloc;
   int allocMapBlkno;
   u_int blockcount;
   buffer_t *alloc_buffer;
   cffs_t *sb;
   int ret;

   /*
    * Get the superblock and associated allocation info for the
    * specified device.
    *
    */

   sb = (cffs_t *)sb_buffer->buffer;
   allocMapBlkno = sb->allocMap + (block / BLOCK_SIZE);
   blockcount = sb->numblocks;

   /*
    * A little error checking for good measure...
    *
    */

   if ((block == 0) || (block >= blockcount)) {
      printf ("cffs_alloc_protWrapper: bad block %d, blockcount %d\n", block, blockcount);
   }
   assert (0 < block && block < blockcount);

   /*
    * Read in the block of the allocation map that we're going to update.
    *
    */

   alloc_buffer = cffs_buffer_getBlock (sb->fsdev, 0, 1, allocMapBlkno, (BUFFER_READ|BUFFER_WITM), NULL);
   assert ((alloc_buffer) && (alloc_buffer->buffer));

   /* 
    * And set the index in the allocation map to "allocated".
    *
    */

#ifdef CFFSD
   ret = cffsd_fsupdate_setptr (action, sb, dinode, alloc_buffer->buffer, ptrloc, blkno);
#else   
   ret = sys_fsupdate_setptr (action, sb, dinode, alloc_buffer->buffer, ptrloc, blkno);
#endif /* CFFSD */

   if (ret == -E_QUOTA) {
     errno = EDQUOT;
     goto error;
   }

   /* 
    * Keep track of how many blocks are allocated in this filesystem,
    *
    */

   if (ret == 0) {
      if (action == CFFS_SETPTR_ALLOCATE) {
         alloced++;
         cffs_superblock_setNumalloced (sb_buffer, (sb->numalloced+1));
      } else if (action == CFFS_SETPTR_DEALLOCATE) {
         alloced--;
         cffs_superblock_setNumalloced (sb_buffer, (sb->numalloced-1));
      }
   }

   /* 
    * Release the superblock and allocation map blocks, marking them
    * dirty if we managed to update the allocation map.
    *
    */

 error:
   if (ret == 0) {
     cffs_buffer_dirtyBlock (sb_buffer);
   }
   cffs_buffer_releaseBlock (alloc_buffer, ret == 0 ? BUFFER_DIRTY : 0);

   return (ret);
#endif /* CFFS_PROTECTED */
}
#endif /* XN */


/* cffs_allocBlock

   allocate a free block and put it on the specified protection list.

*/

block_num_t cffs_allocBlock (buffer_t *sb_buf, dinode_t *dinode, int blockPtrNum, int startsearch) {
     cffs_t *sb = (cffs_t *)sb_buf->buffer;
     block_num_t block;
     block_num_t blockcount = sb->numblocks;
     int ret;

     if ((block = cffs_alloc_doCollocation (sb_buf, dinode, blockPtrNum)) != 0) {
        return (block);
     } else if (startsearch != -1) {
        if ((blockPtrNum == 1) && (cffs_usegrouping)) {
           startsearch = (startsearch + MAX_GROUP_SIZE) & ~((unsigned int)(MAX_GROUP_SIZE-1));
        }
        startsearch = startsearch % blockcount;
     } else {
        startsearch = (unsigned int) random();
        if (cffs_usegrouping) {
           startsearch = (startsearch + MAX_GROUP_SIZE) & ~((unsigned int)(MAX_GROUP_SIZE-1));
        }
        startsearch = startsearch % blockcount;
     }

     if (startsearch < 0) {
        kprintf("cffs_allocBlock: startsearch = %d\n", startsearch);
     }

     /* checking the free block map and allocating a block is not */
     /* atomic so we keep trying until we get a block             */

     do {
        block = cffs_findFree (sb_buf, startsearch);
#if !defined(XN) && !defined(CFFS_PROTECTED)
        ret = cffs_alloc_allocBlock(sb_buf, block);
if (ret == -1) {
kprintf ("cffs_findFree: startsearch %d, block %d, ret %d\n", startsearch, block, ret);
}
#else
        ret = 0;
#endif
     } while (ret == -1);
     assert (ret == 0);

     return (block);
}


uint cffs_alloc_externalNumBlocks (uint dev)
{
   buffer_t *sb_buffer;
   cffs_t *sb;
   int count;

   sb_buffer = cffs_get_superblock (dev, CFFS_SUPERBLKNO, 0);  
   sb = (cffs_t *)sb_buffer->buffer;
   count = sb->numblocks;
   cffs_buffer_releaseBlock (sb_buffer, 0);
   return (count);
}


int cffs_alloc_externalAlloc (uint dev, uint block)
{
   int ret = 0;
#ifdef XN
   ret = sys_xn_alloc_cheat (0, cffs_inode_op (0, 0, block, 0, 0), 0);
   if (! ((ret == 0) || (ret == XN_CANNOT_ALLOC)) ) {
      kprintf ("cffs_alloc_externalAlloc: xn_alloc failed with ret = %d\n", ret);
   }
   assert ((ret == 0) || (ret == XN_CANNOT_ALLOC));
   ret = (ret) ? -1 : 0;
#elif defined(CFFS_PROTECTED)
   {
     int micropart;
     static unsigned char alloced_microparts[MAX_DISKS][1024] = {{0}}; /* XXX -- 1024!?!? */
     buffer_t *sb_buf;

     /*
      * Get the default superblock for this filesystem.
      * We assume that with external allocation, we only
      * use a single superblock.
      *
      */

     sb_buf = cffs_get_superblock (dev, CFFS_SUPERBLKNO, 0);

     /*
      * We need to allocate the micropartition "block" is in if we
      * haven't already done so (though alloced_microparts
      * is just a conservative hint--if it says we have already
      * allocated the micropart then we really have, but if it says
      * we haven't, someone else may still have allocated it).
      *
      */

     micropart = block/MICROPART_SZ_IN_PAGES;
     if (!alloced_microparts[dev][micropart]) {

       /*
	* No need to check the return value of this call. 
	* Either it succeeds and we get the micropart or
	* it fails because it's already allocated.
	*
	*/

       cffs_allocMicropart (dev, sb_buf, micropart);
       alloced_microparts[dev][micropart] = 1;
     }

     /*
      * Do the allocation.
      *
      */

     ret = cffs_alloc_protWrapper (CFFS_SETPTR_EXTERNALALLOC, sb_buf, NULL, NULL, block);

     /* 
      * And we're done with the superblock now
      *
      */

     cffs_buffer_releaseBlock (sb_buf, 0);
   }
#else /* !def CFFS_PROTECTED */
   {
     buffer_t *sb_buf = cffs_get_superblock (dev, 0);
     ret = cffs_alloc_allocBlock (sb_buf, block);
     cffs_buffer_releaseBlock (sb_buf, 0);
   }
#endif /* CFFS_PROTECTED */
   return (ret);
}

int cffs_alloc_externalFree (uint dev, uint block)
{
   int ret = 0;
#ifdef XN
   ret = sys_xn_free_cheat (0, cffs_inode_op (0, block, 0, 0, 0), 0);
   assert (ret == 0);
#elif defined(CFFS_PROTECTED)
   buffer_t *sb_buf;

   sb_buf = cffs_get_superblock (dev, CFFS_SUPERBLKNO, 0);
   ret = cffs_alloc_protWrapper (CFFS_SETPTR_EXTERNALFREE, sb_buf, NULL, NULL, block);
   cffs_buffer_releaseBlock (sb_buf, 0);

#else /* !def CFFS_PROTECTED */
   {
     buffer_t *sb_buf = cffs_get_superblock (dev, 0);
     ret = cffs_alloc_deallocBlock (sb_buf, block);
     cffs_buffer_releaseBlock (sb_buf, 0);
   }
#endif /* CFFS_PROTECTED */
   return (ret);
}

int cffs_allocMicropart (u_int32_t dev, buffer_t *sb_buf, int micropart) {
  int blk;
  int i;
  char *maps[CFFSD_MAX_ALLOCMICROPART_SZ];
  buffer_t *buffer[CFFSD_MAX_ALLOCMICROPART_SZ];
  int ret;
  static int last_micropart = 0;
  int target;
  cffs_t *sb;

  sb = (cffs_t *)sb_buf->buffer;

  /* figure out the micropartition that would hold this
     block and allocate it */

 try_again:
  if (micropart)
    target = micropart;
  else
    target = last_micropart++;

  blk = sb->allocMap + (target * MICROPART_SZ_IN_PAGES)/BLOCK_SIZE;

  for (i = 0; i < FREEMAP_PAGES_PER_MICROPART; i++) {
    buffer[i] = cffs_buffer_getBlock (sb->fsdev, 0, 1, blk+i, BUFFER_READ|BUFFER_WITM, NULL);
    maps[i] = buffer[i]->buffer;
  }

  ret = cffsd_fsupdate_allocMicropart (maps, FREEMAP_PAGES_PER_MICROPART, sb->fsdev, target);
  if (ret < 0) {
    for (i = 0; i < FREEMAP_PAGES_PER_MICROPART; i++) {
      cffs_buffer_releaseBlock (buffer[i], 0);
    }
    if (ret == -E_EXISTS && !micropart) goto try_again;
    else return -1;
  }

  for (i = 0; i < FREEMAP_PAGES_PER_MICROPART; i++) {
    cffs_buffer_releaseBlock (buffer[i], BUFFER_DIRTY);
  }

  return 0;
}

/* cffs_findFree

   get a free block to allocate. Pretty stupid right now--just scans
   through exo-disk's allocationMap looking for a free entry.

*/

block_num_t cffs_findFree (buffer_t *sb_buf, block_num_t startsearch) {
  cffs_t *sb = (cffs_t *)sb_buf->buffer;
#ifdef XN
   int blk;

   assert (startsearch < sb->numblocks);
   /* XXX -- never allocate block 0, handled by keeping the bit set */
   blk = Bit_run(free_map, startsearch, 1);
   if (blk != -1) {
      if (!((blk > 0) && ((blk*(4096/512)) < __sysinfo.si_disks[sb->fsdev].d_size))) {
         kprintf ("bad result from Bit_run: blk %d, startsearch %d, fs->nblks %d, sys->nblks %d\n", blk, startsearch, sb->numblocks, (__sysinfo.si_disks[sb->fsdev].d_size / 8));
      }
      assert ((blk > 0) && ((blk*(4096/512)) < __sysinfo.si_disks[sb->fsdev].d_size));
      return (blk);
   }

   assert ("out of blocks" == 0);              /* out of blocks */
   return (0);         /* stop warning about not returning value */

#else
   u_int index;
   buffer_t *buffer;

 try_again:
   index = startsearch;
   buffer = NULL;

   assert (index < sb->numblocks);
   /* XXX -- never allocate block 0, handled by keeping the bit set */
   do {
      if ((buffer == NULL) || ((index % BLOCK_SIZE) == 0)) {
         int blkno = sb->allocMap + (index / BLOCK_SIZE);
         if (buffer) {
            cffs_buffer_releaseBlock (buffer, 0);
         }
         buffer = cffs_buffer_getBlock (sb->fsdev, 0, 1, blkno, BUFFER_READ, NULL);
      }
      if (buffer->buffer[(index % BLOCK_SIZE)] == CFFS_ALLOCMAP_FREE) {
         cffs_buffer_releaseBlock (buffer, 0);
         return (index);
      }
      index++;
      if (index >= sb->numblocks) {
         index = 0;
      }
   } while (index != startsearch);

   if (cffs_allocMicropart (sb->fsdev, sb_buf, 0) == -1) {
     assert ("out of blocks" == 0);
     return (0);
   }
   goto try_again;
#endif

}


block_num_t cffs_alloc_boundedAlloc (buffer_t *sb_buf, int startsearch, int maxcnt)
{
  cffs_t *sb = (cffs_t *)sb_buf->buffer;
#ifdef XN
   int blk = Bit_run_bounded(free_map, startsearch, startsearch+maxcnt-1, 1);
   if (blk != -1) {
      assert ((blk > 0) && ((blk*(4096/512)) < __sysinfo.si_disks[sb->fsdev].d_size));
      return blk;
   }
#else
   int lowbound = max (1, startsearch);
   int upbound = min (sb->numblocks, (startsearch + maxcnt));
   int i;
   buffer_t *buffer = NULL;

   for (i=lowbound; i<upbound; i++) {
      if ((buffer == NULL) || ((i % BLOCK_SIZE) == 0)) {
         int blkno = sb->allocMap + (i / BLOCK_SIZE);
         if (buffer) {
            cffs_buffer_releaseBlock (buffer, 0);
         }
         buffer = cffs_buffer_getBlock (sb->fsdev, 0, 1, blkno, BUFFER_READ, NULL);
      }
      if (buffer->buffer[(i%BLOCK_SIZE)] == 0) {
#ifdef CFFS_PROTECTED
         if (1) {
#else
         if (cffs_alloc_allocBlock(sb_buf, i) != -1) {
#endif
            cffs_buffer_releaseBlock (buffer, 0);
            return (i);
         }
      }
   }
#endif

   return (0);
}


#if 1

#include "cffs_inode.h"
#include "cffs_embdir.h"


int cffs_alloc_scandDir (buffer_t *sb_buf, dinode_t *dirDInode, int (*func)(dinode_t *, buffer_t *, u_int, u_int, u_int), u_int param1, u_int param2, u_int param3)
{
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   buffer_t *dirBlockBuffer;
   buffer_t *dinodeBlockBuffer;
   char *dirBlock;
   dinode_t *dinode;
   int ret;
   int i, j;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   assert (dirDInode);

   /* search through the directory looking for group members */

   while (currPos < cffs_dinode_getLength(dirDInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirDInode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = min((cffs_dinode_getLength(dirDInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, -dirDInode->dinodeNum, physicalDirBlockNumber, BUFFER_READ, NULL);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (i=0; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
         for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
            dirent = (embdirent_t *) (dirBlock + i + j);
            if (dirent->type == (char)0) {
               continue;
            }
            dinode = NULL;
            dinodeBlockBuffer = NULL;
            if (cffs_inode_blkno(dirent->inodeNum) == dirBlockBuffer->header.diskBlock) {
               dinode = (dinode_t *) (cffs_inode_offsetInBlock(dirent->inodeNum) + dirBlockBuffer->buffer);
            } else if ((cffs_embedinodes == 0) && ((dirent->nameLen != 1) || (dirent->name[0] != '.')) && ((dirent->nameLen != 2) || (dirent->name[0] != '.') || (dirent->name[1] != '.'))) {
               dinode = cffs_dinode_getDInode (dirent->inodeNum, sb->fsdev, (BUFFER_READ|BUFFER_WITM), &dinodeBlockBuffer);
            }
            ret = 0;
            if (dinode) {
               ret = func (dinode, ((dinodeBlockBuffer) ? dinodeBlockBuffer : dirBlockBuffer), param1, param2, param3);
            }
            if (dinodeBlockBuffer) {
               cffs_dinode_releaseDInode (dinode, sb_buf, 0, dinodeBlockBuffer);
            }
            if (ret != 0) {
               cffs_buffer_releaseBlock (dirBlockBuffer, 0);
               return (ret);
            }
         }
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
   return (0);
}


static int setgroupinfo (dinode_t *dinode, buffer_t *buffer, u_int param1, u_int param2, u_int param3)
{
   int groupstart = param1;
   int newgroupsize = param2;

   if (dinode->groupstart == groupstart) {
      cffs_dinode_setGroupsize (dinode, newgroupsize);
      cffs_buffer_dirtyBlock (buffer);
   }

   return (0);
}


#ifdef FORCE_DIRECTORY_COLLOCATION
static int getgroupmember (dinode_t *dinode, buffer_t *buffer, u_int param1, u_int param2, u_int param3)
{
   int groupstart = param1;
   buffer_t **bufP = (buffer_t **) param2;

   if (dinode->groupstart == groupstart) {
      assert (bufP);
      *bufP = buffer;
      cffs_buffer_bumpRefCount (buffer);
      return ((int) dinode);
   }

   return (0);
}
#endif


static int cffs_alloc_notchecked (int groupstart, int numchecked, int *checked)
{
   int i;
   for (i=0; i<numchecked; i++) {
      if (checked[i] == groupstart) {
         return (0);
      }
   }
   return (1);
}


static int finduniquegroupinfo (dinode_t *dinode, buffer_t *buffer, u_int param1, u_int param2, u_int param3)
{
   int numchecked = param1;
   int *checked = (int *) param2;
   int *groupsizeP = (int *) param3;

   if ((dinode->groupsize != 0) && (cffs_alloc_notchecked (dinode->groupstart, numchecked, checked))) {
      assert (dinode->groupstart != 0);
      *groupsizeP = dinode->groupsize;
      return (dinode->groupstart);
   }

   return (0);
}


#define cffs_alloc_setgroupinfo(a,b,c,d) \
	cffs_alloc_scandDir (a, b, setgroupinfo, (u_int) c, (u_int) d, 0)

#define cffs_alloc_getgroupmember(a,b,c,d) \
	cffs_alloc_scandDir (a, b, getgroupmember, (u_int) c, (u_int) d, 0)

#define cffs_alloc_finduniquegroupinfo(a,b,c,d,e) \
	cffs_alloc_scandDir (a, b, finduniquegroupinfo, (u_int) c, (u_int) d, (u_int) e)


int cffs_alloc_putingroup (buffer_t *sb_buf, dinode_t *dirDInode, dinode_t *dinode)
{
   int numchecked = 1;
   int checked[16];
   int groupstart;
   int groupsize;
   int block;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   checked[0] = dirDInode->groupstart;
   while ((numchecked < 16) && ((groupstart = cffs_alloc_finduniquegroupinfo (sb_buf, dirDInode, numchecked, checked, &groupsize)) != 0)) {
      block = cffs_alloc_boundedAlloc (sb_buf, groupstart, MAX_GROUP_SIZE);
      if (block) {
         cffs_dinode_setGroupstart (dinode, groupstart);
         cffs_dinode_setGroupsize (dinode, groupsize);
         if (block >= (groupstart + groupsize)) {
            cffs_dinode_setGroupsize (dinode, (block - groupstart + 1));
            assert (groupsize <= MAX_GROUP_SIZE);
            cffs_alloc_setgroupinfo (sb_buf, dirDInode, groupstart, dinode->groupsize);
         }
/*
printf ("allocating into subsequent group: groupstart %d, groupsize %d\n", groupstart, groupsize);
*/
         return (block);
      } else {
         checked[numchecked] = groupstart;
         numchecked++;
/*
         printf ("group alloc 2 failed: groupstart %d, groupsize %d, block %d\n", dirDInode->groupstart, dirDInode->groupsize, block);
*/
      }
   }

   /* start a new group */
   block = (unsigned int) random() % sb->numblocks;
   block = (block + MAX_GROUP_SIZE) & ~((unsigned int)(MAX_GROUP_SIZE-1));
   block = block % sb->numblocks;
   block = cffs_findFree (sb_buf, block);
#if !defined(XN) && !defined(CFFS_PROTECTED)
   {int ret = cffs_alloc_allocBlock (sb_buf, block);
   assert (ret != -1);
   }
#endif
   cffs_dinode_setGroupstart (dinode, block);
   cffs_dinode_setGroupsize (dinode, 1);
   return (block);
}


block_num_t cffs_alloc_doCollocation (buffer_t *sb_buf, dinode_t *dinode, int blockPtrNum)
{
   block_num_t block = 0;
   inode_t *dirInode = NULL;
   dinode_t *dirDInode;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   if (cffs_usegrouping == 0) {
      return (0);
   }

#ifdef FORCE_DIRECTORY_COLLOCATION
   if ((blockPtrNum < 0) || ((blockPtrNum == 0) && ((cffs_dinode_isDir(dinode)) || (cffs_dinode_getDirNum(dinode) == 0) || (dinode->directBlocks[0] != 0))) || ((blockPtrNum > 0) && (!cffs_dinode_isDir(dinode)))) {
#else
   if ((blockPtrNum < 0) || ((blockPtrNum == 0) && ((cffs_dinode_isDir(dinode)) || (cffs_dinode_getDirNum(dinode) == 0) || (dinode->directBlocks[0] != 0))) || (blockPtrNum > 0)) {
#endif
	/* GROK -- if this assert is true, why checking it in if statement?? */
      assert ((blockPtrNum != 0) || (dinode->directBlocks[0] == 0));
      return (0);
   }

   if (cffs_dinode_isDir(dinode)) {
      dirDInode = dinode;
   } else {
      dirInode = cffs_inode_getInode (cffs_dinode_getDirNum(dinode), sb->fsdev, sb->blk, (BUFFER_READ|BUFFER_WITM));
      dirDInode = dirInode->dinode;
   }

   if (dirDInode->groupstart == 0) {
      assert (cffs_dinode_sanityOK(dirDInode));
      assert (dirDInode->groupsize == 0);
      assert (dirDInode->directBlocks[0] != 0);

      cffs_dinode_setGroupstart (dirDInode, dirDInode->directBlocks[0]);
      cffs_dinode_setGroupsize (dirDInode, 1);
   }

   block = cffs_alloc_boundedAlloc (sb_buf, dirDInode->groupstart, MAX_GROUP_SIZE);
   if (block) {
      cffs_dinode_setGroupstart (dinode, dirDInode->groupstart);
      cffs_dinode_setGroupsize (dinode, dirDInode->groupsize);
      if (block >= (dirDInode->groupstart + dirDInode->groupsize)) {
         cffs_dinode_setGroupsize (dirDInode, (block - dirDInode->groupstart + 1));
         assert (dirDInode->groupsize <= MAX_GROUP_SIZE);
         cffs_alloc_setgroupinfo (sb_buf, dirDInode, dirDInode->groupstart, dirDInode->groupsize);
      }
      if (dirInode) {
         cffs_inode_releaseInode (dirInode, BUFFER_DIRTY);
      }
      return (block);
/*
   } else {
      printf ("1st group alloc failed: groupstart %d, groupsize %d, block %d\n", dirDInode->groupstart, dirDInode->groupsize, block);
*/
   }

   /* GROK -- first cut only.  */

#ifdef FORCE_DIRECTORY_COLLOCATION
   if (cffs_dinode_isDir(dinode)) {
      buffer_t *buffer;
      dinode_t *movee;
      assert (dirDInode == dinode);
      if ((movee = (dinode_t *) cffs_alloc_getgroupmember (sb_buf, dirDInode, dirDInode->groupstart, &buffer)) != NULL) {
         int ret;
         int newblock;
         struct Xn_xtnt src, dst;
         buffer_t *movebuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, sb->movee->dinodeNum, 0, BUFFER_READ, NULL);
         assert (!cffs_dinode_isDir(movee));
         assert (movee != dinode);
         assert (buffer);
         assert (dinode->groupstart == movee->groupstart);

         movee->groupstart = 0;
         movee->groupsize = 0;
         block = movee->directBlocks[0];
         newblock = cffs_alloc_putingroup (sb_buf, dirDInode, movee);
         assert (newblock > 0);
/*
printf ("make space for directory block: move one from %d to %d\n", block, newblock);
*/
         movebuffer->header.diskBlock = newblock;
         movebuffer->buf.b_blkno = newblock * (BLOCK_SIZE/512);
         movee->directBlocks[0] = newblock;

         src.xtnt_block = block;
         src.xtnt_size = 1;
         dst.xtnt_block = newblock;
         dst.xtnt_size = 1;
         if ((ret = sys_bc_move (&__sysinfo.si_pxn[movebuffer->bc_entry->buf_dev], CAP_ROOT, 0, &src, &__sysinfo.si_pxn[movebuffer->bc_entry->buf_dev], CAP_ROOT, 0, &dst, 1)) != 0) {
            kprintf ("cffs_alloc_doCollocation: bc_move failed (ret %d, block %d, newblock %d)\n", ret, block, newblock);
            assert (0);
         }
         assert (movebuffer->header.diskBlock == movebuffer->bc_entry->buf_blk);
         cffs_buffer_releaseBlock (movebuffer, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));
		/* GROK - making assumption about how external dinodes are released !! */
         cffs_buffer_releaseBlock (buffer, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));
         assert (dirInode == NULL);
      }
/*
      printf ("Likely to allocate non-contiguous directory block!!\n");
      printf ("groupstart %d, groupsize %d, length %d\n", dinode->groupstart, dinode->groupsize, dinode->length);
*/
   } else {
      block = cffs_alloc_putingroup (sb_buf, dirDInode, dinode);
   }
#else
   block = cffs_alloc_putingroup (sb_buf, dirDInode, dinode);
#endif

   if (dirInode) {
      cffs_inode_releaseInode (dirInode, 0);
   }

   return (block);
}
#else
block_num_t cffs_alloc_doCollocation (block_num_t fsdev, dinode_t *dinode, int blockPtrNum)
{
   return (0);
}
#endif

