
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
 * Simplistic implementation of Ganger and Patt rules (OSDI 1). 
 * For speed, track whether a block has dependencies.
 */
#include <demand.h>
#include <dependency.h>
#include <kexcept.h>
#include <list.h>
#include <registry.h>
#include <rules.h>
#include <virtual-disk.h>
#include <xn.h>

/* Can write [db, db+nblocks)? */
xn_err_t rules_can_write(struct xr *n) {
	sec_t sec;
	size_t nsecs;

	sec = db_to_sec(n->db);
	nsecs = bytes_to_secs(n->td.size * n->td.nelem);
	
	return sys_sec_can_write_r(sec, nsecs);
}

/* Need to special case volatile kids, since we can free immediately. */
xn_err_t rules_allocate(struct xr *n, struct xr *p) {
	sec_t sec;
	size_t nsecs;
	t_flag *tf;

	l_enq(p->v_kids, n, sib);
	if (n->td.type != XN_DB) {
	  	assert(n->internal_ptrs);
	  	sec = db_to_sec(n->db);
	  	nsecs = bytes_to_secs(n->td.size * n->td.nelem);
		tf = (xr_bind_bc(p) == XN_SUCCESS) ?
			&p->bc->buf_tainted : 0;
		
	  	if(sec_depend_r(n->p_sec, sec, nsecs, tf, 1) != XN_SUCCESS)
	    		fatal(Cannot fail);
	}

	return XN_SUCCESS;
}

/* Called when registry entry is created. */
xn_err_t rules_create_entry(struct xr *n, struct xr *p) {
	l_enq(p->v_kids, n, sib);

        n->initialized = 1;
	return XN_SUCCESS;
}

/* Remove entry from registry (doesn't free disk blocks).  */
xn_err_t rules_flush_entry(struct xr *n) {
	size_t nblocks;

	/* Check dependencies. */
	try(rules_can_write(n));	
	nblocks = bytes_to_blocks(n->td.size * n->td.nelem);
	try(rules_written(n, n->db, nblocks));	/* flush dependencies. */

	n->bc = 0;
	n->page = 0;
#if 0
	/* Cannot remove internal nodes of the tree. */
	if(l_nil(n->v_kids) && l_nil(n->s_kids)) {
		l_remove(n, sib);
		xr_del(n, 0);
	}
#endif
	return XN_SUCCESS;
}

/* XXX: For the moment we free immediately.  */
xn_err_t rules_free(struct xr *p, struct xr *n) {
	size_t nblocks;

	ensure(l_nil(n->v_kids) && l_nil(n->s_kids),	XN_CANNOT_DELETE);

	try(rules_can_write(n));	
	nblocks = bytes_to_blocks(n->td.size * n->td.nelem);
	try(rules_written(n, n->db, nblocks));	/* flush dependencies. */

	l_remove(n, sib);
	xr_del(n, 1);
	return XN_SUCCESS;
}

/* 
 * XXX only make the transitions when the entire in core value
 * was read. 
 */

xn_err_t rules_read(struct xr *xr, db_t db, size_t nblocks) {
        /* xr->dirty = 0; */
	return XN_SUCCESS;
}

xn_err_t rules_written(struct xr *n, db_t db, size_t nblocks) {
	n->dirty = 0;
        n->initialized = 1;
	return sec_written_r(db_to_sec(db), blocks_to_secs(nblocks));
}
