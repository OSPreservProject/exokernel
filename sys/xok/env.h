
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

#ifndef _XOK_ENV_H_
#define _XOK_ENV_H_

#define LOG2NENV 10
#define NENV (1<<LOG2NENV)
#define envidx(envid) ((envid) & (NENV - 1))

/* Values of u_status in struct Uenv */
#define U_SLEEP 0
#define U_RUN 1

/* Values of env_status in struct Env */
#define ENV_FREE 0
#define ENV_OK 1
#define ENV_DEAD 2
#define ENV_QXX 3

#ifndef __ASSEMBLER__

#include <machine/cpufunc.h>
#include <xok/scheduler.h>
#include <xok/cpu.h>
#include <xok/defs.h>
#include <xok/queue.h>
#include <xok/capability.h>
#include <xok/trap.h>
#include <xok/mmu.h> 
#include <xok_include/assert.h>
#include <xok/kerrno.h> 
#include <xok/mplock.h> 
#include <xok/ipc.h> 



/* User-writeable portion of the Env structure */
struct Uenv 
{
  /* 
   * keep these 8 fields together so they all fall on the same
   * cache line. They're used in sequence by the yield code 
   */
  int u_status;                 /* process is runnable if > 0 */
  int u_donate; 		/* env to donate to on yield */
  u_int u_ppc;                  /* eip at timer interrupt */
  u_int u_in_critical;		/* yield at end of timeslice or not */
  u_int u_interrupted;		/* kernel tried to take our timeslice */
  u_int u_critical_eip;         /* eip when entering critical region */
  u_int u_in_pfault;		/* in a page fault or not*/
  u_int u_pred_tag;		/* tag of pred that matched */

  u_int u_entepilogue;          /* Epilogue entry point */
  u_int u_entprologue;          /* Prologue entry point */
  u_int u_entfault;             /* Page fault entry */
  u_int u_entipc1;              /* an IPC entry point */
  u_int u_entipc2;              /* another IPC entry point */
  u_int u_entipca;              /* ASH IPC entry point */
  u_int u_entrtc;               /* Runtime clock entry */
 
  u_int  u_fpu;			/* using the fpu or not */
  u_char u_pendrtc;             /* Runtime clock pending flag */
  u_int  u_xsp;                 /* Exception stack (0 = current stack) */

  u_int u_ashsp;                /* Stack pointer for ash execution */
  u_int u_ashnet;               /* Network ash entry point */
  u_int u_ashdisk;              /* Disk ash entry point */
  
  u_int u_aenvid;		/* grant capability gcap, dominated by gkey, */
  int u_gkey;			/* to envirnment aenvid */
  cap u_gcap;
  
#define U_MAX_EXTRAIPCARGS 	16
  u_int u_ipc1_extra_args[U_MAX_EXTRAIPCARGS];     /* extra pct arguments */

  u_int u_revoked_pages;	/* number of pages kernel is asking for back */
  u_int u_rtc_rega;             /* user controlled profiling clock */
  u_int u_ss_pfaults;           /* set to keep TF on after a page fault */
  u_int u_start_kern_call_counting; /* when to start kern call counting */

  /* 
   * standard process abstraction stuff: most library OS will likely to use
   * these fields  
   */
  u_int pid;			/* process id */
  u_int parent;			/* pid of parent */
#define U_NAMEMAX 255	 	/* should really be PATHNAME */
  u_char name[U_NAMEMAX];	/* name we were invoked under */
  struct ExtProc* ext_proc;     /* extended proc abstraction */



  /***************************************************
   * Below are stuff that probably shouldn't be here * 
   ***************************************************/

#if 0
  /* PAM stuff: supposely sys_prot_call depends on order of these fields */
  u_int prot_entepilogue;
  u_int prot_entprologue;
  u_int prot_entfault;
  u_int prot_entipc1;
  u_int prot_entipc2;
  u_int prot_entipca;
  u_int prot_entrtc;
#endif

