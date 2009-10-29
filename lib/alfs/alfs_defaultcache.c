
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
#include "alfs/alfs_inode.h"
#include "alfs/alfs_protection.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "alfs/alfs_diskio.h"

#include <exos/bufcache.h>
#include <exos/ubc.h>
#include <xok/pxn.h>
#include <xok/sysinfo.h>
#include <exos/cap.h>

#ifdef EXOPC
#include <exos/vm-layout.h>		/* for PAGESIZ */
#else
#define PAGESIZ 4096
#endif

/*
#define NOSCATTER
#define NOGATHER
*/
#define GATHBYDISKBLOCK	1

int alfs_diskwrites = 0;
int alfs_diskreads = 0;


buffer_t *sys_buffer_handleMiss (block_num_t, block_num_t, int, int);
void sys_buffer_flushEntry (buffer_t *);
void sys_buffer_writeBack (buffer_t *, int);


int alfs_inode_getNumContig(inode_t *inode, int block)
{
   int i;
   int diskBlock = alfs_inode_offsetToBlock (inode, (block*BLOCK_SIZE), NULL);
   for (i=1; i<16; i++) {
      int db = alfs_inode_offsetToBlock (inode, ((block+i)*BLOCK_SIZE), NULL);
      if (db != (diskBlock + i)) {
         return (i-1);
      }
   }
   return (16);
}


/* get a new buffer for a block */

buffer_t *alfs_default_buffer_handleMiss (block_num_t dev, block_num_t inodeNum, int block, int flags) {
     inode_t *inode = NULL;
     buffer_t *entry = NULL;
     block_num_t diskBlock = 0;
     struct bc_entry *bc_entry;
     int allocated = 0;
     int rangesize = 1;
     int rangestart;
/*
printf("alfs_buffer_handleMiss: buf %p, dev %d, inodeNum %d, block %d, flags %x\n", entry, dev, inodeNum, block, flags);
*/
     entry = (buffer_t *)alfs_buffertab_getEmptyEntry (alfs_localCache, dev, inodeNum, block);
     if (entry == NULL) {
         alfs_buffer_printCache ();
     }
     assert (entry);

/*
     assert (entry->buffer == NULL);
*/
     assert (block != -1);
     entry->bc_entry = NULL;
     entry->flags = 0;  /* not BUFFER_RDONLY */

     entry->buf.b_resid = -1;

     entry->header.dev = dev;
     entry->header.inodeNum = inodeNum;
     entry->header.block = block;
     entry->inUse = 0;

     if ((block < 0) || (inodeNum == 1)) {
        assert (block);
        if (block < 0) {
           diskBlock = -block;
        } else {
           diskBlock = block;
        }

     } else {   /* file block */
        buffer_t *entry2;
        inode = alfs_inode_getInode (inodeNum, dev, BUFFER_READ);
        diskBlock = alfs_inode_offsetToBlock (inode, (block * BLOCK_SIZE), ((flags & BUFFER_ALLOCSTORE) ? &allocated : NULL));
        /* GROK -- sledge hammer that breaks clean interfaces yet again */
      if ((entry2 = (buffer_t *)alfs_buffertab_findEntry (alfs_localCache, dev, 1, diskBlock, FIND_ALLOCATE)) != NULL) {
assert (0);
#if 0
         entry = entry2;
         assert (alfs_inode_getType(inode) == INODE_DIRECTORY);
         assert (entry->bc_entry->buf_blk == entry->header.diskBlock);
/*
         kprintf ("%d: rename: ino %x, blkno %d ---> ino %x, blkno %d (entry %p)\n", getpid(), entry->header.inodeNum, entry->header.block, inodeNum, block, entry);
*/
         alfs_buffertab_renameEntry (alfs_localCache, (bufferHeader_t *)entry, dev, inodeNum, block);
         assert (alfs_buffertab_findEntry (alfs_localCache, dev, 1, diskBlock, FIND_VALID) == NULL);
         assert ((entry->header.inodeNum == inodeNum) && (entry->header.block ==
 block));
         alfs_inode_releaseInode (inode, 0);
#endif
         return (entry);
      }

     }

     entry->header.diskBlock = diskBlock;

default_buffer_handleMiss_retry:

     if (diskBlock == 0) {
	/* diskBlock will be 0 only if this is a hole in the file.  Just    */
	/* fill is with zeroes.                                             */
	/* GROK -- this currently can cause a consistency problem if cached */
	/* locally.                                                         */

        kprintf ("fix the locally cached hole problem in ALFS, bozo\n");
        assert (0);
        bzero (entry->buffer, BLOCK_SIZE);

   } else if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) != NULL) {
        /* found the desired block in the bufcache */

      entry->bc_entry = bc_entry;
      entry->buffer = exos_bufcache_map (bc_entry, dev, diskBlock, 1);

      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }
      assert (bc_entry->buf_blk == diskBlock);

