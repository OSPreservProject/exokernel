
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

#define LOG2NENV 7
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

#include <xok/cpu.h>
#include <xok/scheduler.h>
#include <xok/defs.h>
#include <xok/queue.h>
#include <xok/capability.h>
#include <xok/trap.h>
#include <xok/mmu.h> 
#include <xok/kerrno.h> 

/* User-writeable portion of the Env structure */
struct Uenv {
  /* keep these 8 fields together so they all fall on the same
     cache line. They're used in sequence by the yield code */
  int u_status;                 /* process is runnable if > 0 */
  int u_donate; 		/* env to donate to on yield */
  u_int u_ppc;                  /* eip at timer interrupt */
  u_int u_in_critical;		/* yield at end of timeslice or not */
  u_int u_interrupted;		/* kernel tried to take our timeslice */
  u_int u_critical_eip;         /* eip when entering critical region */
  u_int u_fpu;			/* using the fpu or not */
  u_int u_in_pfault;		/* in a page fault or not*/

  u_int u_pred_tag;		/* tag of pred that matched */
  /* Don't change the order of the following 7 fields. sys_prot_call
     depends on this order !!! */
  u_int u_entepilogue;          /* Epilogue entry point */
  u_int u_entprologue;          /* Prologue entry point */
  u_int u_entfault;             /* Page fault entry */
  u_int u_entipc1;              /* an IPC entry point */
  u_int u_entipc2;              /* another IPC entry point */
  u_int u_entipca;              /* ASH IPC entry point */
  u_int u_entrtc;               /* Runtime clock entry */
  u_char u_pendrtc;             /* Runtime clock pending flag */
  u_int u_xsp;                  /* Exception stack (0 = current stack) */
  u_int u_xsp2;                 /* 2nd Exception stack (0 = current stack) */
  u_int u_entfault2;            /* 2nd Page fault entry */

  u_int u_ashsp;                /* Stack pointer for ash execution */
  u_int u_ashnet;               /* Network ash entry point */
  u_int u_ashdisk;              /* Disk ash entry point */
  /* Grant capability #gcap (dominated by #key) to acquiring environment
   * #aenvid */
  u_int u_aenvid;
  int u_gkey;
  cap u_gcap;
  /* initial process management fields */
  u_int pid;			/* process id */

  u_int state;			/* running, zombie, etc */
  u_int parent;			/* pid of parent */
#define U_NAMEMAX 255		/* HBXX- this needs to be really PATH_MAX
				   since dynamic programs load themselves in
				   and their prog path can be long.  For now
				   setting it to this value. */
  u_char name[U_NAMEMAX];	/* name we were invoked under */
#define U_MAXCHILDREN 32
  int child_state[U_MAXCHILDREN]; /* what our children are doing */
  u_int child_ret[U_MAXCHILDREN];	/* if child zombie then ret code is here */
  int children[U_MAXCHILDREN];	/* and the pid's of our children */
  u_int u_chld_state_chng;	/* state of our children has changed */
  u_int parent_slot;		/* where we write our ret code in parent */
  u_int u_sig_exec_ign;         /* signals to be ignored in exec'd child */

  unsigned long long u_next_timeout; /* time of next timeout */

  /* epilogue and yield counters */
  u_int u_epilogue_count;
  u_int u_epilogue_abort_count;
  u_int u_yield_count;

  /* when to start kern call counting */
  u_int u_start_kern_call_counting;

  /* ptracing info */
  int u_ptrace_flags;
  u_int u_ptrace_tracer;
  u_int u_ss_pfaults;        /* set to keep TF on after a page fault */

  u_int u_revoked_pages;	/* number of pages kernel is asking for back */

  /* Saved entry points (while in a protected method) 
     Don't change the order (sys_prot_call depends on it) */
  u_int prot_entepilogue;
  u_int prot_entprologue;
  u_int prot_entfault;
  u_int prot_entipc1;
  u_int prot_entipc2;
  u_int prot_entipca;
  u_int prot_entrtc;

  /* pager info */
  u_int u_freeable_pages;       /* approximate count of immediately freeable
				   pages */

  /* extra ipc arguments */
#define U_MAX_EXTRAIPCARGS 16
  u_int u_ipc1_extra_args[U_MAX_EXTRAIPCARGS];

