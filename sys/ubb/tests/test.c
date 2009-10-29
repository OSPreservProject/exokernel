
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

/* Simple test of structure description. */
#include <xok/types.h>

#include <xn.h>
#include <kernel/virtual-disk.h>

#include "ffs.h"
#include "lib/demand.h"
#include "root-catalogue.h"

static db_t install_root(char *name, cap_t c, xn_elem_t t) {
	int res;
	db_t root;
	struct root_entry r;

	switch((res = sys_xn_mount(&r, name, c))) {
	case XN_SUCCESS:
		printf("root lookup hit in cache\n");
		assert(r.t == t);
		assert(r.nelem == 1);
		return r.db;
	case XN_NOT_FOUND:
		break;
	default:
		printf("res = %d\n", res);
		fatal(Some error);
	}

	do { 
		root = db_find_free(random() % XN_NBLOCKS, 1);
		res = sys_install_mount(name, &root, 1, t, c);
	} while(res == XN_CANNOT_ALLOC);

	switch(res) {
	case XN_SUCCESS:
		break;
	case XN_DUPLICATE:
		fatal(Should not happen (race condition));
	default:
		printf("root = %ld, %d\n", root, res);
		fatal(root failed!);
	}
	return root;
}

#define debug printf

/* Sugar for testing. */
#define can_fail(bool) (bool)
#define must_fail(cmd) do { 					\
	int res;						\
	if((res = (cmd)) >= 0) {				\
		printf("Should have failed, got %d\n", res);	\
		fatal(Die);					\
	}							\
} while(0)
#define no_fail(cmd) do { 					\
	int res;						\
	if((res = (cmd)) < 0) {					\
		printf("Should have succeeded, got %d\n", res);	\
		fatal(Die);					\
	}							\
} while(0)

/* 
 * Allocate all the disk blocks, then deallocate and make sure
 * nothing is lost. 
 */
