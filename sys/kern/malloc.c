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

/* examined by tom */

#define __MALLOC_MODULE__

#include <xok/defs.h>
#include <xok_include/string.h>
#include <xok/mmu.h>
#include <xok/pmap.h>
#include <xok/mplock.h>
#include <xok/sysinfo.h>
#include <xok/malloc.h>
#include <xok_include/assert.h>

#define LOG2MINALLOC 4
#define MINALLOC (1<<LOG2MINALLOC) /* 16 bytes */
#define MAXBS (1 + (PGSHIFT - LOG2MINALLOC)) /* 9 */

static inline int 
fls (int) __attribute__ ((const));
     static inline int
       fls (const int v)
{
  int ret;
  asm ("movl $0xffffffff,%0\n"
       "\tbsrl %1,%0\n"
       "1:\tincl %0\n"
: "=&r" (ret): "r" (v):"cc");
  return (ret);
}

#define fls(_v)						\
({							\
  int v = (_v);						\
  int ret;						\
  if (__builtin_constant_p (_v))			\
    ret = (v & 0x80000000) ? 32 : (v & 0x40000000) ? 31	\
      : (v & 0x20000000) ? 30 : (v & 0x10000000) ? 29	\
      : (v & 0x08000000) ? 28 : (v & 0x04000000) ? 27	\
      : (v & 0x02000000) ? 26 : (v & 0x01000000) ? 25	\
      : (v & 0x00800000) ? 24 : (v & 0x00400000) ? 23	\
      : (v & 0x00200000) ? 22 : (v & 0x00100000) ? 21	\
      : (v & 0x00080000) ? 20 : (v & 0x00040000) ? 19	\
      : (v & 0x00020000) ? 18 : (v & 0x00010000) ? 17	\
      : (v & 0x00008000) ? 16 : (v & 0x00004000) ? 15	\
      : (v & 0x00002000) ? 14 : (v & 0x00001000) ? 13	\
      : (v & 0x00000800) ? 12 : (v & 0x00000400) ? 11	\
      : (v & 0x00000200) ? 10 : (v & 0x00000100) ? 9	\
      : (v & 0x00000080) ? 8 : (v & 0x00000040) ? 7	\
      : (v & 0x00000020) ? 6 : (v & 0x00000010) ? 5	\
      : (v & 0x00000008) ? 4 : (v & 0x00000004) ? 3	\
      : (v & 0x00000002) ? 2 : (v & 0x00000001) ? 1	\
      : 0;						\
  else							\
    ret = fls (v);					\
  ret;							\
})

typedef struct _selfp {
  struct _selfp *p;
} *selfp;

struct blocksize {
  selfp bs_list;		/* List of free blocks */
  u_int bs_free;		/* Number of free blocks in list */
  u_int bs_bppg;		/* # blocks in a page */
  u_int unused;
};

static struct blocksize bs[MAXBS];
static u_int size_mask[MAXBS];

static inline u_int
bsnum (u_int size)
{
  int bsn;

  bsn = fls (size) - (LOG2MINALLOC + 1);
  if (bsn < 0)
    bsn = 0;
  if (bsn >= MAXBS) {
    warn ("bsnum: bsn > MAXBS");
    return (32 - LOG2MINALLOC + 1);	/* XXX -- no idea if this is right */
  }
  if (size & size_mask[bsn])
    bsn++;
  if (bsn >= MAXBS) {
    warn ("bsnum: bsn > MAXBS");
    return (32 - LOG2MINALLOC + 1);	/* XXX -- no idea if this is right */
  }
  return (bsn);
}

void
malloc_init (void)
{
  int i;

  for (i = 0; i < MAXBS; i++) {
    bs[i].bs_list = 0L;
    bs[i].bs_free = 0;
    bs[i].bs_bppg = 1 << (PGSHIFT - (i + LOG2MINALLOC));
    size_mask[i] = (1 << (i + LOG2MINALLOC)) - 1;
  }
}


#ifdef __SMP__
// #undef _USE_LOCKS_
#define _USE_LOCKS_ 1
#else
#define _USE_LOCKS_ 1
#endif


