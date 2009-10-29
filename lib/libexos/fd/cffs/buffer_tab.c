
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

/* manage a hash table and free list of disk buffers */

#include "buffer_tab.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <exos/mallocs.h>
#include <xok/sys_ucall.h>

typedef struct {
    bufferHeader_t *freeList;		/* tail of list of free buffers */
    bufferHeader_t *invalidList;	/* tail of list of invalid buffers */
    int numBuffers;			/* number of allocated buffer */
    int maxBuffers;			/* max number of allocated buffers */
    bufferHeader_t *(*newFunc)(void *);	/* called to allocate bufferHeader */
    int (*dumpFunc)(void *);		/* called when buffer reclaimed */
    int numQueues;			/* number of hash queues */
    bufferHeader_t *bufferQueues[1];	/* array of lists of buffers */
} bufCache_t;

static inline int hashBlock (int, int, unsigned int);
static void addToQueue (bufferHeader_t *, bufferHeader_t **);
static void removeFromQueue (bufferHeader_t *, bufferHeader_t **);
static void addToFreeList (bufferHeader_t *, bufferHeader_t **);
static void removeFromFreeList (bufferHeader_t *, bufferHeader_t **);

/* allocate the array of pointers to lists of buffer */
/* works best when "queues" is a power of two */
bufferCache_t * initBufferTab (int queues, int max, bufferHeader_t *(*new)(void *), int (*dump)(void *)) {
     bufCache_t *bufCache = (bufCache_t *) __malloc (sizeof(bufCache_t) + ((queues-1)*sizeof(bufferHeader_t *)));
     bufCache->numBuffers = 0;
     bufCache->maxBuffers = max;
     bufCache->newFunc = new;
     bufCache->dumpFunc = dump;
     bufCache->freeList = NULL;
     bufCache->invalidList = NULL;
     bufCache->numQueues = queues;
     bzero ((char *) &bufCache->bufferQueues, queues*sizeof (bufferHeader_t *));
     return ((bufferCache_t *) bufCache);
}

/* free all memory associated with buffer tab */
void shutdownBufferTab (bufferCache_t *bufferCache) {
   bufCache_t *bufCache = (bufCache_t *) bufferCache;
   int queue;
   bufferHeader_t *tmp;

   assert (bufCache);
   for (queue = 0; queue < bufCache->numQueues; queue++) {
      for (tmp = bufCache->bufferQueues[queue]; tmp != NULL; tmp = tmp->prev) {
         if (tmp->inodeNum != 0) {
            assert ((*bufCache->dumpFunc) (tmp) == OK);
#if 0
            __free (tmp);
#endif
         }
      }
   }
#if 0
   while ((tmp = bufCache->invalidList) != NULL) {
      removeFromFreeList (tmp, &bufCache->invalidList);
      __free (tmp);
   }
#endif
   __free (bufCache);
}

/* compute a hash value given a block numnber */

static inline int hashBlock (int inodeNum, int block, unsigned int numQueues) {
     return ((inodeNum + block) & (numQueues-1));
}

/* find a buffer given a block number. Command is one of FIND_VALID
   meaning find only buffers with valid data, FIND_EMPTY
   meaning find an unused buffer or FIND_ALLOCATE meaning find
   a valid buffer and remove it from the free list. */

bufferHeader_t *cffs_buffertab_findEntry (bufferCache_t *bufferCache, block_num_t dev, int inodeNum, int block, int command) {
     bufCache_t *bufCache = (bufCache_t *) bufferCache;
     int queue;
     bufferHeader_t *tmp;

     assert (bufCache);
/*
printf("cffs_buffertab_findEntry: dev %d, inodeNum %x, block %d, command %d\n", dev, inodeNum, block, command);
*/
     queue = hashBlock (inodeNum, block, bufCache->numQueues);
     for (tmp = bufCache->bufferQueues[queue]; tmp != NULL; tmp = tmp->prev) {
         if ((tmp->block == block) && (tmp->inodeNum == inodeNum) && (tmp->dev == dev)) {
             /* pull off of free list, if needed -- AND IF ON FREE LIST! */
             if ((command == FIND_ALLOCATE) && (tmp->nextFree)) {
	          removeFromFreeList (tmp, &bufCache->freeList);
             }
             return (tmp);
         }
    }
    return (NULL);	/* didn't find the buffer */
}


