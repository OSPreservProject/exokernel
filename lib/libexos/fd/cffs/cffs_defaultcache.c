
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
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <ubb/kernel/virtual-disk.h>
#include <ubb/kernel/kernel.h>
#include <ubb/registry.h>

#ifdef JJ
#define NBPG	XN_BLOCK_SIZE
#include "exobc.h"
#include <ubb/lib/demand.h>
#else
#include <xok/pxn.h>
#include <xok/bc.h>
#include <xok/sysinfo.h>
#include <xok/kerrno.h>
#undef PAGESIZ
#include <exos/vm-layout.h>
#include <exos/cap.h>
#include <assert.h>
#endif

#include <exos/uwk.h>
#include <exos/bufcache.h>

#define WRITEONLY
/*
#define NOSCATTER
#define NOGATHER
*/

int cffs_diskwrites = 0;
int cffs_diskreads = 0;

#ifdef XN
static int usexn = 1;
#else
static int usexn = 0;
#endif


#if 0
static uint latestdev = 0xFFFF0000;
static uint latestdb = 0xFFFF0000;
static char * latestaddr = NULL;
static struct bc_entry * latestbc = NULL;
static buffer_t * latestbuf = NULL;

void cffs_defaultcache_checkfunc (bufferCache_t *cache, bufferHeader_t *buf, int arg)
{
   buffer_t *entry = (buffer_t *) buf;
   if (entry != latestbuf) {
      if ((latestbc == entry->bc_entry) || (latestaddr == entry->buffer)) {
         kprintf ("got a match -- should not be possible! (addr %p == %p)\n", latestaddr, entry->buffer);
         assert (0);
      }
   }
}
#endif


/* get a new buffer for a block */

buffer_t *default_buffer_handleMiss (block_num_t dev, block_num_t sprblk, int inodeNum, int block, int flags, int *error) {
   int allocated = 0;
   inode_t *inode = NULL;
   buffer_t *entry;
   block_num_t diskBlock;
   struct bc_entry *bc_entry;
   int writeable = 1;
   int rangesize = 1;
   int rangestart;
   int ret;
/*
printf("cffs_buffer_handleMiss: buf %p, dev %d, inodeNum %x, block %d, flags %x\n", entry, dev, inodeNum, block, flags);
*/

	/* GROK -- this should exist, but move it into exos_bufcache.c */
#ifndef JJ
   assert ((BUFCACHE_REGION_START + ((__sysinfo.si_nppages+1) * NBPG)) <= BUFCACHE_REGION_END);
#endif

   if ((block < 0) || (inodeNum == 1) || (inodeNum == 2)) {     /* superblock, alloc map, or indirect block */

      assert (block);
      if (block < 0) {
         diskBlock = -block;
      } else {
         diskBlock = block;
      }
   } else if (inodeNum < 0) {	  /* directory block */
     diskBlock = block;
     //     inode = cffs_inode_getInode (-inodeNum, dev, sprblk, BUFFER_READ);
   } else {                       /* file  block */

     if (sprblk == 0)
       return NULL;

      inode = cffs_inode_getInode (inodeNum, dev, sprblk, BUFFER_READ);
      diskBlock = cffs_inode_offsetToBlock (inode, (block * BLOCK_SIZE), ((flags & BUFFER_ALLOCSTORE) ? &allocated : NULL), error);
      if (error && *error == -1) {
	cffs_inode_releaseInode (inode, 0);
	return NULL;
      }
      if (diskBlock == 0)
	return NULL;
   }

   entry = (buffer_t *)cffs_buffertab_getEmptyEntry (localCache, dev, inodeNum, block);
   if (entry == NULL || entry->buffer != NULL) {
     return NULL;
   }

   entry->flags = 0;  /* not BUFFER_RDONLY */
   entry->bc_entry = NULL;

/*
printf ("entry->buffer %p (block %d, inodeNum %d, flags %x)\n", entry->buffer, block, inodeNum, (uint)flags);
*/
   entry->header.dev = dev;
   entry->header.inodeNum = inodeNum;
   entry->header.block = block;
   entry->resid = -1;
   entry->inUse = 0;

   entry->header.diskBlock = diskBlock;

default_buffer_handleMiss_retry:

   if (diskBlock == 0) {
	/* diskBlock will be 0 only if this is a hole in the file.  Just    */
	/* fill is with zeroes.                                             */
	/* GROK -- this currently can cause a consistency problem if cached */
        /* locally.                                                         */

      kprintf ("fix the locally cached hole problem, bozo\n");
      assert (0);
      bzero(entry->buffer, BLOCK_SIZE);

   } else if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) != NULL) {
	/* found the desired block in the bufcache */

#if defined(WRITEONLY) && defined(CFFS_PROTECTED)
      writeable = (inode) ? (!cffs_inode_isDir(inode)) : 0;
#endif

      entry->bc_entry = bc_entry;
      entry->buffer = exos_bufcache_map (bc_entry, dev, diskBlock, writeable);

      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }
      assert (bc_entry->buf_blk == diskBlock);