void *
malloc (u_int size) 
  __XOK_SYNC(calls ppage_alloc_n; 
                locks ppage->pp_klock 
	     or locks MALLOC_LOCK;
	           locks ppage->pp_klock;
		or calls ppage_alloc;
		   locks ppage->pp_klock)
{
  struct blocksize *bsp;
  selfp r, p;
  struct Ppage *pp;
  int i, bsn;
#ifndef _USE_LOCKS_
  int retries = 0;
#endif

  if (size == 0) return (NULL);

  /* 1st try */

  if (size > NBPG) 
  {
    /* multipage malloc -- don't even try to use the remainder of
       last page */
    bsn = (size + NBPG - 1) / NBPG;
    
    if (ppage_alloc_n (PP_KERNEL, &pp, bsn) < 0) 
    {
      warn ("malloc: could not alloc %d contiguous pages (nfreepages %d)",
	    bsn, SYSINFO_GET(si_nfreepages));

      return NULL;
    }

    /* no sync needed because pages not visible to other CPUs */
    Ppage_pp_bs_set(pp, (bsn << 8) | MAXBS);
    for (i = 1; i < bsn; i++) 
      Ppage_pp_bs_set(Ppagearray_get(pp,i), MAXBS);  // pp[i].pp_bs = MAXBS;
    return (pp2va (pp));
  }

  /* second try */

#ifdef _USE_LOCKS_
  MP_SPINLOCK_GET(GLOCK(MALLOC_LOCK));
#endif

  bsn = bsnum(size);
  bsp = &bs[bsn];
  size = 1 << (bsn + LOG2MINALLOC);

#ifndef _USE_LOCKS_
retry:
  retries++;
#endif

  r = bsp->bs_list;
 
  if (r) 
  {
#ifndef _USE_LOCKS_
    int bs_free = bsp->bs_free;
    int atomic = compare_and_swap_64
      ((volatile int*)bsp, (u_int)r, bs_free, (u_int)r->p, bs_free-1);
    
    if (atomic < 0) goto retry;
#else
    bsp->bs_list = r->p;
    bsp->bs_free--;
#endif 

    pp = kva2pp ((u_long)r);
    Ppage_pp_pcfc_atomic_dec(pp);

#ifdef _USE_LOCKS_
    MP_SPINLOCK_RELEASE(GLOCK(MALLOC_LOCK));
#endif
    return (r);
  }

  /* last try */

  if (ppage_alloc (PP_KERNEL, &pp, 0) < 0) 
  {
#ifdef _USE_LOCKS_
    MP_SPINLOCK_RELEASE(GLOCK(MALLOC_LOCK));
#endif
    warn ("malloc: could not alloc page (nfreepages %d)", 
	  SYSINFO_GET(si_nfreepages));
    return NULL;
  }

  /* no sync needed because pages not visible to other CPUs */
  Ppage_pp_bs_set(pp,bsn);               // pp->pp_bs = bsn;
  Ppage_pp_pcfc_set(pp, bsp->bs_bppg-1); // pp->pp_pcfc = bsp->bs_bppg - 1;

  r = pp2va (pp);
  if (size < NBPG) 
  {
    p = (void *) r + size;
    for (i = 1; i < bsp->bs_bppg - 1; i++) {
      selfp n = (void *) p + size;
      p->p = n;
      p = n;
    }
    p->p = NULL;
  }

#ifndef _USE_LOCKS_
  {
    int bs_free = bsp->bs_free;
    int atomic;
    u_int newptr;
   
    if (size >= NBPG)
      newptr = 0;
    else
      newptr = (u_int)((void*)r+size);
    
    atomic = compare_and_swap_64
      ((volatile int*)bsp, 0, bs_free, newptr, bs_free+bsp->bs_bppg-1);

    if (atomic<0) 
    {
      /* no sync needed because pages not visible to other CPUs */
      Ppage_pp_bs_set(pp,0);                 // pp->pp_bs = 0;
      Ppage_pp_klock_acquire(pp);
      ppage_free (pp);
      Ppage_pp_klock_release(pp);
      goto retry;
    }
  }
#else
  if (size < NBPG) 
  {
    bsp->bs_free += bsp->bs_bppg - 1;
    bsp->bs_list = (void *) r + size;
  }
#endif

#ifdef _USE_LOCKS_
  MP_SPINLOCK_RELEASE(GLOCK(MALLOC_LOCK));
#endif
  return (r);
}

