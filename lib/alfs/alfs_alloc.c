
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

/* ALFS -- Application-Level Filesystem */

#include "alfs/alfs_buffer.h"
#include "alfs/alfs.h"
#include "alfs/alfs_dinode.h"
#include "alfs/alfs_alloc.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "alfs/alfs_diskio.h"
#include "alfs/alfs_defaultcache.h"

#include <xok/disk.h>

#include <fd/cffs/cffs_alloc.h>
#include <fd/cffs/cffs_proc.h>

/* XXX -- from fd/cffs/cffs.h */
#define CFFS_SUPERBLKNO 100

#define ALLOC_VIA_CFFS

#define MAX_GROUP_SIZE	16

#ifndef roundup
#define roundup(x,n)	(((x) + ((n)-1)) & (~((n)-1)))
#endif

/* functions to allocate disk locations for inodes and blocks */

char *alfs_allocationMap = NULL;
int alfs_blockcount = 0;
int alfs_alloced = 0;

#ifdef ALLOC_VIA_CFFS
static uint initmetablks_start = 0;
static uint initmetablks_end = 0;
int cffs_mountFS (int devno, int firsttime);
int cffs_unmountFS (int dev, int global_unmount);
#endif

/* XXX -- currently assumes only one disk device
       -- assumes block 0 is never used */

int alfs_safe_allocateBlock (uint fsdev, dinode_t *dinode, int *indirectPtrs, int blockType, int blockPtrNum, int block);

block_num_t alfs_alloc_doCollocation (block_num_t fsdev, dinode_t *dinode, int blockPtrNum);


int alfs_alloc_initmetablks (int size)
{
   return ((((size+BLOCK_SIZE-1)/BLOCK_SIZE)+BLOCK_SIZE-1)/BLOCK_SIZE);
}


block_num_t alfs_alloc_initDisk (cap2_t fscap, block_num_t fsdev, int size, int initmetablks)
{
#ifdef ALLOC_VIA_CFFS
   uint nextblk = 1;
   uint prevblk = 0;
   uint maxstart = 0;
   int count = 0;

   CFFS_CHECK_SETUP (fsdev);

   alfs_blockcount = cffs_alloc_externalNumBlocks (fsdev);
   alfs_superblock->numblocks = (size+BLOCK_SIZE-1) / BLOCK_SIZE;
   alfs_superblock->freeblks = alfs_superblock->numblocks - initmetablks;

   while ((nextblk = alfs_findFree(fsdev, nextblk)) > maxstart) {
      int ret = alfs_alloc_allocBlock (fsdev, nextblk);
      assert (ret == 0);
      count++;
      if ((count != 1) && (nextblk != (prevblk+1))) {
         int i;
         for (i=0; i<(count-1); i++) {
            ret = alfs_alloc_deallocBlock (fsdev, (prevblk-i));
            assert (ret == 0);
         }
         count = 0;
         continue;
      }
      prevblk = nextblk;
      if (count == initmetablks) {
         break;
      }
   }
   initmetablks_start = nextblk - count + 1;
   initmetablks_end = nextblk;
//printf ("initmetablks_start %d, initmetablks_end %d\n", initmetablks_start, initmetablks_end);
   return (initmetablks_start);
#else
   int i;
   int blkno = 2;
   int blks = alfs_alloc_initmetablks (size);

   alfs_blockcount = (size+BLOCK_SIZE-1) / BLOCK_SIZE;
   alfs_allocationMap = (char *) malloc_and_alloc ( roundup (alfs_blockcount, BLOCK_SIZE));
   bzero (alfs_allocationMap, roundup (alfs_blockcount, BLOCK_SIZE));

   assert ((blkno + blks) < BLOCK_SIZE);
   for (i=0; i<(blkno+blks); i++) {
      alfs_allocationMap[i] = (char) 1;
   }

  /* It will actually get written to disk at the unmount that must be called  */

   return (blkno - 1);
#endif
}


