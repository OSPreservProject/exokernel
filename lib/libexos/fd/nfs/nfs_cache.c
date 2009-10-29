
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

#include "nfs_rpc.h"
#include "nfs_struct.h"
#include "nfs_proc.h"

#include <exos/critical.h>

extern char *__progname;

#include <exos/vm-layout.h>		/* for PAGESIZ */
#include <exos/vm.h>			/* for PG_SHARED */
#include <xok/sys_ucall.h>

#include <xok/pmap.h>
#include <xok/pxn.h>
#include <xok/bc.h>
#include <xok/sysinfo.h>

#include <exos/ubc.h>
#include <exos/uwk.h>
#include <exos/cap.h>

#include <unistd.h>

#define printf if (0) printf
#define kprintf if (1) kprintf

//#define PARANOIC		/* perform extra validity tests */
void 
nfs_bc_insert (char *addr, int dev, int index, int offset);



static inline void 
nfsc_checkfence(nfsc_p p) {
#ifdef PARANOIC
  assert(p);
  if(p->fence1 != NFSCACHE_FENCE1 || p->fence2 != NFSCACHE_FENCE2) {
    fprintf(stderr,"BROKEN NFSCE FENCE IN %p (%d out of %d)\n",p,NFSCENUM(p), NFSATTRCACHESZ);
    kprintf("BROKEN NFSCE FENCE IN %p\n",p);
    kpr_nfsc(p);
    pr_nfsc(p);
    assert(0);
  }
#endif
}

static inline void 
nfsc_setfence(nfsc_p p) {
  p->fence1 = NFSCACHE_FENCE1;
  p->fence2 = NFSCACHE_FENCE2;
}


static void 
nfsc_cleare(nfsc_p p) {
  p->flags = 0;
  p->inuse = 0;
  p->refcnt = 0;
  p->dev = 0;
  p->inbuffercache = 0;
  p->writestart = p->writeend = p->n_dirsize = 0;
  nfsc_ts_zero(p);		/* zero timestamp */
  p->sb.st_ino = 0;
}

static void 
nfsc_checkrefcntinuse(nfsc_p p) {
#ifdef PARANOIC
  if (p->inuse == 0 && p->refcnt != 0 || 
      p->inuse != 0 && p->refcnt == 0) {
    fprintf(stderr,"INUSE REFCNT VIOLATION\n");
    kprintf("INUSE REFCNT VIOLATION\n");
    printf("inuse: %d, refcnt: %d\n",
	   p->inuse, p->sb.st_ino);
  }
#endif
}

void 
nfsc_init(nfsc_p cachep, int sz) {
  assert(sz > 0);
  printf("nfsc_init %p %d\n",cachep,sz);
  START(nfsc,init);
  for (; sz > 0; sz--,cachep++) {
    nfsc_setfence(cachep);
    cachep->inuse = 0;
    nfsc_cleare(cachep);
    nfsc_checkfence(cachep);
  }
  STOP(nfsc,init);

}

void 
nfsc_init_dev(int dev) {
    int ret;
    int k = 0;
    char *addr;
    struct Xn_name xname;
    struct Xn_xtnt xtnt;


    printf("nfsc_init_dev %d\n",dev);
    if (dev >= 0) {
      printf("nfsc_init_dev: device greater than 0, convention is nfs devs are < 0\n");
    }

printf ("about to alloc pxn for device %d\n", dev);
    FILLXNAME(dev,&xname);

    xtnt.xtnt_block = 0ULL;
    xtnt.xtnt_size  = 0xffffffffffffffffULL;

    if ((ret = sys_pxn_alloc(1,1,1,k,k,&xname)) < 0) {
      kprintf("(%d)nfsc_init_dev %d: sys_pxn_alloc failed: %d\n",__envid,dev,ret);
      assert(0);
    } else if ((ret = sys_pxn_add_xtnt (NULL, &xname, &xtnt, ACL_W, CAP_ROOT, -1, -1)) < 0) {
      kprintf("nfsc_init_dev %d: sys_pxn_add_xtnt failed: %d\n",dev,ret);
      assert(0);
    }

    /* test bc_insert */
    addr = ALLOCATE_BC_PAGE();
    assert(addr);
    nfs_bc_insert(addr, dev, 0, 0);
    DEALLOCATE_MEMORY_BLOCK(addr,NBPG);
}

