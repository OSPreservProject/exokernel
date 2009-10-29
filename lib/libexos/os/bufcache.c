
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

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <xok/sys_ucall.h>
#include <ubb/xn.h>		/* for XN syscall wrappers */

#include <exos/ubc.h>
#include <exos/bufcache.h>
#include <exos/uwk.h>
#include <exos/pager.h>

#include <exos/vm-layout.h>	/* for BUFCACHE_REGION */
#include <exos/vm.h>		/* for PG_SHARED */

#include <xok/sysinfo.h>	/* for __sysinfo structure */
#include <exos/cap.h>		/* for CAP_ROOT */
#include <xok/pmap.h>		/* for pp_state + */
#include <xok/kerrno.h>

static struct Pp_state pp_state;

#define BUFCACHE_PGNO(vaddr) (PGNO((vaddr) - BUFCACHE_REGION_START))
#define BUFCACHE_ADDR(pgno) (BUFCACHE_REGION_START + vp2va((pgno)))


/* XXX - doesn't work when physical memory is > 256meg */

/* Map bc page that holds <dev, blk> (if it is resident), and return ptr. */
/* Return NULL if not resident (or not resident where expected).          */

void * exos_bufcache_map (struct bc_entry *bc_entry, u32 dev, u32 blk,
			  u_int writeable)
{
   int ret;
   u_int vaddr;

   if (bc_entry == NULL) {
      bc_entry = __bc_lookup (dev, blk);
      if (bc_entry == NULL) {
         return (NULL);
      }
   }

   vaddr = BUFCACHE_ADDR (bc_entry->buf_ppn);

   if (writeable) {
      writeable = PG_W;
   }

   ret = _exos_self_insert_pte (CAP_ROOT, ppnf2pte(bc_entry->buf_ppn,
						   PG_P | PG_U | writeable |
						   PG_SHARED),
				(u_int)vaddr, ESIP_DONTPAGE, 
				&__sysinfo.si_pxn[bc_entry->buf_dev]);

   if ((bc_entry->buf_ppn != BUFCACHE_PGNO(vaddr)) ||
       (bc_entry->buf_state == BC_EMPTY) || (bc_entry->buf_blk != blk) ||
       (bc_entry->buf_dev != dev) ||
       (bc_entry->buf_ppn != (va2ppn(vaddr)))) {
      kprintf ("buf_state %d, buf_blk %d, diskBlock %d, buf_dev %d, dev %d\n",
	       bc_entry->buf_state, bc_entry->buf_blk, blk, bc_entry->buf_dev,
	       dev);
      kprintf ("buf_ppn %d, expected %d\n", bc_entry->buf_ppn,
	       va2ppn(vaddr));
      kprintf ("gotcha: lost race detected (and handled) in "
	       "exos_bufcache_map\n"); 
      exos_bufcache_unmap (dev, blk, (void *)vaddr);
      vaddr = 0;
   } else if (ret != 0) {
      kprintf ("exos_bufcache_map: _exos_self_insert_pte failed (ret %d, "
	       "vaddr %x, ppn %d)\n", ret, vaddr, bc_entry->buf_ppn);
      assert (ret == 0);
   }

   return ((void *) vaddr);
}

