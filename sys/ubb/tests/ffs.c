
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
 * FFS meta data specifications. 
 *
 */
#include <xn-struct.h>
#include <stddef.h>
#include <kernel/virtual-disk.h>
#include <memory.h>
#include <lib/demand.h>
#include "lib/ubb-lib.h"
#include "ffs.h"
#include "root-catalogue.h"
#include "spec.h"
#include "kernel/kernel.h"
#include "lib/sync-disk.h"

#define offset(field) offsetof(struct inode, field)

xn_elem_t s_indir_t, d_indir_t, t_indir_t, inode_t;
int byte_off;
int meta_t, bytes_t, not_meta_t, u_inode_t;

/* Create a write (memcpy(inode + offset, data, nbytes) */
static void m_write(struct xn_m_vec *m, size_t offset, void *data, int nbytes) {
        m->offset = offset;
        m->nbytes = nbytes;
        m->addr = data;
}


void ffs_write_db(struct xn_op *op, int i, db_t db, cap_t cap, int free_db) {
	static struct xn_m_vec m;
	static db_t s_db;

	demand(i < NDIRECT+3, index is too large);

	op->u.db = db;
	op->u.nelem = 1;
	if(i < NDIRECT)
		op->u.type = XN_DB;
	else {
		switch(i) {
		case NDIRECT+0: op->u.type = s_indir_t; break;
		case NDIRECT+1:
		case NDIRECT+2:
		case NDIRECT+3:
		default: fatal(Bogus index);
		}
	}

	op->m.cap = cap;
	op->m.own_id = i;
	op->m.n = 1;

        op->m.mv = &m;
	if(free_db) {
		s_db = 0;
        	m_write(&m, offset(db[i]) - byte_off, &s_db, sizeof s_db);
	} else
        	m_write(&m, offset(db[i]) - byte_off, &op->u.db, sizeof s_db);
}

/* Allocate a direct block */
void ffs_db_alloc(struct xn_op *op, int index, db_t db, cap_t cap) {
	ffs_write_db(op, index, db, cap, 0);
}
/* Free a direct block. */
void ffs_db_free(struct xn_op *op, int index, db_t db, cap_t cap) {
	ffs_write_db(op, index, db, cap, 1);
}

int xn_import(struct udf_type *t) {
	struct root_entry r;
	int res;
	size_t nblocks, nbytes;
	cap_t c;
	long db;
	int i, tid;
	xn_cnt_t n;
	da_t da;
	void *p;

	if(sys_type_mount(t->type_name) == XN_SUCCESS) {
		printf("hit in cache for %s\n", t->type_name);
		return XN_SUCCESS;
	} else 
		printf("<%s> did not hit in cache\n", t->type_name);

	c = 0;

	/* Allocate space, place in root, import. */
	if(sys_xn_mount(&r, t->type_name, c) == XN_SUCCESS) {
		xn_cnt_t cnt;
		ppn_t ppn;

		cnt = r.nelem;
		printf("reading %d\n", r.nelem);
		/* alloc storage. */
		for(ppn = -1, i = 0; i < r.nelem; i++, ppn++) {
			ppn = sys_xn_bind(r.db+i, ppn, 0, XN_FORCE_READIN, 1);
			if(ppn < 0) {
				printf("ppn = %d\n", ppn);
				fatal(Death);
			}
		}
		/* now need to load. */
		if((res = sys_xn_readin(r.db, r.nelem, &cnt)) != XN_SUCCESS) {
			printf("res = %d\n", res);
			fatal(Death);
		}
		sync_wait(&cnt, 0);	
		printf("hit in cache for %s\n", t->type_name);
		if((res = sys_type_mount(t->type_name)) < XN_SUCCESS) {
			printf("res = %d\n", res);
			fatal(SHould have hit);
		}
		return res;
	}

	nblocks = bytes_to_blocks(sizeof *t);
	db = 0;
	if(sys_install_mount(t->type_name, &db, nblocks, XN_DB, c) < 0)
		fatal(Mount failed);

	printf("nblocks = %ld\n", (long)nblocks);
	/* Allocate pages, and copyin. */
	for(i = 0; i < nblocks; i++) {
		if(sys_xn_bind(db+i, -1, c, XN_ZERO_FILL, 1) < 0)
			fatal(Could not bind);
		da = da_compose(db+i, 0);
		p = (char *)t + i * PAGESIZ;
		nbytes = (i == (nblocks - 1)) ? 
				(sizeof *t % XN_BLOCK_SIZE) : PAGESIZ;
		
		if((res = sys_xn_writeb(da, p, nbytes, c)) < 0) {
			printf("res = %d\n", res);
			fatal(Could not write);
		}
		n = 1;
		if(sys_xn_writeback(db+i, 1, &n) < 0)
			fatal(Cannot write back);
		sync_wait(&n, 0);
	}
	
	if((tid = sys_type_import(t->type_name)) < 0)
		fatal(Cannot import);
	return tid;
}

