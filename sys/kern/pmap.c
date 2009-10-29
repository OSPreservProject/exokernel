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

#define __PMAP_MODULE__

#include <xok/pmap.h>
#include <xok/mplock.h>
#include <xok/sysinfo.h>
#include <xok/defs.h>
#include <xok/env.h>
#include <i386/isa/nvram.h>
#include <xok/sys_proto.h>
#include <xok/syscall.h>
#include <xok/syscallno.h>
#include <xok/kerrno.h>
#include <xok_include/assert.h>
#include <xok_include/stdlib.h>	/* for rand */
#include <xok/bc.h>
#include <xok/scheduler.h>
#include <xok/disk.h>		/* for MAX_DISKS */
#include <xok/locore.h>
#include <xok/printf.h>
#include <xok/cpu.h>
#ifdef __SMP__
#include <xok/smp.h>
#endif
#include <ubb/xn.h>


/*
 * This file contains routines for ppage abstraction and routines for
 * manipulating the page directory of an environment.
 *
 * An environment's page directory abstraction (struct EnvPD) may contain
 * multiple page directories, one for each CPU. This is required because on
 * each CPU we may map a certain pages differently for performance reasons
 * (CPUCXT, KSTACKTOP, and UAREA ). All of the page directories in a EnvPD
 * object share the same page tables, but each page directory has its own page
 * directory page with PDEs. Therefore, when modifying a PDE, changes need to
 * propagate to all page directories. When modifying a PTE, changes are
 * automatically reflected in all page directories, but TLB shootdown must be
 * done if page directories are active on other CPUs.
 */

/* A few words about pages and buffers...

   Each page has a type (stored in pp_status) that is one of PP_FREE,
   PP_USER, or one of several other PP_ values (see pmap.h). This is
   fairly broken since these values are either redundant or ignored. For
   example, there is already a reference count field that could be used
   to determine if a page is free or not. Most other values of pp_status
   are ignored. 

   Each page has three independent characteristics:

   A page is either mapped or free. If it is mapped, pp_status !=
   PP_FREE and pp_refcnt > 0. If the page is not mapped, pp_status ==
   PP_FREE. 

   A page may also hold some block from some device and therefore be a
   buffer (meaning it's in the kernel block buffer registry--see bc.c
   for more details). PP_HOLDS_BUFFER(struct Ppage *) is true if a
   given page contains a buffer. In this case, pp_buf will point to
   the bc_entry of the associated buffer.

   A page may also be pinned for pending dma. Allocation requests will
   not be satisfied with pinned pages even if they are free. This
   prevents someone from allocating a page, starting a disk read into
   it, releasing the page, having a second process allocate that page,
   store data into it, and then having it trashed when suddenly the
   disk completes the read request and dma's data into the
   memory. PP_IS_PINNED(struct Ppage *) is true if a physical page is
   pinned.

   There are three key data structures used by pmap. First, is the
   ppage array which contains one struct Ppage structure for each
   phsyical page. The second is the free_list of free physical pages
   that do not contain a buffer. The final is the free_bufs queue
   which holds free pages that do contain a buffer. The point of
   seperating the free pages is to reuse pages containing buffers
   after pages that do not contain blocks buffers. A free page must be
   on either free_list or free_bufs but not both.

   ppage_find_free and friends are carefull to never reclaim a dirty
   buffer since this probably contains data that is expected to be
   written out at some point. Be carefull that you respect the
   convention of leaving dirty data alone unless you really mean
   to be doing this.

   The interfaces exported by this file fall into two categories: internal
   kernel interfaces for other kernel subsystems and interfaces for system
   calls and upcalls to user level programs. See pmap.h for descriptions of
   the interfaces.

*/



struct Ppage *ppages;		/* Array of all Ppage structures */
struct Sysinfo *si;		/* Random information about the system */
u_long nppage;			/* Number of physical pages */

Pte *ppage_upt;			/* Pt for read-only ppage structures */
Pte *envsi_upt;			/* Pt for read-only sysinfo and envs */
Pte *xnmap_upt;			/* Pt for xn registry and xn disk free map */
Pte *cpucxt_pts[NR_CPUS];	/* Pt for cpu contexts */
Pte *kstack_pts[NR_CPUS];	/* Pt for kernel stacks */

static struct Ppage_list free_list;	/* Free list of physical pages */
static struct free_buf_list free_bufs;	/* Filled but unmapped buffers */

static unsigned long maxphys;	/* Highest physical address */



/* 
 * Allocate a page table if one is needed for VA va. Insert the page table
 * into all page directories for the environment given. No TLB shootdown is
 * necessary because page table did not exist anyways.
 */

inline int
pde_check_n_alloc (struct EnvPD* envpd, u_int va) 
  __XOK_REQ_SYNC(on envpd->envpd_spinlock)
{
  Pde *pdep;

#if 0
  printf("%p %d %d %d \n", 
         envpd, envpd->envpd_rc, envpd->envpd_active, 
	 envpd->envpd_cr3[envpd->envpd_active]);
#endif

  assert(envpd && envpd->envpd_rc > 0 && 
         envpd->envpd_active >= 0 && 
         envpd->envpd_active <= NR_CPUS && 
	 envpd->envpd_cr3[envpd->envpd_active] != 0);

  pdep = &(envpd->envpd_pdir[envpd->envpd_active])[PDENO (va)];

  if (!(*pdep & PG_P))
  {
    struct Ppage *pp;
    int r;
    
    if ((r = ppage_alloc (PP_KERNEL, &pp, 0)) < 0) 
    { 
      warn ("pde_check_n_alloc: could not alloc page for pt"); 
      return r; 
    }
    
    bzero (pp2va (pp), NBPG);
    *pdep = pp2pa (pp) | PG_P | PG_W | PG_U;

    /* updating all page directories */
    for(r=0; r<NR_CPUS; r++)
    {
      if (r != envpd->envpd_active && envpd->envpd_cr3[r]!=0)
      {
  	pdep = &(envpd->envpd_pdir[r])[PDENO (va)];
        *pdep = pp2pa (pp) | PG_P | PG_W | PG_U;
      }
    }
  }

  return 0;
}


/* same as pde_check_n_alloc, but only modify the given pdir */
inline int
pde_check_n_alloc_one (Pde *const pdir, u_int va) 
  __XOK_REQ_SYNC(on envpd->envpd_spinlock)
{
  Pde *pdep;

  pdep = &pdir[PDENO (va)];

  if (!(*pdep & PG_P))
  {
    struct Ppage *pp;
    int r;

    if ((r = ppage_alloc (PP_KERNEL, &pp, 0)) < 0) 
    { 
      warn ("pde_check_n_alloc_one: could not alloc page for pt"); 
      return r; 
    }

    bzero (pp2va (pp), NBPG);
    *pdep = pp2pa (pp) | PG_P | PG_W | PG_U;
  }

  return 0;
}



/* 
 * Create a new page directory by copying all PDEs from an existing directory.
 * Does not hierarchically copy PTEs. Returns the page containing the
 * directory.
 */

struct Ppage *
pd_duplicate(Pde * const pdir)
{
  struct Ppage *pp;
  int r;
  u_int *newpdir;

  // printf("****** pd_duplicate on %d (env %d)\n",get_cpu_id(),curenv->env_id);

  if ((r = ppage_alloc (PP_KERNEL, &pp, 0)) < 0) 
    return 0L;

  newpdir = pp2va(pp);
  memcpy(newpdir, pdir, NBPG);

  return pp;
}



/* guard pp with the capability c */
void
ppage_setcap (struct Ppage *pp, cap * c) 
  __XOK_SYNC(locks pp->pp_klock)
{
  Ppage_pp_klock_acquire(pp);
  Ppage_pp_acl_copyin_at(pp, 0, c); // pp->pp_acl[0] = *c;
  Ppage_pp_acl_copyin_at(pp, 1, &cap_null); // pp->pp_acl[1] = cap_null;
  Ppage_pp_klock_release(pp);
}


/* Allocate fixed space in physical memory before malloc is
 * initialized.  *pt will be set to a page table containing the space
 * allocated, so that the space can be mapped read-only in user space.
 */

int ptspace_alloc_forbid = 0;
static void *
ptspace_alloc (Pte ** pt, u_int bytes) __XOK_NOSYNC
{
  u_int i, lim;
  void *ret;
  
  assert(ptspace_alloc_forbid==0);

  *pt = (void *) freemem;
  freemem += NBPG;
  ret = (void *) freemem;
  lim = (bytes + PGMASK) >> PGSHIFT;
  if (lim > NLPG)
    /* bootup */
    panic ("pt_alloc: 4 Megs exceeded!");
  bzero (*pt, NBPG);
  for (i = 0; i < lim; i++, freemem += NBPG)
    {
      bzero ((void *) freemem, NBPG);
      (*pt)[i] = (freemem - KERNBASE) | PG_P | PG_U | PG_W;
    }
  return (ret);
}