void * exos_bufcache_map64 (struct bc_entry *bc_entry, u32 dev, u_quad_t blk64,
			    u_int writeable)
{
   int ret;
   u_int vaddr;
   struct Xn_name *xn;
   struct Xn_name xn_nfs;

   if (bc_entry == NULL) {
      bc_entry = __bc_lookup64 (dev, blk64);
      if (bc_entry == NULL) {
         return (NULL);
      }
   }

   vaddr = BUFCACHE_ADDR (bc_entry->buf_ppn);

   if (writeable) {
      writeable = PG_W;
   }

   if (bc_entry->buf_dev > MAX_DISKS) {
     xn_nfs.xa_dev = bc_entry->buf_dev;
     xn_nfs.xa_name = 0;
     xn = &xn_nfs;
   } else {
     xn = &__sysinfo.si_pxn[bc_entry->buf_dev];
   }

  ret = _exos_self_insert_pte (CAP_ROOT, ppnf2pte(bc_entry->buf_ppn,
						   PG_P | PG_U | writeable |
						   PG_SHARED),
				(u_int)vaddr, ESIP_DONTPAGE, xn);

   if ((bc_entry->buf_ppn != BUFCACHE_PGNO(vaddr)) ||
       (bc_entry->buf_state == BC_EMPTY) || (bc_entry->buf_blk64 != blk64) ||
       (bc_entry->buf_dev != dev) ||
       (bc_entry->buf_ppn != (va2ppn(vaddr)))) {
      kprintf ("buf_state %d, buf_blk %x:%x, diskBlock %x:%x, buf_dev %d, "
	       "dev %d\n", bc_entry->buf_state,
	       QUAD2INT_HIGH(bc_entry->buf_blk64),
	       QUAD2INT_LOW(bc_entry->buf_blk64), QUAD2INT_HIGH(blk64),
	       QUAD2INT_LOW(blk64), bc_entry->buf_dev, dev);
      kprintf ("buf_ppn %d, expected %d\n", bc_entry->buf_ppn,
	       va2ppn(vaddr));
      kprintf ("gotcha: lost race detected (and handled) in "
	       "exos_bufcache_map\n");
      exos_bufcache_unmap64 (dev, blk64, (void *)vaddr);
      vaddr = 0;
   } else if (ret != 0) {
      kprintf ("exos_bufcache_map: _exos_self_insert_pte failed (ret %d, "
	       "vaddr %x, ppn %d)\n", ret, vaddr, bc_entry->buf_ppn);
      assert (ret == 0);
   }

   return ((void *) vaddr);
}


/* Unmap bc page that holds <dev, blk> and is mapped at ptr. */

int exos_bufcache_unmap (u32 dev, u32 blk, void *ptr)
{
   unsigned int vaddr = (unsigned int) ptr;
   struct bc_entry *bc_entry;
   int ret;

   //assert (size == NBPG);
   /* GROK -- this check should go away after awhile... */
   if (! (((bc_entry = __bc_lookup(dev, blk)) != NULL) &&
	  (bc_entry->buf_ppn == BUFCACHE_PGNO(vaddr))) ) {
      kprintf ("exos_bufcache_unmap: not actually mapped (dev %d, blk %d, "
	       "ptr %p, bc_entry %p)\n", dev, blk, ptr, bc_entry);
      return (-1);
      //assert (0);
   }

   if ((vaddr < BUFCACHE_REGION_START) || (vaddr >= BUFCACHE_REGION_END)) {
      kprintf ("exos_bufcache_unmap: ptr (%p) out of range\n", ptr);
      assert (0);
   }

   if (ret = _exos_self_unmap_page (CAP_ROOT, vaddr) < 0) {
      kprintf ("exos_bufcache_unmap: _exos_self_insert_pte failed (ret %d)\n",
	       ret);
      assert (0);
   }
   return (0);
}

int exos_bufcache_unmap64 (u32 dev, u_quad_t blk64, void *ptr)
{
   unsigned int vaddr = (unsigned int) ptr;
   struct bc_entry *bc_entry;
   int ret;

   //assert (size == NBPG);
   /* GROK -- this check should go away after awhile... */
   if (! (((bc_entry = __bc_lookup64(dev, blk64)) != NULL) &&
	  (bc_entry->buf_ppn == BUFCACHE_PGNO(vaddr))) ) {
      kprintf ("exos_bufcache_unmap: not actually mapped (dev %d, blk %x:%x, "
	       "ptr %p, bc_entry %p)\n", dev, QUAD2INT_HIGH(blk64),
	       QUAD2INT_LOW(blk64), ptr, bc_entry);
      return (-1);
      //assert (0);
   }

   if ((vaddr < BUFCACHE_REGION_START) || (vaddr >= BUFCACHE_REGION_END)) {
      kprintf ("exos_bufcache_unmap: ptr (%p) out of range\n", ptr);
      assert (0);
   }

   if (ret = _exos_self_unmap_page (CAP_ROOT, vaddr) < 0) {
      kprintf ("exos_bufcache_unmap: _exos_self_insert_pte failed (ret %d)\n",
	       ret);
      assert (0);
   }
   return (0);
}


