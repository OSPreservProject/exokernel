
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
 * Most of this file would go away if we represented catalogues using
 * UDFs.  We do not do this for (1) simplicity, and (2) the UDF interface
 * is changing --- modifying multiple uses is much harder than a single 
 * one.  XXX rewrite once done (remove fixed sized restrictions as well).
 */
#ifdef EXOPC
#include <xn.h>
#include "kernel.h"
#include <xok_include/string.h>
#else
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#endif

#include <xn-struct.h>

#include "root-catalogue.h"
#include "ubb-lib.h"
#include "demand.h"
#include "kexcept.h"
#include "kernel.h"
#include <sync-disk.h>
#include "registry.h"
#include "template.h"
#include "type.h"
#include <virtual-disk.h>

#define NPAGES		1
static struct root_catalogue *r_c; 	/* PTR to the root catalogue. */
db_t r_db;

static void fake_op(xn_op *op, cap_t c, xn_elem_t t, db_t db, size_t nelem) {
	memset(op, 0, sizeof *op);
	op->u.db = db;
	op->u.nelem = nelem;
	op->u.type = t;
	op->m.cap = c;
}

static struct root_entry * r_find(char *name, int *found) {
	struct root_entry *r;
	int i, n;

	n = r_c->nelem; 
	for(i = 0, r = &r_c->roots[0]; i < n; i++, r++) 
		if(strncmp(name, r->name, sizeof r->name) == 0) {
			*found = 1;
			return r;
		}
	*found = 0;
	return (i < R_NSLOTS) ? r : 0;
}

/* Ensure strlen(p) is < n. */
static int strnlen(char *p, int n) {
	int i;

	if(!read_accessible(p, n))
		return 0;
	for(i = 0; i < n; i++)
		if(!p[i])
			return 1;
	return 0;
}

void bc_check(char *msg, db_t db);

/* Load root catalogue.  We build registry entry by hand.  */
void root_init(int new_p) {
//	bc_check("checking root 1", r_db);
	if(r_c)
		memset(r_c, 0, NPAGES);
	else if(!(r_c = ppn_to_phys(palloc(-1, NPAGES))))
                fatal(Cannot fail);
//	bc_check("checking root 2", r_db);

	/* Read in the old root catalogue. */
	if(!new_p)
#ifdef EXOPC
	        assert (0);
#else		
		sync_read(r_c, r_db, R_NBLOCKS);
#endif
	/* Initialize a new root catalogue */
	else {
        	memset(r_c, 0, sizeof *r_c);
		r_c->da = da_compose(r_db, 0);
#ifndef EXOPC
		sync_write(r_db, r_c, R_NBLOCKS);
#endif
	}
//	bc_check("checking root 3", r_db);

	/* hand construct a registry entry. */
	xr_backdoor_install(XN_NO_PARENT, -1, (void **)&r_c, XN_ROOT_TYPE, 
				XN_BLOCK_SIZE, r_db, R_NBLOCKS);

//	bc_check("checking root 4", r_db);

	/* 
	 * We don't install entries for each since the catalogue could
	 * (in a useful world) be large. 
	 */
}

/* 1 == success, < 1 failure. */
int root_install(char *name, db_t *db, size_t nelem, xn_elem_t t, cap_t c) {
	struct root_entry *r;
	int found;
	xn_op op;
	xn_err_t res;

	/* Preconditions. */
	ensure(read_accessible(name, sizeof r->name), 	XN_CANNOT_ACCESS);
				
	ensure(strnlen(name, sizeof r->name), 		XN_BOGUS_NAME);
	ensure(r = r_find(name, &found), 		XN_CANNOT_ALLOC);
	ensure(!found, 					XN_DUPLICATE);

	fake_op(&op, c, t, *db, nelem);

	/* Commit (happens in the middle of this routine). */
	xn_in_kernel++;
        	res = sys_xn_alloc(r_c->da, &op, 1);
	xn_in_kernel--;
	if(res < 0) 
		sys_return(res);

	strcpy(r->name, name);
	r->c = c;
	*db = r->db = op.u.db;
	r->nelem = nelem;
	r->t = t;
	r->is_type = 0;
	r_c->nelem++;
	
	return XN_SUCCESS;
}

