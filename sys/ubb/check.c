
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

/* XXX: Checking is currently incredibly weak.  Go through and redesign. */
#ifdef EXOPC
#include "kernel.h"
#else
#include <stdio.h>
#include <memory.h>
#endif

#include "ubb.h"
#include "udf.h"
#include "ubb-inst.h"
#include "bit.h"
#include "demand.h"
#include "kexcept.h"
#include "template.h"

void udf_tprint(struct udf_type *t) ;

#define dprint printf

/* For simplicity, only look for subsets. */
int u_in_set(struct udf_ext *s, size_t lb, size_t len) {
	int i, n;
	size_t ub;

	for(n = s->n, i = 0, ub = lb + len - 1; i < n; i++)
		if(lb >= s->v[i].lb && ub <= s->v[i].ub)
			return 1;
	/* dprint("[%ld, %ld) is not in set\n", lb, ub); */
	return 0;
}

int u_set_eq(udf_ext *s1, udf_ext *s2) {
	struct udf_bl *v1, *v2;
	int i;

	if(s1->n != s2->n)
		return 0;

	for(i = 0; i < s1->n && i < UDF_EXT_MAX; i++) {
		v1 = &s1->v[i];
		v2 = &s2->v[i];

		if(v1->lb != v2->lb || v1->ub != v2->ub)
			return 0;
	}
	return i < UDF_EXT_MAX;
}

static int valid_op(unsigned op) {
	if(u_valid_op(op))
		return 1;
	dprint("Bogus op %d\n", op);
	return 0;
}

static int valid_reg(unsigned r) {
	if(u_valid_reg(r))
		return 1;
	dprint("Bogus reg %d\n", r);
	return 0;
}

/* 
 * Rewrite user code to not do bad things.  For the moment, we restrict
 * their operations hard: no indirect loads, and no loops.   As a result,
 * we can statically check all operations.
 */
int udf_check(struct udf_fun *f) {
	struct udf_inst *ip, *inst;
	int i, n;
	unsigned rd, rs1, rs2, op;
	Bit_T reg_set;
	struct udf_fun t;	

	ip = t.insts;
	n = f->ninst;

	memcpy(ip, f->insts, n * sizeof *ip);

	reg_set = Bit_new(U_REG_MAX);
	/* udf_dis(ip, n); */

	/* Check values and registers, and accumulate regs (for init). */
	for(i = 0; i < n; i++) {
		op = U_OP(&ip[i]);
		rd = U_RD(&ip[i]);
		rs1 = rd /* U_RS1(&ip[i]) */;
		rs2 = U_RS2(&ip[i]);

		if(!valid_op(op))
			goto error;
		if(!valid_reg(rd))
#if 0
		XXXX
		if(!u_is_branch(op) && !valid_reg(rs1)) 
			goto error;
#endif
		if(!u_is_imm(op) && !valid_reg(rs2))
			goto error;

		/* record regs. */
		Bit_set(reg_set, rd, rd);
#if 0
		Bit_set(reg_set, rs1, rs1);
#endif
		if(!u_is_imm(op))
			Bit_set(reg_set, rs2, rs2);

		/* Statically check memory references. */
		switch(op) {
		case U_LDII: case U_LDSI: case U_LDBI:
			break;
		case U_STII: case U_STSI: case U_STBI:
			fatal(Illegal ops);
			break;
		case U_LDI: case U_LDS: case U_LDB:
		case U_STI: case U_STS: case U_STB:
			dprint("Bogus op, not handling indirect mem!\n"); 
			goto error;
		}
	}
	
	/* Copy checked instructions to in-kernel buffer. */
	Bit_clear(reg_set, U_NO_REG, U_NO_REG);

	inst = f->insts;

	/* zero out all registers that are used. */
	for(i = f->nargs+1; i < U_REG_MAX; i++)
		if(Bit_get(reg_set, i)) {
			u_movi(inst++, i, 0);
			f->ninst++;
		}
	f->ninst++; 	/* for return. */
	memcpy(inst, ip, sizeof *ip * n);
	inst += n;
	u_reti(inst, 1);
	Bit_free(&reg_set);

	/* udf_dis(ip, n); */ 
	return 1;

error:
	dprint("Failed on inst %d\n", i);
	Bit_free(&reg_set);
	return 0;
}

