
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
 * XOK hack: to keep dependencies, we set parent's to dirty on modification.
 *
 * Does registry track blocks or entities? or both? 
 *
 * Simplifying restriction: types only guarded by limited set of caps. 
 *
 * Can overlay methods to restrict authorization for other 
 * 	than byte-read/write. 
 *
 * Interfaces would profit from modifications for speed.
 *
 * We should probably use alloc(db, n, t) free(db, n, t) move(db, n, t)
 * methods that modify the disk block directly.  This would clean up
 * the inerfaces a bit, since these functions only take in the index
 * as a parameter, and let us do the operations without user assistence.
 * A minor secondary benefit is that the modifications can be statically
 * checked for violations, thereby eliminating runtime overhead.

 *
 * Locking matters:
 *	1. for modifications.
 *	2. for writes (no double write).
 *	3. for reads (no read over other reads).  <--- should not happen.
 */
#include "xn-struct.h"
#include "demand.h"
#include "template.h"
#include "kexcept.h"
#include "kernel.h"
#include "registry.h"
#include "virtual-disk.h"
#include "type.h"
#include "iov.h"
#include "udf.h"

#include <xok/bc.h>
#include <xok/sys_proto.h>
#include <xok/pmap.h>
#include <xok/sysinfo.h>


void scankids(char *, struct xr *);
Arena_T sys_arena;

/* globals filled in by pmap.c with pointers to staticly allocated memory */

struct xr* xn_registry;
char* xn_free_map;

void xn_io_done (u32 dev, u32 blk, u32 len, int write) {
	if(blk && dev == XN_DEV)
		xr_post_io(blk, len, write);
}

/* All objects are block sized or larger, right? */

/* 
 * We should really just keep a damn pointer to the type name, rather than
 * this other nonsense.
 */
static xn_elem_t type_extract(xn_elem_t ty, size_t offset, int *not_exact) {
	udf_type *t;	
	size_t coff, i;
	udf_struct *s;

	t = type_lookup(ty);
	switch(t->class) {
	case UDF_UNION:			/* nothing to do, really. */
	case UDF_BASE_TYPE:
		if(not_exact)	
			*not_exact = offset;
		else 
			demand(!offset, offset should be zero);
		return ty;
	case UDF_STRUCT:
		for(s = &t->u.s, coff = i = 0; i < s->ntypes; i++) {
			demand(s->t[i]->nbytes, Bogus bytes);
			if(coff == offset)
				return s->ty[i];
			else
				coff += s->t[i]->nbytes;

			/* compute offset within the type. */
			if(not_exact && offset < coff) {
				coff -= s->t[i]->nbytes;
				*not_exact = offset - coff;
				return s->ty[i];
			}
		}
		sys_return(XN_BOGUS_TYPE);
	default: 
		fatal(Bogus type);
	}
	return -1; /* SHUTUP -Wall. */
}

#define build_env(chk_early) do {					\
        size_t size, nelem;						\
        xn_elem_t type;							\
									\
        ensure(xn_in_kernel || read_accessible(op, sizeof *op),		\
	       XN_CANNOT_ACCESS);					\
	db  = op->u.db;							\
	if(chk_early && xr_lookup(db))					\
		sys_return(XN_SUCCESS);					\
        ensure(p = xr_lookup(da_to_db(da)), 	XN_REGISTRY_MISS);	\
        ensure((meta = xr_index(da, p)),	XN_NOT_IN_CORE);	\
	try(ty = type_extract(p->td.type, da_to_offset(da), 0));	\
									\
        type = op->u.type;						\
        nelem = op->u.nelem;						\
        try(size = type_size(type));					\
        td = xr_type_desc(type, size, db, nelem);			\
        nblocks = bytes_to_blocks(nelem * size); 			\
} while(0)

int checkpt(struct udf_ckpt *ck, void *meta, size_t size, struct xn_op *op) {
	return (mv_access(size, op->m.mv, op->m.n) < 0) ?
		0 : udf_checkpt(ck, meta, op);
}

	/* types must be divisible by sector sizes or divide into sectors */

/* 
 * If block grows, need to ensure that the base is aligned and that
 * we update any entries in the buffer cache.
 */

/*
 * Restrictions: in core types must be completely contained, and must
 * be divisible into or by the sector size.  Additionally, we need to
 * check what is incore and what can be written. 
 */
	/* Currently we force the entire thing to be incore (?) */

/* 
 * Returns the offset of the meta data for the given pointer; must
 * be contigous, right?
 */
