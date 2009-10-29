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

#define __SCHED_MODULE__


#ifdef __SMP__
#include <xok/apic.h>
#endif
#include <xok/mplock.h>
#include <xok/sysinfo.h>
#include <xok/defs.h>
#include <xok/env.h>
#include <xok/sys_proto.h>
#include <xok/syscallno.h>
#include <xok_include/assert.h>
#include <xok/scheduler.h>
#include <xok/cpu.h>
#include <xok/kerrno.h>
#include <xok/malloc.h>
#include <xok/printf.h>



#ifdef OSKIT_PRESENT
/*
 * need a definiton the bump_jiffies function in
 * the OS-kit.  bump_jiffies is some left over Linux crud.
 */
extern void bump_jiffies();
#endif /* OSKIT_PRESENT */


int should_reschedule = 0;



/* simple (kernel-only) support for in-kernel execution of kernel functions
 * some number of ticks in the future.  This is originally being added to
 * support ported device drivers, but may be otherwise useful...            
 */


typedef struct to_struct
{
  unsigned long long to_ticks;
  void (*to_func) (void *);
  void *to_arg;
  struct to_struct *to_next;
}
to_struct_t;

to_struct_t *to_list[NR_CPUS] = {NULL, };



void
sched_init (void) 
{
  int j, i;

  for (j = 0; j < get_cpu_count(); j++)
    {
      SYSINFO_A_ASSIGN(si_percpu_ticks,j,0);
      SYSINFO_A_ASSIGN(si_percpu_idle_ticks,j,0);

      LIST_INIT (&(__cpucxts[j]->_qfreelist));

      for (i = 0; i < NQUANTUM; i++) {
        __cpucxts[j]->_qvec[i].q_no = i;
        __cpucxts[j]->_qvec[i].q_used = 0;
        
	LIST_INSERT_HEAD 
          (&(__cpucxts[j]->_qfreelist), 
	   &(__cpucxts[j]->_qvec[i]), q_link);
      }

      /* current quantum must start at 1 since the scheduling loop that uses
       * this will skip the idle process and run each process once before
       * giveing out new ticks to each quantum. */

      __cpucxts[j]->_current_q = 1;
      __cpucxts[j]->_in_revocation = 0;

      to_list[j] = NULL;
    }
}




static inline int
quantum_unfree (struct Quantum *qp) 
  __XOK_REQ_SYNC(on qp; qplist)
{
  if (!qp)
    printf ("AHYH1\n");
  if (qp->q_used)
    printf ("AHYH2 %d %d\n", qp->q_no, qp->q_envid);
  
  if (!qp || qp->q_used) 
    return (-1);

  LIST_REMOVE (qp, q_link);
  qp->q_used = 1;
  return (qp->q_no);
}




static inline void 
quantum_free (struct Quantum *qp, int cpu) 
  __XOK_REQ_SYNC(on qp; qplist)
{
  if (qp->q_used)
  {
    qp->q_used = 0;
    qp->q_envid = 0;

    /* set this to zero, which forces whatever that was running on this
     * quantum to be rescheduled soon */
    qp->q_ticks = 0;

    /* 
     * must use __cpucxt here so that pointer with the REAL address of the
     * quantum, not the mapped address, gets put into the list 
     */
    LIST_INSERT_HEAD
      (&(__cpucxts[cpu]->_qfreelist),
       &(__cpucxts[cpu]->_qvec[qp->q_no]), q_link);
  }
}




/* sys_quantum_alloc: Allocates a new quantum (taking any if <q> ==
 * -1, otherwise selecting quantum #<q>).  Protect it with capability
 * #<k>, and schedule environment <envid> to run durring that quantum
 * (or don't schedule anyone to run if <envid> == 0).  Returns quantum
 * number of -1 on failure.
 *
 * sys_quantum_set: Schedules environment <envid> to run during
 * quantum #<q>, using capability #<k> to access the quantum.
 *
 * sys_quantum_free: Frees quantum #<q> which must be accessible
 * through capability #<k>.
 *
 * These procedures modify quantum queue, therefore they use spinlocks to
 * protected modifications.
 */

