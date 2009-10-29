
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
   The kernel buffer cache is just a hash-queue of buffer-headers.
   Each buffer-header describes one page in memory and the block
   contained in that page. Each env gets a read-only view of the
   hash-queues and the headers so procs can lookup blocks and directly
   map the relevant page (each page should be protected by the same
   capabilities that would have protected the underlying disk block).

   Protection is done via proxy xnodes. In order to insert a block
   into the cache the user must have a pxn and a capability for the
   pxn that says the user has write access to the block.

   In each env the first page at UBC contains a struct bc that
   actually holds the heads of the hash queues. Subsequant pages
   contain the actual headers. Unfortunately, we have to have two sets
   of pointers for the queues. The kernel allocates non-contiguous
   pages dynamically for the struct buf's it uses. The user get's
   these pages mapped sequentially starting at UBC. Thus, the pointers
   the kernel uses to follow links do not work for the user nor is
   there an easy way to convert between them.

   Instead, each buf remembers what page above UBC it is allocated on
   so that we can take the lower bits from the kernel's pointers and
   OR them onto the vpn we get from the buf struct and get a pointer
   that's valid in user-land.
*/

/*
   XXX -- if XN is defined we also hook into XN for
   protection. Basically this means that pxn's do not have to be used
   and blocks are read and writtne via XN's interface (instead of
   bc_read_and_insert). We call back into XN to make sure it's ok to
   remove blocks from the cache and stuff like that. Really need to
   document all of this better.
*/

/* XXX -- remove 64 and 32 bit redundant interfaces */

/* XXX - NOT SYNCHRONIZED */

#define __BC_MODULE__

#include <xok/bc.h>
#include <xok/defs.h>
#include <xok/disk.h>
#include <xok/kerrno.h>
#include <xok/printf.h>
#include <xok/pxn.h>
#include <xok/queue.h>
#include <xok/scheduler.h>
#include <xok/sys_proto.h>
#include <xok/syscallno.h>
#include <xok/types.h>
#include <xok_include/assert.h>
#include <xok/sysinfo.h>
#include <xok/pmap.h>

static struct bc_free bc_free_list;	/* start of free list of bufs */
struct bc *bc;		                /* the buffer cache itself */
Pte *bc_upt;			        /* page table for buffer cache */

int
bc_init (void)
{
  int i;
  struct bc_entry *b;

  StaticAssert (sizeof (struct bc) <= NBPG);

  /* init the hash queues */
  for (i = 0; i < BC_NUM_QS; i++)
    KLIST_INIT (&bc->bc_qs[i]);
  
  /* Use rest of page allocate for bc as bc_entries */
  LIST_INIT (&bc_free_list);
  i = (sizeof(*bc) + sizeof(struct bc_entry) - 1) / sizeof(struct bc_entry);
  b = (struct bc_entry *)((u_int)bc + i*sizeof(struct bc_entry));
  for (; i < NBPG/sizeof(struct bc_entry); i++) {
    LIST_INSERT_HEAD (&bc_free_list, b, buf_free);
    b->buf_vp = 0; /* these are on the 0th page of the bc region */
    b++;
  }

  return (0);
}

/* Allocate a page in the buffer cache pt, divide it up into buffer
   headers and put them all onto the free list. */

static int
bc_alloc_bufs ()
{
  struct bc_entry *b;
  struct Ppage *pp;
  int i;
  static u16 next_page = 1;	/* first page in pt holds bc */
  
  /* buf_vp can't exceed the number of entries (1024) in one page directory */
  if (next_page >= 1024) return -E_VSPACE;

  if (ppage_alloc (PP_KERNRO, &pp, 0) < 0) return -E_NO_MEM;

  b = (struct bc_entry *)pp2va (pp);
  /* map the page readonly into everyone's UBC area */
  bc_upt[next_page] = ((unsigned )b - KERNBASE) | PG_P | PG_U | PG_W;

  for (i = 0; i < NBPG/sizeof (struct bc_entry); i++) {
    LIST_INSERT_HEAD (&bc_free_list, b, buf_free);
    b->buf_vp = next_page;
    b->buf_state = BC_EMPTY;
    b++;
  }

  next_page++;

  return (0);
}

