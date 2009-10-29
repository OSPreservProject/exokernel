
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

/* local (per-application) buffer cache */

#include "alfs_buffer.h"
#include "alfs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

bufferCache_t *alfs_localCache = NULL;

buffer_t *alfs_lastBuffer = NULL;

static void affectBuffer (bufferCache_t *, bufferHeader_t *, int);
static int dumpBlock (void *);
static bufferHeader_t *newBuf (void *);

static buffer_t * (*alfs_buffer_handleMiss) (block_num_t, block_num_t, int, int);
static void (*alfs_buffer_flushEntry) (buffer_t *);
static void (*alfs_buffer_writeBack) (buffer_t *, int);

/* initialize buffer cache */

void alfs_buffer_initBufferCache (unsigned int numQueues, int maxBufferCount, buffer_t *(*handleMiss)(block_num_t,block_num_t,int,int), void (*flushEntry)(buffer_t *), void (*writeBack)(buffer_t *,int)) {
    alfs_localCache = buffertab_init (numQueues, maxBufferCount, newBuf, dumpBlock);
    alfs_buffer_handleMiss = handleMiss;
    alfs_buffer_flushEntry = flushEntry;
    alfs_buffer_writeBack = writeBack;
}

/* allocate a new buffer */
static bufferHeader_t *newBuf (void *entry) {
     buffer_t *new = (buffer_t *) malloc_and_alloc(sizeof(buffer_t));
     new->buffer = NULL;
     return((bufferHeader_t *) new);
}

/* Find a buffer in the buffer cache. If the buffer doesn't exists,
   allocate it. Tell the user whether the buffer contains valid data
   or not. We acuire the buffer with the desired capability (read only
   or read-write). */

buffer_t *alfs_buffer_getBlock (block_num_t dev, block_num_t inodeNum, int block, int flags)
{
    buffer_t *entry;
/*
printf("alfs_buffer_getBlock: dev %d, inodeNum %x, block %d, flags %x, ", dev, inodeNum, block, flags);
*/
    assert(inodeNum);
    entry = (buffer_t *) alfs_buffertab_findEntry (alfs_localCache, dev, inodeNum, block, FIND_ALLOCATE);
    if (entry) {
/*
printf("hit\n");
*/
         if (flags & BUFFER_MISSONLY) {
            if (entry->inUse == 0) {
               alfs_buffertab_markFree(alfs_localCache, (bufferHeader_t *) entry);
            }
            return (NULL);
         }

         /* make sure we're not trying to get write access to a block
            that we only have read permission for. */

         if ((flags & BUFFER_WITM) && (entry->flags & BUFFER_RDONLY)) {
              assert (0);	/* sorry, no write permission */
         }

         if ((entry->buf.b_resid == 0) && (!(flags & BUFFER_ASYNC))) {
            while ((disk_poll()) && (entry->buf.b_resid == 0)) ;
         }
  
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
         entry = alfs_buffer_handleMiss (dev, inodeNum, block, (flags | BUFFER_ALLOCMEM));
         assert (entry);

     }
     
     /* remember that someone is using this buffer */
     entry->inUse++;

     alfs_lastBuffer = entry;

     return (entry);
}

/* say we're done with a buffer */

void alfs_buffer_releaseBlock (buffer_t *entry, int flags) {
     int block = entry->header.block;
     int inodeNum = entry->header.inodeNum;
     assert (entry);

     assert(entry == (buffer_t *)alfs_buffertab_findEntry (alfs_localCache, entry->header.dev, inodeNum, block, FIND_VALID));
/*
printf("alfs_buffer_releaseBlock: dev %d, inodeNum %d, block %d, flags %x, inUse %d\n", entry->header.dev, inodeNum, block, flags, entry->inUse);
*/
     if (flags & BUFFER_WRITE) {
         alfs_buffer_writeBack (entry, (flags & BUFFER_ASYNC));
         entry->flags &= ~BUFFER_DIRTY;
     } else if (flags & BUFFER_DIRTY) {
         entry->flags |= BUFFER_DIRTY;
     }

     entry->inUse--;
     assert (entry->inUse >= 0);

     /* if no one is using the buffer, put it on the free list */
     if (entry->inUse == 0) {
         if (flags & BUFFER_INVALID) {
             /* NOTE: if buffer is dirty, this will not write it back */
             alfs_buffer_flushEntry (entry);
             alfs_buffertab_markInvalid (alfs_localCache, (bufferHeader_t *) entry);
/*
             entry->buffer = NULL;
*/
             entry->flags &= ~BUFFER_DIRTY;
             entry->header.inodeNum = 0;
             entry->header.diskBlock = 0;
         } else {
             alfs_buffertab_markFree(alfs_localCache, (bufferHeader_t *) entry);
         }
     }
}

