
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <exos/mallocs.h>
#include <exos/mregion.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <xok/mmu.h>

/* Each pointer can point to a structure: */

struct mregion {
  struct mregion *parent;
  void *pstart; /* first byte in the region */
  void *pend; /* last byte in the region */
  struct mregion **map; /* an array of pointers */
};

static struct mregion *mr_top = NULL;
static int mregion_inited = 0;

static int PtrsPerPtrMap=0; /* The number of pointers in a map of pointers */
static int BytesPerPtrMap=0; /* The number of bytes in a map of pointers */
static int mr_pagesize=0;

/*
  The Lower two bits of each pointer in a pointer map are as follows:
  0=The pointer points to a struct mregion at the next level
    [or if all bits are zero, it means the region is free]
  1=The region is completely taken and this is the start of it
    [the upper bits in this case point to some user defined structure
    where the first pointer points to a function which is called with
    a pointer to the user defined structure and the "address in
    question" as well as other fault related info whenever a fault in
    that region occurs]
  2=The region is completely taken, but is continued from preceding region
    [the upper bits are the same as for the 1 case]
  3=The region is off limits [kernel area]

  Thus, each struct must start on a 32-bit boundary.

  Bit 0 of the map pointer is as follows:
   0=Array of pointers
   1=BitMap-with a pointer at the front to a user defined struct (which
     starts with a pointer to a handler...)
  Bit 1 of the map pointer is as follows:
   0=Contig and Free area info in rest of struct is current
   1=Contig and Free area info in rest of struct is stale

  Thus, the bitmap or array must be on a 32-bit boundary.

  There must be a pointer at the beginning of the bitmap 

  Every two bits of a bitmap represent a machine word.  Thus, the minimum
  allocatable size is a machine word.  The meanings are as follows:
  0=free, 1=first, 2=follow, 3=off limits.
*/

typedef unsigned int mr_CAST;

#define mr_2BITS_MASK ((mr_CAST)3)
#define mr_2BITS_SUBPTR ((mr_CAST)0)
#define mr_2BITS_UPTR ((mr_CAST)1)

/* calculates and returns the size of a subregion of an mregion */
static inline mr_CAST subregionsize(struct mregion *mr) {
  if (mr == mr->parent) /* hack so it'll work at top level */
    return (((mr_CAST)(mr->pend - mr->pstart)) / PtrsPerPtrMap)+1;
  return (mr->pend - mr->pstart + 1) / PtrsPerPtrMap;
}

/* given an mregion and a pointer, which subregion contains the pointer.
   Doesn't error that p is within the bounds of the mregion */
static inline unsigned int getindex(struct mregion *mr, const void *p) {
  p = (void*)(p - mr->pstart);
  return ((mr_CAST)p) / subregionsize(mr);
}

/* simple min and max macros */
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

/* get an mregion pointer corresponding to subregion i of mr */
static inline struct mregion *getsubregion(struct mregion *mr,
					   unsigned int i) {
  return mr->map[i];
}

/* set subregion i of mregion mr to newmr */
static inline void setsubregion(struct mregion *mr, unsigned int i,
				struct mregion *newmr) {
  mr->map[i] = newmr;
}

/* sets a subregion to NULL */
static inline void clearsubregion(struct mregion *mr, unsigned int i) {
  setsubregion(mr, i, NULL);
}

/* sets the lower two bits on a pointer to zero */
static inline void *clear2bits(void *p) {
  return (void*)((mr_CAST)p & ~(mr_CAST)3);
}

/* returns the lower two bits ofa pointer */
static inline mr_CAST get2bits(void *p) {
  return ((mr_CAST)p & (mr_CAST)3);
}

/* sets the lower two bits of a pointer and returns the result */
static inline void *set2bits(void *p, mr_CAST bits) {
  return (void*)(((mr_CAST)p & ~(mr_CAST)3) | (bits & (mr_CAST)3));
}

/* clears the lower two bits of the supplied pointer and returns it.
   use this on the user pointer before dereferencing it */
static inline void *map2ptr(void *ptr) {
  return clear2bits(ptr);
}

