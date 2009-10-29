
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
#include <stdlib.h>
#include <memory.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>

#include <ubb/xn.h>
#include <ubb/kernel/virtual-disk.h>

#include "ffs.h"
#include "demand.h"
#include "ubb/root-catalogue.h"

#include <xok/sys_ucall.h>
#include <ubb/lib/sync-disk.h>

#include <exos/kprintf.h>
static void
chkpt(char* msg) {
  kprintf(msg);
  kprintf("\n");
	sleep(1);
}


#include <ubb/root-catalogue.h>

void print_root_catalogue(void) {
        int i, res;
        struct root_catalogue rc;

        if((res = sys__xn_read_catalogue(&rc)) != XN_SUCCESS) {
                printf("failed res = %d\n", res);
		return;
	}
	printf("root catalogue holds %d elements\n", rc.nelem);
        for(i = 0; i < rc.nelem; i++) {
                struct root_entry *re;

                re = &rc.roots[i];
                printf("entry[%d] = <%s>, db=%ld, nelem=%d, type=%d",
                        i, re->name, re->db, re->nelem, re->t);
                if(re->is_type)
                        printf("is type, type id = %d\n", re->is_type);
                else
                        printf("is a root\n");
        }
	printf("root catalogue holds %d elements\n", rc.nelem);
}


static db_t install_root(char *name, cap_t c, xn_elem_t t) {
	int res;
	db_t root;
	struct root_entry r;

	switch((res = sys__xn_mount(&r, name, c))) {
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

	printf("type = %d\n", t);

	do { 
		root = 0;
		res = sys__install_mount(name, &root, 1, t, c);
	} while(res == XN_CANNOT_ALLOC);

	switch(res) {
	case XN_SUCCESS:
		break;
	case XN_DUPLICATE:
		fatal(Should not happen (race condition));
	default:
		printf("root = %ld, %d, %d\n", root, res, t);
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
		if(sys__xn_alloc(ui->da, s, i, FFS_ANY)

}
#endif

static void file_test(u_inode *ui) {
	size_t mod, rmod;
	char page[XN_BLOCK_SIZE];
	long hint;
	size_t offset, index;
	xn_cnt_t cnt, val;

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
	sync_wait(&cnt, 0);
	assert(cnt == 0);

	/* make it dirty, should see disk writes. */
	cnt = 2;
	no_fail(ffs_write_bytes(ui, index, &cnt, sizeof cnt));
	no_fail(ffs_write_block(ui, index, 1, &cnt));
	sync_wait(&cnt, 1);

	chkpt("First half of test1");

	/* now force a read. */
	no_fail(ffs_free_store(ui, index));
	no_fail(ffs_alloc_store(ui, index, FFS_ANY));

	cnt = 1;
	no_fail(ffs_read_block(ui, index, 1, &cnt));
	sync_wait(&cnt, 0);
	no_fail(ffs_read_bytes(ui, index, &val, sizeof val));
	assert(val == 2);

	no_fail(ffs_write_bytes(ui, index, page, sizeof page));
	/* Can't write past end. */
	must_fail(ffs_write_bytes(ui, index, page, sizeof page+1));

	no_fail(ffs_read_bytes(ui, index, page, sizeof page));
	/* Can't read past end. */
	must_fail(ffs_read_bytes(ui, index, page, sizeof page+1));

	chkpt("Second half of test1");

	no_fail(ffs_free_store(ui, index));
	/* Can't double free */
#if 0
	must_fail(ffs_free_store(ui, index));
#endif
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
	chkpt("Done test1");
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

	chkpt("Starting test 2");

	ui_s_indir(&si, slash);

	/* 
	 * Write slash to disk.  This should fail, since the 
	 * indirect block has not been initialized.
	 */
//	must_fail(sys__xn_writeback(da_to_db(slash->da), 1, &cnt));

	/* now initialize. */
	cnt = 1;
	no_fail(ffs_write_block(slash, i, 1, &cnt));
	sync_wait(&cnt, 0);

	/* should succeed. */
	cnt = 1;
	no_fail(sys__xn_writeback(da_to_db(slash->da), 1, &cnt));
	sync_wait(&cnt, 0);

	/* now free & uninitialized. */

	/* should not let us allocate this disk block. */

	db = ui_get_db(slash, i);
	no_fail(ffs_free_block(slash, i));
#if 0
	/* put dependencies back in. */
	must_fail(ffs_alloc_block(slash, i, db));
#endif

	chkpt("First half of test 2");

	/* write should release db. */
	cnt = 1;
	no_fail(sys__xn_writeback(da_to_db(slash->da), 1, &cnt));
	sync_wait(&cnt, 0);
	no_fail(ffs_alloc_block(slash, i, db));
	assert(ui_get_db(slash, i) == db);

	/* Try it again. */
	must_fail(ffs_write_block(slash, i, 1, &cnt));
	no_fail(ffs_alloc_store(slash, i, FFS_ANY));
	cnt = 1;
	no_fail(ffs_write_block(slash, i, 1, &cnt));
	sync_wait(&cnt, 0);

	chkpt("3/4 test 2");

	cnt = 1;
	no_fail(sys__xn_writeback(da_to_db(slash->da), 1, &cnt));
	sync_wait(&cnt, 0);
	no_fail(ffs_free_block(slash, i));

	cnt = 1;
	no_fail(sys__xn_writeback(da_to_db(slash->da), 1, &cnt));
	sync_wait(&cnt, 0);
	no_fail(ffs_alloc_block(slash, i, db));
	assert(ui_get_db(slash, i) == db);

	no_fail(ffs_free_block(slash, i));
	cnt = 1;
	no_fail(sys__xn_writeback(da_to_db(slash->da), 1, &cnt));
	sync_wait(&cnt, 0);

	chkpt("Done with test 2");
}

int main(int argc, char *argv[]) {
	db_t root;
	int i;
	u_inode ui;
	xn_cnt_t cnt;
	cap_t c;
	da_t pa, da;

	/* this is the first slot ; used to hold /. */
	enum { ROOT_SLOT = 1 };

	if (argc > 1) {
	  	printf("shutdown: %d\n", sys__xn_shutdown());
	  	printf("init: %d\n", sys__xn_init());
		print_root_catalogue();
	  return 1;
	}


	/* Install our types. */
	if(specify_ffs_meta() < 0)
		fatal(Failed);

	printf("root catalogue holds: \n");
	print_root_catalogue();
	
	c = 0;
	root = install_root("ffs-root", c, inode_t);
	pa = da_compose(sys__root(), 0);
	da = da_compose(root, 0);
	printf("root catalogue holds: \n");

	ui_init(&ui, da, pa, c, c);

	/* Allocate backing store for root. */
	if((i = sys__xn_bind(root, -1, 0, XN_ZERO_FILL, 1)) < 0)
		fatal(Could not bind);
	else
		printf("allocated page %d\n", i);

	cnt = 2;
	no_fail(sys__xn_writeback(root, 1, &cnt));
	printf("waiting\n");
	sync_wait(&cnt, 1);
	no_fail(sys__xn_writeback(sys__root(), 1, &cnt));
	printf("waiting\n");
	sync_wait(&cnt, 0);

	no_fail(ffs_set_type(&ui, meta_t, 0));

		file_test(&ui);
		rules_test(&ui);
	printf("done!\n");
	return 0;

	/* Allocate some space for it. */
	for(i = 0; i < 10; i++)
		file_test(&ui);

	for(i = 0; i < 10; i++)
		rules_test(&ui);
	printf("done!\n");
	return 0;
}
