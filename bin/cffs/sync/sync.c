
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <xok/sys_ucall.h>
#include <exos/ubc.h>
#include <xok/sysinfo.h>
#include <exos/process.h>

static void write_buffer (struct bc_entry *b);

int main (int argc, char **argv) {
   int taintcnt = 0;
   struct bc_entry *b;
   int i;

sync_repeat:

   for (i = 0; i < BC_NUM_QS; i++) {
      for (b = KLIST_UPTR (&__bc.bc_qs[i].lh_first, UBC); b; b = KLIST_UPTR (&b->buf_link.le_next, UBC)) {
         if ((b->buf_dev < __sysinfo.si_ndisks) && (b->buf_dirty == BUF_DIRTY)) {
            if (b->buf_tainted) {
               taintcnt++;
            } else {
               write_buffer (b);
            }
         }
      }
   }

   if (taintcnt) {
      taintcnt = 0;
      goto sync_repeat;
   }

   exit (0);
}


#define MAX_CLUSTER 64		    /* max blocks to write per extent */
#define MAX_WRITE (4 * 1024 * 1024) /* max blocks to write per pass */

static void write_buffer (struct bc_entry *b) {
  u32 start, dev;
  int len;
  struct bc_entry *new;
  int ret;
  int resptr;
  int total = 0;

write_buffer_retry:

  start = b->buf_blk;
  dev = b->buf_dev;
  len = 1;

  /* try to grow request up */
  while (1 && total < MAX_WRITE) {
    if (len >= MAX_CLUSTER)
      break;
    new = __bc_lookup (dev, start+len);
    if ((!new) || (new->buf_tainted))
      break;
    if (new->buf_dirty == BUF_DIRTY)
      len++;
    else
      break;
  }

  /* try to grow request down */
  while (1) {
    if (len >= MAX_CLUSTER)
      break;
    new = __bc_lookup (dev, start-1);
    if ((!new) || (new->buf_tainted))
      break;
    if (new->buf_dirty == BUF_DIRTY) {
      start--;
      len++;
    }
    else
      break;
  }

  /* printf ("writing (%d, %d, %d)\n", dev, start, len); */
#if 1
  ret = sys_bc_write_dirty_bufs (dev, start, len, &resptr);
#else
  ret = sys__xn_writeback (start, len, &resptr);
#endif
  if (ret == XN_TAINTED) {
    kprintf ("tainted: this had better be a race condition and not a bug!!!\n");
    goto write_buffer_retry;
  }
  if (ret < 0) {
    kprintf ("syncer's write failed: returned %d\n", ret);
    kprintf ("SYNCER IS EXITING\n");
    exit (-1);
  }

  total += len;

  /* no need to wait for the requests since the kernel will update
     the dirty bits/buffer state automatically when the request is
     done. */
}