/* init the pp fields for a mappable page */
static void
ppage_initpp (struct Ppage *pp, int type) 
  __XOK_SYNC(locks pp->pp_klock)
{
  Ppage_pp_klock_acquire(pp);
  bzero (Ppage_pp_acl_get(pp), Ppage_pp_acl_sizeof(pp));
  Ppage_pp_buf_set(pp, NULL);
  Ppage_pp_status_set(pp, type);
  vpagel_init(Ppage_pp_vpl_ptr(pp));
  Ppage_pp_refcnt_set(pp,0);
  Ppage_pp_pcfc_set(pp,0);
  Ppage_pp_readers_set(pp,0);
  Ppage_pp_writers_set(pp,0);
  Pp_state_ps_writers_set(Ppage_pp_state_ptr(pp),PP_ACCESS_ALL);
  Pp_state_ps_readers_set(Ppage_pp_state_ptr(pp),PP_ACCESS_ALL);
  Ppage_pp_klock_release(pp);
}


/* probe for memory above the 64 meg mark to see if we have more memory
   than the bios reported */
unsigned int
probe_for_memory () __XOK_NOSYNC
{
  unsigned int target_page;
  int tmp, bad;

  /* XXX -- following testing code is heavily stolen from freebsd 
     sys/i386/i386/machdep.c */

  bad = 0;

  for(target_page = PGROUNDDOWN (KERNBASE + EXTPHYSMEM); 
      target_page < 0xffffe000; target_page += NBPG)
    {

      tmp = *(int *) target_page;

      /*
       * Test for alternating 1's and 0's
       */
      *(volatile int *) target_page = 0xaaaaaaaa;
      if (*(volatile int *) target_page != 0xaaaaaaaa)
	{
	  bad = 1;
	}
      /*
       * Test for alternating 0's and 1's
       */
      *(volatile int *) target_page = 0x55555555;
      if (*(volatile int *) target_page != 0x55555555)
	{
	  bad = 1;
	}
      /*
       * Test for all 1's
       */
      *(volatile int *) target_page = 0xffffffff;
      if (*(volatile int *) target_page != 0xffffffff)
	{
	  bad = 1;
	}
      /*
       * Test for all 0's
       */
      *(volatile int *) target_page = 0x0;
      if (*(volatile int *) target_page != 0x0)
	{
	  bad = 1;
	}
      /*
       * Restore original value.
       */
      *(int *) target_page = tmp;

      /* 
       * Stop scanning for memory if we found a bad page.
       */
      if (bad)
	break;
    }

  /* either we fell out of the loop or we found a bad page. either way,
     backup target_page by one page to point it to the last good page */

  target_page -= NBPG;

  /* target_page is offset by KERNBASE+EXTPHYSMEM since that's where
     we have upper physical memory mapped. remove the offset so we 
     know how much memory we actually found */

  target_page -= PGROUNDDOWN (KERNBASE + EXTPHYSMEM);
  return (target_page);
}


#ifdef __SMP__
void
ppage_init_secondary (void) __XOK_NOSYNC
{
  /* nothing to do */
}
  
#endif


/* Initialize ppage structures and reserve memory for read-only
   data structures that get mapped into each env. */

#define nvram_read(r) (mc146818_read(NULL, r))
void
ppage_init (void) __XOK_NOSYNC
{
  long basememsize, extmemsize;
  int i, lim;
  void malloc_init (void);
  extern struct bc *bc;		/* declared in bc.c */
  extern Pte *bc_upt;
  unsigned start_readonly;
  int max_pages = 0;

  printf ("Physical memory: ");

  /* As done by OpenBSD */
  basememsize = ((nvram_read (NVRAM_BASEHI) << 18)
		 | (nvram_read (NVRAM_BASELO) << 10)) & ~PGMASK;
  extmemsize = ((nvram_read (NVRAM_EXTHI) << 18)
		| (nvram_read (NVRAM_EXTLO) << 10)) & ~PGMASK;

  maxphys = extmemsize ? EXTPHYSMEM + extmemsize : basememsize;

  /* if the bios reports 64 megs we may actually have more. Probe
     for it and adjust maxmem based on what the probe finds */

  if (maxphys >= 64 * 1024 * 1024)
      maxphys = probe_for_memory ();
  
  printf ("%dK available, ", (int) (maxphys >> 10));

  /* so we can simulate fewer pages */
  nppage = maxphys >> PGSHIFT;
  if (max_pages != 0)
    if (nppage > max_pages)
      nppage = max_pages;

  freemem = (freemem + PGMASK) & ~PGMASK;	/* Not actually needed */

  LIST_INIT (&free_list);
  TAILQ_INIT (&free_bufs);

  /* remember where we started to allocate pagedirs for mapping
     kernel data structures read-only to the user */
  start_readonly = freemem;

  /* Allocate all the Ppage structures right above the kernel.  Build
   * a page table at ppage_upt so that users can access the Ppage
   * structures (read-only, but the read-only is enforced in the page
   * directory entry, not here). */
  printf("nppage %ld, Ppage_sizeof %d\n", nppage, Ppage_sizeof());
  ppages = ptspace_alloc (&ppage_upt, nppage * Ppage_sizeof());

  /* Allocate a whole page for the sysinfo structure, and then however
   * many it takes to hold the env structures. */
  SysinfoAssertSize(SYSINFO_SIZE);
  si = ptspace_alloc (&envsi_upt, SYSINFO_SIZE + NENV * sizeof (struct Env));
  bzero (si, Sysinfo_sizeof());
  __envs = (void *) si + SYSINFO_SIZE;

  /* allocate a page for the buffer cache. Rest of the pages for it
     are dynamically allocated */
  bc = ptspace_alloc (&bc_upt, NBPG);

#ifdef __SMP__
  for(i=0; i<smp_cpu_count; i++)
  {
    /* allocate CPU context */
    __cpucxts[i] = ptspace_alloc (&cpucxt_pts[i], sizeof(struct CPUContext));
    bzero(__cpucxts[i], sizeof(struct CPUContext));

    /* for APs, allocate kernel stacks */
    if (i != 0)
      ptspace_alloc (&kstack_pts[i], KSTKSIZE);
  }
#else

  /* allocate CPU context */
  __cpucxts[0] = ptspace_alloc (&cpucxt_pts[0], sizeof(struct CPUContext));
  bzero(__cpucxts[0], sizeof(struct CPUContext));

#endif
  
#if 1
  {
    /* XXX -- this really needs to go...but without it things
       crash on bootup... */

    u_int *__xn_free_map;
    void *xn_registry;

    /* allocate space for the XN registry and disk free map */
    /* defined in ubb/xn.h */
    __xn_free_map = ptspace_alloc (&xnmap_upt, UXNMAP_SIZE + UXN_SIZE);
    xn_registry = (struct xr *) ((char *) __xn_free_map + UXNMAP_SIZE);
  }
#endif

  ptspace_alloc_forbid = 1;

  if (freemem >= KERNBASE + maxphys)
    {
      /* bootup */
      panic ("ppage_init: out of physical memory");
    }

  printf ("base = %dK, ", (u_int) (basememsize >> 10));

  /* Now initialize all Ppage structures */
  i = 0;

  /* Save first page for good luck.  (maybe BIOS stores stuff there
   * that gets used for a reboot?) */
  MP_SPINLOCK_INIT(Ppage_pp_klock_ptr(ppages_get(i)));
  Ppage_pp_status_set(ppages_get(i++), PP_INVAL);

  /* save the second page for loadkern so we can reboot on
   *  top of ourselves */

  MP_SPINLOCK_INIT(Ppage_pp_klock_ptr(ppages_get(i)));
  Ppage_pp_status_set(ppages_get(i++), PP_IOMEM);

#ifndef __SMP__

  /* Low memory is all free - except for SMP mode, see below */
  for (lim = (min (basememsize, IOPHYSMEM) >> PGSHIFT); i < lim; i++)
    {
      ppage_initpp (ppages_get(i), PP_FREE);
      Ppage_pp_pinned_set(ppages_get(i),0);
      Ppagelist_head_insert (&free_list, ppages_get(i));
      Sysinfo_si_nfreepages_atomic_inc(si);
    }

#else /* SMP */
  {
    int j;

    /* for SMP, we save a page for the APs to use as the kernel stack and
     * trampoline text place, and one additional page for a p0pdir used to
     * start the APs */

    /* we do this here because according to MP spec v1.4, B-5, AP starting
     * code MUST at location 000VV000h, vectors are limited at 4k page
     * boundaries within the first megabyte of memory (since it is in real
     * mode) */

    if (smp_cpu_count < 2)
      j = 0;
    else
      j = 2;

    /* rest of low memory all free */
    for (lim = (min (basememsize, IOPHYSMEM) >> PGSHIFT) - j; i < lim; i++)
      {
	ppage_initpp (ppages_get(i), PP_FREE);
        Ppage_pp_pinned_set(ppages_get(i),0);
	Ppagelist_head_insert (&free_list, ppages_get(i));
	Sysinfo_si_nfreepages_atomic_inc(si);
      }

    if (smp_cpu_count > 1)
      {	
	/* only if there are more than one cpu */

	/* skip p0pdir_smp page */
	ap_boot_addr_base = pp2pa (ppages_get(i)) + NBPG; 

	for (lim = (min (basememsize, IOPHYSMEM) >> PGSHIFT); i < lim; i++)
	{
          Ppage_pp_status_set(ppages_get(i),PP_KERNEL);
	}
      }
  }
#endif

  /* Potential hole under 640K ? */
  for (lim = IOPHYSMEM >> PGSHIFT; i < lim; i++)
  {
    MP_SPINLOCK_INIT(Ppage_pp_klock_ptr(ppages_get(i)));
    Ppage_pp_status_set(ppages_get(i),PP_INVAL);
  }

  /* I/O hole between 640K and 1 Meg */
  for (lim = EXTPHYSMEM >> PGSHIFT; i < lim; i++)
  {
    MP_SPINLOCK_INIT(Ppage_pp_klock_ptr(ppages_get(i)));
    Ppage_pp_status_set(ppages_get(i),PP_IOMEM);
  }

  /* Kernel memory */
  for (lim = (start_readonly - KERNBASE) >> PGSHIFT; i < lim; i++)
  {
    MP_SPINLOCK_INIT(Ppage_pp_klock_ptr(ppages_get(i)));
    Ppage_pp_status_set(ppages_get(i),PP_KERNEL);
  }

  /* Kernel memory exposed read-only to the user */
  for (lim = (freemem - KERNBASE) >> PGSHIFT; i < lim; i++)
  {
    MP_SPINLOCK_INIT(Ppage_pp_klock_ptr(ppages_get(i)));
    Ppage_pp_status_set(ppages_get(i),PP_KERNRO);
  }

  /* And the rest is free */
  for (lim = nppage; i < lim; i++)
    {
      MP_SPINLOCK_INIT(Ppage_pp_klock_ptr(ppages_get(i)));
      ppage_initpp (ppages_get(i), PP_FREE);
      Ppage_pp_pinned_set(ppages_get(i),0);
      Ppagelist_head_insert (&free_list, ppages_get(i));
      Sysinfo_si_nfreepages_atomic_inc(si);
    }
 
  SYSINFO_ASSIGN(si_nppages,nppage); // si->si_nppages = nppage;
  
  malloc_init ();

  /* give access to the loadkern page to anyone with root cap */
  ppage_grant_iomem (ppages_get(1));
  
  printf ("%dK unused by kernel\n", 
          SYSINFO_GET(si_nfreepages) << (PGSHIFT - 10));
}


