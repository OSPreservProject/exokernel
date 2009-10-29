
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

#ifndef _XOK_VPAGE_H_
#define _XOK_VPAGE_H_

#include <xok/queue.h>

LIST_HEAD(Vpage_list, Vpage);

struct Vpage {
  u_int vp_env;              /* Environment number */
  u_int vp_va;               /* Virtual address of mapping */
  LIST_ENTRY(Vpage) vp_link; /* Link through all mappings of a page */
};

typedef struct Vpage_list Vpage_list_t;

#ifdef KERNEL

#include <xok/malloc.h>

static inline void
vpagel_init(Vpage_list_t *vpl)
{
  LIST_INIT(vpl);
}

static inline struct Vpage*
vpagel_get_first(Vpage_list_t *vpl)
{
  return vpl->lh_first;
}

static inline struct Vpage*
vpagel_get_next(struct Vpage *vp)
{
  return vp->vp_link.le_next;
}

static inline void
vpagel_remove_vpage(struct Vpage *vp)
{
  LIST_REMOVE(vp, vp_link);
}

static inline void
vpagel_add_vpage(Vpage_list_t *vpl, struct Vpage *vp)
{
  LIST_INSERT_HEAD(vpl, vp, vp_link);
}

static inline struct Vpage*
vpage_new(void)
{
  return malloc(sizeof(struct Vpage));
}

static inline void
vpage_free(struct Vpage *vp)
{
  free(vp);
}

static inline u_int
vpage_get_env(struct Vpage *vp)
{
  return vp->vp_env;
}

static inline u_int
vpage_get_va(struct Vpage *vp)
{
  return vp->vp_va;
}

static inline void
vpage_set_env(struct Vpage *vp, u_int env)
{
  vp->vp_env = env;
}

static inline void
vpage_set_va(struct Vpage *vp, u_int va)
{
  vp->vp_va = va;
}

#endif /* KERNEL */

#endif /* _XOK_VPAGE_H_ */
