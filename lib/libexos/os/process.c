
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

/* Process-abstraction related routines.

   This is somewhat of a random collection of routines that fill
   in the missing pieces of the process abstraction mainly presented by
   the vm and file-descriptor code. */

#include <xok/defs.h>
#include <xok/sys_ucall.h>
#include <exos/ipcdemux.h>
#include <exos/conf.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <exos/process.h>
#include <exos/cap.h>
#include <exos/ipc.h>
#include <xok/mmu.h>

#include <exos/shm.h>
#include <exos/locks.h>
#include <exos/critical.h>
#include <exos/signal.h>
#include <exos/callcount.h>
#include <exos/osdecl.h>
#include <exos/uwk.h>
#include <exos/cap.h>
#include <xok/sysinfo.h>

#include <debug/i386-stub.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include <exos/kprintf.h>
#ifdef PROCD
#include "procd.h"
extern char *__progname;
#endif

#ifndef NOFD
#include <fd/procaux.h>
#endif

#include <fd/fdstat.h>


#ifdef PRINTFDSTAT
//#define kproprintf if (pr_fd_statp) kprintf
#define kproprintf(format,args...) /* this way disabled always */
#else
#define kproprintf(format,args...)
#endif


static void SystemStartup ();
static void ExecInit ();

#ifdef PROCESS_TABLE
static void ProcInfoInit ();
proc_info *ProcInfo = (proc_info *)0; /* the process table. Zero so
					    we can tell if it's been
					    inited or not */

static int send_exit(u_int envid, int slot, int status);
static int receive_exit(int code, int slot, int status, int c, u_int caller);
#endif

#ifdef SEED_RANDOM_IN_EXECINIT
static void SeedRandomInit (void);
#endif

void panic (char *s) {
  sys_cputs ("exos panic: ");
  sys_cputs (s);
  sys_cputs ("\n");
  sys_capdump();
  /*  __stacktrace(); */
  printf ("exos panic: %s\n", s);
  __die ();
}


void ProcessFreeQuanta(int eid) 
{
  int i_cpu, i_index, i_offset, error;
  int cpu_total = sys_get_num_cpus();
  struct Env* e = env_id2env(eid, &error);

  if (!e) return; /* XXX - return error? */
  for (i_cpu = 0; i_cpu < cpu_total; i_cpu++) {
    for(i_index = 0; i_index < QMAP_SIZE; i_index++) {
      char q = e->env_quanta[i_cpu][i_index];
      for(i_offset = 0; i_offset < 8; i_offset++) {
        if ((q & (1<<i_offset)) != 0) {
	  sys_quantum_free(0, i_index*8+i_offset, i_cpu);
	}
      }
    }
  }
}


/* Called once at the very start of the first process */
static void SystemStartup () {
  InitCaps();

#ifdef SYSV_SHM
  if (InitializeSysVShmSys () == -1) {
    panic ("Could not initialize System V shared memory\n");
  }
#endif

#ifndef NOFD
  FDFreshProcInit();
#endif
}

/* called whenever a process starts via an exec */
static void ExecInit () {
  OnExec(CopyCaps);
  OnFork(CopyCaps);

#ifdef SEED_RANDOM_IN_EXECINIT
  SeedRandomInit();
#endif

#ifdef SYSV_SHM
  SysVShmExecInit ();
#endif

  ExosLocksInit();  /* uses shm so keep after shm init */

#ifdef PROCESS_TABLE
  ProcInfoInit ();  /* uses shm so keep after shm init */
#endif
#ifdef PROCD
  proc_table_init();
#endif

  FDExecInit();
  signals_init();
  remote_debug_initialize();                 /* needs signals */
}

/* Control goes here from startup_thread before main is executed */

void ProcessStartup () {
  /* if we're the first process to run do the per-system initialization */
  /* kprintf ("ProcessStartup is environment 0x%x envidx %d\n", __envid,envidx(__envid)); */


  if (envidx(__envid) == 1) {
    SystemStartup ();
  }

  if (envidx(__envid) != 0) {
     ExecInit ();
  }
}

/* control allways goes here when a process terminates for whatever
   reason. Termination reason is passed in CompletetionCode */
