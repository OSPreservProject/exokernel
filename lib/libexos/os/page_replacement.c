
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

/* paging operations and data structures for page replacement policies.
   exports:
   _exos_page_request_register
   _exos_paging_init
   _exos_self_insert_pte
   _exos_check_paging
 */
#include <assert.h>
#include <xok/bc.h>
#include <xok/env.h>
#include <xok/kerrno.h>
#include <xok/mmu.h>
#include <xok/queue.h>
#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>

#include <exos/ipc.h>
#include <stdlib.h>
#include <exos/page_replacement.h>
#include <exos/page_io.h>
#include <exos/cap.h>
#include <exos/critical.h>
#include <exos/mallocs.h>
#include <exos/mregion.h>
#include <exos/locks.h>
#include <exos/pager.h>
#include <exos/vm.h>
#include <unistd.h>


static inline void set_freeable_pages(p) {
#if 0
  struct Env e;

  e = *env_id2env(sys_geteid());
  e.env_freeable_pages = p;
  assert(sys_wrenv(0, sys_geteid(), &e) == 0);
#endif
}

static inline u_int get_freeable_pages() {
#if 0
  return env_id2env(sys_geteid())->env_freeable_pages;
#endif
  return 0;
}

static u_int pageable_pages = 0;
static u_int activeq_pages = 0;
static u_int inactiveq_pages = 0;
static u_int freeableq_pages = 0;
static u_int sharedq_pages = 0;

/* per-pageable page info */
struct page_entry {
  struct page_info *pis;
  u_int num_pis:16;
  u_int flags:16; /* see vm.h */
  u_int q:3; /* which of the paging queues is this page on */
#define ACTIVE_Q   1
#define INACTIVE_Q 2
#define FREEABLE_Q 3
#define SHARED_Q   4
  TAILQ_ENTRY(page_entry) page_pq; /* paging queue */
  struct _exos_page_io_data epid;
};

struct paging_ustruct {
  struct mregion_ustruct mru;
  struct page_entry *pe;
  struct mregion_ustruct *oldmru;
};

TAILQ_HEAD(activeq, page_entry);
TAILQ_HEAD(inactiveq, page_entry);
TAILQ_HEAD(freeableq, page_entry);
TAILQ_HEAD(sharedq, page_entry);
static struct activeq activeq_head;
static struct inactiveq inactiveq_head;
static struct freeableq freeableq_head;
static struct sharedq sharedq_head;

/* XXX - need better error function */
#define PAGING_ERROR() { sys_cputs("Internal paging error\n"); while(1); }

struct _exos_pager *_exos_program_pager=NULL;

/* XXX - ignores SHARED_Q */
static inline u_int NEXT_Q(u_int q) {
  if (q == ACTIVE_Q)
    return INACTIVE_Q;
  if (q == INACTIVE_Q)
    return FREEABLE_Q;
  return ACTIVE_Q;
}

static int Q_REMOVE(struct page_entry *pe) {
  if (pe->q == ACTIVE_Q) {
    TAILQ_REMOVE(&activeq_head, pe, page_pq);
    activeq_pages--;
  } else if (pe->q == INACTIVE_Q) {
    TAILQ_REMOVE(&inactiveq_head, pe, page_pq);
    inactiveq_pages--;
  } else if (pe->q == FREEABLE_Q) {
    u_int p;

    TAILQ_REMOVE(&freeableq_head, pe, page_pq);
  
    dlockputs(__PPAGE_LD,"Q_REMOVE get lock ");
    EXOS_LOCK(PPAGE_LOCK);
    dlockputs(__PPAGE_LD,"... got lock\n");
    EnterCritical();

    p = get_freeable_pages();
    assert(p >= 0);
    set_freeable_pages(p - 1);

    EXOS_UNLOCK(PPAGE_LOCK);
    dlockputs(__PPAGE_LD,"Q_REMOVE release lock\n");
    ExitCritical();

    freeableq_pages--;
  } else if (pe->q == SHARED_Q) {
    TAILQ_REMOVE(&sharedq_head, pe, page_pq);
    sharedq_pages--;
  } else {
    PAGING_ERROR();
    return -1;
  }
  return 0;
}