/* Use our own cap. */
static void emit_cap_check(struct udf_fun *f, int parent) {
	/* reuse a register for tmp_r. */
	enum { meta_r = 1, cap_r, tmp_r };
	struct udf_inst *ip;

	ip = f->insts;

	/* 
	 * XXX This is *gross* --- where does the parent come
	 * from after all?
	 */
	if(parent)
		u_sw_seg(ip++, UDF_PARENT + 0);

	u_ldii(ip++, tmp_r, meta_r, offset(gid) - byte_off);
	u_bne(ip++,  tmp_r, cap_r, 1);
		u_reti(ip++, 1);

	u_ldii(ip++, tmp_r, meta_r, offset(uid) - byte_off);
	u_bne(ip++,  tmp_r, cap_r, 1);
		u_reti(ip++, 1);

	/* Access failed. */
	u_reti(ip++, 0);

	f->ninst = ip - f->insts;
}

/* 
 * We allow anyone with a uid or gid to read/write everything.  
 * I guess.   Will need to beef this up to look at the permissions,
 * etc.
 * 
 * Writing this in assembly sucks dick.
 */
void specify_access(struct udf_type *t, int parent) {
	/* We treat all functions the same.  Ech. */
	emit_cap_check(&t->u.t.byte_access, parent);
	emit_cap_check(&t->u.t.block_access, parent);
}


