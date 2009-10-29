
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

#ifndef __PAGER_H__
#define __PAGER_H__

#include <sys/types.h>
#include <xok/env.h>
#include <xok/queue.h>

/* called by startup code */
void _exos_pager_init();

/* a pager's revoker must be of this type
   first argument is number of pages requested and second is priority */
typedef u_int (*_exos_pager_revoker)(u_int, u_int);
#define EPAGER_PRIORITY_URGENT 1
#define EPAGER_PRIORITY_STANDARD 0

struct _exos_pager {
  u_int total;       /* total number of pages under pager's control */
  u_int freeable;    /* approx number of pages immediately freeable */
  _exos_pager_revoker func; /* revoker associated with pager */
  LIST_ENTRY(_exos_pager) list;
};

/* register a function to be called by the top revoker when pages are needed */
struct _exos_pager *_exos_pager_register(_exos_pager_revoker func);

/* unregister a pager */
void _exos_pager_unregister(struct _exos_pager *p);

extern u_int _exos_pager_total_pages;

static inline void _exos_pager_increase_total(struct _exos_pager *p, int num) {
  p->total += num;
  _exos_pager_total_pages += num;
}

static inline void _exos_pager_increase_freeable(struct _exos_pager *p,
						 int num) {
  p->freeable += num;
  UAREA.u_freeable_pages += num;
}

static inline void _exos_pager_decrease_total(struct _exos_pager *p, int num) {
  p->total -= num;
  _exos_pager_total_pages -= num;
}

static inline void _exos_pager_decrease_freeable(struct _exos_pager *p,
						 int num) {
  p->freeable -= num;
  UAREA.u_freeable_pages -= num;
}

#endif
