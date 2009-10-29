
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

#include <exos/critical.h>
#include <exos/ipcdemux.h>
#include <exos/pager.h>
#include <exos/page_replacement.h>
#include <exos/ubc.h>
#include <exos/uwk.h>
#include <exos/vm.h>
#include <stdlib.h>
#include <string.h>
#include <xok/sysinfo.h>
#include <xok/kerrno.h>
#include <xok/kqueue.h>
#include <xok/pmap.h>
#include <xok/sys_ucall.h>

#if 0

/*
 * Check if PHYSICAL page ppn is dirty. 
 *
 * ppn names the page to cehck
 * cap is a capability that gives read-only access to the env's mapping ppn
 * count is the max size of envs, ptes, vas, and hence the max number of
 * env's that will be checked for mapping ppn.
 *
 * On return, envs is filled in with the envs mapping ppn. vas[i] has the virtual
 * address envs[i] has ppn mapped at and ptes[i] has the full pte of the mapping.
 * 
 * This function returns INSIDE A CRITICAL SECTION, so you must call
 * ExitCritical soon after this function returns.
 *
 * XXX - The above behavior may cause SMP to not work, but not sure if this
 * function would ever be used again, and how it will be used.
 */

int ppn_is_dirty (u_int ppn, int count, u_int envs[], u_int ptes[],
		  u_int vas[], u_int cap) {
  int i, j;
  struct Vppage *vp;

  EnterCritical ();

  /* figure out who's mapping this ppn and where they're mapping it */
  for (i = 0, vp = KLIST_UPTR (&__ppages[ppn].pp_vpl.lh_first, UVP);
       vp; vp = KLIST_UPTR (&vp->vp_link.le_next, UVP), i++) {
    assert (i < count);
    envs[i] = vp->vp_env;
    vas[i] = vp->vp_va;
  }

  /* and actualy read all the pte's */
  if (sys_read_pten (envs, vas, caps, ptes) < 0)
    panic ("ppn_is_dirty: could not read ptes");

  /* and now check dirty bits */
  for (j = 0; j < i; j++)
    if (ptes[j] & PG_D)
      return 1;

  return 0;
}

#endif


int __vm_count_refs (unsigned va)
{
   uint ppn;

   if ((vpd[PDENO (va)] & PG_P) != PG_P) {
      return (0);
   }

   if ((vpt[PGNO(va)] & PG_P) != PG_P) {
      return (0);
   }

   ppn = PGNO (vpt[PGNO(va)]);
   return (__ppages[ppn].pp_readers);
}


/* 
 * Map a group of pages into another process.
 *
 * All physical pages that are mapped into virtual memory between addresses
 * va and va+len will be mapped into envid starting at base virtual
 * address newva. The physical pages in this range must be guarded by kp.
 *
 */

int __vm_share_region (unsigned va, unsigned len, u_int kp, u_int ke,
		       int envid, unsigned newva) {
  u_int num_completed = 0;
  if (len == 0) return 0;
  if (_exos_insert_pte_range(kp, &vpt[PGNO(va)],
			     PGNO(PGROUNDUP(va+len)) - PGNO(va), newva,
			     &num_completed, ke, envid, 0, NULL) < 0)
    return -1;

  return 0;
}

/*
 * Allocate pages for a virtual range.
 *
 * Allocates pages for the range va to va+len. Any previous mappings
 * are removed. The new pages are protected by kp. prot_flags is an or'ing
 * of PG_P, PG_U, PG_COW, etc.
 *
 */
int __vm_alloc_region (unsigned va, unsigned len, u_int kp, u_int prot_flags) {
  Pte pte = (prot_flags & PGMASK) & ~PG_MBZ;
  u_int num_pages = PGNO(PGROUNDUP(va+len)) - PGNO(va);
  Pte *ptes = alloca(num_pages * sizeof(Pte));
  int i;
  u_int num_completed;

  if (len == 0) return 0;

  if (!ptes) return -1;

  for (i=num_pages-1; i >= 0; i--)
    ptes[i] = pte;

  num_completed = 0;
  if (_exos_self_insert_pte_range (kp, ptes, num_pages, va, &num_completed,
				   0, NULL) < 0)
    return -1;

  return 0;
}

/*
 * Unmap pages in a virtual range.
 *
 * Removes all mappings in a specified range of virtual addresses. It is not
 * an error if any or all of these pages are not mapped.
 *
 */
