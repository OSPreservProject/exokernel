
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
#include "alfs/alfs_dinode.h"
#include "alfs/alfs_alloc.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "alfs/alfs_alloc.h"
#include <machine/param.h>

#ifdef EMBEDINODES
#include "alfs/alfs_embdir.h"
#endif

#include "alfs/alfs_diskio.h"

int alfs_dinodeMapEntries = ALFS_DINODEMAPBLOCKS * ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK;
dinodeMapEntry_t alfs_dinodeMap[ (ALFS_DINODEMAPBLOCKS * ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK) ];

static int alfs_dinode_allocDInode (block_num_t, int);


int alfs_dinode_initmetablks ()
{
   return (ALFS_DINODEMAPBLOCKS);
}


block_num_t alfs_dinode_initDisk (cap2_t fscap, block_num_t fsdev, char *buf, int blkno)
{
   dinodeMapEntry_t initMap [ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK];
   unsigned int freeMask = 0;
   int i;

   assert (sizeof(dinode_t) == ALFS_DINODE_SIZE);
   assert (ALFS_DINODES_PER_BLOCK <= 32);
   for (i=0; i<ALFS_DINODES_PER_BLOCK; i++) {
      freeMask = (freeMask << 1) + 1;
   }

   for (i=0; i<ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK; i++) {
      initMap[i].dinodeBlockAddr = 0;
      initMap[i].freeDInodes = freeMask;
   }

   /* Allocate dinodes number 0 and 1 for empty and 2 for root, respectively */
   initMap[0].dinodeBlockAddr = ALFS_DINODEMAPBLOCKS + blkno;
   initMap[0].freeDInodes &= ~0x00000007;
   for (i=0; i<ALFS_DINODEMAPBLOCKS; i++) {
      struct buf buft;
      assert (alfs_alloc_allocBlock (fsdev, i+blkno) == 0);
      ALFS_DISK_WRITE (&buft, ((char *) initMap), fsdev, i+blkno);
      initMap[i].dinodeBlockAddr = 0;
      initMap[i].freeDInodes = freeMask;
   }

   /* setup buf to be the first inode block */
   bzero (buf, BLOCK_SIZE);
   for (i=0; i<ALFS_DINODES_PER_BLOCK; i++) {
      ((dinode_t *)(buf + (i * ALFS_DINODE_SIZE)))->dinodeNum = i;
   }

#ifdef EMBEDINODES
   /* inode number 2 is used for root directory */
   ((dinode_t *)(buf + (2 * ALFS_DINODE_SIZE)))->dinodeNum = 0x00020000;
#endif

   assert (alfs_alloc_allocBlock (fsdev, ALFS_DINODEMAPBLOCKS + blkno) == 0);
   return (ALFS_DINODEMAPBLOCKS + blkno);
}


void alfs_dinode_mountDisk (cap2_t fscap, block_num_t fsdev, int blkno)
{
   struct buf buft;
   int i;
   for (i=0; i<ALFS_DINODEMAPBLOCKS; i++) {
      ALFS_DISK_READ (&buft, ((char *) alfs_dinodeMap + (i * BLOCK_SIZE)), fsdev, (i+blkno));
   }
}


void alfs_dinode_unmountDisk (cap2_t fscap, block_num_t fsdev, int blkno)
{
   struct buf buft;
   int i;
   for (i=0; i<ALFS_DINODEMAPBLOCKS; i++) {
      ALFS_DISK_WRITE (&buft, ((char *) alfs_dinodeMap + (i * BLOCK_SIZE)), fsdev, (i+blkno));
   }
}