static void inline
nfsc_checkduplicates(int dev, int ino) {
#ifdef PARANOIC
  nfsc_p p;
  int count = 0;
  for (p = nfs_shared_data->entries;
       p < nfs_shared_data->entries + NFSATTRCACHESZ ; 
       p++) {
    if (p->dev == dev && p->sb.st_ino == ino) count++;
  }
  if (count != 1) {
    fprintf(stderr,"WRONG NUMBER OF COPIES of dev: %d, ino: %d  COUNT: %d\n",dev,ino, count);
    kprintf("WRONG NUMBER OF COPIES of dev: %d, ino: %d  COUNT: %d\n",dev,ino, count);
  }
#endif
}

nfsc_p 
nfsc_get(int dev, int ino) {
  nfsc_p p, p_min;
  nfsc_t fakep;

  START(nfsc,get);
  /* largest time possible */
  gettimeofday(&fakep.timestamp,(void *)0);
  p_min = &fakep;

//  fprintf(stderr,"NFSC_GET request %d,%d\n",dev,ino);
  
  printf("0 start: %p, end: %p\n",nfs_shared_data->entries,
	 nfs_shared_data->entries + NFSATTRCACHESZ );

retry:
  for (p = nfs_shared_data->entries;
       p < nfs_shared_data->entries + NFSATTRCACHESZ ; 
       p++) {
//    printf("1 looking at %d %p\n",NFSCENUM(p),p);
    nfsc_checkfence(p);
    EnterCritical();
    if (p->sb.st_ino == ino && p->dev == dev) {
      printf("inuse: %d, refcnt: %d\n",
	     p->inuse, p->sb.st_ino);
      nfsc_checkrefcntinuse(p);
      p->inuse = 1;
      p->refcnt++;
      ExitCritical();
      printf("NFSC_GET: returning %d,%d used %d (%d)\n",
	     dev,ino,NFSCENUM(p),p->refcnt);
      nfsc_checkduplicates(dev,ino);
      STOP(nfsc,get);
      return p;
    }
    if (!p->inuse) {
      assert(p->refcnt == 0);
//      printf("TS sec: p%ld m%ld, usec: p%ld m%ld\n",
//	     p->timestamp.tv_sec,p_min->timestamp.tv_sec,
//	     p->timestamp.tv_usec,p_min->timestamp.tv_usec);

      if (p->timestamp.tv_sec < p_min->timestamp.tv_sec ||
	  (p->timestamp.tv_sec == p_min->timestamp.tv_sec &&
	   p->timestamp.tv_usec < p_min->timestamp.tv_usec)) {
	p_min = p;
//	printf("NEW MIN: %p (%d)\n",p_min,NFSCENUM(p_min));
      }
      
    }
    ExitCritical();
  }
  printf("Prior entry not found using p_min: %p (%d)\n",p_min,NFSCENUM(p_min));
  p = p_min;

  if (p == &fakep) {
    fprintf(stderr,"NFSC_GET out of space, nothing found\n");
    pr_nfsc(nfs_shared_data->entries);
    STOP(nfsc,get);
    return (nfsc_p) 0;
  }
  /* somebody snatched away from us. */
  EnterCritical();
  if(p->inuse) {
    ExitCritical();
    goto retry;
  } else {
    assert(p->refcnt == 0);
    p->inuse = 1;
    ExitCritical();
  }

  if (p->sb.st_ino != 0) {
    printf("NFSC_GET FLUSH BC %d %d (while looking %d %d)\n",
	    p->dev,p->sb.st_ino,dev,ino);
    /* no unreferenced nfsc should be dirty */
    assert(nfsc_get_writestart(p) == nfsc_get_writeend(p));
  }
  EnterCritical();
  assert(p->refcnt == 0);
  nfsc_cleare(p);
  p->inuse = 1;
  p->refcnt = 1;
  p->dev = dev;
  p->sb.st_ino = ino;
  nfsc_setfence(p);
  ExitCritical();
  printf("NFSC_GET: returning %d,%d new  %d\n",dev,ino,NFSCENUM(p) );
  nfs_shared_data->hint_e = p;
  nfsc_checkduplicates(dev,ino);
  STOP(nfsc,get);
  return p;
}

