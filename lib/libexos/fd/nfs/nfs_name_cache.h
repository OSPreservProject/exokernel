
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


/* Simple <id,name>-Value Caching, indexed by (unique) <id,name>. */

#ifndef __NFS_NAME_CACHE_H__
#define __NFS_NAME_CACHE_H__

	/* NOTE: this value is set low (and without trying to make the   */
	/*       entry structure size a power of 2) so as to make small  */
	/*       name lookups (assumed the common case) better -- more   */
	/*       entries can be squeezed into the same space.  Variable  */
        /*       sizing of entries is avoided for simplicity (apropos?)  */

#define NFS_NAME_CACHE_MAX_NAMELEN	24	/* maximum length of cached name */

/* if you need to check integrity of cache. Be sure to recompile evertything.*/
// #define NFS_CACHE_VALUE_FENCE	

typedef struct nn_value {
  int id;
  void *ptr;
  int negative;
#ifdef NFS_CACHE_VALUE_FENCE
#define VALUE_FENCE 1234567890
  int fence;
#endif
  long ts;			/* timestamp, for now in val */
} nn_value_t, *nn_value_p;

#define compare_value_t(v,u) ((v).id == (u).id && (v).ptr == (u).ptr)
#define compare_value_p(v,u) ((v)->id == (u)->id && (v)->ptr == (u)->ptr)

#define MAKEID(dev,ino) ((dev * -1) << 24 | ino)
#define ID2INO(id) ((id) & 0x00ffffff)
#define ID2DEV(id) ((((id) & 0xff000000) >> 24) * -1)

#define print_value(v) printf("<%d,%d / %p <%d,%d>>",ID2DEV(v.id), ID2INO(v.id), v.ptr,\
			      v.ptr ? GETNFSCEDEV((nfsc_p)v.ptr) : -1,\
			      v.ptr ? GETNFSCEINO((nfsc_p)v.ptr) : -1)

#define print_id(i) printf("<%d,%d>",ID2DEV(i),ID2INO(i))



struct nc_entry {
   struct nc_entry *hash_next;		/* hash chain pointers */
   struct nc_entry *hash_prev;
   struct nc_entry *lru_next;		/* S-LRU list pointers */
   struct nc_entry *lru_prev;
  nn_value_t value;		/* value associated with <id,name> */
  int id;				/* id associated with cache entry */
  char namelen;			/* length of name */
  char name[NFS_NAME_CACHE_MAX_NAMELEN];	/* name associated with cache entry */
};
typedef struct nc_entry nc_entry_t;

struct nc_stats {
   int hits;			/* number of hits */
   int hitchecks;		/* number of entries checked for all hits */
   int misses;			/* number of misses */
   int misschecks;		/* number of entries checked for all misses */
   int adds;			/* number of names added -- same as misses? */
   int addconflicts;		/* number of add that failed do to conflict */
   int indremoves;		/* number of removes index by <id,name> */
   int removes;			/* number of removes requiing full scan */
   int toolongadds;		/* attempts to add oversized names */
   int toolongfinds;		/* attempts to find oversized names */
};
typedef struct nc_stats nc_stats_t;

struct nc {
   unsigned int size;			/* size of the name cache */
   unsigned int hashbuckets;		/* mask for hash bucket selection */
   nc_entry_t *lru_head;		/* head of the LRU list */
   nc_entry_t *free_head;		/* head of the free list */
   nc_stats_t stats;			/* usage/perf stats for name cache */
   nc_entry_t hash[1];			/* the hash buckets */
   /* nc_entry_t nc_overflow[] */	/* the remaining name cache capacity */
};
typedef struct nc nc_t;

#define nfs_name_cache_entries(size)	(((int)(size) \
					    - (2 * (int)sizeof(unsigned int)) \
					    - (2 * (int)sizeof(nc_entry_t *)) \
					    - (int)sizeof(nc_stats_t) \
					) / (int)sizeof(nc_entry_t))

/* name_cache.c prototypes */

static int nfs_name_cache_init (nc_t *nc, int size, int buckets);
static void nfs_name_cache_addEntry (nc_t *namecache, int id, char *name, int namelen, 
			  nn_value_t value);
static int nfs_name_cache_findEntry (nc_t *namecache, int id, char *name, int namelen, 
			  nn_value_t *valueP);
static void nfs_name_cache_removeEntry (nc_t *namecache, int id, char *name, int namelen);
static void nfs_name_cache_removeID (nc_t *namecache, int id);
static void nfs_name_cache_removeValue (nc_t *namecache, nn_value_t value);
static void nfs_name_cache_resetStats (nc_t *namecache);
static void nfs_name_cache_shutdown (nc_t *namecache);
static void nfs_name_cache_printContents (nc_t *namecache);
static void nfs_name_cache_printStats (nc_t *namecache);

typedef void (*nc_rm_t)(int ,char *,nn_value_t);

static void nfs_name_cache_removefunc(nc_rm_t func);

#define nfs_name_cache_getStats(namecache)	&(namecache)->stats

#endif  /* __NFS_NAME_CACHE_H__ */