/* Allocate and map bufcache page, if no conflict.  Return virtual address */
/* of allocated page (or NULL if none).                                    */

void * exos_bufcache_alloc (u32 dev, u32 blk, int zerofill, int writeable,
			    int usexn)
{
   int ret;
   unsigned int vaddr = BUFCACHE_ADDR (__sysinfo.si_nppages);

   if (writeable) {
      writeable = PG_W;
   }

   /* This first call to insert_pte causes a physical page to be allocated. */
   /* Start with page mapped writeable, since might be zerofill.            */

   if (((ret = _exos_self_insert_pte (CAP_ROOT, PG_W | PG_P | PG_U | PG_SHARED,
				      vaddr, ESIP_DONTPAGE, NULL)) < 0) ||
       (vpt[PGNO(vaddr)] == 0)) {
      kprintf ("exos_bufcache_alloc: _exos_self_insert_pte failed (ret %d)\n",
	       ret);
      return (NULL);
   }

   if (zerofill) {
      bzero ((char *)vaddr, NBPG);
   }

   /* do final-location mapping based on "writeable" variable */

   if (((ret = _exos_self_insert_pte (CAP_ROOT,
				      ppnf2pte(va2ppn(vaddr),
					       writeable | PG_P | PG_U |
					       PG_SHARED),
				      BUFCACHE_ADDR (va2ppn(vaddr)),
				      ESIP_DONTPAGE, &__sysinfo.si_pxn[dev])) < 0) ||
       (vpt[PGNO(vaddr)] == 0)) {
      kprintf ("exos_bufcache_alloc: failed to add real mapping (ret %d)\n",
	       ret);
      assert (0);
   }
   vaddr = BUFCACHE_ADDR (va2ppn(vaddr));

   /* Unmap the allocation mapping before inserting into bc, to make sure */
   /* that we never have a non-writeable bc entry mapped writable...      */

   if ((ret = _exos_self_unmap_page (CAP_ROOT,
				     BUFCACHE_ADDR(__sysinfo.si_nppages)))
       < 0) {
      kprintf ("exos_bufcache_alloc: failed to clobber fake mapping "
	       "(ret %d)\n", ret);
      assert (0);
   }

#if 1
   vaddr = (u_int) exos_bufcache_insert (dev, blk, (void *)vaddr, usexn);
   assert (vaddr == BUFCACHE_ADDR (va2ppn(vaddr)));
#else
   vaddr = BUFCACHE_ADDR (va2ppn(vaddr));
#endif

   return ((void *) vaddr);
}


/* Initiate a fill request (i.e., disk fetch into bc) */

int exos_bufcache_initfill (u32 dev, u32 blk, int blockcount, int *resptr)
{
   int ret;

   if (resptr) {
      *resptr = 0;
   }
/*
   ret = sys_xn_readin (firstBlock, blockcount, (xn_cnt_t*)&entry->resid);
*/
   ret = _exos_bc_read_and_insert (dev, blk, blockcount, resptr);
	/* GROK -- this check and assert need to go away.  This is a     */
	/* perfectly fine place to get bad return codes (e.g., E_EXISTS) */
   if (ret != 0) {
      kprintf ("warning: _exos_bc_read_and_insert failed: ret %d, "
	       "firstBlock %d, blockcount %d, dev %d\n", ret, blk, blockcount,
	       dev);
   }
   //assert (ret == 0);

   return (ret);
}


/* Insert the page at ptr into the bc as <dev, blk>, returning the new */
/* address (but not unmapping ptr) if successful and NULL otherwise.   */

