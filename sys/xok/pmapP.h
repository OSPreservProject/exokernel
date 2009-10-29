
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

#ifndef _XOK_PMAP_P_H_
#define _XOK_PMAP_P_H_

/*
 * This file contains implementation of data structures and functions
 * dealing with physical pages. 
 */

#include <xok_include/assert.h>
#include <xok/capability.h>
#include <xok/queue.h>
#include <xok/bc.h>
#include <xok/vpage.h>
#include <xok/malloc.h>
#include <xok/mplock.h>

/* the specification for this file */
#include <xok/pmap.h>



/**************************
 **    struct Pp_state   **
 **************************/

struct Pp_state {	          
  unsigned char ps_writers: 4;
  unsigned char ps_readers: 4;
};

#ifdef KERNEL

STRUCT_SIZEOF(Pp_state)

ALLOCATOR(Pp_state)

DESTRUCTOR(Pp_state)

FIELD_SIMPLE_READER(Pp_state,ps_writers,unsigned char)

FIELD_SIMPLE_READER(Pp_state, ps_readers, unsigned char)

FIELD_ASSIGN(Pp_state, ps_writers, unsigned char)

FIELD_ASSIGN(Pp_state, ps_readers, unsigned char)

static inline int 
ppcompat_state (u_int pp_readers, u_int pp_writers, const struct Pp_state *s) 
{
  if (s->ps_readers == PP_ACCESS_ALL && s->ps_writers == PP_ACCESS_ALL)
    return 1;
  if (pp_readers > s->ps_readers || pp_writers > s->ps_writers)
    return 0;
  return 1;
}
#endif



/***********************
 **    struct Ppage   **
 ***********************/

struct Ppage {
  union {
    Ppage_LIST_entry_t ppu_link;   	/* free list link */
    Ppage_TAILQ_entry_t ppu_buf_link; 	/* free but filled buf link */
    struct {
      Vpage_list_t ppuv_pl;       	/* vpage list */
      u_short ppuv_rcbn;          	/* refcount/blocksize # */
      u_short ppuv_pcfc;          	/* pagecount/free count */
    } ppl_v;
  } pp_u;
#define pp_link pp_u.ppu_link
#define pp_buf_link pp_u.ppu_buf_link
#define pp_vpl pp_u.ppl_v.ppuv_pl
#define pp_refcnt pp_u.ppl_v.ppuv_rcbn
#define pp_bs pp_u.ppl_v.ppuv_rcbn
#define pp_pcfc pp_u.ppl_v.ppuv_pcfc
  unsigned char pp_status;        /* status of ppage - see pmap.h */
  struct Pp_state pp_state;       /* access limits to each page */
  cap pp_acl[PP_ACL_LEN];
  u_short pp_readers, pp_writers; /* number of each type of mapping */
  struct bc_entry *pp_buf;	  /* buf struct if page is in buffer cache */
  int pp_pinned;
  struct kspinlock pp_klock; /* kernel lock for this Ppage struct */
};

#ifdef KERNEL

static inline void 
Ppagelist_head_insert(struct Ppage_list* plist, struct Ppage* pp)
{
  LIST_INSERT_HEAD(plist, pp, pp_link);
}

static inline void 
Ppagelist_head_remove(struct Ppage* pp)
{
  LIST_REMOVE(pp, pp_link);
}

static inline void 
freebuflist_tail_insert(struct free_buf_list* flist, struct Ppage* pp)
{
  TAILQ_INSERT_TAIL(flist, pp, pp_buf_link);
}

static inline void 
freebuflist_tail_remove(struct free_buf_list* flist, struct Ppage* pp)
{
  TAILQ_REMOVE(flist, pp, pp_buf_link);
}

/* general functions */

STRUCT_SIZEOF(Ppage)

static inline u_short 
Ppage_pp_refcnt_get(struct Ppage* pp)
{
  return pp->pp_refcnt;
}

static inline u_short 
Ppage_pp_bs_get(struct Ppage* pp)
{
  return pp->pp_bs;
}

static inline u_short 
Ppage_pp_pcfc_get(struct Ppage* pp)
{
  return pp->pp_pcfc;
}

static inline unsigned char 
Ppage_pp_status_get(struct Ppage* pp)
{
  return pp->pp_status;
}

static inline u_short 
Ppage_pp_readers_get(struct Ppage* pp)
{
  return pp->pp_readers;
}

static inline u_short 
Ppage_pp_writers_get(struct Ppage* pp)
{
  return pp->pp_writers;
}

static inline int 
Ppage_pp_pinned_get(struct Ppage * pp)
{
  return pp->pp_pinned;
}

static inline void 
Ppage_pp_klock_acquire(struct Ppage *pp)
{
  extern u_quad_t lock_pp_counter;
  lock_pp_counter++;
  MP_SPINLOCK_GET(&pp->pp_klock);
}

static inline int 
Ppage_pp_klock_try(struct Ppage *pp)
{
  extern u_quad_t lock_pp_counter;
  lock_pp_counter++;
#ifdef __SMP__
  if (smp_commenced)
    return kspinlock_try(&pp->pp_klock);
  else
#endif
    return 0;
}

static inline void 
Ppage_pp_klock_release(struct Ppage *pp)
{
  MP_SPINLOCK_RELEASE(&pp->pp_klock);
}

/* BC and PMAP module only, but read functions open to USER */

#if defined(__BC_MODULE__) || defined(__PMAP_MODULE__)

static inline struct Pp_state* 
Ppage_pp_state_ptr(struct Ppage* pp)
{
  return &(pp->pp_state);
}