/* Specify ffs meta data. */
int specify_ffs_meta(void) {
	struct udf_type t;

	AssertNow(sizeof(db_t) == sizeof(struct s_indir *));
	AssertNow(sizeof(db_t) == sizeof(struct d_indir *));
	AssertNow(sizeof(db_t) == sizeof(struct t_indir *));

	
	xn_begin(&t, "FFS indirect block", XN_STICKY);
		xn_fixed_seq(&t, 0, sizeof(db_t), 1, XN_DB);
		xn_fixed_seq(&t, 4, sizeof(db_t), N_S_INDIR-1, XN_BYTES);
#if 0
		xn_fixed_seq(&t, 0, sizeof(db_t), N_S_INDIR, XN_DB);
#endif
		specify_access(&t, 1);


	xn_end(&t, sizeof(struct s_indir));
	if((s_indir_t = xn_import(&t)) < 0)
		return s_indir_t;

	xn_begin(&t, "FFS double indirect block", XN_STICKY);
		xn_fixed_seq(&t, 0, sizeof(db_t), N_D_INDIR, s_indir_t);
		specify_access(&t, 1);
	xn_end(&t, sizeof(struct s_indir));
	if((d_indir_t = xn_import(&t)) < 0)
		return d_indir_t;

	xn_begin(&t, "FFS triple indirect block", XN_STICKY);
		xn_fixed_seq(&t, 0, sizeof(db_t), N_T_INDIR, d_indir_t);
		specify_access(&t, 1);
	xn_end(&t, sizeof(struct s_indir));
	if((t_indir_t = xn_import(&t)) < 0)
		return t_indir_t;

        /* curry the macros */
#       define offset(field) 	offsetof(struct inode, field)
#       define size(field) 	(sizeof ((struct inode *)0)->field)

	/* Create inode from a byte stream and sequences of block pointers. */
	xn_begin(&t, "FFS Inode Bytes", XN_NOT_STICKY);
           xn_fixed_seq(&t, offset(uid), size(uid), 1, XN_BYTES);
           xn_fixed_seq(&t, offset(gid), size(gid), 1, XN_BYTES);
           xn_fixed_seq(&t, offset(mod),	size(mod),   1, XN_BYTES);
           xn_fixed_seq(&t, offset(read), 	size(read),  1, XN_BYTES);
           xn_fixed_seq(&t, offset(creat),  	size(creat), 1, XN_BYTES);
	xn_end(&t, byte_off = offset(db[0]));

	if((bytes_t = xn_import(&t)) < 0)
		fatal(Failed);

	xn_begin(&t, "FFS Inode Meta", XN_NOT_STICKY);
           xn_fixed_seq(&t, offset(db[0])-byte_off, sizeof(db_t),NDIRECT,XN_DB);
           xn_fixed_seq(&t, offset(s_indir)-byte_off, sizeof(db_t),1,s_indir_t);
           xn_fixed_seq(&t, offset(d_indir)-byte_off, sizeof(db_t),1,s_indir_t);
           xn_fixed_seq(&t, offset(t_indir)-byte_off, sizeof(db_t),1,t_indir_t);

	   /* type field that we discriminate on. */
           xn_fixed_seq(&t, offset(type)-byte_off, size(type), 1, XN_UNION);
	xn_end(&t, sizeof(struct inode) - byte_off);

	if((meta_t = xn_import(&t)) < 0)
		fatal(Failed);

	xn_begin(&t, "FFS Inode Not-Meta", XN_NOT_STICKY);
           xn_fixed_seq(&t, offset(db[0])-byte_off, sizeof(db_t),NDIRECT,XN_BYTES);
           xn_fixed_seq(&t, offset(s_indir)-byte_off, sizeof(db_t),1,XN_BYTES);
           xn_fixed_seq(&t, offset(d_indir)-byte_off, sizeof(db_t),1,XN_BYTES);
           xn_fixed_seq(&t, offset(t_indir)-byte_off, sizeof(db_t),1,XN_BYTES);

	   /* type field that we discriminate on. */
           xn_fixed_seq(&t, offset(type)-byte_off, size(type), 1, XN_UNION);
	xn_end(&t, sizeof(struct inode) - byte_off);

	if((not_meta_t = xn_import(&t)) < 0)
		fatal(Failed);

	xn_begin(&t, "FFS Union Inode", XN_NOT_STICKY);
		t.class = UDF_UNION;
		t.u.s.ntypes = 2;
		t.u.s.ty[0] = not_meta_t;
		t.u.s.ty[1] = meta_t;
         	xn_spec_union(&t, offset(type)-byte_off, sizeof(unsigned));
	xn_end(&t, 0);
	t.nbytes = sizeof(struct inode) - byte_off;

	if((u_inode_t = xn_import(&t)) < 0)
		return u_inode_t;

	xn_begin(&t, "FFS Inode", XN_NOT_STICKY);
		t.class = UDF_STRUCT;
		t.u.s.ntypes = 2;
		t.u.s.ty[0] = bytes_t;
		t.u.s.ty[1] = u_inode_t;
         	xn_spec_union(&t, offset(type)-byte_off, sizeof(unsigned));

	xn_end(&t, 0);
	t.nbytes = sizeof(struct inode);

	if((inode_t = xn_import(&t)) < 0)
		return inode_t;


#       undef offset
#       undef size

	return XN_SUCCESS;
}

/*
 * (*DANGER*): we rely on reads past the end of the db array to do something
 * useful.
 */
db_t ui_get_db(u_inode *ui, size_t i) { 
	return	ui->incore.db[i]; 
}
db_t ui_get_da(u_inode *ui, size_t i)
        { return da_compose(ui_get_db(ui, i), 0); }
db_t ui_get_store(u_inode *ui, size_t i)
        { return ui->store[i]; }

/* hmm. */
void ui_init(u_inode *ui, da_t da, da_t p, cap_t uid, cap_t gid) {
        memset(ui, 0, sizeof *ui);
        ui->uid = uid;
        ui->gid = gid;
        ui->pa = p;
        ui->da = da + byte_off;
}

void ui_s_indir(u_s_indir *si, u_inode *p) {
	memset(si, 0, sizeof *si);
	si->uid = p->uid;
	si->gid = p->gid;
	si->pa = p->da;
	si->da = da_compose(ui_get_db(p, NDIRECT), 0);
}

/* Example of how to allocate and deallocate disk blocks.  */

/* Sugar for initializing the various data structures. */
void up_new(xn_update *up, db_t db, size_t n, xn_elem_t t)
    { up->db = db;  up->nelem = n; up->type = t; }

void mv_new(xn_m_vec *mv, size_t offset, void *addr, size_t nbytes) 
    { mv->offset = offset;  mv->addr = addr; mv->nbytes = nbytes; }