DEF_ALIAS_FN (sys_quantum_alloc, sys_quantum_set);
DEF_ALIAS_FN (sys_quantum_free, sys_quantum_set);


int
sys_quantum_set (u_int sn, u_int k, int q, u_int cpu, int envid) 
  __XOK_SYNC(locks qlist)
{
  struct Quantum *qp;
  cap c;
  struct Env *e = NULL;
  int r;

  /* printf ("q req: env id %d, quantum %d, cpu %d.\n",envid,q,cpu); */

#ifdef __SMP__
  if (cpu_count2id[cpu] == -1) 
  {
    printf("WARNING: invalid CPU referenced.\n");
    return (-E_CPU_INVALID);
  }
#endif

 
  if (sn != SYS_quantum_free && envid)
  {
    if (! (e = env_id2env (envid, &r))) return r;
  }
  
  /* Restricted capabilities not accepted */
  if ((r = env_getcap (curenv, k, &c)) < 0) return r;
  if (c.c_perm != CL_ALL) return (-E_CAP_INSUFF);
 
  MP_SPINLOCK_GET(&qvec_lock(cpu));

  /* Allocate a new quantum */
  if (sn == SYS_quantum_alloc)
  {
    if (q == -1)
      qp = (__cpucxts[cpu]->_qfreelist).lh_first;
    else if ((unsigned) q < NQUANTUM)
      qp = &(__cpucxts[cpu]->_qvec)[q];
    
    else 
    {
      MP_SPINLOCK_RELEASE(&qvec_lock(cpu));
      return (-E_INVAL);	/* Bogus quantum number argument */
    }
    
    q = quantum_unfree (qp);
    if (q < 0) 
    {
      MP_SPINLOCK_RELEASE(&qvec_lock(cpu));
      return (-E_INVAL);
    }
    
    qp->q_cap = c;
    qp->q_ticks = BASE_TICKS;
    /* printf ("q alloc: env id %d, quantum %d, cpu %d.\n",envid,q,cpu); */
  }

  /* Check access to existing quantum */
  else
  {
    if ((unsigned) q >= NQUANTUM) 
    {
      MP_SPINLOCK_RELEASE(&qvec_lock(cpu));
      return (-E_INVAL); 
    }
    
    qp = &(__cpucxts[cpu]->_qvec)[q];
      
    /* quantum not even allocated */
    if (qp->q_used != 1) 
    {  
      MP_SPINLOCK_RELEASE(&qvec_lock(cpu));
      return (-E_INVAL);
    }

    if ((r = acl_access (&c, &qp->q_cap, 1, ACL_ALL)) < 0) 
    {
      MP_SPINLOCK_RELEASE(&qvec_lock(cpu));
      return (r);
    }
  }

  if (sn == SYS_quantum_free)
  {
    int index = q >> 3;
    int offset = q & 0x7;
    int r; 
    struct Env *e = env_id2env (qp->q_envid, &r);
   
    /* free quantum, remove from quantum map */
    e->env_quanta[cpu][index] &= ~(1 << offset);
    quantum_free (qp, cpu);
  }

  else
  {
    int index = q >> 3;
    int offset = q & 0x7;

    if (sn == SYS_quantum_alloc)
    {
      /* allocating a new quantum, so set the quanta map */
      e->env_quanta[cpu][index] |= (1 << offset);
    }
    
    else 
    {
      int r; /* XXX - return errpr code */
      struct Env *olde = env_id2env (qp->q_envid, &r);
	
      /* transfer of control, modify quanta map of old owner
       * and update the map of the new owner */
      olde->env_quanta[cpu][index] &= ~(1 << offset);
      e->env_quanta[cpu][index] |= (1 << offset);
    }

    qp->q_envid = e->env_id;
  }
 
  MP_SPINLOCK_RELEASE(&qvec_lock(cpu));
  return (q);
}



