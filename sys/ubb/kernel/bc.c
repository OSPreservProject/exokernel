
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

#include <xok_include/assert.h>
#include <xn.h>
#include <xok/types.h>
#include <kexcept.h>
#include "kernel.h"
#include "virtual-disk.h"
#include "xok/bc.h"
#include "lib/table.h"

/*
 *  Veneer on top of bc. 
 */

#include <xok/bc.h>
#include <xok/pmap.h>
#include <xok/env.h>

#define ROOT_CAP &__envs[0].env_clist[0]
#define bc_va(bc) ppn_to_phys((bc)->buf_ppn)

struct bc_entry *bc_alloc_entry(u32 dev, u32 db, int *error) {
       struct bc_entry *bc;
       struct Ppage *pp;

#if 0
        int r;
	if(ppage_alloc(PP_USER, &pp, 0) < 0 
	|| !(bc = bc_insert(XN_DEV, db, ppn(pp), 0, &r))) {
printf ("dropping a page here...: pp %p, refcnt %d\n", pp, pp->pp_refcnt);
		return 0;
	}
	ppage_free(pp);
#endif
	bc = ppage_get_reclaimable_buffer (XN_DEV, db, BC_EMPTY, error, 0);
	if (!bc) {
	  return 0;
	}
	pp = &ppages[bc->buf_ppn];
	ppage_setcap(pp, ROOT_CAP);
	bc_set_dirty(bc);
	return bc;
}

int bc_write(db_t dst, size_t nblocks, xn_cnt_t *cnt) {
        return bc_write_extent (XN_DEV, dst, nblocks, cnt);
}

int bc_read(db_t src, size_t nblocks, xn_cnt_t *cnt) {
        if (bc_read_extent (XN_DEV, src, nblocks, cnt) < 0) {
	  return -1;
	} else {
	  return 0;
	}
}
