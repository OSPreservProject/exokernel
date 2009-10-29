
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


#include <xok/defs.h>
#include <xok/env.h>
#include <xok/sys_proto.h>
#include <xok/syscallno.h>
#include <xok/kerrno.h>
#include <xok_include/assert.h>
#include <xok/vcopy.h>

/* Check that a range of virtual addresses is valid in environment e */
inline int
vcheck (struct Env *e, u_long va, int len, int write)
{
  Pte bits = write ? (PG_P|PG_W|PG_U) : (PG_P|PG_U);
  int npages;
  Pte *ptep;

  if (len > 0)
    npages = (len + (va & PGMASK) + PGMASK) >> PGSHIFT;
  else
    npages = 0;

  va &= ~PGMASK;
  while (npages > 0) {
    ptep = env_va2ptep (e, va);
    if (!ptep || (*ptep & bits) != bits)
      return -1;
    va += NBPG;
    npages--;
  }
  return 0;
}

/* Copy to/from another environment's virtual address space */
#define VCFL_OUT 0x1     /* Copy out (write to renv) */
#define VCFL_KVM 0x2     /* Local VA is actually kernel memory */
inline int
vcopy (char *lva, struct Env *renv, u_long rva, int len, int flags)
{
  Pte bits = (flags & VCFL_OUT) ? (PG_P|PG_W|PG_U) : (PG_P|PG_U);
  int m = page_fault_mode;

  rva = trup (rva);
  if (!(flags & VCFL_KVM)) {
    /* lva supplied by user--be prepared to fault on it */
    lva = trup (lva);
    page_fault_vcopy = 0;
    page_fault_mode = PFM_VCOPY;
  }
  else
    page_fault_mode = PFM_NONE;

  while (len > 0) {
    int n;
    Pte *ptep;
    char *kva;

    n = min (len, NBPG - (rva & PGMASK));
    ptep = env_va2ptep (renv, rva);
    if (!ptep || (*ptep & bits) != bits)
      break;
    kva = (char *) (((u_int) ptov (*ptep & ~PGMASK)) | ((u_int) rva & PGMASK));

    if (flags & VCFL_OUT)
      novbcopy (lva, kva, n);
    else
      novbcopy (kva, lva, n);
    if (!(flags & VCFL_KVM) && page_fault_vcopy)
      break;

    lva += n;
    rva += n;
    len -= n;
  }

  page_fault_mode = m;
  if (! (flags & VCFL_KVM))
    return !len ? 0 : (page_fault_vcopy ? -2 : -1);
  else
    return !len ? 0 : -1;
}


/*
 * Everything below here is a security hole for testing purposes only.
 */

int
sys_vcopyin (u_int sn, u_int senv, u_long sva, void *dst, int len)
{
  struct Env *e;
  int r;

  if (!(e = env_id2env (senv, &r))) return r;
  switch (vcopy (dst, e, sva, len, 0)) {
  case 0:
    return 0;
  case -1:
    return -E_RVMI;
  case -2:
    return -E_FAULT;
  }
  return 0;
}

int
sys_vcopyout (u_int sn, void *src, u_int denv, u_long dva, int len)
{
  struct Env *e;
  int r;

  if (!(e = env_id2env (denv, &r))) return r;
  switch (vcopy (src, e, dva, len, VCFL_OUT)) {
  case 0:
    return 0;
  case -1:
    return -E_RVMI;
  case -2:
    return -E_FAULT;
  }
  return 0;
}
