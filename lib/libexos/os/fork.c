
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


#include <exos/conf.h>
#include <exos/kprintf.h>
#include <stdio.h>
#include <string.h>
#include <xok/defs.h>
#include <xok/sys_ucall.h>
#include <xok/sys_ubatch.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/pmap.h>
#include <xok/sysinfo.h>
#include <exos/osdecl.h>
#include <exos/exec.h>
#include <exos/callcount.h>
#include <exos/vm.h>
#include <exos/process.h>
#include <exos/critical.h>
#include <exos/vm-layout.h>
#include <exos/cap.h>
#include <xok/ash.h>
#include <exos/signal.h>
#ifdef PROCD
#include "procd.h"
#include <exos/uwk.h>
extern char *__progname;
#include <unistd.h>		/* for sleep */
#endif

#ifdef SYSV_SHM
#include <sys/shm.h> 
#endif

#include <assert.h>

#include <setjmp.h>

#if 1
#include <fd/fdstat.h>
#else
#define START(x,y)
#define STOP(x,y)
#endif


#if 1
void
dump_pt(int envid){
  u_int va;
  Pte pte;
  int r;

  for ( va = NBPG ; va < UTOP ; va += NBPG) {
    pte = sys_read_pte (va, 0, envid, &r);
    if (!(pte & PG_P))
      continue;

    printf("va: %8x ppn: %8x s:%d w:%d c:%d\n",(int)va,
	   (pte >> PGSHIFT),
	   (pte & PG_SHARED) ? 1:0,
	   (pte & PG_W) ? 1:0,
	   (pte & PG_COW) ? 1:0);
  }
}
#endif

/* Fork

   System calls are expensive so we batch them up. We scan through our
   address space collecting pte's to be mapped into the new address
   space (these are stored in ptes[] below). At the same time we need
   to write-protect our own address space. To do this efficiently, we
   write protect the biggest contigous region possible durring each
   call. As we scan through our address space we remember (in
   prot_start) the start of the current region that needs to be
   write-protected and the number of pages in it (prot_pgs).

   We have to actually duplicate two pages: the new env needs it's own
   u-area and it's own exception stack. Other than that, everything is
   copy-on-write.
   
   Here's a quick sumarry of what some key variables hold:

   insrt_pgs:     number of pte's to insert in next batched insertion
   ptes:          holds insrt_pgs ptes to be inserted into child
   insert_start:  starting va to insert ptes at
   prot_pgs:      number of pages to write-protect next batched call
   prot_start:    va to start write-protecting at 

 */

#define ISFORK  0
#define ISVFORK 1