static int alfs_dinode_allocDInode (block_num_t fsdev, int allocHint)
{
    int i;
    int ret;
    int dinodeBlockAddr;
    unsigned int dinodeInBlock = 0;
    int dinodeNum = 0;
    dinodeMapEntry_t *dinodeMapEntry = NULL;
/*
printf("alfs_dinode_allocDInode\n");
*/
    for (i=0; i<alfs_dinodeMapEntries; i++) {
       dinodeMapEntry = &alfs_dinodeMap[i];
       if ((dinodeMapEntry->dinodeBlockAddr == 0) || (dinodeMapEntry->freeDInodes)) {
          break;
       }
       dinodeNum += ALFS_DINODES_PER_BLOCK;
   }
   assert (i != alfs_dinodeMapEntries);    /* too many files */

   dinodeBlockAddr = dinodeMapEntry->dinodeBlockAddr;
   if (dinodeBlockAddr == 0) {
      buffer_t *dinodeBlockBuffer;
      do {
         dinodeBlockAddr = alfs_findFree (fsdev, (unsigned int) random() % alfs_blockcount);
         ret = alfs_alloc_allocBlock (fsdev, dinodeBlockAddr);
      } while (ret == -1);
      dinodeBlockBuffer = alfs_buffer_getBlock (fsdev, 1, dinodeBlockAddr, (BUFFER_GET | BUFFER_WITM));
      bzero (dinodeBlockBuffer->buffer, BLOCK_SIZE);
      for (i=0; i<ALFS_DINODES_PER_BLOCK; i++) {
         ((dinode_t *)(dinodeBlockBuffer->buffer + (i * ALFS_DINODE_SIZE)))->dinodeNum = dinodeNum + i;
      }
      alfs_buffer_releaseBlock (dinodeBlockBuffer, BUFFER_WRITE);
      dinodeMapEntry->dinodeBlockAddr = dinodeBlockAddr;
   }

   while (dinodeInBlock <= 32) {
      if (dinodeMapEntry->freeDInodes & (1 << dinodeInBlock)) {
         dinodeMapEntry->freeDInodes &= ~(1 << dinodeInBlock);
/*
printf("freeDInodes %x, dinodeInBlock %d, mask %x\n", dinodeMapEntry->freeDInodes, dinodeInBlock, ~(1 << dinodeInBlock));
*/
         /* GROK -- how and who writes back the dinodeMap */
         return (dinodeNum + dinodeInBlock);
      }
      dinodeInBlock++;
   }
   assert(0);
   return (-1);
}


dinode_t *alfs_dinode_makeDInode (block_num_t fsdev, int allocHint)
{
   int dinodeNum = alfs_dinode_allocDInode (fsdev, allocHint);
   dinode_t *dinode = alfs_dinode_getDInode (dinodeNum, fsdev, (BUFFER_READ | BUFFER_WITM));

   assert (dinode->dinodeNum == dinodeNum);
   bzero ((char *) dinode, ALFS_DINODE_SIZE);
   dinode->refCount = 1;
   dinode->dinodeNum = dinodeNum;

   return (dinode);
}


int alfs_dinode_deallocDInode (block_num_t fsdev, block_num_t dinodeNum)
{
    int dinodeBlockNo = dinodeNum / ALFS_DINODES_PER_BLOCK;
    int dinodeInBlock = dinodeNum % ALFS_DINODES_PER_BLOCK;
    dinodeMapEntry_t *dinodeMapEntry;
/*
printf("alfs_dinode_deallocDInode: dinodeNum %d\n", dinodeNum);
*/
    assert (dinodeNum > 1);
    assert (dinodeNum < ALFS_MAX_DINODECOUNT);

    dinodeMapEntry = &alfs_dinodeMap[dinodeBlockNo];
    assert ((dinodeMapEntry->freeDInodes & (1 << dinodeInBlock)) == 0);
    dinodeMapEntry->freeDInodes |= (1 << dinodeInBlock);
    return (OK);
}


