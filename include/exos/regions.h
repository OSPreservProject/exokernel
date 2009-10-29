
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

/* DMA region */
typedef struct dma_region {
    int key;                    /* capability key */
    void *reg_addr;              /* starting address */
    unsigned int reg_size;      /* size of region */
} dma_region_t, *dma_region_p;

/* DMA control block */
typedef struct dma_ctrlblk { 
    int nregions;            /* number of regions */
    struct dma_region  *dma_regions; /* the regions */
} dma_ctrlblk_t, *dma_ctrlblk_p;


/* Create a set of dma regions */
extern int dma_setup(struct dma_ctrlblk *b);

/* Append a set of dma regions, returns the region id of the first region
   in b.*/
extern int dma_setup_append(struct dma_ctrlblk *b);

/* DMA n bytes from src_addr in src_envid to dst_addr in dst_envid,
 src_addr through src_addr + n, must be a subset of the region specified
 by region id. */
extern int dma_to(int src_envid, void *src_addr, int n, 
	   int dst_envid, void *dst_addr, 
	   int key, int regid);

/* DMA n bytes from src_addr in src_envid to dst_addr in dst_envid,
 src_addr through src_addr + n, must be a subset of the region specified
 by region id. */
extern int dma_from(int src_envid, void *src_addr, int n, 
	     int dst_envid, void *dst_addr, 
	     int key, int regid);


#if 0
/* The vector version of dma_to and dma_from: */
int dma_tov(src_env, ae_recv *r, dst_envid, key, int nregid, int *regid);
int dma_fromv(from, env, key, nregid, int *regid, n, dest_envid, ae_recv *r)
#endif
     /* For debugging */
extern int dma_init(void);	/* this will not be exportable */

extern void dma_print_my_ctrlblk(void);
extern void dma_print_region(struct dma_region *r);
extern void dma_print_ctrlblk(struct dma_ctrlblk *c);