static int exit_called = 0;
void ProcessEnd (int ret, unsigned int epc) __attribute__ ((noreturn));
void ProcessEnd (int ret, unsigned int epc) { 
#ifdef PROCESS_TABLE
  int i;
#endif
  int pid;

#ifdef EXOPROFILE
  extern void _mcleanup (void);
#endif

  /* let dynamically added cleanup code run */

  kproprintf("ExecuteExosExitHandlers\n");
  ISTART(misc,step2);
  ExecuteExosExitHandlers ();
  STOPK(misc,step2);
  /* insert your own cleanup code here */

#ifdef EXOPROFILE
    _mcleanup();
#endif 

  pid = getpid();
#ifndef NOFD
  kproprintf("FDEndProcess\n");
  ISTART(misc,step3);
  FDEndProcess();
  STOPK(misc,step3);
#endif

  /* XXX - hopefully, this can be moved further down at some point */
  if (exit_called) OSCALLEXIT(OSCALL_exit);
  __call_count_results();

#ifdef PROCESS_TABLE
  /* tell parent and children we're dead */
  kproprintf("ProcTableExit\n");
  ISTART(misc,step0);
  ProcTableExit (ret, 0);
  STOPK(misc,step0);

  kproprintf("Drop Ref Counts\n");
  ISTART(misc,step1);
  /* drop refs to ourselves and our parent */
  UnrefPid (getpid ());
  UnrefPid (getppid ());
  /* and drop refs to all our children */
  for (i = 0; i < U_MAXCHILDREN; i++) {
    if (UAREA.child_state[i] != P_EMPTY) {
      UnrefPid (UAREA.children[i]);
    }
  }
  STOPK(misc,step1);
#endif
#ifdef PROCD
  {
    int status;
    //kprintf("pid: %d envid: %d %s proc_exit(%d)",getpid(),__envid,__progname,ret);
    status = proc_exit(ret);
    //kprintf(" -> %d\n", status );
  }
#endif

  /* shutdown any tcp connections in this process. This is special
     cased (and not just part of the normal fd exithandler code)
     because we need to wait a couple of minutes for any sockets
     in time-wait. However, we want to tell everyone else
     that we're done so that shell's won't wait for us etc. */

  {
    extern void exos_tcpsocket_exithandler (); /* in lib/xio/exos_tcpsocket.c */
    exos_tcpsocket_exithandler ();
  }

#ifdef SYSV_SHM
  kproprintf("ClenupSysVShm\n");
  ISTART(misc,step2);
  CleanupSysVShm ();
  STOPK(misc,step2);
#endif

  STOPK(misc,dealloc);
  kproprintf("That was total exit time\n");
  __die ();
}

#ifdef SEED_RANDOM_IN_EXECINIT
static void 
SeedRandomInit(void)
{
  /* Seed the random number generator or all processes (and hence
   * things like cffs) pick random numbers in the same order.
   * srandom takes a u_int.  */
  u_int randomseed = 
    (u_int)(__envid) ^ (u_int)(__sysinfo.si_system_ticks) ^ (u_int)(__envid<<16)
    ^ (u_int)(__sysinfo.si_diskintrs) 
    ^ (u_int)(__sysinfo.si_networks[1].rcvs<<16);
  srandom(randomseed);
}
#endif

#ifdef PROCESS_TABLE

/* process table initialization */

static void ProcInfoInit () {
     int Segment;
     int NeedsInit = 0;
     int Index;
     char *argv[] = {"init"};
 
     /* see if the proc table is already there */
     Segment = shmget (PROC_INFO_KEY, sizeof (proc_info), 0);
     if (Segment == -1) {
	  if (errno == ENOENT) {
	       /* process table doesn't exist, so we need to create it */
	       Segment = shmget (PROC_INFO_KEY, 
				 sizeof (proc_info), IPC_CREAT);
	       NeedsInit = 1;
	  }

	  /* catch the original shmget failing for some reason besides
	     ENOENT or the second shmget failing for any reason. */

	  if (Segment == -1) {
	       panic ("ProcInfoInit: Could not create process table");
	  }
     }

     /* ok, attach the process table to our address space */
     if ((ProcInfo = (proc_info *)shmat (Segment, (char *)0, 0)) == (proc_info *)-1) {
	  ProcInfo = (proc_info *)0; /* so ProcessEnd won't try to use it */
	  panic ("ProcInfoInit: Could not attach process table");
     }

     /* if we just created the proc table above we need to initialize it */
     if (NeedsInit) {
	  for (Index = 0; Index < NENV; Index++) {
	       ProcInfo->pidMap[Index] = 0;
	       ProcInfo->refCount[Index] = 0;
	  }

	  /*	  exos_lock_init (&ProcInfo->lock); */

	  /* we need to add ourselves here */
	  ProcInfo->nextPid = 2;
	  ProcChangeEnv (1, __envid);
	  ProcInfo->refCount[1] = 1;
	  AddProcEntry (&UAREA, argv[0], argv, 1, 0);
     }

     /* no children yet */
     for (Index = 0; Index < U_MAXCHILDREN; Index++) {
       UAREA.child_state[Index] = P_EMPTY;
     }
     
     ipc_register(IPC_EXIT, receive_exit);
}


