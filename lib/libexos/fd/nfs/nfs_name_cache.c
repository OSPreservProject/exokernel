
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
#include <time.h>

#include "nfs_struct.h"
#include "nfs_name_cache.h"
#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <memory.h>

#include <exos/critical.h>
#include <exos/mallocs.h>

#include <fd/proc.h>
#include <exos/vm-layout.h>


#undef NULL
#define NULL	(void *)0

//#define DISABLE_NEGATIVE_RESPONSE

static void nfs_name_cache_addtohash (nc_t *, nc_entry_t *);
static void nfs_name_cache_removefromhash (nc_t *, nc_entry_t *);
static void nfs_name_cache_addtolru (nc_entry_t **, nc_entry_t *);
static void nfs_name_cache_removefromlru (nc_entry_t **, nc_entry_t *);
static nc_entry_t * nfs_name_cache_getFreeEntry (nc_t *, int, char *, int);
static void nfs_name_cache_print_name (char *name, int namelen);

#define nfs_name_cache_addtofreelist(namecache, entry) \
		if ((entry) >= &(namecache)->hash[(namecache)->hashbuckets]) { \
		   assert ((entry)->lru_next == NULL); \
		   (entry)->lru_next = (namecache)->free_head; \
		   (namecache)->free_head = (entry); \
		}

#define nfs_name_cache_hash(namecache,id,name,namelen)	\
		((unsigned int) ((id) + (namelen) + (int)(name)[0] \
                  + (int)(name)[(namelen)-1]) \
                 & (unsigned int) ((namecache)->hashbuckets - 1))

#define nfs_name_cache_match(entry,id,name,namelen) \
		(((entry)->id == (id)) && \
                 ((entry)->namelen == (namelen)) && \
                 ((entry)->name[0] == (name)[0]) &&     /* hopefully quickens */ \
                 (bcmp((entry)->name, (name), (namelen)) == 0))
                 

        /* NOTE: the specified cache size (below) includes meta-info such as */
        /*       hash structures and value statistics                        */
     /* namecache is a pointer to a region of size "size"  */
static int nfs_name_cache_init (nc_t *namecache, int size, int buckets)
{
   int entries;
   int i;

   entries = nfs_name_cache_entries(size);

//   printf("nfs_name_cache_init: size %d, buckets %d, entries %d\n",  size, buckets, entries);

   assert (namecache);
   assert (entries >= buckets);

   assert ((buckets == 128) || (buckets == 64) || (buckets == 32) || (buckets == 16) || (buckets == 8) || (buckets == 4) || (buckets == 2) || (buckets == 1) || (buckets == 256) || (buckets == 512) || (buckets == 1024) || (buckets == 2048));

   namecache->size = size;
   namecache->hashbuckets = buckets;
   namecache->lru_head = NULL;
   namecache->free_head = NULL;
   nfs_name_cache_resetStats (namecache);

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

   return (entries);
}


static inline void nfs_name_cache_shutdown (nc_t *namecache)
{
   assert (namecache);
   __free (namecache);
}


static void nfs_name_cache_addEntry (nc_t *namecache, 
			  int id, char *name, int namelen, nn_value_t value)
{
   nc_entry_t *entry;
   nc_entry_t *reuse = NULL;

#if 0
   printf("nfs_name_cache_addEntry: ");
   print_id(id);
   printf("\t");
   nfs_name_cache_print_name (name, namelen);
   printf(" --> ");
   print_value(value);
   printf("\n");
#endif
   //   if (name[0] == '.') {ExitCritical();return;}

   if (namelen > NFS_NAME_CACHE_MAX_NAMELEN) {
      namecache->stats.toolongadds++;
      return;
   }
   assert (namecache);

   EnterCritical();
   entry = &namecache->hash[(nfs_name_cache_hash(namecache, id, name, namelen))];
   while (entry) {
      if (((unsigned int) entry->namelen | (unsigned int) reuse) == 0) {
         reuse = entry;		/* first reusable entry encountered on chain */
      }
      if (nfs_name_cache_match(entry, id, name, namelen)) {
		/* if same <id,name> index but different value, then replace */
		/* the previous entry.  This does so without the branch.     */
         entry->value = value;
         namecache->stats.addconflicts++;
	 ExitCritical();
	 return;
      }
      entry = entry->hash_next;
   }

   if (reuse == NULL) {
      reuse = nfs_name_cache_getFreeEntry (namecache, id, name, namelen);
   }

   namecache->stats.adds++;
   reuse->value = value;
   reuse->id = id;
   reuse->namelen = namelen;
   bcopy (name, reuse->name, namelen);
   if (reuse->hash_next == NULL) {
      nfs_name_cache_addtohash (namecache, reuse);
   }
   nfs_name_cache_removefromlru (&namecache->lru_head, reuse);
   nfs_name_cache_addtolru (&namecache->lru_head, reuse);
   ExitCritical();
}