dinode_t *alfs_dinode_getDInode (block_num_t dinodeNum, block_num_t fsdev, int flags)
{
    int effdinodeNum = dinodeNum;
    dinodeMapEntry_t *dinodeMapEntry;
    block_num_t dinodeBlockAddr;
    buffer_t *dinodeBlockBuffer;
/*
printf("alfs_dinode_getDInode: dinodeNum %d, flags %x\n", dinodeNum, flags);
*/

#ifdef EMBEDINODES
    if ((alfs_embdir_isEmbedded (dinodeNum)) && !(alfs_embdir_isEntry (dinodeNum))) {
       effdinodeNum = alfs_embdir_transDirNum (dinodeNum);
/*
printf ("translated dinodeNum: %x ==============> %x\n", dinodeNum, effdinodeNum);
*/
    }
    if (alfs_embdir_isEmbedded (effdinodeNum)) {
       dinode_t *dinode;
/*
       printf ("Need to traverse backwards to find embedded directory entry: dinodeNum %x, effdinodeNum %x\n", dinodeNum, effdinodeNum);
*/
       dinode = alfs_embdir_getInode (fsdev, effdinodeNum, dinodeNum, &dinodeBlockBuffer);
       assert (dinode != NULL);
       return (dinode);
    } else
#endif
    {
       int dinodeBlockNo = effdinodeNum / ALFS_DINODES_PER_BLOCK;
       int dinodeInBlock = effdinodeNum % ALFS_DINODES_PER_BLOCK;

       assert (effdinodeNum > 0);
       assert (effdinodeNum < ALFS_MAX_DINODECOUNT);

       dinodeMapEntry = &alfs_dinodeMap[dinodeBlockNo];
       assert ((dinodeMapEntry->freeDInodes & (1 << dinodeInBlock)) == 0);
       dinodeBlockAddr = dinodeMapEntry->dinodeBlockAddr;
/*
printf("alfs_dinode_getDInode midpoint: dinodeBlockNo %d, dinodeBlockAddr %d, dinodeInBlock %d\n", dinodeBlockNo, dinodeBlockAddr, dinodeInBlock);
*/
       dinodeBlockBuffer = alfs_buffer_getBlock (fsdev, 1, dinodeBlockAddr, (flags | BUFFER_READ));
       assert (dinodeBlockBuffer);
       assert (((dinode_t *) dinodeBlockBuffer->buffer + dinodeInBlock)->dinodeNum == dinodeNum);
       return ((dinode_t *) dinodeBlockBuffer->buffer + dinodeInBlock);
   }
}


int alfs_dinode_releaseDInode (dinode_t *dinode, block_num_t fsdev, int flags)
{
    int dinodeNum = dinode->dinodeNum;
    int dinodeBlockNo;
    int dinodeInBlock;
    dinodeMapEntry_t *dinodeMapEntry;
    buffer_t *dinodeBlockBuffer;
    block_num_t dinodeBlockAddr;
    int invalidate = flags & BUFFER_INVALID;
    flags &= ~BUFFER_INVALID;
/*
printf("alfs_dinode_releaseDInode: dinodeNum %d, flags %x, refCount %d\n", dinodeNum, flags, dinode->refCount);
*/

#ifdef EMBEDINODES
    if ((alfs_embdir_isEmbedded (dinodeNum)) && !(alfs_embdir_isEntry (dinodeNum))) {
       dinodeNum = alfs_embdir_transDirNum (dinodeNum);
    }
    if (alfs_embdir_isEmbedded (dinodeNum)) {
        dinodeBlockBuffer = alfs_buffer_getBlock (fsdev, alfs_embdir_dirDInodeNum(dinodeNum), alfs_embdir_entryBlkno(dinodeNum), BUFFER_READ);
    } else
#endif
    {
        assert (dinodeNum > 0);
        assert (dinodeNum < ALFS_MAX_DINODECOUNT);

        dinodeBlockNo = dinodeNum / ALFS_DINODES_PER_BLOCK;
        dinodeInBlock = dinodeNum % ALFS_DINODES_PER_BLOCK;

        dinodeMapEntry = &alfs_dinodeMap[dinodeBlockNo];
        assert ((dinodeMapEntry->freeDInodes & (1 << dinodeInBlock)) == 0);
        dinodeBlockAddr = dinodeMapEntry->dinodeBlockAddr;
        dinodeBlockBuffer = alfs_buffer_getBlock (fsdev, 1, dinodeBlockAddr, BUFFER_READ);
    }

    assert (dinodeBlockBuffer);
    alfs_buffer_releaseBlock (dinodeBlockBuffer, 0);
    alfs_buffer_releaseBlock (dinodeBlockBuffer, flags);
    if (invalidate) {
        alfs_dinode_deallocDInode (fsdev, dinodeNum);
    }
    return (OK);
}