int __vm_free_region (unsigned va, unsigned len, u_int kp) {
  u_int num_pages = PGNO(PGROUNDUP(va+len)) - PGNO(va);
  Pte *ptes = alloca(num_pages * sizeof(Pte));
  u_int num_completed;

  if (len == 0) return 0;

  if (!ptes) return -1;

  bzero(ptes, num_pages * sizeof(Pte));

  num_completed = 0;
  if (_exos_self_insert_pte_range (kp, ptes, num_pages, va, &num_completed,
				   0, NULL) < 0)
    return -1;

  return 0;
}


int __vm_remap_region (u_int oldva, u_int newva, u_int len, u_int kp) {
  u_int num_pages = PGNO(PGROUNDUP(oldva+len)) - PGNO(oldva);
  Pte *ptes = alloca(num_pages * sizeof(Pte));
  int i;
  u_int num_completed;

  if ((oldva & PGMASK) != (newva & PGMASK)) {
     return (-1);
  }

  if (len == 0) return 0;

  if (!ptes) return -1;

  for (i=num_pages-1; i >= 0; i--) {
     ptes[i] = vpt[PGNO(oldva+(i*NBPG))];
  }

  num_completed = 0;
  if (_exos_self_insert_pte_range (kp, ptes, num_pages, newva, &num_completed,
				   0, NULL) < 0) {
     return -1;
  }

  return 0;
}


#if 0
int __vm_double_map_region (u_int va1, u_int va2, int envid, u_int k1,
			    u_int k2, int sz) {
  u_int va;

  for (va = 0; va < sz; va += NBPG) {
    if (sys_self_insert_pte (k1, PG_U|PG_W|PG_P, va1+va) < 0) {
      return -1;
    }
    if (sys_insert_pte (k2, vpt[(va1+va) >> PGHIFT], va2+va) < 0) {
      return -1;
    }
  }
  return 0;
}


#endif

int _exos_self_unmap_page(u_int k, u_int va) {
  return sys_self_insert_pte(k, 0, va);
}

/* wait for some pages to become free */
static int policy(u_int numpages) {
  u_int numfreed = 0;
  int starte, cure;

  starte = random() % NENV;
  cure = starte;
  do {
    if (__envs[cure].env_status == ENV_OK) {
      int p = 0, q;

      /* XXX - ask for a page */
      //      yield(-1);
      //      sys_bc_flush64(BC_SYNC_ANY, 0, BC_SYNC_ANY, 0);
      q = _ipcout(__envs[cure].env_id, IPC_PAGE_REQ, 1,
		  EPAGER_PRIORITY_STANDARD, 0, 0, 0, 0, &p,
		  IPC_FL_OUT_NONBLOCK | IPC_FL_RET_NONBLOCK, 0);
      if (q == IPC_RET_OK && p > 0) numfreed += p;
    }
    cure = (cure + 1) % NENV;
  } while (cure != starte && numfreed < numpages &&
	   (__sysinfo.si_nfreepages - __sysinfo.si_nfreebufpages) <=
	   256 + numpages);

  if (numfreed >= numpages) return numfreed;

  return numpages;
}

static int _exos_self_insert_pte_wait (u_int k, Pte pte, u_int va,
				       u_int flags, struct Xn_name *pxn) {
  int r;
  u_int ppn;
  struct Ppage *pp;

  /* no need to handle possible retries if we're mapping a buffer since
     we know the memory exists (barring race conditions which have
     to be dealt with at a higher level). */

  if (pte & PG_P) {
    ppn = pte >> PGSHIFT;
    pp = &__ppages[ppn];
    if (pp->pp_buf) {
      assert (pxn);
      return sys_self_bc_buffer_map (pxn, k, pte, va);
    }
  }
  
again:

  /* not reached if pte points to a page holding a buffer */

  r = sys_self_insert_pte(k, pte, va);
  if (r == -E_NO_MEM && PGNO(pte) == 0 && (pte & PG_P)) {
    policy(1);
    goto again;
  }

  return r;
}

int _exos_self_insert_pte(u_int k, Pte pte, u_int va, u_int flags, 
			  struct Xn_name *pxn) {
  u_int ppn;
  struct Ppage *pp;

  if (pte == 0) return _exos_self_unmap_page(k, va);

  if (flags & ESIP_URGENT) {
    return __remap_reserved_page(va, pte);
  }

  if (flags & ESIP_DONTPAGE) {
    if (flags & ESIP_DONTWAIT) {
      if (pte & PG_P) {
	ppn = pte >> PGSHIFT;
	pp = &__ppages[ppn];
	if (pp->pp_buf) {
	  assert (pxn);
	  return sys_self_bc_buffer_map (pxn, k, pte, va);
	}
      }

      /* not reached if pte points to a page holding a buffer */

      return sys_self_insert_pte(k, pte, va);
    } else {
      return _exos_self_insert_pte_wait(k, pte, va, flags, pxn);
    }
  } else {
    //    return _exos_paging_self_insert_pte(k, pte, va, flags);
    return _exos_self_insert_pte_wait(k, pte, va, flags, pxn);
  }
}