int alfs_alloc_uninitDisk (cap2_t fscap, block_num_t fsdev, block_num_t blkno, int initmetablks)
{
#ifdef ALLOC_VIA_CFFS
   int ret;
   int i;
/*
printf ("alfs_alloc_uninitDisk: devno %d, blkno %d, initmetablks %d\n", fsdev, blkno, initmetablks);
*/
   for (i=0; i<initmetablks; i++) {
      ret = alfs_alloc_deallocBlock (fsdev, (blkno+i));
      assert (ret == 0);
   }
#endif
   return (0);
}


block_num_t alfs_alloc_mountDisk (cap2_t fscap, block_num_t fsdev, block_num_t blkno, int size)
{
#ifdef ALLOC_VIA_CFFS

   CFFS_CHECK_SETUP (fsdev);

   alfs_blockcount = cffs_alloc_externalNumBlocks (fsdev);

   assert (((alfs_superblock->numblocks+BLOCK_SIZE-1)/BLOCK_SIZE) == alfs_alloc_initmetablks (size));
   return (blkno + ((((size+BLOCK_SIZE-1) / BLOCK_SIZE)+BLOCK_SIZE-1)/BLOCK_SIZE));
#else
   struct buf buf;
   int i;

   alfs_blockcount = (size+BLOCK_SIZE-1) / BLOCK_SIZE;
   alfs_allocationMap = (char *) malloc_and_alloc ( roundup (alfs_blockcount, BLOCK_SIZE));

   for (i=0; i<((alfs_blockcount+BLOCK_SIZE-1)/BLOCK_SIZE); i++) {
      ALFS_DISK_READ (&buf, &alfs_allocationMap[(i*BLOCK_SIZE)], fsdev, (blkno+i));
   }

   return (blkno + ((alfs_blockcount+BLOCK_SIZE-1)/BLOCK_SIZE));
#endif
}


block_num_t alfs_alloc_unmountDisk (cap2_t fscap, block_num_t fsdev, block_num_t blkno)
{
#ifdef ALLOC_VIA_CFFS
   return (blkno + ((((alfs_superblock->size+BLOCK_SIZE-1) / BLOCK_SIZE)+BLOCK_SIZE-1)/BLOCK_SIZE));
#else
   struct buf buf;
   int i;

   for (i=0; i<((alfs_blockcount+BLOCK_SIZE-1)/BLOCK_SIZE); i++) {
      ALFS_DISK_WRITE (&buf, &alfs_allocationMap[(i*BLOCK_SIZE)], fsdev, (blkno+i));
   }

   free (alfs_allocationMap);
   i = blkno + ((alfs_blockcount+BLOCK_SIZE-1)/BLOCK_SIZE);
   alfs_blockcount = 0;

   return (i);
#endif
}


int alfs_alloc_allocBlock (uint fsdev, block_num_t block)
{
/*
printf ("allocBlock %d\n", block);
*/
#ifdef ALLOC_VIA_CFFS
   {
   int ret;
   if (alfs_superblock->freeblks <= 0) {
      printf ("alfs_alloc_allocBlock: File System Full!!\n");
      return (-1);
   }
   if ((block >= initmetablks_start) && (block <= initmetablks_end)) {
      return (0);
   }
   ret = cffs_alloc_externalAlloc (fsdev, block);
   if (ret == 0) {
      alfs_alloced++;
      alfs_superblock->freeblks--;
   }
   return (ret);
   }
#else
   if ((block == 0) || (block >= alfs_blockcount)) {
      printf ("block %d, alfs_blockcount %d\n", block, alfs_blockcount);
   }
   assert (0 < block && block < alfs_blockcount);

   if (alfs_allocationMap[block]) {
      return (-1);
   }
   alfs_allocationMap[block] = (char) 1;
   alfs_alloced++;
   alfs_superblock->freeblks--;
   return (0);
#endif
}


