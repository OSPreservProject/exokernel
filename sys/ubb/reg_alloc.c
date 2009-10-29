
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

#ifdef EXOPC

#else
#define NBPG (4096)
#define UXN_SIZE (25 * NBPG)
#include <stdio.h>
#include <stdlib.h>
#endif

#include <xn.h>
#include <registry.h>
#include <xok/queue.h>
#include <xok_include/assert.h>

LIST_HEAD(freelist, xr);
struct freelist freelist;

struct xr*
reg_alloc() {
  struct xr* entry;

  if (!xn_registry) {
    int i;

    printf("Allocating the registry entries.\n");

    xn_registry = malloc(UXN_SIZE);
    assert(xn_registry);

    LIST_INIT(&freelist);

    for (i = 0; i < (UXN_SIZE / sizeof(struct xr)); i++) {
      LIST_INSERT_HEAD(&freelist, &xn_registry[i], sib);
    }
  }

  entry = freelist.lh_first;
  assert (entry);
  LIST_REMOVE(entry, sib);
  return entry;
}

void
reg_free(struct xr* entry) {
  LIST_INSERT_HEAD(&freelist, entry, sib);
}