int
sys_quantum_get (u_int sn, u_int cpu) 
  __XOK_NOSYNC
{
#ifdef __SMP__
  if (cpu_count2id[cpu] == -1) 
    return (-E_CPU_INVALID);
#endif
  return (__cpucxts[cpu]->_current_q);
}



void
sys_quanta_dump(u_int sn)
  __XOK_NOSYNC
{
  int i,j, indent=0;
  for(i=0; i<get_cpu_count(); i++)
  {
    printf("cpu %d:\n",i);
    indent = 0;
    for(j=0; j< NQUANTUM; j++)
    {
      if (__cpucxts[i]->_qvec[j].q_used == 1)
      {
	printf("q %3d: e %7d; ",j, __cpucxts[i]->_qvec[j].q_envid);
	indent++;
	if (indent==4)
	{
	  indent=0;
	  printf("\n");
	}
      }
    }
  }
}




/* give every quantum a new batch of ticks */

static inline void 
more_ticks () 
  __XOK_NOSYNC
{
  /* SMP synchronization: we only touch quantums on the local CPU quantum
   * vector. Only possible race condition comes from sys_quantum_alloc or
   * sys_quantum_free calls, which reset q_ticks on allocation and
   * deallocation. 
   */
  
  int i;
  for (i = 0; i < NQUANTUM; i++)
  {
    if (quantums[i].q_used)
    {
      /* if this quantum is freed on another CPU after the above check, we
       * give it some ticks that will never be used, big deal.
       *
       * if this quantum is allocated and we missed the above check,
       * sys_quantum_alloc will give it BASE_TICKS.
       *
       * if this quantum is allocated and we gave it another BASE_TICKS, oh
       * well: this could happen if more_ticks is called after quantum_alloc
       * anyways.
       */
      quantums[i].q_ticks += BASE_TICKS;
      if (quantums[i].q_ticks > TICKS_MASK)
        quantums[i].q_ticks = TICKS_MASK;
    }
  }
}




DEF_ALIAS_FN (yield, sched_runnext);
void 
sched_runnext (void) 
  __XOK_SYNC(may localize each e on run queue)
             