static int Q_INSERT_TAIL(struct page_entry *pe) {
  if (pe->q == ACTIVE_Q) {
    TAILQ_INSERT_TAIL(&activeq_head, pe, page_pq);
    activeq_pages++;
  } else if (pe->q == INACTIVE_Q) {
    TAILQ_INSERT_TAIL(&inactiveq_head, pe, page_pq);
    inactiveq_pages++;
  } else if (pe->q == FREEABLE_Q) {
    TAILQ_INSERT_TAIL(&freeableq_head, pe, page_pq);
    
    dlockputs(__PPAGE_LD,"Q_INSERT_TAIL get lock ");
    EXOS_LOCK(PPAGE_LOCK);
    dlockputs(__PPAGE_LD,"... got lock\n");
    EnterCritical();

    set_freeable_pages(get_freeable_pages() + 1);

    EXOS_UNLOCK(PPAGE_LOCK);
    dlockputs(__PPAGE_LD,"Q_INSERT_TAIL release lock\n");
    ExitCritical();
    freeableq_pages++;

  } else if (pe->q == SHARED_Q) {
    TAILQ_INSERT_TAIL(&sharedq_head, pe, page_pq);
    sharedq_pages++;
  } else {
    PAGING_ERROR();
    return -1;
  }
  return 0;
}

static inline u_int Q_SIZE(u_int q) {
  if (q == ACTIVE_Q) return activeq_pages;
  if (q == INACTIVE_Q) return inactiveq_pages;
  if (q == FREEABLE_Q) return freeableq_pages;
  if (q == SHARED_Q) return sharedq_pages;
  PAGING_ERROR();
  return (0);
}

static int paging_inited = 0;
void _exos_paging_init() {
  if (paging_inited) PAGING_ERROR();
  TAILQ_INIT(&activeq_head);
  TAILQ_INIT(&inactiveq_head);
  TAILQ_INIT(&freeableq_head);
  TAILQ_INIT(&sharedq_head);
  paging_inited = 1;
}

#if 0
static int paging_fault_handler(struct mregion_ustruct *mru, void *faddr,
				unsigned int errcode) {
  struct paging_ustruct *pu = (struct paging_ustruct*)mru;

  /* if it's not mapped, then remap it as it was originally mapped */
  if (!isvamapped(faddr)) {
    int i;

    if (pu->pe->num_pis == 0) return -1;
    /* read page and map to first location */
    if (_exos_page_io_page_in(&pu->pe->epid, &pu->pe->pis[0]) == -1)
      return -1;
    /* map into other locations */
    for (i=1; i < pu->pe->num_pis; i++)
      if (_exos_self_insert_pte(0, (vpt[pu->pe->pis[i].vpage] & ~PGMASK) |
				pu->pe->pis[1].flags,
				vp2va(pu->pe->pis[i].vpage),
				pu->pe->flags) == -1)
	return -1;
    return 0;
  } else /* forward the fault */
    return pu->oldmru->handler(pu->oldmru, faddr, errcode);
}
#endif

/* returns 1 if valid and not shared, 0 otherwise */
static int valid_and_getflags(struct page_entry *pe, u_int *porflags,
			      u_int *pandflags) {
  int i, rv=0;
  u_int andflags = PGMASK;
  Pte pte = 0;

  if (!pe) return 0;

  if (porflags) *porflags = 0;
  
  /* if there's no page there, then structure corrupted */
  for (i=0; i < pe->num_pis; i++)
    if (!isvamapped(vp2va(pe->pis[i].vpage))) {
      Q_REMOVE(pe);
      PAGING_ERROR();
      return 0;
    } else {
      if (porflags) *porflags |= (PGMASK & vpt[pe->pis[i].vpage]);
      andflags &= vpt[pe->pis[i].vpage];
      pte = vpt[pe->pis[i].vpage];
      rv = 1;
    }

  if (rv == 0) return 0;

#if 0 /* XXX */

  /* if it's shared, it should be in the shared queue */
  if (__ppages[PGNO(pte)].pp_refcnt > pe->num_pis ||
      (__ppages[PGNO(pte)].pp_refcnt > 1 && !(andflags & PG_COW))) {
    struct Vpage *vp = KLIST_UPTR(&__ppages[PGNO(pte)].pp_vpl.lh_first, UVP);
    
    while (vp) {
      if (vp->vp_env != __envid) {
	Q_REMOVE(pe);
	pe->q = SHARED_Q;
	Q_INSERT_TAIL(pe);
	return 0;
      }
      vp = KLIST_UPTR(&vp->vp_link.le_next, UVP);
    }
  }
#endif

  if (pandflags) *pandflags = andflags;

  return 1;
}