static int nfs_name_cache_findEntry (nc_t *namecache, 
			  int id, char *name, int namelen, nn_value_t *valueP)
{
   nc_entry_t *entry;
   int checks = 0;

   if (namelen > NFS_NAME_CACHE_MAX_NAMELEN) {
      namecache->stats.toolongfinds++;
      return (0);
   }
   assert (namecache);

   entry = &namecache->hash[(nfs_name_cache_hash(namecache, id, name, namelen))];
   while (entry) {
      checks++;
      if (nfs_name_cache_match(entry, id, name, namelen)) {
         *valueP = entry->value;
         namecache->stats.hits++;
         namecache->stats.hitchecks += checks;
         nfs_name_cache_removefromlru (&namecache->lru_head, entry);
         nfs_name_cache_addtolru (&namecache->lru_head, entry);
         return (1);
      }
      entry = entry->hash_next;
   }

   namecache->stats.misses++;
   namecache->stats.misschecks += checks;
   return (0);
}


static void nfs_name_cache_removeEntry (nc_t *namecache, int id, char *name, int namelen)
{
   nc_entry_t *entry;

   if (namelen > NFS_NAME_CACHE_MAX_NAMELEN) {
      return;
   }
   assert (namecache);

   entry = &namecache->hash[(nfs_name_cache_hash(namecache, id, name, namelen))];
   while (entry) {
      if (nfs_name_cache_match(entry, id, name, namelen)) {
         nfs_name_cache_removefromhash(namecache, entry);
         nfs_name_cache_removefromlru(&namecache->lru_head, entry);
         nfs_name_cache_addtofreelist(namecache, entry);
         entry->namelen = 0;
      }
      entry = entry->hash_next;
   }

   namecache->stats.indremoves++;
}


void nfs_name_cache_removeID (nc_t *namecache, int id)
{
   int entries;
   nc_entry_t *entry;
   int i;

   assert (namecache);

   entries = nfs_name_cache_entries(namecache->size);

   for (i=0; i<entries; i++) {
      entry = &namecache->hash[i];
      if ((entry->id == id) && (entry->namelen != 0)) {
         nfs_name_cache_removefromhash(namecache, entry);
         nfs_name_cache_removefromlru(&namecache->lru_head, entry);
         nfs_name_cache_addtofreelist(namecache, entry);
         entry->namelen = 0;
      }
   }

   namecache->stats.removes++;
}


static void nfs_name_cache_removeValue (nc_t *namecache, nn_value_t value)
{
   int entries;
   nc_entry_t *entry;
   int i;

   assert (namecache);

   entries = nfs_name_cache_entries(namecache->size);

   for (i=0; i<entries; i++) {
      entry = &namecache->hash[i];
      if (compare_value_t(entry->value,value) && (entry->namelen != 0)) {
         nfs_name_cache_removefromhash(namecache, entry);
         nfs_name_cache_removefromlru(&namecache->lru_head, entry);
         nfs_name_cache_addtofreelist(namecache, entry);
         entry->namelen = 0;
      }
   }

   namecache->stats.removes++;
}


static void nfs_name_cache_resetStats (nc_t *namecache)
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