static bufferHeader_t *cffs_buffertab_reclaimFreeEntry (bufCache_t *bufCache)
{
   bufferHeader_t *tmp = bufCache->freeList;

   if (tmp != NULL) {
      /* pull the first element off the free list */
      removeFromFreeList (tmp, &bufCache->freeList);
      /* remove from hash queue so that we can add it to the right one */
      removeFromQueue (tmp, &bufCache->bufferQueues[hashBlock (tmp->inodeNum, tmp->block, bufCache->numQueues)]);

      /* dump out the block if it's still allocated */
      /* GROK -- this may cause awkward problems if writeback is needed */
      if (tmp->inodeNum != 0) {
         assert ((*bufCache->dumpFunc) (tmp) == OK);
      }
   }
   return (tmp);
}


bufferHeader_t *cffs_buffertab_getEmptyEntry (bufferCache_t *bufferCache, block_num_t dev, int inodeNum, int block) {
     bufCache_t *bufCache = (bufCache_t *) bufferCache;
     int queue;
     bufferHeader_t *tmp;

     assert (bufCache);

     /* Three cases: we're looking for a block in the buffer cache, we're
	looking for a free block and the buffer cache is smaller than
	it's max size so we can allocate a new buffer, or we need
	a new block and the buffer cache is full so we need to replace
	a buffer. */

     tmp = bufCache->invalidList;
     if (tmp != NULL) {
          removeFromFreeList (tmp, &bufCache->invalidList);
     } else if (bufCache->numBuffers < bufCache->maxBuffers) {
          /* we can allocate a new buffer */
          tmp = (*bufCache->newFunc)(tmp);
          assert (tmp);
          tmp->nextFree = tmp->prevFree = NULL;
          bufCache->numBuffers++;
     } else {		/* no more space -- need to replace buffer */
        tmp = cffs_buffertab_reclaimFreeEntry (bufCache);
     }

     if (tmp) {
        /* insert new buffer into queue */
        queue = hashBlock (inodeNum, block, bufCache->numQueues);
        addToQueue (tmp, &bufCache->bufferQueues[queue]);
     }

     return (tmp);
}


int cffs_buffertab_revokeFreeEntries (bufferCache_t *bufferCache, int cnt)
{
   bufCache_t *bufCache = (bufCache_t *) bufferCache;
   int i;

   for (i=0; i<cnt; i++) {
      bufferHeader_t *tmp = cffs_buffertab_reclaimFreeEntry (bufCache);
      if (tmp == NULL) {
         return (i);
      }
      addToFreeList (tmp, &bufCache->invalidList);
   }
   return (cnt);
}


int cffs_buffertab_renameEntry (bufferCache_t *bufferCache, bufferHeader_t *entry, block_num_t dev, int inodeNum, int block)
{
   bufCache_t *bufCache = (bufCache_t *) bufferCache;
   bufferHeader_t *tmp;
   int queue;

   assert (bufCache);
/*
kprintf("cffs_buffertab_renameEntry: dev %d, inodeNum %x, block %d (to %d %x %d)\n", entry->dev, entry->inodeNum, entry->block, dev, inodeNum, block);
*/
   queue = hashBlock (inodeNum, block, bufCache->numQueues);
   for (tmp = bufCache->bufferQueues[queue]; tmp != NULL; tmp = tmp->prev) {
      if ((tmp->block == block) && (tmp->inodeNum == inodeNum) && (tmp->dev == dev)) {
	/* it is an error to rename over an existing entry! */
	return -1;
      }
   }

   removeFromQueue (entry, &bufCache->bufferQueues[hashBlock (entry->inodeNum, entry->block, bufCache->numQueues)]);

   entry->dev = dev;
   entry->inodeNum = inodeNum;
   entry->block = block;

   /* insert renamed buffer into new queue */
   queue = hashBlock (inodeNum, block, bufCache->numQueues);
   addToQueue (entry, &bufCache->bufferQueues[queue]);
   return 0;
}