void
ppage_grant_iomem (struct Ppage *pp)
  __XOK_SYNC(locks pp->klock)
{
  /* guard the page by the root capability */
  Ppage_pp_klock_acquire(pp);
  ppage_initpp (pp, Ppage_pp_status_get(pp));
  Ppage_pp_acl_ptr_at(pp, 0)->c_valid = 1;
  Ppage_pp_acl_ptr_at(pp, 0)->c_perm = CL_ALL;
  Ppage_pp_klock_release(pp);
}

/* put a page on the free list. If the page contains a cached block
   we put the block on free_bufs. Otherwise we put it on free_list. 
   ppage_alloc looks for pages first on free_list and then on free_bufs. */

void
ppage_free (struct Ppage *pp)
  __XOK_REQ_SYNC(locks on pp->klock)
  __XOK_SYNC(locks PPAGE_FLIST_LOCK; PPAGE_FBUF_LOCK)
{
  if (Ppage_pp_refcnt_get(pp))
    {
      warn ("ppage_free:  attempt to free mapped page");
      return;		/* be conservative and assume page is still used */
    }
  
  if (Ppage_pp_buf_get(pp))
    {
      MP_QUEUELOCK_GET(GQLOCK(PPAGE_FBUF_LOCK));
      freebuflist_tail_insert (&free_bufs, pp);
      
      Sysinfo_si_nfreebufpages_atomic_inc(si);
     
      MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FBUF_LOCK));
    }
  else
    {
      MP_QUEUELOCK_GET(GQLOCK(PPAGE_FLIST_LOCK));
      Ppagelist_head_insert (&free_list, pp);
      MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
    }

  Ppage_pp_status_set(pp, PP_FREE);
  if (Ppage_pp_readers_get(pp) != 0 && Ppage_pp_writers_get(pp) != 0)
    {
      warn ("ppage_free: invalid reader or writer counts");
    }

  Sysinfo_si_nfreepages_atomic_inc(si);
}


/* move a page from the list of unmapped reclaimable buffers to
   the list of free pages */
void
ppage_free_bufs_2_free_list (struct Ppage *pp) 
  __XOK_REQ_SYNC(on pp_klock)
  __XOK_SYNC(calls ppage_reclaim_buffer; ppage_free)
{
  ppage_reclaim_buffer (pp, PP_FREE);
  ppage_free (pp);
}


/* get a page of memory that is put in the buffer cache and on the
   free_bufs list of unmapped reclaimable buffers */
struct bc_entry *
ppage_get_reclaimable_buffer (u32 dev, u_quad_t block, int bc_state,
			      int *error, int userreq) 
  __XOK_SYNC(calls ppage_alloc; bc_insert64; ppage_free)
{
  struct bc_entry *bc;
  struct Ppage *pp;
  int r;

  /* get a page */
  if ((r = ppage_alloc (PP_USER, &pp, userreq)) < 0) {
    *error = r;
    return NULL;
  }

  /* put it in the buffer cache XXX - check error */
  if (!(bc = bc_insert64 (dev, block, pp2ppn (pp), bc_state, &r))) {
    *error = r;
    Ppage_pp_klock_acquire(pp);
    ppage_free (pp);
    Ppage_pp_klock_release(pp);
    return NULL;
  }

  /* and free it (it'll still be in the cache and ppage_reuse_buffer
     will still be able to get it out and use it, but if memory gets
     tight it'll be reclaimable via ppage_reclaim_buffer */
  Ppage_pp_klock_acquire(pp);
  ppage_free (pp);
  Ppage_pp_klock_release(pp);

  return (bc);
}


/*  Two basic operatins on free buffers: reclaim the buffer to use the page
   for something else or resuse the page with the existing contents */
void
ppage_reclaim_buffer (struct Ppage *pp, int type) 
  __XOK_REQ_SYNC(requires pp_klock)
  __XOK_SYNC(locks PPAGE_FBUF_LOCK; 
             calls bc_is_dirty; bc_set_clean; bc_remove)
{
  MP_QUEUELOCK_GET(GQLOCK(PPAGE_FBUF_LOCK));
 
  if (bc_is_dirty (Ppage_pp_buf_get(pp)))
    {
      bc_set_clean (Ppage_pp_buf_get(pp));
    }
  
  Sysinfo_si_nfreebufpages_atomic_dec(si);
  Sysinfo_si_nfreepages_atomic_dec(si);
  
  freebuflist_tail_remove (&free_bufs, pp);

  bc_remove (Ppage_pp_buf_get(pp));
  ppage_initpp (pp, type);
  
  MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FBUF_LOCK));
}


/* move a page off free_bufs and prepare it to be mapped while keeping
   the page in the buffer cache */
void
ppage_reuse_buffer (struct Ppage *pp) 
  __XOK_SYNC(locks pp->pp_klock; PPAGE_FBUF_LOCK)
{
  Ppage_pp_klock_acquire(pp);
  MP_QUEUELOCK_GET(GQLOCK(PPAGE_FBUF_LOCK));

  Sysinfo_si_nfreebufpages_atomic_dec(si);
  Sysinfo_si_nfreepages_atomic_dec(si);

  freebuflist_tail_remove (&free_bufs, pp);
  MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FBUF_LOCK));

  /* we do a mini-initialization of the page. We want to preserve the limits
     on readers/writers, acls, the pp_buf pointer */

  Ppage_pp_status_set(pp, PP_USER);
  vpagel_init(Ppage_pp_vpl_ptr(pp));
  Ppage_pp_refcnt_set(pp, 0);
  Ppage_pp_pcfc_set(pp,0);
  Ppage_pp_klock_release(pp);
}


