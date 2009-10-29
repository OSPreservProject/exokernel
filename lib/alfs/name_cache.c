
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


/* Simple <id,name>-value caching, indexed by (unique) <id,name> */

#include "name_cache.h"
#include "alfs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#undef NULL
#define NULL	(void *)0


static void name_cache_addtohash (nc_t *, nc_entry_t *);
static void name_cache_removefromhash (nc_t *, nc_entry_t *);
static void name_cache_addtolru (nc_entry_t **, nc_entry_t *);
static void name_cache_removefromlru (nc_entry_t **, nc_entry_t *);
static nc_entry_t * name_cache_getFreeEntry (nc_t *, int, char *, int);

#define name_cache_addtofreelist(namecache, entry) \
		if ((entry) >= &(namecache)->hash[(namecache)->hashbuckets]) { \
		   assert ((entry)->lru_next == NULL); \
		   (entry)->lru_next = (namecache)->free_head; \
		   (namecache)->free_head = (entry); \
		}

#define name_cache_hash(namecache,id,name,namelen)	\
		((unsigned int) ((id) + (namelen) + (int)(name)[0] \
                  + (int)(name)[(namelen)-1]) \
                 & (unsigned int) ((namecache)->hashbuckets - 1))

#define name_cache_match(entry,id,name,namelen) \
		(((entry)->id == (id)) && \
                 ((entry)->namelen == (namelen)) && \
                 ((entry)->name[0] == (name)[0]) &&     /* hopefully quickens */ \
                 (bcmp((entry)->name, (name), (namelen)) == 0))
                 

        /* NOTE: the specified cache size (below) includes meta-info such as */
        /*       hash structures and value statistics                        */

nc_t * name_cache_init (int size, int buckets)
{
   nc_t *namecache = (nc_t *) malloc_and_alloc (size);
   int entries = name_cache_entries(size);
   int i;

printf("name_cache_init: size %d, buckets %d, entries %d\n", size, buckets, entries);
   assert (namecache);
   assert (entries >= buckets);

   assert ((buckets == 128) || (buckets == 64) || (buckets == 32) || (buckets == 16) || (buckets == 8) || (buckets == 4) || (buckets == 2) || (buckets == 1) || (buckets == 256) || (buckets == 512) || (buckets == 1024) || (buckets == 2048));

   namecache->size = size;
   namecache->hashbuckets = buckets;
   namecache->lru_head = NULL;
   namecache->free_head = NULL;
   name_cache_resetStats (namecache);

   for (i=0; i<buckets; i++) {
      namecache->hash[i].namelen = 0;		/* marks entry invalid */
      namecache->hash[i].hash_next = NULL;
      namecache->hash[i].lru_next = NULL;
   }

   for (i=buckets; i<entries; i++) {
      namecache->hash[i].namelen = 0;		/* marks entry invalid */
      namecache->hash[i].hash_next = NULL;
      namecache->hash[i].lru_next = namecache->free_head;
      namecache->free_head = &namecache->hash[i];
   }

   return (namecache);
}


void name_cache_shutdown (nc_t *namecache)
{
   assert (namecache);
   free (namecache);
}


void name_cache_addEntry (nc_t *namecache, int id, char *name, int namelen, int value)
{
   nc_entry_t *entry;
   nc_entry_t *reuse = NULL;

   if (namelen > NAME_CACHE_MAX_NAMELEN) {
      namecache->stats.toolongadds++;
      return;
   }
   assert (namecache);

   entry = &namecache->hash[(name_cache_hash(namecache, id, name, namelen))];
   while (entry) {
      if (((unsigned int) entry->namelen | (unsigned int) reuse) == 0) {
         reuse = entry;		/* first reusable entry encountered on chain */
      }
      if (name_cache_match(entry, id, name, namelen)) {
		/* if same <id,name> index but different value, then replace */
		/* the previous entry.  This does so without the branch.     */
         entry->value = value;
         namecache->stats.addconflicts++;
         return;
      }
      entry = entry->hash_next;
   }

   if (reuse == NULL) {
      reuse = name_cache_getFreeEntry (namecache, id, name, namelen);
   }

   namecache->stats.adds++;
   reuse->value = value;
   reuse->id = id;
   reuse->namelen = namelen;
   bcopy (name, reuse->name, namelen);
   if (reuse->hash_next == NULL) {
      name_cache_addtohash (namecache, reuse);
   }
   name_cache_removefromlru (&namecache->lru_head, reuse);
   name_cache_addtolru (&namecache->lru_head, reuse);
}


