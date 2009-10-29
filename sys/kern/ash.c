
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


/* XXX - Don't deal with invalid ashsp */


#include <xok/sysinfo.h>  /* for pkt.h */
#include <xok/trap.h>
#include <xok/env.h>
#include <xok/syscall.h>
#include <xok/sys_proto.h>
#include <xok/pkt.h>
#include <xok/scheduler.h>
#include <xok/init.h>
#include <xok/printf.h>


static int ash_in_ipc;
struct Env *ash_last_env;

#if 0
static int ash_last_irq_mask;
#endif

static struct Trapframe ash_invocation_frame = {
  tf_es:
  GD_UD | 2, GD_UD | 2, 0x0, GD_UT | 2, 0x0 /* FL_IF */, 0x0, GD_UD | 2
};

static inline void
ash_setup (struct Env *e, int in_ash)
{
  ash_va = ASHVM + (NBPD * envidx (e->env_id));

  /* Put small ash segments in the global descriptor table */
  gdt[GD_UT>>3] = seg (STA_X | STA_R, ash_va, 0x3fffff, 2);
  gdt[GD_UD>>3] = seg (STA_W, ash_va, 0x3fffff, 2);

  if (! in_ash) {
#if 0
    /* Only allow timer interrupts */
    ash_last_irq_mask = irq_mask_8259A;
    irq_setmask (0xffff); /* XXX  - should be 0xfffe */
#endif
    ash_last_env = curenv;
  }
  curenv = e;
}

static inline struct Env *
ash_restore (void)
{
  gdt[GD_UT>>3] = seg (STA_X | STA_R, 0x0, ULIM - 1, 3);
  gdt[GD_UD>>3] = seg (STA_W, 0x0, ULIM - 1, 3);
  ash_va = 0;
#if 0
  irq_setmask_8259A (ash_last_irq_mask);
#endif
  if (ash_last_env && ash_last_env->env_status == ENV_OK
      && ash_last_env->env_u)
    return (curenv = ash_last_env);
  printf ("ash_restore: ash_last_env sucks.\n");
  return (curenv = 0);
}

void
ash_invoke (struct Env *e, u_int entry, u_int a1, u_int a2)
{
  if (e->env_status != ENV_OK || !e->env_u)
    return;

  /* Save away the state from the current environment */
  if (! ash_va)
    bcopy (utf, &curenv->env_tf, sizeof (struct Trapframe));

  ash_setup (e, ash_va);

  ash_invocation_frame.tf_eip = entry;
  ash_invocation_frame.tf_esp = e->env_u->u_ashsp;
  ash_invocation_frame.tf_eax = a1;
  ash_invocation_frame.tf_edx = a2;

  env_pop_tf (&ash_invocation_frame);
}

void
ipcas (u_int trapno)
{
  struct Env *e;
  struct Uenv *up;
  u_int entry;
  int r;

#if 1
  /* No IPC's out of an ash. */
  if (ash_va || !(e = env_id2env (utf->tf_edi, &r))
      || e->env_status != ENV_OK || !(up = e->env_u)
      || ! (entry = up->u_entipca)) {
    utf->tf_eax = -1;
    return;
  }
  bcopy (&utf->tf_es, &curenv->env_tf.tf_es, tf_size (tf_es, tf_hi));
#else
  e = &__envs[envidx(utf->tf_edi)];
  up = e->env_u;
  entry = up->u_entipca;
#endif
  ash_setup (e, 0);
  ash_in_ipc = 1;
#if 1
  utf->tf_eip = entry;
  utf->tf_esp = e->env_u->u_ashsp;
  utf->tf_cs = GD_UT | 2;
  utf->tf_ss = GD_UD | 2;
  utf->tf_es = GD_UD | 2;
  utf->tf_ds = GD_UD | 2;
  utf->tf_eflags &= ~FL_IF;
#endif
  env_upcall ();
}

void
ash_return (void)
{
  if (!ash_in_ipc) {
    xokpkt_consume ();

    if (! ash_restore ())
      sched_runnext ();
    env_pop_tf (&curenv->env_tf);
  }
  else {
    if (! ash_restore ())
      sched_runnext ();
    bcopy (&curenv->env_tf.tf_es, &utf->tf_es, tf_size (tf_es, tf_hi));
    ash_in_ipc = 0;
  }
}

void
sys_ash_test (u_int sn, int eid)
{
  struct Env *e;
  int r;

  printf ("   testing ash in env 0x%x\n", eid);
  if (! (e = env_id2env (eid, &r)))
    return;
  printf ("   Testing ashes...\n");
  ash_invoke (e, 2 * NBPG, 0,0);
}

#ifdef __ENCAP__
#include <xok/sysinfoP.h>  /* for pkt.h */
#endif