static void *xr_index(da_t da, struct xr *xr) {
	void *meta;

	if((meta = xr_backing(xr)))
		meta = (char *)meta + da_to_offset(da);
	return meta;
}
#define op_index(op) ((op)->m.own_id)
#define op_cap(op) ((op)->m.cap)
#define op_db(op) ((op)->u.db)
#define op_nelem(op) ((op)->u.nelem)
#define op_type(op) ((op)->u.type)

static sec_t off_to_sec(da_t da, struct xn_modify *m) {
	sec_t sec;
	int i;
	size_t off;

	if(!m->n)
		return da_to_sec(da);
	
	sec = da_to_sec(da + m->mv[0].offset);

	for(i = 0; i < m->n; i++) {
		off = da + m->mv[i].offset;
		demand(sec == da_to_sec(off), Mod cannot span sec);
		off += m->mv[i].nbytes - 1;
		demand(sec == da_to_sec(off), Modification cannot span sec);
	}

	return sec;
}

xn_err_t sys_xn_alloc(da_t da, struct xn_op *op, int nilp) {
        struct xr *p;
        struct xr_td td;
        size_t nblocks;
        db_t db;
	struct udf_ckpt chkpt;
	void *meta;
	int ty;
	sec_t sec;

        /* Preconditions. */
	build_env(0);
	ensure(!p->locked,				XN_LOCKED);
	if(db)
		ensure(db_isfree(db, nblocks),		XN_CANNOT_ALLOC);
	else {
		ensure(db = db_find_free(db, nblocks), 	XN_CANNOT_ALLOC);
		td.base = db;
		ensure(xn_in_kernel ||
		       write_accessible(&op->u.db, sizeof op->u.db), 
						XN_CANNOT_ACCESS);
		op->u.db = db;
	}

	demand(db, Bogus db);
	ensure(nblocks,	XN_BOGUS_NAME);
	ensure(checkpt(&chkpt, meta, p->td.size, op), XN_CANNOT_ACCESS);

        /* Commit happens in the middle of this routine. */
	try_s(udf_alloc(ty, meta, op), udf_rollback(meta, &chkpt));
	sec = off_to_sec(da, &op->m);
	demand(sec == da_to_sec(da), Bogus sec);
	demand(sec, bogus sector);
	try_s(xr_alloc(db, nblocks, td, da, sec, 1), udf_rollback(meta,&chkpt));

        if(db_alloc(db, nblocks) < 0)
		fatal(Cannot happen);
	xr_set_dirty(p);

	sys_return(XN_SUCCESS);
}

/* This is a temporary interface to allow non-XN apps to co-exist with  */
/* XN (and, therefore, to allocate and free using its freemap).  If and */
/* when XN is complete and dubbed "the one true way", this should be    */
/* eliminated and everyone should be forced to comply...                */
xn_err_t sys_xn_alloc_cheat(uint sn, da_t da, struct xn_op *op, int nilp) {
        //struct xr *p;
        struct xr_td td;
        size_t nblocks;
        db_t db;
	//struct udf_ckpt chkpt;
	//void *meta;
	//int ty;
	//sec_t sec;
	size_t size;

        /* Preconditions. */
	//build_env(0);
	db  = op->u.db;
        try(size = type_size(op->u.type));
        nblocks = bytes_to_blocks(op->u.nelem * size);
	//ensure(!p->locked,				XN_LOCKED);
	if(db)
		ensure(db_isfree(db, nblocks),		XN_CANNOT_ALLOC);
	else {
		ensure(db = db_find_free(db, nblocks), 	XN_CANNOT_ALLOC);
		td.base = db;
		ensure(xn_in_kernel ||
		       write_accessible(&op->u.db, sizeof op->u.db), 
						XN_CANNOT_ACCESS);
		op->u.db = db;
	}

	demand(db, Bogus db);
	ensure(nblocks,	XN_BOGUS_NAME);
#if 0 /* no metadata update for cheat routine -- block just disappears */
	ensure(checkpt(&chkpt, meta, p->td.size, op), XN_CANNOT_ACCESS);

        /* Commit happens in the middle of this routine. */
	try_s(udf_alloc(ty, meta, op), udf_rollback(meta, &chkpt));
	sec = off_to_sec(da, &op->m);
	demand(sec == da_to_sec(da), Bogus sec);
	demand(sec, bogus sector);
	try_s(xr_alloc(db, nblocks, td, da, sec, 1), udf_rollback(meta,&chkpt));
#endif

        if(db_alloc(db, nblocks) < 0)
		fatal(Cannot happen);
#if 0 /* no metadata update for cheat routine -- block just disappears */
	xr_set_dirty(p);
#endif

	sys_return(XN_SUCCESS);
}

