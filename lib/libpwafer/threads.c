
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <xok/sys_ucall.h>
#include <xok/scheduler.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/pmapP.h>
#include <machine/param.h>

#include <pwafer/pwafer.h>
#include <pwafer/kprintf.h>
#include <pwafer/locks.h>


static int smp_num_cpus = 0;

/* 
 * A temporary ExtProc structure that must be initialized so using
 * it won't cause a fault. we use this temporarily so fault handler
 * will work from the start.
 */
ExtProc eproc0_temp = {0,};	

/* 
 * Set of ready to use thread control blocks
 */
unsigned int total_threads = 0;
unsigned int next_thread_id = 0;
TCrBlk tcbs[MAX_NUM_THREADS];
#define tcb0 (&tcbs[0])

/* 
 * Set of ready to use eproc structures. We only create one eproc structure
 * per CPU.
 */
ExtProc eprocs[NR_CPUS];
spinlock_t eprocs_lock;

/* 
 * The per-cpu run queue 
 */
TRQ trqs[NR_CPUS];



/* 
 * create_tcb: create a new thread control block 
 */
static inline TCrBlk*
create_tcb(void (*start)())
{
  u_int start_tid, tid, new_tid;
  TCrBlk *tcb;

  start_tid = next_thread_id;
retry:
  tid = next_thread_id;
  new_tid = (tid+1) & (MAX_NUM_THREADS-1);
  
  if (compare_and_swap(&next_thread_id,tid,new_tid) != tid)
  {
    if (start_tid == new_tid) return NULL;
    goto retry;
  }

  if (tcbs[tid].t_status != T_FREE)
  {
    if (start_tid == new_tid) return NULL;
    goto retry;
  }

  atomic_inc(&total_threads);

  tcb = &tcbs[tid];
  tcb->t_id = tid;
  tcb->t_status = T_READY;
  tcb->t_eip = (u_int) start;
  
  return tcb;
}

/*
 * release_tcb: release a thread control block for recycling 
 */
static inline void
release_tcb(u_int tid)
{
  tcbs[tid].t_status = T_FREE;
  atomic_dec(&total_threads);
}


/*
 * release_eproc: release a eproc structure
 */
static inline void
release_eproc(unsigned int cpu)
{
  eprocs[cpu].envid = 0;
  trqs[cpu].head.t_status = T_FREE;
}

/*
 * get_eproc: get the eproc for the given cpu. If eproc does not exist, create
 * new environment (sys_env_clone).
 */
static inline ExtProc*
get_eproc(u_short cpu)
  /* not synchronzied */
{
  if (eprocs[cpu].envid == 0)
  {
    spinlock_acquire(&eprocs_lock);

    if (eprocs[cpu].envid == 0)
    {
      int r;
      struct Env *e;
      struct Env ue;
      struct Uenv new_uarea;
      ExtProc *ep;
      u_int envid;

      envid = sys_env_clone(CAP_USER, &r);
  
      if (!envid) 
      {
	spinlock_release(&eprocs_lock);
        return NULL;
      }

      e = &__envs[envidx(envid)];
      ep = &eprocs[cpu];
      ep->curenv = e;
      ep->tcb = 0;
      ep->stacktop = USTACKTOP - cpu*KT_STACKSIZE;
      ep->stackbot = USTACKTOP - cpu*KT_STACKSIZE - NBPG;
      ep->stackbotlim = USTACKTOP - cpu*KT_STACKSIZE - KT_STACKSIZE;
      ep->xstktop = (u_int) ep->xstk + sizeof(ep->xstk);
     
      /* copy and update uarea for child */
      new_uarea = UAREA;
      new_uarea.u_entprologue = (u_int) &&t_start;
      new_uarea.u_entepilogue = (u_int) &&t_epilogue;
      new_uarea.pid = envid;
      new_uarea.ext_proc = ep;
      new_uarea.u_xsp = ep->xstktop;
      /* don't run it just yet */
      new_uarea.u_status = U_SLEEP;
    
      /* write the uarea */
      if (sys_wru (CAP_USER, envid, &new_uarea) < 0)
      {
        sys_cputs ("sys_wru failed\n");
        sys_env_free(CAP_USER, envid);
        
	spinlock_release(&eprocs_lock);
        return NULL;
      }
      
      if (sys_insert_pte
	  (CAP_USER, PG_U|PG_W|PG_P, ep->stackbot, CAP_USER, envid) < 0)
      {
        sys_cputs("sys_insert_pte failed!\n");
        sys_env_free(CAP_USER, envid);

	spinlock_release(&eprocs_lock);
        return NULL;
      }
    
      ue = *e;
      ue.env_tf.tf_eip = (u_int) &&t_start;
      ue.env_tf.tf_esp = ep->stacktop;
  
      asm ("movl %%edi,%0\n" 
	   "\tmovl %%esi,%1\n" 
           "\tmovl %%ebp,%2\n" 
           "\tmovl %%ebx,%3\n" : 
           "=m" (ue.env_tf.tf_edi), 
           "=m" (ue.env_tf.tf_esi), 
           "=m" (ue.env_tf.tf_ebp), 
           "=m" (ue.env_tf.tf_ebx));

      if (sys_wrenv (CAP_USER, envid, &ue) < 0) 
      {
        sys_cputs("sys_wrenv failed!\n");
        sys_env_free(CAP_USER, envid);

	spinlock_release(&eprocs_lock);
        return NULL;
      }
      
      if ((r = sys_quantum_alloc (CAP_USER, -1, cpu, envid)) < 0)
      {
        sys_cputs("sys_quantum_alloc failed!\n");
        sys_env_free(CAP_USER, envid);

	spinlock_release(&eprocs_lock);
        return NULL;
      }
      
      /* do this last */
      eprocs[cpu].envid = envid;
    }

    spinlock_release(&eprocs_lock);
  }
  return &eprocs[cpu];

t_start:
  UAREA.u_entprologue = (u_int) xue_prologue;
  UAREA.u_entepilogue = (u_int) xue_epilogue;
  __tcb->t_status = T_RUN;
  asm volatile ("jmp %0" :: "r" (__tcb->t_eip));
  return 0;

t_epilogue:
  UAREA.u_entprologue = (u_int) xue_prologue;
  UAREA.u_entepilogue = (u_int) xue_epilogue;
  asm volatile ("jmp %0" :: "r" ((u_int) xue_epilogue));
  return 0;
}


