
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


#include "cffs_dinode.h"
#include "cffs_buffer.h"
#include "cffs.h"
#include "cffs_inode.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <exos/mallocs.h>

#ifndef JJ
#include <exos/bufcache.h>
#endif

#include "cffs_proc.h"

/* defines for the private (per-app) inode cache structures */

#define CFFS_INODE_HASHBUCKETS	64
#define CFFS_INODE_PRIVATEMAX	64

#define CFFS_INODE_HASH(inodeNum, fsdev) \
                ((inodeNum) & (CFFS_INODE_HASHBUCKETS-1))

/*
#define PRINT_INODEINFO	1
*/

static inode_t *allocPrivInode ();
static void deallocPrivInode (inode_t *inode);
static inode_t *checkforInode (block_num_t dinodeNum, block_num_t fsdev);
static void unlistPrivInode (inode_t *inode);
static void listPrivInode (inode_t *inode);

static int cachedInodeCnt = 0;

static inode_t *listedInodeHash[CFFS_INODE_HASHBUCKETS];
static inode_t *listedInodesLRU = NULL;
static inode_t *privateFreelist = NULL;


void cffs_inode_initInodeCache ()
{
   int i;

   for (i=0; i<CFFS_INODE_HASHBUCKETS; i++) {
      listedInodeHash[i] = NULL;
   }
}


static void listPrivInode (inode_t *inode)
{
#ifdef PRINT_INODEINFO
printf ("%d: listPrivInode: inode %p, inodeNum %x\n", getpid(), inode, inode->inodeNum);
#endif

   assert ((inode->next == NULL) && (inode->prev == NULL));
   assert (inode->inodeNum != 0);
   inode->hashbucket = CFFS_INODE_HASH(inode->inodeNum,inode->fsdev);
   inode->next = listedInodeHash[inode->hashbucket];
   inode->prev = NULL;
   if (inode->next) {
      inode->next->prev = inode;
   }
   listedInodeHash[inode->hashbucket] = inode;

   if (listedInodesLRU == NULL) {
      inode->lru_next = inode;
      inode->lru_prev = inode;
      listedInodesLRU = inode;
   } else {
      inode->lru_next = listedInodesLRU->lru_next;
      inode->lru_prev = listedInodesLRU;
      inode->lru_next->lru_prev = inode;
      inode->lru_prev->lru_next = inode;
   }

   cachedInodeCnt++;
}


static void unlistPrivInode (inode_t *inode)
{
#ifdef PRINT_INODEINFO
printf ("%d: unlistPrivInode: inode %p, inodeNum %x\n", getpid(), inode, inode->inodeNum);
#endif

   inode->lru_next->lru_prev = inode->lru_prev;
   inode->lru_prev->lru_next = inode->lru_next;
   if (listedInodesLRU == inode) {
      listedInodesLRU = (inode != inode->lru_prev) ? inode->lru_prev : NULL;
   }
	/* to force segfaults when behaving badly */
   inode->lru_next = NULL;
   inode->lru_prev = NULL;

   if (inode->next) {
      inode->next->prev = inode->prev;
   }

   if (inode->prev) {
      inode->prev->next = inode->next;
   } else {
      assert (listedInodeHash[inode->hashbucket] == inode);
      listedInodeHash[inode->hashbucket] = inode->next;
   }
   inode->hashbucket = 0x80000000;	/* cause fault if re-used */
   inode->next = NULL;
   inode->prev = NULL;
   cachedInodeCnt--;
}


static int cffs_inode_releaseLRU ()
{
   inode_t *inode = listedInodesLRU;
   buffer_t *sb_buf;

   if ((inode) && (inode->refCount > 0)) {
      inode = inode->lru_prev;
      while ((inode != listedInodesLRU) && (inode->refCount > 0)) {
         inode = inode->lru_prev;
      }
   }

   if ((inode) && (inode->refCount <= 0)) {
      assert (inode->refCount == 0);
      unlistPrivInode (inode);
      assert (inode->inodeNum != 0);
      assert (inode->dinodeBlockBuffer->inUse > 0);
      sb_buf = cffs_get_superblock (inode->fsdev, inode->sprblk, 0);
      cffs_dinode_releaseDInode (inode->dinode, sb_buf, 0, inode->dinodeBlockBuffer);
      cffs_buffer_releaseBlock (sb_buf, 0);

      deallocPrivInode (inode);
      return (1);
   }
   return (0);
}


