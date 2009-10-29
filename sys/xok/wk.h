
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

#ifndef _XOK_WK_H_
#define _XOK_WK_H_

#include <xok/mmu.h>
#include <xok/types.h>
#include <xok/env.h>

/* Non-trivial predicates can be constructed in "sum of products" form.      */
/* The WK_OR op separates and "sums" predicate products.  Trivial predicate  */
/* components (form: <value, value, compare op>) that are immediately        */
/* adjacent are "ANDed" to form a product rule (i.e., they must all evaluate */
/* to true for the "product" to evaluate to true).                           */

/* Operations supported */

enum wk_ops {
  WK_GT,
  WK_LT,
  WK_GTE,
  WK_LTE,
  WK_EQ,
  WK_NEQ,
  WK_OR
};

/* types of wk_terms */
  
enum wk_type {
  WK_VAR,
  WK_IMM,
  WK_TAG,
  WK_OP
};  

/* the basic building block of predicates */

struct wk_term {
  enum wk_type wk_type;
  union {
    struct {
      unsigned *p;
      int k;
    } v;
    unsigned i;
    enum wk_ops op;
    unsigned tag;
  } val;
#define wk_var val.v.p
#define wk_cap val.v.k
#define wk_imm val.i
#define wk_op  val.op 
#define wk_tag val.tag
};

#define WK_MAX_PP 128		/* max number of phys pages pred can ref */

#ifdef KERNEL

void wk_free (struct Env *);

#else /* def(KERNEL) */


static inline u_int wk_mktag (int s, struct wk_term t[], u_int tag) {
  t[s].wk_type = WK_TAG;
  t[s].wk_tag = tag;
  return (++s);
}

static inline u_int wk_mkimm (int s, struct wk_term t[], unsigned imm) {
  t[s].wk_type = WK_IMM;
  t[s].wk_imm = imm;
  return (++s);
}

static inline u_int wk_mkvar (int s, struct wk_term t[], unsigned *var,
			      u_int cap) {
  t[s].wk_type = WK_VAR;
  t[s].wk_var = var;
  t[s].wk_cap = cap;
  return (++s);
}

static inline u_int wk_mkop (int s, struct wk_term t[], enum wk_ops op) {
  t[s].wk_type = WK_OP;
  t[s].wk_op = op;
  return (++s);
}

static inline u_int *wk_phys(va) {
  return (u_int *)((vpt[PGNO((u_int)va)] & ~PGMASK) |
		   (((u_int)va) & PGMASK));
}

#endif /* def(KERNEL) */

#endif /* _XOK_WK_H_ */