{
  int ret, cycled_thru_qlist = 0;
  int envid = -1;
  struct Env *e;
  int icq;
  int cpu = cpu_id;

  icq = curq;
  in_revocation = 0;

  if (cpu != get_cpu_id())
  {
    printf("oops, bad CPUCXT mapping on %d (reported %d)\n", get_cpu_id(), cpu);
    assert(0);
  }

  if (curenv) 
  {
    if (curenv->env_cur_cpu != cpu)
    {
      printf("%d: curenv %p %d, env_cur_cpu %d\n",get_cpu_id(), curenv, curenv->env_id,curenv->env_cur_cpu);
#ifdef __SMP__
      assert(curenv->env_cur_cpu == cpu);
#endif
    }
  }

  /* handle internal yields as a result of disk completions or forced
   * rescheduling. We fake this by making it look like the current process is
   * yielding to the guy we want to reschedule to. */

  /* should_reschedule set in bc.c and ipc.c */

  if (should_reschedule)
    {
      if (curenv)
	{

	  /* XXX -- what happens if this process was really trying to yield
	     to someone and over write it here??? */
	  UAREA.u_donate = should_reschedule;
	}
      should_reschedule = 0;
    }

  /* now we try to do some mop up work on curenv */
  if (curenv) 
  {
    env_save_tf();
    envid = UAREA.u_donate;
#if 1
    /* XXX -- no idea why I need this here, but without it we get
       stuck in an endless idle loop when doing the i/o scheduling. */
    UAREA.u_donate = -1;
#endif

#ifdef __SMP__
    /* by setting curenv->env_cur_cpu to -1, other CPU can now run this
     * environment (they also can modify env_tf now, we after this point, we
     * should no longer write to curenv->env_tf) */
    env_force_release(curenv);
#endif
  }

  /* handle directed yields. Env that is yielding fills in
     u_donate with the envid of the proc that should receive the
     process next. */

  if (curenv && envid != -1)
    {
      e = env_id2env (envid, &ret);
      if (e) 
	goto yield_cont;
    }

  /* don't need locks on curq since noone else changes this */
  curq = (curq + 1) & QUANTUM_MASK;

  if (!curq)
    {
      more_ticks ();
      curq = 1;
    }

#ifdef __SMP__
  /* check IPI: in case we lost some IPIs! */
  ipi_handler();
#endif

  for (;;)
    {
      cycled_thru_qlist = 1;

      /* We need to examine quantum vector and we should do this atomically.
       * We don't care, however, if an env is schedule for this round but we
       * missed it just barely, or if a different env is scheduled here but we
       * just barely missed the change. Also, if an environment is being
       * killed, we will see that reflected in env_id2env or env_status later.
       * All we really want is an environment ID. Therefore, we don't really
       * lock quantum vector at all, we just grab the id and validate it.
       * This saves quite some cycles.
       */

      if (quantums[curq].q_used && quantums[curq].q_envid &&
	  quantums[curq].q_ticks)
	{
	  envid = quantums[curq].q_envid;
	  e = env_id2env (envid, &ret);

	yield_cont:
	  if (e && e->env_u)
	    {
#ifdef __SMP__
	      if (e->env_status == ENV_OK && e->env_id == envid && 
		  env_localize(e)==0)
#else
	      if (e->env_status == ENV_OK && e->env_id == envid)
#endif
		{
		  if (e->env_u->u_status == U_SLEEP)
		    {
		      if (e->env_pred)
			{
			  if ((ret = e->env_pred ()))
			    {
			      e->env_u->u_pred_tag = ret;
			      e->env_u->u_status++;
			    }
			}
		    }
		  if (e->env_u->u_status > 0) 
		    env_run (e);
#ifdef __SMP__
		  else
		    env_release(e);
#endif
		}
	    }
	  else if (!e)
	    {
	      MP_SPINLOCK_GET(&qvec_lock(cpu));
	      /* the environment is a bad one, remove it */
	      printf ("freeing garbage environment at quantum %d.\n", curq);
	      quantum_free (&quantums[curq], cpu);
	      /* time for next quantum, release lock */
              MP_SPINLOCK_RELEASE(&qvec_lock(cpu));

	    }
	} 

      /* if the quantum number hasn't changed and we were not yielding,
       * that means we are idling... thus, we better run idle process 
       * so we can get some interrupts...
       */
      if (curq == icq && cycled_thru_qlist)
	{
#ifdef __SMP__
	  if (env_localize(env0) != 0)
	  {
	    printf("cpu %d: trying to run env0 %d, but %d has it?\n",
		   cpu, env0->env_id, env0->env_cur_cpu);
	  } else
#endif
	  env_run (env0); 	/* idle */
	}

      curq = (curq + 1) & QUANTUM_MASK;
      if (!curq)
	{
	  more_ticks ();
	  curq = 1;
	}
    }
}



void
yields (void) 
  __XOK_SYNC(calls sched_runnext)
{
#ifdef ASH_ENABLE
  if (ash_va)
    {
      printf ("yields: Should not be invoked by ash.\n");
      curenv->env_status = ENV_DEAD;
      ash_return ();
    }
  else
    {
#endif
      /* before running sched_runnext, so curenv is still localized */
      if (--UAREA.u_status < 0)
	UAREA.u_status = U_SLEEP;
      sched_runnext ();
#ifdef ASH_ENABLE
    }
#endif
}


int 
timeout (void (*func) (void *), void *arg, u_int ticks) 
  __XOK_NOSYNC
{
  to_struct_t *to_new = (to_struct_t *) malloc (sizeof (to_struct_t));
  if (!to_new)
    {
      warn ("timeout: could not malloc to_struct_t");
      return -1;
    }
  to_new->to_ticks = SYSINFO_GET(si_system_ticks) + ticks;
  to_new->to_func = func;
  to_new->to_arg = arg;

  if ((to_list[cpu_id] == NULL) || 
      (to_list[cpu_id]->to_ticks >= to_new->to_ticks))
    {
      to_new->to_next = to_list[cpu_id];
      to_list[cpu_id] = to_new;
    }
  else
    {
      to_struct_t *tmp = to_list[cpu_id];
      while ((tmp->to_next) && (tmp->to_next->to_ticks < to_new->to_ticks))
	{
	  tmp = tmp->to_next;
	}
      to_new->to_next = tmp->to_next;
      tmp->to_next = to_new;
    }
  
  return 0;
}