  /* shit leftover from libexos */
  u_int state;			/* running, zombie, etc */
#define U_MAXCHILDREN 32
  int child_state[U_MAXCHILDREN]; /* what our children are doing */
  u_int child_ret[U_MAXCHILDREN]; /* if child zombie then ret code is here */
  int children[U_MAXCHILDREN];	/* and the pid's of our children */
  u_int u_chld_state_chng;	/* state of our children has changed */
  u_int parent_slot;		/* where we write our ret code in parent */
  u_int u_sig_exec_ign;         /* signals to be ignored in exec'd child */

  /* epilogue and yield counters */
  u_int u_epilogue_count;
  u_int u_epilogue_abort_count;
  u_int u_yield_count;


  /* ptracing info */
  int u_ptrace_flags;
  u_int u_ptrace_tracer;

  /* time of next timeout */
  unsigned long long u_next_timeout; 

  /* pager info */
  u_int u_freeable_pages;       /* approximate count of immediately freeable */

  /* reserved pages info */
  int u_reserved_pages;       /* the number of reserved pages remaining */

#ifdef __HOST__
  u_short u_cs;                   /* cs at timer interrupt */
  u_short u_ss;                   /* ss at timer interrupt */
  u_short u_ds;                   /* ds at timer interrupt */
  u_short u_es;                   /* es at timer interrupt */
  u_int u_eflags;                 /* eflags (mainly vm86 bit) at timer interrupt */
#endif
};

extern struct Uenv UAREA;         /* The Uenv structure at UADDR */
extern struct Uenv ash_u;         /* The same Uenv structure, but in the ASH */



struct EnvPD			/* environment's shared page dir */
{
  u_int envpd_id;		/* ID of page dir */
  u_int envpd_cr3[NR_CPUS]; 	/* Physical addresses of page dirs */
  u_int *envpd_pdir[NR_CPUS]; 	/* Kernel virtual addresses of page dirs */
  u_int envpd_last_eid[NR_CPUS];/* last env using pdir of each CPU */
  short *envpd_nptes; 		/* # of valid PTE's in each page table */
  short envpd_rc;		/* reference count */
  short envpd_active;		/* index to an allocated page directory */
  struct kspinlock envpd_spinlock; /* kernel spinlock for this pt */
};



typedef int (*Spred)();
struct Env {
  struct Trapframe env_tf;        /* Saved PC/stack/etc. must come first */
  
  int env_allowipc1;              /* set to 1 if accepting ipc's */
  int env_allowipc2;
 
  /* Anything below this point can be read but not written by the user via
   * sys_wrenv. sys_wrenv copies from top of env structure to offset
   * env_rdonly */
#define env_rdonly env_id
  u_int env_id;                   /* Unique environment identifier */
  int env_status;                 /* Status of the environment */
  u_int env_qx;                   /* Quantum expired */
#ifdef ASH_ENABLE
  u_int env_ashuva;               /* Low virtual address of ash region */
#endif
  u_int env_upa;	  	  /* pa of u-dot structure */
  struct Uenv *env_u;             /* Pointer to u-dot structure in kvm */
  u_int env_ticks;		  /* number of accumulated timer ticks */
  u_char env_quanta[NR_CPUS][QMAP_SIZE]; /* allocated quanta for this env */
  struct EnvPD *env_pd;		  /* page directory */
  /* keep the following two fields int instead of short for using with
   * compare_n_swap_32 */
  int env_last_cpu;		  /* last cpu */
  int env_cur_cpu;                /* current cpu, -1 if not active */
  u_int env_ctxcnt;		  /* number of context switches */
  
  msgringent *msgring;		  /* ipc message ring */
  u_int *env_pred_pgs;		  /* which pp's our sched pred is using */
  Spred env_pred;		  /* schedulability predicate */
  LIST_ENTRY(Env) env_link;       /* Free list */
  
  int env_clen;			  
  cap env_clist[MAX_CAPNO];

#ifdef __PAM__
  /* If the following field is >= 0, then it indicates that the process is
     inside a protected method; this field indicates the abstraction id. */
  int prot_abs_id;
#endif
  
  struct kspinlock env_spinlock;  /* kernel spinlock for this env */
  struct krwlock   cap_rwlock;    /* kernel rwlock on capabilities */

#ifdef __HOST__ 		  /* virtualization stuff - kiowa */
  u_int handler;
  u_int handler_stack;
#endif
};

