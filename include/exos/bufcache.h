
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

#ifndef __BUFCACHE_H_
#define __BUFCACHE_H_

#include <xok/sys_ucall.h>
#include <sys/types.h>
#include <exos/ubc.h>


#define exos_bufcache_lookup(a,b)	__bc_lookup((a),(b))
/* struct bc_entry *exos_bufcache_lookup (u32 dev, u32 blk); */

void * exos_bufcache_map (struct bc_entry *bc_entry, u32 dev, u32 blk,
			  u_int writeable);
void * exos_bufcache_map64 (struct bc_entry *bc_entry, u32 dev, u_quad_t blk64,
			    u_int writeable);
int exos_bufcache_unmap (u32 dev, u32 blk, void *ptr);
int exos_bufcache_unmap64 (u32 dev, u_quad_t blk64, void *ptr);
void * exos_bufcache_alloc (u32 dev, u32 blk, int zerofill, int writeable,
			    int usexn);
int exos_bufcache_initfill (u32 dev, u32 blk, int blockcount, int *resptr);
void * exos_bufcache_insert (u32 dev, u32 blk, void *ptr, int usexn);

#define exos_bufcache_setDirty(d,b,v)	sys_bc_set_dirty(d,b,v)
/* int exos_bufcache_setDirty (u32 dev, u32 blk, int cnt, int *resptr); */

int exos_bufcache_initwrite (u32 dev, u32 blk, int blockcount, int *resptr);

#define exos_bufcache_isvalid(bc_entry) \
	((bc_entry->buf_state & BC_VALID) != BC_EMPTY)

#define exos_bufcache_isdirty(bc_entry) \
	(bc_entry->buf_dirty == BUF_DIRTY)

#define exos_bufcache_isiopending(bc_entry) \
	((bc_entry->buf_state & (BC_COMING_IN|BC_GOING_OUT)) != 0)

#define exos_bufcache_issafetoread(bc_entry) \
	((bc_entry->buf_state & (BC_COMING_IN|BC_VALID)) == BC_VALID)

#define exos_bufcache_issafetomodify(bc_entry) \
	((bc_entry->buf_state & (BC_COMING_IN|BC_GOING_OUT|BC_VALID) == BC_VALID)

void exos_bufcache_waitforio (struct bc_entry *bc_entry);

#endif