int name_cache_findEntry (nc_t *namecache, int id, char *name, int namelen, int *valueP)
{
   nc_entry_t *entry;
   int checks = 0;

   if (namelen > NAME_CACHE_MAX_NAMELEN) {
      namecache->stats.toolongfinds++;
      return (0);
   }
   assert (namecache);

   entry = &namecache->hash[(name_cache_hash(namecache, id, name, namelen))];
   while (entry) {
      checks++;
      if (name_cache_match(entry, id, name, namelen)) {
         *valueP = entry->value;
         namecache->stats.hits++;
         namecache->stats.hitchecks += checks;
         name_cache_removefromlru (&namecache->lru_head, entry);
         name_cache_addtolru (&namecache->lru_head, entry);
         return (1);
      }
      entry = entry->hash_next;
   }

   namecache->stats.misses++;
   namecache->stats.misschecks += checks;
   return (0);
}


void name_cache_removeEntry (nc_t *namecache, int id, char *name, int namelen)
{
   nc_entry_t *entry;

   if (namelen > NAME_CACHE_MAX_NAMELEN) {
      return;
   }
   assert (namecache);

   entry = &namecache->hash[(name_cache_hash(namecache, id, name, namelen))];
   while (entry) {
      if (name_cache_match(entry, id, name, namelen)) {
         name_cache_removefromhash(namecache, entry);
         name_cache_removefromlru(&namecache->lru_head, entry);
         name_cache_addtofreelist(namecache, entry);
         entry->namelen = 0;
      }
      entry = entry->hash_next;
   }

   namecache->stats.indremoves++;
}


void name_cache_removeID (nc_t *namecache, int id)
{
   int entries;
   nc_entry_t *entry;
   int i;

   assert (namecache);

   entries = name_cache_entries(namecache->size);

   for (i=0; i<entries; i++) {
      entry = &namecache->hash[i];
      if ((entry->id == id) && (entry->namelen != 0)) {
         name_cache_removefromhash(namecache, entry);
         name_cache_removefromlru(&namecache->lru_head, entry);
         name_cache_addtofreelist(namecache, entry);
         entry->namelen = 0;
      }
   }

   namecache->stats.removes++;
}


void name_cache_removeValue (nc_t *namecache, int value)
{
   int entries;
   nc_entry_t *entry;
   int i;

   assert (namecache);

   entries = name_cache_entries(namecache->size);

   for (i=0; i<entries; i++) {
      entry = &namecache->hash[i];
      if ((entry->value == value) && (entry->namelen != 0)) {
         name_cache_removefromhash(namecache, entry);
         name_cache_removefromlru(&namecache->lru_head, entry);
         name_cache_addtofreelist(namecache, entry);
         entry->namelen = 0;
      }
   }

   namecache->stats.removes++;
}


void name_cache_resetStats (nc_t *namecache)
{
   assert (namecache);

   namecache->stats.hits = 0;
   namecache->stats.hitchecks = 0;
   namecache->stats.misses = 0;
   namecache->stats.misschecks = 0;
   namecache->stats.adds = 0;
   namecache->stats.addconflicts = 0;
   namecache->stats.indremoves = 0;
   namecache->stats.removes = 0;
   namecache->stats.toolongadds = 0;
   namecache->stats.toolongfinds = 0;
}