void cffs_inode_shutdownInodeCache ()
{
   inode_t *inode;
   int i;
   buffer_t *sb_buf;

   for (i=0; i<CFFS_INODE_HASHBUCKETS; i++) {
      while (listedInodeHash[i]) {
         inode = listedInodeHash[i];
         unlistPrivInode (inode);
         assert (inode->inodeNum != 0);
         assert (inode->dinodeBlockBuffer->inUse > 0);
	 sb_buf = cffs_get_superblock (inode->fsdev, inode->sprblk, 0);
         cffs_dinode_releaseDInode (inode->dinode, sb_buf, 0, inode->dinodeBlockBuffer);
	 cffs_buffer_releaseBlock (sb_buf, 0);
         deallocPrivInode (inode);
      }
   }

   assert (cachedInodeCnt == 0);
   assert (listedInodesLRU == NULL);
}


void cffs_inode_flushInodeCache ()
{
   inode_t *inode;
   int i;
   buffer_t *sb_buf;

   for (i=0; i<CFFS_INODE_HASHBUCKETS; i++) {
      inode = listedInodeHash[i];

      while (inode) {
         inode_t *next = inode->next;
         if (inode->refCount == 0) {
            unlistPrivInode (inode);
            assert (inode->inodeNum != 0);
            assert (inode->dinodeBlockBuffer->inUse > 0);
	    sb_buf = cffs_get_superblock (inode->fsdev, inode->sprblk, 0);
            cffs_dinode_releaseDInode (inode->dinode, sb_buf, 0, inode->dinodeBlockBuffer);
	    cffs_buffer_releaseBlock (sb_buf, 0);
            deallocPrivInode (inode);
         }
         inode = next;
      }
   }
}


void cffs_inode_printInodeCache ()
{
   inode_t *inode;
   int i;

   printf ("cffs_inode_printInodeCache: cachedInodeCnt %d\n", cachedInodeCnt);
   for (i=0; i<CFFS_INODE_HASHBUCKETS; i++) {
      inode = listedInodeHash[i];
      while (inode) {
         printf ("inodeNum %d (%x), refCount %d, dinode %p, linkCount %d, openCount %d\n", inode->inodeNum, inode->inodeNum, inode->refCount, inode->dinode, ((inode->dinode) ? cffs_inode_getLinkCount(inode) : 0), ((inode->dinode) ? cffs_inode_getOpenCount(inode) : 0));
         printf ("	buffer %p (inodeNum %x, block %d, inUse %d)\n", inode->dinodeBlockBuffer, inode->dinodeBlockBuffer->header.inodeNum, inode->dinodeBlockBuffer->header.block, inode->dinodeBlockBuffer->inUse);
         inode = inode->next;
      }
   }
}


static inode_t *checkforInode (block_num_t inodeNum, block_num_t fsdev)
{
   inode_t *inode = listedInodeHash[CFFS_INODE_HASH(inodeNum,fsdev)];

   while ((inode) && ((inode->inodeNum != inodeNum) || (inode->fsdev != fsdev))) {
      inode = inode->next;
   }

   return (inode);
}


static inode_t *allocPrivInode ()
{
   inode_t *inode = privateFreelist;

   if (inode == NULL) {
      inode = (inode_t *) __malloc (sizeof(inode_t));
   } else {
      privateFreelist = inode->next;
   }
   bzero ((char *)inode, sizeof(inode_t));
   inode->hashbucket = 0x80000000;		/* cause fault if unlisted */
   return (inode);
}


static void deallocPrivInode (inode_t *inode)
{
   assert ((inode->next == NULL) && (inode->prev == NULL));
   assert (inode->refCount == 0);
   inode->refCount = -1;
   inode->next = privateFreelist;
   inode->prev = NULL;
   inode->lru_prev = NULL;
   inode->lru_next = NULL;
   privateFreelist = inode;
}