static nc_entry_t * nfs_name_cache_getFreeEntry (nc_t *namecache, int id, char *name, int namelen)
{
   nc_entry_t *entry;
   nc_entry_t *head = &namecache->hash[(nfs_name_cache_hash(namecache, id, name, namelen))];

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
            nfs_name_cache_removefromhash (namecache, entry);
            nfs_name_cache_removefromlru (&namecache->lru_head, entry);
            entry->namelen = 0;
            return (entry);
         }
         entry = entry->lru_next;
      } while (entry != namecache->lru_head);
   }

   assert(0);
   return (NULL);
}


static void nfs_name_cache_addtohash (nc_t *namecache, nc_entry_t *entry)
{
   nc_entry_t *head;
   if (entry >= &namecache->hash[namecache->hashbuckets]) {
      assert (entry->hash_next == NULL);
      head = &namecache->hash [(nfs_name_cache_hash (namecache, entry->id, entry->name, entry->namelen))];
      entry->hash_next = head->hash_next;
      entry->hash_prev = head;
      head->hash_next = entry;
      if (entry->hash_next) {
         entry->hash_next->hash_prev = entry;
      }
   }
}

static nc_rm_t rm_func = (nc_rm_t)NULL;

static void nfs_name_cache_removefunc(nc_rm_t f) {
  rm_func = f;
}

static void nfs_name_cache_removefromhash (nc_t *namecache, nc_entry_t *entry)
{
  if (rm_func) {
    rm_func(entry->id, entry->name, entry->value);
  }
  if (entry >= &namecache->hash[namecache->hashbuckets]) {
    /* (entry->hash_prev != NULL) guaranteed by static heads */
    entry->hash_prev->hash_next = entry->hash_next;
    if (entry->hash_next) {
      entry->hash_next->hash_prev = entry->hash_prev;
    }
    entry->hash_next = NULL;
  }
}


static void nfs_name_cache_addtolru (nc_entry_t **headptr, nc_entry_t *entry)
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


static void nfs_name_cache_removefromlru (nc_entry_t **headptr, nc_entry_t *entry)
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


static void nfs_name_cache_print_name (char *name, int namelen)
{
   int i;
   for (i=0; i<namelen; i++) {
      printf("%c", name[i]);
   }
}

static inline void nfs_name_cache_printEntry(nc_entry_t *entry) {
  print_id(entry->id);
  printf("\t");
  nfs_name_cache_print_name (entry->name, entry->namelen);
  printf("\t(%d)\t --> ", entry->namelen);
  print_value(entry->value);
  printf("\n");
}

static void nfs_name_cache_printContents (nc_t *namecache)
{
   int entries;
   int i;
   nc_entry_t *entry;

   assert (namecache);

   entries = nfs_name_cache_entries (namecache->size);
   printf("Contents of name cache (%x %d)\n", (u_int) namecache, entries);
   for (i=0; i<entries; i++) {
     entry = &namecache->hash[i];
     if (entry->namelen) nfs_name_cache_printEntry(entry);
   }
}


static void nfs_name_cache_printStats (nc_t *namecache)
{
   assert (namecache);

   printf("Stats for name cache (%x)\n", (u_int) namecache);
   printf("Number of hits: \t%d\t(%d entry comparisons)\n", namecache->stats.hits, namecache->stats.hitchecks);
   printf("Number of misses:\t%d\t(%d entry comparisons)\n", namecache->stats.misses, namecache->stats.misschecks);
   printf("Number of additions:\t%d\t(%d resident)\n", (namecache->stats.adds + namecache->stats.addconflicts), namecache->stats.addconflicts);
   printf("Number of removals requested:\t%d\t(%d specific)\n", (namecache->stats.indremoves + namecache->stats.removes), namecache->stats.indremoves);
   printf("Number of oversized interactions:\t%d\t(%d finds)\n", (namecache->stats.toolongadds + namecache->stats.toolongfinds), namecache->stats.toolongfinds);
}






/* ------------------------------------------------------------------------ */

#define MAKEID(dev,ino) ((dev * -1) << 24 | ino)
#define ID2INO(id) ((id) & 0x00ffffff)
#define ID2DEV(id) ((((id) & 0xff000000) >> 24) * -1)
static nc_t *nnc;

#define lprintf if (1) kprintf
#define rprintf if (1) printf
#define kprintf if (0) printf
#define printf if (0) printf
#define fprintf if (1) fprintf