/* fill in a buffer header and put it on a queue */

struct bc_entry *
bc_insert64 (u32 d, u_quad_t b, u_int ppn, u8 state, int *error)
{
  struct bc_entry *buf;
  int r;

  if (ppn >= nppage) {
    *error = -E_INVAL;
    return NULL;
  }

  buf = bc_free_list.lh_first;
  if (!buf) {
    if ((r = bc_alloc_bufs ()) != 0) {
      *error = r;
      return (NULL);
    }
    buf = bc_free_list.lh_first;
  }
  LIST_REMOVE(buf, buf_free);

  buf->buf_dev = d;
  buf->buf_blk64 = b;
  buf->buf_ppn = ppn;
  buf->buf_state = BC_VALID | state;
  buf->buf_writecnt = 0;
  buf->buf_dirty = BUF_CLEAN;
  buf->buf_tainted = 0;
  buf->buf_user1 = 0;
  buf->buf_user2 = 0;

  /* remember that this page holds this buffer */
  /* XXX - what if there's already a buf on the ppage? */
  Ppage_pp_buf_set(ppages_get(ppn), buf);
  
  /* this page can no longer be manipulated using
     the normal ppage routines... */
  ppage_acl_zero(ppages_get(ppn));

  /* link it into the queue */
  KLIST_INSERT_HEAD (&bc->bc_qs[__bc_hash64 (d, b)], buf, buf_link, buf_vp);

  return (buf);
}

struct bc_entry *
bc_insert (u32 d, u32 b, u_int ppn, u8 state, int *error)
{
  return (bc_insert64 (d, (u_quad_t )b, ppn, state, error));
}

/* find a buffer in the cache */

struct bc_entry *
bc_lookup64 (u32 d, u_quad_t b)
{
  struct bc_entry *buf;

  buf = KLIST_KPTR (&bc->bc_qs[__bc_hash64 (d, b)].lh_first);
  while (buf) {
    /* compare block first, more likely to be different */
    if (buf->buf_blk64 == b && buf->buf_dev == d) {
      return (buf);
    }
    buf = KLIST_KPTR (&buf->buf_link.le_next);
  }

  return (NULL);
}

struct bc_entry *
bc_lookup (u32 d, u32 b)
{
  return (bc_lookup64 (d, (u_quad_t )b));
}

/* print the buffer cache */

void
bc_print ()
{
  int i = 0;

  for (i = 0; i < BC_NUM_QS; i++) {
    struct bc_entry *buf, *next;
    int q_printed = 0;

    for (buf = KLIST_KPTR (&bc->bc_qs[i].lh_first); buf; buf = next) {
      if (!q_printed) {
	printf ("%d:", i);
	q_printed = 1;
      }
      printf (" %qu", buf->buf_blk64);
      next = KLIST_KPTR (&buf->buf_link.le_next);
    }
    if (q_printed)
      printf("\n");
  }
}

/* remove a buffer from the buffer cache */

void
bc_remove (struct bc_entry *b)
{
  struct Ppage *pp;
#ifdef XN
  extern int xn_remove (u32, u32, int);

  /* xn_remove just returns ok for all non-XN devices */
  assert (xn_remove (b->buf_dev, b->buf_blk, 1));
#endif

  assert (b);
  assert (b->buf_state & BC_VALID);
  pp = ppages_get(b->buf_ppn);
  assert (Ppage_pp_buf_get(pp) == b);
  KLIST_REMOVE (b, buf_link);
  LIST_INSERT_HEAD (&bc_free_list, b, buf_free);
  b->buf_state = BC_EMPTY;
  Ppage_pp_buf_set(pp, NULL); // pp->pp_buf = NULL;
}

/* Manipulate the dirty bits on buffers. We wrap these in functions
   since the dirty bit handling has a habit of changing quite a bit
   over time.

   bc_is_dirty is an inline defined in xok/bc.h
*/

void
bc_set_dirty (struct bc_entry *b)
{
  /* if the buffer isn't already dirty then we really are increasing
     the number of dirty buffers in the system */
  if (!bc_is_dirty (b)) {
    Sysinfo_si_ndpages_atomic_inc(si);
    b->buf_dirty = BUF_DIRTY;
  }
}

