
/*
 * Copyright MIT 1999
 */

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <xok/scheduler.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/ipc.h>
#include <xok/pmap.h>
#include <xok/sysinfo.h>
#include <xok/batch.h>
#include <xok/sys_ucall.h>
#include <xok/sys_ubatch.h>

#include <vos/cap.h>
#include <vos/fd.h>
#include <vos/errno.h>
#include <vos/kprintf.h>
#include <vos/locks.h>
#include <vos/proc.h>
#include <vos/vm-layout.h>
#include <vos/vm.h>


vos_child_start_handler_t child_handlers[FORK_HANDLER_MAX];
u_int child_handler_index = 0;

vos_fork_handler_t fork_handlers[FORK_HANDLER_MAX];
u_int fork_handler_index = 0;


/* make fork-style copies of current env's page table for child environment
 * using COW. 
 */

static int
fork_copy_pt (u_int k, u_int new_envid)
{
  u_int va;
  Pte pte;
#define NBATCHES 128
  Pte ptes[NBATCHES];
  int insrt_pgs = 0;
  u_int insert_start;

  /* we copy everything until UTOP */
  for (insert_start = va = NBPG ; va < UTOP ; ) 
  {
    /* for each va, depends on what pte it has (present, shared, writable,
     * non-writable, etc), it may need to be changed to COW in both parent
     * and child pt and/or simply map into child pt. if it needs to be
     * inserted into child pt or changed in parent pt, we collect them and
     * insert them in a batch for efficiency purposes.
     */

    /* CAUTION - we must not set the COW bits on parent PTEs before we insert
     * PTEs for the child process. COW fault handler changes a PTE from PG_COW
     * to PG_W if the page has only one reference count. Therefore, if we set
     * a parent PTE to PG_COW first, then insert the PTE for child for the
     * same page later (batched), there is a window where the page should stay
     * as COW but its ref count is one.
     */

    if ((va - insert_start)/NBPG >= NBATCHES) 
      /* we do pte insertion via batching so it will be fast. we do NBATCHES
       * number of insertions at a time */
    {
insert_collected_ptes:
      if (insrt_pgs)  /* if there are pages to insert */
      {
	SYSBATCH_ALLOC(child_mappings, NBATCHES);
	SYSBATCH_ALLOC(parent_mappings, NBATCHES);
	int i;
	u_int insert_start_keep;
	u32 dev;
	u32 blk;		
	int ret;
	struct Xn_name *xn;
	struct Xn_name xn_nfs;

	sysbatch_init (child_mappings, NBATCHES);
	sysbatch_init (parent_mappings, NBATCHES);

	insert_start_keep = insert_start;
	for (i = 0; i < insrt_pgs; i++, insert_start += NBPG) 
	{
 	  if ((ptes[i] & (PG_P|PG_COW)) == (PG_COW|PG_P)) 
	    /* set parent PTE COW bit */
	    sys_batch_self_mod_pte_range 
	      (parent_mappings, k, PG_COW, PG_W, insert_start, 1);

	  if (ptes[i] & PG_P) 
	    /* for each pte that is present */
	  {   
	    u_int ppn = ptes[i] >> PGSHIFT;

	    if (__ppages[ppn].pp_buf) /* page is a buffer cache */
	    {
	      if ((ret = sys_bc_ppn2buf (ppn, &dev, &blk)) < 0) 
	      {
		sys_cputs ("fork: couldn't call sys_bc_ppn2buf\n");
		return -1;
	      }

	      if (dev >= MAX_DISKS) 
	      {
		xn_nfs.xa_dev = dev;
		xn_nfs.xa_name = 0;
		xn = &xn_nfs;
	      } 
	      else 
	      {
		xn = &__sysinfo.si_pxn[dev];
	      }
	     
	      /* insert a batched syscall for buffer pages */
	      sys_batch_bc_buffer_map 
	        (child_mappings, xn, k, ptes[i], insert_start, k, new_envid);
	      continue;
	    } 
	  }
	 
	  /* insert a batched syscall for normal pages */
	  sys_batch_insert_pte 
	    (child_mappings, k, ptes[i], insert_start, k, new_envid);
	}

	/* make the batch request for child */
	ret = sys_batch (child_mappings);
	if (ret < 0) 
	{
	  sys_cputs ("fork: sys_batch child mappings failed\n");
	  return -1;
	}

	/* AFTER the batch request for child, do so for parent */
	ret = sys_batch (parent_mappings);
	if (ret < 0)
	{
	  sys_cputs("fork: sys_batch parent mappings failed\n");
	  return -1;
	}

	insrt_pgs = 0;
      }
      
      insert_start = va;
      if (va >= UTOP)  /* this was the last set of insertions */
        break;
    }

    /* pte insertion done, now we see if we can batch some more insertions */

    if ((vpd[PDENO (va)] & (PG_P|PG_U)) != (PG_P|PG_U)) 
      /* pde not present: skip rest of the pg directory */
    {  
      va = (va + NBPD) & ~PDMASK;
      continue;
    }

    /* get page table entry for this va */
    pte = vpt[va >> PGSHIFT];

    if (!(pte & PG_P) || pte & PG_SHARED) 
      /* page not present or shared, so no need to change to COW */
      ptes[insrt_pgs++] = pte;
    
    else if (va >= __stkbot && va < USTACKTOP) 
    {
      /* va is USTACKTOP-NBPG, that is, the exception page on stack */

      if (vm_self_insert_pte (k, PG_P|PG_U|PG_W, FORK_TEMP_PG, 0, NULL) < 0) 
      {
	sys_cputs ("fork: vm_self_insert_pte failed\n");
	return -1;
      }
      bcopy ((char *)va, (char *)FORK_TEMP_PG, NBPG);

      if (vm_insert_pte (k, (vpt[FORK_TEMP_PG >> PGSHIFT] & ~PGMASK) |
			 (pte & PGMASK), va, 0, new_envid, 0, NULL) < 0) 
      {
	sys_cputs ("fork: vm_insert_pte failed\n");
	return -1;
      }

      /* insert new exception stack page into child environment */
      ptes[insrt_pgs++]=(vpt[FORK_TEMP_PG >> PGSHIFT] & ~PGMASK)|(pte & PGMASK);

      if (vm_self_unmap_page (k, FORK_TEMP_PG) < 0) 
      {
	sys_cputs ("fork: vm_self_unmap_page failed\n");
	return -1;
      }
    }
    
    else if (!(pte & PG_W))  
      /* not writeable pages: RO or already COWed */
      ptes[insrt_pgs++] = pte;
    
    else 
      /* normal writeable page - only turn a page to COW if it is writable */
      ptes[insrt_pgs++] = (pte & ~PG_W) | PG_COW;
    
    va += NBPG;

    if (va >= UTOP) goto insert_collected_ptes;
  }
  return 0;
}