void
sched_intr (int trapno) 
  __XOK_SYNC(calls revoke_process; calls env_free; calls sched_runnext)
{
  int cpu = cpu_id;
  
  irq_eoi (0);

  /* no need to lock this... always sync updates */
  INC_FIELD_AT(si,Sysinfo,si_percpu_ticks,cpu,1); 
  if (curenv == env0)
  {
    INC_FIELD_AT(si,Sysinfo,si_percpu_idle_ticks,cpu,1); 
  }

#ifdef OSKIT_PRESENT
  /*
   * mseu, I'm adding this to support the OS-kit's
   * bump_jiffies.  bump_jiffies increments the jiffies
   * variable, which servers the same purpose for the linux
   * device drivers that si_ticks does for the exokernel.
   */

  bump_jiffies();
#endif /* OSKIT_PRESENT */

  /* 
   * Got to becareful not to use the env registered for the quantum, q->env,
   * but curenv. q->env may be a new env as a result of sys_quantum_set, or
   * q->env might have yielded to curenv.
   */
  curenv->env_ticks++;

  /* fire any ready-to-go "timeout" functions */
  
  while ((to_list[cpu]) && 
         (to_list[cpu]->to_ticks <= SYSINFO_GET(si_system_ticks)))
  {
    to_struct_t *to_done = to_list[cpu];
    to_list[cpu] = to_list[cpu]->to_next;
    to_done->to_func (to_done->to_arg);
    free (to_done);
  }
  
  if (in_revocation)
  {
    if (curenv->env_qx == MAX_QX)
    {
      printf ("env %d time exceeded\n", curenv->env_id);
      printf ("eip where timed out %x\n", utf->tf_eip);
      printf ("first enter critical eip: %x\n",UAREA.u_critical_eip);
      printf ("nested level: %d\n", UAREA.u_in_critical);
      in_revocation = 0;

      env_free (curenv);
      curenv = NULL;
      sched_runnext ();
    }
    else
    {
      curenv->env_qx++;
    }
  }

  else
  {
    /* no need to lock quantum vector because the only possible race condition
     * would come from sys_quantum_alloc/free, which changes q_ticks. If
     * q_ticks is reset to BASE_TICKS, we will just run a few more cycles. If
     * q_ticks is changed to 0, we expire, this time or next time around.
     */
    --quantums[curq].q_ticks;
    if (quantums[curq].q_ticks <= 0)
    {
      /* if we set q_ticks to 0 after another CPU already allocated a new env,
       * not a big deal: the next time more_ticks is called, q_ticks will incr
       * to BASE_TICKS and the new env will run.
       */
      quantums[curq].q_ticks = 0;
      revoke_processor ();
    }
  }
}


void 
revoke_processor ()
  __XOK_SYNC(calls sched_runnext)
{
  UAREA.u_ppc = utf->tf_eip;
#ifdef __HOST__
  UAREA.u_cs = utf->tf_cs;
  UAREA.u_ss = utf->tf_ss;
  UAREA.u_ds = utf->tf_ds;
  UAREA.u_es = utf->tf_es;
  UAREA.u_eflags = utf->tf_eflags;
#endif
  if (UAREA.u_entepilogue)
    {
      in_revocation = 1;
      curenv->env_qx = 0;
      utf->tf_eip = UAREA.u_entepilogue;
#ifdef __HOST__
      utf->tf_cs = GD_UT|3;
      utf->tf_ss = GD_UD|3;
      utf->tf_ds = GD_UD|3;
      utf->tf_es = GD_UD|3;
      utf->tf_eflags &= ~(1<<17);  /* clear VM86 bit */
#endif
    }
  else
    {
      sched_runnext ();
    }
}


#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