/* pids are reference counted */

void RefPid (int pid) {
  dlockputs(__PROCD_LD,"RefPid get lock ");
  EXOS_LOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical ();
  if (ProcInfo->pidMap[pid] == 0) {
    EXOS_UNLOCK(PROCINFO_LOCK);
    dlockputs(__PROCD_LD,"RefPid release lock\n");
    ExitCritical ();
    /*    exos_unlock (&ProcInfo->lock);*/
    panic ("bogus pid passed to RefPid");
  }
  ProcInfo->refCount[pid]++;
  EXOS_UNLOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"RefPid release lock\n");
  ExitCritical ();
}

void UnrefPid (int pid) {
  dlockputs(__PROCD_LD,"UnrefPid get lock ");
  EXOS_LOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical ();
  if (ProcInfo->pidMap[pid] == 0) {
    EXOS_UNLOCK(PROCINFO_LOCK);
    dlockputs(__PROCD_LD,"UnrefPid release lock\n");
    ExitCritical ();
    return;
  }
  if (ProcInfo->refCount[pid] == 0) {
    EXOS_UNLOCK(PROCINFO_LOCK);
    dlockputs(__PROCD_LD,"UnrefPid release lock\n");
    ExitCritical ();
    panic ("bogus pid passed to UnrefPid");
  }
  ProcInfo->refCount[pid]--;
  if (ProcInfo->refCount[pid] == 0) {
    ProcInfo->pidMap[pid] = 0;
  }
  EXOS_UNLOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"UnrefPid release lock\n");
  ExitCritical ();
}

int GetRefCount (int pid) {
  int ret;
  dlockputs(__PROCD_LD,"GetRefCount get lock ");
  EXOS_LOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical ();
  if (ProcInfo->pidMap[pid] == 0) {
    EXOS_UNLOCK(PROCINFO_LOCK);
    dlockputs(__PROCD_LD,"GetRefCount release lock\n");
    ExitCritical ();
    panic ("bogus pid passed to GetRefCount");
  }
  ret = ProcInfo->refCount[pid];
  EXOS_UNLOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"GetRefCount release lock\n");
  ExitCritical ();
  return (ret);
}