//kprintf ("found it (diskBlock %d), now wait for it (%p, ppn %d)\n", diskBlock, entry->buffer, PGNO(vpt[PGNO(entry->buffer)]));

      exos_bufcache_waitforio (bc_entry);

        /* Make sure we don't get old crap (if allocating new). */
      if ((allocated) && ((flags & BUFFER_READ) || (inode == NULL) || (alfs_inode_getType(inode) == INODE_DIRECTORY))) {
         bzero (entry->buffer, BLOCK_SIZE);
      }

   } else if ((allocated) || !(flags & BUFFER_READ)) {
        /* Newly allocated block or block whose contents will be completely */
        /* overwritten.  No disk read is needed; instead, insert it into    */
        /* cache, and bzero it if contents will be used (ie, READ).         */

      int zerofill = (flags & BUFFER_READ) || (inode == NULL) || (alfs_inode_getType(inode) == INODE_DIRECTORY);
      entry->buffer = (char *) exos_bufcache_alloc (dev, diskBlock, zerofill, 1, 0);
      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }
      if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) == NULL) {
         kprintf ("lookup failed after `successful' alloc\n");
         assert (0);
      }
      entry->bc_entry = bc_entry;

//kprintf ("allocate for zeros: %p (ppn %d), inodeNum %x, block %d, diskBlock %d\n", entry->buffer, PGNO(vpt[PGNO(entry->buffer)]), inodeNum, block, diskBlock);

   } else if (flags & BUFFER_READ) {
        /* gonna do a disk read... */

//kprintf("diskread: inodeNum %x, block %d, diskBlock %d\n", inodeNum, block, diskBlock);

        /* first, determine boundaries for disk read (remember: clustering */
        /* and large disk I/Os are good things!)                           */

      rangestart = diskBlock;

#ifndef NOSCATTER
      if (inode != NULL) {
         if ((block == 0) && (alfs_inode_getGroupsize(inode) > 1)) {
            rangesize = alfs_inode_getGroupsize (inode);
            rangestart = alfs_inode_getGroupstart (inode);
         } else {
            rangesize = 1 + alfs_inode_getNumContig(inode, block);
         }
         rangesize = min (16, rangesize);
      }
#endif
      assert ((diskBlock >= rangestart) && (diskBlock < (rangestart + rangesize)));

        /* figure out the two end points and then generate request sequence... */
      {
      int firstblock = diskBlock;
      int lastblock = diskBlock;
      while (((firstblock-1) >= rangestart) && (exos_bufcache_lookup(dev, (firstblock-1)) == NULL)) {
         firstblock--;
      }
      while (((lastblock+1) < (rangestart + rangesize)) && (exos_bufcache_lookup(dev, (lastblock+1)) == NULL)) {
         lastblock++;
      }

//kprintf ("Using read clustering: inodeNum %x, block %d, contig %d\n", inodeNum, block, (lastblock - firstblock + 1));

        /* now, initiate the actual disk read */

      alfs_diskreads++;
      exos_bufcache_initfill (dev, firstblock, (lastblock-firstblock+1), &entry->buf.b_resid);
      if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) == NULL) {
         kprintf ("Didn't really initiate request\n");
         assert (0);
      }
      entry->bc_entry = bc_entry;
      entry->buffer = exos_bufcache_map (bc_entry, dev, diskBlock, 1);
      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }

      exos_bufcache_waitforio (bc_entry);
      }

   } else {
      kprintf ("alfs_defaultcache_handleMiss: this case shouldn't happen...\n");
      assert (0);
   }

   if (inode != NULL) {
      alfs_inode_releaseInode (inode, 0);
   }

   if (allocated) {
#ifdef USE_BC_DIRTY
      if (!entry->bc_entry->buf_dirty) {
         int ret = exos_bufcache_setDirty (dev, diskBlock, 1);
         assert (ret == 0);
      }
#else
      entry->flags |= BUFFER_DIRTY;
#endif
   }


     return (entry);
}


void alfs_default_buffer_flushEntry (buffer_t *entry)
{
   /* and unmap the block from our address space */
/*
printf ("flushEntry: dev %d, inodeNum %x, block %d, diskBlock %d, buffer %p\n", entry->header.dev, entry->header.inodeNum, entry->header.block, entry->header.diskBlock, entry->buffer);
*/
   exos_bufcache_unmap (entry->header.dev, entry->header.diskBlock, entry->buffer);
}


