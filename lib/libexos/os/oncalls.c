
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

#include <exos/process.h>
#include <stdio.h>
#include <errno.h>

/* hold callbacks that should be invoked when we exec */
static int NextOnExecHandler = 0;
#define MAX_ONEXEC_HANDLERS 16
static OnExecHandler OnExecHandlers[MAX_ONEXEC_HANDLERS];

static int NextOnForkHandler = 0;
#define MAX_ONFORK_HANDLERS 16
static OnForkHandler OnForkHandlers[MAX_ONFORK_HANDLERS];

static int NextOnExitHandler = 0;
#define MAX_ONEXIT_HANDLERS 16
static OnExitHandler OnExitHandlers[MAX_ONEXIT_HANDLERS];

int OnExec (OnExecHandler F) {
     if (NextOnExecHandler >= MAX_ONEXEC_HANDLERS) {
	  return (-1);
     }
     OnExecHandlers[NextOnExecHandler++] = F;
     return (0);
}

/* procedures called in the same order as called by OnExec FIFO */
int ExecuteOnExecHandlers(u_int k, int envid, int execonly) { 
  /* call handlers registered by OnExec */
  int Handler;

  for (Handler = 0; Handler < NextOnExecHandler; Handler++) {
    if (OnExecHandlers[Handler] (k,envid,execonly) == -1) {
      return (-1);
    }
  }
  return 0;
}

int OnFork (OnForkHandler F) {
     if (NextOnForkHandler >= MAX_ONFORK_HANDLERS) {
	  return (-1);
     }
     OnForkHandlers[NextOnForkHandler++] = F;
     return (0);
}

/* procedures called in the same order as called by OnFork FIFO */
int ExecuteOnForkHandlers(u_int k, int envid, int NewPid) { 
  /* call handlers registered by OnFork */
  int Handler;

  for (Handler = 0; Handler < NextOnForkHandler; Handler++) {
    if (OnForkHandlers[Handler] (k,envid,NewPid) == -1) {
      return (-1);
    }
  }
  return 0;
}

static int RegisterExitFunc (OnExitHandlerFunc F, void *arg, int type) {
     if (NextOnExitHandler >= MAX_ONFORK_HANDLERS) {
	  return (-1);
     }
     OnExitHandlers[NextOnExitHandler].handler = F;
     OnExitHandlers[NextOnExitHandler].atexit = type;
     OnExitHandlers[NextOnExitHandler].arg = arg;
     NextOnExitHandler++;
     return (0);
}

/* The onlyn real difference between ExosExitHandler and atexit is
   that ExosExitHandler is called if at all humanly possible when we
   exit while atexit is only called if we exit via exit(3) or
   returning from main. */

int ExosExitHandler (OnExitHandlerFunc F, void *arg) {
  return (RegisterExitFunc (F, arg, 0));
}

/* procedures called in the reverse order as called by OnExec LIFO */
static int ExecuteExitHandlers(int type) { 
  int Handler;
  for (Handler = NextOnExitHandler - 1; 
       Handler >= 0; Handler--) {
    if (OnExitHandlers[Handler].atexit == type)
      OnExitHandlers[Handler].handler (OnExitHandlers[Handler].arg);
  }
  return 0;
}

int ExecuteExosExitHandlers (int status) {
  return ExecuteExitHandlers (0);
}

int ExecuteAtExitHandlers () {
  return ExecuteExitHandlers (1);
}
