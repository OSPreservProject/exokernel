/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */


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

#ifndef _XOK_PXN_H_
#define _XOK_PXN_H_

#include <xok/types.h>
#include <xok/capability.h>
#include <xok/queue.h>
#include <xok/disk.h>
#include <xok/xoktypes.h>

LIST_HEAD (Xn_xtnt_ref_list, Xn_xtnt_ref);

/* a refernce to an extent */
struct Xn_xtnt_ref {
  struct Pxn *xr_pxn;		/* who the pxn is */
  int xr_bptr;			/* what bptr the extent is */
  LIST_ENTRY(Xn_xtnt_ref) xr_next; /* only used as part of xtnt_forged_to */
};

/* a single extent of blocks */
struct Xn_xtnt {
  u64 xtnt_block;
  u64 xtnt_size;
  struct Xn_xtnt_ref_list xtnt_forged_to; /* pxns who inherited this extent
					     from us */
  struct Xn_xtnt_ref xtnt_forged_from; /* where we came from */
};

/* unique name for a pxn. xa_dev must the device the pxn applies to, xa_name
   is opaque and used entirely by the user. */
struct Xn_name {
#define PXN_PSEUDO_DEV 0x10000000
  u32 xa_dev;
  u64 xa_name;
};

/* a pointer to either an extent or a pxn */
struct Xn_bptr {
#define BPTR_EMPTY 0
#define BPTR_XTNT 1
#define BPTR_PXN 2
  u8 bptr_type;
#define bptr_xtnt bptr_u.xtnt
#define bptr_pxn bptr_u.pxn
  union {
    struct Xn_xtnt xtnt;
    struct Pxn *pxn;
  } bptr_u;
};

/* in-core disk protection structure. Basically, an acl that protects 
   a list of extents and a list of other pxn's */
struct Pxn {
  u16 pxn_bptr_sz;		 /* number of bptrs in this pxn */
  u8 pxn_xtnt_acl_max;		 /* max number of capabilities protecting
				    extents */
  u8 pxn_pxn_acl_max;		 /* max number of capabilities protecting
				    pxn */
  cap *pxn_xtnt_acl;		 /* ptr to extent acl */
  cap *pxn_pxn_acl;		 /* ptr to pxn acl */
  struct Xn_bptr *pxn_bptrs;	 /* ptr to bptrs */
  struct Xn_name pxn_name;	 /* name of on-disk xnode we refer to,
				    if any (a kind of pxn ID) */
  int pxn_refcnt;		 /* how many other pxn's point to us */
  u8 pxn_frozen;		 /* can't change the caps on this */
  LIST_ENTRY(Pxn) pxn_link;	 /* next pxn */
  TAILQ_ENTRY(Pxn) pxn_lru_link; /* next pxn */
};

/* exported functions */
static inline int is_pxn_disk_dev (struct Xn_name *n) {
  return (n->xa_dev < MAX_DISKS);
}

#ifdef KERNEL
struct Pxn *lookup_pxn (struct Xn_name *n);
void pxn_init (void);
int pxn_alloc (u8 pxn_caps, u8 xtnt_caps, u16 bptrs, cap *c_pxn, cap *c_xtnt,
	       struct Xn_name *n);
void pxn_remove_xtnt (struct Pxn *p, struct Xn_xtnt *xtnt);
int pxn_remove_pxn (struct Pxn *p1, struct Pxn *p2, cap *c_p1);
void pxn_dealloc (struct Pxn *p);
int pxn_add_cap (struct Xn_name *n, cap *c_new, cap *c_pxn, int flag);
int pxn_add_xtnt (struct Pxn *dst_pxn, struct Pxn *src_pxn, 
		  struct Xn_xtnt *xtnt, cap *c_dst, cap *c_src_pxn,
		  cap *c_src_xtnt);
int pxn_add_pxn (struct Pxn *dst_pxn, struct Pxn *src_pxn, cap *c_dst,
		 cap *c_src);
int pxn_authorizes_xtnt (struct Pxn *pxn, cap *c, struct Xn_xtnt *xtnt, 
			 int access, int *error);
int pxn_kernel_add_xtnt (struct Xn_name *xn, struct Xn_xtnt *xtnt);
int pxn_verify_access (struct Pxn *p, cap *c, int access, int *error);
#endif

#endif /* _XOK_PXN_H_ */