static nc_entry_t * name_cache_getFreeEntry (nc_t *namecache, int id, char *name, int namelen)
{
   nc_entry_t *entry;
   nc_entry_t *head = &namecache->hash[(name_cache_hash(namecache, id, name, namelen))];

   if (head->namelen == 0) {
      return (head);
   }

   if ((entry = namecache->free_head)) {
      namecache->free_head = entry->lru_next;
      entry->lru_next = NULL;
      return (entry);
   }

   assert (namecache->lru_head != NULL);
   if ((entry = namecache->lru_head)) {
      do {
         if ((entry >= &namecache->hash[namecache->hashbuckets]) || (entry == head)) {
            name_cache_removefromhash (namecache, entry);
            name_cache_removefromlru (&namecache->lru_head, entry);
            entry->namelen = 0;
            return (entry);
         }
         entry = entry->lru_next;
      } while (entry != namecache->lru_head);
   }

   assert(0);
   return (NULL);
}


static void name_cache_addtohash (nc_t *namecache, nc_entry_t *entry)
{
   nc_entry_t *head;
   if (entry >= &namecache->hash[namecache->hashbuckets]) {
      assert (entry->hash_next == NULL);
      head = &namecache->hash [(name_cache_hash (namecache, entry->id, entry->name, entry->namelen))];
      entry->hash_next = head->hash_next;
      entry->hash_prev = head;
      head->hash_next = entry;
      if (entry->hash_next) {
         entry->hash_next->hash_prev = entry;
      }
   }
}


static void name_cache_removefromhash (nc_t *namecache, nc_entry_t *entry)
{
   if (entry >= &namecache->hash[namecache->hashbuckets]) {
		/* (entry->hash_prev != NULL) guaranteed by static heads */
      entry->hash_prev->hash_next = entry->hash_next;
      if (entry->hash_next) {
         entry->hash_next->hash_prev = entry->hash_prev;
      }
      entry->hash_next = NULL;
   }
}


static void name_cache_addtolru (nc_entry_t **headptr, nc_entry_t *entry)
{
   assert (entry->lru_next == NULL);

   if (*headptr != NULL) {
      entry->lru_next = *headptr;
      entry->lru_prev = entry->lru_next->lru_prev;
      entry->lru_next->lru_prev = entry;
      entry->lru_prev->lru_next = entry;
   } else {
      entry->lru_next = entry;
      entry->lru_prev = entry;
      *headptr = entry;
   }
}


static void name_cache_removefromlru (nc_entry_t **headptr, nc_entry_t *entry)
{
   if (entry->lru_next != NULL) {
      if (entry->lru_next != entry) {
         entry->lru_prev->lru_next = entry->lru_next;
         entry->lru_next->lru_prev = entry->lru_prev;
         if (*headptr == entry) {
            *headptr = entry->lru_next;
         }
      } else {
         *headptr = NULL;
      }
      entry->lru_next = NULL;
   }
}


static void name_cache_print_name (char *name, int namelen)
{
   int i;
   for (i=0; i<namelen; i++) {
      printf("%c", name[i]);
   }
}


void name_cache_printContents (nc_t *namecache)
{
   int entries;
   int i;
   nc_entry_t *entry;

   assert (namecache);

   entries = name_cache_entries (namecache->size);
   printf("Contents of name cache (%x %d)\n", (u_int) namecache, entries);
   for (i=0; i<entries; i++) {
      entry = &namecache->hash[i];
      if (entry->namelen) {
         printf("%d\t", entry->id);
         name_cache_print_name (entry->name, entry->namelen);
         printf("\t(%d)\t --> %d\n", entry->namelen, entry->value);
      }
   }
}


void name_cache_printStats (nc_t *namecache)
{
   assert (namecache);

   printf("Stats for name cache (%x)\n", (u_int) namecache);
   printf("Number of hits: \t%d\t(%d entry comparisons)\n", namecache->stats.hits, namecache->stats.hitchecks);
   printf("Number of misses:\t%d\t(%d entry comparisons)\n", namecache->stats.misses, namecache->stats.misschecks);
   printf("Number of additions:\t%d\t(%d resident)\n", (namecache->stats.adds + namecache->stats.addconflicts), namecache->stats.addconflicts);
   printf("Number of removals requested:\t%d\t(%d specific)\n", (namecache->stats.indremoves + namecache->stats.removes), namecache->stats.indremoves);
   printf("Number of oversized interactions:\t%d\t(%d finds)\n", (namecache->stats.toolongadds + namecache->stats.toolongfinds), namecache->stats.toolongfinds);
}

