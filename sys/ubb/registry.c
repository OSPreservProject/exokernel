
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
 * Registry code.  Ideally it is a mapping of (db -> ppn).  However,
 * we use it to guarentee a variety of invarients.  Simplifications
 * are welcome.
 *
 * Autogen:
 *	1. have file that describes what we want.
 *
 *	2. gen and append that header to file so it is a matter of
 *	rerunning when changes are made.
 * XXX
 *	1. add extents.
 *
 *** Most important: 
 *	- go through and for every complex thing, figure out how to 
 *	get them to do it for us.
 */

#define __XN_MODULE__

#include <xn-struct.h>
#include <kexcept.h>
#include <kernel.h>
#include <demand.h>
#include "virtual-disk.h"
#include "registry.h"
#include "mem.h"
#include "table.h"
#include "list.h"
#include "iov.h"
#include "type.h"
#include "rules.h"
#include "xok/bc.h"
#include <xok/sysinfo.h>
#include <xok/sys_proto.h>


static Table_T registry;
static Table_T pinned;

/* for table manipulations: should auto gen as well. */
static struct xr *delete(db_t db) 
	{ return Table_remove(registry, (void *)db); }
inline struct xr *xr_lookup(db_t db) 
	{ return !db ? 0 : Table_get(registry, (void*)db); }
static int insert(db_t db, struct xr *xr) 
	{ return Table_put(registry, (void *)db, xr) != (void *)-1; }

/* callbacks from bc */
int xn_remove (u32 dev, u32 blk, u32 len) {
#       define print_val(key, val) printf("key = %ld, val = %p\n", (db_t)key, val)

        xn_err_t res;

        res = 0;

        if(!blk || dev != XN_DEV)
                demand(len <= 1, Bogus length);
        else if(xr_lookup(blk)) {
	  if (pinned) {
	    ppn_t ppn = (ppn_t)Table_get(pinned, (void*)blk);
	    if (ppn) {
	      printf("Pinned! %d %ld\n", blk, ppn);
	      fatal(pinned);
	    }
	  } else {
	    printf ("No pin table\n");
	  }
	  if((res = xr_flush(blk, len, 0)) != XN_SUCCESS) {
	    printf("** BAD *** xr_flush failed, returned %d %d %d\n", res, blk, len);
	    // fatal(Failed);
	  }   
	}

                /* printf("xn_remove: called on blk %d, %d\n", blk, len); */
        return 1;
}


/*
 * Work.
 */

struct xr_td xr_type_desc(xn_elem_t t, size_t size, db_t base, size_t nelem) {
	struct xr_td td;
	td.type = t;
	td.base = base;
	td.nelem = nelem;
	td.size = size;

	return td;
}

xn_err_t xr_cap_check(xn_op_t op, struct xr *xr, cap_t cap) {
	return 1;
}

static struct xr *xr_new(db_t db, struct xr_td td, da_t parent, sec_t sec) {
	struct xr *xr, *p;

	demand(db, cannot be nil);
	demand(sec, cannot be nil);

	if(!(xr = malloc(sizeof *xr)))
		return 0;
        MP_LOCK(SYSINFO_LOCK);
	INC_FIELD(si,Sysinfo,si_xn_entries,1);
        MP_UNLOCK(SYSINFO_LOCK);
	memset(xr, 0, sizeof *xr);
        xr->db = db;
        xr->td = td;
        xr->p = parent;
        xr->p_sec = sec;
        xr->refs = 1;
	xr->internal_ptrs = 1;

	xr->state = XN_VOLATILE;
	xr->initialized = 0;
	xr->internal_ptrs = (xr->td.type != XN_DB);
	xr->locked = 0;
	xr->dirty = 1;	/* dirty because on disk is not coherent. */

	/* XXX: The inflexibility of this propogation is a shameful hack. */
	if(!type_is_sticky(xr->td.type))
		xr->sticky = 0;
	else {
		p = xr_lookup(da_to_db(parent));
		demand(p, impossible);
		xr -> sticky = !p->sticky ?  parent : p->sticky;
	}
        LIST_INIT(&xr->v_kids);
        LIST_INIT(&xr->s_kids);

	/* printf("allocated xr %p, %ld\n", xr, xr->db); */
	return xr;
}

void xr_del(struct xr *xr, int freedb) {

	/* printf("free xr %p, %ld, freedb = %d\n", xr, xr->db, freedb); */
#if 0
	if(xr->bc)
		printf("free %ld page=%d\n", xr->db, xr->bc->buf_ppn);
#endif
	if(freedb) { 
		demand(xr->db, Bogus db);
		db_free(xr->db, 1);
	}
	if(!delete(xr->db))
		fatal(could not free);
	memset(xr, 0, sizeof *xr);
	free(xr);
        MP_LOCK(SYSINFO_LOCK);
	DEC_FIELD(si,Sysinfo,si_xn_entries,1);
        MP_UNLOCK(SYSINFO_LOCK);
}