void mod_new(xn_modify *m, cap_t cap, int udf_id, xn_m_vec *mv, size_t n)
        { m->cap = cap; m->own_id = udf_id; m->n = n; m->mv = mv; }


/* Used by dealloc and alloc. */
xn_err_t 
ffs_realloc(xn_op *op, xn_err_t (*opr)(), da_t da, cap_t cap, int n, db_t db, int t) 
{
        xn_m_vec mv;

        /* Specify what to free/allocate. */
        up_new(&op->u, db, 1, t);

        /* Where to modify. */
        mv_new(&mv,offsetof(struct inode, db[n]), &db, sizeof db);

        /* What cap and UDF to use to perform modification. */
        mod_new(&op->m, cap, n, &mv, 1);

        return opr(da, op, 1);
}

/*
 * Routines to allocate/free direct blocks.  the UDF interpreting this
 * slot is named n.  ``in'' is a read-only copy of this value in the
 * kernel's buffer cache.
 */
int ffs_dalloc(u_inode *ui, int n, db_t hint) { 
	xn_err_t res;
	xn_op op;

        ffs_db_alloc(&op, n, hint, ui->uid);
	res = sys_xn_alloc(ui->da, &op, XN_NIL);

	/* Sanity check: read from xn. */
        if(res == XN_SUCCESS)
                ui->incore.db[n] = op.u.db;
	return res;
}

int ffs_dfree(u_inode *ui, int n) { 
	xn_op op;
	db_t db;

	db = ui_get_db(ui, n);
	return ffs_realloc(&op, sys_xn_free, ui->da, ui->uid, n, db, XN_DB); 
}

/* Allocate block for index; non-destructive on errors. */
int ffs_alloc_block(u_inode *ui, size_t index, long hint) {
        xn_op op;
        db_t db;
        int res;

        do {
		if(hint == FFS_ANY)
                	db = db_find_free(hint, 1);
		else
			db = hint;
                ffs_db_alloc(&op, index, db, ui->uid);
                res = sys_xn_alloc(ui->da, &op, XN_NIL);
        } while(hint < 0 && res == XN_CANNOT_ALLOC);

        if(res == XN_SUCCESS)
                ui->incore.db[index] = db;
        return res;
}

int ffs_free_block(u_inode *ui, size_t i) {
        xn_op op;

        ffs_db_free(&op, i, ui_get_db(ui, i), ui->uid);
        return sys_xn_free(ui->da, &op, XN_NIL);
}

/* Allocate backing store (non-destructive on errors, for testing). */
int ffs_alloc_store(u_inode *ui, size_t i, ppn_t hint) {
        ppn_t ppn;

        ppn = sys_xn_bind(ui_get_db(ui, i), -1, 0, XN_ALLOC_ZERO, 1);
        if(ppn > 0)
                ui->store[i] = ppn;
        return ppn;
}

int ffs_write_block(u_inode *ui, size_t index, size_t n, xn_cnt_t *cnt) {
	return sys_xn_writeback(ui_get_db(ui, index), n, cnt);
}
int ffs_read_block(u_inode *ui, size_t index, size_t n, xn_cnt_t *cnt) {
	return sys_xn_readin(ui_get_db(ui, index), n, cnt);
}

int ffs_write_bytes(u_inode *ui, size_t index, void *addr, size_t nbytes) {
        return sys_xn_writeb(ui_get_da(ui, index), addr, nbytes, ui->uid);
}

int ffs_read_bytes(u_inode *ui, size_t index, void *addr, size_t nbytes) {
        return sys_xn_readb(addr, ui_get_da(ui, index), nbytes, ui->uid);
}

int ffs_free_store(u_inode *ui, size_t i) {
        return sys_xn_unbind(ui_get_db(ui, i), ui_get_store(ui, i), ui->uid);
}

int ffs_write_data(u_inode *ui, size_t offset, void *addr, size_t nbytes) {
        return sys_xn_writeb(ui->da+offset-byte_off, addr, nbytes, ui->uid);
}

int ffs_read_data(u_inode *ui, size_t offset, void *addr, size_t nbytes) {
        return sys_xn_readb(addr, ui->da+offset-byte_off, nbytes, ui->uid);
}
#define offset(field) offsetof(struct inode, field)

int ffs_set_type(u_inode *ui, int ty, cap_t cap) {
     return sys_xn_set_type(ui->da + offset(type) - byte_off, ty, &ty, sizeof ty, cap);
}