LIST_HEAD(Env_list, Env);

#define env_offset(field) ((u_int) &((struct Env *) 0)->field)


#ifdef KERNEL
extern struct Env *__envs;
#else /* !KERNEL */
extern struct Env __envs[];
#endif /* !KERNEL */



static inline struct Env *
env_id2env (u_int envid, int *error)
{
  struct Env *e;

  e = &__envs[envidx (envid)];
  if (e->env_status == ENV_FREE) {
    *error = -E_BAD_ENV;
    return (NULL);
  }
  if (e->env_id != envid) {
    *error = -E_BAD_ENV;
    return (NULL);
  }
  *error = 0;
  return (e);
}


#ifdef KERNEL

#include <xok/kclock.h>
#include <machine/cpufunc.h>

void kill_env (struct Env *e);
void backtrace (tfp);
void env_init (void);
int env_alloc (struct Env **e, int newpt, int cpu);
void env_free (struct Env *);
struct Env *env_access (u_int k, u_int envid, u_char perm, int *error);
void set_cap_uarea (struct Env *e, cap *c);

void ash_invoke (struct Env *, unsigned int, unsigned int, unsigned int);
void ash_return (void);


#ifdef __SMP__
#define env0 __cpucxts[cpu_id]->_idle_env
#else
#define env0 &__envs[0]
#endif

extern struct Env* init_env;

extern unsigned int p0cr3;
extern unsigned int *p0pdir;


/* Get a capability from an environment's capability list */
static inline int
env_getcap (struct Env *e, u_int k, cap *c) 
  __XOK_SYNC(readsync on env_clist[k])
{
  if (k >= e->env_clen)
    return -E_CAP_INVALID;

  MP_RWLOCK_READ_GET(&curenv->cap_rwlock);
  *c = e->env_clist[k];
  MP_RWLOCK_READ_RELEASE(&curenv->cap_rwlock);

  if (!c->c_valid) 
    return -E_CAP_INVALID;

  return 0;
}


#ifdef __SMP__

/* returns 0 if env e can be and is localized (env_cur_cpu = cpu_id),
 * returns -1 otherwise */
static inline int
env_localize(struct Env *e)
{
  if (e->env_cur_cpu != -1 && e->env_cur_cpu != cpu_id) return -1;
  if (e->env_cur_cpu == cpu_id) return 0;

  if (compare_and_swap(&e->env_cur_cpu, -1, cpu_id) == -1)
    return 0;
  return -1;
}

static inline void
env_release(struct Env *e)
{
  if (!curenv || e->env_id != curenv->env_id) e->env_cur_cpu = -1;
}

  
static inline void
env_force_release(struct Env *e)
{
  e->env_cur_cpu = -1;
}
#endif



#define env_upcall() env_pop_tf(utf)

static inline void env_pop_tf (tfp tf) __attribute__ ((noreturn));
static inline void
env_pop_tf (tfp tf)
{
  extern void panic (const char *, ...) __attribute__ ((noreturn));

#ifdef __SMP__
#if 0
  if (get_cpu_id() != cpu_id)
  {
    printf("warning: get_cpu_id %d, cpu_id %d\n", get_cpu_id(), cpu_id);
    assert(get_cpu_id() == cpu_id);
  }
#endif
#endif
  asm volatile ("movl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\tiret"
		:: "g" (tf) : "memory");
  panic ("iret failed");  /* mostly to placate the compiler */
}



/* do a context switch: 
 * - map CPU specific entries into page directory
 * - switch correct page directory into cr3
 * - load cr3
 */