static inline struct bc_entry* 
Ppage_pp_buf_get(struct Ppage* pp)
{
  return pp->pp_buf;
}

static inline void 
Ppage_pp_buf_set(struct Ppage* pp, struct bc_entry* b)
{
  pp->pp_buf = b;
}

static inline void 
Ppage_pp_state_copyin(struct Ppage* pp, struct Pp_state* ps)
{
  pp->pp_state = *ps;
}

#endif /* __BC_MODULE__ || __PMAP_MODULE__ */

/* MALLOC and PMAP only */

#if defined(__MALLOC_MODULE__) || defined(__PMAP_MODULE__)

static inline void 
Ppage_pp_bs_set(struct Ppage* pp, u_short newval)
{
  pp->pp_bs = newval;
}

static inline void 
Ppage_pp_pcfc_set(struct Ppage* pp, u_short newval)
{
  pp->pp_pcfc = newval;
}

static inline void 
Ppage_pp_pcfc_atomic_inc(struct Ppage *pp)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (pp->pp_pcfc) : "0" (pp->pp_pcfc));
}

static inline void 
Ppage_pp_pcfc_atomic_dec(struct Ppage *pp)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (pp->pp_pcfc) : "0" (pp->pp_pcfc));
}

#endif /* __MALLOC_MODULE__ or __PMAP_MODULE__ */

/* PMAP only, except read functions open to USER */

#if defined(__PMAP_MODULE__)

FIELD_SIZEOF(Ppage, pp_acl)

static inline cap* 
Ppage_pp_acl_ptr_at(struct Ppage* pp, int index)
{
  return &(pp->pp_acl[index]);
}

static inline cap* 
Ppage_pp_acl_get(struct Ppage* pp)
{
  return pp->pp_acl;
}

static inline Ppage_LIST_entry_t* 
Ppage_pp_link_ptr(struct Ppage* pp)
{
  return &(pp->pp_link);
}

static inline Ppage_TAILQ_entry_t* 
Ppage_pp_buf_link_ptr(struct Ppage* pp)
{
  return &(pp->pp_buf_link);
}

static inline Vpage_list_t* 
Ppage_pp_vpl_ptr(struct Ppage* pp)
{
  return &(pp->pp_vpl);
}

static inline void 
Ppage_pp_refcnt_set(struct Ppage* pp, u_short newval)
{
  pp->pp_refcnt = newval;
}

static inline void 
Ppage_pp_status_set(struct Ppage* pp, unsigned char newval)
{
  pp->pp_status = newval;
}

static inline void 
Ppage_pp_link_copyin(struct Ppage* pp, Ppage_LIST_entry_t *e)
{
  pp->pp_link = *e;
}

static inline void 
Ppage_pp_buf_link_copyin(struct Ppage* pp, Ppage_TAILQ_entry_t *e)
{
  pp->pp_buf_link = *e;
}

static inline void 
Ppage_pp_vpl_copyin(struct Ppage* pp, Vpage_list_t* vpl)
{
  pp->pp_vpl = *vpl;
}

static inline void 
Ppage_pp_acl_copyin_at(struct Ppage* pp, int index, cap *c)
{
  pp->pp_acl[index] = *c;
}

static inline void 
Ppage_pp_readers_set(struct Ppage* pp, u_short newval)
{
  pp->pp_readers = newval;
}

static inline void 
Ppage_pp_writers_set(struct Ppage* pp, u_short newval)
{
  pp->pp_writers = newval;
}

static inline void 
Ppage_pp_pinned_set(struct Ppage * pp, int newval)
{
  pp->pp_pinned = newval;
}

static inline struct kspinlock* 
Ppage_pp_klock_ptr(struct Ppage * pp)
{
  return &(pp->pp_klock);
}

#endif /* __PMAP_MODULE__ */

#endif /* KERNEL */



/****************************
 **   global ppage table   **
 ****************************/

#ifdef KERNEL

GEN_ARRAY_REF(Ppage)

ARRAY_REF(ppages, Ppage)

#endif /* KERNEL */





/*********************************************
 **   some pmap module routines in kernel   **
 *********************************************/

#ifdef KERNEL

static inline int 
pp2ppn(struct Ppage* pp)
{
  return ((pp) - ppages);
}

static inline struct Ppage* 
pa2pp(u_long kva)
{
  u_long ppn = (u_long) (kva) >> PGSHIFT;
  if (ppn >= nppage)
    panic (__FUNCTION__ ":%d: pa2pp called with invalid pa",
	   __LINE__);
  return ppages_get(ppn); 
}

static inline struct Ppage* 
kva2pp(u_long kva)
{
  u_long ppn = ((u_long) (kva) - KERNBASE) >> PGSHIFT;
  if (ppn >= nppage)
    panic (__FUNCTION__ ":%d: kva2pp called with invalid kva",
       __LINE__);
  return ppages_get(ppn);
}

static inline Pte *
pt_get_ptep (const Pde * pdir, u_int va) 
{
  Pde pde;
  Pte *pt;

  pde = pdir[PDENO (va)];
  pt = ptov (pde & ~PGMASK);
  return (&pt[PTENO (va)]);
}

static inline int
ppage_is_reclaimable(const struct Ppage* pp)
{
  return ((pp->pp_status == PP_FREE) && (pp->pp_pinned == 0) && 
      ((pp->pp_buf == NULL) || (bc_is_reclaimable(pp->pp_buf))));
}

#endif /* KERNEL */

#endif /* _XOK_PMAP_P_H_ */

