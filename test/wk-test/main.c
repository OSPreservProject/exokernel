
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

#include <xok/wk.h>
#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <exos/process.h>
#include <stdio.h>
#include <exos/mallocs.h>
#include <assert.h>
#include <xok/pmap.h>
#include <exos/osdecl.h>
#include <xok/sysinfo.h>
#include <unistd.h>

int main () {
#if 0
  int k = 3;
  int (*f)();
#endif
  u_int ppn;
  int *j;
  struct wk_term t[50];
  int s = 0;
  u_int *ticks_lower, *ticks_upper;
  u_int *si_ticks_lower, *si_ticks_upper;
  unsigned long long ticks;
  u_int temp_page;

  temp_page = (u_int)__malloc(NBPG);
  assert (temp_page != 0);
  j = (int *)temp_page+1;
  assert (sys_self_insert_pte (0, PG_P|PG_U|PG_W, temp_page) >= 0);

  si_ticks_lower = (u_int *)&__sysinfo.si_system_ticks;
  si_ticks_upper = si_ticks_lower+1;

  ticks = __sysinfo.si_system_ticks + 500;
  ticks_lower = (u_int *)&ticks;
  ticks_upper = ticks_lower+1;

  /* first term will always be false--just want to force a ref to a user page */
  s = wk_mktag (s, t, 2);
  s = wk_mkvar (s, t, wk_phys (j), 0);
  s = wk_mkimm (s, t, 5);
  s = wk_mkop (s, t, WK_GT);

  s = wk_mkop (s, t, WK_OR);

  s = wk_mktag (s, t, 3);
  s = wk_mkvar (s, t, wk_phys (ticks_upper), 0);
  s = wk_mkvar (s, t, wk_phys (si_ticks_upper), 0);
  s = wk_mkop (s, t, WK_LTE);
  s = wk_mkvar (s, t, wk_phys (ticks_lower), 0);
  s = wk_mkvar (s, t, wk_phys (si_ticks_lower), 0);
  s = wk_mkop (s, t, WK_LTE);

  ppn = vpt[temp_page >> PGSHIFT] >> PGSHIFT;
  *j = 4;

  if (sys_wkpred (t, s)) {
    printf ("sys_wkpred returned error\n");
  }

  assert (__ppages[ppn].pp_refcnt > 1);

  printf ("sleeping...\n");
  UAREA.u_status = U_SLEEP;
  yield (-1);
  printf ("but I'm back with tag %d!\n", UAREA.u_pred_tag);

  __free((void*)temp_page);
  return 0;
}
  







