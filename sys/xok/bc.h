
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

#ifndef _XOK_BC_H_
#define _XOK_BC_H_

#include <xok/queue.h>
#include <xok/kqueue.h>
#include <xok/types.h>
#include <xok/capability.h>
#include <xok/disk.h>
#include <xok/xoktypes.h>

/* buffer */
KLIST_HEAD(buf_head, bc_entry, u16);
struct bc_entry {
  u32 buf_dev;	        /* identifies backing store object */
  union {
    u32 buf_blk;	        
    u_quad_t buf_blk64;
  } blk_u;              /* offset within object that block lives */
#define buf_blk blk_u.buf_blk
#define buf_blk64 blk_u.buf_blk64
  u32 buf_user1;	/* 1st word of user maintainable state */
  u32 buf_user2;	/* 2nd word of user maintainable state */
  u32 buf_ppn;	        /* physical page caching this block */
  u8 buf_dirty;		/* XXX seperate dirty flag */
#define BUF_CLEAN 0
#define BUF_DIRTY 1	/* block is dirty and needs to be written out */
  u8  buf_tainted;	/* The buffer is tainted: don't write out. */
  u16 buf_vp;	        /* virt pg we're mapped at = vpn(UBC) + buf_vp */
  u8 buf_writecnt;	/* number of outstanding writes for bc entry */
  u32 buf_state;	/* one of the following */
#define BC_EMPTY 0      /* buffer is invalid and no i/o in progress */
#define BC_VALID 1      /* block is valid */
#define BC_COMING_IN 2  /* block is being loaded */
#define BC_GOING_OUT 4  /* block is being written out from cache */
  union {			        /* kernel VAs only */
    LIST_ENTRY(bc_entry) buf_free;	/* next buffer on free list */
    KLIST_ENTRY(bc_entry, buf_head) buf_link;  /* link for buffers on same
						  queue */
  } buf_u;
#define buf_free buf_u.buf_free
#define buf_link buf_u.buf_link

  /* XXX - not used */
  KLIST_ENTRY(bc_entry, buf_head) buf_deps; /* points to blocks that require
					       us to be written before they
					       can be written */
};

#define BC_MAX_DISK_REQUEST_SIZE	(16 * NBPG)
#define BC_SYNC_ANY (MAX_DISKS + 1)
#define BC_NUM_QS 512		/* number of hash queues */
#define BC_MASK_QS 0x1ff

static inline unsigned int
__bc_hash (u32 dev, u32 blk)
{
  return ((dev + blk) & BC_MASK_QS);
}

static inline unsigned int
__bc_hash64 (u32 dev, u_quad_t blk)
{
  return ((dev + blk) & BC_MASK_QS);
}

/* buffer cache */
struct bc {
  struct buf_head bc_qs[BC_NUM_QS];	/* a bunch of hash-queues */
};

#ifdef KERNEL

LIST_HEAD(bc_free, bc_entry);	/* type of free list */

static inline int
bc_is_tainted (struct bc_entry *b)
{
  return (b->buf_tainted != 0);
}

static inline int
bc_is_dirty (struct bc_entry *b)
{
  return (b->buf_dirty == BUF_DIRTY);
}

static inline int
bc_is_reclaimable (struct bc_entry *b)
{
  return (! bc_is_tainted(b) && ! bc_is_dirty(b));
}

int bc_init (void);
void bc_print ();
struct bc_entry *bc_insert (u32 d, u32 b, u_int ppn, u8 state, int *error);
struct bc_entry *bc_insert64 (u32 d, u_quad_t b, u_int ppn, u8 state,
			      int *error);
struct bc_entry *bc_lookup (u32 d, u32 b);
struct bc_entry *bc_lookup64 (u32 d, u_quad_t b);
void bc_remove (struct bc_entry *buf);
void bc_remove_blocks (u32 dev, u_quad_t blk, u32 num);
void bc_flush_by_buf (struct bc_entry *buf);
void flush_a_buf (u32 d, u_quad_t b, int force);
void flush_a_buf2 (struct bc_entry*, int force);
void bc_set_clean (struct bc_entry *buf);
void bc_set_dirty (struct bc_entry *buf);
int bc_read_extent64 (u32 dev, u_quad_t block, u32 num, int *resptr,
		      int userreq);
int bc_read_extent (u32 dev, u32 block, u32 num, int *resptr);
int bc_write_extent64 (u32 dev, u_quad_t block, u32 num, int *resptr);
int bc_write_extent (u32 dev, u32 block, u32 num, int *resptr);
void bc_diskreq_done64 (u32 d, u_quad_t b, int write, char *addr, u_int envid);
void bc_diskreq_done (u32 d, u32 b, int write, char *addr, u_int envid);
int bc_can_remove (struct bc_entry *);

#endif /* KERNEL */

extern struct bc __bc;

#endif /* _XOK_BC_H_ */
