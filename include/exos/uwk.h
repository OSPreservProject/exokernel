
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


#ifndef __UWK_H_
#define __UWK_H_

#include <sys/types.h>
#include <xok/wk.h>

/* WKTAGs dedicated to the uwk.c functionality.  Don't use them! */

#define UWK_TAG_MIN	0x30000000
#define UWK_TAG_MAX	0x3000FFFF

/***************** prototypes for functions provided by uwk.c *****************/

/* Wrappers for waiting for an arbitrary predicate to evaluate to true.       */
/* These should always be used in lieu of explicitly downloading and sleeping */

void wk_waitfor_pred_directed (struct wk_term *t, int sz, int envid);
#define wk_waitfor_pred(t, sz)	wk_waitfor_pred_directed((t), (sz), -1)

/* Functions for registering and deregistering extra predicate components    */
/* that should be attached to all downloaded WK predicates.  The "construct" */
/* routine will be called at predicate construction time, and the "callback" */
/* routine will be called if the provided predicate component wakes the      */
/* sleeper.  Both will be given "param" as a parameter.  "Maxlen" must be    */
/* the upper bound on the predicate returned by "construct".                 */

int wk_register_extra (int (*construct)(struct wk_term *,u_quad_t),
		       void (*callback)(u_quad_t), u_quad_t param, int maxlen);
int wk_deregister_extra (int (*construct)(struct wk_term *,u_quad_t),
			 u_quad_t param);

/* Generic functions for waiting for trivial occurrances */

void wk_waitfor_value (int *addr, int value, u_int cap);
void wk_waitfor_value_neq (int *addr, int value, u_int cap);
void wk_waitfor_value_lt (int *addr, int value, u_int cap);
void wk_waitfor_value_gt (int *addr, int value, u_int cap);
void wk_waitfor_usecs (u_int usecs);
void wk_dump(struct wk_term* t, int sz);

/* generic constructors for predicate components */

#define UWK_MKCMP_EQ_PRED_SIZE 3
int wk_mkcmp_eq_pred (struct wk_term *t, int *addr, int value, u_int cap);
#define UWK_MKCMP_NEQ_PRED_SIZE 3
int wk_mkcmp_neq_pred (struct wk_term *t, int *addr, int value, u_int cap);
#define UWK_MKENVDEATH_PRED_SIZE 10
int wk_mkenvdeath_pred (struct wk_term *t);
#define UWK_MKSLEEP_PRED_SIZE 10
int wk_mksleep_pred (struct wk_term *t, u_quad_t ticks);
int wk_mkusleep_pred (struct wk_term *t, unsigned int usecs);
#define UWK_MKSIG_PRED_SIZE 3
int wk_mksig_pred (struct wk_term *t);		/* provided by os/signal.c */

void wk_waitfor_freepages ();
#define UWK_MKREV_PRED_SIZE 3
int wk_mkrev_pred (struct wk_term *);

#define UWK_MKTRUE_PRED_SIZE 3
int wk_mktrue_pred(struct wk_term *t);
#define UWK_MKFALSE_PRED_SIZE 3
int wk_mkfalse_pred(struct wk_term *t);

#endif  /* __UWK_H_ */