//kprintf ("found it (diskBlock %d), now wait for it (%p, ppn %d)\n", diskBlock, entry->buffer, PGNO(vpt[PGNO(entry->buffer)]));

      exos_bufcache_waitforio (bc_entry);

	/* Make sure we don't get old crap (if allocating new). */
	/* allocated can't be true if inode is NULL. */
      if ((allocated) && (!cffs_inode_isDir(inode)) && (flags & BUFFER_READ)) {
         bzero (entry->buffer, BLOCK_SIZE);
      }

   } else if ((allocated) || !(flags & BUFFER_READ)) {
	/* Newly allocated block or block whose contents will be completely */
	/* overwritten.  No disk read is needed; instead, insert it into    */
	/* cache, and bzero it if contents will be used (ie, READ).         */

      int zerofill = (flags & BUFFER_READ) || (inode == NULL) || (cffs_inode_isDir(inode));
      writeable = (inode) ? (!cffs_inode_isDir(inode)) : 0;
      entry->buffer = (char *) exos_bufcache_alloc (dev, diskBlock, zerofill, writeable, usexn);
      if (entry->buffer == NULL) {
	/* in case it failed for lack of free pages */
         wk_waitfor_freepages ();
         goto default_buffer_handleMiss_retry;
      }
      if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) == NULL) {
         kprintf ("lookup failed after `successful' alloc\n");
         assert (0);
      }
	/* GROK -- this kind of assert needs to go away... */
      assert (bc_entry->buf_ppn == (((u_int)entry->buffer - BUFCACHE_REGION_START)/NBPG));
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
         if ((block == 0) && (cffs_inode_getGroupsize(inode) > 1)) {
            rangesize = cffs_inode_getGroupsize (inode);
            rangestart = cffs_inode_getGroupstart (inode);
         } else {
            rangesize = 1 + cffs_inode_getNumContig(inode, block);
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

      cffs_diskreads++;
      ret = exos_bufcache_initfill (dev, firstblock, (lastblock-firstblock+1), &entry->resid);
      assert (ret != -E_INVAL);
      if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) == NULL) {
         if (ret == -E_NO_MEM) {
            wk_waitfor_freepages();
         }
         goto default_buffer_handleMiss_retry;
         //kprintf ("Didn't really initiate request\n");
         //assert (0);
      }
      entry->bc_entry = bc_entry;
#if defined(WRITEONLY) && defined(CFFS_PROTECTED)
      writeable = (inode) ? (!cffs_inode_isDir(inode)) : 0;
#endif
      entry->buffer = exos_bufcache_map (bc_entry, dev, diskBlock, writeable);
      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }

      exos_bufcache_waitforio (bc_entry);
      }

   } else {
      kprintf ("cffs_defaultcache_handleMiss: this case shouldn't happen...\n");
      assert (0);
   }

   if (inode != NULL) {
      cffs_inode_releaseInode (inode, 0);
   }

   if ((allocated) && (! exos_bufcache_isdirty(entry->bc_entry))) {
      ret = exos_bufcache_setDirty (dev, diskBlock, 1);
      assert (ret == 0);
   }

#ifndef JJ
	/* GROK -- me go away.  This is an exos_bufcache.c internal thing. */
   assert (entry->bc_entry->buf_ppn == (((u_int)entry->buffer - BUFCACHE_REGION_START)/NBPG));
#endif
   assert (entry->bc_entry->buf_blk == entry->header.diskBlock);

#if 0
{
  latestdev = dev;
  latestdb = diskBlock;
  latestaddr = entry->buffer;
  latestbc = entry->bc_entry;
  latestbuf = entry;
  cffs_buffer_execOnAll (cffs_defaultcache_checkfunc, diskBlock);
}
#endif

   return (entry);
}


void default_buffer_flushEntry (buffer_t *entry)
{
   int ret;

	/* GROK -- the second clause of this assert inexactly excuses cases */
	/* where the first file block gets moved (by some other process) in */
	/* favor a directory block (via the grouping algorithms).           */

   if (! (entry->bc_entry->buf_blk == entry->header.diskBlock) ) {
      kprintf ("bc_entry other than expected (buf_blk %d, diskBlock %d, block %d, ino %x\n", entry->bc_entry->buf_blk, entry->header.diskBlock, entry->header.block, entry->header.inodeNum);
      kprintf ("bc_entry->buf_state %x, buf_dev %d\n", entry->bc_entry->buf_state, entry->bc_entry->buf_dev);
      kprintf ("bc_entry->buf_ppn %d, ptr's pp %d\n", entry->bc_entry->buf_ppn, PGNO(vpt[PGNO(entry->buffer)]));
   }
   assert ((entry->bc_entry->buf_blk == entry->header.diskBlock) || (entry->header.block == 0));

   /* unmap the block from our address space */
   ret = exos_bufcache_unmap (entry->bc_entry->buf_dev, entry->bc_entry->buf_blk, entry->buffer);
   if (ret != 0) {
      kprintf ("unmap failed: ret %d, buf_dev %d, buf_blk %d, buf_state %x, buf_ppn %d, bc_entry %p, mappgno %d\n", ret, entry->bc_entry->buf_dev, entry->bc_entry->buf_blk, entry->bc_entry->buf_state, entry->bc_entry->buf_ppn, entry->bc_entry, PGNO(vpt[PGNO(entry->buffer)]));
   }
   assert (ret == 0);
   entry->bc_entry = NULL;
   entry->buffer = NULL;
}