int alfs_alloc_deallocBlock (uint fsdev, block_num_t block)
{
/*
printf ("deallocBlock %d\n", block);
*/
#ifdef ALLOC_VIA_CFFS
   {
   int ret = cffs_alloc_externalFree (fsdev, block);
   if (ret == 0) {
      alfs_alloced--;
      alfs_superblock->freeblks++;
   }
   return (ret);
   }
#else
   assert (0 < block && block < alfs_blockcount);
   if (alfs_allocationMap[block] == 0) {
      return (-1);
   }
   alfs_allocationMap[block] = (char) 0;
   alfs_alloced--;
   alfs_superblock->freeblks++;
   return (0);
#endif
}


/* alfs_allocBlock

   allocate a free block and put it on the specified protection list.

*/

/* GROK */
#define ALFS_INODE_DISKADDR(inodeNum) \
		((unsigned int) random() % alfs_blockcount)

block_num_t alfs_allocBlock (block_num_t fsdev, dinode_t *dinode, int *indirectPtrs, int blockType, int blockPtrNum, int allocHint) {
     int startsearch;
     block_num_t block;
     int ret;

     assert ((blockType & ALFS_ALLOC_DIRECT) || (indirectPtrs) || ((blockType & ALFS_ALLOC_INDIRECT) && (!(blockType & ALFS_ALLOC_DATA))));
     if ((indirectPtrs == NULL) && (blockType & ALFS_ALLOC_DATA) && ((block = alfs_alloc_doCollocation (fsdev, dinode, blockPtrNum)) != 0)) {
        return (block);
#ifdef GROUPING
     } else if ((blockPtrNum) && (blockType & ALFS_ALLOC_DATA)) {
        if (blockType & ALFS_ALLOC_DIRECT) {
           startsearch = alfs_dinode_getDirect(dinode, (blockPtrNum-1));
           if (blockPtrNum == 1) {
              startsearch = (startsearch + MAX_GROUP_SIZE) & ~((unsigned int)(MAX_GROUP_SIZE-1));
           }
        } else {
           startsearch = indirectPtrs[(blockPtrNum-1)];
        }
     } else if (blockType & ALFS_ALLOC_DATA) {
        if (blockType & ALFS_ALLOC_DIRECT) {
           startsearch = ALFS_INODE_DISKADDR(dinode->dinodeNum);
           startsearch = (startsearch + MAX_GROUP_SIZE) & ~((unsigned int)(MAX_GROUP_SIZE-1));
        } else {
           startsearch = dinode->singleIndirect;
        }
     } else {
        startsearch = (unsigned int) random() % alfs_blockcount;
        startsearch = (startsearch + MAX_GROUP_SIZE) & ~((unsigned int)(MAX_GROUP_SIZE-1));
     }
     startsearch = startsearch % alfs_blockcount;
#else
     } else if ((blockPtrNum) && (blockType & ALFS_ALLOC_DATA)) {
         startsearch = (blockType & ALFS_ALLOC_DIRECT) ? alfs_dinode_getDirect(dinode, (blockPtrNum-1)) : indirectPtrs[(blockPtrNum-1)];
     } else if (blockType & ALFS_ALLOC_DATA) {
         startsearch = (blockType & ALFS_ALLOC_DIRECT) ? ALFS_INODE_DISKADDR(dinode->dinodeNum) : dinode->singleIndirect;
     } else {
         startsearch = (unsigned int) random() % alfs_blockcount;
     }
#endif

if (startsearch < 0) {
   printf("alfs_allocBlock: startsearch = %d\n", startsearch);
}

     /* checking the free block map and allocating a block is not */
     /* atomic so we keep trying until we get a block             */

     /* XXX -- so should we lock the allocation map and then allocate */
     /* or will this be faster ?                                      */

     do {
	  block = alfs_findFree (fsdev, startsearch);
          ret = alfs_safe_allocateBlock (fsdev, dinode, indirectPtrs, blockType, blockPtrNum, block);
     } while (ret == CSBADBLK);
     assert (ret == OK);
     return (block);
}


/* alfs_findFree

   get a free block to allocate. Pretty stupid right now--just scans
   through exo-disk's alfs_allocationMap looking for a free entry.

*/