xn_err_t free_all(udf_type *t, void *meta) {
	int i, ty;
	struct udf_struct *s;

	switch(t->class) {
	case UDF_UNION:
		if((ty = udf_get_union_type(t, meta)) == XN_NIL)
			return XN_SUCCESS;
		ensure(t = type_lookup(ty), XN_TYPE_MISS);
		/*FALLTHROUGH*/
	case UDF_BASE_TYPE:
		return udf_free_all(t, meta);

	/* only one struct, and we are done. */
	case UDF_STRUCT:
		s = &t->u.s;
		for(i = 0; i < s->ntypes; i++) {
			t = s->t[i];
			try(udf_free_all(t, meta));
			meta = (char *)meta + t->nbytes;
		}
		return XN_SUCCESS;
	default: fatal(Impossible type);
	}
}
/* 
 * Free all the given blocks -- need the cap: note [db, db+nblocks)
 * is a compacted sequence of types.  The base db must describe
 * what the sequence is, right?  Do we allow multiple sequences?
 * Additionally, the types are uniform, right?
 */
xn_err_t xn_free_all(db_t db, size_t nelem, xn_elem_t ty) {
	void *meta;
	udf_type *t;
	struct xr *xr;

	/* 
	 * Has nothing to free; should really check has internal 
	 * pointers.  
	 */
	if(ty == XN_DB)
		return XN_SUCCESS;
	
	ensure(xr = xr_lookup(db), 		XN_REGISTRY_MISS);
	try(xr_bind_bc(xr));
	ensure(t = type_lookup(ty),		XN_TYPE_MISS);
	demand(nelem == 1, I have not tested other values.);

	meta = xr_backing(xr);
	return free_all(meta, t);
}


xn_err_t sys_xn_free(da_t da, struct xn_op *op, int nilp) {
        struct xr *p;
        size_t nblocks;
        db_t db;
	struct udf_ckpt chkpt;
	void *meta;
	struct xr_td td;
	xn_elem_t ty;

       	/* Preconditions. */
	build_env(0);
//      Greg says, "You don't need to check that."
//	ensure(!p->locked,				XN_LOCKED);
	ensure(checkpt(&chkpt, meta, p->td.size, op), 	XN_CANNOT_ACCESS);
        ensure(db > 0,					XN_BOGUS_NAME);
	ensure(nblocks,					XN_TOO_SMALL);
	ensure(!db_isfree(db, nblocks),			XN_CANNOT_FREE);
	ensure((db+nblocks) <= SYSINFO_GET_AT(si_xn_blocks,0), XN_CANNOT_FREE);

        /* Commit. */
	try_s(udf_dealloc(ty, meta, op), udf_rollback(meta, &chkpt));
	try_s(xr_delete(db, nblocks), udf_rollback(meta, &chkpt));
	xr_set_dirty(p);

	sys_return(XN_SUCCESS);
}


/* This is a temporary interface to allow non-XN apps to co-exist with  */
/* XN (and, therefore, to allocate and free using its freemap).  If and */
/* when XN is complete and dubbed "the one true way", this should be    */
/* eliminate and everyone should be forced to comply...                 */
xn_err_t sys_xn_free_cheat(uint sn, da_t da, struct xn_op *op, int nilp) {
        //struct xr *p;
        size_t nblocks;
        db_t db;
	//struct udf_ckpt chkpt;
	//void *meta;
	//struct xr_td td;
	//xn_elem_t ty;
	size_t size;

       	/* Preconditions. */
	//build_env(0);
	db  = op->u.db;
        try(size = type_size(op->u.type));
        nblocks = bytes_to_blocks(op->u.nelem * size);
//      Greg says, "You don't need to check that."
//	ensure(!p->locked,				XN_LOCKED);
//	ensure(checkpt(&chkpt, meta, p->td.size, op), 	XN_CANNOT_ACCESS);
        ensure(db > 0,					XN_BOGUS_NAME);
	ensure(nblocks > 0,				XN_TOO_SMALL);
	ensure(!db_isfree(db, nblocks),			XN_CANNOT_FREE);
	ensure((db+nblocks) <= SYSIFO_GET_AT(si_xn_blocks,0), XN_CANNOT_FREE);

        /* Commit. */
#if 0 /* no metadata update for cheat routine -- block just disappears */
	try_s(udf_dealloc(ty, meta, op), udf_rollback(meta, &chkpt));
	try_s(xr_delete(db, nblocks), udf_rollback(meta, &chkpt));
	xr_set_dirty(p);
#endif
        if(db_free(db, nblocks) < 0)
		fatal(Cannot happen);

	sys_return(XN_SUCCESS);
}

