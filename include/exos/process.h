
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

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <exos/conf.h>
#include <exos/callcount.h>
#include <xok/env.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "xok/syscall.h"
static inline void yield (int envid) {
  UAREA.u_donate = envid;
  asm ("pushl %0\n"
       "\tpushfl\n"
       "\tjmp _xue_yield\n"
       :: "g" (&&next));
  goto next; /* get around warning about unused label */
next:
}

void ProcessStartup ();
void ProcessFreeQuanta(int);
void ProcessEnd (int ret, unsigned int epc) __attribute__ ((noreturn));

#ifdef PROCESS_TABLE

/* states a UAREA.child_state[n] can be in */
#define P_EMPTY 0
#define P_RUNNING 1
#define P_ZOMBIE 2

#undef PID_MIN
#define PID_MIN 2		/* pid 1 is reserved for init */
#undef PID_MAX
#define PID_MAX NENV

typedef struct proc_info {
  u_int nextPid;		/* next pid to assign to a process */
  u_int pidMap[NENV];		/* maps pid - > env */
  u_int refCount[NENV];		/* ref's to each pid */
} proc_info;

#define PROC_INFO_KEY 837212	/* shm key used to identify segment */

extern proc_info *ProcInfo;

int AllocateFreePid (u_int e);
void AddProcEntry (struct Uenv *, char *path, 
		   char *argv[], int pid, int parent);
void ProcTableExit (int ret, int status);
void ProcChangeEnv (int pid, int envid);
int GetChildSlot (int pid);
void ClearChildInfo (struct Uenv *u);
void RefPid (int pid);
void UnrefPid (int pid);
int GetRefCount (int pid);


#endif /* PROCESS_TABLE */

typedef int (*OnExecHandler)(u_int k, int envid, int execonly);
int OnExec (OnExecHandler F);
int ExecuteOnExecHandlers(u_int k,int envid, int execonly); 
/* only to executed by exec */

typedef int (*OnForkHandler)(u_int k, int envid, int NewPid);
int OnFork (OnForkHandler F);
int ExecuteOnForkHandlers(u_int k, int envid, int NewPid);

typedef void (*OnExitHandlerFunc)(void *arg); 

typedef struct {
  OnExitHandlerFunc handler;
  unsigned char atexit;
  void *arg;
} OnExitHandler;
int ExosExitHandler (OnExitHandlerFunc F, void *arg);
int ExecuteExosExitHandlers();
int ExecuteAtExitHandlers();

void __stacktrace (int tostdout);

/* DEBUGGING ARGV stuff, to show argvs at seg faults.   */
void __debug_copy_argv(int argc, char **argv);
void __debug_print_argv(int tostdoutorconsole);	/* 1 = stdout, 0 = console */

extern u_int pid2envid(pid_t pid); /* if returns 0, means failure */
extern pid_t envid2pid(u_int envid); /* if returns 0, means failure */

#endif /* __PROCESS_H__ */
