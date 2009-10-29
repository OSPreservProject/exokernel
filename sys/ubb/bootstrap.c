
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

#define __XN_MODULE__

/* DRE: kernel doesn't do synchronous disk activity; elided for correctness. */
#include <xn.h>
#include <kexcept.h>
#include <registry.h>
#include <kernel.h>
#include <demand.h>
#include <template.h>
#include "root-catalogue.h"
#include "sync-disk.h"
#include <xok/kerrno.h>
#include <xok/printf.h>
#include <xok/sysinfo.h>


/* DRE */
int xn_in_kernel;

#define SUPER_BLOCK_DB	1

static struct super_block {
	unsigned cookie;	/* cookie to verify superblock. */
	db_t 	r_db;		/* root catalogue */
	db_t 	t_db;		/* type catalogue */
	db_t 	f_db;		/* free map */
	size_t  f_nbytes;	/* number of bytes in freemap. */
	unsigned clean;		/* Was the shutdown clean? */
} super_block;

void init(void);

/* Hack. */
db_t t_db, f_db;
#define T_NBLOCKS	1

static unsigned compute_cookie(void *p) {
	unsigned *u;

	u = p;
	return (u[1] ^ u[2] ^ u[3] ^ u[4] ^ u[5]) + 133;
}

xn_err_t
sys_xn_info(db_t* r_db, db_t* t_db, db_t* f_db, size_t* f_nbytes) {
  if (r_db) *r_db = super_block.r_db;
  if (t_db) *t_db = super_block.t_db;
  if (f_db) *f_db = super_block.f_db;
  if (f_nbytes) *f_nbytes = super_block.f_nbytes;
  return XN_SUCCESS;
}

int XN_DEV;

void bc_check(char *msg, db_t db) {
	printf("msg = %s\n", msg);
	if(bc_lookup(XN_DEV, db))
		printf("db %ld: in bc %p\n", db, bc_lookup(XN_DEV, db));
}


/*
 * Initialize a disk.  In real life we will have to deal with
 * bootstrapping both the template file and root file.
 */
xn_err_t sys_xn_init(void) {
	int new_p;
#ifndef EXOPC
	char block[XN_BLOCK_SIZE];
#endif
	size_t f_blocks, nbytes;
	void *fm;

	SYSINFO_ASSIGN(si_xn_entries, 0);

	XN_DEV = 0;

	if (XN_DEV >= SYSINFO_GET(si_ndisks)) { 
	  printf ("No XN is being configured (XN_DEV %d, si_ndisks %d)\n", 
	      XN_DEV, SYSINFO_GET(si_ndisks));
	  return -E_NO_XN;
	} else {
	  printf ("Giving XN permissions to disk %d\n", XN_DEV);
        }

	init();

	/* Allocate blocks for the catalogues. */
	fm = db_get_freemap(&nbytes);
	super_block.f_nbytes = nbytes;
	f_blocks = bytes_to_blocks(nbytes);
	printf("free map takes up %d f_blocks %d\n", nbytes, f_blocks);
	f_db = SUPER_BLOCK_DB + 1;
	r_db = f_db + f_blocks;
	t_db = r_db + R_NBLOCKS;
	
	if(db_alloc(SUPER_BLOCK_DB, 1) != SUPER_BLOCK_DB)
		fatal(Could not allocate);
//	bc_check("checking super", SUPER_BLOCK_DB);

	if(db_alloc(f_db, f_blocks) != f_db)
		fatal(Could not allocate);
//	bc_check("checking freemap", f_db);
        if(db_alloc(r_db, R_NBLOCKS) != r_db)
                fatal(Could not allocate);
//	bc_check("checking root catalogue", r_db);
        if(db_alloc(t_db, T_NBLOCKS) != t_db)
                fatal(Could not allocate);
//	bc_check("checking type catalogue", t_db);
	
#ifdef EXOPC
	/* Always redo disk. */
	new_p = 1;
#else
	/* See if we are booting on a new disk. */
	sync_read(block, SUPER_BLOCK_DB, 1);
	memcpy(&super_block, block, sizeof super_block);
	new_p = (super_block.cookie != compute_cookie(&super_block));

	if(!new_p) {
		printf("old disk\n");
		assert(super_block.r_db == r_db);
		assert(super_block.t_db == t_db);
		assert(super_block.f_db == f_db);
		/* tells us if we have to reconstruct. */
		if(super_block.clean)
			printf("clean shutdown\n");
		else {
			printf("unclean shutdown\n");
			/* xn_reconstruct_disk(); */
		}

		/* read in free map. */
		fm = malloc(f_blocks * XN_BLOCK_SIZE);
		assert(fm);
		sync_read(fm, super_block.f_db, f_blocks);
		db_put_freemap(fm, nbytes);
		free(fm);

		root_init(new_p);
	} else
#endif
 {
		printf("new disk\n");
		super_block.r_db = r_db;
		super_block.t_db = t_db;
		super_block.f_db = f_db;
		super_block.f_nbytes = nbytes;
		super_block.clean = 1;
		super_block.cookie = compute_cookie(&super_block);

		/* Create new entry. */
		root_init(new_p); 	
	}
	super_block.clean = 0;
	super_block.cookie = compute_cookie(&super_block);

#ifndef EXOPC
	sync_write(SUPER_BLOCK_DB, &super_block, 1);
	sync_read(block, SUPER_BLOCK_DB, 1);
	assert(compute_cookie(block) == super_block.cookie);
#endif

        return XN_SUCCESS;
}

db_t sys_root(void) {
        return super_block.r_db;
}

xn_err_t sys_xn_shutdown(void) {
#ifndef EXOPC
	void *f;
	size_t nbytes, nblocks;
#endif

	/* DRE */
	/* Flush registry */
	try(xr_clean(XN_ALL));
	try(xr_flushall(XN_ALL));

#ifndef EXOPC
	/* Flush free map. */
	f = db_get_freemap(&nbytes);
	printf("nbytes = %d, fnbytes = %d\n", nbytes, super_block.f_nbytes);
	assert(nbytes == super_block.f_nbytes);
	nblocks = bytes_to_blocks(nbytes);
	sync_write(super_block.f_db, f, nblocks);

	/* Do last to make atomic. */
	super_block.clean = 1;
	super_block.cookie = compute_cookie(&super_block);

	sync_write(SUPER_BLOCK_DB, &super_block, 1);

	disk_shutdown();
#endif
	tmplt_shutdown();

	return XN_SUCCESS; 	/* Should reboot. */
}

xn_err_t sys_xn_format(void) {
	fatal(Not implemented);
	return XN_SUCCESS;
}

/* This will be a method. */
xn_err_t 
sys_install_mount(char *name, db_t *db, size_t nelem, xn_elem_t t, cap_t c) {
        xn_elem_t res;
	
	ensure(write_accessible(db, sizeof *db), 	XN_CANNOT_ACCESS);

	/* DRE */
	xn_in_kernel = 1;
		res = root_install(name, db, nelem, t, c);
	xn_in_kernel = 0;

        sys_return(res);
}


#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