static void update_parent(da_t p, db_t db, size_t nelem, size_t nbytes) {
	size_t n;
	struct xr *k;
	int i;

	n = (nelem * nbytes) / XN_BLOCK_SIZE;

	for(i = db; i < (db + n); i++)
		if((k = xr_lookup(i)))
			k->p = p;
}

xn_err_t sys_xn_swap(da_t v1, struct xn_op *op1, da_t v2, struct xn_op *op2) {
        struct xr *p1, *p2;
	void *meta1, *meta2;
        size_t size1, size2;
	int res;
	struct udf_ckpt chkpt1, chkpt2;

       /* Preconditions. */

	ensure(read_accessible(op1, sizeof *op1), XN_CANNOT_ACCESS);
	ensure(read_accessible(op2, sizeof *op2), XN_CANNOT_ACCESS);

        /* Get in core block rep. */
        ensure(p1 = xr_lookup(da_to_db(v1)), XN_REGISTRY_MISS);
        ensure(meta1 = xr_index(v1, p1),     XN_NOT_IN_CORE);

        ensure(p2 = xr_lookup(da_to_db(v2)), XN_REGISTRY_MISS);
        ensure(meta2 = xr_index(v2, p2),     XN_NOT_IN_CORE);

	size1 = p1->td.size;
	ensure(checkpt(&chkpt1, meta1, size1, op1), XN_CANNOT_ACCESS);

	size2 = p2->td.size;
	ensure(checkpt(&chkpt2, meta2, size2, op2), XN_CANNOT_ACCESS);

	/* We don't know where to write to roll back, right? */
	if((res = udf_move(p1->td.type, meta1, op1, &op2->u, &op1->u)) < 0) {
		udf_rollback(meta1, &chkpt1);
		sys_return(res);
	} else if((res = udf_move(p2->td.type, meta2, op2, &op1->u, &op2->u)) < 0) {
		udf_rollback(meta1, &chkpt1);
		udf_rollback(meta2, &chkpt2);
		sys_return(res);
	}

	/* Need to swap parent pointers blocks pointed to by p1/p2. */
	update_parent(v1, op2->u.db, op2->u.nelem, type_size(op2->u.type));
	update_parent(v2, op1->u.db, op1->u.nelem, type_size(op1->u.type));

	sys_return(XN_SUCCESS);
}

/* 
 * Essentially simply free the disk block in src and allocate
 * it in dst.
 */
xn_err_t 
sys_xn_move(da_t dst, struct xn_op *msrc, da_t src, struct xn_op *mdst) {
	fatal(Unimplemented);
	sys_return(XN_SUCCESS);
}

/* Check that raw writes are allowed to [da, da + nbytes). */
xn_elem_t xr_get_wptr(struct xr *p, da_t da, size_t nbytes, cap_t cap) {
	int toff, ty;

	try(ty = type_extract(p->td.type, da_to_offset(da), &toff));
	try(type_writable(ty, cap, toff, nbytes));
	return XN_SUCCESS;
}

/* Check that raw writes are allowed to [da, da + nbytes). */
xn_elem_t xr_readable(struct xr *p, da_t da, size_t nbytes, cap_t cap) {
	int toff, ty;

	try(ty = type_extract(p->td.type, da_to_offset(da), &toff));
	try(type_readable(ty, cap, toff, nbytes));
	return XN_SUCCESS;
}

/* This can only be applied to data that has no pointers. */
xn_err_t sys_xn_writeb(da_t da, void * src, size_t nbytes, cap_t cap) {
	struct xr *p;
	void *meta;

	ensure(read_accessible(src, nbytes), 	XN_CANNOT_ACCESS);
        ensure(p = xr_lookup(da_to_db(da)), 	XN_REGISTRY_MISS);
	ensure(meta = xr_backing(p), 		XN_NOT_IN_CORE);
	try(xr_get_wptr(p, da, nbytes, cap));

	memcpy((char *)meta + da_to_offset(da), src, nbytes);
	xr_set_dirty(p);

	sys_return(XN_SUCCESS);
}

