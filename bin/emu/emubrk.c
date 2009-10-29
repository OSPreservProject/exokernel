
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


#include <assert.h>
#include <errno.h>
#include <exos/osdecl.h>
#include <exos/vm.h>
#include <xok/mmu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emubrk.h"

u_int emu_brkpt = 0;
int emu_brk_heap_zero = -1;
#define SOFT_HEAP_SIZE NBPG*10
static u_int soft_break = 0;

int
emu_brk (const char *a)
{
  static int heap_demand_alloc = -1, heap_soft=-1;
  u_int addr = (u_int )a; /* just so the types work out nicely below */
  u_int first_unmapped = heap_soft == 1 ? soft_break : PGROUNDUP(emu_brkpt);

  /* check environment for options */
  if (emu_brk_heap_zero == -1)
    emu_brk_heap_zero = getenv("HEAP_ZERO") ? 1 : 0;
  if (heap_soft == -1) {
    /*    heap_demand_alloc = getenv("NO_HEAP_DEMAND_ALLOC") ? 0 : 1; */
    heap_demand_alloc = 0; /* XXX - fault handler can't handle it for now */
    heap_soft = getenv("NO_HEAP_SOFT") ? 0 : 1;
    if (heap_soft) soft_break = first_unmapped;
  }

  /* make sure they're not growing the data segment somewhere they shouldn't */
  if (addr >= __stkbot) {
    return (-1);
  }

  /* just return zero if they aren't changing anything */
  if (addr == emu_brkpt) {
    return (0);
  }

  /* round address up if needed */
  addr = PGROUNDUP(addr);

  if (addr > first_unmapped) {
    /* increase heap */
    if (!heap_demand_alloc)
      do {
	if (!isvamapped(first_unmapped) &&
	    _exos_self_insert_pte (0, PG_U|PG_W|PG_P, first_unmapped, 0, NULL) < 0) {
	  errno = ENOMEM;
	  return -1;
	}
	if (emu_brk_heap_zero)
	  bzero ((char *)first_unmapped, NBPG);
	first_unmapped += NBPG;
      } while (first_unmapped < addr);
    if (heap_soft) soft_break = addr;
  } else if (!heap_soft)
    /* reduce heap without soft heap */
    while (first_unmapped > addr) {
      first_unmapped -= NBPG;
      if (isvamapped(first_unmapped))
	if (_exos_self_insert_pte (0, 0, first_unmapped, 0, NULL) < 0) {
	  errno = ENOMEM;
	  return -1;
	}
    }
  else
    /* reduce heap with soft heap */
    while (soft_break > addr + SOFT_HEAP_SIZE) {
      soft_break -= NBPG;
      if (isvamapped(soft_break))
	if (_exos_self_insert_pte (0, 0, soft_break, 0, NULL) < 0) {
	  errno = ENOMEM;
	  return -1;
	}
    }

  return (emu_brkpt = addr);
}

char *
emu_sbrk (int incr)
{
  char *brkpt = (char *)emu_brkpt;

  if (emu_brk ((char *)(emu_brkpt + incr)) < 0) {
    errno = ENOMEM;
    return ((char *)-1);
  } else {
    return (brkpt);
  }
}
