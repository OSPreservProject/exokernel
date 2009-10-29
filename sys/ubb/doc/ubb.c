
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

#include "ubb.h"
#include "set.h"

typedef int (*meta_chk)(Set old, Set new, db_t db);

Set ubb_run(void* meta, ubb_p up, size_t index);
#define debug (void)

static inline int 
meta_transaction(meta_chk chk, void* meta, size_t size, ubb_p up, size_t index, db_t b, struct xn_m_vec *mv, int n) {
	int res;
	Set old_set, new_set;
	struct xn_m_vec *old_data;

	if(mv_access(size, mv, n) < 0)
		return -1; 
	/* Old state used to check and to roll back. */
	old_set = ubb_run(meta, up, index);
	/* Make a record of the data that mv will overwrite. */
	old_data = mv_read(meta, mv, n);
	/* set_print("old", old_set); */

	mv_write(meta, mv, n);
	new_set = ubb_run(meta, up, index);
	/* set_print("new", new_set); */

	/* ensure: {old_set - b} == new_set */
	if(chk(old_set, new_set, b))
		res = 0;
	else {
		/* rollback */
		mv_write(meta, old_data, n);
		res = -1;
	}

	/* Free data consumed by the modules. */
	set_free();

	return res;
}

/* Modify meta, should not produce a difference in the access set. */
int meta_write(void* meta, size_t size, ubb_p up, size_t index, struct xn_m_vec *mv, int n) {
	/* ensure that old == new */
	static inline int nop(Set old, Set new, db_t db) {
		return set_eq(old, new);
	}
	return meta_transaction(nop, meta, size, up, index, 0, mv, n);
} 

/* Add exactly b to the set guarded by meta. */
int meta_alloc(void* meta, size_t size, ubb_p up, size_t index, db_t db, struct xn_m_vec *mv, int n) {
	/* ensure that {old_set U b} == new_set */
	static inline int add(Set old, Set new, db_t db) {
		return set_eq(set_union(old, set_single((T){db})), new);
	}
	return meta_transaction(add, meta, size, up, index, db, mv, n);
}

/* Delete exactly b from set guarded by meta. */
int meta_dealloc(void* meta, size_t size, ubb_p up, size_t index, db_t db, struct xn_m_vec *mv, int n) {
	/* ensure that  old_set - new = {db}  */
	static inline int remove(Set old, Set new, db_t db) {
		return set_eq(set_diff(old, new), set_single((T){db}));
	}
	return meta_transaction(remove, meta, size, up, index, db, mv, n);
}

/* 
 * Make sure that the before set is nil --- need to test on all
 * udf's. 
 */
int meta_init(void* meta, ubb_p up) {
	return set_empty(ubb_run(meta, up, 0)) ?
		0 : -1;
}

Set udf_access;

void ubb_add(db_t db, size_t n, int type) {
	int i;
	struct xn_update u;

	if(!db)
		return;
	u.nelem = n;
	u.type = type;

	for(i = 0; i < n; i++) {
		u.db = db + i;
		udf_access = set_union(udf_access, set_single(u));
	}
}

/* up will call the set contruction routine. */
Set ubb_run(void* meta, ubb_p up, size_t index) {
        udf_access = set_new();
        up(meta, index);
        return udf_access;
}
