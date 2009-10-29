
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

#ifndef _XOK_PMAP_H_
#define _XOK_PMAP_H_

/*
 * This file contains specifications of data structures and functions
 * dealing with physical pages. 
 */


#include <xok_include/assert.h>
#include <xok/capability.h>
#include <xok/queue.h>
#include <xok/vpage.h>
#include <xok/mplock.h>

#include <xok/pmap_decl.h>

/* contains macros that produce data encapsulation inline functions */
#include "../tools/data-encap/encap.h"

struct Env;
struct EnvPD;


/**************************
 **    struct Pp_state   **
 **************************/

struct Pp_state;

#ifdef KERNEL

/* returns the size of the structure */
STRUCT_SIZEOF_DECL(Pp_state)

/* returns an allocated structure */
ALLOCATOR_DECL(Pp_state)

/* frees an allocated structure */
DESTRUCTOR_DECL(Pp_state)

/* returns Pp_state->ps_writers */
FIELD_SIMPLE_READER_DECL(Pp_state, ps_writers, unsigned char)

/* returns Pp_state->ps_readers */
FIELD_SIMPLE_READER_DECL(Pp_state, ps_readers, unsigned char)

/* sets Pp_state->ps_writers = newval */
FIELD_ASSIGN_DECL(Pp_state, ps_writers, unsigned char)

/* sets Pp_state->ps_readers = newval */
FIELD_ASSIGN_DECL(Pp_state, ps_readers, unsigned char)

/* returns true if pp can be moved to state s */
static inline int ppcompat_state 
  (u_int pp_readers, u_int pp_writers, const struct Pp_state *s);

#endif



/***********************
 **    struct Ppage   **
 ***********************/

struct Ppage;

/* 
 * Ppage list bookkepping, in kernel only 
 */

/* the following defines two Ppage lists: a linked list and a queue */
LIST_HEAD(Ppage_list, Ppage);
typedef LIST_ENTRY(Ppage) Ppage_LIST_entry_t;

TAILQ_HEAD(free_buf_list, Ppage);
typedef TAILQ_ENTRY(Ppage) Ppage_TAILQ_entry_t;

#ifdef KERNEL

/* performs LIST_INSERT_HEAD */
static inline void Ppagelist_head_insert(struct Ppage_list*, struct Ppage*);

/* performs LIST_REMOVE */
static inline void Ppagelist_head_remove(struct Ppage* pp);

/* performs TAILQ_REMOVE */
static inline void 
freebuflist_tail_remove(struct free_buf_list*, struct Ppage*);

/* performs TAILQ_INSERT_TAIL */
static inline void 
freebuflist_tail_insert(struct free_buf_list*, struct Ppage*);


/*
 * general access functions - open to all
 */

/* returns the size of the structure */
STRUCT_SIZEOF_DECL(Ppage)

/* returns Ppage->pp_refcnt */
FIELD_SIMPLE_READER_DECL(Ppage, pp_refcnt, u_short)

/* returns Ppage->pp_bs */
FIELD_SIMPLE_READER_DECL(Ppage, pp_bs, u_short)

/* returns Ppage->pp_pcfc */
FIELD_SIMPLE_READER_DECL(Ppage, pp_pcfc, u_short)

/* returns Ppage->pp_status */
FIELD_SIMPLE_READER_DECL(Ppage, pp_status, unsigned char)

/* returns Ppage->pp_readers */
FIELD_SIMPLE_READER_DECL(Ppage, pp_readers, u_short)

/* returns Ppage->pp_writers */
FIELD_SIMPLE_READER_DECL(Ppage, pp_writers, u_short)

/* returns Ppage->pp_pinned */
FIELD_SIMPLE_READER_DECL(Ppage, pp_pinned, int)

/* acquire lock on Ppage */
static inline void Ppage_pp_klock_acquire(struct Ppage *pp);

/* try to acquire lock on Ppage, returns 0 if successful */
static inline int Ppage_pp_klock_try(struct Ppage *pp);

/* release lock on Ppage */
static inline void Ppage_pp_klock_release(struct Ppage *pp);

/*
 * access functions only visible to BC and PMAP modules 
 * (read functions visible to user level code)
 */

#if defined(__BC_MODULE__) || defined(__PMAP_MODULE__)

/* returns Ppage->pp_buf */
FIELD_SIMPLE_READER_DECL(Ppage, pp_buf, struct bc_entry*)