static int pe_clear_PG_A(struct page_entry *pe) {
  int i;

  if (!pe) return -1;

  for (i=0; i < pe->num_pis; i++)
    if ((vpt[pe->pis[i].vpage] & PG_A) &&
	sys_self_insert_pte(0, vpt[pe->pis[i].vpage] & ~PG_A,
			    vp2va(pe->pis[i].vpage))) {
      return -1;
    }

  return 0;
}

static int pe_free_each(struct page_entry *pe) {
  int i;

  if (!pe) return -1;

  for (i=0; i < pe->num_pis; i++)
    if (_exos_self_unmap_page(CAP_ROOT, vp2va(pe->pis[i].vpage)))
      return -1;

  return 0;
}


/* free up to numpages, return the actual number freed */
static u_int check_freeable(u_int numpages, u_int priority) {
  struct page_entry *pe, *next_pe;
  u_int numfreed = 0, orflags, andflags;
  struct bc_entry *bce;

return 0; /* XXX - not ready for use yet */
  pe = freeableq_head.tqh_first;
  while (pe) {
    
    dlockputs(__PPAGE_LD,"check_freeable get lock ");
    EXOS_LOCK(PPAGE_LOCK);
    dlockputs(__PPAGE_LD,"... got lock\n");
    EnterCritical();
    next_pe = pe->page_pq.tqe_next;

    if (valid_and_getflags(pe, &orflags, &andflags) == 0) {
      /* nothing - not valid */
    }
    /* if accessed lately, then move to the active q */
    else if (orflags & PG_A) {
      Q_REMOVE(pe);
      pe->q = ACTIVE_Q;
      Q_INSERT_TAIL(pe);
      if (pe_clear_PG_A(pe) == -1) {
        EXOS_UNLOCK(PPAGE_LOCK);
        dlockputs(__PPAGE_LD,"check_freeable release lock\n");
	ExitCritical();
	return -1;
      }
    }
    /* if dirty, but not accessed lately, then it should
       be on the inactive queue instead of the freeable */
    else if (orflags & PG_D) {
      Q_REMOVE(pe);
      pe->q = INACTIVE_Q;
      Q_INSERT_TAIL(pe);
    }
    /* if page is being cleaned as we speak then skip */
    else if ((bce = _exos_ppn_to_bce(PGNO(vpt[pe->pis[0].vpage]))) &&
	     (bce->buf_state & BC_VALID) &&
	     !(bce->buf_state & BC_GOING_OUT)) {
      /* nothing */
    }
    /* the page is immediately freeable */
    else {
      if (pe_free_each(pe) == -1) {
        EXOS_UNLOCK(PPAGE_LOCK);
        dlockputs(__PPAGE_LD,"check_freeable release lock\n");
	ExitCritical();
	return -1;
      }
      pageable_pages--;
      Q_REMOVE(pe);
      numfreed++;
      if (numfreed >= numpages) {
        EXOS_UNLOCK(PPAGE_LOCK);
        dlockputs(__PPAGE_LD,"check_freeable release lock\n");
	ExitCritical();
	return numfreed;
      }
    }
    pe = next_pe;
    EXOS_UNLOCK(PPAGE_LOCK);
    dlockputs(__PPAGE_LD,"check_freeable release lock\n");
    ExitCritical();
  }

  return numfreed;
}

/* Need pages? This function will get them. It decides where to get
   them from */
static int policy(u_int numpages) {
  u_int numfreed = 0;
  int starte, cure;

  /* First - do we have any pages we can free immediately ourselves */
  numfreed += check_freeable(numpages, EPAGER_PRIORITY_STANDARD);
  if (numfreed >= numpages) return numfreed;

  /* if we don't have immediately freeable pages then look for
     someone who does */
  starte = random() % NENV;
  cure = starte;
#if 0
  do {
    if (__envs[cure].env_status == ENV_OK &&
	__envs[cure].env_freeable_pages > 0) {
      int p;

      /* ask for a page */
      p = sipcout(__envs[cure].env_id, IPC_PAGE_REQ, 1, 0, 0);
      if (p > 0) numfreed += p;
    }
    cure = (cure + 1) & NENV;
  } while (cure != starte && numfreed < numpages);
#endif
  if (numfreed >= numpages) return numfreed;

  /* XXX - if we still don't have enough pages then we need to be 
     more foreceful */
  assert(0);
}