/* Set field to determine union.  Must be nil, first, then the given type. */
xn_err_t 
sys_xn_set_type(da_t da, int expected_ty, void *src, size_t nbytes, cap_t cap) {
	struct xr *p;
	void *meta;
	int ty, toff, off;

	ensure(read_accessible(src, nbytes), 	XN_CANNOT_ACCESS);
        ensure(p = xr_lookup(da_to_db(da)), 	XN_REGISTRY_MISS);
	ensure(meta = xr_backing(p), 		XN_NOT_IN_CORE);
	try(ty = type_extract(p->td.type, da_to_offset(da), &toff));

	/* toff = offset within type. */

	off = da_to_offset(da) - toff;	/* beginning of type. */
	meta = (char *)meta + off;	/* beginning of type */
	try(udf_set_type(ty, expected_ty, meta, toff, src, nbytes));
	xr_set_dirty(p);

	sys_return(XN_SUCCESS);
}

/* Read out bytes. */
xn_err_t sys_xn_readb(void * dst, da_t da, size_t nbytes, cap_t cap) {
	struct xr *p;
	void *meta;

	ensure(write_accessible(dst, nbytes), 	XN_CANNOT_ACCESS);
        ensure(p = xr_lookup(da_to_db(da)), 	XN_REGISTRY_MISS);
	ensure(meta = xr_backing(p), 		XN_NOT_IN_CORE);
	try(xr_readable(p, da, nbytes, cap));

	memcpy(dst, (char *)meta + da_to_offset(da), nbytes);
	sys_return(XN_SUCCESS);
}

/* 
 * XXX:
 * We are assuming no paging at the moment.  Otherwise, how to 
 * deal with blocks that are in the holding area?  Do we want
 * to allow reference counts?
 */

/* Should just bind it in, unless it is nil. */

/* Protocol for writing: fetch it in core, may not be bound to parent. */

/* Should have read/write flags? */

/* Probably should make sure the block is legal, eh? */
xn_err_t sys_xn_readin(db_t db, size_t nblocks, xn_cnt_t *cnt) {
	ensure(db,		XN_BOGUS_NAME);
	ensure(nblocks,		XN_TOO_SMALL);
  	if (cnt)
    		ensure((cnt = valid_user_ptr(cnt)), XN_CANNOT_ACCESS);
  	return xr_bread(db, nblocks, cnt);
}

xn_err_t sys_xn_writeback(db_t db, size_t nblocks, xn_cnt_t *cnt) {
	ensure(db,		XN_BOGUS_NAME);
	ensure(nblocks,		XN_TOO_SMALL);
  	if (cnt)
    		ensure((cnt = valid_user_ptr(cnt)), XN_CANNOT_ACCESS);
  	return xr_bwrite(db, nblocks, cnt);
}

/* 
 * Bind to a page; note that you can bind in pages that have data,
 * thereby eliminating any copy into the buffer cache.
 */
xn_err_t sys_xn_bind(db_t db, ppn_t ppn, cap_t cap, xn_bind_t flag, int alloc) {
        struct xr *p;
	int r;

	/* Need to clean states up. */
        ensure(p = xr_lookup(db), 		XN_REGISTRY_MISS);
	ensure(xr_bind_bc(p) != XN_SUCCESS,	XN_IN_CORE);
	ensure(db,				XN_BOGUS_PTR);

	if(flag == XN_ALLOC_ZERO)
		flag = p->initialized ?  XN_FORCE_READIN : XN_ZERO_FILL;
		
	switch(flag) {
	case XN_ZERO_FILL:
	case XN_BIND_CONTENTS:
		if(p->initialized && !xr_cap_check(XN_WRITE, p, cap))
			sys_return(XN_BOGUS_CAP);
		break;
        case XN_FORCE_READIN:
        default:
		sys_return(XN_ILLEGAL_OP);
	} 

	/* 
	 * We allow you to bind uninitialized data blocks, but
	 * not meta data. 
	 */
        if(!alloc) {
               	if (flag != XN_ZERO_FILL)
               		ensure(!p->internal_ptrs,    XN_ILLEGAL_OP);

                ensure(pcheck(ppn, 1, cap), 	XN_CANNOT_ALLOC);
                p->bc = bc_insert (XN_DEV, p->db, ppn, 0, &r);
                p->page = ppn_to_phys(ppn);
                demand (p->bc, BC state fluctuating during syscall);
        } else {
		demand(!p->bc, Page and bc out of sync);
		p->bc = bc_alloc_entry(XN_DEV, p->db, &r);
		p->page = bc_va(p->bc);
		ensure(p->bc, XN_CANNOT_ALLOC);
        }

	bc_set_dirty (p->bc);
	if(flag == XN_ZERO_FILL)
	  	memset(p->page, 0, PAGESIZ);

        return(p->bc->buf_ppn);
}