void
bc_set_clean (struct bc_entry *b)
{
  /* if the buffer really is dirty then we really are decreasing the
     number of dirty buffers by one */
  if (bc_is_dirty (b)) {
    Sysinfo_si_ndpages_atomic_dec(si);
    b->buf_dirty = BUF_CLEAN;
  }
}

/* let a user mark a buffer as dirty. 
   XXX -- who should we let do this? */

int
sys_bc_set_dirty64 (u_int sn, u32 dev, u32 blk_hi, u32 blk_low, int dirty)
{
  struct bc_entry *b;
  u_quad_t blk = INT2QUAD (blk_hi, blk_low);

  b = bc_lookup64 (dev, blk);
  if (!b)
    return -E_NOT_FOUND; 
  if (dirty) {
    bc_set_dirty (b);
  } else {
    bc_set_clean (b);
  }

  return 0;
}

int
sys_bc_set_dirty (u_int sn, u32 dev, u32 blk, int dirty)
{
  return (sys_bc_set_dirty64 (sn, dev, 0, blk, dirty));
}

/* this is pretty hackly. We want the user to be able to store some 
   user-specific metadata along with each buffer so for now we just 
   let them write a couple words. */
/* XXX -- who should we let do this? */

int
sys_bc_set_user64 (u_int sn, u32 dev, u32 blk_hi, u32 blk_low, u_int user1, 
		   u_int user2)
{
  struct bc_entry *b;
  u_quad_t blk = INT2QUAD (blk_hi, blk_low);

  b = bc_lookup64 (dev, blk);
  if (!b) {
    return -E_NOT_FOUND;
  }
  b->buf_user1 = user1;
  b->buf_user2 = user2;
  return 0;
}

int
sys_bc_set_user (u_int sn, u32 dev, u32 blk, u_int user1, u_int user2)
{
  return (sys_bc_set_user64 (sn, dev, 0, blk, user1, user2));
}

/* XXX -- who should we let do this? */

int
sys_bc_set_state64 (u_int sn, u32 dev, u32 blk_hi, u32 blk_low, u32 state)
{
  struct bc_entry *b;
  u_quad_t blk = INT2QUAD (blk_hi, blk_low);

  switch(state) {
  case BC_EMPTY:  
  case BC_VALID:  
  case BC_COMING_IN:  
  case BC_GOING_OUT:
    /* XXX - are we missing a valid combination? */
    break;
  default:
    return -E_INVAL;
  }
    
  b = bc_lookup64 (dev, blk);
  if (!b)
    return -E_NOT_FOUND;
  
  b->buf_state = state;

  return 0;
}

int
sys_bc_set_state (u_int sn, u32 dev, u32 blk, u32 state)
{
  return (sys_bc_set_state64 (sn, dev, 0, blk, state));
}

/* insert the mapping (dev, blk) -> ppn into the cache, where dev is
   in xn_user and blk comes from b_hi and b_low. The buffer's initial
   state is buf_state and the page's sharing mode is initially
   pp_state_user. The pxn xn_user should allow write access to block
   b.  The caller must have write access to physical page ppn with
   capability k. */

int
sys_bc_insert64 (u_int sn, struct Xn_name *xn_user, u32 b_hi, u32 b_low,
		 u8 buf_state, u_int k, u_int ppn,
		 struct Pp_state *pp_state_user)
{
  cap c;
  struct Ppage *pp;
  struct Xn_xtnt xtnt;
  struct Pxn *pxn;
  u_quad_t b = INT2QUAD (b_hi, b_low);
  struct Pp_state *pp_state_kernel = Pp_state_alloc();
  struct Xn_name xn_kernel;
  struct bc_entry *buf;
  int r;

  /* copy in parameters */
  copyin (xn_user, &xn_kernel, sizeof (xn_kernel));
  copyin (pp_state_user, pp_state_kernel, Pp_state_sizeof());

  /* check the page level permissions */
  if ((r = env_getcap (curenv, k, &c)) < 0)
  {
    Pp_state_free(pp_state_kernel);
    return r;
  }

  if (ppn < 1 || ppn >= nppage)
  {
    Pp_state_free(pp_state_kernel);
    return -E_INVAL;
  }
  
  pp = ppages_get(ppn);
  if (Ppage_pp_status_get(pp) != PP_USER)
  {
    Pp_state_free(pp_state_kernel);
    return -E_INVAL;
  }

  if (!ppcompat_state (Pp_state_ps_readers_get(Ppage_pp_state_ptr(pp)),
                       Pp_state_ps_writers_get(Ppage_pp_state_ptr(pp)),
		       pp_state_kernel))
  {
    Pp_state_free(pp_state_kernel);
    return -E_SHARE;
  }

  if ((r = ppage_acl_check (pp, &c, PP_ACL_LEN, ACL_W)) < 0) 
  {
    Pp_state_free(pp_state_kernel);
    return r;
  }

  /* now check block level permissions */
  if (!(pxn = lookup_pxn (&xn_kernel)))
  {
    Pp_state_free(pp_state_kernel);
    return -E_NOT_FOUND;
  }
  
  /* set the block size automatically for disk devices. force all
     other pseudo-devices to use a block size of NBPG */

  /* XXX is this the right check for pseudo-devices? */
  if (xn_kernel.xa_dev >= MAX_DISKS) {
    xtnt.xtnt_block = b;
    xtnt.xtnt_size = NBPG;
  } else {
    xtnt.xtnt_block = b*NBPG/disk_sector_sz (xn_kernel.xa_dev);
    xtnt.xtnt_size = NBPG/disk_sector_sz (xn_kernel.xa_dev);
  }

  /* treat as a write since they're adding to cache */
  /* XXX - Why is there an XXX here? */
  if (!pxn_authorizes_xtnt (pxn, &c, &xtnt, ACL_W, &r)) 
  {
    Pp_state_free(pp_state_kernel);
    return r;
  }

  /* ok, if we made it this far we can insert the block into the cache */
  if (bc_lookup64 (pxn->pxn_name.xa_dev, b))
  {
    Pp_state_free(pp_state_kernel);
    return -E_EXISTS;
  }

  buf = bc_insert64 (pxn->pxn_name.xa_dev, b, ppn, buf_state, &r);
  if (buf == NULL) 
  {
    Pp_state_free(pp_state_kernel);
    return r;
  }
 
  Ppage_pp_state_copyin(pp, pp_state_kernel);

  Pp_state_free(pp_state_kernel);
  return (0);
}

int
sys_bc_insert (u_int sn, struct Xn_name *xn, u32 b, u8 buf_state, u_int k,
	       u_int ppn, struct Pp_state *pp_state)
{
  return (sys_bc_insert64 (sn, xn, 0, b, buf_state, k, ppn, pp_state));
}

/* Read an extent from disk into the buffer cache, allocating the memory
   pages for the blocks as needed. 

   XXX -- XN uses this to do it's reading. */

int
bc_read_extent64 (u32 dev, u_quad_t block, u32 num, int *resptr, int userreq)
{
  int i, j;
  struct Ppage *pp;
  struct buf *diskbuf = NULL;
  struct bc_entry *bc_entry;
  int error;

  for (i = 0; i < num; i++) {
    bc_entry = ppage_get_reclaimable_buffer (dev, block+i, BC_COMING_IN,
					      &error, userreq);
    if (!bc_entry)
      goto bc_read_extent64_cleanup;

    pp = ppages_get(bc_entry->buf_ppn);

    /* set protection info to no capabilities (no one can map--must use
       bc_buffer_map w/ a pxn to map buffer cache pages */
    ppage_acl_zero(pp);
    Pp_state_ps_writers_set(Ppage_pp_state_ptr(pp),PP_ACCESS_ALL);
    Pp_state_ps_readers_set(Ppage_pp_state_ptr(pp),PP_ACCESS_ALL);

    /* add the current block to the list of blocks being assembled into
       the disk request. */
    if ((error = disk_prepare_bc_request (dev, block + i, pp2va(pp),
					  B_READ | B_BC_REQ,
					  (i == num-1) ? resptr : 0,
					  &diskbuf)) < 0) {
      ppage_reclaim_buffer (pp, PP_FREE);
      ppage_free (pp);
      goto bc_read_extent64_cleanup;
    }

    /* techincally the buffer is not yet clean, but it turns out
       to be convnient to know that cleaning i/o has been started
       even if it hasn't completed yet */
    bc_set_clean (bc_entry);
  }
  
  /* execute the disk requests */
  if (diskbuf != NULL) {
    if ((error = disk_bc_request (diskbuf)) < 0)
      goto bc_read_extent64_cleanup;
  } else if (resptr)
    (*resptr)--;

  return 0;

 bc_read_extent64_cleanup:
  disk_buf_free_chain (diskbuf);

  for (j = 0; j < i; j++) {
    bc_entry = bc_lookup64 (dev, block + j);
    assert (bc_entry);
    assert (bc_entry->buf_ppn);
    pp = ppages_get(bc_entry->buf_ppn);
    ppage_reclaim_buffer (pp, PP_FREE);
    ppage_free (pp);
  }

  return error;
}

int
bc_read_extent (u32 dev, u32 block, u32 num, int *resptr)
{
  return (bc_read_extent64(dev, INT2QUAD(0, block), num, resptr, 0));
}

/* read pagecount blocks and insert the mappings (dev, b+i) -> (newly
   allocated pages) into the cache. The buffer's initial state is
   INCOMING.

   XXX -- not used if XN is being used. */

int
sys_bc_read_and_insert (u_int sn, u32 dev, u32 b_hi, u32 b_low,
			u_int pagecount, int *resptr)
{
  u_quad_t b = INT2QUAD (b_hi, b_low);
  int i, ret;


  if (pagecount < 1 || pagecount > (BC_MAX_DISK_REQUEST_SIZE/NBPG)) {
    return -E_INVAL;
  }

  if (dev >= SYSINFO_GET(si_ndisks)) {
    return -E_INVAL;
  }

  /* XXX -- should also check to make sure b is valid */

  for (i=0; i < pagecount; i++)
    if (bc_lookup64 (dev, b + i)) return -E_EXISTS;

  if (resptr) {
    /* XXX - use PFM instead of isava* */
     if ((((unsigned int) resptr) % sizeof(int)) || 
	 !(isvawriteable (resptr)))
       return -E_FAULT;
     resptr = (int *) pa2kva(va2pa(resptr));
  }

  if ((ret = bc_read_extent64 (dev, b, pagecount, resptr, 1)) < 0) {
    return ret;
  }

  return 0;
}

DEF_ALIAS_FN (sys_self_bc_buffer_map, sys_bc_buffer_map);
int
sys_bc_buffer_map (u_int sn, struct Xn_name *xn_user, u_int kx, Pte pte,
		   u_int va, u_int ke, int envid)
{
  int r;
  int perm;
  u_int ppn;
  struct Ppage *pp;
  struct Pxn *p;
  struct Xn_xtnt xtnt;
  struct Xn_name xn_kernel;
  cap c;
  struct Env *e;

  if (sn == SYS_self_bc_buffer_map)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &r)))
    return r;

  if (!(pte & PG_P)) return -E_INVAL;

  /* get parameters */
  copyin (xn_user, &xn_kernel, sizeof (xn_kernel));
  if ((r = env_getcap (curenv, kx, &c)) < 0) return r;

  /* Make sure that bits that have to be zero are and that
     the caller isn't trying to insert a supervisor pte entry */
  pte &= ~PG_MBZ;
  pte |= PG_U;

  if (pte & PG_W)
    perm = ACL_W;
  else
    perm = 0;

  ppn = PGNO(pte);
  if (ppn >= nppage || ppn < 1) return -E_INVAL;

  /* we only deal with pages holding buffers */
  pp = ppages_get(ppn);
  if (!Ppage_pp_buf_get(pp)) return -E_BUFFER;

  /* check to make sure we have permission to read or write the
     disk block cached in this page */
  p = lookup_pxn (&xn_kernel);
  if (!p) return -E_NOT_FOUND;
  if (xn_kernel.xa_dev != Ppage_pp_buf_get(pp)->buf_dev) return -E_INVAL;

  if (xn_kernel.xa_dev >= MAX_DISKS) {
    xtnt.xtnt_block = Ppage_pp_buf_get(pp)->buf_blk;
    xtnt.xtnt_size = NBPG;
  } else {
    xtnt.xtnt_block = Ppage_pp_buf_get(pp)->buf_blk *
      NBPG/disk_sector_sz (xn_kernel.xa_dev);
    xtnt.xtnt_size = NBPG/disk_sector_sz (xn_kernel.xa_dev);
  }

  if (!pxn_authorizes_xtnt (p, &c, &xtnt, perm, &r)) return r;

  /* if the page isn't allocated, pull it off the free list */
  if (Ppage_pp_status_get(pp) == PP_FREE) {
    ppage_reuse_buffer (pp);
  }

  /* and finally map the page into the environment */
  if ((r = ppage_insert (e, pp, va, pte & PGMASK)) < 0) {
    if (Ppage_pp_refcnt_get(pp) == 0) ppage_free (pp);
    return r;
  }
  
  return 0;
}

/* Actually enqueue a list of requests to the disk. This is called by
 * sys_bc_write_dirty_bufs and by other parts of the kernel that want
 * to write disk blocks. 
 *
 * All the blocks in the extent must be in the buffer cache and must
 * not already be the destination of a pending disk read. 

 XXX -- XN uses this to do it's writing. */

int
bc_write_extent64 (u32 dev, u_quad_t block, u32 num, int *resptr)
{
  int i, j, error;
  struct buf *diskbuf = NULL;
  struct bc_entry *b;
  u32 *buf_state_save = NULL;
  u8 *buf_dirty_save = NULL;

  if (!(buf_state_save = (u32*) malloc (sizeof(u32) * num)) ||
      !(buf_dirty_save = (u8*) malloc (sizeof(u8) * num))) {
    error = -E_NO_MEM;
    goto bc_write_extent64_return;
  }

  for (i = 0; i < num; i++) {
    if (!(b = bc_lookup (dev, block+i)) ||
	!(b->buf_state & BC_VALID) ||
	b->buf_state & BC_COMING_IN) {
      error = -E_INVAL;
      goto bc_write_extent64_cleanup;
    }

    b->buf_writecnt++;
    buf_state_save[i] = b->buf_state;
    b->buf_state |= BC_GOING_OUT;
    buf_dirty_save[i] = b->buf_dirty;

    if ((error = disk_prepare_bc_request (dev, block + i,
					  pp2va (ppages_get(b->buf_ppn)),
					  B_WRITE | B_BC_REQ,
					  (i == num-1) ? resptr : 0,
					  &diskbuf)) < 0) {
      b->buf_writecnt--;
      b->buf_state = buf_state_save[i];
      goto bc_write_extent64_cleanup;
    }

    /* techincally the buffer is not yet clean, but it turns out to
       be convnient to know that cleaning i/o has been started even
       if it hasn't completed yet. We still won't reuse the page
       since pp_pinned will be non-zero. */
    bc_set_clean (b);
  }

  if (diskbuf != NULL) {
    if ((error = disk_bc_request (diskbuf)) < 0)
      goto bc_write_extent64_cleanup;
  } else if (resptr)
    (*resptr)--;

  error = 0;
  goto bc_write_extent64_return;

 bc_write_extent64_cleanup:
  disk_buf_free_chain (diskbuf);

  for (j = 0; j < i; j++) {
    b = bc_lookup64 (dev, block + j);
    assert (b);
    assert (b->buf_ppn);
    b->buf_writecnt--;
    b->buf_state = buf_state_save[j];
    if (buf_dirty_save[j] != b->buf_dirty) {
      if (buf_dirty_save[j] == BUF_DIRTY)
	bc_set_dirty(b);
      else
	bc_set_clean(b);
    }
  }

 bc_write_extent64_return:
  if (buf_state_save) free (buf_state_save);
  if (buf_dirty_save) free (buf_dirty_save);
  return error;
}