/* free the memory for an mregion and for all its children and mark it
   clear in the parent */
static void free_mregion(struct mregion *mr) {
  unsigned int i;

  /* free any children */
  /* check each ptr */
  for (i=0; i < PtrsPerPtrMap; i++) {
    struct mregion *submr = getsubregion(mr, i);

    if (submr && get2bits(submr) == mr_2BITS_SUBPTR) free_mregion(submr);
  }

  /* mark it free in the parent */
  if (mr != mr->parent)
    clearsubregion(mr->parent, getindex(mr->parent, mr->pstart));
  else
    mr_top = NULL;

  /* free the memory */
  exos_pinned_free(mr->map);
  exos_pinned_free(mr);
}

/* is a sub/region full? test the lower two bits of the pointer */
static inline unsigned int isfull(struct mregion *mr) {
  return (((mr_CAST)mr) & ((mr_CAST)3));
}

/* extend a full or empty subregion to have children */
static int extenddown(struct mregion *mr, unsigned int i) {
  struct mregion *submr = getsubregion(mr, i), *newmr;

  /* if already has children, then there's nothing to do */
  if (submr && !isfull(submr)) return -1;

  /* we'll need memory for the new mregion struct */
  newmr = exos_pinned_malloc(sizeof(*mr));
  if (!newmr) return -1;
  newmr->map = exos_pinned_malloc(BytesPerPtrMap);
  if (!newmr->map) {
    exos_pinned_free(newmr);
    return -1;
  }

  /* init values */
  newmr->parent = mr;
  newmr->pstart = mr->pstart + subregionsize(mr)*i;
  newmr->pend = newmr->pstart + subregionsize(mr) - 1;
  /* if we're extending a full subregion */
  if (isfull(submr)) {
    unsigned int j;

    for (j=0; j < PtrsPerPtrMap; j++)
      setsubregion(newmr, j, submr);
  } else /* if extending an empty subregion */
    bzero(newmr->map, BytesPerPtrMap);

  /* record the new child in the parent */
  setsubregion(mr, i, newmr);
  return 0;
}

/* dealloc a specified range of memory */
static int mregion_free2(struct mregion *mr, const void *start,
			 const void *end) {
  if (!mr) mr = mr_top;

  /* some things just don't make sense */
  if (!mr || isfull(mr) || start > end) return -1;
  if (start == end) return 0;

  /* if we're too low down in the tree then start higher */
  if (start < mr->pstart || end > mr->pend)
    return mregion_free2(mr->parent, start, end);

  /* if the entire mregion must be freed... */
  if (mr->pstart == start && mr->pend == end)
    free_mregion(mr); /* then free everything */
  else {
    unsigned int i, j;
    const void *p, *p2;
    struct mregion *submr;

    /* restrict clearing to calculated indexes */
    i = getindex(mr, start);
    j = getindex(mr, end);
    p = start; /* p is a subregion free start value */
    /* for each relavent subregion */
    for (; i <= j; i++) {
      /* p2 is a subregion free end value */
      p2 = MIN(end, mr->pstart+(i+1)*subregionsize(mr)-1);
      submr = getsubregion(mr, i);
      /* if subregion is full... */
      if (isfull(submr))
	if (subregionsize(mr) == p2-p+1)
	  /* if removing entire 'full' subregion then just mark it clear
	     in the parent */
	  clearsubregion(mr, i);
	else { /* if freeing only part of a 'full' subregion... */
	  /* must extend the subregion in order to free only part of it */
	  if (extenddown(mr, i)) return -1;
	  submr = getsubregion(mr, i);
	  /* recurse */
	  if (mregion_free2(submr, p, p2)) return -1;
	}
      else if (submr) /* if the region has children... */
	if (mregion_free2(submr, p, p2)) return -1;
      /* else if already empty, then do nothing */

      /* new value for subregion start */
      p = mr->pstart + subregionsize(mr)*(i+1);
    }
  }

  return 0;
}

int mregion_free(const void *start, const void *end) {
  return mregion_free2(NULL, start, end);
} 

