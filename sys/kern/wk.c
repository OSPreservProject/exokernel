
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

/* A predicate is represented as a sum-of-products, that is
   (A1 A2 ... ) OR (B1 B2 ...) OR ...
   where each element in a product (the A?'s and B?'s) are simple
   predicates like v > 10.

   Predicates are represented in memory as an array of wk_term's,
   one term for each immediate, variable, operator, conjunction or
   disjunction. A single product is considered to be a group of
   contiguous wk_term's that are not WK_ORs. The whole mess is
   terminated by a WK_END.  */

#include <vcode/vcode.h>
#include <xok/wk.h>
#include <xok/mmu.h>
#include <xok/sys_proto.h>
#include <xok/kerrno.h>
#include <xok/malloc.h>
#include <xok_include/assert.h>
#include <xok/printf.h>

#ifndef __CAP__
#include <xok/pmapP.h>
#else
#include <xok/pmap.h>
#endif

#define WK_MAX_CODE_BYTES 4096

#define OVERRUN_SAFETY 20
#define OVERRUN_CHECK						\
{								\
  if (v_ip > code + WK_MAX_CODE_BYTES - OVERRUN_SAFETY) {	\
    warn ("wk_compile: out of code space\n");			\
    ret = -E_INVAL;						\
    goto error;							\
  }								\
}

static int next_pp; /* outside function so can be used by cleanup code */
static int wk_compile (struct wk_term *t, int sz, char *code,
		       u_int *pred_pages) {
  int i;
  v_reg_t r1, r2, z, tag;
  v_label_t end_of_term;
  int start_term = 1;
  int op1 = 1;
  cap c;
  struct Ppage *pp;
  u_int ppn;
  int ret = 0;

  next_pp = 0;

  v_lambda ("", "", NULL, 1, code, WK_MAX_CODE_BYTES);
  if (!v_getreg (&r1, V_U, V_TEMP) ||
      !v_getreg (&r2, V_U, V_TEMP) ||
      !v_getreg (&z, V_U, V_TEMP) ||
      !v_getreg (&tag, V_U, V_TEMP))
    panic ("wk_compile: architecture doesn't have enough registers.");

  v_setu (tag, -1);
  v_setu (z, 0);  
  
  for (i = 0; i < sz; i++) {
    if (start_term) {
      end_of_term = v_genlabel ();
      start_term = 0;
    }
    OVERRUN_CHECK;
    switch (t[i].wk_type) {
    case WK_VAR:
      if (next_pp >= WK_MAX_PP-1) {
	warn ("wk_compile: too many pages in predicate\n");
	ret = -E_INVAL;
	goto error;
      }
      if ((ret = env_getcap (curenv, t[i].wk_cap, &c)) < 0) {
	goto error;
      }
      ppn = PGNO((u_int)t[i].wk_var);
      if (!ppn || ppn >= nppage) {
	printf ("at index %d\n", i);
	warn ("wk_compile: invalid physical page\n");
	ret = -E_INVAL;
	goto error;
      }
      pp = ppages_get(ppn);
      switch (Ppage_pp_status_get(pp)) {
      case PP_USER:
	if ((ret = ppage_acl_check(pp,&c,PP_ACL_LEN,0)) < 0) {
	  goto error;
	}
	ppage_pin (pp);
	pred_pages[next_pp++] = ppn;
	break;
      case PP_KERNRO:
	/* user can access pages that each env get's mapped r/o */
	break;
      default:
	printf ("at index %d\n", i);
	warn ("wk_compile: attempt to reference non PP_KERNRO or PP_USER page\n");
	ret = -E_INVAL;
	goto error;
      }
      if (op1) {
	v_ldui (r1, z, (int )ptov (t[i].wk_var));
	op1 = 0;
      } else {
	v_ldui (r2, z, (int )ptov (t[i].wk_var));
	op1 = 1;
      }
      break;
    case WK_IMM:
      if (op1) {
	v_setu (r1, t[i].wk_imm);
	op1 = 0;
      } else {
	v_setu (r2, t[i].wk_imm);
	op1 = 1;
      }
      break;
    case WK_TAG: {
      v_setu (tag, t[i].wk_tag);
      break;
    }
    case WK_OP: {
      switch (t[i].wk_op) {
      case WK_GT: {
	v_bleu (r1, r2, end_of_term); 
	break;
      }
      case WK_GTE: {
	v_bltu (r1, r2, end_of_term); 
	break;
      }
      case WK_LT: {
	v_bgeu (r1, r2, end_of_term);
	break;
      }
      case WK_LTE: {
	v_bgtu (r1, r2, end_of_term); 
	break;
      }
      case WK_EQ: {
	v_bneu (r1, r2, end_of_term);
	break;
      }
      case WK_NEQ: {
	v_bequ (r1, r2, end_of_term);
	break;
      }
      case WK_OR: {
	v_retu (tag);
	v_label (end_of_term);
	start_term = 1; 
	break;
      }
      default: {
	printf ("at index %d\n", i);
	warn ("wk_compile: invalid wk-pred instruction\n");
	ret = -E_INVAL;
	goto error;
      }
      }
      break;
    }
    default:
      printf ("at index %d\n", i);
      warn ("wk_compile: invalid wk-pred type\n");
      ret = -E_INVAL;
      goto error;
    }
  }
      
  /* end the last term */
  OVERRUN_CHECK;
  v_retu (tag);
  v_label (end_of_term);

  v_retui (0);
  v_end (NULL);

error:
  /* have to do this even on error so that our caller can just call
     wk_free to clean memory/ref counts up */
  pred_pages[next_pp] = 0;
  curenv->env_pred_pgs = pred_pages;
  curenv->env_pred = (Spred)code;
  return ret;
}

void wk_free (struct Env *e) {
  int i;
  struct Ppage *pp;

  if (e->env_pred) {
    free (e->env_pred);
    e->env_pred = NULL;
  }
  if (!e->env_pred_pgs)
    return;
  for (i = 0; e->env_pred_pgs[i]; i++) {
    pp = ppages_get(e->env_pred_pgs[i]);
    ppage_unpin (pp);
  }
  free (e->env_pred_pgs);
  e->env_pred_pgs = NULL;
}
 
static char *cleanup_code;
static u_int *cleanup_pred_pages;
static void wk_pfcleanup(u_int va, u_int errcode, u_int trapno) {
  cleanup_pred_pages[next_pp] = 0;
  curenv->env_pred_pgs = cleanup_pred_pages;
  curenv->env_pred = (Spred)cleanup_code;
  wk_free(curenv);
}

int sys_wkpred (u_int sn, struct wk_term *t, int sz) {
  char *code;
  u_int *pred_pages;
  int ret;

  if (curenv->env_pred) {
    wk_free (curenv);
  }
  if (t) {
    int m;

    code = (char *)malloc (WK_MAX_CODE_BYTES);
    if (!code) return -E_NO_MEM;
    pred_pages = (u_int *)malloc (WK_MAX_PP * sizeof (int));
    if (!pred_pages) {
      free (code);
      return -E_NO_MEM;
    }

    m = page_fault_mode;
    page_fault_mode = PFM_PROP;
    t = trup (t);
    syscall_pfcleanup = wk_pfcleanup;

    cleanup_code = code;
    cleanup_pred_pages = pred_pages;
    if ((ret = wk_compile (t, sz, code, pred_pages)) < 0) {
      wk_free (curenv);
      page_fault_mode = m;
      return (ret);
    }

    page_fault_mode = m;
  }

  return (0);
}


#ifdef __CAP__
#include <xok/pmapP.h>
#endif