static pid_t
fork1 (int forktype)
{
  int envid;
  u_int va;
  u_int k = 0;
  struct Env e;
  int r;

  volatile u_int old_prologue = UAREA.u_entprologue;
  volatile u_int old_epilogue = UAREA.u_entepilogue;
  int NewPid = 1;
#ifdef PROCESS_TABLE
  char *argv[] = {UAREA.name, NULL}; 
#endif
  Pte pte;
#define BATCH 128
  Pte ptes[BATCH];
  int insrt_pgs = 0;
  int prot_pgs = 0;
  u_int prot_start = 0;
  u_int insert_start;
  struct Uenv cu;
  int ret;

  assert(forktype == ISFORK || forktype == ISVFORK);

  START(misc,fork);

#define WRITE_PROTECT								\
  if (prot_pgs > 0) {								\
    int ret2;									\
    if ((ret2 = sys_self_mod_pte_range (0, PG_COW, PG_W, prot_start,		\
					prot_pgs)) < 0) {			\
      sys_cputs ("fork: couldn't call sys_self_mod_pte_range\n");		\
      kprintf ("ret = %d, prot_start = %x, prot_pgs = %d\n", ret2, prot_start,	\
	       prot_pgs);							\
      goto fork_error;								\
    }										\
    prot_pgs = 0;								\
  }

  if (! (envid = sys_env_alloc (0, &ret))) {
    STOP(misc,fork);

    return (-1);
  }

  /* save fpu state so child can get a copy */
  {
    extern u_int _exos_fpu_used_ctxt;
    if (_exos_fpu_used_ctxt == 1)
    { 
      EnterCritical();
      asm("fnsave __exos_fpus\n"); 
      asm("fwait\n");

      /* need to restore it so parent can keep on using fpu */
      asm("frstor __exos_fpus\n");
      ExitCritical();
    }
  }

#ifdef ASH_ENABLE
  if (__curenv->env_ashuva) sys_env_ashuva(k, envid, __curenv->env_ashuva);
#endif

  for (insert_start = va = NBPG ; va < UTOP ; ) {
    /* time to insert our collected pte's into child? */
    if ((va - insert_start)/NBPG >= BATCH) {
      if (insrt_pgs) {
	SYSBATCH_ALLOC(batched_mappings, BATCH);
	int i;
	u32 dev;
	u32 blk;		
	int ret;
	struct Xn_name *xn;
	struct Xn_name xn_nfs;

	sysbatch_init (batched_mappings, BATCH);
	for (i = 0; i < insrt_pgs; i++, insert_start += NBPG) {
	  if (ptes[i] & PG_P) {
	    u_int ppn = ptes[i] >> PGSHIFT;
	    if (__ppages[ppn].pp_buf) {
	      if ((ret = sys_bc_ppn2buf (ppn, &dev, &blk)) < 0) {
		sys_cputs ("fork: couldn't call sys_bc_ppn2buf\n");
		kprintf ("ret = %d\n", ret);
		goto fork_error;
	      }
	      if (dev >= MAX_DISKS) {
		xn_nfs.xa_dev = dev;
		xn_nfs.xa_name = 0;
		xn = &xn_nfs;
	      } else {
		xn = &__sysinfo.si_pxn[dev];
	      }
	      sys_batch_bc_buffer_map (batched_mappings, xn, CAP_ROOT, ptes[i], insert_start, CAP_ROOT, envid);
	      continue;
	    } 
	  }
	  sys_batch_insert_pte (batched_mappings, CAP_ROOT, ptes[i], insert_start, CAP_ROOT, envid);
	}

	ret = sys_batch (batched_mappings);
	if (ret < 0) {
	  sys_cputs ("fork: sys_batch failed\n");
	  kprintf ("retval = %d\n", batched_mappings[sysbatch_error (&batched_mappings[0])].sb_syscall.retval);
	  goto fork_error;
	}
	  
	insrt_pgs = 0;
      }
      insert_start = va;
    }

    if ((vpd[PDENO (va)] & (PG_P|PG_U)) != (PG_P|PG_U)) {
      va = (va + NBPD) & ~PDMASK;
      WRITE_PROTECT;
      continue;
    }

    pte = vpt[va >> PGSHIFT];
    if (! (pte & PG_P)) {
      ptes[insrt_pgs++] = pte;
      va += NBPG;
      WRITE_PROTECT;
      continue;
    } else if (pte & PG_SHARED) {
      ptes[insrt_pgs++] = pte;
      WRITE_PROTECT;
#ifdef ASH_ENABLE
    } else if ((va >= __stkbot && va < USTACKTOP) ||
	       (va >= 0x2000 && va < 0x400000)) {
#else
    } else if (va >= __stkbot && va < USTACKTOP) {
#endif
      /* copy the top of stack since that's where our exception stack is */
      if (_exos_self_insert_pte (0, PG_P|PG_U|PG_W, FORK_TEMP_PG,
				 ESIP_DONTPAGE, NULL) < 0) {
	sys_cputs ("(\n");
	goto fork_error;
      }
      bcopy ((char *)va, (char *)FORK_TEMP_PG, NBPG);
      if (_exos_insert_pte (0, (vpt[FORK_TEMP_PG >> PGSHIFT] & ~PGMASK) |
			    (pte & PGMASK), va, 0, envid, 0, NULL) < 0) {
	sys_cputs ("XXX\n");
	goto fork_error;
      }
      ptes[insrt_pgs++] = (vpt[FORK_TEMP_PG >> PGSHIFT] & ~PGMASK) |
	(pte & PGMASK);
      WRITE_PROTECT;
      if (_exos_self_unmap_page (0, FORK_TEMP_PG) < 0) {
	sys_cputs ("^\n");
	goto fork_error;
      }
    } else if (va == (UADDR & ~PGMASK)) {
      /* alloc page for u area */
      Pte u_area_pte;

      u_area_pte = sys_read_pte (UADDR, 0, envid, &r);
      ptes[insrt_pgs++] = u_area_pte;
      WRITE_PROTECT;
#ifdef ASH_ENABLE
    } else if (va == __curenv->env_ashuva + ASH_UADDR) {
      /* u-area alias */
      Pte u_area_pte;

      u_area_pte = sys_read_pte (UADDR, 0, envid, &r) & ~PGMASK;
      ptes[insrt_pgs++] = u_area_pte | (pte & PGMASK);
      WRITE_PROTECT;
#endif /* ASH_ENABLE */
    } else if (!(pte & PG_W)) {
      /* normal page - not writeable */
      ptes[insrt_pgs++] = pte;
      WRITE_PROTECT;
    } else {
      /* normal page - writeable */
      ptes[insrt_pgs++] = (pte & ~PG_W) | PG_COW;
      /* mark start of region to protect if we're not already in one */
      if (!prot_pgs)
	prot_start = va;
      prot_pgs++;
    }
    va += NBPG;
  }

  WRITE_PROTECT;

  if (insrt_pgs) {
    SYSBATCH_ALLOC(batched_mappings, BATCH);
    int i;
    u32 dev;
    u32 blk;
    int ret;
    struct Xn_name *xn;
    struct Xn_name xn_nfs;

    sysbatch_init (batched_mappings, BATCH);
    for (i = 0; i < insrt_pgs; i++, insert_start += NBPG) {
      if (ptes[i] & PG_P) {
	u_int ppn = ptes[i] >> PGSHIFT;
	if (__ppages[ppn].pp_buf) {
	  if ((ret = sys_bc_ppn2buf (ppn, &dev, &blk)) < 0) {
	    sys_cputs ("fork: couldn't call sys_bc_ppn2buf\n");
	    kprintf ("ret = %d\n", ret);
	    goto fork_error;
	  }
	  if (dev >= MAX_DISKS) {
	    xn_nfs.xa_dev = dev;
	    xn_nfs.xa_name = 0;
	    xn = &xn_nfs;
	  } else {
	    xn = &__sysinfo.si_pxn[dev];
	  }
	  sys_batch_bc_buffer_map (batched_mappings, xn, CAP_ROOT, ptes[i], insert_start, CAP_ROOT, envid);
	  continue;
	} 
      }
      sys_batch_insert_pte (batched_mappings, CAP_ROOT, ptes[i], insert_start, CAP_ROOT, envid);
    }

    ret = sys_batch (batched_mappings);
    if (ret < 0) {
      sys_cputs ("fork: sys_batch failed\n");
      kprintf ("retval = %d\n", batched_mappings[sysbatch_error (&batched_mappings[0])].sb_syscall.retval);
      goto fork_error;
    }
  }
  
  /* copy and update our u-area for the child */
  cu = UAREA;
  cu.u_entprologue = (u_int) &&child;
  cu.u_entepilogue = (u_int) &&fork_epilogue;
#ifdef PROCESS_TABLE
  NewPid = AllocateFreePid (envid);
  AddProcEntry (&cu, argv[0], argv, NewPid, UAREA.pid);
  if ((cu.parent_slot = GetChildSlot (NewPid)) < 0) {
    sys_cputs ("@\n");
    goto fork_error;
  }
  cu.u_chld_state_chng = 0;
  ClearChildInfo (&cu);
#endif
#ifdef PROCD
  switch(forktype) {
  case ISFORK: NewPid = proc_fork(envid); break;
  case ISVFORK: NewPid = proc_vfork(envid); break;
  }
#endif
  if (sys_wru (0, envid, &cu) < 0) {
sys_cputs ("!\n");
    goto fork_error;
  }

  /* Copy our environment, set eip to address of child label */
  e = *__curenv;
  asm ("movl %%edi,%0\n"
       "\tmovl %%esi,%1\n"
       "\tmovl %%ebp,%2\n"
       "\tmovl %%esp,%3\n"
       "\tmovl %%ebx,%4\n"
       : "=m" (e.env_tf.tf_edi),
       "=m" (e.env_tf.tf_esi),
       "=m" (e.env_tf.tf_ebp),
       "=m" (e.env_tf.tf_esp),
       "=m" (e.env_tf.tf_ebx));
  e.env_tf.tf_eip = (u_int) &&child;
  if (sys_wrenv (k, envid, &e) < 0) {
    sys_cputs ("~\n");
    goto fork_error;
  }

  /* Let other modules do fork-specific work like updating ref counts etc */
  if (ExecuteOnForkHandlers(k,envid,NewPid) == -1) {
    sys_cputs ("|\n");
    goto fork_error;
  }
  /* get more reserved pages */
  __vm_alloc_region((u_int)__eea->eea_reserved_first,
		    __eea->eea_reserved_pages*NBPG, CAP_ROOT,
		    PG_P | PG_U | PG_W);
#if 0
  dump_pt (__envid);
  printf ("now dumping child...\n");
  dump_pt (envid);
  printf ("done dumping pts\n");
#endif

  /* start scheduling the new process */
  if (sys_quantum_alloc (k, -1, 0, envid) < 0) {
    sys_cputs ("=\n");
    goto fork_error;
  }

  STOP(misc,fork);
#ifdef PROCD
  if (forktype == ISVFORK) {
    proc_p childp;
    childp = efind(envid);
    //assert(childp);
	/* GROK -- I think childp can only be NULL if the child finishes */
	/* before the parent runs again.  This, combined with the fact   */
	/* that the assert above doesn't make sense with the previous    */
	/* version of this conditional made no sense, makes me believe   */
	/* that this version *does* make sense.  (he hopes)              */
    if ((childp == NULL) || (childp->p_pptr != curproc)) {
      //kprintf("DIFFERENT CHILDP childp: %p curproc: %p\n",childp,curproc);
      goto skip;
    }
    signals_off();
    for(;;) {
      int flag;
      // EXOS_LOCK
      EnterCritical();
      flag = childp->p_flag;
      // EXOS_UNLOCK
      ExitCritical();
      if ((flag & P_PPWAIT) == 0) {
	//kprintf("PPWAIT cleared\n");
	break;
      }
      wk_waitfor_value_neq(&childp->p_flag,flag, CAP_PROCD);
    }
    signals_on();
  skip:
  }
#endif
  return (NewPid);

  fork_error:
  ProcessFreeQuanta(envid);
  sys_env_free (0, envid);
  return (-1);

child:
  UAREA.u_entprologue = old_prologue;
  UAREA.u_entepilogue = old_epilogue;
  __envid = envid;
  if (! (__curenv = env_id2env (__envid, &r)))
    kprintf ("$$ env_init: could not get __curenv");
#ifdef PROCD
  curproc = efind(__envid);
#endif
  return (0);

  /* If a timer interrupt comes into the child process before it has
   * reset the prologue and epilogue, things could get ugly.
   * Therefore we stick this in front of the regular epilogue until
   * the prologue has been reset. */
fork_epilogue:
  UAREA.u_entprologue = old_prologue;
  UAREA.u_entepilogue = old_epilogue;
  asm volatile ("jmp %0" :: "r" (old_epilogue));
  return (0);  /* save a warning */
}

void fork_end () {}

pid_t	 
vfork (void) {
  pid_t r;
  OSCALLENTER(OSCALL_vfork);
  signals_off();
  r = fork1(ISVFORK);
  signals_on();
  OSCALLEXIT(OSCALL_vfork);
  return r;
}

pid_t	 
fork (void) {
  pid_t r;
  OSCALLENTER(OSCALL_fork);
  signals_off();
  r = fork1(ISFORK);
  signals_on();
  OSCALLEXIT(OSCALL_fork);
  return r;
}