/* returns &Ppage->pp_state */
FIELD_PTR_READER_DECL(Ppage, pp_state, struct Pp_state*)

/* sets Ppage->pp_buf = b */
FIELD_ASSIGN_DECL(Ppage, pp_buf, struct bc_entry *)

/* sets Ppage->pp_state = *ps */
FIELD_COPYIN_DECL(Ppage, pp_state, struct Pp_state *)

#endif /* __BC_MODULE__ || __PMAP_MODULE__ */

/*
 * access functions only visible to MALLOC and PMAP modules
 */

#if defined(__MALLOC_MODULE__) || defined(__PMAP_MODULE__)

/* sets Ppage->pp_bs */
FIELD_ASSIGN_DECL(Ppage, pp_bs, u_short)

/* sets Ppage->pp_pcfc */
FIELD_ASSIGN_DECL(Ppage, pp_pcfc, u_short)

static inline void Ppage_pp_pcfc_atomic_inc(struct Ppage *pp);
static inline void Ppage_pp_pcfc_atomic_dec(struct Ppage *pp);

#endif /* __MALLOC_MODULE__ or __PMAP_MODULE__ */

/*
 * access functions only visible to PMAP modules 
 * (read functions visible to user level code)
 */

#if defined(__PMAP_MODULE__)

/* returns Ppage->pp_acl size */
FIELD_SIZEOF_DECL(Ppage, pp_acl)

/* returns Ppage->pp_acl */
FIELD_SIMPLE_READER_DECL(Ppage, pp_acl, cap*)

/* returns &Ppage->pp_acl[index] */
ARRAY_PTR_READER_DECL(Ppage, pp_acl, cap*)

/* returns &Ppage->pp_link */
FIELD_PTR_READER_DECL(Ppage, pp_link, Ppage_LIST_entry_t*)

/* returns &Ppage->pp_buf_link */
FIELD_PTR_READER_DECL(Ppage, pp_buf_link, Ppage_TAILQ_entry_t*)

/* returns &Ppage->pp_vpl */
FIELD_PTR_READER_DECL(Ppage, pp_vpl, Vpage_list_t*)

/* returns &Ppage->pp_klock */
FIELD_PTR_READER_DECL(Ppage, pp_klock, struct kspinlock *)

/* sets Ppage->pp_refcnt */
FIELD_ASSIGN_DECL(Ppage, pp_refcnt, u_short)

/* sets Ppage->pp_status */
FIELD_ASSIGN_DECL(Ppage, pp_status, unsigned char)

/* sets Ppage->pp_link = *e */
FIELD_COPYIN_DECL(Ppage, pp_link, Ppage_LIST_entry_t*)

/* sets Ppage->pp_buf_link = *e */
FIELD_COPYIN_DECL(Ppage, pp_buf_link, Ppage_TAILQ_entry_t*)

/* sets Ppage->pp_vpl = *vpl */
FIELD_COPYIN_DECL(Ppage, pp_vpl, Vpage_list_t*)

/* sets Ppage->pp_acl[index] = *c */
ARRAY_COPYIN_DECL(Ppage, pp_acl, cap *)

/* sets Ppage->pp_readers */
FIELD_ASSIGN_DECL(Ppage, pp_readers, u_short)

/* sets Ppage->pp_writers */
FIELD_ASSIGN_DECL(Ppage, pp_writers, u_short)

/* sets Ppage->pp_pinned */
FIELD_ASSIGN_DECL(Ppage, pp_pinned, int)

#endif /* __PMAP_MODULE__ */

#endif /* KERNEL */




/****************************
 **   global ppage table   **
 ****************************/


#ifndef KERNEL  /* libOS - __ppages */

/* this is the array of all physical pages */
extern struct Ppage __ppages[];

#else           /* KERNEL - ppages */

extern struct Ppage *ppages;
extern u_long nppage;

ARRAY_REF_DECL(ppages, Ppage)

GEN_ARRAY_REF_DECL(Ppage)

#endif /* KERNEL */




/*******************************
 **   pmap module in kernel   **
 **                           **
 **   pmap.c  and  pmapP.h    **
 *******************************/

#ifdef KERNEL

/* initializes all the Ppage structures */
void ppage_init (void);

/* allocates n contiguous physical page frames */
int  ppage_alloc_n (u_char, struct Ppage **, int);

/* allocates a physical page */
int  ppage_alloc (u_char, struct Ppage **, int userreq);

