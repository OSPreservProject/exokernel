
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

/* 
 * User-level library for creating the various structures.  Need to work
 * on this interface a bit. 
 */
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include <ubb/ubb.h>
#include <ubb/lib/bit.h>

#include <exos/mallocs.h>

#define U_MAX_EXT	16
#define U_MAX_INST	128

static u_reg regn = 1;

void u_reg_reset(void) {
	regn = 1;
}

u_reg u_getreg(void) {
	demand(regn < U_REG_MAX, out of registers!);
	return regn++;
}

int u_alloc_rwset(struct udf_ext *e, int n) {
	e->n = 0;
	return 1;
}

int u_add_rwset(struct udf_ext *e, size_t lb, size_t ub) {
	size_t n;
	struct udf_bl *b;

	n = e->n;
	if(n >= U_MAX_EXT)
		return 0;

	if(n && e->v[n-1].ub == lb)
		e->v[n-1].ub = ub;
	else {
		b = &e->v[n];
		b->lb = lb;
		b->ub = ub;
		e->n++;
	}

	return 1;
}

int u_alloc_f(struct udf_fun *f, int nargs, int type, int rset, int wset) {
	memset(f, 0, sizeof *f);
	u_reg_reset();

#if 0
        if(!u_alloc_rwset(&f->read_set, 1))
		return 0;

	if(!u_alloc_rwset(&f->write_set, 1))
                return 0;
#endif

	f->ninst = 0;
	f->nargs = nargs;
	f->result_t = type;

	return 1;
}

void u_delete_fun(struct udf_fun *f) {
#if 0
	__free(f->read_set.v);
	__free(f->write_set.v);
#endif
	__free(f->insts);
}

#define U_MAX_REGIONS	16

int u_type_alloc(struct udf_type *t, char *tn, size_t nbytes, size_t nowns) {

	memset(t, 0, sizeof *t);
	strcpy(t->type_name, tn);
	t->nbytes = nbytes;

	return 1;
}

void u_type_delete(struct udf_type *t) {
}

/** Disk freemap. */
#ifndef JJ
Bit_T free_map;
#endif

int db_isfree(db_t base, size_t nblocks) {
        return Bit_off(free_map, base, base + nblocks);
}

int db_find_free(db_t db, size_t nblocks) {
        long base;
        static long start;

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



