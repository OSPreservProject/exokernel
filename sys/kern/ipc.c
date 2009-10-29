
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

#include <xok/env.h>
#include <xok/apic.h>
#include <xok/ipc.h>
#include <xok/ipi.h>
#include <xok/cpu.h>
#include <xok/capability.h>
#include <xok/kerrno.h>
#include <xok/sys_proto.h>
#include <xok/mplock.h>
#include <xok/pctr.h>


/* SYNCHRONIZED - benjie, 3/14/99 */


#define TRAPRET(v) \
  {                  \
    utf->tf_eax = v; \
    return;          \
  }


/*
 * Kernel IPC entry point for IPC1 and IPC2.  On entry:
 *
 *      %edi - Destination environment ID
 *      %eax - Argument 1
 *      %edx - Argument 2
 *      %ecx - Argument 3
 *      %ebx - Number of extra args (Argument 4 for ipc2)
 *      %esi - Argument 5
 *      (optional for ipc1 for %ebx != 0) address of args on top of stack
 *
 * ipc1 can have optional args copied via the stack. And the kernel blocks
 * further ipc's to the destination until the desination does an ipc2 or calls
 * sys_allowipc1. ipc2 does not have the optional args via stack.  And the
 * kernel does not block further ipc2's after an ipc2. ipc2 also unblocks ipc1
 * for the current environment if it was blocked.  
 */

static inline void
ipc (const u_int trapno) 
  __XOK_SYNC(localizes e)
{
  struct Env *e;
  register struct Uenv *up;
  register u_int eip;
  int s = 0;
  static u_long ipc_etfes;
  int r;
  int *pargs;

  if (trapno == T_IPC2 || trapno == T_IPC2S)
  {
    /* reallow caller's ipc1's if they were temporarily blocked */
    if (curenv->env_allowipc1 == XOK_IPC_TEMP_BLOCKED)
      curenv->env_allowipc1 = XOK_IPC_ALLOWED;
  }

#ifdef ASH_ENABLE 
  if (ash_va) 
    TRAPRET(-E_BAD_ENV);
#endif /* ASH_ENABLE */

  if (!(e = env_id2env (utf->tf_edi, &r)))
    TRAPRET(r);

  if (e->env_status != ENV_OK || !(up = e->env_u)) 
    TRAPRET(-E_BAD_ENV);


#ifdef __SMP__
  /* mark e so no one else will run it */
  if (env_localize(e) < 0)
  {
#if 0
    printf("PCT: env %d is running on another CPU, not available\n", 
	   e->env_id);
#endif
    TRAPRET(-E_ENV_RUNNING);
  }
#endif

  switch (trapno) {
  case T_IPC1S:
    s = 1;
    /* cascade */
  case T_IPC1:

    if (e->env_allowipc1 != XOK_IPC_ALLOWED) 
    {
#ifdef __SMP__
      env_release(e);
#endif
      TRAPRET(-E_IPC_BLOCKED);
    }

    if (utf->tf_ebx > U_MAX_EXTRAIPCARGS) 
    {
#ifdef __SMP__
      env_release(e);
#endif
      TRAPRET(-E_INVAL);
    }

    if (utf->tf_ebx > 0)
    {
      copyin((void*)utf->tf_esp, &pargs, sizeof(pargs));
      copyin(pargs, up->u_ipc1_extra_args, utf->tf_ebx * sizeof(u_int));
    }
 
    eip = up->u_entipc1;
    e->env_allowipc1 = XOK_IPC_TEMP_BLOCKED;
    break;
  
  case T_IPC2S:
    s = 1;
    /* cascade */
  case T_IPC2:
    if (e->env_allowipc2 != XOK_IPC_ALLOWED) 
    {
#ifdef __SMP__
      env_release(e);
#endif
      TRAPRET(-E_IPC_BLOCKED);
    }

    eip = up->u_entipc2;
    break;
  }
    
  UAREA.u_status -= s;
  if (UAREA.u_status < 0) UAREA.u_status = 0;

  /* wake up client */
  up->u_status++;
  /* set up the caller ID */
  utf->tf_edi = curenv->env_id;

  curenv->env_tf.tf_esp = utf->tf_esp;
  UAREA.u_ppc = utf->tf_eip;

#ifdef __SMP__
  if (e != curenv) env_force_release(curenv);
#endif

  /* set up upcall point */
  e->env_tf.tf_eip = eip;
  ipc_etfes = (u_long) &e->env_tf.tf_es;

  check_rtc_interrupt(e->env_u);

  env_ctx_switch(e);
  
  /* make sure TS bit in cr0 is set so that using the FPU will
     cause a trap */
  {
    u_int r = rcr0();
    if (!(r & CR0_TS)) lcr0(r | CR0_TS);
  }

  asm volatile ("\tmovl %0,%%esp\n"
		"\tpopal\n"
		"\tmovl %1,%%esp\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\tiret"
		:: "g" (utf),
		"m" (ipc_etfes)
		: "memory");
} 


void
ipc1 (void)
{
  ipc (T_IPC1);
}

void
ipc1s (void)
{
  ipc (T_IPC1S);
}

void
ipc2 (void)
{
  ipc (T_IPC2);
}

void
ipc2s (void)
{
  ipc (T_IPC2S);
}

void
sys_set_allowipc1(u_int sn, int value)
{
  if (curenv) curenv->env_allowipc1 = value;
}

void
sys_set_allowipc2(u_int sn, int value)
{
  if (curenv) curenv->env_allowipc2 = value;
}


/* 
 * Fast yield path: does not try to evaluate wk predicate, just wake up the
 * environment. Also does not change the environment's u_status.
 */

void 
fastyield (void)
  __XOK_SYNC(localizes e)
{
  struct Env* e;
  u_int envid = UAREA.u_donate;
  int r;

  UAREA.u_donate = -1;
  
  if (!(e = env_id2env (envid, &r)))
    sched_runnext();

  if (e != curenv)
  {
#ifdef __SMP__
    if (e->env_status != ENV_OK || env_localize(e) < 0)
#else
    if (e->env_status != ENV_OK)
#endif
    {
      if (e->env_status != ENV_OK) 
	printf("env dead? going to scheduler\n");
      sched_runnext();
    }
    
    env_save_tf(); 

#ifdef __SMP__
    env_force_release(curenv);
#endif
  }

  else
    env_save_tf(); 

  env_run(e);
}