void alfs_default_buffer_writeBack (buffer_t *entry, int async)
{
   int ret;
   block_num_t diskBlock = entry->header.diskBlock;
#ifdef USE_BC_DIRTY
   struct bc_entry *tmp;
#else
   buffer_t *tmp;
#endif
   int contig = 1;
/*
printf("alfs_buffer_writeBack: inodeNum %x, block %d, diskBlock %d\n", entry->header.inodeNum, entry->header.block, diskBlock);
*/
   if (entry->bc_entry->buf_tainted) {
      if (async) {
         int ret = exos_bufcache_setDirty (entry->header.dev, entry->header.diskBlock, 1);
         assert (ret == 0);
      } else {
         kprintf ("Explicitly trying to initiate a write on a tainted block!! (inodeNum %d, block %d, buf_dirty %d, buf_tainted %d\n", entry->header.inodeNum, entry->header.block, entry->bc_entry->buf_dirty, entry->bc_entry->buf_tainted);
      }
   }

   /* GROK -- case of not-yet-alloced space is not handled! */
   assert (diskBlock > 0);
   entry->buf.b_resid = 0;
   alfs_diskwrites++;

  assert ((entry->bc_entry->buf_blk == entry->header.diskBlock) || (entry->header.block == 0));

#ifdef NOGATHER
#define MAXCONTIG       1
#else
#define MAXCONTIG       16
#endif

#ifdef USE_BC_DIRTY
   while ((contig < MAXCONTIG) && ((tmp = exos_bufcache_lookup (entry->header.dev, (diskBlock + contig))) != NULL) && (tmp->buf_dirty) && (!tmp->buf_tainted)) {
#else
   while ((contig < MAXCONTIG) && ((tmp = (buffer_t *) alfs_buffertab_findDiskBlock (alfs_localCache, entry->header.dev, (diskBlock + contig))) != NULL) && (tmp->inUse == 0) && (tmp->flags & BUFFER_DIRTY) && (!tmp->bc_entry->buf_tainted)) {
      tmp->flags &= ~BUFFER_DIRTY;
#endif
      contig++;
   }

#ifdef USE_BC_DIRTY
   while ((contig < MAXCONTIG) && ((tmp = exos_bufcache_lookup(entry->header.dev, (diskBlock - 1))) != NULL) && (tmp->buf_dirty) && (!tmp->buf_tainted)) {
#else
   while ((contig < MAXCONTIG) && ((tmp = (buffer_t *) alfs_buffertab_findDiskBlock (alfs_localCache, entry->header.dev, (diskBlock - 1))) != NULL) && (tmp->inUse ==
 0) && (tmp->flags & BUFFER_DIRTY) && (!tmp->bc_entry->buf_tainted)) {
      tmp->flags &= ~BUFFER_DIRTY;
#endif
      contig++;
      diskBlock--;
   }

   if ((ret = exos_bufcache_initwrite (entry->bc_entry->buf_dev, diskBlock, contig, ((!async) ? &entry->buf.b_resid : NULL))) != 0) {
      kprintf ("default_writeBack: bc_write_dirty_bufs failed (ret %d, dev %d, blk %d, contig %d)\n", ret, entry->bc_entry->buf_dev, entry->bc_entry->buf_blk, contig);
#ifdef USE_BC_DIRTY
printf ("this can legally happen now...\n");
#endif
      assert (0);
   }

   if (!async) {
      exos_bufcache_waitforio (entry->bc_entry);
   } else {
      entry->buf.b_resid = -1;
   }

}


void ALFS_DISK_READ (struct buf *buf, char *memaddr, uint fsdev, uint blkno)
{
   struct bc_entry *bc_entry;

restart:

   if ((bc_entry = exos_bufcache_lookup(fsdev, blkno)) != NULL) {
      char *contents = exos_bufcache_map (bc_entry, fsdev, blkno, 0);
      if (contents == NULL) {
         goto restart;
      }
      exos_bufcache_waitforio (bc_entry);
      bcopy (contents, memaddr, BLOCK_SIZE);
      exos_bufcache_unmap (fsdev, blkno, contents);
   } else {
      exos_bufcache_initfill (fsdev, blkno, 1, NULL);
      goto restart;
   }
}


void ALFS_DISK_WRITE (struct buf *buf, char *memaddr, uint fsdev, uint blkno)
{
   struct bc_entry *bc_entry;

restart:

   if ((bc_entry = exos_bufcache_lookup(fsdev, blkno)) != NULL) {
      char *contents = exos_bufcache_map (bc_entry, fsdev, blkno, 1);
      if (contents == NULL) {
         goto restart;
      }
      exos_bufcache_waitforio (bc_entry);
      bcopy (memaddr, contents, BLOCK_SIZE);
      exos_bufcache_setDirty (fsdev, blkno, 1);
      exos_bufcache_initwrite (fsdev, blkno, 1, NULL);
      exos_bufcache_unmap (fsdev, blkno, contents);
   } else {
      exos_bufcache_alloc (fsdev, blkno, 0, 1, 0);
      goto restart;
   }
}


void alfs_defaultcache_moveblock (uint dev, uint oldblock, uint newblock)
{
   struct Xn_xtnt src, dst;
#if 0
   int ret;
#endif

   src.xtnt_block = oldblock;
   src.xtnt_size = 1;
   dst.xtnt_block = newblock;
   dst.xtnt_size = 1;
   assert (0);
#if 0
   if ((ret = sys_bc_move (&__sysinfo.si_pxn[dev], CAP_ROOT, 0, &src, &__sysinfo.si_pxn[dev], CAP_ROOT, 0, &dst, 1)) != 0) {
      printf ("cffs_alloc_doCollocation: bc_move failed (ret %d, oldblock %d, newblock %d)\n", ret, oldblock, newblock);
      assert (0);
   }
#endif

}