void alfs_dinode_writeDInode (dinode_t *dinode, block_num_t fsdev)
{
   int dinodeNum = dinode->dinodeNum;
   int dinodeBlockNo;
   int dinodeInBlock;
   dinodeMapEntry_t *dinodeMapEntry;
   buffer_t *dinodeBlockBuffer;
   block_num_t dinodeBlockAddr;
/*
printf("alfs_dinode_writeDInode: dinodeNum %d, refCount %d\n", dinodeNum, dinode->refCount);
*/

#ifdef EMBEDINODES
   if ((alfs_embdir_isEmbedded (dinodeNum)) && !(alfs_embdir_isEntry (dinodeNum))) {
      dinodeNum = alfs_embdir_transDirNum (dinodeNum);
   }
   if (alfs_embdir_isEmbedded (dinodeNum)) {
      dinodeBlockBuffer = alfs_buffer_getBlock (fsdev, alfs_embdir_dirDInodeNum(dinodeNum), alfs_embdir_entryBlkno(dinodeNum), BUFFER_READ);
   } else
#endif
   {
      assert (dinodeNum > 0);
      assert (dinodeNum < ALFS_MAX_DINODECOUNT);

      dinodeBlockNo = dinodeNum / ALFS_DINODES_PER_BLOCK;
      dinodeInBlock = dinodeNum % ALFS_DINODES_PER_BLOCK;

      dinodeMapEntry = &alfs_dinodeMap[dinodeBlockNo];
      assert ((dinodeMapEntry->freeDInodes & (1 << dinodeInBlock)) == 0);
      dinodeBlockAddr = dinodeMapEntry->dinodeBlockAddr;
      dinodeBlockBuffer = alfs_buffer_getBlock (fsdev, 1, dinodeBlockAddr, BUFFER_READ);
   }

   assert (dinodeBlockBuffer);
   alfs_buffer_releaseBlock (dinodeBlockBuffer, BUFFER_WRITE);
}


void alfs_dinode_truncDInode (dinode_t *dinode, block_num_t fsdev, int length)
{
   int i;
   int ret = 0;
   int blockFrees = 0;

   assert (length == 0);   /* simple, common case only (for now) */

   dinode->groupstart = 0;
   dinode->groupsize = 0;

   dinode->length = 0;
   for (i=0; i<NUM_DIRECT; i++) {
      if (dinode->directBlocks[i]) {
         ret |= alfs_alloc_deallocBlock (fsdev, dinode->directBlocks[i]);
         dinode->directBlocks[i] = 0;
         blockFrees++;
      }
   }
   if (dinode->singleIndirect) {
      buffer_t *indirectBlockBuffer = alfs_buffer_getBlock (fsdev, dinode->dinodeNum, -dinode->singleIndirect, (BUFFER_READ | BUFFER_WITM));
      int *indirectBlock = (int *) indirectBlockBuffer->buffer;
      for (i=0; i<NUM_SINGLE_INDIRECT; i++) {
         if (indirectBlock[i]) {
            ret |= alfs_alloc_deallocBlock (fsdev, indirectBlock[i]);
            indirectBlock[i] = 0;
            blockFrees++;
         }
      }
      alfs_buffer_releaseBlock (indirectBlockBuffer, BUFFER_INVALID);
      ret |= alfs_alloc_deallocBlock (fsdev, dinode->singleIndirect);
      dinode->singleIndirect = 0;
      blockFrees++;
   }
   assert (ret == 0);
/*
   printf("alfs_dinode_truncDInode done: dinodeNum %d, blockFrees %d\n", dinode->dinodeNum, blockFrees);
*/
}


