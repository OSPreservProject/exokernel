
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

#include <exos/ipcdemux.h>
#include <exos/kprintf.h>
#include <exos/mallocs.h>
#include <exos/pager.h>
#include <unistd.h>

/* total number of pages controlled by registered pagers (according to the
   pagers themselves */
u_int _exos_pager_total_pages=0;
static u_int inited=0;

LIST_HEAD(, _exos_pager) pagers;

/* register a function to be called by the top revoker when pages are needed */
struct _exos_pager *_exos_pager_register(_exos_pager_revoker func) {
  struct _exos_pager *p;

  if (!inited) return NULL;
  p = (struct _exos_pager*)__malloc(sizeof(*p));
  if (p == NULL) return NULL;
  p->total = p->freeable = 0;
  p->func = func;
  LIST_INSERT_HEAD(&pagers, p, list);
  return p;
}

/* unregister a pager */
void _exos_pager_unregister(struct _exos_pager *p) {
  if (!inited) return;
  LIST_REMOVE(p, list);
  __free(p);
}

/* a request from somewhere for num pages to be freed
   returns the number actually freed */
static u_int _exos_pager_top_revoker(u_int num, u_int priority) {
  u_int numfreed = 0;
  struct _exos_pager *curpager=NULL, *firstchecked;

  /* if not inited or no pagers registered... */
  if (!inited || !pagers.lh_first) return 0;
  /* check all registered pagers */
  /* XXX - come up with better alg - use freeable pages info */ 
  if (!curpager) curpager = pagers.lh_first;
  firstchecked = curpager;
  while (numfreed < num) {
    numfreed += curpager->func(num-numfreed, priority);
    curpager = curpager->list.le_next;
    if (!curpager) curpager = pagers.lh_first;
    if (curpager == firstchecked) break;
  }

  return numfreed;
}

/* called from the prologue when it has detected that the kernel has
   requested us to give up some pages */
void _do_revocation () {
  u_int request = UAREA.u_revoked_pages;

  UAREA.u_revoked_pages = 0;
  if (_exos_pager_top_revoker(request, EPAGER_PRIORITY_URGENT) < request) {
    /* XXX - print to a log somewhere instead? */
    kprintf("Pid %d unable to satisfy request for %d pages from kernel\n",
	    getpid(), request);
  }
}

/* ipc page request handler - returns number of pages freed */
static int page_req(int code, int request, int priority, u_int caller) {
  if (code != IPC_PAGE_REQ) return 0;
  return _exos_pager_top_revoker(request, priority);
}

/* initialize pager list and register ipc page request handler */
void _exos_pager_init() {
  LIST_INIT(&pagers);
  ipcdemux_register(IPC_PAGE_REQ, page_req, 4);
  inited = 1;
}
