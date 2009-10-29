
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

#include <xok/sys_proto.h>
#include <xn.h>

xn_err_t
sys__xn_alloc (u_int sn, da_t da, struct xn_op * op, int nilp) {
  return sys_xn_alloc (da, op, nilp);
}

xn_err_t
sys__xn_free (u_int sn, da_t da, struct xn_op * op, int nilp) {
  return sys_xn_free (da, op, nilp);
}

xn_err_t
sys__xn_swap (u_int sn, da_t da1, struct xn_op * op1,
	                da_t da2, struct xn_op * op2) {
  return sys_xn_swap (da1, op1, da2, op2);
}

xn_err_t
sys__xn_move (u_int sn, da_t da1, struct xn_op * op1,
	                da_t da2, struct xn_op * op2) {
  return sys_xn_move (da1, op1, da2, op2);
}

xn_err_t
sys__xn_writeb (u_int sn, da_t da, void * src, size_t nbytes, cap_t cap) {
  return sys_xn_writeb (da, src, nbytes, cap);
}

xn_err_t
sys__xn_set_type (u_int sn, da_t da, int expected, void* src, size_t nbytes,
		  cap_t cap) {
  return sys_xn_set_type (da, expected, src, nbytes, cap);
}

xn_err_t
sys__xn_readb (u_int sn, void * dst, da_t da, size_t nbytes, cap_t cap) {
  return sys_xn_readb (dst, da, nbytes, cap);
}

xn_err_t
sys__xn_readin (u_int sn, db_t db, size_t nbytes, xn_cnt_t *cnt) {
  return sys_xn_readin (db, nbytes, cnt);
}

xn_err_t
sys__xn_writeback (u_int sn, db_t db, size_t nbytes, xn_cnt_t *cnt) {
  return sys_xn_writeback (db, nbytes, cnt);
}

xn_err_t
sys__xn_bind (u_int sn, db_t db, ppn_t ppn, cap_t cap, xn_bind_t b, int a) {
  return sys_xn_bind (db, ppn, cap, b, a);
}

xn_err_t
sys__xn_unbind (u_int sn, db_t db, ppn_t ppn, cap_t cap) {
  return sys_xn_unbind (db, ppn, cap);
}

xn_err_t
sys__xn_insert_attr (u_int sn, da_t da, struct xn_op * op) {
  return sys_xn_insert_attr (da, op);
}

xn_err_t
sys__xn_read_attr (u_int sn, void * dst, db_t db, size_t nbytes, cap_t cap) {
  return sys_xn_read_attr (dst, db, nbytes, cap);
}

xn_err_t
sys__xn_delete_attr (u_int sn, db_t db, size_t nbytes, cap_t cap) {
  return sys_xn_delete_attr (db, nbytes, cap);
}

xn_err_t
sys__xn_mount (u_int sn, struct root_entry * r, char * name, cap_t cap) {
  return sys_xn_mount (r, name, cap);
}

xn_err_t
sys__type_mount (u_int sn, char * name) {
  return sys_type_mount (name);
}

xn_err_t
sys__type_import (u_int sn, char * name) {
  return sys_type_import (name);
}

xn_err_t
sys__xn_info (u_int sn, db_t *r, db_t *t, db_t *f, size_t *m) {
  return sys_xn_info (r,t,f,m);
}

xn_err_t
sys__xn_init (u_int sn) {
  return sys_xn_init ();
}

db_t
sys__root(u_int sn) {
  return sys_root();
}

xn_err_t
sys__xn_read_catalogue (u_int sn, struct root_catalogue* cat) {
  return sys_xn_read_catalogue (cat);
}

xn_err_t
sys__xn_shutdown (u_int sn) {
  return sys_xn_shutdown ();
}

xn_err_t
sys__install_mount (u_int sn, char * name, db_t * db, size_t nelem, xn_elem_t t, cap_t c) {
  return sys_install_mount (name, db, nelem, t, c);
}

db_t
sys__db_find_free(u_int sn, db_t hint, size_t nblocks) {
  return sys_db_find_free (hint, nblocks);
}