/* returns 1 if positive cache hit, 0 on a miss, and -1 on a negative cache hit. */
int
nfs_cache_lookup(int dev,int ino,const char *name, nfsc_p *p) {
  int ret = 0;
  int id;
  long t;
  nn_value_t val;
  nfsc_p e;

  START(nfs_lookup,cache_lookup);
  t = time(NULL);
  printf("nfs_cache_lookup: <%d,%d> %s --> ?",dev, ino,name);

  id = MAKEID(dev,ino);
  assert(dev < 0);		/* nfs device */
  ret = nfs_name_cache_findEntry(nnc,id,(char *)name, strlen(name),&val);

  if (ret) {
    /* HBXX - need to also check timestamp of negative */
#ifdef NFS_CACHE_VALUE_FENCE
    if (val.fence != VALUE_FENCE) {
      fprintf(stderr,"\n\n\nBROKEN FENCE: <%d,%d> %s returned \n"
	      "id %d ptr: %p n; %d fence: %d\n",
	      dev,ino,name, val.id, val.ptr, val.negative, val.fence);
      assert(0);
    }
#endif
    if (val.negative) {
#ifdef DISABLE_NEGATIVE_RESPONSE
      STOP(nfs_lookup,cache_lookup);
      return 0;			/* say miss */
#else
      if ((val.ts + NFSNCNEGATIVEVALID) < t) {
	nfs_name_cache_removeEntry (nnc, id,(char *)name, strlen(name));
	STOP(nfs_lookup,cache_lookup);
	return 0;			/* say miss */
      } else {
	STOP(nfs_lookup,cache_lookup);
	return -1;		/* negative cache hit */
      }
#endif
    }

    /* we have to validate the entry from the cache, since we dont increment ref counts
     * for entries in the cache.*/
    e = (nfsc_p)val.ptr;
    id = val.id;
    ino = ID2INO(id);
    dev = ID2DEV(id);

    printf("hit <%d,%d>\n",dev,ino);
    /* make sure the device and inode are still the same. */
    EnterCritical();
    if (GETNFSCEDEV(e) == dev && GETNFSCEINO(e) == ino) {
      printf("entry %p has _not_ changed: %d,%d\n",e,dev,ino);
      /* take the entry */
      if (!nfsc_is_inuse(e)) nfsc_set_inuse(e);
      nfsc_inc_refcnt(e);
      ExitCritical();
      *p = e;
      STOP(nfs_lookup,cache_lookup);
      return 1;			/* positive cache hit */
    } else {
      ExitCritical();
      printf("entry %p has CHANGED: pointed to %d,%d points to: %d,%d\n",e,dev,ino,
	     GETNFSCEDEV(e),GETNFSCEINO(e));
      /* delete entry */
      nfs_name_cache_removeEntry (nnc, id,(char *)name, strlen(name));
    }

    print_value(val);
  } else {
    printf("miss\n");
  }
  
  STOP(nfs_lookup,cache_lookup);
  return 0;			/* cache miss */


}

/* called by nfs_unlink, nfs_rmdir, nfs_stat */
void 
nfs_cache_remove(struct file *filp, const char *name) {
  nfsc_p e;
  int id;
  e = GETNFSCE(filp);
  id = MAKEID(GETNFSCEDEV(e),GETNFSCEINO(e));

  printf("nfs_cache_remove: <%d,%d> %s\n",
	 ID2DEV(id),ID2DEV(id),name ? name : "RMALL");
  if (name) {
    nfs_name_cache_removeEntry (nnc, id,(char *)name, strlen(name));
  } else {
    nfs_name_cache_removeID (nnc, id);
  }
}

void 
nfs_cache_remove_target(struct file *filp) {
  nfsc_p e;
  nn_value_t val;
  e = GETNFSCE(filp);
  val.id = MAKEID(GETNFSCEDEV(e),GETNFSCEINO(e));
  val.ptr = NULL;
  val.negative = 0;

  printf("nfs_cache_remove_target: <%d,%d>\n",
	 ID2DEV(val.id),ID2DEV(val.id));
  nfs_name_cache_removeValue (nnc, val);
}