static void affectBuffer (bufferCache_t *cache, bufferHeader_t *entryHeader, int flags)
{
   buffer_t *entry = (buffer_t *) entryHeader;
/*
printf("AFFECTBUFFER: dev %x, inodeNum %d, block %d, flags %x\n", entry->header.dev, entry->header.inodeNum, entry->header.block, flags);
*/
   if ((flags & BUFFER_WRITE) && (entry->flags & BUFFER_DIRTY)) {
      alfs_buffer_writeBack (entry, (flags & BUFFER_ASYNC));
      entry->flags &= ~BUFFER_DIRTY;
   }
   if ((flags & BUFFER_INVALID) && (entry->inUse == 0)) {
      alfs_buffer_flushEntry (entry);
      alfs_buffertab_markInvalid (cache, entryHeader);
/*
      entry->buffer = NULL;
*/
      entry->flags &= ~BUFFER_DIRTY;
      entry->header.inodeNum = 0;
      entry->header.diskBlock = 0;
   }
}

/* act on a specific cache entry (if present) */

int alfs_buffer_affectBlock (block_num_t dev, block_num_t inodeNum, int block, int flags) {
     buffer_t *entry;
     int ret = NOTOK;

     entry = (buffer_t *)alfs_buffertab_findEntry (alfs_localCache, dev, inodeNum, block, FIND_VALID);
     if (entry != NULL) {
/* GROK -- wanted?
         if (entry->inUse) {
              return (NOTOK);
         }
*/
         affectBuffer (alfs_localCache, (bufferHeader_t *) entry, flags);
         ret = OK;
     }
     return (ret);
}

/* act on all cache entries for a file */

void alfs_buffer_affectFile (block_num_t dev, block_num_t inodeNum, int flags, int notneg)
{
   alfs_buffertab_affectFile (alfs_localCache, dev, inodeNum, affectBuffer, flags, notneg);
}

/* act on all cache entries for a device */

void alfs_buffer_affectDev (block_num_t dev, int flags)
{
   alfs_buffertab_affectDev (alfs_localCache, dev, affectBuffer, flags);
}

/* act on all cache entries */

void alfs_buffer_affectAll (int flags)
{
   alfs_buffertab_affectAll (alfs_localCache, affectBuffer, flags);
}
     
/* tell the cache server we're no longer interested in a block */

static int dumpBlock (void *e) {
     buffer_t *entry = (buffer_t *)e;
/*
printf("dumpBlock: inodeNum %d, block %d, flags %x\n", entry->header.inodeNum, entry->header.block, entry->flags);
*/
     if (entry->flags & BUFFER_DIRTY) {
         alfs_buffer_writeBack (entry, 0);
         entry->flags &= ~BUFFER_DIRTY;
     }
     alfs_buffer_flushEntry (entry);
/*
     entry->buffer = NULL;
*/
     entry->header.inodeNum = 0;
     entry->header.diskBlock = 0;
     return  (OK);
}

/* write out any dirty data in the cache and free all memory */

void alfs_buffer_shutdownBufferCache () {
    buffertab_shutdown (alfs_localCache);
}

void alfs_buffer_printBlock (bufferHeader_t *bufHead)
{
    buffer_t *buffer = (buffer_t *) bufHead;
    printf ("buf %x, dev %x, inodeNum %d, block %d, inUse %d, diskBlock %d\n", (u_int) buffer, buffer->header.dev, buffer->header.inodeNum, buffer->header.block, buffer->inUse, buffer->header.diskBlock);
}

void alfs_buffer_printCache ()
{
    alfs_buffertab_printContents (alfs_localCache, alfs_buffer_printBlock);
}