static int extract_inset(Bit_T s, struct udf_ext *rws, size_t nbytes) {
	int i,n;
	size_t lb, ub;
	struct udf_bl *v;

	n = rws->n;
	v = rws->v;
	for(i = 0; i < n; i++, v++) {
		lb = v->lb;
		ub = v->ub;
		/* dprint("lb=%d, ub = %d\n", lb, ub); */
		if(lb > ub || ub > nbytes)
			return 0;

		Bit_set(s, lb, ub - 1);
	}
	return 1;
}
static int extract_f_inset(Bit_T s, struct udf_fun *f, int i, size_t nbytes) {
	struct udf_ext ext;

	memset(&ext, 0, sizeof ext);
	udf_getset(&ext, f, i);
	/* dprint("extracting %d\n", i); */
	return extract_inset(s, &ext, nbytes);
}

static int check_base_type(udf_type *t) {
	int i, overlap, res, nowns, nbytes;
	struct udf_fun *owns;
	Bit_T rwset, rw, tset;

	nbytes = t->nbytes;
	nowns = t->u.t.nowns;
	owns = &t->u.t.owns;

	rwset = Bit_new(nbytes);
	tset  = Bit_new(nbytes);
	res = 0;

	if(!udf_check(owns))
		goto end;
#ifdef DRE
	printf("reads\n");
	udf_dis(t->u.t.read_f.insts, t->u.t.read_f.ninst);

	printf("nowns = %d\n", nowns);
	udf_dis(t->u.t.owns.insts, t->u.t.owns.ninst);
#endif

	for(rw = 0, i = 0; i < nowns; i++) {
		/* Accumulate read and write sets. */
		if(!extract_f_inset(tset, &t->u.t.read_f, i, nbytes))
			goto end;
		rw = Bit_union(rwset, tset);

		/* non-zero = non-empty overlap. */
		overlap = Bit_count(rw) != (Bit_count(rwset) + Bit_count(tset));

		/* annoying cleanup. */
		Bit_clear(tset, 0, nbytes - 1);
		Bit_free(&rwset);
		rwset = rw;

		if(overlap) {
			printf("overlap failed %d\n", i);
			goto end;
		}
	}

	/* Cannot be any overlap between the raw and rwset. */
	Bit_clear(tset, 0, nbytes-1);
	if(!extract_inset(tset, &t->u.t.raw_write, nbytes))
		goto end;
#if 0
	if(!extract_inset(tset, &t->u.t.raw_read, nbytes))
		goto end;
#endif
	/* intersection must be nil. */
	if(!Bit_overlap(rwset, tset))
		res = 1;

	udf_init(t); 
end:
	Bit_free(&tset);
	Bit_free(&rwset); 
	return res;
}

/* Only the union type-check function needs to be valid. */
static int check_union(udf_type *t) {
	int i, ty, t_init, n;
	struct udf_struct *u;
	udf_type *t0, *t1;

	t_init = udf_type_init(t);

	check(t_init == XN_NIL, t_init must be nil, 0);
	check(udf_check(&t->get_type), bogus get_type, 0);
	u = &t->u.u;
	n = u->ntypes;
	check(n > 0 && n < UDF_MAX_TYPES, 
		type->nbytes 0 or too large check_union, 0);

	/* Currently force types to be user-defined, right? */
	for(t0 = t, i = 0, u = &t->u.u; i < n; i++) {
		ty = u->ty[i];

		check(xn_user_elem(ty), must be a user defined type, 0);
		check(t1 = type_lookup(ty), missed in registry, 0);

		/* type field must be in the same place for all of these. */
		check(u_set_eq(&t0->type_access, &t1->type_access),
			type read sets must be identical, 0);

		/* 
		 * Have to be the same size.  (XXX Probably only need to
		 * be <=, right?).
		 */
		check(t->nbytes == t1->nbytes, Bytes must be identical, 0);
	}
	return 1;
}