/* guards physical page with capability */
void ppage_setcap (struct Ppage *pp, cap *c);

/* moves a page to the proper free list depending on if it contains a buffer */
void ppage_free (struct Ppage *);

/* maps a page in a specified environment */
int  ppage_insert (struct Env *, struct Ppage *, u_int, u_int);

/* unmaps a page from an environment. returns 0 if successful. if returns -1,
 * environment can no longer be run because env_u is removed ... */
int ppage_remove (struct Env *const e, u_int va);

/* moves a page from free_bufs to free_list - the page is removed from the
 * buffer cache */
void ppage_free_bufs_2_free_list (struct Ppage *pp);

/* removes a page from the free_bufs list and allow the page to be mapped
 * again. keeps the buffer in the page */
void ppage_reuse_buffer (struct Ppage *pp);

/* specifically moves a page off the free_bufs list and initialize it in
 * preparation of being mapped. DANGER: this will happily reuse dirty buffers
 * which might not be what you want */
void ppage_reclaim_buffer (struct Ppage *pp, int type);

/* like ppage_alloc, but go ahead and inserts the allocated page into the
 * buffer cache and put it on free_bufs. Good for reading data into the buffer
 * cache yet still allowing the page to be reclaimed if memory gets low */
struct bc_entry *
ppage_get_reclaimable_buffer (u32 dev, u_quad_t block, int bc_state, 
                              int *error, int userreq);

/* specifically moves a page off the free page list and initialize it in
 * preparation of being mapped */
void ppage_reclaim_page (struct Ppage *pp, int type);
   
/* removes a page from either free_list or free_bufs, depending upon which
 * it's on and then init it */
void ppage_reclaim (struct Ppage *pp, int type);

/* allows a physical page where some i/o device lies to be mapped.  */
void ppage_grant_iomem (struct Ppage *pp);

/* allocates a page directory entry if one is needed for the va, insert the
 * new entry in all the page directories for the environment given */
int  pde_check_n_alloc (struct EnvPD* envpd, u_int va);

/* same as pde_check_n_alloc, but only insert to the specified pdir */
int  pde_check_n_alloc_one (Pde *const, u_int va);

/* create a new page directory by copying all PDEs from an existing directory.
 * Does not hierarchically copy PTEs. Returns the page containing the
 * directory. */
struct Ppage * pd_duplicate(Pde *const);

/* inserts a range of pages into the page table of the environment specified.
 * The num_completed field allows the function to be interrupted by a page
 * fault or to return an error and then restart at the proper location without
 * redoing what's already been done */
int  
pmap_insert_range (u_int sn, u_int k, Pte *ptes, u_int num, u_int va, 
                   u_int *num_completed, u_int ke, int envid, int userreq);

/* invalidates the TLB if envid is current environment's id */
void tlb_invalidate (u_int va, u_int addrspace_id);

/* marks a page pinned. increment the system pinned page count if this is the
 * first time the page is pinned */
void ppage_pin (struct Ppage *pp);

/* unpins a page. decrement the system pinned page count if necessary */
void ppage_unpin (struct Ppage *pp);

/* returns result of performing acl access check on ppage's cap */
int ppage_acl_check (struct Ppage *pp, cap *c, int len, u_char perm);

/* zero out ppage acl */
void ppage_acl_zero (struct Ppage *pp);

/* returns true if ppage is reclaimable */
static inline int ppage_is_reclaimable(const struct Ppage *pp);

/* translates from a Ppage structure to a page number */
static inline int pp2ppn(struct Ppage*);

/* translates from a Ppage structure to a physical address */
#define pp2pa(ppage_p) ((u_long) (pp2ppn (ppage_p) << PGSHIFT))

/* translates from a Ppage structure to a virtual address */
#define pp2va(ppage_p) ((void *) (KERNBASE + pp2pa (ppage_p)))

/* translates from a physical address to a Ppage structure */
static inline struct Ppage* pa2pp(u_long kva);

/* translates from a virtual address to a Ppage structure */
static inline struct Ppage* kva2pp(u_long kva);

/* returns the page table entry for address va in page directory pdir */
static inline Pte *pt_get_ptep (const Pde * pdir, u_int va);



#endif /* KERNEL */
#endif /* _XOK_PMAP_H_ */

#if !defined(__ENCAP__) || !defined(KERNEL)
#include <xok/pmapP.h>
#endif

