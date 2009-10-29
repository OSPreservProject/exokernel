
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


/* local (per-application) buffer cache */

#include "cffs.h"
#include "cffs_buffer.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <exos/mallocs.h>
#include <xok/sysinfo.h>

#ifndef JJ
#include <exos/bufcache.h>
#include <exos/vm.h>
#include <exos/cap.h>
#endif

bufferCache_t *localCache = NULL;

buffer_t *lastBuffer = NULL;

static void affectBuffer (bufferCache_t *, bufferHeader_t *, int);
static int dumpBlock (void *);
static bufferHeader_t *newBuf (void *);

static buffer_t * (*cffs_buffer_handleMiss) (block_num_t, block_num_t, int, int, int, int *);
static void (*cffs_buffer_flushEntry) (buffer_t *);
static void (*cffs_buffer_writeBack) (buffer_t *, int);
static void (*cffs_buffer_writeBack_withlock) (buffer_t *);

/* initialize buffer cache */

void cffs_buffer_initBufferCache (unsigned int numQueues, int maxBufferCount, buffer_t *(*handleMiss)(block_num_t,block_num_t,int,int,int,int *), void (*flushEntry)(buffer_t *), void (*writeBack)(buffer_t *,int), void (*writeBack_withlock)(buffer_t *)) {
    localCache = initBufferTab (numQueues, maxBufferCount, newBuf, dumpBlock);
    cffs_buffer_handleMiss = handleMiss;
    cffs_buffer_flushEntry = flushEntry;
    cffs_buffer_writeBack = writeBack;
    cffs_buffer_writeBack_withlock = writeBack_withlock;
}

/* allocate a new buffer */
static bufferHeader_t *newBuf (void *entry) {
   buffer_t *new;
   new = (buffer_t *) __malloc(sizeof(buffer_t));
   new->buffer = NULL;
   return((bufferHeader_t *) new);
}

/* Find a buffer in the buffer cache. If the buffer doesn't exists,
   allocate it. Tell the user whether the buffer contains valid data
   or not. We acuire the buffer with the desired capability (read only
   or read-write). */

buffer_t *cffs_buffer_getBlock (block_num_t dev, block_num_t sprblk, int inodeNum, int block, int flags, int *error)
{
    buffer_t *entry;
/*
printf("cffs_buffer_getBlock: dev %d, inodeNum %x, block %d, flags %x, ", dev, inodeNum, block, flags);
*/
    if (dev < 0 || dev >= __sysinfo.si_ndisks) {
      assert (0);
    }

    assert(inodeNum);
    entry = (buffer_t *) cffs_buffertab_findEntry (localCache, dev, inodeNum, block, FIND_ALLOCATE);
    if (entry) {
/*
printf("hit\n");
*/
         if (flags & BUFFER_MISSONLY) {
            if (entry->inUse == 0) {
               cffs_buffertab_markFree(localCache, (bufferHeader_t *) entry);
            }
            return (NULL);
         }

         /* make sure we're not trying to get write access to a block
            that we only have read permission for. */

         if ((flags & BUFFER_WITM) && (entry->flags & BUFFER_RDONLY)) {
              assert (0);	/* sorry, no write permission */
         }

#ifndef JJ
         assert (exos_bufcache_isvalid(entry->bc_entry));
         if ((exos_bufcache_isiopending(entry->bc_entry)) && (!(flags & BUFFER_ASYNC))) {
            exos_bufcache_waitforio (entry->bc_entry);
         }
#endif

#if 0		/* To check for wild writes... */
         if ((ret = sys_self_mod_pte_range (CAP_ROOT, PG_W, PG_RO, (uint)entry->buffer, 1)) < 0) {
            printf ("cffs_buffer_getBlock: mod_pte_range failed (ret %d, vaddr %p)\n", ret, entry->buffer);
            assert (0);
         }
#endif
  
     } else {
/*
printf("miss\n");
*/
         if (flags & BUFFER_HITONLY) {
            return (NULL);
         }

         /* find a slot in the buffer table to hold the new buffer */
/* GROK !!!!!! */
flags |= BUFFER_ALLOCSTORE;
         entry = cffs_buffer_handleMiss (dev, sprblk, inodeNum, block, (flags | BUFFER_ALLOCMEM), error);
	 if (entry == NULL || (error && *error == -1)) {
	   return NULL;
	 }
         assert (entry != NULL);
     }
     
     /* remember that someone is using this buffer */
     entry->inUse++;

     lastBuffer = entry;

     assert (dev == entry->header.dev);
     assert (entry->buffer != NULL);

     return (entry);
}