block_num_t alfs_findFree (uint fsdev, int startsearch) {
#ifdef ALLOC_VIA_CFFS

   block_num_t block;
   void *sb_buf;		/* use void * so we don't confuse cffs and alfs buffer_t's */

   extern void *cffs_get_superblock (u_int, block_num_t, u_int);
   extern void cffs_buffer_releaseBlock (void *, u_int);

   sb_buf = cffs_get_superblock (fsdev, CFFS_SUPERBLKNO, 0); 
   block = cffs_findFree (sb_buf, startsearch);
   cffs_buffer_releaseBlock (sb_buf, 0);
   return (block);
#else
     int index = startsearch;

     assert (index < alfs_blockcount);
     /* XXX -- never allocate block 0, handled by keeping the bit set */
     do {
        if (!alfs_allocationMap[index]) {
           return (index);
        }
        index++;
        if (index >= alfs_blockcount) {
           index = 0;
        }
     } while (index != startsearch);

     assert ("out of blocks" == 0);		/* out of blocks */
     return (0);		/* stop warning about not returning value */
#endif
}


/* alfs_safe_allocateBlock

   allocate a free block and put it on the specified protection list.

*/

int alfs_safe_allocateBlock (uint fsdev, dinode_t *dinode, int *indirectPtrs, int blockType, int blockPtrNum, int block) {
     int ret;

/* temporary bypass
	  ret = cs_allocateBlock (inodeBlock, uidCap, gidCap, block, blockType);
*/
if (block < 0) {
   printf("alfs_safe_allocateBlock: %d\n", block);
}
     ret = alfs_alloc_allocBlock (fsdev, block);
     if (ret != 0) {
         return (CSBADBLK);
     }
/*
printf ("alfs_safe_allocBlock - success: block %d\n", block);
*/
     return (OK);
}


#ifdef GROUPING
#include "alfs/alfs_inode.h"
#include "alfs/alfs_embdir.h"