/* 
 * Unbind a page: ppn is optional (This should be called by xn_free). 
 * Used as an aggressive hint or to test reading from disk (write
 * data, flush page, read back in).
 */
xn_err_t sys_xn_unbind(db_t db, ppn_t h_ppn, cap_t cap) {
        struct xr *p;
	ppn_t ppn;

        ensure(p = xr_lookup(db), 		XN_REGISTRY_MISS);
	try(xr_bind_bc(p));
        ensure(xr_cap_check(XN_WRITE, p, cap), 	XN_BOGUS_CAP);

	ppn = phys_to_ppn(p->page);
	ensure(h_ppn < 0 || ppn == h_ppn,	XN_BOGUS_PAGE);

	/* See whether it is safe to remove from the buffer cache. */
	ensure(!xr_is_dirty(p), 		XN_DIRTY_BLOCKS);

	if(xr_is_pinned(db)) {
		printf("trying to unbind pinned block %ld\n", db);
		sys_return(XN_ILLEGAL_OP);
	}

	/* Will do a callback into us to free. */
	if (Ppage_pp_status_get(ppages_get(p->bc->buf_ppn)) == PP_USER) {
		printf("Someone mapping buffer?\n");
	} else {
                /* XXX - ugly */
		bc_set_clean(p->bc);
		while (Ppage_pp_pinned_get(ppages_get(p->bc->buf_ppn)) > 0)
		  ppage_unpin (ppages_get(p->bc->buf_ppn));
		bc_flush_by_buf(p->bc);
		p->bc = 0;
		p->page = 0;
		demand(!bc_lookup(XN_DEV, db),	Just removed for real);
	}
	sys_return(ppn);
}

/* Returns if parent is not in core. */
xn_err_t sys_xn_insert_attr(da_t da, struct xn_op *op) {
        struct xr *p;
        db_t db;
        size_t nblocks;
        void *meta;
	struct xr_td td;
	xn_elem_t ty;
	xn_err_t res;
	sec_t sec;

       	/* Preconditions. */
	build_env(1);
	ensure(db,		XN_BOGUS_NAME);
	ensure(nblocks,		XN_TOO_SMALL);
	sec = off_to_sec(da, &op->m);
	demand(sec, bogus sector);

	/* Commit. */
	try(udf_contains(ty, meta, op));
	demand(!db_isfree(db, nblocks), should not happen!);
	res = xr_alloc(db, nblocks, td, da, sec, 0);
	if(res == XN_REGISTRY_HIT)
		res = XN_SUCCESS;
	sys_return(res);
}

/* Permission checks? */
xn_err_t sys_xn_read_attr(void * dst, db_t db, size_t nblocks, cap_t cap) {
	struct xr *xr;
	size_t nbytes, i;

	nbytes = sizeof *xr * nblocks;
	ensure(write_accessible(dst, nbytes), 	XN_CANNOT_ACCESS);

	for(i = 0; i < nblocks; i++) {
        	ensure(xr = xr_lookup(db+i), 	XN_REGISTRY_MISS);
		memcpy(dst, xr, sizeof *xr);
		dst = (char *)dst + sizeof *xr;
	}
	sys_return(XN_SUCCESS);
}

/* 
 * XXX Want a system call to allocate large amounts of attributes
 * and pages in one swoop.  Need batching + opt?
 */

/*
 * Currently anyone can delete.  XXX: Should allow some things to be 
 * associated with a write capability for sequencing.   We fail if
 * we cannot flush an entry: should return which one it is.
 */
xn_err_t sys_xn_delete_attr(db_t db, size_t nblocks, cap_t cap) {
	ensure(db,		XN_BOGUS_NAME);
	ensure(nblocks,		XN_TOO_SMALL);
	try(xr_flush(db, nblocks, cap));
	sys_return(XN_SUCCESS);
}

void xn_init(void) {
	if(!sys_arena && !(sys_arena = Arena_new()))
		fatal(Could not allocate);
}

char* __xn_free_map;

#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