/* alfs_dinode_offsetToBlock

   convert an offset into a file into a block number, given an dinode.
   This should only be called for offset < MAX_FILE_SIZE

   dinode is the buffer containing the dinode
   offset is the offset into file
   allocate determines whether we should try to allocate blocks
     for offsets that correspond to un-allocated blocks. If we do
     allocate any blocks, this value is set to true. If we do not
     it is set to false.

   returns block number or 0 if attempting to read from hole in file 

 */

block_num_t alfs_dinode_offsetToBlock (dinode_t *dinode, block_num_t fsdev, unsigned offset, int *allocate) {
     buffer_t *indirectBlock;
     int *indirectPtrs;
     block_num_t blockNum;
     int allocHint = 0;
/*
printf("alfs_dinode_offsetToBlock: dinodeNum %x, offset %d, allocate %x\n", dinode->dinodeNum, offset, (u_int) allocate);
*/
     assert (offset < MAX_FILE_SIZE);

     if (allocate != NULL) {
         allocHint = *allocate;
         *allocate = 0;
     }

     /* see if the offset is mapped to a direct block */
     if (offset < BLOCK_SIZE*NUM_DIRECT) {
	  int directBlockNumber = offset / BLOCK_SIZE;
	  blockNum = dinode->directBlocks[directBlockNumber];

	  /* allocate the block if needed */
	  if ((blockNum == 0) && (allocate)) {
               blockNum = alfs_allocBlock (fsdev, dinode, NULL, ALFS_ALLOC_DATA_DIR, directBlockNumber, allocHint);
               dinode->directBlocks[directBlockNumber] = blockNum;
               *allocate = 1;
	  }
     } else {
         /* have to read indirect block. May have to allocate it first. */
         if ((dinode->singleIndirect == 0) && (allocate)) {
              /* allocate and initialize indirect block. */
              dinode->singleIndirect = alfs_allocBlock (fsdev, dinode, NULL, ALFS_ALLOC_INDIRECT, 0, allocHint);
              indirectBlock = alfs_buffer_getBlock (fsdev, dinode->dinodeNum, -dinode->singleIndirect, (BUFFER_GET | BUFFER_WITM));
              bzero (indirectBlock->buffer, BLOCK_SIZE);
          } else if ((dinode->singleIndirect == 0) && (!allocate)) {
              return (0);	/* messy -- not allocated and we're reading */
          } else {
              /* read in indirect block */
              indirectBlock = alfs_buffer_getBlock (fsdev, dinode->dinodeNum, -dinode->singleIndirect, BUFFER_READ);
          }
       
          /* ok, lookup block in indirect block and allocate if doesn't exist */
       
          offset -= BLOCK_SIZE * NUM_DIRECT;
          indirectPtrs = (int *) indirectBlock->buffer;
          blockNum = indirectPtrs[(offset / BLOCK_SIZE)];
          if ((blockNum == 0) && (allocate)) {
              blockNum = alfs_allocBlock (fsdev, dinode, indirectPtrs, ALFS_ALLOC_DATA_INDIR, (offset / BLOCK_SIZE), allocHint);
              indirectPtrs[(offset / BLOCK_SIZE)] = blockNum;
              indirectBlock->flags |= BUFFER_DIRTY;
              *allocate = 1;
          }
          alfs_buffer_releaseBlock (indirectBlock, 0);
     }
/*
printf("alfs_dinode_offsetToBlock return: blockNum %d\n", blockNum);
*/
    return (blockNum);
}