/* Take a free page off the free page list */
void
ppage_reclaim_page (struct Ppage *pp, int type) 
  __XOK_REQ_SYNC(locks on pp_klock)
  __XOK_SYNC(PPAGE_FLIST_LOCK)
{
  MP_QUEUELOCK_GET(GQLOCK(PPAGE_FLIST_LOCK));
 
  Ppagelist_head_remove (pp);

  Sysinfo_si_nfreepages_atomic_dec(si);

  MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
 
  ppage_initpp (pp, type);
}

/* remove a page from either free_list or free_bufs, depending upon
   which it's on and then init it. */

void
ppage_reclaim (struct Ppage *pp, int type) 
  __XOK_REQ_SYNC(locks on pp_klock)
  __XOK_SYNC(calls ppage_reclaim_buffer or ppage_reclaim_page)
{
  assert(pp!=NULL);
  
  if (Ppage_pp_buf_get(pp))
    {
      assert(Ppage_pp_buf_get(pp)!=NULL);
      ppage_reclaim_buffer (pp, type);
    }
  else
    {
      ppage_reclaim_page (pp, type);
    }
}


/* find a free page, first trying free_list and then free_bufs. If we
   have to take a page from free_bufs, we make sure to only take
   clean pages. */
static int
ppage_find_free (struct Ppage **pp, int userreq) 
  __XOK_SYNC(locks PPAGE_FLIST_LOCK; 
             condlocks ppage->pp_klock; 
	     releases PPAGE_FLIST_LOCK;
	     locks PPAGE_FBUF_LOCK;
	     condlocks ppage->pp_klock;
	     releases PPAGE_FBUF_LOCK)
{
  int n;

  n = SYSINFO_GET(si_nfreepages) - SYSINFO_GET(si_nfreebufpages);

  /* XXX - use #define for 256 */
  if (!userreq || n > 256) 
  {
    /* locking is a bit tricky here: since we need to examine list, we need to
     * lock list first. We cannot just try to acquire the lock on an element
     * of the list because that would produce potential deadlock
     * possibilities. Therefore we "try" each lock until we finds one...
     */

    MP_QUEUELOCK_GET(GQLOCK(PPAGE_FLIST_LOCK));

    /* check for a free page not holding a buffer */
    *pp = free_list.lh_first;
    while (*pp)
    {
#if 0
      /* try the lock */
      if (Ppage_pp_klock_try(*pp) != 0) 
      {
	/* did not get it */
        *pp = Ppage_pp_link_ptr(*pp)->le_next;
	continue;
      }
      /* got the lock */
#endif

      /* not pinned */
      if (Ppage_pp_pinned_get(*pp) == 0) 
      {
#if 0
        Ppage_pp_klock_release(*pp);
#endif
        MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
        return 0;
      }

      /* pinned */
#if 0
      Ppage_pp_klock_release(*pp);
#endif
      *pp = Ppage_pp_link_ptr(*pp)->le_next;
    }
    
    MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
  }

  /* ok, no free pages. try to find a page holding a clean buffer. 
     The queue is in lru order so we start at the head and work
     our way down. */

  /* locking: same deal as above, lock list, then tries to lock the elements,
   * only continue if acquired lock, otherwise, goto next element. */
  
  MP_QUEUELOCK_GET(GQLOCK(PPAGE_FBUF_LOCK));
  
  for (*pp = free_bufs.tqh_first; 
       *pp; 
       *pp = Ppage_pp_buf_link_ptr(*pp)->tqe_next)
  {
    if (Ppage_pp_klock_try(*pp) != 0)
      /* failed to acquire lock */
      continue;

    /* got the lock */

    /* not reclaimable */
    if (!ppage_is_reclaimable (*pp))
    {
      Ppage_pp_klock_release(*pp);
      continue;
    }

    Ppage_pp_klock_release(*pp);
    MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FBUF_LOCK));
    return 0;
  }

  MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FBUF_LOCK));

  /* no free pages or clean unmapped buffers found so we're out of pages */
  *pp = NULL;			/* XXX -- just to be safe */
  printf("can't find memory in ppage_find_free\n");
  printf("%d, %d, %d\n", SYSINFO_GET(si_nppages), SYSINFO_GET(si_ndpages), SYSINFO_GET(si_nfreepages));
  return -E_NO_MEM;
}


/* 
 * allocate n contiguous physical page frames (efficiency secondary...) 
 * XXX -- doesn't try to allocate pages that are on free_list first
 * and then, if that fails, try to get them from free_bufs 
 */
int
ppage_alloc_n (u_char type, struct Ppage **pp, int n) 
  __XOK_SYNC(locks PPAGE_FLIST_LOCK; 
             condlocks ppage->pp_klock;
	     calls ppage_reclaim;
	     unlocks PPAGE_FLIST_LOCK)
{
  int i;
  struct Ppage *p;

  if (n < 2)
  {
    warn ("ppage_alloc_n: invalid value for n (%d). Assuming n = 2.", n);
    n = 2;
  }

  MP_QUEUELOCK_GET(GQLOCK(PPAGE_FLIST_LOCK));
  *pp = free_list.lh_first;
  
ppage_alloc_n_repeat:
  /* Try to find a free range of pages at least starting on the free list */
  while (*pp)
    {
      p = *pp;
      
      if (Ppage_pp_klock_try(p) != 0)
      { /* didn't get the lock */

	*pp = Ppage_pp_link_ptr(*pp)->le_next;
	continue;
      }

      /* got lock on *pp/p */
      for (i = 0; i < n; i++)
	{
	  if ((p >= ppages_get(nppage)) || (!ppage_is_reclaimable (p))) 
	  {
	    Ppage_pp_klock_release(p);
	    *pp = Ppage_pp_link_ptr(*pp)->le_next;
	    goto ppage_alloc_n_repeat;
	  }

	  Ppage_pp_klock_release(p);
	  INC_PTR(p,Ppage,1);  // p++;
	  
	  if (Ppage_pp_klock_try(p) != 0)
	    /* didn't get the lock */
	  {
	    *pp = Ppage_pp_link_ptr(*pp)->le_next;
	    goto ppage_alloc_n_repeat;
	  }
	}

      Ppage_pp_klock_release(p);
      break;
    }

  if (!(*pp))
    {
      int cnt = 0;
      p = 0;
      /* Open it up and just find any free range of pages */
      for (i = 0; i < nppage; i++)
	{
	  if (!(*pp))
	    {
	      *pp = ppages_get(i);
	      p = ppages_get(i);
	    }

	  if (Ppage_pp_klock_try(p) != 0)
	  { 
	    /* no lock on p, restart from start */
	    *pp = NULL;
	    cnt = 0;
	    continue;
	  }

	  /* got lock */

	  if (!ppage_is_reclaimable (p))
	    {
	      Ppage_pp_klock_release(p);
	      
	      /* discontinous, restart */
	      *pp = NULL;
	      cnt = 0;
	    }
	  else
	    {
	      Ppage_pp_klock_release(p);
	      INC_PTR(p,Ppage,1);  // p++;
	      cnt++;
	      if (cnt == n)
		break;
	    }
	}
      if (cnt < n)
	{
          MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
	  return (-1);
	}
    }
  
  p = *pp;
  for (i = 0; i < n; i++)
    {
      /* everything on my list is already locked */
      assert(Ppage_pp_klock_try(p) == 0);
      ppage_reclaim (p, type); 
      Ppage_pp_klock_release(p);
      INC_PTR(p,Ppage,1);  // p++;
    }
  
  MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
  return (0);
}



/* allocate a single page. First tries to find a free empty page, but if
   that fails will take a page that contains an unreferenced cached block */
int
ppage_alloc (u_char type, struct Ppage **pp, int userreq) 
  __XOK_SYNC(locks PPAGE_FLIST_LOCK; PPAGE_FBUF_LOCK;
             calls ppage_find_free; ppage_reclaim)
{
  int r;

  MP_QUEUELOCK_GET(GQLOCK(PPAGE_FLIST_LOCK));
  MP_QUEUELOCK_GET(GQLOCK(PPAGE_FBUF_LOCK));

  if ((r = ppage_find_free (pp, userreq)) < 0) 
  {
    MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FBUF_LOCK));
    MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
    return r; 
  }

  assert(Ppage_pp_klock_try(*pp)==0);
  ppage_reclaim (*pp, type); 
  Ppage_pp_klock_release(*pp);
  
  MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FBUF_LOCK));
  MP_QUEUELOCK_RELEASE(GQLOCK(PPAGE_FLIST_LOCK));
  return (0);
}