void _exos_page_request_register() {
  _exos_program_pager = _exos_pager_register(check_freeable);
}

#if 0
static int pageq_add(u_int vpage, int q) {
  struct page_entry *pe;
  struct mregion_ustruct *mru;
  struct paging_ustruct *pu;

  mru = mregion_get_ustruct((void*)vp2va(vpage));
  pu = exos_pinned_malloc(sizeof(*pu));
  if (pu == NULL) return -1;
  pu->mru.handler = paging_fault_handler;
  pe = &(pu->pe);
  pe->vpage = vpage;
  pe->q = q;
  _exos_page_io_data_init(&pe->epid);
  if (Q_INSERT_TAIL(pe) < 0) {
    exos_pinned_free(pu);
    return -1;
  }
  if (mregion_alloc((void*)vp2va(vpage), NBPG,
		    (struct mregion_ustruct*)pu) < 0) {
    Q_REMOVE(pe);
    exos_pinned_free(pu);
    return -1;
  }

  pu->oldmru = mru;

  pageable_pages++;
  
  return 0;
}

static int pageq_remove_pu(struct paging_ustruct *pu, u_int vpage) {
  int i;

  if (pu == NULL) { /* this wouldn't make sense */
    PAGING_ERROR();
    return;
  }

  for (i=0; i < pu->pe->num_pis; i++)
    if (pu->pe->pis[i].vpage == vpage) break;

  if (i == pu->pe->num_pis) {
    return -1;
  }

  if (pu->pe->num_pis > 1) {
    pu->pe->pis[i] = pu->pe->pis[pu->pe->num_pis-1];
    pu->pe->num_pis--;
    pu->pe->pis = exos_pinned_realloc(pu->pe->pis,
				      pu->pe->num_pis*
				      sizeof(struct page_info));
  } else {
    pageable_pages--;
    Q_REMOVE(pu->pe);
    exos_pinned_free(pu->pe->pis);
    exos_pinned_free(pu->pe);
  }

  if (pu->oldmru == NULL) {
    if (mregion_free((void*)vp2va(vpage), (void*)vp2va(vpage) + NBPG - 1))
      PAGING_ERROR();
  }
  else {
    if (mregion_alloc((void*)vp2va(vpage), NBPG, pu->oldmru))
      PAGING_ERROR();
  }
  exos_pinned_free(pu);

  return 0;
}
#endif

int _exos_paging_self_insert_pte(u_int k, Pte pte, u_int va, u_int flags) {
  static int already_here = 0;
  int r, tryagain=0;
  Pte oldpte;

  return sys_self_insert_pte(k, pte, va);

  if (already_here || UAREA.pid < 3) /* XXX - ugly */
    return sys_self_insert_pte(k, pte, va);
  already_here = 1;

  /* try and get the current pte */
  if ((vpd[PDENO(va)] & (PG_P | PG_U)) == (PG_P|PG_U))
    oldpte = vpt[PGNO(va)];
  else
    oldpte = 0;

  do {
    r = sys_self_insert_pte(k, pte, va);
    if (r == 0) { /* if succeeded... */
      struct paging_ustruct *pu;

      pu = (struct paging_ustruct*)mregion_get_ustruct((void*)va);
      /* if there's already a pageable page there... */
#if 0
      if (pu && pu->mru->handler == paging_fault_handler) {
	if (_exos_page_io_unuse_page(&pu->pe.epid) == -1 ||
	    pageq_remove_pu(pu, PGNO(va)) == -1) {
	  r = -E_INVAL; /* XXX need better return value */
	  break;
	}
      }

      /* we have a new phyical page mapping */
      if ((pte & PG_P)) {
	if (!paging_inited) _exos_paging_init();
	pageq_add(PGNO(va), ACTIVE_Q); /* XXX - check for error */
      }
#endif
    }
    /* if out of memory... */
    else if (r == -E_NO_MEM && PGNO(pte) == 0 && (pte & PG_P))
      if (policy(1) >= 1)
	tryagain = 1;      
      else
	tryagain = 0;
  } while (tryagain);
  
  already_here = 0; 
  return r;
}