void
nfsc_put(nfsc_p p) {
  extern char *__progname;
  nfsc_checkfence(p);
//  fprintf(stderr,"NFSC_PUT request %d,%d      %d\n",p->dev,
//	 p->sb.st_ino,p - nfs_shared_data->entries);

  START(nfsc,put);

  if (!p->inuse) {
    fprintf(stderr,"WARNING RETURNING UNUSED NFSCE refcnt: %d\n",p->refcnt);
    kpr_nfsc(p);
    pr_nfsc(p);
    STOP(nfsc,put);
    return;
  }
  if (p->flags & NFSCE_NEVERRELEASE && p->refcnt == 1) {
    kprintf("TRYING TO RELEASE NFSCE %p, that is marked no release (pid %s %d)\n",
	    p,__progname,getpid());
#if 0
    kpr_nfsc(p);
#endif
    STOP(nfsc,put);
    return;
  }

  EnterCritical();
  nfsc_dec_refcnt(p);
  if (p->refcnt == 0) p->inuse = 0;
  ExitCritical();
  STOP(nfsc,put);
}

/* checks to see if e is uptodate (it was updated less than timeout seconds ago),
   if not will fetch attributes from server
   returns 0; if mod times have not changed.
   returns 1; if mod times have changed
   returns -1 if there is an error in fetching the attributes
   it will not flush anything.
 */
int
nfsc_checkchange(nfsc_p e, int timeout) {
  int status;
  struct timeval diff, curtime;
  struct nfs_fh *fhandle;

  fhandle = nfsc_get_fhandle(e);

  gettimeofday(&curtime, (void *)0);
  timersub(&curtime, &(e->timestamp), &diff);

  if (diff.tv_sec > timeout || nfsc_ts_iszero(e)) {
    /* get attributes, and check change of access time */
    struct timespec cached_mtime;

    cached_mtime = nfsc_get_mtimespec(e);

    status = nfs_stat_fh(fhandle, e);
    if (status == -1) return status;
    nfsc_settimestamp(e);
    /* The fhandle has probably gone stale */

    assert(status == 0);
    /*     if (cached_st_mtime != e->sb.st_mtime || */
    /* 	cached_st_mtimensec != e->sb.st_mtimensec) { */
    if (timespeccmp(&cached_mtime, &nfsc_get_mtimespec(e), !=)) {
      /* mod time has changed */
      return 1;
    } else {
      /* mod time has not changed */
      return 0;
    }
  } else {
    /* the nfsce is within timeout  */
    return 0;
  }
}


char *
ALLOCATE_BC_PAGE (void)
{
   int ret;
   unsigned int bc_addr = BUFCACHE_REGION_START + (__sysinfo.si_nppages * NBPG);

   START(nfsc,alloc);

   if (((ret = _exos_self_insert_pte (0, (PG_W | PG_P | PG_U | PG_SHARED),
				      bc_addr, ESIP_DONTPAGE, NULL)) < 0) ||
       (vpt[PGNO(bc_addr)] == 0)) {
     kprintf ("ALLOCATE_BC_PAGE: _exos_self_insert_pte failed (ret %d)\n",
	      ret);
     STOP(nfsc,alloc);
     assert(0);
   }

   if (((ret = _exos_self_insert_pte (0,
				      ((PGNO(vpt[PGNO(bc_addr)]) << PGSHIFT) 
				       | PG_W | PG_P | PG_U | PG_SHARED),
				      (BUFCACHE_REGION_START +
				       (PGNO(vpt[PGNO(bc_addr)]) * NBPG)),
				      ESIP_DONTPAGE, NULL))
	< 0) ||
       (vpt[PGNO(bc_addr)] == 0)) {
      kprintf ("ALLOCATE_BC_PAGE: failed to add real mapping (ret %d)\n", ret);
      assert (0);
   }

   bc_addr = BUFCACHE_REGION_START + (PGNO(vpt[PGNO(bc_addr)]) * NBPG);

   if ((ret = _exos_self_unmap_page (0, (BUFCACHE_REGION_START + (__sysinfo.si_nppages * NBPG)))) < 0) {
     kprintf ("ALLOCATE_BC_PAGE: failed to clobber fake mapping (ret %d)\n", ret);
     assert (0);
   }

   STOP(nfsc,alloc);
#if 0
   fprintf(stderr,"ALLOCATE_BC_PAGE %p ppn: %d  %s\n",(char *)bc_addr,
	   PGNO(vpt[PGNO(bc_addr)]),
	   __progname);
#endif
  return ((char *) bc_addr);
}