void default_buffer_writeBack_withlock (buffer_t *entry)
{
   int ret;
/*
printf("cffs_buffer_writeBack_withlock: inodeNum %x, block %d, diskBlock %d\n", entry->header.inodeNum, entry->header.block, diskBlock);
*/
   assert ((entry->bc_entry->buf_blk == entry->header.diskBlock) || (entry->header.block == 0));
   cffs_diskwrites++;

   if ((ret = exos_bufcache_initwrite (entry->bc_entry->buf_dev, entry->bc_entry->buf_blk, 1, &entry->resid)) != 0) {
      printf ("default_writeBack: bc_write_dirty_bufs failed (ret %d, dev %d, blk %d)\n", ret, entry->bc_entry->buf_dev, entry->bc_entry->buf_blk);
      assert (0);
   }

   exos_bufcache_waitforio (entry->bc_entry);
}


void default_buffer_writeBack (buffer_t *entry, int async)
{
   int ret;
   block_num_t diskBlock = entry->header.diskBlock;
   struct bc_entry *tmp;
   int contig = 1;
/*
printf("cffs_buffer_writeBack: inodeNum %x, block %d, diskBlock %d\n", entry->header.inodeNum, entry->header.block, diskBlock);
*/
   if (entry->bc_entry->buf_tainted) {
	/* GROK -- this problem, which is related to incomplete integration */
	/* with an incomplete XN, can't really be solved at this point.     */
	/* Just mark the tainted block as dirty, leave it for the syncer,   */
	/* and hope that a poorly-timed crash doesn't cause nasty stuff to  */
	/* happen during fsck...  One real solution would be to write out   */
	/* the sequence of dependent blocks (to untaint this block).        */
      //kprintf ("Explicitly trying to initiate a write on a tainted block!! (inodeNum 0x%x, block %d, buf_dirty %d, buf_tainted %d\n", entry->header.inodeNum, entry->header.block, entry->bc_entry->buf_dirty, entry->bc_entry->buf_tainted);
      ret = exos_bufcache_setDirty (entry->header.dev, entry->header.diskBlock, 1);
      assert (ret == 0);
      return;
   }

   /* GROK -- case of not-yet-alloced space is not handled! */
   assert (diskBlock > 0);
   cffs_diskwrites++;

   if (! (entry->bc_entry->buf_blk == entry->header.diskBlock) ) {
      kprintf ("bc_entry other than expected (buf_blk %d, diskBlock %d, block %d, ino %x\n", entry->bc_entry->buf_blk, entry->header.diskBlock, entry->header.block, entry->header.inodeNum);
      kprintf ("bc_entry->buf_state %x, buf_dev %d\n", entry->bc_entry->buf_state, entry->bc_entry->buf_dev);
      kprintf ("bc_entry->buf_ppn %d, ptr's pp %d\n", entry->bc_entry->buf_ppn, PGNO(vpt[PGNO(entry->buffer)]));
   }
   assert ((entry->bc_entry->buf_blk == entry->header.diskBlock) || (entry->header.block == 0));

#ifdef NOGATHER
#define MAXCONTIG	1
#else
#define MAXCONTIG	16
#endif

   while ((contig < MAXCONTIG) && ((tmp = exos_bufcache_lookup (entry->header.dev, (diskBlock + contig))) != NULL) && (exos_bufcache_isdirty(tmp)) && (!tmp->buf_tainted)) {
      contig++;
   }

   while ((contig < MAXCONTIG) && ((tmp = exos_bufcache_lookup(entry->header.dev, (diskBlock - 1))) != NULL) && (exos_bufcache_isdirty(tmp)) && (!tmp->buf_tainted)) {
      contig++;
      diskBlock--;
   }

   if ((ret = exos_bufcache_initwrite (entry->bc_entry->buf_dev, diskBlock, contig, &entry->resid)) != 0) {
      kprintf ("default_writeBack: bc_write_dirty_bufs failed (ret %d, dev %d, blk %d, contig %d)\n", ret, entry->bc_entry->buf_dev, entry->bc_entry->buf_blk, contig);
      kprintf ("depending on the error, this can legally happen now...\n");
      assert (0);
   }

   if (!async) {
      exos_bufcache_waitforio (entry->bc_entry);
   }
}


u_int default_buffer_revoker (u_int pagecnt, u_int priority)
{
   int cnt = cffs_buffertab_revokeFreeEntries (localCache, pagecnt);
   kprintf ("default_buffer_revoker: asked for %d at %d and gave %d\n", pagecnt, priority, cnt);
   return (cnt);
}