/* write out the buffer without releasing it */

void cffs_buffer_dirtyBlock (buffer_t *entry) {
   assert (entry != NULL);
   if ( ! (exos_bufcache_isdirty(entry->bc_entry))) {
      exos_bufcache_setDirty (entry->bc_entry->buf_dev, entry->bc_entry->buf_blk, 1);
   }
}


/* write out the buffer without releasing it */

void cffs_buffer_writeBlock (buffer_t *entry, int flags) {
   assert (entry != NULL);
   cffs_buffer_writeBack (entry, (flags & BUFFER_ASYNC));
}


/* write out the buffer without releasing it or the lock*/

void cffs_buffer_writeBlock_withlock (buffer_t *entry, int flags) {
   assert (entry != NULL);
   cffs_buffer_writeBack_withlock (entry);
}


/* write out the buffer without releasing it */

void cffs_buffer_bumpRefCount (buffer_t *entry) {
   assert (entry != NULL);
   entry->inUse++;
}

/* rename a buffer */

int cffs_buffer_renameEntry (buffer_t *entry, block_num_t dev, int inode, block_num_t block) {
  return (cffs_buffertab_renameEntry (localCache, (bufferHeader_t *)entry, dev, inode, block));
}

/* say we're done with a buffer */

void cffs_buffer_releaseBlock (buffer_t *entry, int flags) {
     assert (entry != NULL);
/*
printf("%d: cffs_buffer_releaseBlock: entry %p, dev %d, diskBlock %d, inodeNum %d, block %d, flags %x, inUse %d\n", getpid(), entry, entry->header.dev, entry->header.diskBlock, inodeNum, block, flags, entry->inUse);
     assert(entry == (buffer_t *)cffs_buffertab_findEntry (localCache, entry->header.dev, entry->header.inodeNum, entry->header.block, FIND_VALID));
*/

     if (flags & BUFFER_WRITE) {
         cffs_buffer_writeBack (entry, (flags & BUFFER_ASYNC));
     } else if (flags & BUFFER_DIRTY) {
        if ( ! exos_bufcache_isdirty(entry->bc_entry)) {
           exos_bufcache_setDirty (entry->bc_entry->buf_dev, entry->bc_entry->buf_blk, 1);
        }
     }

     entry->inUse--;
     if (entry->inUse < 0) {
        kprintf ("inUse %d, inodeNum %x, blkno %d, diskBlock %d, buffer %p\n", entry->inUse, entry->header.inodeNum, entry->header.block, entry->header.diskBlock, entry->buffer);
     }
     assert (entry->inUse >= 0);

#if 0		/* To check for wild writes */
     if (entry->inUse == 0) {
        if ((ret = sys_self_mod_pte_range (CAP_ROOT, PG_RO, PG_W, (uint)entry->buffer, 1)) < 0) {
           printf ("cffs_buffer_releaseBlock: mod_pte_range failed (ret %d, vaddr %p)\n", ret, entry->buffer);
           assert (0);
        }
     }
#endif

     /* if no one is using the buffer, put it on the free list */
     if (entry->inUse == 0) {
         if (flags & BUFFER_INVALID) {
             /* NOTE: if buffer is dirty, this will not write it back */
             cffs_buffer_flushEntry (entry);
	     cffs_buffer_markInvalid (entry);
         } else {
             cffs_buffertab_markFree(localCache, (bufferHeader_t *) entry);
         }
     } else if (flags & BUFFER_INVALID) {
        kprintf ("can't invalidate buffer that is still in use\n");
     }
}

void cffs_buffer_markInvalid (buffer_t *entry) {
  cffs_buffertab_markInvalid (localCache, (bufferHeader_t *)entry);
  entry->buffer = NULL;
  entry->header.inodeNum = 0;
  entry->header.diskBlock = 0;
}