volatile u_int vaptr[NR_CPUS][NR_CPUS];

inline void
tlb_invalidate (u_int va, u_int addrspace_id) 
  __XOK_NOSYNC
{
#ifdef __SMP__
  int i;
  int cpu = cpu_id;
  int not_done = 1;
#endif

  /* only flush the entry if we're modifying the current address space */
  if (!curenv || curenv->env_pd->envpd_id == addrspace_id)
    invlpg(va);

#ifdef __SMP__
  for(i = 0; i < smp_cpu_count; i++)
  {
    vaptr[cpu][i] = 0;

    if (i == cpu_id) continue;
    
    if (__cpucxts[i]->_current_pd_id == addrspace_id)
    {
      vaptr[cpu][i] = va;
      
      /* send a TLB shootdown */
      if (ipi_send_message(i,IPI_CTRL_TLBFLUSH,(u_int)&vaptr[cpu][i]) < 0)
      {
	printf("warning: cannot send tlb invalidate signal, retry once\n");
	if (ipi_send_message(i,IPI_CTRL_TLBFLUSH,(u_int)&vaptr[cpu][i]) < 0)
	  vaptr[cpu][i] = 0;
      }
    }
  }

  /* now wait til they are all done */
  while(not_done == 1)
  {
    not_done = 0;
    for (i = 0; i < smp_cpu_count; i++)
      if (vaptr[cpu][i] != 0) not_done = 1;

    /* go handle our own message... this avoids potential deadlocks */
    ipi_handler();

    asm volatile("" ::: "memory");
  }
#endif
}


/* remove a mapping of a page from an address space */
int
ppage_remove (struct Env *const e, u_int va) 
  __XOK_REQ_SYNC(on e->env_pd->envpd_spinlock)
  __XOK_SYNC(locks ppage->pp_klock; calls ppage_free)
{
  Pde *const pd = e->env_pd->envpd_pdir[e->env_pd->envpd_active];
  struct Ppage *pp;
  Pte *ptep;
  struct Vpage *vp;

  /* Find the PTE and unmap the page */
  ptep = pt_get_ptep (pd, va);
  if (*ptep & PG_P)
  {
    if ((*ptep >> PGSHIFT) >= nppage)
    {
      warn("ppage_remove: found bogus PTE 0x%08x (pd %p, envid %d, va 0x%08x)", 
	 *ptep, pd, e->env_id, va);
      return 0;
    }
     
    pp = ppages_get(*ptep >> PGSHIFT);
    Ppage_pp_klock_acquire(pp);

    if (*ptep & PG_W)
      Ppage_pp_writers_set(pp,Ppage_pp_writers_get(pp)-1);
    Ppage_pp_readers_set(pp,Ppage_pp_readers_get(pp)-1);

    if (--e->env_pd->envpd_nptes[PDENO (va)] < 0)
      warn ("ppage_remove: removed too many PTE's from a page table");

    /* Get the Vpage and free it */
    va &= ~PGMASK;
    for (vp = vpagel_get_first(Ppage_pp_vpl_ptr(pp)); vp; 
	 vp = vpagel_get_next(vp))
    {
      if (vpage_get_env(vp) == e->env_id && vpage_get_va(vp) == va)
      {
        vpagel_remove_vpage(vp);
        vpage_free(vp);
        break;
      }
    }

    /* free the page if it's not mapped anywhere. */
    Ppage_pp_refcnt_set(pp,Ppage_pp_refcnt_get(pp)-1);
    if (Ppage_pp_refcnt_get(pp) == 0) 
      ppage_free (pp);
    
    Ppage_pp_klock_release(pp);
  }
  *ptep = 0;
  
  tlb_invalidate (va, e->env_pd->envpd_id);
  return 0;
}



/* insert a mapping of a page into an address space */

int
ppage_insert (struct Env *const e, struct Ppage *pp, u_int va, u_int perm) 
  __XOK_SYNC(locks e->env_pd->envpd_spinlock;
                   pp->pp_klock; calls ppage_remove)
{
  Pde *const pd = e->env_pd->envpd_pdir[e->env_pd->envpd_active];
  Pte *ptep;
  struct Vpage *vp;
  int r;

  va &= ~PGMASK;
  vp = vpage_new();
  if (!vp) 
  {
    printf("can't create vpage in ppage_insert\n");
    printf("%d, %d, %d\n", SYSINFO_GET(si_nppages), SYSINFO_GET(si_ndpages), SYSINFO_GET(si_nfreepages));
    return -E_NO_MEM;
  }
  
  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  Ppage_pp_klock_acquire(pp);

  vpage_set_env(vp, e->env_id);
  vpage_set_va(vp, va);
  vpagel_add_vpage(Ppage_pp_vpl_ptr(pp), vp);

  if (perm & PG_W) 
    Ppage_pp_writers_set(pp,Ppage_pp_writers_get(pp)+1);

  Ppage_pp_readers_set(pp,Ppage_pp_readers_get(pp)+1);
  Ppage_pp_refcnt_set(pp,Ppage_pp_refcnt_get(pp)+1);

  if ((r = pde_check_n_alloc (e->env_pd, va)) < 0) 
  {
    
    if (perm & PG_W) 
      Ppage_pp_writers_set(pp,Ppage_pp_writers_get(pp)-1);
    Ppage_pp_readers_set(pp,Ppage_pp_readers_get(pp)-1);
    Ppage_pp_refcnt_set(pp,Ppage_pp_refcnt_get(pp)-1);
   
    vpagel_remove_vpage(vp);
    vpage_free(vp);

    Ppage_pp_klock_release(pp);
  
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return r;
  }

  ptep = pt_get_ptep (pd, va);
  if (*ptep & PG_P) 
    ppage_remove (e, va);

  *ptep = pp2pa (pp) | perm;
  e->env_pd->envpd_nptes[PDENO (va)]++;

  Ppage_pp_klock_release(pp);
  
  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  return 0;
}



void
ppage_pin (struct Ppage *pp) 
  __XOK_REQ_SYNC(on pp->pp_klock)
{
  INC_FIELD(pp,Ppage,pp_pinned,1); // pp->pp_pinned++;
  assert (Ppage_pp_pinned_get(pp) > 0);
  if (Ppage_pp_pinned_get(pp) == 1) 
    Sysinfo_si_pinnedpages_atomic_inc(si);
}



void
ppage_unpin (struct Ppage *pp) 
  __XOK_REQ_SYNC(on pp->pp_klock)
{
  assert (Ppage_pp_pinned_get(pp) > 0);
  DEC_FIELD(pp,Ppage,pp_pinned,1); // pp->pp_pinned--;
  if (Ppage_pp_pinned_get(pp) == 0) 
    Sysinfo_si_pinnedpages_atomic_dec(si);
}



void
ppage_acl_zero (struct Ppage *pp) 
  __XOK_REQ_SYNC(on pp->pp_klock)
{
  acl_init (Ppage_pp_acl_get(pp), PP_ACL_LEN);
}



int
ppage_acl_check (struct Ppage *pp, cap *c, int len, u_char perm)
  __XOK_REQ_SYNC(on pp->pp_klock)
{
  return acl_access(c, Ppage_pp_acl_get(pp), len, perm);
}



/*
 * PMAP SYSTEM CALLS
 *
 */

DEF_ALIAS_FN (sys_self_insert_pte, sys_insert_pte);
int
sys_insert_pte (u_int sn, u_int k, Pte pte, u_int va, u_int ke, int envid)
  __XOK_SYNC(calls pmap_insert_range)
{
  u_int num_completed = 0;
  int ret;

  ret = pmap_insert_range (sn, k, &pte, 1, va, &num_completed, ke, envid, 1);
  return ret;
}


DEF_ALIAS_FN (sys_self_insert_pte_range, sys_insert_pte_range);
int
sys_insert_pte_range (u_int sn, u_int k, Pte * ptes, u_int num, u_int va,
		      u_int *num_completed, u_int ke, int envid)
  __XOK_SYNC(calls pmap_insert_range)
{
  u_int m;
  int ret;

  ptes = trup (ptes);
  m = page_fault_mode;
  page_fault_mode = PFM_PROP;

  ret = pmap_insert_range (sn, k, ptes, num, va, num_completed, ke, envid, 1);

  page_fault_mode = m;
  return (ret);
}