static void
do_before_fork_handlers(u_int new_envid)
{
  int i;

  for(i=0; i < fork_handler_index; i++)
  {
    if (fork_handlers[i] != 0L) 
      (*(fork_handlers[i]))(new_envid);
  }
}


static void
do_child_start_handlers()
{
  int i;

  for(i=0; i < child_handler_index; i++)
  {
    if (child_handlers[i] != 0L) 
      (*(child_handlers[i]))();
  }
}


/* implements fork in the following steps:
 *   1. get a new env
 *   2. copy pt (see fork_copy_pt), uses COW
 *   3. copy parent uarea and update it, set as child uarea 
 *   4. copy parent env and update it, set as child env. initial prologue
 *      code for child sets up child process structure and other UAREA fields,
 *      then return 0 to instruction after the call to fork.
 *   5. schedule child 
 *   6. parent returns child pid
 *
 * fork.txt gives a more in-depth description. If you change anything here,
 * you should probably update that document as well.
 */
static pid_t 
fork0 (u_int target_cpu, int quantum) 
{
  int r;
  u_int new_envid;
  struct Env new_e;
  struct Uenv new_uarea;
  extern int vos_fpu_used_ctxt;

  volatile u_int old_prologue = UAREA.u_entprologue;
  volatile u_int old_epilogue = UAREA.u_entepilogue;

  /* get a new environment to work with */
  if (!(new_envid = sys_env_alloc (CAP_USER, &r))) 
  {
    r = V_NOFORK;
    return r;
  }

  flush_io();
  do_before_fork_handlers(new_envid);
  
  
  /* save fpu state: we must do this before we copy pagetable since
   * the states are saved in memory, so after copy_pt any write would
   * just trigger COW */
 
  cpulock_acquire();
  if (vos_fpu_used_ctxt == 1)
  {
    asm volatile ("fnsave _vos_fpus\n");
    asm volatile ("fwait\n");
    asm volatile ("frstor _vos_fpus\n");
  }
  cpulock_release();


  /* copy all page table entries */
  if (fork_copy_pt(CAP_USER,new_envid) < 0) 
  {
    r = V_NOFORK;
    goto fork0_err;
  }
  
  /* copy and update uarea for child */
  new_uarea = UAREA;
  new_uarea.u_entprologue = (u_int) &&child;
  new_uarea.u_entepilogue = (u_int) &&fork_epilogue;
  new_uarea.pid = new_envid;
  new_uarea.parent = getpid();
  new_uarea.ext_proc = 0L;

  /* write the uarea */
  if (sys_wru (CAP_USER, new_envid, &new_uarea) < 0) 
  {
    sys_cputs ("fork: sys_wru failed\n");
    r = V_NOFORK;
    goto fork0_err;
  }

  /* copy our environment, set eip to address of child label */
  new_e = *__curenv;
  asm ("movl %%edi,%0\n"
       "\tmovl %%esi,%1\n"
       "\tmovl %%ebp,%2\n"
       "\tmovl %%esp,%3\n"
       "\tmovl %%ebx,%4\n"
       : "=m" (new_e.env_tf.tf_edi),
       "=m" (new_e.env_tf.tf_esi),
       "=m" (new_e.env_tf.tf_ebp),
       "=m" (new_e.env_tf.tf_esp),
       "=m" (new_e.env_tf.tf_ebx));
  new_e.env_tf.tf_eip = (u_int) &&child;
 
  /* write child env */
  if (sys_wrenv (CAP_USER, new_envid, &new_e) < 0) 
  {
    sys_cputs ("fork: sys_wrenv failed\n");
    r = V_NOFORK;
    goto fork0_err;
  }
  
  /* schedule the environment: we can't do this until all the child state is
   * setup */
  if ((r = sys_quantum_alloc (CAP_USER, quantum, target_cpu, new_envid)) < 0) 
  {
    /* try again with any quantum */
    if ((r = sys_quantum_alloc (CAP_USER, -1, target_cpu, new_envid)) < 0) 
    {
      r = V_NOFORK;
      goto fork0_err;
    }
  }

  /* parent: return child id */
  return new_envid;
  
fork0_err:
  sys_env_free(CAP_USER,new_envid);
  RETERR(r);

child: /* child process starts to execute here */
  UAREA.pid = sys_geteid();
  UAREA.u_entprologue = old_prologue;
  UAREA.u_entepilogue = old_epilogue;

  /* process specific setup */
  {
    extern void __proc_child_start(void);
    do_child_start_handlers();
    __proc_child_start();
  }

  /* return 0 to children after call of fork */
  return 0;

  /* if a timer interrupt comes into the child process before it has
   * reset the prologue and epilogue, things could get ugly.
   * Therefore we stick this in front of the regular epilogue until
   * the prologue has been reset. */
fork_epilogue:
  UAREA.pid = sys_geteid();
  UAREA.u_entprologue = old_prologue;
  UAREA.u_entepilogue = old_epilogue;
  asm volatile ("jmp %0" :: "r" (old_epilogue));
  return 0;    /* to avoid compiler warning */
}



pid_t 
fork()
{
  return fork0(sys_get_cpu_id(), -1);
}


pid_t
fork_to_cpu(u_int cpu, int quantum)
{
  if (quantum >= NQUANTUM)
    RETERR(V_INVALID);
  if (quantum < 0)
    quantum = -1;
  return fork0(cpu, quantum);
}


