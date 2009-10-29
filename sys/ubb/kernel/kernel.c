
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
#include <xok/types.h>
#include <stdio.h>

#include "kernel/kernel.h"
#include "kernel/virtual-disk.h"
#include "lib/demand.h"

int cur_pid = 1;
cap_t root_cap = 0;
int root_pid = 0;

int udf_copyout(void *dst, void *src, size_t nbytes) {
	if(!dst || !src)
		return 0;
        memcpy(dst, src, nbytes);
        return 1;
}

int udf_copyin(void *dst, void *src, size_t nbytes) {
	if(!dst || !src)
		return 0;
        memcpy(dst, src, nbytes);
        return 1;
}

int read_accessible(void *src, size_t nbytes) {
	return (src || !nbytes) ? 1 : 0;
}

int write_accessible(void *src, size_t nbytes) {
	return (src || !nbytes) ? 1 : 0;
}

/* check wether cap allows op */
int auth_check(int op, cap_t xncap, cap_t cap, size_t size) {
        return memcmp(xncap, cap, size) == 0;
}

int vm_writable(void *dst, size_t nbytes) {
	return dst ? 1 : 0;
}

int switch_pid(int npid) {
	int opid;

	opid = cur_pid;
	cur_pid = npid;
	return opid;
}

/** Disk freemap. */
Bit_T free_map;

int db_isfree(db_t base, size_t nblocks) {
	return Bit_off(free_map, base, base + nblocks);
}

int db_find_free(db_t db, size_t nblocks) {
	long base;
	static long start;

	if(db >= XN_NBLOCKS)
		db = 0;
	if((base = db)) {
		/* Check if the bits are set. */
		if(!db_isfree(base, nblocks))
			return 0;
	} else {
		start = base;

		if((base = Bit_run(free_map, start, nblocks)) < 0)
			return 0; /* out of memory. */
		assert(db_isfree(base, nblocks));
	}
	start += 30;
	return base;
}

void *db_get_freemap(size_t *nbytes) {
	*nbytes = XN_NBLOCKS / 8;
	return Bit_handle(free_map, *nbytes);
}

void db_put_freemap(void *f_map, size_t nbytes) {
	Bit_copyin(free_map, f_map, nbytes);
}


int db_free(db_t db, size_t n) {
	long base;
	base = db;
	demand(!db_isfree(base, n), Blocks not allocated.);
	Bit_clear(free_map, base, base + n - 1);
	return base;
}

int db_alloc(db_t db, size_t n) {
	long base;
	base = db;
	if(db >= XN_NBLOCKS || !db_isfree(base, n))
		return 0;
	Bit_set(free_map, base, base + n - 1);
	demand(!db_isfree(base, n), must be alloced);
	return base;
}

/** Physical memory, of a sort. */

#define PHYS_MEM 512	/* pretty small... */
static struct page { char page[XN_BLOCK_SIZE]; } *pages;

Bit_T pmap; 	/* 0 = unallocated, 1 = allocated */

static long pmap_start;

/* right now we just allocate any page. */
ppn_t palloc(ppn_t base, size_t npages) {
	size_t sum;

	/* out of memory. */
	if(base < 0) {
		if((base = Bit_run(pmap, pmap_start, npages)) < 0)
			return 0;
		pmap_start = base;
	}

	sum = base + npages;
	if(sum > PHYS_MEM || sum < base)
		return 0;

	/* Check if the bits are set. */
	if(!Bit_off(pmap, base, base + npages))
		return 0;

	Bit_set(pmap, base, base + npages - 1);
	printf("allocating [%ld, %ld)\n", base, base + npages);
	return base;
}

void pfree(ppn_t base, size_t npages) {
	printf("freeing [%ld, %ld)\n", base, base + npages);
	demand(!Bit_off(pmap, base, base + npages), pages not allocated.);
	Bit_clear(pmap, base, base + npages - 1);
}

ppn_t phys_to_ppn(void *phys) {
	struct page *p = phys;
	
	return  !(p >= &pages[0] && p < &pages[PHYS_MEM]) ?
		0 : (p - &pages[0]);
}

void * ppn_to_phys(ppn_t ppn) {
	return ((unsigned)ppn >= PHYS_MEM) ? 0 : &pages[ppn];
}

int pcheck(ppn_t base, size_t npages, cap_t cap) {
	return !Bit_off(pmap, base, base + npages);
}

void kernel_init(void) {
	static int init;

	pmap_start = 1;
	if(init) {
		Bit_clear(free_map, 0, XN_NBLOCKS-1);
		return;
	}
	init = 1;
	pages = (void *)malloc(XN_BLOCK_SIZE * (PHYS_MEM+1));
	assert(pages);
	pages = (void *)roundup((unsigned)pages, XN_BLOCK_SIZE);
	demand((unsigned) pages % XN_BLOCK_SIZE == 0, Bogus alignment);

	pmap = Bit_new(PHYS_MEM);
	free_map = Bit_new(XN_NBLOCKS);
	demand(db_isfree(0, 1), should have allocated);
	db_alloc(0, 1);
	demand(!db_isfree(0, 1), should have allocated);
}