DEF_ALIAS_FN (sys_self_remove_vas, sys_remove_vas);
int
sys_remove_vas (u_int sn, u_int * vas, u_int num, u_int ke, int envid)
  __XOK_SYNC(locks e->env_pd->envpd_spinlock; calls ppage_remove)
{
  struct Env *e;
  u_int m;
  int r;

  if (sn == SYS_self_remove_vas)
    e = curenv;
  else if (!(e = env_access (ke, envid, ACL_W, &r)))
    return r;

  m = page_fault_mode;
  page_fault_mode = PFM_PROP;

  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  for (; num; num--)
    {
      Pte *ptep;
      u_int va = vas[num - 1];

      /* Can't touch PHYSMAP pte's */
      if (va < 0 || va >= UTOP)
      {
        MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
	page_fault_mode = m;
	return -E_INVAL;
      }

      if (!(e->env_pd->envpd_pdir[e->env_pd->envpd_active][PDENO (va)] & PG_P))
      {
        MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
	page_fault_mode = m;
	return -E_INVAL;
      }

      ptep = pt_get_ptep (e->env_pd->envpd_pdir[e->env_pd->envpd_active], va);

      if (*ptep & PG_P)
	  ppage_remove (e, va);
    }

  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  page_fault_mode = m;

  return (0);
}


static int
pmap_insert_common (Pte *new_pte, struct Env *e, u_int va)
  __XOK_SYNC(locks e->env_pd->envpd_spinlock; calls ppage_remove)
{
  Pte *ptep;
  int r;

  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  /* Make sure the page directory for this page table exists. Allocate
     it if it doesn't. */

  if ((r = pde_check_n_alloc (e->env_pd, va)) < 0) 
  {
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return r;
  }
  ptep = pt_get_ptep (e->env_pd->envpd_pdir[e->env_pd->envpd_active], va);

  /* XXX -- this is important--do not remove */
  if (*ptep == *new_pte)
  {
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return 1;
  }

  /* flush the entry from the TLB if we're manipulating the current
     address space */
  if (*ptep & PG_P) {
    ppage_remove (e, va);
    *ptep = 0;
  }

  /* If the PG_P bit isn't set, the processor ignores the contents
       of the pte so the user can put anything in they want. */
  if (!(*new_pte & PG_P)) {
    *ptep = *new_pte;
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return 1;
  }
  
  /* Make sure that bits that have to be zero are and that
       the user isn't trying to insert a supervisor pte entry */
  *new_pte &= ~PG_MBZ;
  *new_pte |= PG_U;

  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  return 0;
}


/* The num_completed field allows the function to be interrupted by a page
   fault or to return an error and then restart at the proper location without
   redoing what's already been done. */
int
pmap_insert_range (u_int sn, u_int k, Pte * ptes, u_int total, u_int va,
		   u_int *num_completed, u_int ke, int envid, int userreq)
  __XOK_SYNC(calls pmap_insert_common; 
                locks ppage->pp_klock; calls ppage_reclaim_page; 
	        releases ppage->pp_klock;
	     or calls ppage_alloc;
	     locks ppage->pp_klock; 
	     calls ppage_free; 
	     calls ppage_acl_check;
	     calls ppage_free)
{

  struct Env *e;
  struct Ppage *pp;
  u_int ppn, num;
  u_char perm;
  cap c;
  Pte pte;
  int r;

  if (sn == SYS_self_insert_pte_range || sn == SYS_self_insert_pte)
    e = curenv;
  else if (!(e = env_access (ke, envid, ACL_W, &r)))
  {
    return (r);
  }

  /* Can't touch PHYSMAP pte's */
  if (va < 0 || va + (total - 1) * NBPG >= UTOP)
    return (-E_INVAL);

  /* Get the right ppage and check access */
  if ((r = env_getcap (curenv, k, &c)) < 0)
  {
    return (r);
  }

  for (num = *num_completed; num < total; num++, va += NBPG)
    {
      int new_pg = 0;

      pte = *ptes++;
      r = pmap_insert_common (&pte, e, va);
      if (r < 0) return r;
      if (r > 0) continue;

      if (pte & PG_W)
	perm = ACL_W;
      else
	perm = 0;

      /* two cases: map a specified page or map any free page */

      ppn = pte >> PGSHIFT;
      if (ppn) {

	  /* user specified a particular page to map */

	  if (ppn >= nppage || ppn < 1) return (-E_INVAL);

	  /* make sure the page doesn't hold a buffer (must got through
	   * bc_buffer_map to map those pages). Assuming it doesn't hold a
	   * buffer, see if the page is on the free list and remove it if
	   * it is. 
	   */

	  pp = ppages_get(ppn);

	  Ppage_pp_klock_acquire(pp);
	  
	  if (Ppage_pp_buf_get(pp)) 
	  {
	    Ppage_pp_klock_release(pp);
	    return -E_BUFFER;
	  }

	  if (Ppage_pp_status_get(pp) == PP_FREE)
	    {
	      /* make sure there's no pending dma on this page */

	      if (Ppage_pp_pinned_get(pp)>0) 
	      {
		Ppage_pp_klock_release(pp);
		return (-E_INVAL);
	      }

	      /* pull this page off the free page list */
	      ppage_reclaim_page (pp, PP_USER);
	      new_pg = 1;
	    }
	  
	  Ppage_pp_klock_release(pp);
	}
      else
	{
	  /* user didn't specify a particular page so find one for him */
	  if ((r = ppage_alloc (PP_USER, &pp, userreq)) < 0) return (r);
	  new_pg = 1;
	}

      /* now check access to this page. If it's a new page there's nothing
	 to check, but we have to guard the page with the capability the user
	 called us with. Otherwise, the capability the user passed in must
	 grant access to this page AND the per-page reader/writer limits
	 must not be exceeded. */

      Ppage_pp_klock_acquire(pp);

      if (new_pg)
	{
	  acl_init (Ppage_pp_acl_get(pp), PP_ACL_LEN);
	  
	  if (acl_add (Ppage_pp_acl_get(pp), PP_ACL_LEN, &c, &r)) 
	  {
	    ppage_free(pp);
	    Ppage_pp_klock_release(pp);
	    return r;
	  }
	}

      else if ((r = ppage_acl_check(pp,&c,PP_ACL_LEN,perm)) < 0)
      {
        Ppage_pp_klock_release(pp);
	return r;
      }

      else if (!ppcompat_state (Ppage_pp_readers_get(pp) + 1, pte & PG_W ?
				Ppage_pp_writers_get(pp) + 1 : 
				Ppage_pp_writers_get(pp),
				Ppage_pp_state_ptr(pp)))
      {
        Ppage_pp_klock_release(pp);
	return -E_SHARE;
      }

      /* insert the page into the env's pagetables */

      if ((r = ppage_insert (e, pp, va, pte & PGMASK)) < 0) 
      {
	if (new_pg) ppage_free(pp);
        Ppage_pp_klock_release(pp);
	return r;
      }

      Ppage_pp_klock_release(pp);
      (*num_completed)++;
    }

  return (0);
}



