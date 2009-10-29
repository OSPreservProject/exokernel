
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

#include <exos/conf.h>

#ifdef XN

/* Rewrite Rewrite Rewrite Rewrite */
/* Rewrite Rewrite Rewrite Rewrite */
/* Rewrite Rewrite Rewrite Rewrite */
/* Rewrite Rewrite Rewrite Rewrite */
#include <string.h>
#include <memory.h>

#ifdef JJ
#include <ubb/lib/demand.h>
#else
#include <assert.h>
#define fatal(a)	demand(0,a)
#endif

#include <ubb/ubb.h>
#include <ubb/lib/ubb-lib.h>
#include "spec.h"

static size_t cur_offset;
static size_t cur_index;
static int lock;

void xn_begin(struct udf_type *t, char *name, int sticky) {
	demand(!lock, can only specify one type at a time);
	memset(t, 0, sizeof *t);
	cur_offset = 0;
	cur_index = 0;
	lock = 1;
	memset(t->type_name, 0, sizeof t->type_name);
	strcpy(t->type_name, name);
	t->is_sticky = sticky;
	t->class = UDF_BASE_TYPE;
}

/* Link region lists. */
void xn_end(struct udf_type *t, size_t nbytes) {

	demand(lock, did not begin specification);
	lock = 0;
	t->u.t.nowns = cur_index;
	t->nbytes = cur_offset;
	demand(nbytes == cur_offset, bogus size!);
}

static void add_rwset(struct udf_type *t, size_t lb, size_t ub) {
	/* printf("adding [%ld, %ld)\n", lb, ub); */
	if(!u_add_rwset(&t->u.t.raw_read, lb, ub))
		fatal(Failed!);
	if(!u_add_rwset(&t->u.t.raw_write, lb, ub))
		fatal(Failed!);
}

static void 
add_dom(struct udf_type *t, int index, int base, int elsize, int nelem) {
	struct udf_inst *ip;
	struct udf_fun *f;
	int index_r, tmp, ub, boff;
	static struct udf_type *last_t;
	static int last_ub, last_elsize;
	static struct udf_inst *last_ip;

	f = &t->u.t.read_f;
	ip = f->insts +  f->ninst;

	index_r = 1; tmp = 2; 	/* hardcoded registers. */

	/* 
	 * if(index < ub) 
	 * 	return base + (index - lb) * elmsize;
	 */

	ub = index + nelem;
	boff = 5;	/* number of instructions to return index. */

	if(last_t == t && index == last_ub && elsize == last_elsize) {
		u_bgei(last_ip, index_r, ub, boff);
	} else {
		last_ip = ip;
		last_elsize = elsize;
		last_t = t;

		u_bgei(ip++, index_r, ub, boff);
			u_subi(ip++, tmp, index_r, index);
			u_muli(ip++, tmp, tmp, elsize);
			u_addi(ip++, tmp, tmp, base);
			u_add_cexti(ip++, tmp, elsize, XN_BYTES);
			u_reti(ip++, 1);

		f->ninst += boff + 1;
		demand(f->ninst < UDF_INST_MAX, too many instructions);
	}
	last_ub = ub;
}

static void 
add_ext(struct udf_type *t, int index, int base, int elsize, int nelem, int ty) {
	struct udf_inst *ip;
	struct udf_fun *f;
	int index_r, tmp, ub, boff;

	f = &t->u.t.owns;
	ip = f->insts +  f->ninst;
	boff = 5;
	index_r = 1; tmp = 2; 	/* hardcoded registers. */

	ub = index + nelem;

	/* 
	 *   if (index < ub)	
	 * 	add(meta + (index - lb) * elmsize + base);
	 */
	u_bgei(ip++, index_r, ub, boff);
		u_subi(ip++, tmp, index_r, index);
		u_muli(ip++, tmp, tmp, elsize);
		u_ldii(ip++, tmp, tmp, base);
		u_add_cexti(ip++, tmp, 1, ty);
		u_reti(ip++, 1);

	f->ninst += boff + 1;
	demand(f->ninst < UDF_INST_MAX, too many instructions);
}

/* just load the value. */
static void  type_field(udf_type *t, int offset, int elmsize) {
	struct udf_inst *ip;
	struct udf_fun *f;
	int tmp_r;

	f = &t->get_type;
	demand(!f->ninst, Can only have one type field!);
	ip = f->insts +  f->ninst;

	tmp_r = 1;
	switch(elmsize) {
	case 2: 	u_ldsi(ip++, tmp_r, U_NO_REG, offset); break;
	case 4: 	u_ldii(ip++, tmp_r, U_NO_REG, offset); break;
	default: 	fatal(not handling this size);
	}
	u_ret(ip++, tmp_r);

	f->ninst = ip - f->insts;
	demand(f->ninst < UDF_INST_MAX, too many instructions);

	if(!u_alloc_rwset(&t->type_access, 1))
		fatal(Should not happen);
	if(!u_add_rwset(&t->type_access, offset, offset+elmsize))
		fatal(Should not happen);
}

void xn_fixed_seq(struct udf_type *t, size_t offset, size_t elmsize, size_t nelem, int type) {
	size_t ub, lb;

	demand(cur_offset == offset, bogus offset!);
	lb = offset;
	ub = lb + elmsize * nelem;

	if(type == XN_BYTES) {
		add_rwset(t, lb, ub);

	/* Create a UDF to read the type field */
	} else if(type == XN_UNION) {
		demand(nelem == 1, more than one is meaningless);
		type_field(t, offset, elmsize); 
	} else {
		add_dom(t, cur_index, offset, elmsize, nelem);
		add_ext(t, cur_index, offset, elmsize, nelem, type);
		cur_index += nelem;
	}
	cur_offset += elmsize * nelem;
}

#endif /* XN */