/*
 * select_cpu: returns the cpu without an eproc structure or the cpu with
 * smallest runqueue. 
 */
static inline unsigned short
select_cpu()
{
  int i, lowest, lowest_cpu;
  
  for(i=0; i<smp_num_cpus && i<NR_CPUS; i++)
  {
    if (eprocs[i].envid == 0)
      return i;
  }

  lowest = trqs[0].size;
  lowest_cpu = 0;
  
  for(i=1; i<smp_num_cpus && i<NR_CPUS; i++)
  {
    if (trqs[i].size <= lowest)
    {
      lowest = trqs[i].size;
      lowest_cpu = i;
    }
  }

  return lowest_cpu;
}


/*
 * pw_init: create initial eproc and tcb structures for first environment
 * (thread 0)
 */
void 
pw_init()
{
  struct Env *ce = &__envs[envidx(sys_geteid())];
#ifdef __SMP__
  int cpu = sys_get_cpu_id();
#else
  int cpu = 0;
#endif
  int i;

  smp_num_cpus = sys_get_num_cpus();
  
  /* first need to set up temp eproc0 so fault handler will work */
  eproc0_temp.envid = sys_geteid();
  eproc0_temp.curenv = ce;
  eproc0_temp.tcb = 0;
  eproc0_temp.stacktop = USTACKTOP;
  eproc0_temp.stackbot = USTACKTOP-NBPG;
  eproc0_temp.xstktop = (u_int) eproc0_temp.xstk + sizeof(eproc0_temp.xstk);
  eproc0_temp.stackbotlim = USTACKTOP-KT_STACKSIZE;
  UAREA.ext_proc = &eproc0_temp;

  /* initialize global data */
  for(i = 0; i < smp_num_cpus; i++)
  {
    trqs[i].size = 1;
    trqs[i].head.t_status = T_FREE;
    trqs[i].head.t_next = 0;
    trqs[i].tail = &trqs[i].head;
    spinlock_reset(&trqs[i].trq_lock);
  }
  
  for(i = 0; i < MAX_NUM_THREADS; i++)
  {
    tcbs[i].t_status = T_FREE;
    tcbs[i].t_id = 0;
    tcbs[i].t_next = 0;
  }
  
  for(i = 0; i < NR_CPUS; i++)
  {
    eprocs[i].envid = 0;
  }
  spinlock_reset(&eprocs_lock);

  tcb0->t_id = 0;
  tcb0->t_status = T_RUN;
  tcb0->t_next = 0;

  eprocs[cpu].envid = sys_geteid();
  eprocs[cpu].curenv = ce;
  eprocs[cpu].tcb = tcb0;
  eprocs[cpu].stacktop = USTACKTOP;
  eprocs[cpu].stackbot = USTACKTOP-NBPG;
  eprocs[cpu].xstktop = (u_int) eprocs[cpu].xstk + sizeof(eprocs[cpu].xstk);
  eprocs[cpu].stackbotlim = USTACKTOP-KT_STACKSIZE;
  UAREA.ext_proc = &eprocs[cpu];

  next_thread_id = 1;
  total_threads = 1;

  trqs[cpu].size = 2;
  trqs[cpu].head.t_next = tcb0;
  trqs[cpu].tail = tcb0;
}