int
bc_write_extent (u32 dev, u32 block, u32 num, int *resptr)
{
  return (bc_write_extent64(dev, INT2QUAD(0, block), num, resptr));
}

/* System call interface to write all dirty buffers containing blocks
 * in the range [block, block+num-1].  Anyone can currently start a
 * write on a block that is in the buffer cache so we don't do any
 * permission checking here. All we really do is verify the resptr the
 * user passes in.
 */

/* XXX - so should we do checks, or is it okay not to? */

int
sys_bc_write_dirty_bufs64 (u_int sn, u32 dev, u32 b_hi, u32 b_low, u32 num,
			   int *resptr)
{
  u_quad_t block = INT2QUAD(b_hi, b_low);

  if (resptr) {
    /* XXX - use PFM instead of isava* */
    if ((((unsigned int) resptr) % sizeof(int)) || 
	!(isvawriteable (resptr)))
      return -E_FAULT;
    resptr = (int *) pa2kva(va2pa(resptr));
  }

  return (bc_write_extent64 (dev, block, num, resptr));
}

int
sys_bc_write_dirty_bufs (u_int sn, u32 dev, u32 block, u32 num, int *resptr)
{
  return (sys_bc_write_dirty_bufs64(sn, dev, 0, block, num, resptr));
}

/* flush a buffer named by bc_entry * */

void
bc_flush_by_buf (struct bc_entry *buf)
{
  struct Ppage *pp;

  /* make sure the buffer exists and that no one is using it */
  if (!buf)
    return;

  pp = ppages_get(buf->buf_ppn);
  assert (Ppage_pp_status_get(pp) == PP_FREE || 
          Ppage_pp_status_get(pp) == PP_USER);

  /* don't flush a mapped buffer */
  /* don't flush a buffer for a pinned page*/
  /* don't flush a dirty buffer */
  if (Ppage_pp_status_get(pp) == PP_USER ||
      Ppage_pp_pinned_get(pp) || buf->buf_dirty)
    return;

  /* it's in the cache and no one is mapping it, so it must be
     on free_bufs. remove it from free_bufs and the cache and then
     insert it on free_list */
  ppage_free_bufs_2_free_list (pp);
}

/* flush a buffer named by (device,block) */

static inline void
flush_by_dev (u32 d, u_quad_t b)
{
  return (bc_flush_by_buf (bc_lookup64 (d, b)));
}

/* 
 * Flush a buffer named by (d, b) if no one's mapping it, it's not
 * dirty, and it's not pinned.
 */

void
sys_bc_flush64 (u_int sn, u32 d, u32 b_hi, u32 b_low, u32 num)
{
  int i;
  struct bc_entry *buf, *next;
  u_quad_t b = INT2QUAD (b_hi, b_low);

  if (d != BC_SYNC_ANY && b != BC_SYNC_ANY) {
    /* flush a particular range of blocks on a particular device */
    if (num > BC_NUM_QS) {
      for (i = 0; i < BC_NUM_QS; i++)
	for (buf = KLIST_KPTR(&bc->bc_qs[i].lh_first); buf ; buf = next) {
	  next = KLIST_KPTR(&buf->buf_link.le_next);
	  if (buf->buf_blk64 >= b && buf->buf_blk64 < (b+num) &&
	      buf->buf_dev == d)
	    bc_flush_by_buf (buf);
	}
    } else 
      for (i = 0; num; i++, num--) {
	flush_by_dev (d, b+i);
      }
  } else {
    if (d == BC_SYNC_ANY) {
      for (i = 0; i < BC_NUM_QS; i++) {
	for (buf = KLIST_KPTR(&bc->bc_qs[i].lh_first); buf;
	     buf = next) {
	  next = KLIST_KPTR(&buf->buf_link.le_next);
	  bc_flush_by_buf (buf);
	}
      }
    } else if (b == BC_SYNC_ANY) {
      for (i = 0; i < BC_NUM_QS; i++) {
	for (buf = KLIST_KPTR(&bc->bc_qs[i].lh_first); buf;
	     buf = next) {
	  next = KLIST_KPTR(&buf->buf_link.le_next);
	  if (buf->buf_dev == d)
	    bc_flush_by_buf (buf);
	}
      }
    }
  }
}