int GetChildSlot (int pid) {
  int i;

  /* XXX -- more lock misuse */
  dlockputs(__PROCD_LD,"GetChildSlot get lock ");
  EXOS_LOCK(UAREA_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical ();

  for (i = 0; i < U_MAXCHILDREN; i++) {
    if (UAREA.child_state[i] == P_EMPTY) {
      UAREA.child_state[i] = P_RUNNING;
      UAREA.children[i] = pid;
      EXOS_UNLOCK(UAREA_LOCK);
      dlockputs(__PROCD_LD,"GetChildSlot release lock\n");
      ExitCritical ();
      /* remember that our child has a ref to us and vice-versa */
      RefPid (pid);
      RefPid (getpid ());
      return (i);
    }
  }

  EXOS_UNLOCK(UAREA_LOCK);
  dlockputs(__PROCD_LD,"GetChildSlot release lock\n");
  ExitCritical ();
  return (-1);
}

void ClearChildInfo (struct Uenv *cu) {
  int i;

  for (i = 0; i < U_MAXCHILDREN; i++) {
    cu->child_state[i] = P_EMPTY;
  }
}

/* fill in our personal info. */

void AddProcEntry (struct Uenv *u, char *path, 
		   char *argv[], int pid, int parent) {
  u->pid = pid;
  {
    extern unsigned pending_nonblocked_signal;
    pending_nonblocked_signal = 0;
  }
  u->state = P_RUNNING;
  u->parent = parent;
  strncpy (u->name, path, U_NAMEMAX - 1); 
  u->name[U_NAMEMAX - 1] = 0;
}

/* set return status

   Write our exit status into our parent's u-area and then send them
   a SIGCHLD. */

void ProcTableExit (int ret, int status) {
  int i;
  /*  struct Uenv pu; */

  /* JJJ  What happens if we ipc to a dead env? */
  /* tell all our children we're going away */
  for (i=0; i < U_MAXCHILDREN; i++) {
    if (UAREA.child_state[i] == P_RUNNING)
      send_exit(ProcInfo->pidMap[UAREA.children[i]], -1, 0);
  }

  /* tell our parent we died, but only if our parent is still there */
  if (UAREA.parent_slot != -1) {
    send_exit(ProcInfo->pidMap[getppid()], UAREA.parent_slot, ret);
    kill (getppid (), SIGCHLD);
  }
}

/* called in a critical section */
void ProcChangeEnv (int pid, int env) {
#if 0
  assert ((envidx(env) > 0) && (envidx(env) < NENV));
  if (envidx(env) <= 0 || envidx(env) >= NENV) {
    ExitCritical ();
    assert (0);
  }
#endif
  ProcInfo->pidMap[pid] = env;
}

u_int 
pid2envid(pid_t pid) {
  return ProcInfo->pidMap[pid];
}
#endif /* PROCESS_TABLE */

/* this alias crap is due to gcc suckage not likeing the 
   name _exit with respect to the noreturn attribute */

void _exit (int) __attribute__ ((noreturn));
void _osexit (int) __attribute__ ((noreturn));
DEF_ALIAS_FN (_exit,_osexit);
void _osexit (int ret) {
  static int exited_before = 0;
  extern int __returned_from_main; /* in xrt0.c */
  
  signals_off();		/* we don't want recursive exits */

  if (!exit_called) {
    OSCALLENTER(OSCALL_exit);
    exit_called = 1;
  }
  if (exited_before) {
    sys_cputs("\n_exit: recursive exit!!\n");
    __stacktrace(0);
    __stacktrace(1);

    __die ();
  }
  exited_before = 1;
  kproprintf("ExecuteAtExitHandlers\n");
  ISTART(misc,put);
  if (__returned_from_main == 1) {
    ExecuteAtExitHandlers(0);
  } else {
    ExecuteAtExitHandlers(ret);    
  }
  STOPK(misc,put);
  ProcessEnd (W_EXITCODE (ret, 0), 0);
}

#ifdef PROCESS_TABLE
/* find terminated child *pid if *pid > 0, otherwise any pid that has
   terminated. If options & WNOHANG is set, block until child has
   been found. When child is found and if status is non-null, set
   *status to the returned status of the child. 

   Returns pid of terminated child if one was found, else zero

   XXX - SA_NOCLDWAIT semantics not implemented
*/
int wait4 (int pid, int *status, int options, struct rusage *rusage) {
  int child = 0;		/* keep compiler happy */
  int ret;
  int saw_child = 0;

  OSCALLENTER(OSCALL_wait4);
  /* XXX - rusage not implementetd */
  if (rusage != (struct rusage *)0)
    memset((void *)rusage, 0, sizeof(struct rusage));

  while (1) {
    /* poll for children once */
    dlockputs(__PROCD_LD,"wait4 get lock ");
    EXOS_LOCK(UAREA_LOCK);
    dlockputs(__PROCD_LD,"... got lock\n");
    EnterCritical ();
    saw_child = 0;
    for (child = 0; child < U_MAXCHILDREN; child++) {
      if (UAREA.child_state[child] != P_EMPTY) {
	saw_child = 1;
      }
      if (UAREA.child_state[child] == P_ZOMBIE) {
	if (pid > 0)
	  if (pid != UAREA.children[child])
	    continue;
	ret = UAREA.children[child];
	if (status)
	  *status = UAREA.child_ret[child];
	UAREA.child_state[child] = P_EMPTY;
	UnrefPid (UAREA.children[child]);
	UAREA.u_chld_state_chng = 0;
  
	EXOS_UNLOCK(UAREA_LOCK);
	dlockputs(__PROCD_LD,"wait4 release lock\n");
	ExitCritical ();
	OSCALLEXIT(OSCALL_wait4);
	return ret;
      }
    }
    EXOS_UNLOCK(UAREA_LOCK);
    dlockputs(__PROCD_LD,"wait4 release lock\n");
    ExitCritical ();
 
    if (!saw_child) {
      errno = ECHILD;
      OSCALLEXIT(OSCALL_wait4);
      return -1;
    }

    if (! (options & WNOHANG)) {
	/* wait for state to change on child process */
      wk_waitfor_value (&UAREA.u_chld_state_chng, 1, CAP_ROOT);
    } else {
      OSCALLEXIT(OSCALL_wait4);
      return 0;
    }
  }
}

/* find a free pid and associate it with the given env */

int AllocateFreePid (u_int e) {
  u_int start;
  u_int cycle = 0;

  dlockputs(__PROCD_LD,"AllocateFreePid get lock ");
  EXOS_LOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical ();
  start = ProcInfo->nextPid;

  do {
    if (start == PID_MAX) {
      start = PID_MIN;
      if (cycle) {
  	EXOS_UNLOCK(PROCINFO_LOCK);
  	dlockputs(__PROCD_LD,"AllocateFreePid release lock\n");
	ExitCritical ();
	panic ("AllocateFreePid: Out of pids");
      } else {
	cycle = 1;
      }
    }
  } while (ProcInfo->pidMap[start++] != 0);

  ProcInfo->nextPid = start;
  ProcChangeEnv (start-1, e);
  EXOS_UNLOCK(PROCINFO_LOCK);
  dlockputs(__PROCD_LD,"AllocateFreePid release lock\n");
  ExitCritical ();
  RefPid (start-1);
  return (start-1);
}

static int receive_exit(int code, int slot, int status, int c, u_int caller) {
  assert(code == IPC_EXIT);
  if (slot == -1) {		/* Parent Death */
    UAREA.parent_slot = -1;
  } else {
    UAREA.child_state[slot] = P_ZOMBIE;
    UAREA.child_ret[slot] = status;
    UAREA.u_chld_state_chng = 1;
  }
  return 0;
}

static int send_exit(u_int envid, int slot, int status) {
  return sipcout(envid, IPC_EXIT, slot, status, 0);
}

#endif /* PROCESS_TABLE */

extern char *__progname;

/* DEBUGGING ARGV */
#define DEBUGGING_ARGV
#define DEBUGGING_NARGV 25
#define DEBUGGING_ARGV_LENGTH 50
static char __debugging_argv[DEBUGGING_NARGV][DEBUGGING_ARGV_LENGTH];
static int __debugging_nargv;
static int __debugging_nargv_real;

void __debug_copy_argv(int argc, char **argv) {
#ifdef DEBUGGING_ARGV
  int i;
  __debugging_nargv_real = argc;
  __debugging_nargv = min(argc, DEBUGGING_NARGV);
  for (i = 0; i < __debugging_nargv; i++) {
    strncpy((char *)__debugging_argv[i],
	    argv[i], 
	    DEBUGGING_ARGV_LENGTH - 1);
    __debugging_argv[i][DEBUGGING_ARGV_LENGTH - 1] = '\0';
  }
#endif
}
/* 1 = stdout, 0 = console */
void __debug_print_argv(int tostdoutorconsole) {
#ifdef DEBUGGING_ARGV
  int i;
  if (tostdoutorconsole) {
    printf("real number of argvs %d, saved: %d\n",
	   __debugging_nargv_real,__debugging_nargv);

    for (i = 0; i < __debugging_nargv; i++) {
      printf("argv[%2d] \"%s\"\n",i,(char *)__debugging_argv[i]);
    }
  } else {
    kprintf("real number of argvs %d, saved: %d\n",
	   __debugging_nargv_real,__debugging_nargv);
    for (i = 0; i < __debugging_nargv; i++) {
      kprintf("[%d:%s]",i,(char *)__debugging_argv[i]);
    }
    kprintf("\n");
  }
#endif
}

