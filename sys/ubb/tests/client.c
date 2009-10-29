
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
 * A sort-of-tester.  Since it runs in the same address space, it obviously
 * is not prevented from doing nasty things to ubb verifier's data 
 * structures.  It is mainly used to test the interfaces and to
 * demonstrate the idea.
 */
#include <memory.h>
#include <stddef.h>
#include "ubb.h"
#include "ubb-lib.h"
#include "demand.h"

/* FUCKING gcc gives bullshit warnings for no effect left-hand comma expr. */
#define debug (void)

/* sort-of-unix inode. */
struct inode {
	/* permissions. */
	unsigned short uid;
	unsigned short gid;

	/* important times: when last read, written and created. */
      	unsigned mod;
	unsigned read;
	unsigned creat;

	/* pointers. */
#	define DIRECT_BLOCKS 15
	db_t db[DIRECT_BLOCKS];
	db_t indirect;
	db_t double_indirect;
	db_t triple_indirect;
};

enum { INODE_TYPE = 1, DISK_B, INDIRECT_B, DINDIRECT_B, TINDIRECT_B };

/* enumerate all blocks we have access too. */
static void i_access(void* meta, size_t index) {
	struct inode *inode;
	int i;

	demand(index == 0, bogus index!);
	inode = meta;
	for(i = 0; i < DIRECT_BLOCKS; i++)
		if(inode->db[i])
			ubb_add(inode->db[i], 1, DISK_B);
	
	if(inode->indirect)
		ubb_add(inode->indirect, 1, INDIRECT_B);
	if(inode->double_indirect)
		ubb_add(inode->double_indirect, 1, DINDIRECT_B);
	if(inode->triple_indirect)
		ubb_add(inode->triple_indirect, 1, TINDIRECT_B);
}

static size_t i_index(size_t off) {
	return 0;
}