/* Collapse all nested structs into their constituent parts. */
static int check_struct(udf_type *t) {
	int i, ty, n, j, en;
	struct udf_struct *s, s1, *e;
	udf_type *t0;
	
	s = &t->u.s;
	n = s->ntypes;
	check(n > 0 && n < UDF_MAX_TYPES, 
		type->nbytes 0 or too large check_struct, 0);
	
	/* Currently force types to be user-defined, right? */
	for(j = i = 0; i < n; i++) {
		ty = s->ty[i];
		check(xn_user_elem(ty), must be a user defined type, 0);
		check(t0 = type_lookup(ty), missed in registry, 0);

		if(t0->class != UDF_STRUCT) {
			s1.ty[j] = ty;
			s1.t[j] = t0;
			j++;
		} else {
			/* Collapse nested structures. */
			e= &t0->u.s;
			en = e->ntypes;
			check((en + j) < UDF_MAX_TYPES, too many types, 0);
			memcpy(&s1.t[j], &e->t[0], sizeof e->t[0] * en);
			memcpy(&s1.ty[j], &e->ty[0], sizeof e->ty[0] * en);
			j += en;
		}
	}
	*s = s1;
	s->ntypes = j;
	return 1;
}

/* 
 * Ensure read/write sets are non-overlapping and that UDF's are well-formed.
 * (still incomplete).  
 */
static int check_methods(struct udf_type *t) {
	switch(t->class) {
	case UDF_BASE_TYPE: 	return check_base_type(t);
	case UDF_UNION:		return check_union(t);
	case UDF_STRUCT:	return check_struct(t);
	default: return 0;
	}	
}

int udf_type_check(struct udf_type *t) {
	size_t nbytes, n, i;

	/* udf_tprint(t); */

	check(nbytes = t->nbytes, type has no bytes!, 0);
	t->u.t.owns.nargs = 1;
	t->u.t.read_f.nargs = 1;
	if(!check_methods(t))
		return 0;

	for(i = 0, n = sizeof(t->type_name); i < n; i++) 
		if(!t->type_name[i])
			break;
	return i < n;
}

static void print_rwset(char *str, struct udf_ext *s) {
        int i, n;

        n = s->n;
        printf("  set <%s> = %d\n", str, n);

        for(i = 0;  i < n; i++)
                printf("\t%d = [%d, %d)\n", i, s->v[i].lb, s->v[i].ub);
}

#if 0
static void print_f_rwset(char *str, int index, struct udf_fun *f) {
	struct udf_ext s;

	return;

	/* Want to run to derive the read set, right? */
	udf_getset(&s, f, index);
	print_rwset(str, &s);
}
#endif

void udf_tprint(struct udf_type *t) {
	int n, i;
	struct udf_fun *owns;
	struct udf_struct *s;

	printf("type %s: nbytes = %d\n", t->type_name, t->nbytes);
	
	print_rwset("Raw read set",  &t->u.t.raw_read);
	print_rwset("Raw write set", &t->u.t.raw_write);

	printf("Access control for bytes\n");
	udf_dis(t->u.t.byte_access.insts, t->u.t.byte_access.ninst);

	printf("Access control for blocks\n");
	udf_dis(t->u.t.block_access.insts, t->u.t.block_access.ninst);

	print_rwset("type access",  &t->type_access);
	printf("get type = %d\n", t->get_type.ninst);
	udf_dis(t->get_type.insts, t->get_type.ninst);

	switch(t->class) {
	case UDF_BASE_TYPE:
		printf("Indexed dis\n");
		udf_dis(t->u.t.read_f.insts, t->u.t.read_f.ninst);
		owns = &t->u.t.owns;
		n = t->u.t.nowns;
		printf("  own methods: %d\n", n);
		udf_dis(owns->insts, owns->ninst);
		break;
	case UDF_STRUCT:
		for(i = 0, s = &t->u.s; s->t[i]; i++)
			printf("struct[%d] = %s, nbytes %d\n", 
				i, s->t[i]->type_name, s->t[i]->nbytes);
		break;
	case UDF_UNION:
		for(i = 0, s = &t->u.s; s->t[i]; i++)
			printf("union[%d] legal %s, nbytes %d\n", 
				i, s->t[i]->type_name, s->t[i]->nbytes);
		break;
	default: fatal(Bogus type);
	}
}