void 
DEALLOCATE_MEMORY_BLOCK (char *ptr, int size)
{
  unsigned int bc_addr = (unsigned int) ptr;
   int ret;

   START(nfsc,dealloc);
   assert (size == NBPG);

   if ((bc_addr < BUFCACHE_REGION_START) || (bc_addr >= BUFCACHE_REGION_END)) {
     kprintf ("DEALLOCATE_MEMORY_BLOCK: ptr (%p) out of range\n", ptr);
      assert (0);
   }

   if ((vpd[PDENO (bc_addr)] & (PG_P|PG_U)) != (PG_P|PG_U)) {
     assert(0);
   }
   if (ret = _exos_self_unmap_page (0, bc_addr) < 0) {
      kprintf ("DEALLOCATE_MEMORY_BLOCK: _exos_self_unmap_page failed (ret %d)\n", ret);
      assert (0);
   }
//   fprintf(stderr,"DEALLOCATE_MEMORY_BLOCK %p\n",(char*)bc_addr);
   STOP(nfsc,dealloc);
}




void 
nfs_bc_insert (char *addr, int dev, int ino, int offset) {
  u_int k;
  int ret;
  u_int ppn;
  u_quad_t block;
  Pte pte;
  struct Pp_state pp_state;
  struct bc_entry *bc_entry;
  struct Xn_name xn;

  START(nfsc,insert);
  k = 0;
  block = NFSBCBLOCK(ino,offset);
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

  printf("#nfs_bc_insert: addr: %p d:%d i:%d off:%d pp:%d (%d,%08qx) \n", 
	 addr, dev, ino, offset,ppn,dev,block);
  
  if ((ret = sys_bc_insert64 (&xn, QUAD2INT_HIGH(block), 
			      QUAD2INT_LOW(block), 0, k, ppn, &pp_state)) 
      != 0) {
    kprintf ("sys_bc_insert64 failed: ret %d (ppn %d, diskBlock %qd, buffer %p)\n"
	    ,ret, ppn, block, addr);
    STOP(nfsc,insert);
    return;
  }
  if ((bc_entry = __bc_lookup64 ((u32)dev, block)) == NULL) {
    kprintf ("lookup failed after insert\n");
    STOP(nfsc,insert);
    return ;
    assert (0);
  }
  if (!((bc_entry->buf_ppn == (((u_int)addr - BUFCACHE_REGION_START)/NBPG)))) {
    kprintf("((bc_entry->buf_ppn NOT EQ (((u_int)addr - BUFCACHE_REGION_START)/NBPG)))\n");
  }
  STOP(nfsc,insert);

}

void 
nfs_flush_bc(int dev, int ino, int pageno) {
  u_quad_t block = NFSBCBLOCK(ino,0);
  //  printf("nfs_flush_bc dev: %d ino: %d block: %08x range: %d, pageno: %d\n",
	     //	 dev, ino, block,  NFSMAXOFFSET / NFSPGSZ,pageno);
  START(nfsc,flush);

  if (pageno > (NFSMAXOFFSET / NFSPGSZ)) {
    STOP(nfsc,flush);
    return;
  }
  if (pageno == NFSFLUSHALL) {
    sys_bc_flush64(dev, QUAD2INT_HIGH (block), 
		   QUAD2INT_LOW(block), NFSMAXOFFSET / NFSPGSZ);
  } else {
    sys_bc_flush64(dev, QUAD2INT_HIGH(block), QUAD2INT_LOW (block), pageno);
  }
  STOP(nfsc,flush);
}


void 
nfs_flush_dev(int dev) {
  int i;
  START(nfsc,flush);
  if (dev == 0) {
    for (i = -1 ; i > -4 ; i--) {
      sys_bc_flush(i, BC_SYNC_ANY, 0xffffffff);
    }
  } else {
    sys_bc_flush(dev, BC_SYNC_ANY, 0xffffffff);
  }
  STOP(nfsc,flush);
}

int 
nfs_get_nfscd_envid(void) {
  /* XXX -- not implemented */
  return 0;
}

