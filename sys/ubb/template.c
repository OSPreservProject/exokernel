
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
 * Import an xn template and compile to a representation that can be checked 
 * quickly. 
 *
 * TODO:
 *	+ need to write to disk for real.
 *	+ need to do copyin, etc.
 */
#include <xok/types.h>
#include "xn.h"
#include "demand.h"
#include "kexcept.h"
#include <kernel.h>
#include "virtual-disk.h"
#include "udf.h"
#include "ubb-lib.h"

/***************************************************************************
 * XXX: Following code is fake.  Fix to write to disk.
 *
 */
#define MAXTEMPLATES  128

typedef struct xn_tmplt {
        struct udf_type *t;
	int active;
	size_t nbytes;	/* size of the template */
} xn_tmplt;

static xn_tmplt catalogue[MAXTEMPLATES];
static unsigned ntemplates = 1;	

static xn_tmplt *tlookup(xn_elem_t type) {
	xn_tmplt *t;

	ensure(type < ntemplates, 0);
	t = &catalogue[type];
	
	return !t->active ? 0 : t;
}

static xn_err_t talloc(void) {
	int n;
	ensure(ntemplates < MAXTEMPLATES, XN_TOO_MANY_TEMPLATES);
	n = ntemplates++;	/* uch. */
	catalogue[n].active = 1;
	return n;
}

/* Insert into catalogue. Should check for structural equivalence. */
static xn_err_t tinsert(struct udf_type *ut) {
	xn_tmplt *t;
	xn_elem_t type;

	try(type = talloc());
	t = tlookup(type);
	demand(t, bogus insertion);

	t->t = ut;
	t->nbytes = t->t->nbytes;

	return type;
}

static xn_err_t tdelete(xn_elem_t type) {
	xn_tmplt *t;

	ensure((t = tlookup(type)) && t->active, XN_BOGUS_TYPE);
	t->active = 0;
	return XN_SUCCESS;
}

/************************************************************************
 * Routines used by the rest of the kernel to manipulate structures via
 * templates.
 */

struct udf_type *type_lookup(xn_elem_t t) ;

xn_err_t type_unswizzle(udf_type *t) {
	struct udf_struct *s;
	int i;
	
	assert(t->nbytes);
	switch(t->class) {
	case UDF_BASE_TYPE:  
		break;
	case UDF_STRUCT:	
	case UDF_UNION:
		s = &t->u.s;
		check(s->ntypes < UDF_MAX_TYPES, ntypes too large, 0);
		for(i = 0; i < s->ntypes; i++) {
			if(!(s->t[i] = type_lookup(s->ty[i])))
				return XN_TYPE_MISS;
		}
		break;
	default: 
		return XN_BOGUS_TYPE;
	}
	return XN_SUCCESS;
}

/* Create volatile binding to type t. */
xn_err_t type_insert(struct udf_type *t) {

	demand(t->nbytes, Cannot be nil!);
	try(type_unswizzle(t));
	return tinsert(t);
}

/* 
 * They must have installed it in the root registry.
 * We check it and change the capabilty.  (XXX Must be
 * read-only.)
 */
xn_err_t type_import(struct udf_type *t) {
	xn_err_t ty;

	if(t->nbytes == 0)
		return XN_BOGUS_TYPE;
	try(type_unswizzle(t));
	demand(!t->active, Bogus active);
	try(ty = type_insert(t));
	if(udf_type_check(t)) {
		demand(t->nbytes, no bytes!);
		t->active = 1;
		return ty;
	} else {
		tdelete(ty);
		return XN_BOGUS_TYPE;
	}
}

void tmplt_shutdown(void) {
	int i;
	udf_type *t;

	ntemplates = 1;
	for(i = XN_ROOT_TYPE; i <= XN_DB; i++) {
		t = type_lookup(i);
		free(t);
	}
	memset(catalogue, 0, sizeof catalogue);
}
void tmplt_init(void) {
	struct udf_type *t;
	size_t nbytes;
	int i;

	ntemplates = 1;	
	for(i = XN_ROOT_TYPE; i < XN_DB; i++) {
		t = calloc(1, sizeof *t);
		assert(t);
		if(i == XN_TYPE_TYPE)
			t->nbytes = sizeof *t;
		if(i != tinsert(t))
			fatal(Bogus template id);
	}

	t = calloc(1, sizeof *t);
	assert(t);
	nbytes = XN_BLOCK_SIZE;
   	/* Should we seperate and let them build up? */
        if(!u_type_alloc(t, "DiskBlock", nbytes, 0))
                fatal(Could not allocate);

	t->class = UDF_BASE_TYPE;
	t->is_sticky = 1;

      	if(!u_alloc_rwset(&t->u.t.raw_read, 1))
                fatal(Could not allocate);
        if(!u_alloc_rwset(&t->u.t.raw_write, 1))
                fatal(Could not allocate);
        if(!u_add_rwset(&t->u.t.raw_write, 0, nbytes-1))
                fatal(Could not allocate);
        if(!u_add_rwset(&t->u.t.raw_read, 0, nbytes-1))
                fatal(Could not allocate);

	if(XN_DB != tinsert(t))
 		fatal(Bogus result);
}

struct udf_type *type_lookup(xn_elem_t t) {
	xn_tmplt *tmplt;
        if((tmplt = tlookup(t)))
		return tmplt->t;
	return 0;
}