static void affectBuffer (bufferCache_t *cache, bufferHeader_t *entryHeader, int flags)
{
   buffer_t *entry = (buffer_t *) entryHeader;
/*
printf("AFFECTBUFFER: dev %x, inodeNum %d, block %d, flags %x\n", entry->header.dev, entry->header.inodeNum, entry->header.block, flags);
*/
   if ((flags & BUFFER_WRITE) && (exos_bufcache_isdirty(entry->bc_entry))) {
      cffs_buffer_writeBack (entry, (flags & BUFFER_ASYNC));
   }
   if ((flags & BUFFER_INVALID) && (entry->inUse != 0)) {
/*
      extern cffs_inode_printInodeCache();
      printf ("Trying to invalidate, but inUse is not 0: inodeNum %x, block %d, diskBlock %d, inUse %d\n", entry->header.inodeNum, entry->header.block, entry->header.diskBlock, entry->inUse);
      printf ("INODE CACHE CONTENTS\n");
      cffs_inode_printInodeCache ();
*/
   } else if (flags & BUFFER_INVALID) {
/*
      printf ("invalidating: inodeNum %x, block %d, diskBlock %d\n", entry->header.inodeNum, entry->header.block, entry->header.diskBlock);
*/
      cffs_buffer_flushEntry (entry);
      cffs_buffer_markInvalid (entry);
   }
}

/* act on a specific cache entry (if present) */

int cffs_buffer_affectBlock (block_num_t dev, int inodeNum, int block, int flags) {
   buffer_t *entry;
   int ret = -1;

   entry = (buffer_t *)cffs_buffertab_findEntry (localCache, dev, inodeNum, block, FIND_VALID);
   if (entry != NULL) {
/* GROK -- wanted?
      if (entry->inUse) {
         return (-1);
      }
*/
      affectBuffer (localCache, (bufferHeader_t *) entry, flags);
      ret = OK;
   }
   return (ret);
}

/* act on all cache entries for a file */

void cffs_buffer_affectFile (block_num_t dev, int inodeNum, int flags, int notneg)
{
   cffs_buffertab_affectFile (localCache, dev, inodeNum, affectBuffer, flags, notneg);
}

/* act on all cache entries corresponding to blocks at or after firstBlock in the file */

void cffs_buffer_affectFileTail (block_num_t dev, block_num_t inodeNum, off_t newLength, off_t oldLength, int flags) {

  while (newLength/BLOCK_SIZE <= oldLength/BLOCK_SIZE) {
    cffs_buffer_affectBlock (dev, inodeNum, newLength/BLOCK_SIZE, flags);
    newLength += BLOCK_SIZE;
  }

}

/* act on all cache entries for a device */

void cffs_buffer_affectDev (block_num_t dev, int flags)
{
   cffs_buffertab_affectDev (localCache, dev, affectBuffer, flags);
}

/* act on all cache entries */

void cffs_buffer_affectAll (int flags)
{
   cffs_buffertab_affectAll (localCache, affectBuffer, flags);
}

/* execute provided function on all cache entries */

void cffs_buffer_execOnAll (void (*func)(bufferCache_t *, bufferHeader_t *, int), int arg)
{
   cffs_buffertab_affectAll (localCache, func, arg);
}

     
/* tell the cache server we're no longer interested in a block */

static int dumpBlock (void *e) {
     buffer_t *entry = (buffer_t *)e;
/*
printf("dumpBlock: inodeNum %d, block %d, flags %x\n", entry->header.inodeNum, entry->header.block, entry->flags);
*/
     if (exos_bufcache_isdirty (entry->bc_entry)) {
        cffs_buffer_writeBack (entry, BUFFER_ASYNC);
     }
     cffs_buffer_flushEntry (entry);
     entry->buffer = NULL;
     entry->header.inodeNum = 0;
     entry->header.diskBlock = 0;
     return  (OK);
}

/* write out any dirty data in the cache and free all memory */

void cffs_buffer_shutdownBufferCache () {
   shutdownBufferTab (localCache);
   localCache = NULL;
}

void cffs_buffer_printBlock (bufferHeader_t *bufHead)
{
    buffer_t *buffer = (buffer_t *) bufHead;
    printf ("buf %x, dev %x, inodeNum %d, block %d, inUse %d, diskBlock %d\n", (u_int) buffer, buffer->header.dev, buffer->header.inodeNum, buffer->header.block, buffer->inUse, buffer->header.diskBlock);
}

void cffs_buffer_printCache ()
{
    cffs_buffertab_printContents (localCache, cffs_buffer_printBlock);
}

buffer_t *cffs_get_superblock (u_int32_t dev, u_int32_t blk, u_int32_t flags) {
   struct buffer_t *buffer;

   buffer = cffs_buffer_getBlock (dev, blk, -(blk*32 + 1), blk, BUFFER_READ | flags, NULL);
   assert (buffer && buffer->buffer);

   return buffer;
}