void
free (void *ptr) 
  __XOK_SYNC(locks pp->pp_klock; 
                calls ppage_free; 
	     or releases pp->pp_klock;
                locks MALLOC_LOCK;
	        locks pp->pp_klock; 
	        calls ppage_free)
{
  selfp r, *r2;
  selfp p = ptr;
  struct Ppage *pp;
  struct blocksize *bsp;
#ifndef _USE_LOCKS_
  int bs_free, atomic;
#endif

  if (!p) return;

  pp = kva2pp ((u_long) p);

  /* deal with multipage frees */
  if ((Ppage_pp_bs_get(pp) & 0xff) == MAXBS) 
  {
    /* don't need to sync ppage in the following code: we assume we won't call
     * free on two different CPUs with the same or aliased ptrs */

    int i, n;
    n = Ppage_pp_bs_get(pp) >> 8; // n = pp->pp_bs >> 8;
    for (i = 0; i < n; i++) 
    {
      Ppage_pp_bs_set(pp, 0); // pp->pp_bs = 0;
      Ppage_pp_klock_acquire(pp);
      ppage_free (pp);
      Ppage_pp_klock_release(pp);
      INC_PTR(pp,Ppage,1); // pp++;
    }
    return;
  }

  if (Ppage_pp_status_get(pp) != PP_KERNEL) 
  {
    warn ("free: attempt to free a non-kernel page");
    return;
  }
 
#ifdef _USE_LOCKS_
  MP_SPINLOCK_GET(GLOCK(MALLOC_LOCK));
#endif
  
  bsp = &bs[Ppage_pp_bs_get(pp)];
  Ppage_pp_pcfc_atomic_inc(pp);

#ifndef _USE_LOCKS_
retry:
  p->p = bsp->bs_list;
  bs_free = bsp->bs_free;
  
  atomic = compare_and_swap_64 
    ((volatile int*)bsp, (u_int)p->p, bs_free, (u_int)p, bs_free+1);
  if (atomic < 0) goto retry;
  
  bs_free++;
#else
  p->p = bsp->bs_list;
  bsp->bs_list = p;
  bsp->bs_free++;
#endif

  /* Recycle: if the entire page is now free and there are plenty of other
   * blocks of the same size then free the page */

#ifdef _USE_LOCKS_ 
  /* don't need to sync ppage, MALLOC_LOCK is enough */

  if (Ppage_pp_pcfc_get(pp) == bsp->bs_bppg &&
      bsp->bs_free >= (3 * bsp->bs_bppg)) 
  {
    r = bsp->bs_list;
    r2 = &bsp->bs_list;
    while (r) {
      if (PGNO((u_int)r) == PGNO((u_int)ptr))
	*r2 = r->p;
      else
	r2 = (selfp*)r;
      r = r->p;
    }
    bsp->bs_free -= bsp->bs_bppg;
    Ppage_pp_bs_set(pp,0);
    Ppage_pp_klock_acquire(pp);
    ppage_free (pp);
    Ppage_pp_klock_release(pp);
  }
#else
  /* this is perhaps a bit less efficient, but since we won't be doing this
   * too often, acceptable: we remove the whole list using compare and swap,
   * then modify list, then place it back on. this way our list will never be
   * referenced by anyone else.
   */
retry_recycle:
  p = bsp->bs_list;
  bs_free = bsp->bs_free;

  /*
   * XXX - how do we prevent two CPUs trying the following at the same time!!!
   * I don't think it will be fatal, the second CPU simply won't find anything
   * to remove, but it would be inefficient.
   */

  if (Ppage_pp_pcfc_get(pp) == bsp->bs_bppg &&
      bsp->bs_free >= (3 * bsp->bs_bppg)) 
  {
    selfp* head;
    int bs_free_tmp;

    atomic = compare_and_swap_64((volatile int*)bsp,(u_int)p,bs_free,0,0);
    if (atomic < 0) 
      goto retry_recycle;
    
    head = &p;

    /* don't need to sync ppage below, since it is no longer on the master
     * list, only us can traverse it */

    /* have to retest just in case */
    if (Ppage_pp_pcfc_get(pp) == bsp->bs_bppg &&
        bs_free >= (3 * bsp->bs_bppg)) 
    {
      r = p;
      r2 = &p;
      while (r) 
      {
        if (PGNO((u_int)r) == PGNO((u_int)ptr))
	  *r2 = r->p;
        else
	  r2 = (selfp*)r;
        r = r->p;
      }
      bs_free -= bsp->bs_bppg;
      Ppage_pp_bs_set(pp,0);
      Ppage_pp_klock_acquire(pp);
      ppage_free (pp);
      Ppage_pp_klock_release(pp);
    } 
    
    else
    {
      r = p;
      r2 = &p;
      while (r)
      {
	r2 = (selfp*)r;
	r = r->p;
      }
    }

    /* r2 is now the tail of the out-of-main-list list */
retry_putback:
    bs_free_tmp = bsp->bs_free;
    *r2 = bsp->bs_list;
    atomic = compare_and_swap_64
      ((volatile int*)bsp, (u_int)*r2,   bs_free_tmp,
                           (u_int)*head, bs_free+bs_free_tmp);
    if (atomic < 0) 
      goto retry_putback;
  }
#endif

#ifdef _USE_LOCKS_
  MP_SPINLOCK_RELEASE(GLOCK(MALLOC_LOCK));
#endif
}

#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif
