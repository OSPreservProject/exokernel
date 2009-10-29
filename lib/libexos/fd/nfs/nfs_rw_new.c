
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

#if 0
#undef PRINTF_LEVEL
#define PRINTF_LEVEL 99
#endif
#include "nfs_rpc.h"
#include "nfs_struct.h"
#include "nfs_proc.h"

/* for nfs_proc_read_ra */
#include "nfs_xdr.h"

#include <exos/critical.h>


#include <exos/vm-layout.h>		/* for PAGESIZ */
#include <exos/vm.h>			/* for PG_SHARED */
#include <xok/sys_ucall.h>

#include <xok/pmap.h>
#include <xok/pxn.h>
#include <xok/bc.h>
#include <xok/sysinfo.h>

#include <exos/ubc.h>
#include <exos/uwk.h>
#include <exos/bufcache.h>

extern char *__progname;

#include "errno.h"
#include <exos/debug.h>

#include <stdlib.h>		/* atexit */

#include <unistd.h>		/* for usleep */

#include <exos/ipcdemux.h>
#include <exos/ipc.h>
#define fprintf if (1) usleep(50); if (1) fprintf

#undef PR
#if 0
#define PR fprintf(stderr,"@ %s:%d\n",__FILE__,__LINE__)
#else
#define PR
#endif

#define NFS_FLUSH_THRESHOLD 8*4096

/* Assumptions for Writes:
   pages in the region:    BUFCACHE_REGION_STAR : (__sysinfo.si_nppages * NBPG);
   are mapped if they are dirty.  
   dirty if defined as the region that needs to be flushed out to server.
   defined by e->writestart and e->writeend, where e is a nfsc.

   nfsc_flush_region2 flushes at most 2 pages.
   if also unmaps dirty pages.


 */

/* changes in region */
#if 0
#define d2printf(format,args...) fprintf(stderr,format, ## args)
#else
#define d2printf(format,args...) 
#endif

/* FLUSHINGs */
#if 0
#define d3printf(format,args...) fprintf(stderr,format, ## args)
#else
#define d3printf(format,args...) 
#endif

/* nfsc_sync */
#if 0
#define d4printf(format,args...) fprintf(stderr,format, ## args)
#else
#define d4printf(format,args...) 
#endif

/* FLUSHING 2s */
#if 0
#define d5printf(format,args...) fprintf(stderr,format, ## args)
#else
#define d5printf(format,args...) 
#endif

#if 0  /* AE_RECV procedures */
static void pr_ae_recv(struct ae_recv *send_recv) {
  int i,j = 0;
  printf("NFS RW NEW\n");
  printf("ae_recv: %08x, ae_recv->n: %d\n",(int)send_recv,send_recv->n);
  for (i=0; i < send_recv->n; i++) {
    printf("[%02d] data: %08x, sz: %2d\t",
	   i, (int)send_recv->r[i].data, send_recv->r[i].sz);
    j += send_recv->r[i].sz;
    demand (j <= (8192+300), too much data to send);
  }
  printf("total size: %d\n",j);
}
#else
#define pr_ae_recv(x)
#endif

extern char *__progname;

#if 0
static void dump_text(char *buffer, int size) {
  fprintf(stderr,"**** DUMP_TEXT %p:%d  ppn %d\n",buffer,size, va2ppn(buffer));
  return ;
  while(size-- > 0) {
    fprintf(stderr,"%c",*buffer);
    buffer++;
  }
  fprintf(stderr,"\n****\n");
}
#define dump_text(b,s)
#endif

#if 0
static void show_bc_mapped(char *label) {
  u_int vaddr;
  fprintf(stderr,"SHOW_BC_MAPPED: %s\n",label);
  for ( vaddr = BUFCACHE_REGION_START;
	vaddr < BUFCACHE_REGION_START +  (__sysinfo.si_nppages * NBPG);
	vaddr += NBPG) {
    if (isvamapped(vaddr)) {
      fprintf(stderr,"vaddr: %p  ppn %d\n",
	      (void *)vaddr,va2ppn(vaddr));
    }
  }
}
#else
#define show_bc_mapped(l)
#endif

struct bc_entry *
nfs_new_bc_insert (char *addr, int dev, u_quad_t block) {
  u_int k;
  int ret;
  u_int ppn;
  Pte pte;
  struct Pp_state pp_state;
  struct bc_entry *bc_entry;
  struct Xn_name xn;

  START(nfsc,insert);
  k = 0;
  pp_state.ps_readers = pp_state.ps_writers = PP_ACCESS_ALL;

  if ((vpd[PDENO (addr)] & (PG_P|PG_U)) != (PG_P|PG_U)) {
    assert(0);
  }

  pte = vpt[(unsigned int)addr >> PGSHIFT];

  if ((pte & (PG_P|PG_U)) != (PG_P|PG_U)) {
    assert(0);
  }

  ppn = PGNO(pte);

  FILLXNAME(dev,&xn);
  
  if ((ret = sys_bc_insert64 (&xn, QUAD2INT_HIGH(block), 
			      QUAD2INT_LOW(block), 0, k, ppn, &pp_state)) 
      != 0) {
    //    fprintf (stderr,"sys_bc_insert64 failed: ret %d (ppn %d, diskBlock %qd, buffer %p)\n"
    //	    ,ret, ppn, block, addr);
    if (ret != -E_EXISTS) {
      STOP(nfsc,insert);
      assert(0);
    }
    STOP(nfsc,insert);
    return NULL;
  }
  if ((bc_entry = __bc_lookup64 ((u32)dev, block)) == NULL) {
    fprintf (stderr,"nfs_new_bc_insert: lookup failed after insert (dev %d, block %qd)\n", dev, block);
    STOP(nfsc,insert);
    assert (0);
  }
  if (!((bc_entry->buf_ppn == (((u_int)addr - BUFCACHE_REGION_START)/NBPG)))) {
    fprintf(stderr,"((bc_entry->buf_ppn NOT EQ (((u_int)addr - BUFCACHE_REGION_START)/NBPG)))\n");
  }
  STOP(nfsc,insert);
  return bc_entry;
}

/* Returns an address, where the physical page of the a bc_entry is mapped. */
/* you can use DEALLOCATE_MEMORY_BLOCK to unmap the address  */
inline char *
new_getbcmapping(struct bc_entry *bc_entry) {
   int ret;
   Pte pte;
   u_int vaddr;
   u_int ppn;
   struct Xn_name xn;

   assert(bc_entry);
   vaddr = BUFCACHE_REGION_START + (bc_entry->buf_ppn * NBPG);

   if ((vpd[PDENO (vaddr)] & (PG_P|PG_U)) != (PG_P|PG_U)) goto mapit;

   pte = vpt[vaddr >> PGSHIFT];

   if ((pte & (PG_P|PG_U)) != (PG_P|PG_U)) goto mapit;
   
   ppn = PGNO(pte);
   if (ppn != bc_entry->buf_ppn) {
#if 1
     printf("BUFFER CACHE VIOLATION: v: %p has ppn: %d, bc_entry: pp: %d (%d,%08x)\n",
	    (void *)vaddr,ppn,bc_entry->buf_ppn,bc_entry->buf_dev, bc_entry->buf_blk);
     kprintf("BUFFER CACHE VIOLATION: v: %p has ppn: %d, bc_entry: pp: %d (%d,%08x)\n",
	    (void *)vaddr,ppn,bc_entry->buf_ppn,bc_entry->buf_dev, bc_entry->buf_blk);
#endif
     assert(0);
   }

   goto done;
   
mapit:

   FILLXNAME(bc_entry->buf_dev , &xn);

   if ((ret = _exos_self_insert_pte 
	(0, ((bc_entry->buf_ppn << PGSHIFT) | PG_P | PG_U | PG_W | PG_SHARED),
	 (u_int)vaddr, ESIP_DONTPAGE, &xn)) < 0) {
#if 1
     printf ("getbcmapping: _exos_self_insert_pte failed (ret %d, vaddr %x, ppn %d)\n", ret, vaddr, bc_entry->buf_ppn);
     kprintf ("getbcmapping: _exos_self_insert_pte failed (ret %d, vaddr %x, ppn %d)\n", ret, vaddr, bc_entry->buf_ppn);
#endif
     assert (0);
   }
done:
#if 0
   fprintf(stderr,"Mapped %p <%d,%d> @ %d  ppn: %d  %s\n",(char *)vaddr,
	   bc_entry->buf_dev,
	   NFSBCBLOCK2I(bc_entry->buf_blk),
	   NFSBCBLOCK2O(bc_entry->buf_blk),
	   bc_entry->buf_ppn,
	   __progname);
#endif
   return (char *)vaddr;
}




/* nfs_get_page:
   with flags NFS_CHECK_PAGE returns:
   NULL if the page is not in bc
   vaddr of the page in the bc and guarantees the buf entry is VALID
   with no flags:
   NULL if there is an error reading
   vaddr of the page in the bc and guarantees the buf entry is VALID
   
   */
char *
nfs_get_page(nfsc_p e, short pageno, short flags) {
  int count, dev, ino;
  u_quad_t block;
  struct bc_entry *bc_entry;
  struct nfs_fattr fattr;
  char *vaddr;
  int status;
  assert(e);

  dev = GETNFSCEDEV(e);
  ino = GETNFSCEINO(e);
  
  block = NFSBCBLOCK(ino,pageno);

  bc_entry = __bc_lookup64 ((u32)dev, block);
  
  if (bc_entry == NULL) {
    if (flags & NFS_CHECK_PAGE) return NULL;
    vaddr = ALLOCATE_BC_PAGE();

    if ((status = nfs_get_nfscd_envid()) && status != __envid) {
      status = sipcout(status,IPC_NFS_READ,(u_int)e,pageno * NFSPGSZ,NFSPGSZ);
    }

    /* fetch the page */
    count = nfs_proc_read(nfsc_get_fhandle(e),
			  pageno * NFSPGSZ,
			  NFSPGSZ,
			  vaddr,
			  &fattr);

    if (nfsc_neq_mtime(e,&fattr) || nfsc_ts_iszero(e)) {
      nfs_flush_nfsce(e);
    }
    nfsc_settimestamp(e);
    nfs_fill_stat(&fattr,e);

    if (count == -1) {
      fprintf(stderr,"nfs_get_page warning nfs_proc_read erred: %d\n",errno);
      return NULL; /* errno will be set by nfs_proc_read */
    }
    
    /* HBXX - something should be done with the fattr */

  redo_bc_insert:
    bc_entry = nfs_new_bc_insert(vaddr,dev,block);
    if (bc_entry == NULL) {
      char *newvaddr;
      bc_entry = __bc_lookup64 ((u32)dev, block);
      if (bc_entry == NULL) goto redo_bc_insert;
      newvaddr = exos_bufcache_map64(bc_entry, dev, block, 1);
      if (newvaddr == NULL) goto redo_bc_insert;
      DEALLOCATE_MEMORY_BLOCK(vaddr, NBPG);
      vaddr = newvaddr;
    }
    assert(bc_entry->buf_state == BC_VALID);
  } else {
    /* this will make sure that the entry is locked in */
    vaddr = new_getbcmapping(bc_entry);	
    /* race condition between __bc_lookup and mapping the page */
    assert(vaddr);	 
    if (bc_entry->buf_state != BC_VALID) {
      assert(0); /* block until page is in */
      wk_waitfor_value((int *)(&bc_entry->buf_state),BC_VALID,NFS_CAP);
    }
  }
  return vaddr;
}

/* we don't unmap a page if is dirty */
static inline int
nfs_put_page(nfsc_p e, int pageno, void *vaddr) {
  int dirty_start,dirty_end;
  int page_dirty_start, page_dirty_end;

  dirty_start = nfsc_get_writestart(e);
  dirty_end = nfsc_get_writeend(e);

  page_dirty_start = dirty_start / NFSPGSZ;
  page_dirty_end = (dirty_end == 0) ? 0 : ((dirty_end - 1) / NFSPGSZ);

  if (pageno < page_dirty_start || pageno > page_dirty_end ||
      dirty_start == dirty_end) {
    /* we unmapped because not dirty */
    if (vaddr == 0) {
      vaddr = nfs_get_page(e, pageno, NFS_CHECK_PAGE);
      assert(vaddr);
    }
    DEALLOCATE_MEMORY_BLOCK(vaddr,NFSPGSZ);
  }
  return 0;
}


/* preconditions:
   offset + length will not span more than 2 pages. 
   pages will be in buffer cache because they are mapped in.

   at the end it unmaps the pages contain in its range */

/* HBXX nfsc_flush_region2 should update the writestart and writeend boundaries,
   not nfsc_flush_region */
static inline int 
nfsc_flush_region2(nfsc_p e, int offset,int length) {
  int dirty_start,dirty_end;
  int page_dirty_start, page_dirty_end;
  int ino,dev;
  u_quad_t block;
  struct bc_entry *bc_entry;
  struct bc_entry *bc_entry2 = NULL;
  struct bc_entry *bc_entry3 = NULL;

  u_int vaddr;

  u_int bc_addr = 0;
  u_int bc_addr2 = 0;
  u_int bc_addr3 = 0;
  struct nfs_fattr fattr;
  int ret,status;
  vaddr = BUFCACHE_REGION_START + (__sysinfo.si_nppages * NBPG);

  assert(length > 0);
  assert(length <= 8192);

  dirty_start = offset;
  dirty_end = offset + length;
  
  page_dirty_start = dirty_start / NFSPGSZ;
  page_dirty_end = (dirty_end == 0) ? 0 : ((dirty_end - 1) / NFSPGSZ);

  if ((page_dirty_end - page_dirty_start < 0 || 
	page_dirty_end - page_dirty_start > 2)) {
    kprintf("%s:%d ASSERTION FAILURE pde %d pds %d ds %d de %d\n",
	    __FILE__,__LINE__,page_dirty_end,page_dirty_start,
	    dirty_start,dirty_end);
    assert(0);
    return -1;
  }

  
  ino = GETNFSCEINO(e);
  dev = GETNFSCEDEV(e);

  block = NFSBCBLOCK(ino,page_dirty_start);

  d5printf("<%d,%d> %d bytes @ %d  pde %d pds %d ds %d de %d\n",
	   dev,ino,length,offset,
	   page_dirty_end,page_dirty_start,
	   dirty_start,dirty_end);

  d5printf("*** WRITING %7d: %7d-%-7d (i%d) [%d,%d]\n",
	   length,offset,offset+length,ino,
	   page_dirty_start,page_dirty_end);
  
#if 1
  {
    struct ae_recv m;
    int remaining = length;
    
    block = NFSBCBLOCK(ino,page_dirty_start);
    bc_entry = __bc_lookup64 ((u32)dev, block);
    if (bc_entry == NULL) {
      kprintf("%3d WARNING: missing bc entry for (%d,%d) while flushing %d@%d (%d)\n",getpid(),
	      (u32)dev, ino,page_dirty_start,length,offset);
      kpr_nfsc(e);
      goto goon;
    }
    assert(bc_entry);
    assert (bc_entry->buf_state == BC_VALID);
    bc_addr = bc_entry->buf_ppn;
    m.n = 1;
    m.r[0].sz = MIN(remaining, NBPG - (dirty_start % NBPG));
    //    m.r[0].data = (char*)(bc_addr * NBPG) + (dirty_start % NBPG);
    //    m.r[0].data += BUFCACHE_REGION_START;
    m.r[0].data = new_getbcmapping(bc_entry) + (dirty_start % NBPG);

    remaining -= m.r[0].sz;
    
    if (page_dirty_start < page_dirty_end ) {
      block = NFSBCBLOCK(ino,page_dirty_start + 1);
      bc_entry2 = __bc_lookup64 ((u32)dev, block);
      assert(bc_entry2);
      assert (bc_entry2->buf_state == BC_VALID);
      bc_addr2 = bc_entry2->buf_ppn;

      m.r[m.n].sz = MIN(remaining, NBPG);
      //      m.r[m.n].data = (char*)(bc_addr2 * NBPG);
      //      m.r[m.n].data += BUFCACHE_REGION_START;
      m.r[m.n].data = new_getbcmapping(bc_entry2);
      remaining -= m.r[m.n].sz;
      m.n++;
    }

    if (page_dirty_start + 1 < page_dirty_end) {
      block = NFSBCBLOCK(ino,page_dirty_start + 2);
      bc_entry3 = __bc_lookup64 ((u32)dev, block);
      assert(bc_entry3);
      assert (bc_entry3->buf_state == BC_VALID);
      bc_addr3 = bc_entry3->buf_ppn;

      m.r[m.n].sz = MIN(remaining, NBPG);
      //      m.r[m.n].data = (char*)(bc_addr3 * NBPG);
      //      m.r[m.n].data += BUFCACHE_REGION_START;
      m.r[m.n].data = new_getbcmapping(bc_entry3);

      remaining -= m.r[m.n].sz;
      m.n++;
    }
    pr_ae_recv(&m);
#if 1
    status = nfs_proc_writev(nfsc_get_fhandle(e),
			     offset,
			     length,
			     &m,
			     &fattr);
    
#endif
    
    if (ret = _exos_self_unmap_page (0,
				     BUFCACHE_REGION_START + bc_addr * NBPG) < 0) {
      kprintf ("DEALLOCATE_MEMORY_BLOCK: _exos_self_unmap_page failed (ret %d)\n", ret);
      assert (0);
    }
    if (bc_addr2) {
      if (ret = _exos_self_unmap_page (0,
				       BUFCACHE_REGION_START + bc_addr2 * NBPG) < 0) {
	kprintf ("DEALLOCATE_MEMORY_BLOCK: _exos_self_unmap_page failed (ret %d)\n", ret);
	assert (0);
      }
    }

    if (bc_addr3) {
      if (ret = _exos_self_unmap_page (0,
				       BUFCACHE_REGION_START + bc_addr3 * NBPG) < 0) {
	kprintf ("DEALLOCATE_MEMORY_BLOCK: _exos_self_unmap_page failed (ret %d)\n", ret);
	assert (0);
      }
    }
    goto goon;

  }
#endif

goon:
  if ((status = nfs_get_nfscd_envid()) && status != __envid) {
    status = sipcout(status,IPC_NFS_WRITE,(u_int)e,offset,length);
  }

#if 0
  d5printf("nfs_proc_write ppn %d,%d,%d\n",
	   bc_entry->buf_ppn,
	   (bc_entry2 == NULL) ? -1 : bc_entry2->buf_ppn,
	   (bc_entry3 == NULL) ? -1 : bc_entry3->buf_ppn);
#endif
  d5printf("VADDR: base: %p  %p  %d %d \n",(char*)vaddr,
	   (char *)vaddr + (dirty_start % NBPG),
	   isvamapped((char*)vaddr),
	   isvamapped((char*)vaddr+NFSPGSZ));
  d5printf("DONE WRITE\n");
  if (status == 0) {
#ifdef NO_WRITE_SHARING
    /* if no write sharing then we update the attributes */
    nfsc_settimestamp(e);
    nfs_fill_stat(&fattr,e);
#else
    /* zero the timestamp so on the next read or stat we fetch it again */
    nfsc_ts_zero(e);
#endif
  }
  return 0;
}

/* flushes the dirty region of a nfsc.  one of two ways:
   NFSC_FLUSH_REGION_ALL: flushes the whole region.
   NFSC_FLUSH_REGION_ALIGN: flushes the region until aligned to a page,
   and <= 2 pages remaining are dirty.
   */
int
nfsc_flush_region(nfsc_p e,short options) {
  int dirty_start,dirty_end;
  int l;
  int status;
  dirty_start = nfsc_get_writestart(e);
  dirty_end = nfsc_get_writeend(e);

  START(nfsc,write);
  show_bc_mapped("BEFORE FLUSH");
  switch(options) {
  case NFSC_FLUSH_REGION_ALL:
    d3printf("FLUSH_ALL\n");
    while((dirty_end - dirty_start) > 0) {

      if ((dirty_end - dirty_start) > 8192)
	l = ((dirty_start + 8192) & ~(NFSPGSZ-1)) - dirty_start;
      else
	l = dirty_end - dirty_start;
      status = nfsc_flush_region2(e,dirty_start,l);
      dirty_start += l;
      nfsc_set_writestart(e,dirty_start);
    }
    break;
  case NFSC_FLUSH_REGION_ALIGN:
    d3printf("FLUSH_ALIGN\n");
    while((dirty_end - dirty_start) >= 8192) {


      l = ((dirty_start + 8192) & ~(NFSPGSZ-1)) - dirty_start;
      status = nfsc_flush_region2(e,dirty_start,l);
      dirty_start += l;
      nfsc_set_writestart(e,dirty_start);
    }
    break;
  default:
    assert(0);
  }
  show_bc_mapped("AFTER FLUSH");
  STOP(nfsc,write);
  return 0;
}






/* NEW NFS READ */

int
nfs_read_new(struct file *filp, char *buffer, int length, int blocking) {
  char *vaddr;
  nfsc_p e;
  int amountToCopy;
  int blockOffset;
  int totlength;
  int size,offset,pageno;
  
  demand(filp, bogus filp);

  /* check to see if it is a directory */
  if (S_ISDIR(filp->f_mode)) {errno = EPERM; return -1;}
  /* make sure we have read permission */
  if ((filp->f_flags & O_WRONLY)) {errno = EBADF; return -1;}

  e = GETNFSCE(filp);
  size = nfsc_get_size(e);
  offset = filp->f_pos;
#if 0
  kprintf("nfs_read_new:  filp: %08x size: %d offset: %d nbyte: %d\n",
	  (int)filp, size, offset, length);
#endif
  /* HBXX ASSUME SIZE IS UP TO DATE FIX ME LATER */
  if (offset >= size) return(0); /* reading EOF */

  if (offset+length > size) {
    length = size - offset;
  }

  if ((offset + length) > NFSMAXOFFSET) 
    return nfs_read(filp,buffer,length,blocking);

  blockOffset = offset % NFSPGSZ; /*don't do each time*/
  totlength = 0;
  pageno = offset / NFSPGSZ;

  while(length) {
    amountToCopy = min (length, NFSPGSZ - blockOffset);
    
    vaddr = nfs_get_page(e, pageno, 0);
    assert(vaddr);
    memcpy (buffer, vaddr + blockOffset, amountToCopy);

    nfs_put_page(e, pageno, vaddr);

    buffer += amountToCopy;
    offset += amountToCopy;
    length -= amountToCopy;
    totlength += amountToCopy;

    blockOffset = 0;
    pageno++;
  }
  filp->f_pos += totlength;
  return totlength;
}

int
nfs_bmap(struct file *filp, u_int *diskBlockHighP, u_int *diskBlockLowP,
	 off_t offset, u_int *seqBlocksP) {
  nfsc_p e;
  int extent;
  u_quad_t block;
  int tmp_inode, tmp_offset;
  demand (filp, bogus filp);
  demand(! (offset & 0xfff), offset must be page aligned);

  e = GETNFSCE(filp);
  if (offset > PGROUNDUP(nfsc_get_size(e))) {
    kprintf("nfs_bmap error: offset: %u:%u, size: %u\n",(int)offset,(int)PGROUNDUP(nfsc_get_size(e)), (int)nfsc_get_size(e));
    errno = EINVAL;
    return -1;
  }
  
  tmp_offset = (int )offset;
  extent = (nfsc_get_size(e) - tmp_offset + (NFSPGSZ - 1)) / NFSPGSZ;
  block = NFSBCBLOCK(GETNFSCEINO(e),((int)tmp_offset)/NFSPGSZ);
  tmp_inode = NFSBCBLOCK2I(block);
  tmp_offset = NFSBCBLOCK2O(block);
#if 0
  printf ("nfs_bmap_read: block = %x offset = %qx tmp_offset = %x inode = %x tmp_i"
          " node = %x\n", block, 
          offset/NBPG, tmp_offset, e->sb.st_ino, tmp_inode);  
#endif
  assert(offset + extent * NFSPGSZ < NFSMAXOFFSET);
  
  if (seqBlocksP) *seqBlocksP = extent;
  *diskBlockHighP = QUAD2INT_HIGH(block);
  *diskBlockLowP = QUAD2INT_LOW(block);
#if 0
  printf ("nfs_bmap: size = %qd offset %qd pp %qd xt %d\n", 
          nfsc_get_size(e), offset, offset/NFSPGSZ, extent);
#endif
  return 0;
}

int
nfs_bmap_read(int fd, u_quad_t block) {
  int inode;
  int offset;
  struct file *filp;
  nfsc_p e;
  char *status;

  CHECKFD(fd, -1);
  filp = __current->fd[fd];
  assert(filp->op_type == NFS_TYPE);
  e = GETNFSCE(filp);

  inode = NFSBCBLOCK2I(block);
  offset = NFSBCBLOCK2O(block);
  
  offset = offset * NFSPGSZ;

  if (inode != e->sb.st_ino) {
    pr_nfsc(e);
    assert(0);
  }

  assert(offset < NFSMAXOFFSET);
  status = nfs_get_page(e, offset / NFSPGSZ, 0);
  if (status == NULL) {
    kprintf("nfs_bmap_read: status: %p, errno: %d\n",status,errno);
    assert(0);
    return -1;
  }
  return 0;
}


static int run = 0;
static inline int 
nfs_write_page(nfsc_p e, int offset, int amountToCopy, char *buffer) {


  int length;
  int dirty_start, dirty_end, end,start;
  int page_dirty_start, page_dirty_end, page_start, page_end;
  short changed = 0;
  length = amountToCopy;

  end = offset + length;
  start = offset;

  d2printf("nfs_write_page: offset: %d, amount: %d, buffer: %p\n",
	   offset, amountToCopy, buffer);
  dirty_start = nfsc_get_writestart(e);
  dirty_end = nfsc_get_writeend(e);

  if (dirty_end == dirty_start) {
    /* move both pointers forward */
    dirty_start = dirty_end = offset;
    nfsc_set_writestart(e,dirty_start);
    nfsc_set_writeend(e,dirty_end);
  }

  page_start = start / NFSPGSZ;
  page_end = page_start;
  page_dirty_start = dirty_start / NFSPGSZ;
  page_dirty_end = (dirty_end == 0) ? 0 : ((dirty_end - 1) / NFSPGSZ);

  assert(dirty_start <= dirty_end);


  /* ONE PAGE WRITE AT A TIME */
  assert((offset % NFSPGSZ) + amountToCopy <= NFSPGSZ);
  


  d2printf("* %3d %10s: data %7d-%-7d:%7d dirty %7d-%-7d:%6d %d\n",run,
	  __progname,start,end,end-start,dirty_start,dirty_end,
	  dirty_end-dirty_start,GETNFSCEINO(e));
  run++;

  d2printf("     ps %4d pe %4d pds %4d pde %4d\n",
	  page_start,page_end, page_dirty_start, page_dirty_end);
#if 0
#endif

  if (page_start == page_dirty_start ||
      page_start + 1 == page_dirty_start) {
    if (start < dirty_start) {
      d2printf("decreasing start %d --> %d\n",dirty_start, start);
      nfsc_set_writestart(e,start);
      dirty_start = start;
    }
    /* prepending */
    changed = 1;
  }
  if (page_end == page_dirty_end ||
      page_end - 1 == page_dirty_end) {
    if (end > dirty_end) {
      d2printf("increasing    end %d --> %d  (start %d)\n",dirty_end, end, dirty_start);
      nfsc_set_writeend(e,end);
      dirty_end = end;
    }
    /* appending */
    changed = 1;
  }


  if ((dirty_end - dirty_start) >= NFS_FLUSH_THRESHOLD) {
    d3printf("%4d %10s: FLUSHING  %7d-%-7d :%7d  data %7d-%-7d :%d\n",
	    run,__progname,dirty_start,dirty_end,(dirty_end - dirty_start),
	    start,end,changed);

      nfsc_flush_region(e,NFSC_FLUSH_REGION_ALIGN);

  } else if (!changed && 
	     (page_start < (page_dirty_start - 1) ||
	     page_end > (page_dirty_end + 1))) {
    d3printf("%4d %10s: OUT OF ORDER dirty: %7d-%-7d :%7d data %7d-%-7d :%d\n",
	    run,__progname,
	    nfsc_get_writestart(e),nfsc_get_writeend(e),
	    nfsc_get_writeend(e)-nfsc_get_writestart(e),
	    start,end,end - start);

    nfsc_flush_region(e,NFSC_FLUSH_REGION_ALL);

    d3printf("# setting: %d-%d\n",start,end);
    nfsc_set_writestart(e,start);
    nfsc_set_writeend(e,end);
  } 


#if 0
  status = nfs_proc_write(nfsc_get_fhandle(e),
			  offset,
			  amountToCopy,
			  buffer,
			  &fattr);
  
  nfsc_settimestamp(e);
  nfs_fill_stat(&fattr,e);
  return status;
#else
  return 0;
#endif

}

/* NEW NFS WRITE 
 ASSUMPTION ONLY ON BLOCK CAN BE DIRTY AT A TIME */