/* given a reference mregion and a requested location, return the
   lowest mregion containing that location and set the index into its
   map.
*/
static struct mregion *mregion_find(struct mregion *mr, void *ptr,
				    unsigned int *ip) {
  struct mregion *submr;

  if (!mr) return NULL;

  /* go up the tree if necessary */
  if (ptr < mr->pstart || ptr > mr->pend)
    return mregion_find(mr->parent, ptr, ip);

  /* continue downward if necessary */
  submr = getsubregion(mr, getindex(mr, ptr));
  if (submr == NULL || isfull(submr)) {
    *ip = getindex(mr, ptr);
    return mr;
  }

  return mregion_find(submr, ptr, ip);
}

struct mregion_ustruct *mregion_get_ustruct(void *addr) {
  struct mregion *mr;
  unsigned int i, k;

  mr = mregion_find(mr_top, addr, &i);
  if (!mr) return NULL;
  mr = getsubregion(mr, i);

  k = get2bits(mr);
  if (!mr || k != mr_2BITS_UPTR) return NULL;
  mr = clear2bits(mr);

  return (struct mregion_ustruct*)mr;
}

/* find size free bytes at start and use u as the user defined structure, and
   start looking from mr.
*/
static int mregion_alloc2(struct mregion *mr, void *start, size_t size,
			  struct mregion_ustruct *u) {
  unsigned int i, j, k;
  void *tstart;

  /* restriced use */
  if (size == 0 || size != PGROUNDUP(size) ||
      start != (void*)PGROUNDUP((unsigned int)start))
    return -1;

  /* if this is the first time... */
  if (!mregion_inited) {
    mregion_inited = 1;
    mr_pagesize = getpagesize();
    PtrsPerPtrMap = 32;
    BytesPerPtrMap = PtrsPerPtrMap*(sizeof(void*));
  }
  /* if no mr is specified then start from the top */
  if (!mr) {
    /* if there is no top then make it! */
    if (!mr_top) {
      mr_top = exos_pinned_malloc(sizeof(*mr_top));
      if (!mr_top) return -1;
      mr_top->map = exos_pinned_malloc(BytesPerPtrMap);
      if (!mr_top->map) {
	exos_pinned_free(mr_top);
	mr_top = NULL;
	return -1;
      }
      mr_top->parent = mr_top;
      mr_top->pstart = 0;
      mr_top->pend = (void*)-1;
      bzero(mr_top->map, BytesPerPtrMap);
    }
    mr = mr_top;
  }

  /* free anything that might be in the way */
  mregion_free2(mr, start, start+size-1);
 
  /* allocate */
  tstart = start;
  do {
    mr = mregion_find(mr, tstart, &i);
    if (!mr) {
      /* roll back allocation */
      mregion_free2(mr, start, tstart-1);
      return -1;
    }
    /* extend down as necessary */
    while (subregionsize(mr) > size ||
	   tstart != mr->pstart + subregionsize(mr)*i) {
      if (extenddown(mr, i)) {
	/* roll back allocation */
	mregion_free2(mr, start, tstart-1);
	return -1;
      }
      mr = mregion_find(mr, tstart, &i);
    }

    j = getindex(mr, MIN(mr->pend, tstart+size-1));
    if (tstart+size-1 < mr->pstart + subregionsize(mr)*(j+1) - 1) {
      /* given the above constraints, j should never be zero and thus
	 this shouldn't cause wraparound */
      j--;
    }
    /* record allocation */
    for (k=i; k <= j; k++)
      setsubregion(mr, k, set2bits(u, mr_2BITS_UPTR));

    /* increment/decrement counters */
    size -= MIN(subregionsize(mr)*(j-i+1), size);
    tstart += subregionsize(mr)*(j-i+1);
  } while (size > 0);

  return 0;
}

int mregion_alloc(void *start, size_t size, struct mregion_ustruct *u) {
  return mregion_alloc2(NULL, start, size, u);
}

static void *__exos_pinned_brkpt=NULL;
static void *__exos_pinned_brkpt_aligned=NULL;
static void *__exos_brkpt=NULL;
static void *__exos_brkpt_aligned=NULL;

