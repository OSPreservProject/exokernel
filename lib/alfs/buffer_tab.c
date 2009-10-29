
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
#include "alfs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

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

static inline int hashBlock (block_num_t, block_num_t, unsigned int);
static void addToQueue (bufferHeader_t *, bufferHeader_t **);
static void removeFromQueue (bufferHeader_t *, bufferHeader_t **);
static void addToFreeList (bufferHeader_t *, bufferHeader_t **);
static void removeFromFreeList (bufferHeader_t *, bufferHeader_t **);

/* allocate the array of pointers to lists of buffer */
/* works best when "queues" is a power of two */
bufferCache_t * buffertab_init (int queues, int max, bufferHeader_t *(*new)(void *), int (*dump)(void *)) {
     bufCache_t *bufCache = (bufCache_t *) malloc_and_alloc (sizeof(bufCache_t) + ((queues-1)*sizeof(bufferHeader_t *)));
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
void buffertab_shutdown (bufferCache_t *bufferCache) {
     bufCache_t *bufCache = (bufCache_t *) bufferCache;
     int queue;
     bufferHeader_t *current;

     assert (bufCache);
     for (queue = 0; queue < bufCache->numQueues; queue++) {
	  for (current = bufCache->bufferQueues[queue]; current != NULL; current = current->prev) {
	       if (current->inodeNum != 0) {
		    assert ((*bufCache->dumpFunc) (current) == OK);
	       }
	  }
     }
     free (bufCache);
}

/* compute a hash value given a block numnber */

static inline int hashBlock (block_num_t inodeNum, block_num_t block, unsigned int numQueues) {
     return ((inodeNum + block) & (numQueues-1));
}

/* find a buffer given a block number. Command is one of FIND_VALID
   meaning find only buffers with valid data, FIND_EMPTY
   meaning find an unused buffer or FIND_ALLOCATE meaning find
   a valid buffer and remove it from the free list. */

bufferHeader_t *alfs_buffertab_findEntry (bufferCache_t *bufferCache, block_num_t dev, block_num_t inodeNum, block_num_t block, int command) {
     bufCache_t *bufCache = (bufCache_t *) bufferCache;
     int queue;
     bufferHeader_t *current;

     assert (bufCache);
     /* Three cases: we're looking for a block in the buffer cache, we're
	looking for a free block and the buffer cache is smaller than
	it's max size so we can allocate a new buffer, or we need
	a new block and the buffer cache is full so we need to replace
	a buffer. */
/*
printf("alfs_buffertab_findEntry: dev %d, inodeNum %d, block %d, command %d\n", dev, inodeNum, block, command);
*/
     queue = hashBlock (inodeNum, block, bufCache->numQueues);
     for (current = bufCache->bufferQueues[queue]; current != NULL; current = current->prev) {
         if ((current->block == block) && (current->inodeNum == inodeNum) && (current->dev == dev)) {
             /* pull off of free list, if needed -- AND IF ON FREE LIST! */
             if ((command == FIND_ALLOCATE) && (current->nextFree)) {
	          removeFromFreeList (current, &bufCache->freeList);
             }
             return (current);
         }
    }
    return (NULL);	/* didn't find the buffer */
}


bufferHeader_t *alfs_buffertab_getEmptyEntry (bufferCache_t *bufferCache, block_num_t dev, block_num_t inodeNum, block_num_t block) {
     bufCache_t *bufCache = (bufCache_t *) bufferCache;
     int queue;
     bufferHeader_t *current;

     assert (bufCache);
     current = bufCache->invalidList;
     if (current != NULL) {
          removeFromFreeList (current, &bufCache->invalidList);
     } else if (bufCache->numBuffers < bufCache->maxBuffers) {
          /* we can allocate a new buffer */
          current = (*bufCache->newFunc)(current);
          assert (current);
          current->nextFree = current->prevFree = NULL;
          bufCache->numBuffers++;
     } else {		/* no more space -- need to replace buffer */
          if (bufCache->freeList == NULL) {
	      return (NULL); /* no buffers not in use */
          }
          /* pull the first element off the free list */
          current = bufCache->freeList;
          removeFromFreeList (current, &bufCache->freeList);
          /* remove from hash queue so that we can add it to the right one */
          removeFromQueue (current, &bufCache->bufferQueues[hashBlock (current->inodeNum, current->block, bufCache->numQueues)]);

          /* dump out the block if it's still allocated */
          /* GROK -- this may cause awkward problems if writeback is needed */
          if (current->inodeNum != 0) {
	       assert ((*bufCache->dumpFunc) (current) == OK);
          }
     }

     /* insert new buffer into queue */
     queue = hashBlock (inodeNum, block, bufCache->numQueues);
     addToQueue (current, &bufCache->bufferQueues[queue]);

     return (current);
}

bufferHeader_t * alfs_buffertab_findDiskBlock (bufferCache_t *bufferCache, block_num_t dev, block_num_t diskBlock)
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

void alfs_buffertab_affectFile (bufferCache_t *bufferCache, block_num_t dev, block_num_t inodeNum, void (*action)(bufferCache_t *, bufferHeader_t *, int), int flags, int notneg)
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

void alfs_buffertab_affectDev (bufferCache_t *bufferCache, block_num_t dev, void (*action)(bufferCache_t *, bufferHeader_t *, int), int flags)
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

void alfs_buffertab_affectAll (bufferCache_t *bufferCache, void (*action)(bufferCache_t *, bufferHeader_t *, int), int flags)
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

void alfs_buffertab_markFree (bufferCache_t *bufferCache, bufferHeader_t *entry) {
    bufCache_t *bufCache = (bufCache_t *) bufferCache;
    assert (bufferCache);
    addToFreeList (entry, &bufCache->freeList);
}

void alfs_buffertab_markInvalid (bufferCache_t *bufferCache, bufferHeader_t *entry) {
    bufCache_t *bufCache = (bufCache_t *) bufferCache;
    assert (bufferCache);
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

void alfs_buffertab_printContents (bufferCache_t *bufferCache, void (*print)(bufferHeader_t *))
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