static int
_exos_insert_pte_wait (u_int k, Pte pte, u_int va, u_int ke, int envid,
		       u_int flags, struct Xn_name *pxn)
{
  int r;

  /* XXX - doesn't deal with buffer pages */
  
again:
  r = sys_insert_pte(k, pte, va, ke, envid);
  if (r == -E_NO_MEM && PGNO(pte) == 0 && (pte & PG_P)) {
    policy(1);
    goto again;
  }

  return r;
}

int
_exos_insert_pte(u_int k, Pte pte, u_int va, u_int ke, int envid, u_int flags,
		 struct Xn_name *pxn)
{
  /* XXX - doesn't deal with buffer pages */
  
  if (flags & ESIP_DONTPAGE) {
    if (flags & ESIP_DONTWAIT) {
      return sys_insert_pte(k, pte, va, ke, envid);
    } else {
      return _exos_insert_pte_wait(k, pte, va, ke, envid, flags, pxn);
    }
  } else {
    //    return _exos_paging_self_insert_pte(k, pte, va, flags);
    return _exos_insert_pte_wait(k, pte, va, ke, envid, flags, pxn);
  }
}

static int
_exos_self_insert_pte_range_wait (u_int k, Pte *ptes, u_int num, u_int va,
				  u_int *num_completed, u_int flags,
				  struct Xn_name *pxn)
{
  int r;

  /* XXX - range syscall doesn't take pxn's */
  
again:
  r = sys_self_insert_pte_range(k, ptes, num, va, num_completed);
  if (r == -E_NO_MEM && PGNO(ptes[*num_completed]) == 0 &&
      (ptes[*num_completed] & PG_P)) {
    policy(1);
    goto again;
  }

  return r;
}

int
_exos_self_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
			    u_int *num_completed, u_int flags,
			    struct Xn_name *pxn)
{
  if (flags & ESIP_DONTPAGE) {
    if (flags & ESIP_DONTWAIT) {
      /* XXX - range call doesn't take pxn's */
      return sys_self_insert_pte_range(k, ptes, num, va, num_completed);
    } else {
      return _exos_self_insert_pte_range_wait(k, ptes, num, va, num_completed,
					      flags, pxn);
    }
  } else {
    //    return _exos_paging_self_insert_pte(k, pte, va, flags);
    return _exos_self_insert_pte_range_wait(k, ptes, num, va, num_completed,
					    flags, pxn);
  }
}

static int
_exos_insert_pte_range_wait (u_int k, Pte *ptes, u_int num, u_int va,
			     u_int *num_completed, u_int ke, int envid,
			     u_int flags, struct Xn_name *pxn)
{
  int r;

  /* XXX - range syscall doesn't take pxn's */
  
again:
  r = sys_insert_pte_range(k, ptes, num, va, num_completed, ke, envid);
  if (r == -E_NO_MEM && PGNO(ptes[*num_completed]) == 0 &&
      (ptes[*num_completed] & PG_P)) {
    policy(1);
    goto again;
  }

  return r;
}

int
_exos_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
		       u_int *num_completed, u_int ke, int envid, u_int flags,
		       struct Xn_name *pxn)
{
  if (flags & ESIP_DONTPAGE) {
    if (flags & ESIP_DONTWAIT) {
      /* XXX - range call doesn't take pxn's */
      return sys_insert_pte_range(k, ptes, num, va, num_completed, ke, envid);
    } else {
      return _exos_insert_pte_range_wait(k, ptes, num, va, num_completed,
					 ke, envid, flags, pxn);
    }
  } else {
    //    return _exos_paging_self_insert_pte(k, pte, va, flags);
    return _exos_insert_pte_range_wait(k, ptes, num, va, num_completed, ke,
				       envid, flags, pxn);
  }
}

struct bc_entry *_exos_ppn_to_bce(u_int ppn) {
  u32 dev, blk_hi, blk_low;
  u_quad_t blk;

  if (sys_bc_ppn2buf64(ppn, &dev, &blk_hi, &blk_low)) return NULL;

  blk = INT2QUAD(blk_hi, blk_low);
  return __bc_lookup64 (dev, blk);
}

int
_exos_bc_read_and_insert(u_int dev, u_quad_t block, u_int num,
			 int *resptr)
{
  int r;

 again:
  r = sys_bc_read_and_insert(dev, QUAD2INT_HIGH(block), QUAD2INT_LOW(block),
			     num, resptr);
  if (r == -E_NO_MEM) {
    policy(num);
    goto again;
  }

  return r;
}
