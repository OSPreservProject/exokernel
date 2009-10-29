
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

#include "udf.h"
#include "template.h"
#include "set.h"
#include "xn-struct.h"
#include "registry.h"
#include "demand.h"
#include <kernel.h>
#include <kexcept.h>

/* 
 * Give no access (udf's currently not allowed to write meta data).
 * They could, as long as either (1) they did not read this value ever,
 * or (2) we ran them again and rolled back --- this makes sure 
 * they did not spit out a bogus answer.
 */
static struct udf_ext no_access;

typedef int (*udf_chk)(Set old, Set new, struct xn_update u, void *state);
typedef int (*udf_funp)(Set s, struct xn_update u);

Set udf_run(void* meta, struct udf_fun *up, size_t index, struct udf_ext *r, struct udf_ext *w);
Set udf_incore_run(void* meta, struct udf_fun *up, size_t index, struct udf_ext *r, struct udf_ext *w);

int udf_checkpt(struct udf_ckpt *ck, void *meta, xn_op *op) {
	ck->n = 0;
	if((ck->n = op->m.n))
		return (ck->m = mv_read(meta, op->m.mv, ck->n)) != 0;
	else {
		ck->m = 0;  
		return 1;
	}
}

void udf_check_free(struct udf_ckpt *ck) {
        Arena_free(sys_arena);           
}

/* Given a UDF and index, zero out its read set. */
void udf_zero(void *meta, udf_fun *f, int index) {
	udf_ext access;
	struct udf_bl *v;
	int i;

 	udf_getset(&access, f, index);
	v = &access.v[0];

	for(i = 0; i < access.n; i++, v++)
		memset((char *)meta + v->lb, 0, v->ub - v->lb);
}
void udf_rollback(void *meta, struct udf_ckpt *ckpt) {
	mv_write(meta, ckpt->m, ckpt->n);
}

#if 0
static int udf_registry_miss;
#endif
static int udf_check_registry;


/* 
 * Free everything except for blocks that are not in core.
 *
 * Things would be a lot more efficient if we could just free each
 * disk block as it was encountered rather than storing it in
 * some intermediate data structure for later extraction.
 *
 * This whole deal is not particularly efficient.  Fuck.  They
 * could just give us a hint as to how many owns functions need
 * to be run.
 * 
 * Intermediate hacks: build up the set and then consume rather
 * than boundary cross.  
 *
 * Fast case: Don't modify any bytes, just free the blocks.
 */
xn_err_t udf_free_all(udf_type *t, void *meta) {
#if 0
	udf_ext all_access;
	int nowns, i, hint;
	udf_fun *owns;
	Set s;
	xn_err_t res;

	if(depth++ > 3)
		return XN_TOO_DEEP;

	demand(t->class == UDF_BASE_TYPE, bogus class);

	all_access.n = 1;
	all_access.v[0].lb = 0;
	all_access.v[0].ub = t->nbytes;

	nowns = t->u.t.nowns;
	owns = &t->u.t.owns;
	hint = nowns;

	udf_check_registry = 1;
	res = XN_SUCCESS;
	for(miss = 0, i = 0; i < hint; i++) {
		s = udf_run(meta, owns, i, &all_access, &all_access);

		if(udf_registry_miss) {
			res = XN_REGISTRY_MISS;
			udf_registry_miss = 0;
		} else if(s) {
			E e;
			struct xn_update T
			size_t bytes;
			udf_type *t0;

			/* 
			 * The pointers in these blocks have to be the
			 * entire deal. 
			 */
			for(e = l_first(s->set); e; e = l_next(e, next)) {
				t0 = type_lookup(e->type);
				demand(t0, must be in registry!);

				ensure(m0 = xr_backing(t0), XN_TYPE_MISS);
				bytes = e->nelem * e->nbytes;
				db_free(db, bytes_to_blocks(bytes));
			}

			/* Nuke this UDF's read set. */
			udf_zero(meta, owns, i);
		}

		if(s) 
			set_free();
	}
	udf_check_registry = 0;
	return res;
#endif
	return XN_SUCCESS;
}