int
nfs_write_new(struct file *filp, char *buffer, int length, int blocking) {
  char *vaddr;
  nfsc_p e;
  int amountToCopy;
  int blockOffset;
  int totlength;
  int size,offset,pageno, status;
  int ino,dev;
  u_quad_t block;
  
  demand(filp, bogus filp);

  /* check to see if it is a directory */
  if (S_ISDIR(filp->f_mode)) {errno = EPERM; return -1;}
  /* make sure we have write permission */
  if (!((filp->f_flags & O_WRONLY) || (filp->f_flags & O_RDWR)))
    {errno = EBADF; return -1;}

  e = GETNFSCE(filp);
  size = nfsc_get_size(e);

  if (filp->f_flags & O_APPEND)
    filp->f_pos = nfsc_get_size(e);

  offset = filp->f_pos;
#if 1
  if (1 || offset != size) {
    d3printf("NFS_WRITE_NEW (%10s): f: %08x sz: %5d off: %5d nbyte: %d\n",
	  __progname,(int)filp, size, offset, length);
  }
#endif
  /* HBXX ASSUME SIZE IS UP TO DATE FIX ME LATER */

  if ((offset + length) > NFSMAXOFFSET) 
    return nfs_write(filp,buffer,length,blocking);

  blockOffset = offset % NFSPGSZ; /*don't do each time*/
  totlength = 0;
  pageno = offset / NFSPGSZ;

  while(length) {
    
    amountToCopy = min (length, NFSPGSZ - blockOffset);

    if ((amountToCopy < min(NFSPGSZ, (nfsc_get_size(e) - offset)))
	|| (blockOffset != 0)) {
      d3printf("getting old page: %d\n",pageno);
      vaddr = nfs_get_page(e, pageno, 0);
      assert(vaddr);

    } else {
      
      d3printf("getting old or allocating new page: %d\n",pageno);
      /* just get old copy if it exists -- will replace otherwise */
      vaddr = nfs_get_page(e, pageno, NFS_CHECK_PAGE);
      if (vaddr == NULL) {
	dev = GETNFSCEDEV(e);
	ino = GETNFSCEINO(e);
	block = NFSBCBLOCK(ino,pageno);

	vaddr = ALLOCATE_BC_PAGE();
	assert(vaddr);
	d3printf("allocating new page for %d,%d pageno %d, physical %p ppn: %d\n",
		 dev,ino, pageno,vaddr,va2ppn(vaddr));
	nfs_new_bc_insert(vaddr,dev,block);
	
	/* need to detect hole in the page if so clear it properly */
      }
    }

    d3printf("memcpy %p:%d --> %p, offset: %d pageno: %d ppn: %d\n",
	    buffer,amountToCopy,vaddr+ blockOffset,offset,pageno,va2ppn(vaddr));

    memcpy(vaddr + blockOffset,buffer, amountToCopy);

    status = nfs_write_page(e, offset, amountToCopy, buffer);

    assert(status == amountToCopy || status == 0);

    buffer += amountToCopy;
    offset += amountToCopy;
    length -= amountToCopy;
    totlength += amountToCopy;

    blockOffset = 0;
    pageno++;
  }
  filp->f_pos += totlength;

  if (nfsc_get_writeend(e) > nfsc_get_size(e)) {
    nfsc_set_size(e,(off_t)nfsc_get_writeend(e));
  }

  return totlength;
}

int
nfsc_sync(nfsc_p e) {
  int dirty_start = nfsc_get_writestart(e);
  int dirty_end = nfsc_get_writeend(e);


  if (S_ISDIR(nfsc_get_mode(e))) {
    nfsc_set_writestart(e,0);
    nfsc_set_writeend(e,0);
    return 0;
  }
  if ((dirty_end - dirty_start) == 0) return 0;

  {
    d4printf("* %4d %10s: CLOSING %7d %2d  dirty %7d-%-7d:%7d\n",
	    run,__progname,GETNFSCEINO(e),
	    nfsc_get_flags(e),
	    dirty_start,dirty_end,dirty_end-dirty_start);
    run++;

    nfsc_flush_region(e,NFSC_FLUSH_REGION_ALL);
    /* reset boundaries */
    nfsc_set_writestart(e,0);
    nfsc_set_writeend(e,0);

  }
  return 0;
}



/* FOR DEBUGGING */


static inline char *
get_buf_name(unsigned int state) {
  static char number[10];
  switch(state) {
  case BC_EMPTY: return "BC_EMPTY";
  case BC_VALID: return "BC_VALID";
  case BC_COMING_IN: return "BC_COMING_IN";
  case BC_GOING_OUT: return "BC_GOING_OUT";
  default:
    sprintf(number,"%d",state);
    return (char *)number;
  }
}

int 
pr_nfs_bc(int fd) {
  struct file *filp;
  struct bc_entry *bc_entry;
  nfsc_p e;
  int ino, pageno, size, dev;
  u_quad_t block;
  int last;
  filp = __current->fd[fd];

  if (filp == NULL || filp->f_dev >= 0) {errno = EBADF; return -1;}

  e = GETNFSCE(filp);
  dev = GETNFSCEDEV(e);
  ino = GETNFSCEINO(e);
  size = nfsc_get_size(e);

  fprintf(stderr,"File: inode: %d dev: %d  size: %d\n",ino,dev,size);
  last = -1;
  for (pageno = 0; pageno < (size + NFSPGSZ - 1) / NFSPGSZ; pageno++) {
    block = NFSBCBLOCK(ino,pageno);
    bc_entry = __bc_lookup64 ((u32)dev, block);

    if (bc_entry) {
      fprintf(stderr,"page: %d (%8d) buf_state %s\n",
	      pageno,pageno*NFSPGSZ,
	      get_buf_name(bc_entry->buf_state));
      if (last + 1 != pageno) {
	fprintf(stderr,"not sequential\n");
	last = pageno;
      } else {
	last++;
      }
    }
  }
  fprintf(stderr,"Done\n");
  return 0;
}