  /* user controlled profiling clock */
  u_int u_rtc_rega;
};
extern struct Uenv UAREA;             /* The Uenv structure at UADDR */
extern struct Uenv ash_u;         /* The same Uenv structure, but in the ASH */

struct Env {
  struct Trapframe env_tf;        /* Saved PC/stack/etc.  Must come first */
  int env_allowipc1;              /* set to 1 if accepting ipc's */
  int env_allowipc2;
  /* Anything below this point can be read but not written by the user */
#define env_rdonly env_id
  u_int env_id;                   /* Unique environment identifier */
  int env_status;                 /* Status of the environment */
  u_int env_qx;                   /* Quantum expired */
  u_int env_ashuva;               /* Low virtual address of ash region */
  struct Uenv *env_u;             /* Pointer to u-dot structure in kvm */
  u_int *env_pdir;                /* Kernel virtual address of page dir */
  short *env_npte;                /* # of valid PTE's in each page table */
  u_int env_cr3;                  /* Physical address of page dir */
  LIST_ENTRY(Env) env_link;       /* Free list */
  int env_clen;
  cap env_clist[MAX_CAPNO];
  u_int env_cur_cpu;		  /* current/last cpu */
  u_int *env_pred_pgs;		  /* which pp's our sched pred is using */
  Spred env_pred;		  /* schedulability predicate */
  u_int env_ticks;		  /* number of accumulated timer ticks */
  u_char env_quanta[NR_CPUS][QMAP_SIZE]; /* allocated quanta for this env */
  u_int env_ctxcnt;		  /* number of context switches (incremented
				     in env_run) */

  /* If the following field is >= 0, then it indicates that the process is
     inside a protected method; this field indicates the abstraction id. */
  int prot_abs_id;
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
  return (e);
}

#ifdef KERNEL

#include <xok/kclock.h>
#include <machine/cpufunc.h>

void kill_env (struct Env *e);
void backtrace (tfp);

#define env0 &__envs[0]

extern unsigned int p0cr3;
extern unsigned int *p0pdir;


/* Get a capability from an environment's capability list */
static inline cap *
env_getcap (struct Env *e, u_int k, int *error)
{
  if (k >= e->env_clen || ! e->env_clist[k].c_valid) {
    *error = -E_CAP_INVALID;
    return (NULL);
  }
  return (&e->env_clist[k]);
}

void env_init (void);
int env_alloc (struct Env **e);
void env_free (struct Env *);
void static inline env_run (struct Env *) __attribute__ ((noreturn));
struct Env *env_access (u_int k, u_int envid, u_char perm, int *error);

void ash_invoke (struct Env *, unsigned int, unsigned int, unsigned int);
void ash_return (void);

#define env_upcall() env_pop_tf (utf);

static inline void env_pop_tf (tfp tf) __attribute__ ((noreturn));
static inline void
env_pop_tf (tfp tf)
{
  extern void panic (const char *, ...) __attribute__ ((noreturn));
  asm volatile ("movl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\tiret"
		:: "g" (tf) : "memory");
  panic ("iret failed");  /* mostly to placate the compiler */
}



/* Run an environment.  We need to restore the es, and ds registers
 * (the prologue can do the rest).  We restore es and ds because, God
 * forbid, there might be some sort of segmentation in use in addition
 * to the paging protection, and so the user might not be allowed
 * access to the full 32 bits of virtual address space used by the
 * kernel segment descriptors.  More importantly, however, we can't
 * give out segment registers with kernel privs anyway, so we have to
 * reload them from somewhere.
 */
static void inline
env_run (struct Env *e)
{
  if (curenv)
    memcpy (&curenv->env_tf.tf_es, &utf->tf_es, tf_size (tf_es, tf_hi));
  curenv = e;
  if (e->env_id != env_cr3_eid) {
    env_cr3_eid = e->env_id;
    e->env_ctxcnt++;
    lcr3(e->env_cr3);
  }
  /* make sure TS bit in cr0 is set so that using the FPU will
     cause a trap */
  {
    u_int r = rcr0();
    if (!(r & CR0_TS)) lcr0(r | CR0_TS);
  }

  if (UAREA.u_entprologue)
    e->env_tf.tf_eip = UAREA.u_entprologue;
  check_rtc_interrupt();
  env_pop_tf (&e->env_tf);
}

#endif /* KERNEL */

#endif /* !__ASSEMBLER__ */

#endif /* !_XOK_ENV_H_ */