static inline void env_ctx_switch(struct Env *e)
  /* this function should only be called after e->env_cur_cpu has been set to
   * cpu_id. This guarantees that only one processor can attempt to run
   * this function at a time */
{
  int ua_changed = 0;

  curenv = e;

  if (e->env_pd->envpd_cr3[cpu_id] == 0) 
  {
    extern int env_dup_pdir_forcpu(struct Env *e);
    if (env_dup_pdir_forcpu(e) < 0)
    {
      printf("context switch: cannot create pdir for env %d\n",e->env_id);
      curenv = 0;
      sched_runnext();
    }
  }
    
  if (e->env_id != e->env_pd->envpd_last_eid[cpu_id])
  {
    /* if UAREA not already mapped at UADDR, map it */

    ((Pte *)ptov(e->env_pd->envpd_pdir[cpu_id][PDENO(UADDR)]&~PGMASK))
      [PTENO(UADDR)] = e->env_upa | PG_P | PG_W | PG_U;
    
    e->env_pd->envpd_last_eid[cpu_id] = e->env_id;
    ua_changed = 1;
  }

#ifndef __SMP__
  if (e->env_pd->envpd_id != cur_pd) 
#else
  if (e->env_pd->envpd_id != cur_pd || e->env_last_cpu != cpu_id) 
#endif
  {
#ifdef __SMP__
    if (e->env_last_cpu != cpu_id)
      e->env_last_cpu=cpu_id;
#endif
    cur_pd = e->env_pd->envpd_id;
    e->env_ctxcnt++;
    lcr3(e->env_pd->envpd_cr3[cpu_id]);
  }

  else if (ua_changed)
    /* invalidate local TLB */
    invlpg(UADDR);
}



static inline void
env_save_all_tf ()
{
  memcpy (&curenv->env_tf, utf, sizeof(struct Trapframe));
}

static inline void
env_save_tf ()
{
  memcpy (&curenv->env_tf.tf_es, &utf->tf_es, tf_size (tf_es, tf_hi));
}



static inline void env_run (struct Env *) __attribute__ ((noreturn));

/* Run an environment.  We need to restore the es, and ds registers
 * (the prologue can do the rest).  We restore es and ds because, God
 * forbid, there might be some sort of segmentation in use in addition
 * to the paging protection, and so the user might not be allowed
 * access to the full 32 bits of virtual address space used by the
 * kernel segment descriptors.  More importantly, however, we can't
 * give out segment registers with kernel privs anyway, so we have to
 * reload them from somewhere.
 */

static inline void 
env_run (struct Env *e)
{
  if (e->env_u->u_entprologue)
    e->env_tf.tf_eip = e->env_u->u_entprologue;
  
  check_rtc_interrupt(e->env_u);
 
  env_ctx_switch(e);
  
  /* make sure TS bit in cr0 is set so that using the FPU will
     cause a trap */
  {
    u_int r = rcr0();
    if (!(r & CR0_TS)) lcr0(r | CR0_TS);
  }

  env_pop_tf (&e->env_tf);
}




/* Get PTE pointer.  Returns the address of page table entry for
 * address addr in page directory pdir */
static inline Pte *
env_va2ptep (struct Env *e, u_int va)
{
  Pde *const pdir = e->env_pd->envpd_pdir[e->env_pd->envpd_active];
  Pde pde;
  Pte *pt;

  pde = pdir[PDENO (va)];
  if (! (pde & PG_P) || (pde & PG_PS))
    return (NULL);
  pt = ptov (pde & ~PGMASK);
  return (&pt[PTENO (va)]);
}
#define va2ptep(va) env_va2ptep (curenv, va)

/* Get PTE value.  Returns the value of page table entry for
 * address addr in page directory pdir */
static inline u_int
env_va2pte (struct Env *e, u_int va)
{
  return ((u_int) (*(env_va2ptep(e,va))));
}
#define va2pte(va) env_va2pte (curenv, va)

static inline u_int
env_va2pa (struct Env *e, void *va)
{
  Pde *const pdir = e->env_pd->envpd_pdir[e->env_pd->envpd_active];
  Pde pde;
  Pte pte;

  pde = pdir[PDENO ((u_int)va)];
  if (! (pde & PG_P))
    return 0;
  if (pde & PG_PS)
    return ((pde & ~PDMASK) | ((u_int) va & PDMASK));
  pte = ((Pte*) ptov (pde & ~PGMASK))[PTENO (va)];
  if (! (pte & PG_P))
    return 0;
  return ((pte & ~PGMASK) | ((u_int) va & PGMASK));
}
#define va2pa(va) env_va2pa (curenv, va)



#endif /* KERNEL */

#endif /* !__ASSEMBLER__ */

#endif /* !_XOK_ENV_H_ */
