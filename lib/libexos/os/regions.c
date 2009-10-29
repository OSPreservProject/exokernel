
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
#include <xok/sys_ucall.h>

#include <exos/conf.h>
#include <exos/regions.h>
#include <exos/vm-layout.h>
#include <xok/mmu.h>

#include <fd/proc.h>

/* #define MEMCPY */

/* change the number on the if to turn on or off */
#define dprintf if (0) kprintf

/* To support protected memory transfers from environment to another
 * environment xok supports dma regions.  An environment pass the kernel a
 * pointer to a table with dma regions.  Each region is protected by its
 * own capability.  The environment can pass the capabililies for regions
 * to other environments so that they can copy to or from the region.
 * Using system calls environments can copy in and out; the kernel will
 * check they have the appropriate rights and whether the addresses
 * involved are mapped.  */

/* Create a set of dma regions */
#define MAXREGIONS 1000

#define REGIONSIZE(n) \
(sizeof(struct dma_ctrlblk) + n *(sizeof (struct dma_region)))

static struct dma_ctrlblk *ctrlblk = (struct dma_ctrlblk *)0;

int dma_init(void) {
  int status;
  status = fd_shm_alloc(DMAREGIONS_SHM_OFFSET,
			REGIONSIZE(MAXREGIONS) ,
			(char *)DMAREGIONS_REGION);

  StaticAssert(REGIONSIZE(MAXREGIONS) <= DMAREGIONS_REGION_SZ)
  dprintf("dma_init ...");
  if (status == -1) {
    demand(0, problems attaching shm);
    return -1;
  }
  ctrlblk = (struct dma_ctrlblk *)DMAREGIONS_REGION;
  if (status) {
    /* first time. */
    dprintf("first Ok ctrlblk %p size: %d\n",ctrlblk,DMAREGIONS_REGION_SZ);
    ctrlblk->nregions = 0;
  } else {
    dprintf("attaching only %p size: %d\n",ctrlblk,DMAREGIONS_REGION_SZ);
    /*printf("This is not first process, just attaching memory\n");  */
  }
  return 0;
}

int dma_setup(struct dma_ctrlblk *b) {
  assert(ctrlblk);
  assert(b);
  demand(b->nregions < MAXREGIONS, too many regions);
  
  if (ctrlblk->nregions) {
    printf("dma already set up, use dma_setup_append\n");
    return -1;
  }
				 
  ctrlblk->nregions = b->nregions;
  ctrlblk->dma_regions = (struct dma_region *)(ctrlblk + 1);
  dprintf("dma_setup ctrlblk %p, n: %d, dma_regions: %p\n",
	  ctrlblk,ctrlblk->nregions,ctrlblk->dma_regions);
  memcpy((void *)ctrlblk->dma_regions,b->dma_regions,
	 b->nregions * sizeof(struct dma_region));
  return 0;
}

/* returns the regid of the first dma_ctrlblk */
int dma_setup_append(struct dma_ctrlblk *b) {
  int initialregions;
  assert(ctrlblk);
  assert(b);
  demand((b->nregions + ctrlblk->nregions) < MAXREGIONS, too many regions);
  
  if (ctrlblk->nregions == 0) return dma_setup(b);
  
  initialregions = ctrlblk->nregions;

  ctrlblk->nregions += b->nregions;

  ctrlblk->dma_regions = (struct dma_region *)(ctrlblk + 1);
  dprintf("dma_setup_append ctrlblk %p, n: %d, dma_regions: %p\n",
	 ctrlblk,ctrlblk->nregions,ctrlblk->dma_regions);
  memcpy((void *)&ctrlblk->dma_regions[initialregions],
	 b->dma_regions,
	 b->nregions * sizeof(struct dma_region));
  return initialregions;
}

void dma_print_region(struct dma_region *r) {
  printf("region: %p - key: %-8d reg_addr: %p, reg_size: %-8d\n",
	 r,r->key, r->reg_addr, r->reg_size);
}

void dma_print_ctrlblk(struct dma_ctrlblk *c) {
  int i;
  printf("ctrlblk: %p, nregions: %-8d\n",
	 c, c->nregions);
  for(i = 0; i < c->nregions; i++) {
    printf("%4d ",i);
    dma_print_region(&c->dma_regions[i]);
  }
}

void dma_print_my_ctrlblk(void) {dma_print_ctrlblk(ctrlblk);}

/* DMA n bytes from src_addr in src_envid to dst_addr in dst_envid,
 dst_addr through dst_addr + n, must be a subset of the region specified
 by region id. */
int dma_to(int src_envid, void *src_addr, int n, 
	   int dst_envid, void *dst_addr, 
	   int key, int regid) {
  struct dma_region *r;
#ifndef MEMCPY
  int t;
#endif

  assert(src_envid == 0 || __envid);
  assert(ctrlblk);

  dprintf("dma_to  se %d, src %p, n %d, de %d, dst %p, key %d regid %d\n",
	  src_envid, src_addr,  n, dst_envid, dst_addr, key, regid);

  if (regid > ctrlblk->nregions || regid < 0) {
    dprintf("region out of bounds: %d > %d\n",regid,ctrlblk->nregions);
    return -1;
  }
  r = &ctrlblk->dma_regions[regid];

  if (dst_addr < r->reg_addr ||
      (dst_addr + n) > (r->reg_addr + r->reg_size)) {
    return -1;
    dprintf("dma_to out of bounds %p  %p  %p\n",r->reg_addr,dst_addr, 
	    (r->reg_addr + r->reg_size));
  }
  dprintf("dma_to  %p:%d --> %p(%d)\n", src_addr, n, dst_addr, 
	  (vpt[((u_int) dst_addr) >> PGSHIFT] >> PGSHIFT));
#ifdef MEMCPY
  memcpy(dst_addr, src_addr, n);
#else
  if ((t = sys_vcopyout(src_addr, dst_envid, (u_long) dst_addr, n)) != 0) {
      printf("dma_to: vcopy failed %d\n", t);
      assert(0);
  }
#endif
  return 0;
}

/* DMA n bytes from src_addr in src_envid to dst_addr in dst_envid,
 src_addr through src_addr + n, must be a subset of the region specified
 by region id. */
int dma_from(int src_envid, void *src_addr, int n, 
	     int dst_envid, void *dst_addr, 
	     int key, int regid) {
  struct dma_region *r;
#ifndef MEMCPY
  int t;
#endif

  assert(dst_envid == 0 || __envid);
  assert(ctrlblk);

  dprintf("dma_from se %d, src %p, n %d, de %d, dst %p, key %d regid %d\n",
	  src_envid, src_addr,  n, dst_envid, dst_addr, key, regid);

  if (regid > ctrlblk->nregions || regid < 0) {
    dprintf("region out of bounds: %d > %d\n",regid,ctrlblk->nregions);
    return -1;
  }
  r = &ctrlblk->dma_regions[regid];

  if (src_addr < r->reg_addr ||
      (src_addr + n) > (r->reg_addr + r->reg_size)) {
    dprintf("dma_from out of bounds %p  %p  %p\n",r->reg_addr,src_addr, 
	    (r->reg_addr + r->reg_size));
    return -1;
  }

#ifdef MEMCPY
  memcpy(dst_addr, src_addr, n);
#else
  if ((t = sys_vcopyin(src_envid, (u_long) src_addr, dst_addr, n)) != 0) {
      dprintf("dma_from: vcopyin failed %d %d\n", t, src_envid);
      return t;
  }
#endif
  dprintf("dma_from %p:%d --> %p\n", src_addr, n, dst_addr);
  return 0;
}

