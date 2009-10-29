
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

#ifndef XNODE_KERNEL_H
#define XNODE_KERNEL_H

#include <ubb/xntypes.h>

struct xn_m_vec;	/* specify update to meta data. */
struct xn_modify;	/* specify how to modify meta data. */
struct xn_update;	/* Specifies what external transition should occur. */
struct xn_op;		/* Encapsulate the above state. */
struct ubb_type;	/* Specify a given type. */
struct xn_iov;

extern struct xr* xn_registry;
#ifdef KERNEL
extern char* __xn_free_map;
#else
extern char __xn_free_map;
#endif

xn_err_t
sys_xn_info(db_t* r_db, db_t* t_db, db_t* f_db, size_t* f_nbytes);

/* 
 * Import a set of client-defined types into the kernel.  
 * Returns the type names.
 */
xn_err_t sys_type_import(char *type_name);
xn_err_t sys_type_mount(char *type_name); 	

/*
 * Allocate space for the root of an application-level file system.
 * Installs the type and pointer to the root in a ``well-known''
 * location on disk.  After allocation, applications can then alloc,
 * read, write, etc as normal.  On reboot, the kernel can (logically) 
 * traverse file systems from these roots, garbage collecting and performing
 * consistency checks.  Fails if:
 *	- root is an invalid disk block or is already allocated.
 *	- type is an invalid type.
 */
xn_err_t sys_install_mount(char *name, db_t *db, size_t nelem, xn_elem_t t, cap_t c);

/* Simple mount: looks up name an returns the entire entry. */
struct root_entry;
xn_err_t sys_xn_mount(struct root_entry *r, char *name, cap_t c); 	

/* 
 * Return the disk block of the root file system.  Clients use this for 
 * bootstrapping.
 */
db_t sys_root(void);

xn_err_t sys_xn_init(void); 		/* Initialize xn. */
xn_err_t sys_xn_shutdown(void); 	/* Shutdown disk. */
xn_err_t sys_xn_format(void); 		/* First time formating. */


/*******************************************************************
 * Meta-data update routines. 
 */

/*
 * Allocate: [db, db + (sizeof type * nelem)/disk sector size) ] and
 * write [db, nelem] into the meta data.
 * Fails if:
 * 	- the capability does not work.
 *	- the blocks are not free.
 *	- da is not in core.
 *	- own_id is invalid or mv contains bogus data.
 *	- the UDF returns the wrong data. 
 *	- nilp is set and the previous ptr of the subpartition is not nil.
 */
xn_err_t sys_xn_alloc(da_t da, struct xn_op *op, int nilp);

/*
 * Free: [db, db + (sizeof type * nelem)/disk sector size) ] 
 * Fails for similar reasons to the deallocation. 
 *	- nilp is set and after the modification there are still blocks.
 */
xn_err_t sys_xn_free(da_t da, struct xn_op *op, int nilp);

/* 
 * Place value described in m1 into v2, and in m2 into v1.  Fails:
 *	- m1/m2 do not describe valid contents of v1/v2 (either before
 * 	or after).
 *	- capabilities fail.
 */
xn_err_t sys_xn_swap(da_t v1, struct xn_op *m1, da_t v2, struct xn_op *m2);

/* Identical to swap except that the destination contains a nil pointer. */
xn_err_t sys_xn_move(da_t dst, struct xn_op *msrc, da_t src, struct xn_op *mdst);

/***************************************************************
 * Routines to move data from/to disk and from/to buffer cache.
 */

/* 
 * Copy [src, src + nbytes) to [da, da + nbytes) (all of the bytes must
 * be in core).  Fails if:
 *	- The capability is insufficient for any block [db, db + nbytes).
 *	- Any block is out of core.
 *	- A page in the range is invalid.
 */
xn_err_t sys_xn_writeb(da_t da, void * src, size_t nbytes, cap_t cap);

/* 
 * Copy [da, da + nbytes) to [dst, dst + nbytes).  Fails for reasons 
 * identical to above, with the addition:
 *	- [dst, dst + nbytes) is not writable.
 */
xn_err_t sys_xn_readb(void * dst, da_t da, size_t nbytes, cap_t cap);

/* 
 * Write [db, db + nblocks) back to disk.  Note: it does not require 
 * access checks.  Fails if:
 * 	- any db is invalid.
 */