static inline void
start_cpu (int target_cpu)
{
  struct Uenv uarea;
  ExtProc *ep = get_eproc(target_cpu);
  TCrBlk *tcb = trqs[target_cpu].head.t_next;
  int r = 0;
 
  sys_cputs("start_cpu called\n");
  ep->tcb = tcb;
  trqs[target_cpu].head.t_status = T_TRQ_UP;

retry:
  sys_rdu(CAP_USER, ep->envid, &uarea);
  uarea.u_status = U_RUN;
  if ((r = sys_wru(CAP_USER, ep->envid, &uarea)) < 0)
  {
    kprintf("sys_wru failed, retry\n");
    goto retry;
  }
  return;
}


static int
wait4_ready(TRQ *trq)
{
  while(trq->size == 1)
  {
    if (total_threads == 0) return -1;
    yield(-1);
    asm volatile("" ::: "memory");
  }
  return trq->size;
}


static inline void
thread_wait4next()
{
#ifdef __SMP__
  int cpu = __curenv->env_cur_cpu;
#else
  int cpu = 0;
#endif
  TCrBlk *tcb;

  if (wait4_ready(&trqs[cpu]) < 0)
  {
    kprintf("on cpu %d: killing kt %d %d\n", cpu, __envid, sys_geteid());
    /* all threads are killed, we are done */
    release_eproc(cpu);
    __die();
  }

  else
  {
    tcb = trqs[cpu].head.t_next;
    __eproc->tcb = tcb;
    __tcb->t_status = T_RUN;

    asm volatile ("movl %0,%%esp" :: "r" (__eproc->stacktop));
    asm volatile ("jmp %0" :: "r" (__tcb->t_eip));
  }
}


/******************************
 * exported thread interfaces *
 ******************************/

int
pw_thread_create(void(*start)(), short target_cpu)
{
  ExtProc *ep;
  TCrBlk *tcb;
  TCrBlk *tail;
 
  if (target_cpu > smp_num_cpus-1) return -1;

  if (target_cpu < 0) 
    target_cpu = select_cpu();

  /* create tcb */
  tcb = create_tcb(start);
  if (tcb == NULL) return -1;
  tcb->t_next = 0;
  
  /* check if eproc exists, if not, create */
  ep = get_eproc(target_cpu);
  
  if (ep == NULL) 
  {
    release_tcb(tcb->t_id);
    return -1;
  }

  /* place on run queue */
  cpulock_acquire();
  spinlock_acquire(&trqs[target_cpu].trq_lock);

retry:
  tail = trqs[target_cpu].tail;

  if (compare_and_swap((int*)&(tail->t_next), 0, (u_int)tcb) == 0)
  {
    /* 
     * successfully place self as the next ptr after original tail, now try to
     * swing the tail to be self. don't worry if we fail, because the else
     * clause of this if will always adjust the tail pointer if it becomes
     * inconsistent 
     */
    compare_and_swap ((int*)&(trqs[target_cpu].tail), (u_int)tail, (u_int)tcb);
    atomic_inc(&(trqs[target_cpu].size));

    spinlock_release(&trqs[target_cpu].trq_lock);
  } 

  else
  {
    /* okay, tail pointer was inconsistent, swing it by one */
    compare_and_swap 
      ((int*)&(trqs[target_cpu].tail), (u_int) tail, (u_int) tail->t_next);
    goto retry;
  }

#ifdef __SMP__
  if (target_cpu != __curenv->env_cur_cpu && 
      trqs[target_cpu].head.t_status != T_TRQ_UP)
    start_cpu(target_cpu);
#endif

  cpulock_release();
  return tcb->t_id;
}


void
pw_thread_die()
{
#ifdef __SMP__
  int cpu = __curenv->env_cur_cpu;
#else
  int cpu = 0;
#endif
  TCrBlk *s = trqs[cpu].head.t_next;

  if (__tcb->t_id != (trqs[cpu].head.t_next)->t_id)
  {
    kprintf
      ("warning: top thread in run queue != current thread, skip dequeue\n");
    return;
  }

  cpulock_acquire();
  spinlock_acquire(&trqs[cpu].trq_lock);
  
  compare_and_swap((int*)&(trqs[cpu].tail), (u_int)s, (u_int)&(trqs[cpu].head));
  trqs[cpu].head.t_next = (trqs[cpu].head.t_next)->t_next;
  atomic_dec(&(trqs[cpu].size));
  
  spinlock_release(&trqs[cpu].trq_lock);
  cpulock_release();

  release_tcb(__tcb->t_id);
  __eproc->tcb = 0;

}


void
pw_thread_exit()
{
  pw_thread_die();
  thread_wait4next();
}


int
pw_thread_id()
{
  return __threadid;
}


void
__exit(void)
{
  pw_thread_exit();
}


