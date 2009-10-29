
/*
 * Copyright 1999 MIT
 */

#ifndef _VOS_SBUF_H_
#define _VOS_SBUF_H_

#include <xok/mmu.h>
#include <xok/pmap.h>

#include <vos/vm-layout.h>
#include <vos/vm.h>
#include <vos/proc.h>
#include <vos/errno.h>


/*
 * sbuf - shared buffer: each sbuf points to a piece of memory shared among
 * untrusted processes. A sbuf has a capability attached to it. Using the sbuf
 * abstraction, data structures with the same access capability (shared among
 * the same set of processes) will be allocated from the same page, therefore
 * avoiding wasting memory.
 *
 * A sbuf can be passed around by value between domains. A consequence of this
 * requirement is that they must be addressable across domains. The best way
 * to do this is via physical page numbers. For simplicity, we limit the size
 * of sbuf to be NBPG, which is adequate for just about everything.
 */

typedef struct sbuf
{
  u_int   owner;
  u_int   va;
  u_int   ppn;
  u_int   size;
  u_short capability;

  struct sbuf *next;
} sbuf_t;

#define SBUF_PTE_OFFSET(s) ((s)->va-(PGROUNDDOWN((s)->va)))
#define SBUF(s,t) ((t) (s).va)
#define SBUF_PPN_EQUALS(s,b) ((s)->ppn == (b)->ppn)


/* sbuf_pool: pool of memory that we create sbufs from */
typedef struct sbuf_pool
{
  u_short     capability; 	/* capability on pages in the pool */
  u_int       prot_flag;	/* protection flag for pages in the pool */
  sbuf_t*     sbufs; 		/* sbufs pointing to free space */

  struct sbuf_pool *next;
} sbuf_pool_t;



/* sbuf_alloc: allocates a new sbuf that has the capability number k. The
 * protection flag given will be tapped onto the page IN ADDITON to PG_U and
 * PG_P. If prot_flag is PG_W, the memory b points to will not be shared at a
 * fork. If prot_flag is PG_W|PG_SHARED, the memory b points to be will shared
 * after fork. Returns 0 if successful. Otherwise returns -1. Errno is set to:
 *    
 *   V_INVALID:     given sbuf is null
 *   V_NOMEM:       no more memory
 *   V_SBUF_TOOBIG: given sbuf too big (bigger than NBPG)
 */
extern int
sbuf_alloc(u_short k, size_t size, sbuf_t* b, u_int prot_flag);


/* sbuf_free: frees a given sbuf. Returns 0 if successful. Otherwise, returns
 * -1. Errno is set to:
 *
 *   V_INVALID: given subf is null
 *   V_NOMEM:   not enough memory to create a duplicate sbuf in pool 
 */
extern int
sbuf_free(sbuf_t* b);


/* sbuf_map_page: maps a page in the sbuf va region. If given pte is 0, create
 * a new page first. Returns the va where the page is mapped if successful.
 * Otherwise, returns -1. Errno is set to: 
 * 
 *   V_NOMEM:   page creation (call to vm_alloc_region) failed
 *   V_CANTMAP: cannot map the page anywhere (vm_self_insert_pte failed)
 */
extern int
sbuf_map_page(int k, Pte *pte);


/* sbuf_find_mapping: given the physical page number, find the va where it is
 * mapped to in the sbuf va region. Returns the va if found. Otherwise,
 * returns -1. Errno is set to V_INVALID.
 */
extern int 
sbuf_find_mapping(u_int ppn);


/*
 * shared_malloc: similar to malloc, but creates memory with PG_W|PG_SHARED
 * flag on (shared across fork/exec). k is the capability on the shared
 * memory.  Can only allocate  upto NBPG. Returns the memory pointer if
 * succeeded, NULL otherwise. Errnos are the same as those for sbuf_alloc.
 */
extern inline void *
shared_malloc(u_short k, size_t size)
{
  sbuf_t b;
  int r;

  r = sbuf_alloc(k, size, &b, PG_W|PG_SHARED);

  if (r < 0)
    return NULL;

  return SBUF(b,void *);
}


/*
 * shared_free: similar to free, but free memory with PG_W|PG_SHARED flag on
 * and in the sbuf region. k is the capability on the shared memory. Returns 0
 * if successful. -1 otherwise. Errno is set to those defined by sbuf_free.
 */
extern inline int
shared_free(void *ptr, u_short k, size_t size)
{
  sbuf_t b;

  b.owner = getpid();
  b.va = (u_int)ptr;
  b.ppn = va2ppn((u_int)ptr);
  b.size = size;
  b.capability = k;

  return sbuf_free(&b);
}



#endif /* __VOS_SBUF_H__ */

