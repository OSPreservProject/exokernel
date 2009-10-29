
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

#include <xok/sys_ucall.h>

#include <xok/wk.h>
#include <xok/mmu.h>
#include <exos/process.h>
#include <stdio.h>

int main () {
#if 0
  int i = 2;
  int k = 3;
  int (*f)();
#endif
  int j = 4;
  struct wk_term t[11];

  t[0].wk_type = WK_VAR;
  t[0].wk_var = (unsigned *)wk_phys(&j);
  t[0].wk_cap = 0;
  t[1].wk_type = WK_OP;
  t[1].wk_op = WK_GT;
  t[2].wk_type = WK_IMM;
  t[2].wk_imm = 3;
#if 0
  t[3].wk_type = WK_OP;
  t[3].wk_op = WK_OR;
  t[4].wk_type = WK_IMM;
  t[4].wk_imm = 4;
  t[5].wk_type = WK_OP;
  t[5].wk_op = WK_GT;
  t[6].wk_type = WK_IMM;
  t[6].wk_imm = 3;
  t[7].wk_type = WK_VAR;
  t[7].wk_var = &j;
  t[8].wk_type = WK_OP;
  t[8].wk_op = WK_NEQ;
  t[9].wk_type = WK_VAR;
  t[9].wk_var = &k;
  t[10].wk_type = WK_END;
#endif

  printf ("about to write pred...\n");

printf ("t = %p\n", t);
  if (sys_wkpred (t, 3)) {
    printf ("sys_wkpred returned error\n");
  }

  printf ("sleeping...\n");
  UAREA.u_status = U_SLEEP;
  yield (-1);

  printf ("but I'm back!\n");

  return 0;
}
  