DEF_ALIAS_FN (sys_self_mod_pte_range, sys_mod_pte_range);
int
sys_mod_pte_range (u_int sn, u_int k, Pte add, Pte remove, u_int va,
		   u_int num, u_int ke, int envid)
  __XOK_SYNC(locks e->env_pd->envpd_spinlock;
             locks ppage->pp_klock;
	     calls ppage_acl_check;
	     releases ppage->pp_klock)
{
  struct Env *e;
  u_int ppn;
  u_char perm;
  Pte *ptep;
  cap c;
  int r;

  if (sn == SYS_self_mod_pte_range)
    {
      e = curenv;
      envid = e->env_id;
    }
  else if (!(e = env_access (ke, envid, ACL_W, &r)))
    {
      return (r);
    }

  /* Check pte bits */
  if (add & PG_MBZ)
    {
      return (-E_INVAL);
    }

  if (remove & PG_U)
    {
      return (-E_INVAL);
    }

  perm = 0;
  if (add & PG_W)
    perm |= ACL_W;

  /* Can't touch PHYSMAP pte's */
  if (va < 0 || va + (num - 1) * NBPG >= UTOP)
    {
      return (-E_INVAL);
    }

  /* only care about flags */
  add &= PGMASK;
  remove &= PGMASK;

  /* Get the right ppage and check access */
  if ((r = env_getcap (curenv, k, &c)) < 0)
    {
      return (r);
    }

  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  for (; num; num--, va += NBPG)
    {
      Pte orig_pte;
      struct Ppage *pp;
      int adj;

      /* get existing pte */
      if ((r = pde_check_n_alloc (e->env_pd, va)) < 0)
	{
          MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
	  return r;
	}

      ptep = pt_get_ptep (e->env_pd->envpd_pdir[e->env_pd->envpd_active], va);

      /* Just software?  No problem, you always have permission. */
      if (!(*ptep & PG_P))
	{
	  add &= ~PG_P;
	  *ptep &= ~remove;
	  *ptep |= add;
	  
	  /* punt the page mapping */ 
	  tlb_invalidate (va, e->env_pd->envpd_id);
	  continue;
	}

      /* if not adding or removing flags that the kernel cares about, then we
	 don't need extra checks */
      if (!mmu_has_hardware_flags(add) && !mmu_has_hardware_flags(remove))
	{
	  *ptep &= ~remove;
	  *ptep |= add;
	  
	  /* punt the page mapping */ 
	  tlb_invalidate (va, e->env_pd->envpd_id);
	  continue;
	}

      ppn = *ptep >> PGSHIFT;
      pp = ppages_get(ppn);

      Ppage_pp_klock_acquire(pp);
      
      if ((r = ppage_acl_check (pp, &c, PP_ACL_LEN, perm)) < 0)
	{
	  Ppage_pp_klock_release(pp);
          MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
	  return (r);
	}

      /* update the pte bits */
      orig_pte = *ptep;
      *ptep &= ~remove;
      *ptep |= add;

      adj = 0;
      if ((orig_pte & PG_W) && !(*ptep & PG_W))
	adj = -1;
      if (!(orig_pte & PG_W) && (*ptep & PG_W))
	adj = 1;

      /* punt the page mapping */ 
      tlb_invalidate (va, e->env_pd->envpd_id);

      if (!ppcompat_state (Ppage_pp_readers_get(pp), 
	                   Ppage_pp_writers_get(pp) + adj, 
	                   Ppage_pp_state_ptr(pp)))
	{
	  *ptep = orig_pte;
	  Ppage_pp_klock_release(pp);
          
	  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
	  return -E_SHARE;
	}
      Ppage_pp_writers_set(pp, Ppage_pp_writers_get(pp) + adj);
     
      Ppage_pp_klock_release(pp);
    }
	  
  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  return (0);
}


int
sys_pstate_mod (u_int sn, u_int k, u_int ppn, int r_state, int w_state)
  __XOK_SYNC(locks pp->pp_klock)
{
  cap c;
  struct Ppage *pp;
  struct Pp_state *ps;
  int r;

  /* Get the right ppage and check access */
  if ((r = env_getcap (curenv, k, &c)) < 0)
    return r;
  if (ppn < 1 || ppn >= nppage)
    return -E_INVAL;

  pp = ppages_get(ppn);

  Ppage_pp_klock_acquire(pp);
  
  if ((r = ppage_acl_check (pp, &c, PP_ACL_LEN, 0)) < 0)
  {
    Ppage_pp_klock_release(pp);
    return (r);
  }

  ps = Pp_state_alloc();
  /* make sure the new state is ok */
  Pp_state_ps_readers_set(ps, (r_state == -1 ? 
                              Pp_state_ps_readers_get(Ppage_pp_state_ptr(pp)) 
			      : r_state));
  Pp_state_ps_writers_set(ps, (w_state == -1 ? 
                              Pp_state_ps_writers_get(Ppage_pp_state_ptr(pp)) 
			      : w_state));
  
  if (!ppcompat_state (Ppage_pp_readers_get(pp), 
                       Ppage_pp_writers_get(pp), ps))
  {
    Pp_state_free(ps);
    Ppage_pp_klock_release(pp);
    return -E_SHARE;
  }
  
  Ppage_pp_state_copyin(pp, ps);
  Pp_state_free(ps);
    
  Ppage_pp_klock_release(pp);
  return 0;
}


int
sys_read_pte (u_int sn, u_int va, u_int ke, int envid, int *error)
  __XOK_SYNC(locks e->env_pd->envpd_spinlock)
{
  struct Env *e;
  Pde pde;
  Pte *pt;
  int r;

  if (!(e = env_access (ke, envid, ACL_R, &r))) 
  {
    copyout(&r, error, sizeof(r));
    return (0);
  }

  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  pde = e->env_pd->envpd_pdir[e->env_pd->envpd_active][PDENO (va)];
  if (!(pde & PG_P)) 
  {
    r = -E_NOT_FOUND;
    copyout(&r, error, sizeof(r));
    
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return (0);
  }
  pt = ptov (pde & ~PGMASK);
  r = pt[PTENO(va)];

  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  return r;
}



/* Free page table with no valid translations.  e->env_pd->envpd_nptes[pdeno]
 * actually keeps track of how many translations are in a table, so in
 * theory we could just do this automatically when there are no valid
 * translations in a page table.  However, the user may care about
 * whatever garbage is in in 31 bits of a non-present PTE, or may want
 * to be able to address certain ranges of the virtual page table.
 * Therefore, let the user manually free page tables with this system
 * call.
 */
DEF_ALIAS_FN (sys_self_pt_free, sys_pt_free);
int
sys_pt_free (u_int sn, u_int va, u_int ke, int envid)
  __XOK_SYNC(locks e->env_pd->envpd_spinlock; locks pp_klock; calls ppage_free)
{
  struct Env *e;
  Pde *pdep;
  Pte *pt;
  int r;

  if (sn == SYS_self_pt_free)
    e = curenv;
  else if (!(e = env_access (ke, envid, ACL_W, &r)))
    return (r);

  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  /* Can't free PHYSMAP page tables or those with translations */
  if (va >= UTOP || e->env_pd->envpd_nptes[PDENO (va)] > 0)
  {
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return (-E_INVAL);
  }

#ifdef ASH_ENABLE
  /* Can't free the ASH page table */
  if ((va & ~PDMASK) == e->env_ashuva)
  {
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return (-E_INVAL);
  }
#endif

  pdep = &e->env_pd->envpd_pdir[e->env_pd->envpd_active][PDENO (va)];
  if (!(*pdep & PG_P))
  {
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return (0);
  }

  /* remove from every page directory */
  for(r=0; r<NR_CPUS; r++)
  {
    if (e->env_pd->envpd_cr3[r] != 0)
    {
      struct Ppage *pp; 
      
      pdep = &e->env_pd->envpd_pdir[r][PDENO (va)];
      pt = ptov (*pdep & ~PGMASK);
      pp = kva2pp((u_long)pt);
      
      Ppage_pp_klock_acquire(pp);
      ppage_free (pp);
      Ppage_pp_klock_release(pp);

      *pdep = 0;
    }
  }

  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  return (0);
}



int
sys_pgacl_read (u_int sn, u_int pa, cap * res, u_short res_len)
  __XOK_SYNC(locks pp->pp_klock; calls acl_read)
{
  struct Ppage *pp;
  int r;

  pa >>= PGSHIFT;
  if (pa >= nppage)
    return (-E_INVAL);
  pp = ppages_get(pa);

  Ppage_pp_klock_acquire(pp);
  r = acl_read (Ppage_pp_acl_get(pp), PP_ACL_LEN, res, res_len);
  Ppage_pp_klock_release(pp);

  return r;
}



int
sys_pgacl_setsize (u_int sn, u_int k, u_int pa, u_short size)
  __XOK_SYNC(locks pp->pp_klock; calls ppage_acl_check; calls acl_setsize)
{
  struct Ppage *pp;
  cap c;
  int r;

  if ((r = env_getcap (curenv, k, &c)) < 0)
    return (r);
  pa >>= PGSHIFT;
  if (pa >= nppage)
    return (-E_INVAL);
  pp = ppages_get(pa);

  Ppage_pp_klock_acquire(pp);
  if ((r = ppage_acl_check (pp, &c, PP_ACL_LEN, ACL_MOD)) < 0)
  {
    Ppage_pp_klock_release(pp);
    return (r);
  }
  r = acl_setsize (Ppage_pp_acl_get(pp), PP_ACL_LEN, size);
  Ppage_pp_klock_release(pp);

  return r;
}


int
sys_pgacl_mod (u_int sn, u_int k, u_int pa, u_short pos, cap * n)
  __XOK_SYNC(locks pp->pp_klock; calls acl_mod)
{
  struct Ppage *pp;
  cap c;
  int r;

  if ((r = env_getcap (curenv, k, &c)) < 0)
    return (r);
  pa >>= PGSHIFT;
  if (pa >= nppage)
    return (-E_INVAL);
  pp = ppages_get(pa);

  Ppage_pp_klock_acquire(pp);
  r = acl_mod (&c, Ppage_pp_acl_get(pp), PP_ACL_LEN, pos, n);
  Ppage_pp_klock_release(pp);
  
  return r;
}