bufferHeader_t * cffs_buffertab_findDiskBlock (bufferCache_t *bufferCache, block_num_t dev, block_num_t diskBlock)
{
   bufCache_t *bufCache = (bufCache_t *) bufferCache;
   bufferHeader_t *entry;
   int i;

   assert (bufferCache);

   for (i=0; i<bufCache->numQueues; i++) {
      entry = bufCache->bufferQueues[i];
      while (entry != NULL) {
         if ((entry->dev == dev) && (entry->diskBlock == diskBlock)) {
            return (entry);
         }
         entry = entry->prev;
      }
   }
   if ((entry = bufCache->freeList)) {
      do {
         if ((entry->dev == dev) && (entry->diskBlock == diskBlock)) {
            return (entry);
         }
         entry = entry->nextFree;
      } while (entry != bufCache->freeList);
   }
   return (NULL);
}

void cffs_buffertab_affectFile (bufferCache_t *bufferCache, block_num_t dev, int inodeNum, void (*action)(bufferCache_t *, bufferHeader_t *, int), int flags, int notneg)
{
   bufCache_t *bufCache = (bufCache_t *) bufferCache;
   bufferHeader_t *entry;
   bufferHeader_t *prev;
   int i;

   assert (bufferCache);

   for (i=0; i<bufCache->numQueues; i++) {
      entry = bufCache->bufferQueues[i];
      while (entry != NULL) {
         prev = entry->prev;
         if ((entry->inodeNum == inodeNum) && (entry->dev == dev) && ((!notneg) || (entry->block >= 0))) {
            (*action) (bufferCache, entry, flags);
         }
         entry = prev;
      }
   }
   if ((entry = bufCache->freeList)) {
      do {
         prev = (entry != entry->nextFree) ? entry->nextFree : NULL;
         if ((entry->inodeNum == inodeNum) && (entry->dev == dev) && ((!notneg) || (entry->block >= 0))) {
            (*action) (bufferCache, entry, flags);
         }
         entry = prev;
      } while ((entry) && (entry != bufCache->freeList));
   }
}

void cffs_buffertab_affectDev (bufferCache_t *bufferCache, block_num_t dev, void (*action)(bufferCache_t *, bufferHeader_t *, int), int flags)
{
   bufCache_t *bufCache = (bufCache_t *) bufferCache;
   bufferHeader_t *entry;
   bufferHeader_t *prev;
   int i;

   assert (bufferCache);

   for (i=0; i<bufCache->numQueues; i++) {
      entry = bufCache->bufferQueues[i];
      while (entry != NULL) {
         prev = entry->prev;
         if (entry->dev == dev) {
            (*action) (bufferCache, entry, flags);
         }
         entry = prev;
      }
   }
   if ((entry = bufCache->freeList)) {
      do {
         prev = (entry != entry->nextFree) ? entry->nextFree : NULL;
         if (entry->dev == dev) {
            (*action) (bufferCache, entry, flags);
         }
         entry = prev;
      } while ((entry) && (entry != bufCache->freeList));
   }
}

void cffs_buffertab_affectAll (bufferCache_t *bufferCache, void (*action)(bufferCache_t *, bufferHeader_t *, int), int flags)
{
   bufCache_t *bufCache = (bufCache_t *) bufferCache;
   bufferHeader_t *entry;
   bufferHeader_t *prev;
   int i;

   assert (bufferCache);

   for (i=0; i<bufCache->numQueues; i++) {
      entry = bufCache->bufferQueues[i];
      while (entry != NULL) {
         prev = entry->prev;
         (*action) (bufferCache, entry, flags);
         entry = prev;
      }
   }
   if ((entry = bufCache->freeList)) {
      do {
         prev = (entry != entry->nextFree) ? entry->nextFree : NULL;
         (*action) (bufferCache, entry, flags);
         entry = prev;
      } while ((entry) && (entry != bufCache->freeList));
   }
}

