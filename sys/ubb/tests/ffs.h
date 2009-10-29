
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

#ifndef __FFS_H__
#define __FFS_H__

#include "lib/demand.h"

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
#       define DIRECT_BLOCKS 15
        db_t db[DIRECT_BLOCKS];
        struct s_indir *s_indir;
        struct d_indir *d_indir;
        struct t_indir *t_indir;

	/*  XXX for testing union. */
	unsigned type;
};

AssertNowF(sizeof(db_t) == sizeof(struct s_indir *));
AssertNowF(sizeof(db_t) == sizeof(struct d_indir *));
AssertNowF(sizeof(db_t) == sizeof(struct t_indir *));

enum { FFS_ANY = 0};


#define N_INDIR           (XN_BLOCK_SIZE/sizeof(db_t))
#define N_S_INDIR         N_INDIR
#define N_D_INDIR         N_INDIR
#define N_T_INDIR         N_INDIR

struct s_indir { db_t db[N_S_INDIR]; };
struct d_indir { struct s_indir *s_indir[N_D_INDIR]; };
struct t_indir { struct d_indir *d_indir[N_T_INDIR]; };

/* User-level meta data reps.  A bit too much duplication, but whatever. */
typedef struct u_inode {
        da_t da;                /* disk address of the inode itself. */
        da_t pa;                        /* parent. */

        cap_t   uid;
        cap_t   gid;

        size_t nbytes;          /* size of the file in bytes. */
        struct inode incore;    /* incore version of inode. */
#	define NDIRECT DIRECT_BLOCKS
        ppn_t  store[NDIRECT+3];  /* pages backing us up. */
} u_inode;

typedef struct u_s_indir {
	da_t da;		/* ours */
	da_t pa;		/* parents */

	cap_t uid;		/* inherited from parent. */
	cap_t gid;

	struct s_indir 	si;
	ppn_t 		store[N_S_INDIR];
} u_s_indir;

extern xn_elem_t s_indir_t, d_indir_t, t_indir_t, inode_t;
extern int meta_t, bytes_t, not_meta_t, u_inode_t;

void ffs_db_alloc(struct xn_op *op, int index, db_t db, cap_t cap);
void ffs_db_free(struct xn_op *op, int index, db_t db, cap_t cap);
int specify_ffs_inode(struct ubb_type *t);
db_t ui_get_db(u_inode *ui, size_t i);
db_t ui_get_da(u_inode *ui, size_t i);
db_t ui_get_store(u_inode *ui, size_t i);
void ui_init(u_inode *ui, da_t da, da_t p, cap_t uid, cap_t gid);
void ui_s_indir(u_s_indir *si, u_inode *ui);
int ffs_alloc_block(u_inode *ui, size_t index, long hint);
int ffs_free_block(u_inode *ui, size_t i);
int ffs_alloc_store(u_inode *ui, size_t i, ppn_t hint);
int ffs_write_bytes(u_inode *ui, size_t index, void *addr, size_t nbytes);
int ffs_read_bytes(u_inode *ui, size_t index, void *addr, size_t nbytes);
int ffs_free_store(u_inode *ui, size_t i);
int ffs_write_data(u_inode *ui, size_t offset, void *addr, size_t nbytes);
int ffs_read_data(u_inode *ui, size_t offset, void *addr, size_t nbytes) ;
int ffs_write_block(u_inode *ui, size_t index, size_t n, xn_cnt_t *cnt);
int ffs_read_block(u_inode *ui, size_t index, size_t n, xn_cnt_t *cnt);
int specify_ffs_meta(void);

int ffs_dalloc(u_inode *ui, int n, db_t hint);
int ffs_dfree(u_inode *ui, int n);
int ffs_set_type(u_inode *ui, int ty, cap_t cap);

#endif /* __FFS_H__ */
