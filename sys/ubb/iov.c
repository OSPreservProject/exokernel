
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

#include "xn-struct.h"
#include "kernel.h"
#include "kexcept.h"
#include "registry.h"
#include "virtual-disk.h"
#include "demand.h"
#include "rules.h"
#include "xok/bc.h"

/*
 * Not used anymore: look here. 
 *
 * Fails if the buffer is locked for I/O or is not in the registry.
 * If the buffer is not in the registry, but is in the buffer cache
 * we load it.
 *
 * The only reason for incore is because we can bind pages and
 * then, at a much later point, read them in.
 */

/* Failed: Unlock everything that was locked. */
static void iov_rollback(db_t db, size_t nblocks) {
	int i;
	struct xr *p;

	for(i = 0; i < nblocks; i++) {
       		if((p = xr_lookup(db+i)))
			p->locked = 0;
	}
}

/* Find a miss, and split when we do. */
static xn_err_t xr_pre_io(db_t db, size_t nblocks, int writep) {
        struct xr *p;
	int i;
	xn_err_t res;

	/* XN DRE */
	/* Readin from disk can only happen if the blocks are not in core. */

	res = XN_SUCCESS;
	for(i = 0; i < nblocks && res == XN_SUCCESS; i++) {
		/* not ours --- bc will check it. */
        	if(!(p = xr_lookup(db+i))) 
			;
		else if(writep) {
			/* bc will check the other conditions. */
			if((res = rules_can_write(p)) == XN_SUCCESS)
				p->locked = 1;
		} else {
                        if(bc_lookup(XN_DEV, db+i))
                                res = XN_IN_CORE;
                        else if(!p->initialized)
                                res = XN_DIRTY_BLOCKS;
                        else
                                p->locked = 1;
		} 
	}
	if(res != XN_SUCCESS)
		iov_rollback(db, i-1);
	return res;
}


/* Walk down the list, unlocking them and checking. */
void xr_post_io(db_t db, size_t nblocks, int writep) {
        unsigned i;
        struct xr *xr;

        for(i = 0; i < nblocks; i++, db++) {
		/* XN XXX: Random callback from bc (I hope). */
        	if(!(xr = xr_lookup(db)))
			continue;

        	xr->locked = 0;
        	if(writep)
        		rules_written(xr, db, 1);
        	else
        		rules_read(xr, db, 1);
	}
}

xn_err_t xr_bwrite(db_t dst, size_t nblocks, xn_cnt_t *cnt) {
	int res;

        try(xr_pre_io(dst, nblocks, 1));

	/* must succeed. */
        if((res = bc_write(dst, nblocks, cnt) >= 0))
		res = XN_SUCCESS;
	else 
		iov_rollback(dst, nblocks);
	return res;
}

xn_err_t xr_bread(db_t src, size_t nblocks, xn_cnt_t *cnt) {
	int i, res;
	struct xr *p;

        try(xr_pre_io(src, nblocks, 0));

	/* Must succeed: only do a read an insert. */
        if((res = bc_read(src, nblocks, cnt)) < 0) {
		iov_rollback(src, nblocks);
		return res;
	}

	/* Now get a handle on the bc entry */
	for(i = 0; i < nblocks; i++, src++) {
        	if((p = xr_lookup(src))) {
			if(xr_bind_bc(p) < 0)
				fatal(Just read in: cannot fail);
		}
	}
        return XN_SUCCESS;
}