void * exos_bufcache_insert (u32 dev, u32 blk, void *ptr, int usexn)
{
   int ret;
   u_int vaddr;
   struct bc_entry *bc_entry;

   vaddr = BUFCACHE_ADDR (va2ppn(ptr));

/* GROK -- this does not current install mappings for new vaddr, so it */
/* must be assuming that vaddr and ptr are same!!                      */

   assert (vaddr == (uint) ptr);

   if (usexn == 0) {
/*
kprintf ("vaddr %p, vpn %d, pte %x, ppn %d\n", entry->buffer,
 ((u_int)entry->buffer >> PGSHIFT), (u_int)vpt[(u_int)entry->buffer >> PGSHIFT],
 vpt[(u_int)entry->buffer >> PGSHIFT] >> PGSHIFT);
*/
      pp_state.ps_readers = pp_state.ps_writers = PP_ACCESS_ALL;
      if ((ret = sys_bc_insert (&__sysinfo.si_pxn[dev], blk, 0,
				CAP_ROOT, va2ppn(ptr), &pp_state)) != 0) {
         if (ret == -E_EXISTS) {
            kprintf ("gotcha: lost race detected (and handled) in "
		     "exos_bufcache_insert\n");
            return (NULL);
         }
         kprintf ("sys_bc_insert failed: ret %d (ppn %d, diskBlock %d, "
		  "buffer %p)\n", ret, va2ppn(ptr), blk, ptr);
	 kprintf ("dev %d, blk %d\n", dev, blk);
         assert (0);
      }

   } else {   /* don't use XN */
#ifdef XN     
//kprintf ("xn_bind: diskBlock %d, inodeNum %d, block %d, zerofill %d\n", diskBlock, entry->header.inodeNum, entry->header.block, zerofill);
      //if ((ret = sys_xn_bind (blk, virt_to_ppn(ptr), (cap_t)CAP_ROOT, ((zerofill) ? XN_ZERO_FILL : XN_BIND_CONTENTS), 0)) < 0) {
      if ((ret = sys_xn_bind (blk, va2ppn(ptr), (cap_t)CAP_ROOT,
			      XN_ZERO_FILL, 0)) < 0) {
         kprintf ("sys_xn_bind failed: ret %d (ppn: %d, blk %d, buffer %p)\n",
		  ret, va2ppn(ptr), blk, ptr);
	 kprintf ("dev %d, blk %d\n", dev, blk);
         assert (0);
      }
#else
      assert (0);
#endif
     
   }

   if ((bc_entry = __bc_lookup (dev, blk)) == NULL) {
      kprintf ("lookup failed after insert\n");
      assert (0);
   }

   assert (bc_entry->buf_ppn == BUFCACHE_PGNO(vaddr));

   return ((void *) vaddr);
}


int exos_bufcache_initwrite (u32 dev, u32 blk, int blockcount, int *resptr)
{
   int ret;

   if (resptr) {
      *resptr = 0;
   }

#ifdef XN
//kprintf ("sys_xn_writeback: b %d, cnt %d\n", b, cnt);
   ret = sys_xn_writeback (blk, blockcount, resptr);
//kprintf ("done: ret %d\n", ret);
#else
   ret = sys_bc_write_dirty_bufs (dev, blk, blockcount, resptr);
#endif
   return (ret);
}


void exos_bufcache_waitforio (struct bc_entry *bc_entry)
{
   assert (bc_entry != NULL);

   if (exos_bufcache_isiopending (bc_entry)) {
      struct wk_term t[(2*UWK_MKCMP_EQ_PRED_SIZE)+1];
      int sz = 0;
      sz += wk_mkcmp_eq_pred (&t[sz], &bc_entry->buf_state, BC_VALID, CAP_ROOT);
      sz = wk_mkop (sz, t, WK_OR);
      sz += wk_mkcmp_eq_pred (&t[sz], &bc_entry->buf_state, BC_EMPTY, CAP_ROOT);
      wk_waitfor_pred (t, sz);
   }
}

