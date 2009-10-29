
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

/* Fake user-level implementation of various kernel utilities. */

#include "xok/defs.h"
#include "xok/mmu.h"
#include "xok/types.h"
#include "xok/pmap.h"
#include "xok/sysinfo.h"
#include "xok/vcopy.h"

#include "kernel.h"
#include "virtual-disk.h"
#include "bit.h"
#include "demand.h"

int cur_pid = 1;
#ifdef EXOPC
#include <xok/env.h>
#define root_cap __envs[0].env_clist[0]
#else
cap_t root_cap = 0;
#endif
int root_pid = 0;

/* copyin/copyout will propogate faults back up to userlevel and make
   sure that the user doesn't pass a pointer into kernel space to us */

xn_cnt_t* valid_user_ptr(xn_cnt_t* ptr) {
  if (ptr) {
    Pte* pte;
    if ((((unsigned int) ptr) % sizeof(int)) ||
	!(pte = va2ptep((u_int)ptr)) ||
	((*pte & (PG_P|PG_U|PG_W)) != (PG_P|PG_U|PG_W))) {
      return 0;
    }
    return (xn_cnt_t*)pa2kva(va2pa(ptr));
  }
  return 0;
}

int udf_copyout(void *dst, void *src, size_t nbytes) {
	if(!dst || !src)
		return 0;
	copyout (src, dst, nbytes);
        return 1;
}

int ubb_copyin(void *dst, void *src, size_t nbytes) {
	if(!dst || !src)
		return 0;
	copyin (src, dst, nbytes);
        return 1;
}

/* look up the pte's and verify a) that they exist and b) have the
   correct permissions. vcheck in vcopy.c does all the work. */

int read_accessible(void *src, size_t nbytes) {
  int ret;

  ret = vcheck (curenv, (u_int )src, nbytes, 0);
  if (ret == -1)
    return 0;
  else
    return 1;
}

int write_accessible(void *src, size_t nbytes) {
  int ret;

  ret = vcheck (curenv, (u_int )src, nbytes, 1);
  if (ret == -1)
    return 0;
  else
    return 1;
}

/* check wether cap allows op */
int auth_check(int op, cap_t xncap, cap_t cap, size_t size) {
#if 0
    if (acl_access (cap, xncap, 1, op) < 0)
      return 0;
    else
#endif
      return 1;
}

/** Disk freemap. */
Bit_T free_map;

int db_isfree(db_t base, size_t nblocks) {
	if ((base < 0) || ((base+nblocks) > si->si_xn_blocks[0])) {
	   return (0);
        }
	return Bit_off(free_map, base, base + nblocks);
}


/* 0 on failure, db on success. */
db_t sys_db_find_free(db_t hint, size_t nblocks) {
        return db_find_free(hint, nblocks);
}

db_t db_find_free(db_t db, size_t nblocks) {
	static long start;
	long base;

	if(!(base = db))
		base = start;

	if((base = Bit_run(free_map, base, nblocks)) < 0)
		return 0; /* out of memory. */
	assert(db_isfree(base, nblocks));

	if(!db)
		start = (base + nblocks) % XN_NBLOCKS;
	if(!start)
		start++;

	return base;
}

void *db_get_freemap(size_t *nbytes) {
	unsigned long long xndisksize = (unsigned long long) XN_NBLOCKS * (unsigned long long) XN_BLOCK_SIZE;
	unsigned long long realdisksize = (unsigned long long) si->si_disks[XN_DEV].d_size * (unsigned long long) si->si_disks[XN_DEV].d_bsize;
	printf ("XN: hard-coding disk size to be %ld %d-byte blocks\n", XN_NBLOCKS, XN_BLOCK_SIZE);
        demand ((xndisksize <= realdisksize), hard-coded disk size is too big);
	*nbytes = XN_NBLOCKS / 8;
	return Bit_handle(free_map, *nbytes);
}

void db_put_freemap(void *f_map, size_t nbytes) {
	printf ("db_put_freemap entered??\n");
	Bit_copyin(free_map, f_map, nbytes);
}


int db_free(db_t db, size_t n) {
	long base;
	base = db;
	demand(!db_isfree(base, n), Blocks not allocated.);
	si->si_xn_freeblks[0]++;
	Bit_clear(free_map, base, base + n - 1);
	return base;
}

int db_alloc(db_t db, size_t n) {
	long base;
	base = db;
	if(!db_isfree(base, n))
		return 0;
	si->si_xn_freeblks[0]--;
	Bit_set(free_map, base, base + n - 1);
	demand(!db_isfree(base, n), must be alloced);
	return base;
}

/** Physical memory, of a sort. */

/* right now we just allocate any page. */
ppn_t palloc(ppn_t base, size_t npages) {
  struct Ppage *pp;

  if (npages <= 0)
    return 0;

  /* XXX -- this is stupid...we should have one routine to call to
     allocate a page */

  if (npages == 1) {
    if (ppage_alloc (PP_KERNEL, &pp, 0) == -1)
      return 0;
  } else {
    if (ppage_alloc_n (PP_KERNEL, &pp, npages) == -1)
      return 0;
  }

  return (ppn (pp));
}

void pfree(ppn_t base, size_t npages) {
  int i;

  for (i = 0; i < npages; i++) {
    demand ((ppages[base+i].pp_status == PP_KERNEL), freeing non-allocated page);
    ppage_free (&ppages[base+i]);
  }
}

ppn_t phys_to_ppn(void *phys) {
  return (ppn(kva2pp(phys)));
}

void * ppn_to_phys(ppn_t p) {
  return ((void *)pa2kva (pp2pa (&ppages[p])));
}

/* XXX guess this just makes sure the extent of pages are allocated 
   and guarded by cap. */
int pcheck(ppn_t base, size_t npages, cap_t cap) {
  int i;

  for (i = 0; i < npages; i++) {
    if (ppages[base+i].pp_status != PP_USER) {
printf ("pcheck failing due to bad page status: base %ld, i %d, npages %d, pp_status %x\n", base, i, npages, ppages[base+i].pp_status);
      return 0;
    }
#if 0    
    if (acl_access (cap, ppages[base+i].pp_acl, PP_ACL_LEN, ACL_W) < 0) {
      return 0;
    }
#endif
  }
  return 1;
}

void kernel_init(void) {
        StaticAssert(XN_NBLOCKS/8 <= UXNMAP_SIZE);
	printf("Building freemap in kernal a pa: %ld\n",kva2pa(__xn_free_map));
	bzero (si->si_xn_blocks, MAX_DISKS*sizeof(u_int));
	bzero (si->si_xn_freeblks, MAX_DISKS*sizeof(u_int));
	free_map = Bit_build(XN_NBLOCKS, __xn_free_map);
	si->si_xn_blocks[0] = XN_NBLOCKS;
	si->si_xn_freeblks[0] = XN_NBLOCKS;
	demand(db_isfree(0, 1), should have allocated);
	db_alloc(0, 1);
	demand(!db_isfree(0, 1), should have allocated);
}