static int mregion_brks_inited = 0;
void mregion_init() {
  void *ptr;

  ptr = (void*)DYNAMIC_MAP_REGION;
  mregion_brks_inited = 1;
  
  /* set brk points */
  /* for use by mregions, exos_malloc, and paging */
  ptr = (void*)PGROUNDUP((unsigned int)ptr);
  __exos_pinned_brkpt_aligned = __exos_pinned_brkpt = ptr;
  /* for use by all ExOS code (__malloc) */
  /* leaves 4 megs for the pinned region */
  __exos_brkpt_aligned = __exos_brkpt = ptr + 4*1024*1024;
}

int mregion_fault(void *faddr, unsigned int errcode) {
  unsigned int i, k;
  struct mregion *mr;

  if (!mregion_brks_inited) mregion_init();
  mr = mregion_find(mr_top, faddr, &i);

  if (!mr) return 0;

  mr = getsubregion(mr, i);

  k = get2bits(mr);
  if (!mr || k != mr_2BITS_UPTR) return 0;
  mr = clear2bits(mr);

  return ((struct mregion_ustruct*)mr)->handler((struct mregion_ustruct*)mr,
						faddr, errcode);
}

static char *exos_pinned_brk_aligned(const char *addr) {
  int diff;

  addr = (char*)PGROUNDUP((unsigned int)addr);
  
  diff = (unsigned int)addr - (unsigned int)__exos_pinned_brkpt_aligned;

  if (diff > 0) {
    if (__vm_alloc_region((unsigned int)__exos_pinned_brkpt_aligned, diff,
			  0, PG_U|PG_W|PG_P) < 0)
      return (void*)-1;
    __exos_pinned_brkpt_aligned += diff;
  } else if (diff < 0) {
    diff = -diff;
    if (__vm_free_region((unsigned int)addr, diff, 0) < 0) return (void*)-1;
    __exos_pinned_brkpt_aligned -= diff;
  }

  return 0;
}

char *exos_pinned_brk(const char *addr) {
  if (!mregion_brks_inited) mregion_init();
  if (exos_pinned_brk_aligned(addr) < 0) return (void*)-1;
  __exos_pinned_brkpt = (char*)addr;

  return 0;
}

char *exos_pinned_sbrk(int incr) {
  if (!mregion_brks_inited) mregion_init();
  if (exos_pinned_brk_aligned(__exos_pinned_brkpt + incr) < 0)
    return (void*)-1;
  __exos_pinned_brkpt += incr;
  return __exos_pinned_brkpt - incr;
}

static int __brk_handler(struct mregion_ustruct *mru, void *faddr,
			 unsigned int errcode) {
  /* if page not present then make one present! */
  if (!(errcode & FEC_PR)) {
    if (__vm_alloc_region(PGROUNDDOWN((unsigned int)faddr), 1, 0,
			  PG_U|PG_W|PG_P) < 0)
      return 0;
    return 1;
  }
  return 0;
}

static struct mregion_ustruct __brk_ustruct = { __brk_handler };

static char *__brk_aligned(const char *addr) {
  int diff;

  addr = (void*)PGROUNDUP((unsigned int)addr);
  
  diff = (unsigned int)addr - (unsigned int)__exos_brkpt_aligned;

  if (diff > 0) {
    if (mregion_alloc(__exos_brkpt_aligned, diff, &__brk_ustruct) < 0)
      return (void*)-1;
    __exos_brkpt_aligned += diff;
  } else if (diff < 0) {
    diff = -diff;
    if (__vm_free_region((unsigned int)addr, diff, 0) < 0 ||
	mregion_free(addr, addr + diff - 1) < 0)
      return (void*)-1;
    __exos_brkpt_aligned -= diff;
  }

  return 0;
}

char *__brk(const char *addr) {
  if (!mregion_brks_inited) mregion_init();
  if (__brk_aligned(addr) < 0) return (void*)-1;
  __exos_brkpt = (char*)addr;

  return 0;
}

char *__sbrk(int incr) {
  if (!mregion_brks_inited) mregion_init();
  if (__brk_aligned(__exos_brkpt + incr) < 0) return (void*)-1;
  __exos_brkpt += incr;
  return __exos_brkpt - incr;
}