xn_err_t xr_flush(db_t db, size_t nblocks, cap_t cap) {
        struct xr *xr;
	int i;

        /* Should we fail if block can't be deleted? */
        for(i = 0; i < nblocks; i++)
                if((xr = xr_lookup(db + i)))
                        try(rules_flush_entry(xr));

	return XN_SUCCESS;
}

xn_err_t xr_insert(db_t db, struct xr_td td, da_t parent, sec_t sec) {
	struct xr *xr;
	
	if((xr = xr_lookup(db))) {
		ensure(xr->p == parent, XN_BOGUS_ENTRY);
		sys_return(XN_REGISTRY_HIT);
	}
	ensure(xr = xr_new(db, td, parent, sec), XN_OUT_OF_MEMORY);
	ensure_s(insert(db, xr), { free(xr); sys_return(XN_OUT_OF_MEMORY); });

	return XN_SUCCESS;
}

/* Insert an entry without a parent into the registry.  XXX. */
xn_err_t
xr_backdoor_install(da_t p, sec_t sec, void **page, xn_elem_t t, size_t tsize, db_t db, int nb)
{
        struct xr_td td;
        struct xr *xr;
	int r;

        td = xr_type_desc(t, tsize, db, nb);

	demand(!db_isfree(db, nb), Installing entry that is free!);

        try(xr_insert(db, td, p, sec));

        if(!(xr = xr_lookup(db)))
                fatal(Cannot fail);

    	xr->bc = bc_alloc_entry(XN_DEV, xr->db, &r);
        xr->page = bc_va(xr->bc);
	memcpy(xr->page, *page, PAGESIZ);
	*page = xr->page;
	if(xr_pin(db, nb) != XN_SUCCESS)
		printf("pin failed\n");

	assert(nb == 1);

        xr->initialized = 1;
        xr->state = XN_PERSISTENT;
	xr->dirty = 1;

	return XN_SUCCESS;
}

static xn_err_t xrfree(struct xr *xr) {
	if(xr->dirty)
		return 0;
	if(xr->page)
		pfree(phys_to_ppn(xr->page), 1);
        free(xr);
	return 1;
}

static int flushall(struct xr *xr) {
	struct xr *n, *p;

	/* Check that volatile kids are free. */
        for(n = l_first(xr->v_kids); n; ) {
		p = n;
		n = l_next(n, sib);
		if(!flushall(p))
			return 0;
	}

        for(n = l_first(xr->s_kids); n; ) {
		p = n;
		n = l_next(n, sib);
		if(!flushall(p))
			return 0;
	}

	return xrfree(xr);
}

/* Recursively remove all mappings, cannot be dirty. */
xn_err_t xr_flushall(db_t db) {
	/* Used to verify that all kids are clean. */
#	define	flush_root(key, val) xrfree(val)


	struct xr *n;

	if(db == XN_ANY) {
		TABLE_MAP(registry, flush_root);
	} else {
		/* If it is not in the registry, then it must be flushed */
		ensure(n = xr_lookup(db),	XN_SUCCESS);
		ensure(flushall(n),		XN_DIRTY_BLOCKS);
	}
	return XN_SUCCESS;
#	undef flush_root
}

static int isclean(struct xr *xr) {
	struct xr *n;

	if(xr->dirty)
		return 0;

	/* Check that volatile kids are free. */
        for(n = l_first(xr->v_kids); n; n = l_next(n, sib))
		if(!isclean(n))
			return 0;

        for(n = l_first(xr->s_kids); n; n = l_next(n, sib))
		if(!isclean(n))
			return 0;
	return 1;
}

/* Verify that the tree underneath db is clean. */
int xr_clean(db_t db) {
	/* Used to verify that all kids are clean. */
#	define	check_clean(key, val) \
		ensure(isclean(val),		XN_DIRTY_BLOCKS);

	struct xr *n;

	if(db == XN_ANY) {
		TABLE_MAP(registry, check_clean);
	} else {
		/* If it is not in the registry, then it must be clean. */
		ensure(n = xr_lookup(db),	XN_SUCCESS);
		ensure(isclean(n),		XN_DIRTY_BLOCKS);
	}
	return XN_SUCCESS;
#	undef check_clean
}

void xr_reset(void) {
	if(registry)
		Table_free(&registry);	
	registry = Table_new(8192, 0, 0);
	pinned = Table_new(128, 0, 0);
}

xn_err_t xr_alloc(db_t db, size_t n, struct xr_td td, da_t parent, sec_t sec, int alloc) {
        int i, res;
	struct xr *xr, *p;

	demand(db, Bogus db);
	/* Now go through and run bootstrapping code. */
	p = xr_lookup(da_to_db(parent));
	demand(p, should not fail);

	if(alloc) {
        	for(i = 0; i < n; i++) {
                	res = xr_insert(db + i, td, parent, sec);

			if(res == XN_SUCCESS)
				continue;

			/* Failed: remove all entries. */
			while(i-- > 0) {
				printf("failed insertion: removing %ld\n", db+i);
				xr = xr_lookup(db+i);
				demand(xr, just inserted);
				xr_del(xr, 0);
			}
			sys_return(res);
		}
        	for(i = 0; i < n; i++) {
			xr = xr_lookup(db + i);
			demand(xr, Must succeed);
			rules_allocate(xr, p);
		}
	} else {
        	for(i = 0; i < n; i++) {
			switch((res = xr_insert(db + i, td, parent, sec))) {
			case XN_REGISTRY_HIT:
				break;
			case XN_SUCCESS:
				xr = xr_lookup(db + i);
				rules_create_entry(xr, p);
				break;
			default:
				sys_return(res);
			}
		}
	}
        return XN_SUCCESS;
}