void alfs_alloc_setgroupinfo (block_num_t fsdev, dinode_t *dirDInode, int groupstart, int oldgroupsize, int newgroupsize) {
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   buffer_t *dirBlockBuffer;
   char *dirBlock;
   dinode_t *dinode;
   int dirty;
   int i;

   assert (dirDInode);

   /* search through the directory looking for the name */

   while (currPos < alfs_dinode_getLength(dirDInode)) {
      dirBlockLength = min((alfs_dinode_getLength(dirDInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirDInode->dinodeNum, dirBlockNumber, BUFFER_READ);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      dirty = 0;
      for (i=0; i < dirBlockLength; i += dirent->entryLen) {
         dirent = (embdirent_t *) (dirBlock + i);
         dinode = (dinode_t *) embdirentcontent (dirent);
         if ((dirent->type == (char) 1) && (dinode->groupstart == groupstart)) {
            assert (dinode->groupsize == oldgroupsize);
            dinode->groupsize = newgroupsize;
            dirty = BUFFER_DIRTY;
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, dirty);
      currPos += dirBlockLength;
   }
}


dinode_t * alfs_alloc_getgroupmember (block_num_t fsdev, dinode_t *dirDInode, int groupstart, buffer_t **buffer) {
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   buffer_t *dirBlockBuffer;
   char *dirBlock;
   dinode_t *dinode;
   int i;

   assert (dirDInode);

   /* search through the directory looking for the name */

   while (currPos < alfs_dinode_getLength(dirDInode)) {
      dirBlockLength = min((alfs_dinode_getLength(dirDInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirDInode->dinodeNum, dirBlockNumber, BUFFER_READ);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (i=0; i < dirBlockLength; i += dirent->entryLen) {
         dirent = (embdirent_t *) (dirBlock + i);
         dinode = (dinode_t *) embdirentcontent (dirent);
         if ((dirent->type == (char) 1) && (dinode->groupstart == groupstart)) {
            *buffer = dirBlockBuffer;
            return (dinode);
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
   *buffer = NULL;
   return (NULL);
}


int alfs_alloc_notchecked (int groupstart, int numchecked, int *checked)
{
   int i;
   for (i=0; i<numchecked; i++) {
      if (checked[i] == groupstart) {
         return (0);
      }
   }
   return (1);
}


int alfs_alloc_finduniquegroupinfo (block_num_t fsdev, dinode_t *dirDInode, int numchecked, int *checked, int *groupsize) {
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   buffer_t *dirBlockBuffer;
   char *dirBlock;
   dinode_t *dinode;
   int dirty;
   int i;

   assert (dirDInode);

   /* search through the directory looking for the name */

   while (currPos < alfs_dinode_getLength(dirDInode)) {
      dirBlockLength = min((alfs_dinode_getLength(dirDInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirDInode->dinodeNum, dirBlockNumber, BUFFER_READ);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      dirty = 0;
      for (i=0; i < dirBlockLength; i += dirent->entryLen) {
         dirent = (embdirent_t *) (dirBlock + i);
         dinode = (dinode_t *) embdirentcontent (dirent);
         if ((dirent->type == (char) 1) && (dinode->groupsize != 0) && (alfs_alloc_notchecked (dinode->groupstart, numchecked, checked))) {
            assert (dinode->groupstart != 0);
            alfs_buffer_releaseBlock (dirBlockBuffer, 0);
            *groupsize = dinode->groupsize;
            return (dinode->groupstart);
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
   return (0);
}


int alfs_alloc_putingroup (block_num_t fsdev, dinode_t *dirDInode, dinode_t *dinode)
{
   int numchecked = 1;
   int checked[16];
   int groupstart;
   int groupsize;
   int block;

   checked[0] = dirDInode->groupstart;
   while ((groupstart = alfs_alloc_finduniquegroupinfo (fsdev, dirDInode, numchecked, checked, &groupsize)) != 0) {
      block = alfs_findFree (fsdev, groupstart);
      if ((block >= groupstart) && (block < (groupstart + MAX_GROUP_SIZE))) {
         assert (alfs_safe_allocateBlock (fsdev, NULL, NULL, 0, 0, block) != CSBADBLK);
         dinode->groupstart = groupstart;
         dinode->groupsize = groupsize;
         if (block >= (groupstart + groupsize)) {
            groupsize = block - groupstart + 1;
            assert (groupsize <= MAX_GROUP_SIZE);
         }
         alfs_alloc_setgroupinfo (fsdev, dirDInode, groupstart, dinode->groupsize, groupsize);
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
   block = (unsigned int) random() % alfs_blockcount;
   block = (block + MAX_GROUP_SIZE) & ~((unsigned int)(MAX_GROUP_SIZE-1));
   block = block % alfs_blockcount;
   block = alfs_findFree (fsdev, block);
   assert (alfs_safe_allocateBlock (fsdev, NULL, NULL, 0, 0, block) != CSBADBLK);
   dinode->groupstart = block;
   dinode->groupsize = 1;
   return (block);
}
#endif


block_num_t alfs_alloc_doCollocation (block_num_t fsdev, dinode_t *dinode, int blockPtrNum)
{
#ifdef GROUPING
   block_num_t block = 0;
   inode_t *dirInode = NULL;
   dinode_t *dirDInode;
   int groupsize;

#ifdef FORCE_DIRECTORY_COLLOCATION
   if ((!(alfs_embdir_isEmbedded(dinode->dinodeNum))) || ((blockPtrNum == 0) && (alfs_dinode_getType(dinode) == INODE_DIRECTORY)) || ((blockPtrNum != 0) && (alfs_dinode_getType(dinode) != INODE_DIRECTORY))) {
#else
   if ((!(alfs_embdir_isEmbedded(dinode->dinodeNum))) || ((blockPtrNum == 0) && (alfs_dinode_getType(dinode) == INODE_DIRECTORY)) || (blockPtrNum != 0)) {
#endif
      return (0);
   }

   if (alfs_dinode_getType(dinode) == INODE_DIRECTORY) {
      dirDInode = dinode;
   } else {
      dirInode = alfs_inode_getInode (alfs_embdir_dirDInodeNum(dinode->dinodeNum), fsdev, (BUFFER_READ|BUFFER_WITM));
      dirDInode = dirInode->dinode;
   }

   if (dirDInode->groupstart == 0) {
      assert (dirDInode->directBlocks[0] != 0);
      dirDInode->groupstart = dirDInode->directBlocks[0];
      dirDInode->groupsize = 1;
   }

   /* GROK -- stop assuming that someone can get between "check if free" and */
   /*         "allocate" */

   block = alfs_findFree (fsdev, dirDInode->groupstart);
   if ((block >= dirDInode->groupstart) && (block < (dirDInode->groupstart + MAX_GROUP_SIZE))) {
      assert (alfs_safe_allocateBlock (fsdev, NULL, NULL, 0, 0, block) != CSBADBLK);
      groupsize = dirDInode->groupsize;  /* keep old value for setgroupinfo() */
      dinode->groupstart = dirDInode->groupstart;
      dinode->groupsize = dirDInode->groupsize;
      if (block >= (dirDInode->groupstart + dirDInode->groupsize)) {
         dirDInode->groupsize = block - dirDInode->groupstart + 1;
         assert (dirDInode->groupsize <= MAX_GROUP_SIZE);
      }
      alfs_alloc_setgroupinfo (fsdev, dirDInode, dirDInode->groupstart, groupsize, dirDInode->groupsize);
      if (dirInode) {
         alfs_inode_releaseInode (dirInode, BUFFER_DIRTY);
      }
      return (block);
/*
   } else {
      printf ("group alloc failed: groupstart %d, groupsize %d, block %d\n", dirDInode->groupstart, dirDInode->groupsize, block);
*/
   }

   /* GROK -- first cut only.  Want to include */

#ifdef FORCE_DIRECTORY_COLLOCATION
   if (alfs_dinode_getType(dinode) == INODE_DIRECTORY) {
      buffer_t *buffer;
      dinode_t *movee;
      assert (dirDInode == dinode);
      assert (dinode->length <= (MAX_GROUP_SIZE * BLOCK_SIZE));
      if ((movee = alfs_alloc_getgroupmember (fsdev, dirDInode, dirDInode->groupstart, &buffer)) != NULL) {
         int newblock;
         buffer_t *movebuffer = alfs_buffer_getBlock (fsdev, movee->dinodeNum, 0, BUFFER_READ);
         assert (movee != dinode);
         assert (buffer && buffer->buffer);
         assert (movebuffer && movebuffer->buffer);
         assert (movebuffer->header.diskBlock == movee->directBlocks[0]);
         assert (movebuffer->header.diskBlock > 0);
         assert (dinode->groupstart == movee->groupstart);
         assert (dinode->groupsize == movee->groupsize);
         movee->groupstart = 0;
         movee->groupsize = 0;
         block = movee->directBlocks[0];
         newblock = alfs_alloc_putingroup (fsdev, dirDInode, movee);
         assert (newblock > 0);
         assert (movee->groupstart && movee->groupsize);
/*
printf ("make space for directory block: move one from %d to %d\n", block, newblock);
*/
         movebuffer->header.diskBlock = newblock;
         movebuffer->buf.b_blkno = newblock * (BLOCK_SIZE/512);
         movee->directBlocks[0] = newblock;

         alfs_defaultcache_moveblock (movebuffer->header.dev, block, newblock);

         alfs_buffer_releaseBlock (movebuffer, BUFFER_DIRTY);
         alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);
         assert (dirInode == NULL);
         return (block);
      }
      printf ("Likely to allocate non-contiguous directory block!!\n");
      printf ("groupstart %d, groupsize %d, length %d\n", dinode->groupstart, dinode->groupsize, dinode->length);
      block = 0;
   } else {
      block = alfs_alloc_putingroup (fsdev, dirDInode, dinode);
      assert (dirInode);
      alfs_inode_releaseInode (dirInode, BUFFER_DIRTY);
      return (block);
   }
#else
   block = alfs_alloc_putingroup (fsdev, dirDInode, dinode);
#endif

   if (dirInode) {
      alfs_inode_releaseInode (dirInode, 0);
   }

   return (block);

#else
   return (0);
#endif
}