inode_t * cffs_inode_makeInode (block_num_t fsdev, block_num_t sprblk, uint inodeNum, dinode_t *dinode, buffer_t *dinodeBlockBuffer)
{
   inode_t *inode;

#ifdef PRINT_INODEINFO
printf ("%d: makeInode: inodeNum %x\n", getpid(), inodeNum);
#endif

   assert (inodeNum != 0);
   if ((inode = checkforInode (inodeNum, fsdev)) != NULL) {
      assert (inode->dinode == dinode);
      inode->refCount++;
      /* don't hold extra refs to dinodeBlockBuffer -- one is sufficient */
      cffs_buffer_releaseBlock (dinodeBlockBuffer, 0);
      return (inode);
   }

   inode = allocPrivInode();
   inode->dinode = dinode;
   inode->fsdev = fsdev;
   inode->sprblk = sprblk;
   inode->inodeNum = inodeNum;
   inode->dinodeBlockBuffer = dinodeBlockBuffer;
   inode->refCount = 1;
   inode->next = inode->prev = NULL;
   listPrivInode (inode);
   return (inode);
}


inode_t *cffs_inode_getInode (block_num_t inodeNum, block_num_t fsdev, block_num_t sprblk, int flags)
{
   inode_t *inode;

#ifdef PRINT_INODEINFO
printf ("%d: cffs_inode_getInode: inodeNum %x, fsdev %x, flags %x\n", getpid(), inodeNum, fsdev, flags);
#endif

   assert (inodeNum != 0);
   inode = checkforInode (inodeNum, fsdev);

   if (inode == NULL) {
      dinode_t *dinode;
      buffer_t *dinodeBlockBuffer;
      if ((dinode = cffs_dinode_getDInode (inodeNum, fsdev, flags, &dinodeBlockBuffer)) == NULL) {
         kprintf("cffs_inode_getInode: getDInode failed (inodeNum %x, fsdev %x, flags %x)\n", inodeNum, fsdev, flags);
         assert (0);
      }
      inode = cffs_inode_makeInode(fsdev, sprblk, inodeNum, dinode, dinodeBlockBuffer);
   } else {
      inode->refCount++;
   }

#ifdef PRINT_INODEINFO
printf ("%d: got Inode: inodeNum %d (%x), refCount %d, dinode %p, block[0] %d\n", getpid(), inodeNum, inodeNum, inode->refCount, inode->dinode, inode->dinode->directBlocks[0]);
#endif

   assert (cffs_dinode_sanityOK(inode->dinode));

   return (inode);
}


void cffs_inode_tossInode (inode_t *inode)
{
#ifdef PRINT_INODEINFO
printf ("%d: tossInode: inodeNum %d (%x), refCount %d\n", getpid(), inode->inodeNum, inode->inodeNum, inode->refCount);
#endif

   assert (inode->refCount == 1);
   inode->refCount--;
   assert (inode->inodeNum != 0);
   assert (inode->dinode == NULL);
   unlistPrivInode (inode);

   cffs_buffer_releaseBlock (inode->dinodeBlockBuffer, 0);
   deallocPrivInode (inode);
}


buffer_t *cffs_inode_getDinodeBuffer (inode_t *inode)
{
   assert (cffs_dinode_sanityOK(inode->dinode));
   return (inode->dinodeBlockBuffer);
}


int cffs_inode_getRefCount (inode_t *inode)
{
   assert (cffs_dinode_sanityOK(inode->dinode));
   return (inode->refCount);
}


int cffs_inode_releaseInode (inode_t *inode, int flags)
{
  buffer_t *sb_buf;

   assert (cffs_dinode_sanityOK(inode->dinode));

#ifdef PRINT_INODEINFO
printf ("%d: releaseInode: inodeNum %d (%x), refCount %d, dinode %p, block[0] %d\n", getpid(), inode->inodeNum, inode->inodeNum, inode->refCount, inode->dinode, ((inode->dinode) ? inode->dinode->directBlocks[0] : -1));
#endif

   assert (inode->refCount > 0);
   inode->refCount--;
   assert (inode->inodeNum != 0);
   if (inode->refCount == 0) {
      unlistPrivInode (inode);
      assert (inode->dinodeBlockBuffer->inUse > 0);
      if (((flags & (BUFFER_WRITE|BUFFER_INVALID)) == 0) && (cffs_inode_getLinkCount(inode) > ((cffs_inode_isDir(inode)) ? 1 : 0)) && ((cachedInodeCnt < CFFS_INODE_PRIVATEMAX) || (cffs_inode_releaseLRU()))) {
         if (flags & BUFFER_DIRTY) {
            cffs_buffer_dirtyBlock (inode->dinodeBlockBuffer);
         }
         listPrivInode (inode);
         return (0);
      }
      assert (inode->dinodeBlockBuffer->inUse > 0);
      sb_buf = cffs_get_superblock (inode->fsdev, inode->sprblk, 0);
      cffs_dinode_releaseDInode (inode->dinode, sb_buf, flags, inode->dinodeBlockBuffer);
      cffs_buffer_releaseBlock (sb_buf, 0);
      deallocPrivInode (inode);
   } else {
      assert ((cffs_inode_getLinkCount (inode) != 0) || (cffs_inode_getOpenCount(inode) != 0));
   }
   return (0);
}