int
sys_revoke_pages (u_int sn, u_int num)
  __XOK_SYNC(locks ppage->pp_klock; releases ppage->pp_klock; 
             localizes each e we are revoking pages from)
{
  int ppn;
  struct Ppage *pp;
  struct Env *e;
  int eid;
  int cnt;
  int r;

  while (num)
    {
      int status;

      /* we pick an env to revoke from by picking a random page and asking
	 an owning env to give a page back. this way revocation
	 request are proportional to memory owned. */

      do
	{
repeat:
	  ppn = rand () % nppage;
	  pp = ppages_get(ppn);
	  if (Ppage_pp_klock_try(pp) != 0)
	    goto repeat;
          status = Ppage_pp_status_get(pp);
	  if (status != PP_USER)
	    Ppage_pp_klock_release(pp);
	}
      while (status != PP_USER);

      /* get an env that's mapping this page */
      /* XXX -- always picks the first env for now */

      eid = vpage_get_env(vpagel_get_first(Ppage_pp_vpl_ptr(pp)));
      Ppage_pp_klock_release(pp);

      e = env_id2env (eid, &r);
      if (!e)
	{
	  warn ("sys_revoke_pages: couldn't find env_if %x  listed under a "
		"page's vpl", eid);
	  return r;
	}

      if (!e->env_u)
	continue;

#ifdef __SMP__
      if (env_localize(e) < 0)
	continue;
#endif

      /* how many pages to revoke */
      cnt = num > PP_REVOCATION_INCREMENT ? PP_REVOCATION_INCREMENT : num;
      num -= cnt;

      /* notify the env that it owes us some pages */
      e->env_u->u_revoked_pages = cnt;

#ifdef __SMP__
      env_release(e);
#endif
    }

  return 0;
}


void
page_fault_handler (u_int trapno, u_int errcode)
  __XOK_SYNC(curenv is already localized)
{
  void fault_trampoline (int trap, int errcode);
  tfp tf;
  register u_int va;
  u_int *xsp;

  tfp_set (tf, errcode, tf_edi);

#ifdef __HOST__
  if (tf->tf_cs < GD_NULLS*8 && curenv->handler) {
      asm volatile ("movl %0, %%eax\n"
		    "leave\n"
		    "jmp %%eax" :: "g" ((u_int)&fault_trampoline));
  }
#endif

  asm volatile ("movl %%cr2,%0":"=r" (va):);

  /* Always propagate if the fault from user mode */
  if (errcode & FEC_U)
    {
      page_fault_mode = PFM_PROP;
    }
  else if (page_fault_mode == PFM_VCOPY)
    {
      printf ("%%%% [0x%x] VCOPY fault (code 0x%x) for VA 0x%x (env 0x%x)\n"
	      "   eip = 0x%x, cs = 0x%x, eflags = 0x%x, cpu = %d\n",
	      trapno, 0xffff & errcode, va, curenv ? curenv->env_id : -1,
	      tf->tf_eip, 0xffff & tf->tf_cs, tf->tf_eflags, get_cpu_id());
      tf->tf_ecx = 0;		/* Stop the faulting bcopy */
      page_fault_vcopy = 1;
      return;
    }

#if 0				/* DEBUGGING */
  printf ("%%%% [0x%x] Page fault (code 0x%x) for VA 0x%x (env 0x%x)"
	  "   eip = 0x%x, cs = 0x%x, eflags = 0x%x, cpu = %d\n",
	  trapno, 0xffff & errcode, va, curenv ? curenv->env_id : -1,
	  tf->tf_eip, 0xffff & tf->tf_cs, tf->tf_eflags, get_cpu_id());
  /* Kernel traps are taken on same stack, so %sp in trapframe only
   * when trap is from user mode.  */
  if (tf->tf_cs & 0x3)
    printf ("   esp = 0x%x, ss = 0x%x", tf->tf_esp,
	    0xffff & tf->tf_ss);
  delay (10000000);
#endif

  if (page_fault_mode == PFM_KILL || ash_va)
    {
      printf ("%%%% Page fault in %s!  Nailing the little bugger.\n",
	      ash_va ? "ash" : "kill mode");
      printf ("%%%% [0x%x] Page fault (code 0x%x) for VA 0x%x (env 0x%x)\n"
	      "   eip = 0x%x, cs = 0x%x, eflags = 0x%x, cpu = %d\n",
	      trapno, 0xffff & errcode, va, curenv ? curenv->env_id : -1,
	      tf->tf_eip, 0xffff & tf->tf_cs, tf->tf_eflags, get_cpu_id());

      page_fault_mode = PFM_NONE;
      kill_env (curenv);
    }

  if (page_fault_mode != PFM_PROP)
    {
      printf ("%%%% [0x%x] Page fault (code 0x%x) for VA 0x%x (env 0x%x)\n"
	      "   eip = 0x%x, cs = 0x%x, eflags = 0x%x, cpu = %d\n",
	      trapno, 0xffff & errcode, va, curenv ? curenv->env_id : -1,
	      tf->tf_eip, 0xffff & tf->tf_cs, tf->tf_eflags, get_cpu_id());
      /* Kernel traps are taken on same stack, so %sp in trapframe only
       * when trap is from user mode.  */
      if (errcode & FEC_U)
	printf ("   esp = 0x%x, ss = 0x%x\n", tf->tf_esp,
		0xffff & tf->tf_ss);
      panic ("Unexpected page fault (in mode %d)", page_fault_mode);
    }
  
  else if (curenv->env_id == 0)
  {
    printf ("%%%% [0x%x] Page fault (code 0x%x) for VA 0x%x (env 0x%x)\n"
	    "   eip = 0x%x, cs = 0x%x, eflags = 0x%x, cpu = %d\n",
	    trapno, 0xffff & errcode, va, curenv ? curenv->env_id : -1,
	    tf->tf_eip, 0xffff & tf->tf_cs, tf->tf_eflags, get_cpu_id());
    /* Kernel traps are taken on same stack, so %sp in trapframe only
     * when trap is from user mode.  */
    if (errcode & FEC_U) 
      printf ("   esp = 0x%x, ss = 0x%x\n", tf->tf_esp, 0xffff & tf->tf_ss);
    panic ("Unexpected page fault in idle environment");
  }


  /* restart system call (2 byte trap) */
  if (!(errcode & FEC_U))
    {
      /* restart system call (2 byte trap) */
      utf->tf_eip -= 2;
      /* call optional system call cleanup routine */
      if (syscall_pfcleanup) syscall_pfcleanup(va, errcode, trapno);
    }

  assert(curenv != NULL);
  assert(curenv->env_u != NULL);

  /* Call user trap handler */
  page_fault_mode = PFM_KILL;
  /* 12 here is pretty arbitrary... it is simply a bit of room for kernel and
   * user level handler to work with. since we will restore the esp
   * explicitly, this can be any number as long as the apps and kernel agree
   * with each other */
  xsp = trup ((u_int *) ((UAREA.u_xsp) ? UAREA.u_xsp : utf->tf_esp) - 12);

  xsp[0] = va;
  xsp[1] = errcode;
  xsp[2] = utf->tf_eflags;
  if (UAREA.u_ss_pfaults == 0)
    utf->tf_eflags &= ~FL_TF;
  xsp[3] = utf->tf_eip;
  xsp[4] = utf->tf_esp;
  xsp[5] = UAREA.u_xsp;
  UAREA.u_xsp = 0;
  page_fault_mode = PFM_NONE;
  if (!(utf->tf_eip = UAREA.u_entfault))
    {
#if 1				/* DEBUGGING */
      printf ("%%%% [0x%x] Page fault (code 0x%x) for VA 0x%x (env 0x%x)"
	      "   eip = 0x%x, cs = 0x%x, eflags = 0x%x, cpu = %d\n",
	      trapno, 0xffff & errcode, va, curenv ? curenv->env_id : -1,
	      tf->tf_eip, 0xffff & tf->tf_cs, tf->tf_eflags, get_cpu_id());
      /* Kernel traps are taken on same stack, so %sp in trapframe only
       * when trap is from user mode.  */
      if (tf->tf_cs & 0x3)
	printf ("   esp = 0x%x, ss = 0x%x", tf->tf_esp,
		0xffff & tf->tf_ss);
      /*  delay (10000000); */
#endif
      printf ("%%%% Bad fault handler address.  Nailing the little bugger.\n");
      page_fault_mode = PFM_NONE;


      kill_env (curenv);
    }
  utf->tf_esp = (u_int) xsp;
  env_upcall ();
}


#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

