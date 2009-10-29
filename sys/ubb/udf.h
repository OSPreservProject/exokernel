
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

#ifndef __UDF_H__
#include "ubb.h"

int udf_checkpt(struct udf_ckpt *ck, void *meta, xn_op *op);
void udf_rollback(void *meta, struct udf_ckpt *ckpt);
int udf_contains(int type, void *meta, xn_op *op);
int udf_write(int type, void *meta, xn_op *op);
int udf_alloc(int type, void *meta, xn_op *op);
int udf_dealloc(int type, void *meta, xn_op *op);
int udf_move(int type, void *meta, xn_op *op, xn_update *o_add, xn_update *o_del);
int udf_init(struct udf_type *t);
struct udf_fun *udf_lookup(struct udf_fun **read_f, int type, void *meta, size_t own_id, cap_t c);
void udf_tprint(struct udf_type *t);
int udf_getset(struct udf_ext *e, struct udf_fun *f, int index);
int udf_illegal_access(size_t offset, size_t nbytes, struct udf_ext *e);


int udf_type_check(struct udf_type *t);
void udf_add(db_t db, size_t n, int type);
int udf_interp(struct udf_fun *f, struct udf_ctx *ctx, void *xn, unsigned *params, int narg, struct udf_ext *r_access, struct udf_ext *w_access);
int udf_check(struct udf_fun *f);
int u_in_set(struct udf_ext *s, size_t lb, size_t len);
void udf_dis(struct udf_inst *ip, int n);

int udf_blk_access(int op, struct xr *xr, void *meta, cap_t cap, size_t index);

int udf_switch_seg(udf_ctx *ctx, int n, void **meta, udf_ext **r_access, udf_ext **w_access);

int udf_get_union_type(udf_type *t, void *meta);
int udf_type_init(struct udf_type *t);

xn_err_t
udf_set_type(int ty, int exp, void *meta, size_t off, void *p, size_t nbytes);

xn_err_t udf_free_all(udf_type *t, void *meta);

#endif

