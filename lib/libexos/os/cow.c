
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

#include <exos/conf.h>
#include <xok/sys_ucall.h>
#include <exos/process.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <xok/mmu.h>
#include <xok/pmap.h>
#include <string.h>
#include <stdio.h>
#include <exos/critical.h>

static inline Pte uncow_pte (Pte pte) {
  pte &= ~PG_COW;
  if (! (pte & PG_RO)) {
    pte |= PG_W;
  }
  return (pte);
}

#define env2envid(e) ((u_int)((char *)e - (u_int )&__envs[0])/sizeof (__envs[0]))



/* XXX -- race conditions in using pp_refcnt */

void do_cow_fault (u_int va, Pte pte, u_int eip) {
   /* XXX - uhm this only works if there's no aliasing 
     of COW pages */
  u_int orig_ppn = pte >> PGSHIFT;
  u_int add;

  /* ok, if we're the only one refing this page, we don't really need
     to copy it. We don't do this if we're in the fork code cause the
     refcnt's might not be right yet. Basicaly, we batch pte
     insertions so even though someone else might eventually be
     referencing this page they may not have the page mapped yet so
     it'll look like we're the only one using it. */

  dlockputs(__PFAULT_LD,"do_cow_fault get lock ");
  EXOS_LOCK(PPAGE_LOCK);
  dlockputs(__PFAULT_LD,"... got lock\n");
  EnterCritical ();

  if (__ppages[orig_ppn].pp_refcnt == 1 && __ppages[orig_ppn].pp_buf == NULL) {
    if (! (pte & PG_RO))
      add = PG_W;
    else
      add = 0;
    if (sys_self_mod_pte_range (0, add, PG_COW, va, 1) < 0)
      panic ("couldn't update cow pte");
  
    EXOS_UNLOCK(PPAGE_LOCK);
    dlockputs(__PFAULT_LD,"do_cow_fault release lock\n");
    ExitCritical (); 
    return;
  }

  if (_exos_self_insert_pte (0, PG_P|PG_U|PG_W, COW_TEMP_PG,
			     ESIP_DONTPAGE | ESIP_URGENT, NULL) < 0)
    panic ("couldn't alloc new cow page");
  bcopy ((char *)(va & ~PGMASK), (char *)(COW_TEMP_PG), NBPG);
  pte = (vpt[(COW_TEMP_PG) >> PGSHIFT] & ~PGMASK) | (pte & PGMASK);
  if (_exos_self_insert_pte (0, uncow_pte (pte), va, ESIP_DONTPAGE, NULL) < 0)
    panic ("couldn't re-map new cow page");
  if (_exos_self_unmap_page (0, COW_TEMP_PG) < 0)
    panic ("couldn't unmap temp cow page");

  EXOS_UNLOCK(PPAGE_LOCK);
  dlockputs(__PFAULT_LD,"do_cow_fault release lock\n");
  ExitCritical (); 

  __replinish();
}