#if 0
static void file_test(u_inode *ui) {
	int i;
	struct s_indir s;

	ffs_alloc_block(ui->da, s, i, FFS_ANY)
	for(i = 0; i < N_S_INDIR; i++) {
		ff_write_db(&op, i, 0, s->ui, FFS_ANY);
		if(sys_xn_alloc(ui->da, s, i, FFS_ANY)

}
#endif

static void file_test(u_inode *ui) {
	size_t mod, rmod;
	char page[XN_BLOCK_SIZE];
	long hint;
	size_t offset, index;
	xn_cnt_t cnt, val;
	int nwrites, nreads;

	nreads = disk_info.read_req;
	nwrites = disk_info.write_req;

	index = random() % DIRECT_BLOCKS;
	hint = random() % XN_NBLOCKS;

	no_fail(ffs_alloc_block(ui, index, FFS_ANY));
	/* Can't alloc twice. */
	hint = ui_get_db(ui, index);
	must_fail(ffs_alloc_block(ui, index, hint));

	no_fail(ffs_alloc_store(ui, index, FFS_ANY));
	/* Can't alloc twice. */
	must_fail(ffs_alloc_store(ui, index, FFS_ANY));

	cnt = 1;
	must_fail(ffs_read_block(ui, index, 1, &cnt));
	no_fail(ffs_write_block(ui, index, 1, &cnt));
	assert(cnt == 0);

	assert(nreads == disk_info.read_req);
	assert((1+nwrites) == disk_info.write_req);

	/* make it dirty, should see disk writes. */
	cnt = 2;
	no_fail(ffs_write_bytes(ui, index, &cnt, sizeof cnt));
	nwrites = disk_info.write_req;
	no_fail(ffs_write_block(ui, index, 1, &cnt));
	assert((nwrites + 1) == disk_info.write_req);

	/* now force a read. */
	no_fail(ffs_free_store(ui, index));
	no_fail(ffs_alloc_store(ui, index, FFS_ANY));
	assert(nreads == disk_info.read_req);
	no_fail(ffs_read_block(ui, index, 1, &cnt));
	assert((nreads + 1) == disk_info.read_req);
	no_fail(ffs_read_bytes(ui, index, &val, sizeof val));
	assert((nreads + 1) == disk_info.read_req);
	assert(val == 2);

	no_fail(ffs_write_bytes(ui, index, page, sizeof page));
	/* Can't write past end. */
	must_fail(ffs_write_bytes(ui, index, page, sizeof page+1));

	no_fail(ffs_read_bytes(ui, index, page, sizeof page));
	/* Can't read past end. */
	must_fail(ffs_read_bytes(ui, index, page, sizeof page+1));


	no_fail(ffs_free_store(ui, index));
	/* Can't double free */
	must_fail(ffs_free_store(ui, index));
	/* Can't operate on a deleted page. */
	must_fail(ffs_read_bytes(ui, index, page, sizeof page));
	must_fail(ffs_write_bytes(ui, index, page, sizeof page));

	mod = random();
	rmod = 0;
	offset = offsetof(struct inode, mod);
	no_fail(ffs_write_data(ui, offset, &mod, sizeof mod));
	no_fail(ffs_read_data(ui, offset, &rmod, sizeof rmod));

	offset = offsetof(struct inode, db[0]);
	must_fail(ffs_write_data(ui, offset, &mod, sizeof mod));
	must_fail(ffs_read_data(ui, offset, &rmod, sizeof rmod));

	no_fail(ffs_free_block(ui, index));
	must_fail(ffs_free_block(ui, index));

}

static void rules_test(struct u_inode *slash) {
	struct u_s_indir si;
	int i;
	xn_cnt_t cnt;
	db_t db;

	/* Allocate indirect blocks. */
	i = NDIRECT;
	no_fail(ffs_alloc_block(slash, i, FFS_ANY));
	no_fail(ffs_alloc_store(slash, i, FFS_ANY));

	ui_s_indir(&si, slash);

	/* 
	 * Write slash to disk.  This should fail, since the 
	 * indirect block has not been initialized.
	 */
//	must_fail(sys_xn_writeback(da_to_db(slash->da), 1, &cnt));

	/* now initialize. */
	no_fail(ffs_write_block(slash, i, 1, &cnt));
	/* should succeed. */
	no_fail(sys_xn_writeback(da_to_db(slash->da), 1, &cnt));

	/* now free & uninitialized. */

	/* should not let us allocate this disk block. */

	db = ui_get_db(slash, i);
	no_fail(ffs_free_block(slash, i));
#if 0
	/* put dependencies back in. */
	must_fail(ffs_alloc_block(slash, i, db));
#endif

	/* write should release db. */
	no_fail(sys_xn_writeback(da_to_db(slash->da), 1, &cnt));
	no_fail(ffs_alloc_block(slash, i, db));
	assert(ui_get_db(slash, i) == db);

	/* Try it again. */
	must_fail(ffs_write_block(slash, i, 1, &cnt));
	no_fail(ffs_alloc_store(slash, i, FFS_ANY));
	no_fail(ffs_write_block(slash, i, 1, &cnt));

	no_fail(sys_xn_writeback(da_to_db(slash->da), 1, &cnt));
	no_fail(ffs_free_block(slash, i));
#if 0
	must_fail(ffs_alloc_block(slash, i, db));
#endif
	no_fail(sys_xn_writeback(da_to_db(slash->da), 1, &cnt));
	no_fail(ffs_alloc_block(slash, i, db));
	assert(ui_get_db(slash, i) == db);

	no_fail(ffs_free_block(slash, i));
	no_fail(sys_xn_writeback(da_to_db(slash->da), 1, &cnt));
}

int main(int argc, char *argv[]) {
	db_t root;
	int i;
	u_inode ui;
	xn_cnt_t cnt;
	cap_t c;
	da_t pa, da;
	int j;

	/* this is the first slot ; used to hold /. */
	enum { ROOT_SLOT = 1 };

	j = 0;
loop:
	sys_xn_init(); 	/* Initialize the disk. */


	/* Install our types. */
	if(specify_ffs_meta() < 0)
		fatal(Failed);
	
	c = 0;
	root = install_root("ffs-root", c, inode_t);
	pa = da_compose(sys_root(), 0);
	da = da_compose(root, 0);

	ui_init(&ui, da, pa, c, c);

	/* Allocate backing store for root. */
	if((i = sys_xn_bind(root, -1, 0, XN_ZERO_FILL, 1)) < 0)
		fatal(Could not bind);
	else
		printf("allocated page %d\n", i);

	no_fail(sys_xn_writeback(root, 1, &cnt));

	no_fail(sys_xn_writeback(sys_root(), 1, &cnt));

	no_fail(ffs_set_type(&ui, meta_t, 0));


	/* Allocate some space for it. */
	for(i = 0; i < 10; i++)
		file_test(&ui);

	for(i = 0; i < 10; i++)
		rules_test(&ui);

	printf("shutting down\n");
	no_fail(sys_xn_shutdown());

	if(j++ < 10) goto loop;

	return 0;
}