/* check a percentage of pages per absolute time */
/* XXX - make this dynamically settable */
#define PAGING_PERCENT 5
#define PAGING_PERIOD_USECS 5000000
void _exos_check_paging() {
#if 0
  static unsigned long long last_tick_count = 0;
  static u_int cur_q = FREEABLE_Q, cur_to_check = 0;
  static int already_here = 0;
#endif

  return;
#if 0
  /* prevent recursive calls or paging of the startup progs */
  if (already_here || UAREA.pid < 3) return; /* XXX - ugly */
  already_here = 1;

  if (!last_tick_count) /* init counter */
    last_tick_count = __sysinfo.si_system_ticks;
  else if ((__sysinfo.si_system_ticks - last_tick_count) * __sysinfo.si_rate >
	   PAGING_PERIOD_USECS) { /* time to check some pages */
    u_int to_check = ((pageable_pages * PAGING_PERCENT) / 100) + 1;
    struct page_entry *pe;

    while (pageable_pages > 0 && to_check > 0)
      if (!cur_to_check) {
	/* if we've checked enough pages in the current q, then go to next q */
	cur_q = NEXT_Q(cur_q);
	cur_to_check = Q_SIZE(cur_q);
      }
      else {
	Pte pte;

	switch (cur_q) {
	case ACTIVE_Q:
	  pe = activeq_head.tqh_first;
	  if ((pte = valid_and_not_shared(pe))) {
	    Q_REMOVE(pe);
	    /* if accessed then put at end of active q, else inactive q */
	    if (pte & PG_A)
	      assert(sys_self_insert_pte(0, pte & ~PG_A,
					 vp2va(pe->vpage)) == 0); /* XXX */
	    else
	      pe->q = INACTIVE_Q;
	    Q_INSERT_TAIL(pe);
	    to_check--;
	    cur_to_check--;
	  }
	  else cur_to_check = 0;
	  break;

	case INACTIVE_Q:
	  pe = inactiveq_head.tqh_first;
	  if ((pte = valid_and_not_shared(pe))) {
	    Q_REMOVE(pe);
	    /* if active, then move to active q */
	    if (pte & PG_A) {
	      pe->q = ACTIVE_Q;
	      assert(sys_self_insert_pte(0, pte & ~PG_A,
					 vp2va(pe->vpage)) == 0); /* XXX */
	    }
	    /* if inactive, then clean it and put on freeable q */
	    else {
	      pe->q = FREEABLE_Q;
	      if (pte & PG_D) {
		assert(_exos_page_io_page_clean(pe->vpage, &pe->epid) == 0);
	      }
	    }
	    Q_INSERT_TAIL(pe);
	    to_check--;
	    cur_to_check--;
	  }
	  else cur_to_check = 0;
	  break;

	case FREEABLE_Q:
	  pe = freeableq_head.tqh_first;
	  if ((pte = valid_and_not_shared(pe))) {
	    Q_REMOVE(pe);
	    /* if active, then move to active q, else to end of freeable q */
	    if (pte & PG_A) {
	      pe->q = ACTIVE_Q;
	      assert(sys_self_insert_pte(0, pte & ~PG_A,
					 vp2va(pe->vpage)) == 0); /* XXX */
	    }
	    Q_INSERT_TAIL(pe);
	    to_check--;
	    cur_to_check--;
	  }
	  else cur_to_check = 0;
	  break;
	}
      }

    last_tick_count = __sysinfo.si_system_ticks;
  }

  already_here = 0;
#endif
}

/* XXX */
/* ensure that a page is considered pageable.
   if it's not mapped, then this will set the pte for the vpage.
   if it is mapped, it'll save the pte in the info struct for the page */
int _exos_paging_make_pageable(u_int vpage, Pte pte) {
  /* invalid pte */
  /*  if (pte & PG_P) return -1;

  if (!isvamapped(vp2va(vpage))) {
    if (sys_self_insert_pte(0, pte, vp2va(vpage))) return -1;
  } else {
    struct page_entry *pe;

    pe = htable_lookup(vpage);
    if (!pe) {
      if (pageq_add(vpage, ACTIVE_Q)) return -1;
    }
    pe->pte = pte;
  }


  struct paging_ustruct *pu;

  pu = (struct paging_ustruct*)mregion_get_ustruct(vp2va(vpage));
  if (pu == NULL) {
    
  }


  */

  return 0;
}