#if 0
 Take lccs output and create a virtual risc? annotations
		via function calls....

 /* This would be a royal pain in asm. */
 dbi = indir(inode, offsetof(struct inode, db[i]));
 bool = v_eq(dbi, cnstu(0));
 stmt = v_ubb_add(dbi);
 stmt = v_if(bool, stmt);
 v_for(v_asgn(i, 0), v_lt(i, DIRECT_BLOCKS), v_add(i, i, cnst(i), stmt);
#endif

/* Create methods to read and write data. */

/* Create a write (memcpy(inode + offset, data, nbytes) */
size_t i_write(struct m_vec *m, void *data, int nbytes, int offset) {
	m->offset = offset;
	m->nbytes = nbytes;
	m->addr = data;
	return offset;
}

static int i_dwrite(struct m_vec *m, int d, db_t *db) {
	return i_write(m, db, sizeof *db, offsetof(struct inode, db[d]));
}

static void base_test(void) {
	static struct inode inode;
	m_vec m;
	db_t db;
	unsigned mod, ind;

	if(meta_init(&inode, i_access) < 0)
		assert(0);
	inode.db[0] = 1;
	if(meta_init(&inode, i_access) >= 0)
		assert(0);

	/* now allocate disk block 1. */
	db = 1;
	ind = i_index(i_dwrite(&m, 0, &db));

	if(meta_alloc(&inode, sizeof(inode), i_access, ind, db, &m, 1) < 0)
		assert(0);
	/* Should we prevent multiple additions? */
	if(meta_alloc(&inode, sizeof(inode), i_access, ind, db, &m, 1) < 0)
		assert(0);
	if(meta_alloc(&inode, sizeof(inode), i_access, ind, 11, &m, 1) >= 0)
		assert(0);

	/* Now initialize mod time to something. */
	mod = 14;
	ind = i_index(i_write(&m, &mod, sizeof mod,offsetof(struct inode, mod)));

	if(meta_write(&inode, sizeof(inode), i_access, ind, &m, 1) < 0)
		assert(0);

	ind = i_index(m.offset = offsetof(struct inode, db[1]));
	if(meta_write(&inode, sizeof(inode), i_access, ind, &m, 1) >= 0)
		assert(0);

	/* Now deallocate 1 */
	db = 0;
	ind = i_index(i_dwrite(&m, 0, &db));

	if(meta_dealloc(&inode, sizeof(inode), i_access, ind, 1, &m, 1) < 0)
		assert(0);

	if(meta_dealloc(&inode, sizeof(inode), i_access, ind, 11, &m, 1) >= 0)
		assert(0);
}

static void meta_test(ubb_p access_p, size_t (*index_p)(size_t)) {
	m_vec m;
	db_t db;
	unsigned mod;
	int i, n, res, offset, ind;
	static struct inode inode, copy;

	/* randomly delete and add disk blocks. */
	memset(&inode, 0, sizeof inode);
	for(i = 0; i < 1000; i++) {
		/* debug("trial %d\n", i); */
		memcpy(&copy, &inode, sizeof inode);

		switch(random() % 3) {
		/* allocate it. */
		case 0:
			n = random() % DIRECT_BLOCKS;
			db = random();
			ind = index_p(i_dwrite(&m, n, &db));
			/* debug("adding %d\n", db); */
			res = meta_alloc(&inode, sizeof(inode), access_p, ind, db, &m, 1);
			if(copy.db[n])
				demand(res < 0, should have failed!);
			else {
				demand(res >= 0, should have succeeded!);
				copy.db[n] = db;
			}
			break;

		/* free it. */
		case 1: 
			n = random() % DIRECT_BLOCKS;
			if(random() % 2)
				db = inode.db[n];
			else
				db = 0;
			ind = index_p(i_dwrite(&m, n, &db));
			/* debug("deleting %d\n", db); */
			res = meta_dealloc(&inode, sizeof(inode), access_p, ind, db, &m, 1);

			if(!db || db == copy.db[n])
				demand(res < 0, should have failed!);
			else {
				demand(res >= 0, should have succeeded!);
				copy.db[n] = 0;
			}
			break;

		/* modify non-disk data. */
		case 2:
			/* need to test unaligned. */
			offset = (random() % sizeof(struct inode)) & ~3;
			mod = random();
			ind = index_p(i_write(&m, &mod, sizeof mod, offset));

			/* debug("writing\n"); */
			res = meta_write(&inode, sizeof(inode), access_p, ind, &m, 1);

			if(offset > offsetof(struct inode, creat)) {
				demand(res < 0, should have failed!);
			} else {
				demand(res >= 0, should have succeeded!);
				memcpy((char *)&copy + offset, &mod, sizeof(mod));
			}
			break;
		}
		demand(memcmp(&copy, &inode, sizeof inode) == 0, 
				bogus values!);
	}

}

/*********************************************************************
 * Method testing. 
 */

static void inode_f(struct ubb_fun *f) {
	struct ubb_inst *ip;
	u_reg r;
	int i;

#       define offset(field) offsetof(struct inode, field)

	static void u_ident(u_reg r, size_t off, int type) {
		u_ldii(ip++, r, U_NO_REG, off);
		u_add_cexti(ip++, r, 1, type);
	}

	/* read/write the entire inode. */
	if(!u_add_rwset(&f->read_set, 0, sizeof(struct inode)))
		fatal(Could not allocate);
	if(!u_add_rwset(&f->write_set, 0, sizeof(struct inode)))
		fatal(Could not allocate);

	ip = f->insts;
	r = u_getreg();
	for(i = 0; i < DIRECT_BLOCKS; i++)
		u_ident(r, offset(db[i]), DISK_B);

	u_ident(r, offset(indirect), INDIRECT_B);
	u_ident(r, offset(double_indirect), DINDIRECT_B);
	u_ident(r, offset(triple_indirect), TINDIRECT_B);

	f->ninst = ip - f->insts;

#	undef offset
}

static void method_test(void) {
	struct ubb_fun f;

	static void interp_access(void* meta, size_t index) {
		if(!ubb_interp(&f, meta))
			fatal(Interp failed);
	}

	if(!u_alloc_f(&f, 1, UBB_SET, 1, 1))
		fatal(Could not allocate);

	inode_f(&f);

	if(!ubb_check(&f))
		fatal(Check failed!);

	meta_test(interp_access, i_index);
	u_delete_fun(&f);
}

/*********************************************************************
 * UDF Library test.
 */

/* subpartition inode. */
static void inode_sub_f(struct ubb_fun *f, int index) {
	u_reg r;
	struct ubb_inst *ip;
	size_t lb, ub;
	int type;

	static void u_ident(u_reg r, size_t off, int type) {
		u_ldii(ip++, r, U_NO_REG, off);
		u_add_cexti(ip++, r, 1, type);
	}

#       define offset(field) offsetof(struct inode, field)


	ip = f->insts;
	r = u_getreg();

	if(index >= 0 && index < DIRECT_BLOCKS) {
		lb = offset(db[index]);
		type = DISK_B;
	} else {
		switch(index) {
		case DIRECT_BLOCKS+0:
			lb = offset(indirect);
			type = INDIRECT_B;
			break;
		case DIRECT_BLOCKS+1:
			lb = offset(double_indirect);
			type = DINDIRECT_B;
			break;
		case DIRECT_BLOCKS+2:
			lb = offset(triple_indirect);
			type = TINDIRECT_B;
			break;
		default: fatal(Bogus index);
		}
	}
	u_ident(r, lb, type);
	ub = lb + sizeof(db_t);

	/* read/write the entire inode. */
	if(!u_add_rwset(&f->read_set, lb, ub))
		fatal(Could not allocate);
	if(!u_add_rwset(&f->write_set, lb, ub))
		fatal(Could not allocate);

	f->ninst = ip - f->insts;
#	undef offset
}

static size_t inode_sub_index(size_t off) {
	size_t base;

	base = offsetof(struct inode, db[0]);
	return (off < base) ?
		0 : /* really doesn't affect anything... */
		(off - base) / sizeof(db_t);
}

/* number of subpartitions. */
static int inode_nsub(void) {
	return DIRECT_BLOCKS+3;
}

static void lib_test(struct ubb_lib *l) {
	struct ubb_fun *fv;
	int i,n;

	static void interp_access(void* meta, size_t index) {
		struct ubb_fun *f;
		int n;

		f = &l->owns.funv[index];
		n = l->owns.nfun;

		demand(index < n, Bogus range!);
		if(!ubb_interp(f, meta))
			fatal(Interp failed);
	}

	fv = l->owns.funv;
	n = l->owns.nfun;

	for(i = 0; i < n; i++, fv++) {
		if(!u_alloc_f(fv, 1, UBB_SET, 1, 1))
			fatal(Could not allocate);

		inode_sub_f(fv, i);
	}

	if(!ubb_lib_check(l, sizeof(struct inode)))
		fatal(Library check failed);

	meta_test(interp_access, inode_sub_index);
}

static void library_test(void) {
	struct ubb_lib l;

	if(!u_liballoc(&l, inode_nsub()))
		fatal(Could not alloc);

	lib_test(&l);

	u_libdelete(&l);
}

/* Need to test the rest of the meta data. */

/******************************************************************
 *  Acl test. 
 */
static void acl_test(void) {
	struct ubb_type t;
	size_t lb, ub;
	enum { uid, gid };

	/* Should we seperate and let them build up? */
	if(!u_type_alloc(&t, "Inode", sizeof(struct inode), inode_nsub()))
		fatal(Could not allocate);

	/* Allocate our two caps. */
	if(!u_cap_alloc(&t, uid, offsetof(struct inode, uid)))
		fatal(Could not alloc);
	if(!u_cap_alloc(&t, gid, offsetof(struct inode, gid)))
		fatal(Could not alloc);

	lb = 0; ub = sizeof(struct inode);

	if(!u_region_alloc(&t, U_CAP, uid, U_READ | U_WRITE | U_EXEC, lb, ub))
		fatal(Could not alloc);	
	if(!u_region_alloc(&t, U_CAP, gid, U_READ | U_WRITE | U_EXEC, lb, ub))
		fatal(Could not alloc);	


	if(!ubb_type_check(&t))
		fatal(Failed!);
	lib_test(&t.l);

	u_type_delete(&t);
}



/*********************************************************************
 * Driver.
 */

int main(void) {
	base_test();	/* simple tests. */
	meta_test(i_access, i_index);	/* read/write/mod tests. */
	method_test();	/* check that functions are correctly sandboxed. */
	library_test();	/* check library ops. */
	acl_test();
	return 0;
}
