
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
#include <exos/osdecl.h>
#include <exos/callcount.h>
#include <exos/mregion.h>
#include <exos/vm.h>
#include <xok/mmu.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int _exos_brk_heap_zero = -1;

static int
brk_handler(struct mregion_ustruct *mru, void *faddr, unsigned int errcode)
{
  u_int page = PGROUNDDOWN((u_int)faddr);
  /* if page not present then make one present! */
  if (!(errcode & FEC_PR)) {
    if (__vm_alloc_region(page, 1, 0, PG_U|PG_W|PG_P) < 0)
      return 0;
    if (_exos_brk_heap_zero == -1)
      _exos_brk_heap_zero = getenv("HEAP_ZERO") ? 1 : 0;
    if (_exos_brk_heap_zero)
      bzero ((char *)page, NBPG);
    return 1;
  }
  return 0;
}

static struct mregion_ustruct brk_ustruct = { brk_handler };

#define SOFT_HEAP_SIZE NBPG*10
static u_int soft_break = 0;
static char *
brk_aligned(const char *a)
{
  static int heap_demand_alloc = -1, heap_soft=-1;
  static u_int brkpt_aligned = 0;
  u_int addr = (u_int )a; /* just so the types work out nicely below */
  u_int first_unmapped;

  if (brkpt_aligned == 0) brkpt_aligned = PGROUNDUP(__brkpt);
  first_unmapped = heap_soft == 1 ? soft_break : brkpt_aligned;

  /* check environment for options */
  if (_exos_brk_heap_zero == -1) {
    _exos_brk_heap_zero = getenv("HEAP_ZERO") ? 1 : 0;
    heap_demand_alloc = getenv("NO_HEAP_DEMAND_ALLOC") ? 0 : 1;
    heap_soft = getenv("NO_HEAP_SOFT") ? 0 : 1;
    if (heap_soft) soft_break = first_unmapped;
  }

  /* make sure they're not growing the data segment somewhere they shouldn't */
  if (addr >= __stkbot || addr <= NBPD)  return (void*)-1;

  /* round address up if needed */
  addr = PGROUNDUP(addr);

  if (addr > first_unmapped) {
    /* increase heap */
    if (mregion_alloc((void*)first_unmapped,
		      addr-first_unmapped, &brk_ustruct) < 0)
      return (void*)-1;
    if (!heap_demand_alloc)
      do {
	if (!isvamapped(first_unmapped) &&
	    _exos_self_insert_pte (0, PG_U|PG_W|PG_P, first_unmapped, 0, NULL) < 0)
	  return (void*)-1;
	if (_exos_brk_heap_zero)
	  bzero ((char *)first_unmapped, NBPG);
	first_unmapped += NBPG;
      } while (first_unmapped < addr);
    if (heap_soft) soft_break = addr;
  } else if (addr < first_unmapped) {
    if (!heap_soft) {
      /* reduce heap without soft heap */
      if (__vm_free_region(addr, first_unmapped-addr, 0) < 0 ||
	  mregion_free((void*)addr, (void*)first_unmapped-1) < 0)
	return (void*)-1;
    } else { /* reduce heap with soft heap */
      if (addr + SOFT_HEAP_SIZE < first_unmapped) {
	soft_break = addr + SOFT_HEAP_SIZE;
	if (__vm_free_region(soft_break, first_unmapped-soft_break, 0) < 0 ||
	    mregion_free((void*)soft_break, (void*)first_unmapped-1) < 0)
	  return (void*)-1;
      }
    }
  }

  brkpt_aligned = addr;
  return 0;
}

char *
brk(const char *addr)
{
  OSCALLENTER(OSCALL_break);
  if (brk_aligned(addr) < 0) {
    errno = ENOMEM;
    OSCALLEXIT(OSCALL_break);
    return (void*)-1;
  }
  __brkpt = (u_int)addr;
  OSCALLEXIT(OSCALL_break);

  return 0;
}

char *
sbrk(int incr)
{
  OSCALLENTER(OSCALL_sbrk);
  if (brk_aligned((void*)(__brkpt + incr)) < 0) {
    errno = ENOMEM;
    OSCALLEXIT(OSCALL_sbrk);
    return (void*)-1;
  }
  __brkpt += incr;
    OSCALLEXIT(OSCALL_sbrk);
  return (void*)(__brkpt - incr);
}