/* 
 * XXX Should make the assumption that each type that is allocated 
 * is a true disk block.
 */

extern int xn_in_kernel;
inline int udf_read(udf_funp fun, int type, void *meta, xn_op *op) {
	size_t index;
	struct udf_fun *f, *read_f;
	struct udf_ext access;
	int res;

	/* hack since we subvert the type system */
	if(xn_in_kernel && xn_isspecial(type))
		return XN_SUCCESS;

	index = op->m.own_id;

	if(!(f = udf_lookup(&read_f, type, meta, index, op->m.cap)))
		return 0;

	udf_getset(&access, read_f, index);
	res = fun(udf_run(meta, f, index, &access, &no_access), op->u);

	set_free();
	return res;
}

/* Caller is responsible for rolling back changes. */
static inline int 
meta_transaction(udf_chk chk, int type, void *meta, xn_op *op, void *state) {
	int res;
	Set old_set, new_set;
	struct xn_m_vec *mv;
	size_t n, index;
	struct udf_fun *f, *read_f;
	struct udf_ext access;

	/* hack since we subvert the type system */
	if(xn_in_kernel && xn_isspecial(type))
		return XN_SUCCESS;

	if(!(f = udf_lookup(&read_f, type, meta, index = op->m.own_id, op->m.cap)))
		return XN_BOGUS_INDEX;

	mv = op->m.mv;
	n = op->m.n;

	/* Do access control for this function. */
	udf_getset(&access, read_f, index);

	old_set = udf_run(meta, f, index, &access, &no_access);
	/* set_print("old", old_set); */
	mv_write(meta, mv, n);
	new_set = udf_run(meta, f, index, &access, &no_access);
	/* set_print("new", new_set); */

	/* ensure: {old_set - b} == new_set */
	res = chk(old_set, new_set, op->u, state) ? 
		XN_SUCCESS : XN_TYPE_ERROR;

	/* Free data consumed by the modules. */
	set_free();

	return res;
}

/* Check that the given set is a subset of what is in the udf. */
int udf_contains(int type, void *meta, xn_op *op) {
	/* ensure that old == new */
	static inline int contains(Set s, struct xn_update u) {
		return set_issubset(s, set_single(u));
	}

	return udf_read(contains, type, meta, op);
}

/* ensure that old == new */
static inline int nop(Set old, Set new, struct xn_update u, void *unused) {
	return set_eq(old, new);
}

/* Modify meta, should not produce a difference in the access set. */
int udf_write(int type, void  *meta, xn_op *op) {
	return meta_transaction(nop, type, meta, op, 0);
} 

/* ensure that {old_set U b} == new_set */
static inline int add(Set old, Set new, struct xn_update u, void *unused) {
	return set_eq(set_union(old, set_single(u)), new);
}

/* Add exactly b to the set guarded by meta. */
int udf_alloc(int type, void  *meta, xn_op *op) {
	return meta_transaction(add, type, meta, op, 0);
}

/* ensure that  old_set - new = {db}  */
static inline int remove(Set old, Set new, struct xn_update u, void *nil) {
	return set_eq(set_diff(old, new), set_single(u));
}

/* Delete exactly b from set guarded by meta. */
int udf_dealloc(int type, void *meta, xn_op *op) {
	return meta_transaction(remove, type, meta, op, 0);
}

/* They don't actually have to say what to add, delete, right? */
static struct xstate { Set add; Set del; } state;

static inline int move(Set old, Set new, struct xn_update u, void *unused) {
	if(!set_issubset(old, state.del) || !set_isdisjoint(old, state.add))
		return 0;
	return set_eq(new, set_union(set_diff(old, state.del), state.add));
}
/* 
 * Use vector in o->m to add the blocks in o_add->u and delete those
 * in o_del->u.
 */
