
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


#ifndef _XOK_DISK_H_
#define _XOK_DISK_H_

#include <xok/buf.h>
#include <xok/types.h>

#define MAX_DISKS 8
#define DISK_MAX_SCATTER	257

struct disk {
  u_int d_id;			/* partition number (ie which si_disks) */
  u_int d_dev;			/* wich disk this partition is on */

  u_quad_t d_size;		/* sectors per disk */
  u_int d_bsize,		/* block(sector) size */
    d_bshift,
    d_bmod;

  u_int d_cyls,			/* cylinders per disk */
    d_heads,			/* heads(tracks) per cylinder */
    d_sectors;			/* sectors per head(track) */

  u_int d_ongoingReads,
    d_ongoingWrites;

  u_int d_queuedReads,
    d_queuedWrites;

  u_int d_readcnt,
    d_writecnt;

  u_quad_t d_readsectors,
    d_writesectors;

  u_int d_intrcnt;
  void (*d_strategy)(struct buf *);
  u_quad_t d_part_off;		/* in sectors from start of disk */
};

#ifdef KERNEL

void disk_giveaway (void);
struct buf *disk_buf_alloc (void);
void disk_buf_free (struct buf *buf);
void disk_buf_free_chain (struct buf *buf);

u_quad_t disk_size (u_int dev);
u_int disk_sector_sz (u_int dev);

/* for the bc code -- these calls are internal */
int disk_prepare_bc_request (u_int devno, u_quad_t blkno, void *vaddr,
			     u_int flags, int *resptr, struct buf **headbp);
int disk_bc_request (struct buf *bp);
int disk_pollforintrs (u_int devno);
#endif

#endif /* !_XOK_DISK_H_ */