void inline
nfs_cache_add_devino(int dev, int ino, char *name, nfsc_p e) {
  int id;
  nn_value_t val;

  id = MAKEID(dev,ino);
#ifdef NFS_CACHE_VALUE_FENCE
  val.fence = VALUE_FENCE;
#endif
  if (e) {
    val.ptr = e; val.id = MAKEID(GETNFSCEDEV(e),GETNFSCEINO(e)); val.negative = 0;
  } else {
    val.ptr = (nfsc_p)0; val.id = 0; val.negative = 1;
  }
  val.ts = time(NULL);
  nfs_name_cache_addEntry(nnc,id, (char *)name, strlen(name),val);

#if 0
  status = nfs_name_cache_findEntry(nnc, id, (char *)name, strlen(name), &val2);
  val2.id = val2.negative = 0; val2.ptr = NULL;
  if (val.id != val2.id || val.ptr != val2.ptr || val.negative != val2.negative)
    fprintf(stderr,"\n\nstatus: %d vals do not match: \nval.id: %d val2.id: %d\n" 
	    "ptr %p ptr2 %p\nneg %d neg2 %d\n",
	    status,val.id, val2.id, val.ptr, val2.ptr, val.negative, val2.negative)
#ifdef NFS_CACHE_VALUE_FENCE
      fprintf(stderr,"fence: %d fence2:%d\n ts: %ld ts2: %ld",
	      val.fence, val2.fence);
#endif
#endif
}

void 
nfs_cache_add(nfsc_p d, char *name, nfsc_p e) {
  nfs_cache_add_devino(GETNFSCEDEV(d),GETNFSCEINO(d),name,e);
}











/* ------------------------------------------------------------------------ */

static void 
nfs_lookup_cache_remove_func(int id, char *name, nn_value_t val) {
  int ino, dev;
  int ino2, dev2;
  nfsc_p e;
  int namelen;

  return;
  START(nfs_lookup,remove);
  ino = ID2INO(id);
  dev = ID2DEV(id);

  e = (nfsc_p)val.ptr;
  id = val.id;
  ino2 = ID2INO(id);
  dev2 = ID2DEV(id);

  printf("REMOVING ENTRY: ");
  print_id(id);
  printf("\t");
  namelen = strlen(name);
  nfs_name_cache_print_name (name, namelen);
  printf("\t(%d)\t --> ", namelen);
  print_value(val);
  printf("\n");
  STOP(nfs_lookup,remove);
}

int 
nfs_cache_init(void) {
  int entries,status;
  extern int ok_envprog(char *teststr);
  START(nfs_lookup,init);
  
  status = fd_shm_alloc(NFS_LOOKUP_SHM_OFFSET,
			NFS_LOOKUP_CACHE_SZ,
			(char *)NFS_LOOKUP_SHARED_REGION);
  if (status == -1) {
    return -1;
  }
  nnc = (nc_t *)NFS_LOOKUP_SHARED_REGION;

  if (status) {			/* first time */
    entries = nfs_name_cache_init(nnc, NFS_LOOKUP_CACHE_SZ, 64);
    kprintf("Initializing name cache: %d entries\n",entries);
  } else {
    //kprintf("Name cache already mapped\n");
  }
  nfs_name_cache_removefunc(nfs_lookup_cache_remove_func);
  STOP(nfs_lookup,init);
  return 0;
}





/* for hsh.c */
int b_nfsncache(int argc, char **argv) {
  int entries;

  if (argc < 2) {
  usage:
    fprintf(stderr,"usage: %s [flush|stats|dump]\n",argv[0]);
    return 0;
  }
  if (!strcmp("flush",argv[1])) {
    entries = nfs_name_cache_init(nnc, NFS_LOOKUP_CACHE_SZ, 64);
    //fprintf(stderr,"Initializing name cache: %d entries\n",entries);
  } else   if (!strcmp("stats",argv[1])) {
    nfs_name_cache_printStats(nnc);
  } else   if (!strcmp("dump",argv[1])) {
    nfs_name_cache_printContents(nnc); 
  } else   if (!strcmp("query",argv[1])) {
    if (!argv[2]) goto usage;
  } else goto usage; 
  return 1;
}
