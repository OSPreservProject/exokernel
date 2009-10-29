
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

#ifndef __XN_CACHE_H__
#define __XN_CACHE_H__
/*
 * Registry interface.
 * Two levels: db registry, then xn registry on top.
*/
#ifdef EXOPC
#include <xok/bc.h>
#else
#include <stddef.h>
#endif

#include <ubb/xn.h>
#include <ubb/kernel/kernel.h>
#include <xok/queue.h>

/*
 * We need to know the type and base determine what type this disk block
 * is and the number of elements to determine if the entire thing is legally
 * referenced.
 */
struct xr_td {
	xn_elem_t 	type;		/* base type. */
	db_t		base;		/* beginning disk address */
	size_t 		nelem;		/* number of elements of type */
	size_t		size;		/* size of base type */
};

typedef enum { 
	XN_VOLATILE = 1, XN_REACHABLE, XN_PERSISTENT,	/* Survive reboot? */ 
	XN_INCOMING = 1 << 16, XN_OUTGOING = 1 << 17, 	/* I/O states. */
	XN_LOCK = 1 << 18,				/* User-settable. */
	XN_MUST_READIN = 1 << 19                        /* Must readin */
} state_t;

#if 0
enum {
	XN_EMPTY = BC_EMPTY,
	XN_VALID = BC_VALID,
	XN_COMING_IN = BC_COMING_IN,
	XN_GOING_OUT = BC_GOING_OUT
};
#endif

typedef da_t sec_t;
struct bc_entry;

/* 
 * Buffer cache entry.  It is too big --- should allow extents.
 * Useful features:
 *	- attribute can exist w/o having an entity.
 */
struct xr {
	db_t 		db;		/* The physical block we correspond to. */

	struct xr_td	td;		/* type descriptor for this block. */
#	define xr_type(_x) ((_x)->td.type)

	da_t 		p;	/* address of parent pointer. */
	sec_t 		p_sec;	/* the specific sector of the pointer to us. */

	/* This may expand to include user-specified bindings.  */
	da_t		sticky;		/* da of last parent that had caps. */
#	define xr_get_sticky(p) ((p)->sticky)

	/* 
	 * Consitency attributes.  Default values are zeros for everything 
	 * except refs (default == 1).
	 */
	void *page;
	struct bc_entry *bc;

	size_t		refs;	/* References to this entry. */
	state_t 	state;

	unsigned 	locked:1;	/* locked for io or by user. */
	unsigned 	initialized:1;	/* Has disk object been initialized? */
	unsigned 	dirty:1;	/* Is the page dirty? */
	unsigned	internal_ptrs:1;/* Does the xn have ptrs? */

	LIST_HEAD(klist, xr) 	v_kids, 	/* volatile in core kids. */
				s_kids;		/* stable, in core kids. */
	LIST_ENTRY(xr) 	 sib;		/* used by v_kids, s_kids, willfree */
};

#define xr_is_dirty(_x) 	(_x)->bc->buf_dirty
void *xr_backing(struct xr *n);
void xr_set_dirty(struct xr *n);
void xr_set_clean(struct xr *n);

enum { XN_NO_PARENT = 0 };

/* Insert block db(da) of type t with parent p into registry. */ 
xn_err_t xr_insert(da_t db, struct xr_td td, da_t parent, sec_t sec);

xn_err_t xr_toff(struct xr *xr, size_t offset);

/* Lookup da in registry.  Returns nil on failure. */
struct xr *xr_lookup(db_t db);
struct xr *xr_raw_lookup(db_t db);

/* Delete [db, db + nblocks) from the registry.  If mustfree is set, fails
   if there is a registry miss. */
xn_err_t xr_delete(db_t db, size_t nblocks);

/* 
 * Allocate [db, db + nblocks) and place attribute entries for them
 * in the buffer cache.  We track that these blocks are uninitialized
 * in the buffer cache.
 *
 * Provide an operation that will write out all data except pointers
 * to uninitialized blocks.  This will allow stronger stability, and
 * decreased memory requirements.
 */
xn_err_t xr_alloc(db_t db, size_t n, struct xr_td td, sec_t sec, da_t parent, int alloc);

/* Reset volatile registry state. */
void xr_reset(void);

xn_err_t xr_willfree(db_t db, size_t nblocks);
xn_err_t xr_mark_dirty(db_t db, size_t nblocks);
xn_err_t xr_pin(db_t db, size_t nblocks);

struct xr_td xr_type_desc(xn_elem_t t, size_t size, db_t base, size_t nelem);

/* Walk down the cache, copying [da, da+nbytes) to user-level. */
xn_err_t xr_incore(db_t db, size_t nblocks);

/* Used to guarentee invariants. */
void xr_allocate(struct xr *p, struct xr *n);
void xr_read(struct xr *p, struct xr *n);
xn_err_t xr_write(struct xr *n);
xn_err_t xr_flush(db_t db, size_t nblocks, cap_t cap);
xn_err_t xr_contig(db_t db, size_t nblocks);

/* Should allow a vector of capabilities? */
xn_err_t xr_cap_check(xn_op_t op, struct xr *p, cap_t cap);

xn_err_t xr_bwrite(db_t dst, size_t nblocks, xn_cnt_t *cnt);
xn_err_t xr_bread(db_t src, size_t nblocks, xn_cnt_t *cnt);

/* Called by the io routine before initiating request. */
xn_err_t xr_io(struct xr *xr, int writep);
xn_err_t
xr_backdoor_install(da_t p, sec_t sec, void **page, xn_elem_t t, size_t tsize, db_t db, int nb);

/* 
 * Write all blocks that are kids of db to disk (0 = all blocks in core).
 * Will do this in the correct order to preserve dependencies. 
 * If free_p is set, it also free's storage.
 */
xn_err_t xr_flushall(db_t db);

int xr_clean(db_t db);
void xr_del(struct xr *xr, int freedb);
int xr_io_in_progress(struct xr *n);
inline xn_err_t xr_bind_bc(struct xr *n);
int xr_is_pinned(db_t db);

#endif