void cffs_inode_dirtyInode (inode_t *inode)
{
   exos_bufcache_setDirty (inode->dinodeBlockBuffer->bc_entry->buf_dev, inode->dinodeBlockBuffer->bc_entry->buf_blk, 1);
}


void cffs_inode_writeInode (inode_t *inode, int flags)
{
   cffs_dinode_writeDInode (inode->dinode, inode->fsdev, inode->dinodeBlockBuffer);
}


int cffs_inode_truncInode (inode_t *inode, off_t length)
{
  buffer_t *sb_buf;

   /* make sure we're not trying to write too big of a file */
   if (length > MAX_FILE_SIZE) {                                      
      return -EFBIG;
   }

   if (length >= cffs_inode_getLength(inode)) {
      cffs_inode_setLength (inode, length);

   } else {
     sb_buf = cffs_get_superblock (inode->fsdev, inode->sprblk, 0);
#ifndef XN
#ifdef CFFS_PROTECTED
      if (length == 0) {
         cffs_dinode_setGroupstart (inode->dinode, 0);
         cffs_dinode_setGroupsize (inode->dinode, 0);
      }
      cffs_dinode_truncDInode (inode->dinode, sb_buf, length);
      cffs_buffer_releaseBlock (sb_buf, 0);
      cffs_inode_setLength (inode, length);
#else
      dinode_t tmpDInode = *(inode->dinode);
      /* nullify pointers */
      if (length == 0) {
         cffs_dinode_setGroupstart (inode->dinode, 0);
         cffs_dinode_setGroupsize (inode->dinode, 0);
      }
      cffs_inode_setLength (inode, length);
      cffs_dinode_clearBlockPointers (inode->dinode, length);
      if (cffs_softupdates == 0) {
         cffs_inode_writeInode (inode, BUFFER_WRITE);
      }
      cffs_dinode_truncDInode (&tmpDInode, sb_buf, length);
#endif
#else
      if (length == 0) {
         cffs_dinode_setGroupstart (inode->dinode, 0);
         cffs_dinode_setGroupsize (inode->dinode, 0);
      }
      cffs_dinode_truncDInode (inode->dinode, sb_buf, length);
      cffs_inode_setLength (inode, length);
#endif
   }

   return (0);
}


block_num_t cffs_inode_offsetToBlock (inode_t *inode, off_t offset, int *allocate, int *error)
{
   block_num_t blkno;
   buffer_t *sb_buf;

#ifndef JJ
   assert (inode->dinodeBlockBuffer != NULL);
   assert (exos_bufcache_isvalid(inode->dinodeBlockBuffer->bc_entry));
   if (exos_bufcache_isiopending (inode->dinodeBlockBuffer->bc_entry)) {
      //kprintf("would have called offsetToBlock with ongoing request...\n");
      exos_bufcache_waitforio (inode->dinodeBlockBuffer->bc_entry);
   }
#endif

   sb_buf = cffs_get_superblock (inode->fsdev, inode->sprblk, 0);
   blkno = cffs_dinode_offsetToBlock (inode->dinode, sb_buf, offset, allocate, error);
   cffs_buffer_releaseBlock (sb_buf, 0);

   if (error && *error == -1)
     return 0;

   if ((allocate) && (*allocate)) {
      cffs_buffer_dirtyBlock (inode->dinodeBlockBuffer);
   }

   return (blkno);
}

int cffs_inode_getNumContig (inode_t *inode, block_num_t blkno)
{
  int cnt;
  buffer_t *sb_buf;

  sb_buf = cffs_get_superblock (inode->fsdev, inode->sprblk, 0);
  cnt = cffs_dinode_getNumContig (inode->dinode, sb_buf, blkno);
  cffs_buffer_releaseBlock (sb_buf, 0);
  
  return (cnt);
}