/* Load a root entry into the registry. */
xn_err_t sys_xn_mount(struct root_entry *re, char *name, cap_t c) {
	int found;
	struct root_entry *r;
	xn_op op;
	xn_err_t res;

	ensure((r = r_find(name, &found)) && found, 	XN_NOT_FOUND);
	ensure(udf_copyout(re, r, sizeof *r), 		XN_CANNOT_ACCESS);
        ensure(type_lookup(r->t),                       XN_TYPE_MISS);

	/* XXX need to check authorization. */

	fake_op(&op, c, r->t, r->db, r->nelem);
	xn_in_kernel++;
		res = sys_xn_insert_attr(r_c->da, &op);
	xn_in_kernel--;
	sys_return(res);
}

/* Insert type into volatile type table. */
xn_err_t sys_type_mount(char *name) {
	struct root_entry *r;
	int found;
	size_t nblocks;
	struct udf_type *t;

	ensure((r = r_find(name, &found)) && found, 	XN_NOT_FOUND);
	ensure(r->is_type, 				XN_BOGUS_TYPE);

	/* Fast path. */
	ensure(!type_lookup(r->is_type),		r->is_type);

	nblocks = type_to_blocks(XN_TYPE_TYPE, 1);
	try(xr_contig(r->db, nblocks));

	/* (sort of) trick bc into holding them.  */
	if(xr_pin(r->db, nblocks) != XN_SUCCESS)
		fatal(Should not fail);

	t = xr_lookup(r->db)->page;
	demand(t->active, did no write back);
	return type_insert(xr_lookup(r->db)->page);
}

	/* 
	 * Where do type names come from?  Can't be slot, since it can
	 * move.   Increment a counter, I suppose.
	 */
	/* Just use the normal type names.  We will fix this later. */


/* Import type; side-effect: form a volatile binding. */
xn_err_t sys_type_import(char *name) {
	struct root_entry *r;
	int found, tid;
	size_t nblocks, expected;

	ensure((r = r_find(name, &found)) && found, 	XN_NOT_FOUND);
	ensure(!r->is_type, 				XN_BOGUS_TYPE);

	nblocks = type_to_blocks(r->t, r->nelem);
	expected = type_to_blocks(XN_TYPE_TYPE, 1);
	ensure(nblocks == expected,  XN_TOO_SMALL);

	try(xr_contig(r->db, nblocks));
	if(xr_pin(r->db, nblocks) != XN_SUCCESS)
		fatal(Should not fail);
	try(tid = type_import(xr_lookup(r->db)->page));
	r->is_type = tid;

	/* Writeback; we ignore count (XXX). */
	xn_in_kernel++;
	if(sys_xn_writeback(r->db, nblocks, 0) < 0)
		fatal(Writeback cannot fail);
	xn_in_kernel--;

	return tid;
}

/* Remove element from root catalogue; makes stable (bad idea?). */
int root_delete(char *name, cap_t c) {
	xn_op op;
	struct root_entry *r, *p;
	int found;
	xn_err_t res;

	ensure(strnlen(name, sizeof r->name[0]), 	XN_BOGUS_NAME);
	ensure(r = r_find(name, &found), 		XN_CANNOT_ALLOC);
	ensure(!found, 					XN_DUPLICATE);

	fake_op(&op, c, r->t, r->db, r->nelem);
	xn_in_kernel++;
		res = sys_xn_free(r_c->da, &op, 1);
	xn_in_kernel--;
	if(res < 0)
		sys_return(res);

	/* copy new element down. */
	p = &r_c->roots[r_c->nelem--];
	*r = *p;

	return XN_SUCCESS;
}

/* 
 * Recurse down rooted trees, reallocating disk blocks. 
 * (Clean shutdown should write out the free map too).
 */
void reconstruct(int clean) {
	struct root_entry *r;
	int i;

	/* Page in all the types. */
	
	/* Now page in all the roots. */
	for(r = r_c->roots, i = 0; i < r_c->nelem; i++, r++)
		if(!db_alloc(r->db, type_to_blocks(r->t, r->nelem)))
			fatal(Should not fail!);
}

xn_err_t sys_xn_read_catalogue(struct root_catalogue *c) {
        ensure(udf_copyout(c, r_c, sizeof *r_c),     XN_CANNOT_ACCESS);
        return XN_SUCCESS;
}

