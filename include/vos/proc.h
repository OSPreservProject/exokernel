
/*
 * Copyright 1999 MIT
 */


#ifndef __VOS_PROCESS_H__
#define __VOS_PROCESS_H__

#include <xok/types.h>
#include <xok/env.h>
#include <sys/syslimits.h>


/* From entry.S */
extern void xue_epilogue (void);
extern void xue_prologue (void);
extern void xue_fault (void);
extern void xue_yield (void);
extern char edata;
extern char end;

/* from xrt0.c */
extern u_int __brkpt;
extern u_int __stkbot;
extern u_int __xstktop;


/* defined in xok/types.h 
 * 
 * typedef u_int pid_t;   
 * typedef u_int uid_t;
 * typedef u_int gid_t;
 */


/* maximum number of processes = maximum number of environments supported by
 * the exokernel */
#define NPROC NENV

/* proc.p_status values */
#define PROC_FREE 	0 		/* proc is free */
#define PROC_RUN  	1		/* proc is running */
#define PROC_SLEEP	2  		/* proc is sleeping */
#define PROC_STOP	3		/* proc is stopped */
#define PROC_DEAD	4		/* proc is dead */


/* proc data structure kept in procd and per-process */
struct ExtProc 
{
  /* default process abstraction in UAREA has pid, ppid and name. */
  
  struct Env *curenv;			/* pointer to current environment */
  u_int p_num_children;			/* number of children */
#define CHILDREN_MASK NPROC>>3
  u_char p_children[CHILDREN_MASK];	/* children bitmap */
};

#define my_proc	 UAREA.ext_proc
#define __curenv (UAREA.ext_proc)->curenv



/* yield to a particular env, going through sched */
static inline void env_yield (int envid) 
{
  UAREA.u_donate = envid;
  asm ("pushl %0\n"
       "\tpushfl\n"
       "\tjmp _xue_yield\n"
       :: "g" (&&next));
  goto next; /* get around warning about unused label */
next:
}


/* fast yield to a particular env */
static inline void env_fastyield (int envid) 
{
  UAREA.u_donate = envid;
  asm ("pushl %0\n"
       "\tpushfl\n"
       "\tjmp _xue_fyield\n"
       :: "g" (&&next));
  goto next; /* get around warning about unused label */
next:
}


/* returns the process ID */
extern inline pid_t
getpid (void)
{
  return UAREA.pid;
}

/* returns parent pid */
extern inline pid_t
getppid (void)
{
  return UAREA.parent;
}


/* returns true if process still alive */
extern inline int env_alive(u_int eid)
{
 if (__envs[envidx(eid)].env_status == ENV_FREE ||
     __envs[envidx(eid)].env_id != eid)
   return 0;
 return 1;
}


/* suspend the environment - only wakesup on libos related wakeup predicate
 * wakeups, on IPC requests, or directed yield.
 */
extern void env_suspend();


/* procedures to be run right before and after fork */

#define FORK_HANDLER_MAX 64
typedef void (*vos_fork_handler_t) (u_int);
typedef void (*vos_child_start_handler_t) ();

extern vos_fork_handler_t fork_handlers[FORK_HANDLER_MAX];
extern vos_child_start_handler_t child_handlers[FORK_HANDLER_MAX];
extern u_int fork_handler_index;
extern u_int child_handler_index;

extern inline int
proc_before_fork(vos_fork_handler_t h)
{
  if (fork_handler_index >= FORK_HANDLER_MAX)
    return -1;

  else
  {
    fork_handlers[fork_handler_index] = h;
    fork_handler_index++;
    return fork_handler_index-1;
  }
}

extern inline int
proc_child_start(vos_child_start_handler_t h)
{
  if (child_handler_index >= FORK_HANDLER_MAX)
    return -1;

  else
  {
    child_handlers[child_handler_index] = h;
    child_handler_index++;
    return child_handler_index-1;
  }
}


/* procedure to fork and exec */
extern pid_t fork();
extern pid_t fork_to_cpu(u_int cpu, int quantum);


/* _exit */
extern void _exit(int status) __attribute__ ((noreturn));


#endif /* __VOS_PROCESS_H__ */