int udf_move(int type, void *meta, xn_op *op, xn_update *o_add, xn_update *o_del) {
	Set add, del;

	/* 
	 * make sure we have all of del, and none of add:
 	 * 	(old inter del = del) && (old inter add = nil)
	 * make sure add is in the final version and del is not:
 	 * 	(new inter add = add) && (new inter del = nil)
 	 * make sure we added exactly add and deleted exactly del:
	 * 	new = (old - del) U add:
	 */

	if(!(add = set_single(*o_add)) || !(del = set_single(*o_del)))
		return XN_CANNOT_ALLOC;
	state.add = add; state.del = del;
	return meta_transaction(move, type, meta, op, 0);
}

static unsigned meta[PAGESIZ];

int udf_type_init(struct udf_type *t) {
	return udf_get_union_type(t, meta);
}

xn_err_t 
udf_set_type(int ty, int exp, void *meta, size_t off, void *p, size_t nbytes) {
	char ckpt[32];
	udf_type *t;

        ensure(nbytes < sizeof ckpt,           XN_WRITE_FAILED);
	ensure(t = type_lookup(ty),		XN_TYPE_MISS);
	ensure(t->class == UDF_UNION,		XN_TYPE_ERROR);

	/* we will have to allow this to change. */
        ensure(XN_NIL == udf_get_union_type(t, meta), XN_TYPE_ERROR);
	ensure(u_in_set(&t->type_access, off, nbytes), XN_WRITE_FAILED);

	/* save state. */
        memcpy(ckpt, (char *)meta + off, nbytes);

	/* modify state. */
        memcpy((char *)meta + off, p, nbytes);

        if(exp == udf_get_union_type(t, meta))
		return XN_SUCCESS;

	/* restore state */
        memcpy((char *)meta + off, ckpt, nbytes);
	return XN_TYPE_ERROR;
}

/* 
 * Make sure that the before set is nil --- need to test on all
 * udf's. 
 */
int udf_init(struct udf_type *t) {
	struct udf_fun *owns;
	size_t nowns, i, nbytes;
	struct udf_ext r_access;


	nbytes = t->nbytes;
	nowns = t->u.t.nowns;
	owns = &t->u.t.owns;

	for(i = 0; i < nowns; i++) { 
		udf_getset(&r_access, &t->u.t.read_f, i);
		if(!set_empty(udf_run(meta, owns, i, &r_access, &no_access)))
			return -1;
	}
	return 0;
}

Set udf_access;


Set udf_run(void* meta, struct udf_fun *f, size_t index, struct udf_ext *r, struct udf_ext *w) {
	unsigned params[1];

        udf_access = set_new();
	params[0] = index;
	return !udf_interp(f, 0, meta, params, 1, r, w) ? 0 : udf_access;
}

static struct udf_ext *udf_extent;

static inline void ext_copy(struct udf_ext *dst, struct udf_ext *src) {
	int i, n;

	for(i = 0, n = src->n; i < n; i++)
		dst->v[i] = src->v[i];
	dst->n = n;
}

/* Extract the set from getset (can memoize for the moment). */
int udf_getset(struct udf_ext *e, struct udf_fun *f, int index) {
	unsigned params[1];
	int res;

	/* Used for memoized cache. */
	static int last_index;
	static struct udf_fun *last_f;
	static struct udf_ext ext;

	/* 
	 * Should we do this in a continuation passing style? 
	 * udf interp could return where it is and what value
 	 * it has for the instruction and we could then resume.
	 */

	/* memoize */
	if(index == last_index && f == last_f) {
		/* Yea, we hit: return old value. */
		ext_copy(e, &ext);
		return 1;
	}

	params[0] = index;
	memset(e, 0, sizeof *e);
	udf_extent = e;
	/* 
	 * Is currently a strictly algebraic computation: not allowed to
	 * look at meta data.
	 */
	res = udf_interp(f, 0, 0, params, 1, &no_access, &no_access);

	/* reset cache. */
	last_f = f;
	last_index = index;
	ext_copy(&ext, e);

	return res;
}