xn_err_t xr_contig(db_t db, size_t nblocks) {
	int i;
	struct xr *xr;
	void *expected, *page;

        for(expected = 0, i = 0; i < nblocks; i++) {
		ensure(xr = xr_lookup(db+i), XN_REGISTRY_MISS);
		ensure(page = xr_backing(xr), XN_NOT_CONTIG);

		if(expected && page != expected)
			sys_return(XN_NOT_CONTIG);
		expected = (char *)page + PAGESIZ;
	}

	return XN_SUCCESS;
}

xn_err_t xr_pin(db_t db, size_t nblocks) {
	int i;
	struct xr *xr;

	demand(nblocks, bogus number of blocks);
        for(i = 0; i < nblocks; i++) {
		ensure(xr = xr_lookup(db+i), XN_REGISTRY_MISS);
		xr->dirty = 1;
		if(xr_bind_bc(xr) == XN_SUCCESS) { 
		  //printf("Pinning %ld!\n", db);
		  if (pinned) {
		    Table_put(pinned, (void*)db, (void *)(int)xr->bc->buf_ppn);
		    ppage_pin (&ppages[xr->bc->buf_ppn]);
		  } else {
		    printf("No pinned table (put)\n");
			sys_return(XN_CANNOT_ALLOC);
		  }
		}
		else {
			printf("could not pin %ld\n", xr->db);
			sys_return(XN_NOT_IN_CORE);
		}
	}
	return XN_SUCCESS;
}

int xr_is_pinned(db_t db) {
  return (int)Table_get(pinned, (void*)db);
}

xn_err_t xr_no_pages(db_t db, size_t nblocks) {
	int i;
	struct xr *xr;

        for(i = 0; i < nblocks; i++)
		if((xr = xr_lookup(db+i)) || xr->page)
			sys_return(XN_IN_CORE);
	return XN_SUCCESS;
}

xn_err_t xr_delete(db_t db, size_t nblocks) {
        int i;
        struct xr *xr;
	struct bc_entry *bc;

        for(i = 0; i < nblocks; i++) {
        	ensure(xr = xr_lookup(db + i), XN_REGISTRY_MISS);

		/* XXX: we should handle this by deferred dealloc. */
		//ensure(!xr->locked,	XN_LOCKED);

                demand((xr->bc && xr->page) || (!xr->bc && !xr->page),
                               must be allocated and freed simultaneously);

		if(!(bc = xr->bc)) {
			if(xr_bind_bc(xr) == XN_SUCCESS)
				bc = xr->bc;
		}

                try(rules_free(0, xr));

  		if(bc) {
			if(xr_is_pinned(db+i)) 
				printf("freeing pinned buf: %ld\n", db+i);
			/* XXX - ugly */
			if (ppages[bc->buf_ppn].pp_status != PP_USER) {
			  bc_set_clean(bc);
			  while (ppages[bc->buf_ppn].pp_pinned > 0)
			    ppage_unpin (&ppages[bc->buf_ppn]);
			  bc_flush_by_buf(bc);
			}
		}
			
	}
       return XN_SUCCESS;
}


void xr_set_dirty(struct xr *n) {
        n->dirty = 1;
        bc_set_dirty(n->bc);
}
void xr_set_clean(struct xr *n) {
        n->dirty = 0;
        bc_set_clean(n->bc);
}

void *xr_backing(struct xr *n) {
	if(!n->page)
		xr_bind_bc(n);
	return n->page;
}

int xr_io_in_progress(struct xr *n) {
        struct bc_entry *bc;

        bc = n->bc;
        demand(bc, Bogus bc);
        return  (bc->buf_state == BC_COMING_IN
                || bc->buf_state == BC_GOING_OUT);
}

/*
 * For the moment, we disallow having pages that must be bound in
 * core before being read.   Where should we check for I/O at the
 * bc level?
 */
inline xn_err_t xr_bind_bc(struct xr *n) {
        if(n->bc) {
                demand(n->page, Out of sync!);
                demand(n->bc == bc_lookup(XN_DEV, n->db), 
			Lookup failed: bc took buffer);
		if(n->bc->buf_state == BC_EMPTY)
			printf("status == EMPTY\n");
                return XN_SUCCESS;
        } else if(n->page)
		printf("have page %p and nil buf for db %ld\n", n->page, n->db);

        ensure(n->bc = bc_lookup(XN_DEV, n->db), XN_NOT_IN_CORE);
        n->page = bc_va(n->bc);
        return XN_SUCCESS;
}



#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