void cffs_buffertab_markFree (bufferCache_t *bufferCache, bufferHeader_t *entry) {
    bufCache_t *bufCache = (bufCache_t *) bufferCache;
    assert (bufferCache);
    addToFreeList (entry, &bufCache->freeList);
}

void cffs_buffertab_markInvalid (bufferCache_t *bufferCache, bufferHeader_t *entry) {
    bufCache_t *bufCache = (bufCache_t *) bufferCache;
    assert (bufferCache);

    sys_bc_set_dirty64 (entry->dev, 0, entry->diskBlock, 0);
    sys_bc_flush64 (entry->dev, 0, entry->diskBlock, 1);
    if (entry->nextFree) {
       removeFromFreeList (entry, &bufCache->freeList);
    }
    removeFromQueue (entry, &bufCache->bufferQueues[hashBlock (entry->inodeNum, entry->block, bufCache->numQueues)]);
    addToFreeList (entry, &bufCache->invalidList);
}

static void addToQueue (bufferHeader_t *entry, bufferHeader_t **queueTailPtr) {
     bufferHeader_t *queueTail = *queueTailPtr;
     entry->next = NULL;
     entry->prev = queueTail;
     if (queueTail != NULL) {
	  queueTail->next = entry;
     }
     *queueTailPtr = entry;
}


static void removeFromQueue (bufferHeader_t *entry, bufferHeader_t **queueTailPtr) {
     if (entry->next != NULL) {
	  entry->next->prev = entry->prev;
     }
     if (entry->prev != NULL) {
	  entry->prev->next = entry->next;
     }
     if (entry == *queueTailPtr) {
	  *queueTailPtr = entry->prev;
     }
     entry->next = entry->prev = NULL;
}


/* LRU list using nextFree, prevFree.  The LRU entry is pointed to by	*/
/* listHead.  The MRU entry is pointed to by listHead->prevFree.	*/

static void addToFreeList (bufferHeader_t *entry, bufferHeader_t **listHeadPtr) {
     bufferHeader_t *listHead = *listHeadPtr;
     if (listHead) {
        entry->nextFree = listHead;
        entry->prevFree = listHead->prevFree;
        listHead->prevFree = entry;
        entry->prevFree->nextFree = entry;
     } else {
        entry->nextFree = entry;
        entry->prevFree = entry;
        *listHeadPtr = entry;
     }
}

static void removeFromFreeList (bufferHeader_t *entry, bufferHeader_t **listHeadPtr) {
    if (entry->nextFree == entry) {
        assert (*listHeadPtr == entry);
        *listHeadPtr = NULL;
    } else {
        entry->nextFree->prevFree = entry->prevFree;
        entry->prevFree->nextFree = entry->nextFree;
        if (*listHeadPtr == entry) {
            *listHeadPtr = entry->nextFree;
        }
    }
    entry->nextFree = NULL;
    entry->prevFree = NULL;
}

void cffs_buffertab_printContents (bufferCache_t *bufferCache, void (*print)(bufferHeader_t *))
{
     bufCache_t *bufCache = (bufCache_t *) bufferCache;
     bufferHeader_t *entry;
     int i;

     printf("Buffer cache contents: numBuffers %d\n", bufCache->numBuffers);
     for (i=0; i<bufCache->numQueues; i++) {
         entry = bufCache->bufferQueues[i];
         while (entry) {
             print (entry);
             entry = entry->prev;
         }
     }
     if ((entry = bufCache->freeList)) {
         printf("Free list (in LRU order)\n");
         do {
             print (entry);
             entry = entry->nextFree;
         } while (entry != bufCache->freeList);
     }
     if ((entry = bufCache->invalidList)) {
         printf("Invalid list (in LRU order)\n");
         do {
             print (entry);
             entry = entry->nextFree;
         } while (entry != bufCache->invalidList);
     }
}