void
sys_bc_flush (u_int sn, u32 d, u32 b, u32 num)
{
  return (sys_bc_flush64 (sn, d, 0, b, num));
}

/* a way for the caller to get the bc_entry for a physical page */

int
sys_bc_ppn2buf64 (u_int sn, u32 ppn, u32 *d, u32 *b_hi, u32 *b_low)
{
  struct Ppage *pp;
  u_int blk_hi, blk_low;

  if (ppn < 1 || ppn >= nppage)
    return -E_INVAL;
  pp = ppages_get(ppn);
  if (Ppage_pp_status_get(pp) != PP_USER)
    return -E_INVAL;

  /* if no buf, then return error */
  if (Ppage_pp_buf_get(pp) == NULL) return -E_NOT_FOUND;

  /* else copy the dev and blk number and return 0 */
  copyout(&(Ppage_pp_buf_get(pp))->buf_dev, d, sizeof(*d));
  blk_hi = QUAD2INT_HIGH((Ppage_pp_buf_get(pp))->buf_blk64);
  blk_low = QUAD2INT_LOW((Ppage_pp_buf_get(pp))->buf_blk64);
  if (b_hi)			/* compatibility with 32 bit version */
    copyout (&blk_hi, b_hi, sizeof (*b_hi));
  copyout (&blk_low, b_low, sizeof (*b_low));

  return 0;
}

int
sys_bc_ppn2buf (u_int sn, u32 ppn, u32 *d, u32 *b)
{
  return (sys_bc_ppn2buf64 (sn, ppn, d, NULL, b));
}

/* called when I/O initiated by bc_[read,write]_extent completes. */

void
bc_diskreq_done64 (u32 devno, u_quad_t blkno, int write, char *addr,
		   u_int envid)
{
  struct Ppage *pp = kva2pp((u_long)addr);
  struct bc_entry *bc_entry = Ppage_pp_buf_get(pp);
#ifdef XN
  extern void xn_io_done (u32 dev, u32 blk, u32 len, int write);
#endif

  /* XXX - is it legal for the pinned page to not have a buf? */
  if (bc_entry == NULL) {
    return;
  }

  if ((bc_entry->buf_state & (BC_COMING_IN|BC_GOING_OUT)) == 0) {
    warn ("Invalid state for completing bc_diskreq (%d, %d)\n",
	  bc_entry->buf_state, bc_entry->buf_writecnt);
    warn (" devno %u, blkno %qu, write %d\n", devno, blkno, write);
    warn (" actual buf_dev %u and buf_blk %qu\n", bc_entry->buf_dev,
	  bc_entry->buf_blk64);
    return;
  }

#ifdef XN
  /* call back into the XN module to let it know that i/o is done
     on this block */
  xn_io_done (bc_entry->buf_dev, bc_entry->buf_blk, 1, write);
#endif

  if (!write) {
    if (! (bc_entry->buf_state & BC_COMING_IN)) {
      warn ("bc_diskreq_done: BC_COMING_IN not set in buf_state");
    } else {
      bc_entry->buf_state &= ~BC_COMING_IN;

      /* schedule toward processes finishing i/o */
      /* XXX this is clearly broken. need to figure out a better way
	 of doing this. */
      if (envid && should_reschedule == 0)  /* best effort reschedule */
	should_reschedule = envid;
    }
  } else {
    if (! (bc_entry->buf_state & BC_GOING_OUT) && 
	(bc_entry->buf_writecnt <= 0)) {
      warn ("bc_diskreq_done: BC_GOING_OUT not set or no recorded requests");
    } else {
      bc_entry->buf_writecnt--;
      if (bc_entry->buf_writecnt == 0) {
	bc_entry->buf_state &= ~BC_GOING_OUT;
      }
    }
  }
}

void
bc_diskreq_done (u32 devno, u32 blkno, int write, char *addr, u_int envid)
{
  return (bc_diskreq_done64(devno, INT2QUAD(0, blkno), write, addr, envid));
}


#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