int udf_add_ext(size_t base, size_t len) {
	size_t n;
	struct udf_bl *v;

	n = udf_extent->n;
	if(n >= UDF_EXT_MAX)
		return 0;

	v = &udf_extent->v[n];
	udf_extent->n++;
	v->lb = base;
	v->ub = base + len;
	return 1;
}

int udf_get_union_type(udf_type *t, void *meta) {
 	return udf_interp(&t->get_type, 0, meta,
                        0, 0, &t->type_access, &no_access); 
}

/* Find what type is union really is. */
struct udf_type *udf_resolve_union(udf_type *t, void *meta) {
	int ty;

	ty = udf_get_union_type(t, meta);
	return !ty ? 0 : type_lookup(ty);
}

udf_fun *
udf_lookup(udf_fun **read_f, int type, void *meta, size_t own_id, cap_t c) {
	udf_type *t;

	if(!(t = type_lookup(type)))
		return 0;
	else if(t->class == UDF_UNION && !(t = udf_resolve_union(t, meta)))
		return 0;

	*read_f = &t->u.t.read_f;
	if(own_id >= t->u.t.nowns)
		return 0;
	return &t->u.t.owns;
}

void udf_add(db_t db, size_t n, int type) {
        int i;
        struct xn_update u;

	if(type == XN_BYTES) {
		i = udf_add_ext(db, n);
		demand(i, bogus i);
		return;
	} else if(udf_check_registry) {
		fatal(not here);
#if 0
		if(!xr_incore(db, n)) {
			udf_registry_miss = 1;	
			return;
		}
#endif
	}
		
        if(!db)
                return;
        u.nelem = n;
        u.type = type;

        for(i = 0; i < n; i++) {
                u.db = db + i;
                udf_access = set_union(udf_access, set_single(u));
        }
}

int udf_illegal_access(size_t offset, size_t nbytes, struct udf_ext *e) {
        return !u_in_set(e, offset, nbytes);
}

/* Should really be a method, right? */
int udf_blk_access(int op, struct xr *xr, void *meta, cap_t cap, size_t index) {
	struct udf_ctx ctx;
	struct udf_type *t;
	unsigned params[3];
 	int res;

	t = type_lookup(xr->td.type);
	ctx.segs[0] = xr_get_sticky(xr);
	params[0] = (unsigned)meta;
	params[1] = (unsigned)&cap;
	params[2] = (unsigned)index;
	res = udf_interp(&t->u.t.block_access, &ctx, meta, 
			params, 3, &t->u.t.raw_read, &t->u.t.raw_write);

	if(res == 0)
		return XN_BOGUS_CAP;
	else if(res == 1)
		return XN_SUCCESS;
	return res;
}

/* Caller needs to handle udf self, right?  Uch.  What a fucking mess. */
int 
udf_switch_seg(udf_ctx *ctx, int n, void **meta, udf_ext **r_access, udf_ext **w_access) {
	struct xr *xr;
	struct udf_type *t;

	if(!ctx || n >= (UDF_PARENT + UDF_MAX_SEG))
		return XN_BOGUS_CTX;
	
	n -= UDF_PARENT;

	ensure(xr = xr_lookup(da_to_db(ctx->segs[n])), XN_REGISTRY_MISS);
	ensure(t = type_lookup(xr->td.type), XN_TYPE_MISS);

	/* We are not handling mutiple types here, at all. */
	*meta = (char *)xr_backing(xr) + da_to_offset(ctx->segs[n]); 
	*r_access = &t->u.t.raw_read;
	*w_access = &t->u.t.raw_write;
	return XN_SUCCESS;
}