xn_err_t sys_xn_writeback(db_t db, size_t nblocks, xn_cnt_t *cnt);
xn_err_t sys_xn_writebackv(struct xn_iov *iov);

/* 
 * Read the range [db, db+nblocks) from disk.  Fails if:
 *	- we miss in the registry.
 *	- There is no physical page to read the values into.
 */
xn_err_t sys_xn_readin(db_t db, size_t nblocks, xn_cnt_t *cnt);
xn_err_t sys_xn_readinv(struct xn_iov *iov);

/* 
 * Associate ppn with db.  If ppn = -1, then it allocates a
 * new page (-2 then zero-fills it as well).  The data in the page
 * is not bound until either (1) a readin occurs, or (2) 
 * bind is set and the process has write permission.
 */
xn_err_t sys_xn_bind(db_t db, ppn_t ppn, cap_t cap, xn_bind_t flag, int alloc);

/* 
 * Free page associated with db.  If ppn > 0 then it checks to 
 * ensure that the given page is associated with the registry
 * entry.
 */
xn_err_t sys_xn_unbind(db_t db, ppn_t h_ppn, cap_t cap);

/***********************************************************************
 * Registry manipulation routines. 
 */

/*
 * Install a binding of (cache entry -> disk block) based on the
 * pointer named by the disk address.  This call is provided primarily
 * for space efficiency: applications can install attributes without
 * having to page the object in.  This can be useful for operations
 * such as sub-partitioning.  Note that allocation, deletion and 
 * reading and writing all perform this call implicitly since they
 * require registry entries.  Fails if:
 *	- The capability is insufficient
 *	- The da is non-sensical (implies the first)
 *	- [db, db+nelem) is not guarded by the pointer
 */
xn_err_t sys_xn_insert_attr(da_t da, struct xn_op *op);

/* 
 * Read the block's registry attributes (should be mappable too).   
 * Fails if:
 *	- The range [dst, dst + sizeof attr) is not available
 *	for writing.
 * 	- The capability is insufficient.
 * 	- The disk block's attributes are not in core.
 */
xn_err_t sys_xn_read_attr(void * dst, db_t db, size_t nblocks, cap_t cap);

/*
 * Delete a registry entry.  Fails if: 
 *	- the entry is guarding a violated constraint.
 *	- the entry is in use.
 */
xn_err_t sys_xn_delete_attr(db_t db, size_t nblocks, cap_t cap);

/* Set field to determine union.  Must be nil, first, then the given type. */
xn_err_t
sys_xn_set_type(da_t da, int expected_ty, void *src, size_t nbytes, cap_t cap);

struct root_catalogue;
xn_err_t sys_xn_read_catalogue(struct root_catalogue *c);

db_t sys_db_find_free (db_t hint, size_t nblocks);

#if defined(EXOPC) && !defined(KERNEL)
#define	sys_xn_alloc	sys__xn_alloc
#define	sys_xn_free	sys__xn_free
#define	sys_xn_swap	sys__xn_swap
#define	sys_xn_move	sys__xn_move
#define	sys_xn_writeb	sys__xn_writeb
#define	sys_xn_set_type	sys__xn_set_type
#define	sys_xn_readb	sys__xn_readb
#define	sys_xn_readin	sys__xn_readin
#define	sys_xn_writeback	sys__xn_writeback
#define	sys_xn_bind	sys__xn_bind
#define	sys_xn_unbind	sys__xn_unbind
#define	sys_xn_insert_attr	sys__xn_insert_attr
#define	sys_xn_read_attr	sys__xn_read_attr
#define	sys_xn_delete_attr	sys__xn_delete_attr
#define	sys_xn_mount	sys__xn_mount
#define	sys_type_mount	sys__type_mount
#define	sys_type_import	sys__type_import
#define	sys_xn_info	sys__xn_info
#define	sys_xn_init	sys__xn_init
#define	sys_root	sys__root
#define	sys_xn_shutdown	sys__xn_shutdown
#define	sys_xn_read_catalogue	sys__xn_read_catalogue
#define	sys_install_mount	sys__install_mount
#define	sys_db_find_free	sys__db_find_free
#endif

#endif /* XNODE_KERNEL_H */

