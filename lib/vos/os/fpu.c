
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
#include <xok/sys_ucall.h>

#include <vos/locks.h>
#include <vos/proc.h>
#include <vos/fpu.h>
#include <vos/uidt.h>


struct Fpu_state vos_fpus;
u_int vos_fpu_used_ctxt = 0;


void 
fpu_print_saved_info()
{
  kprintf("*** %d",getpid());
  kprintf(" cw: 0x%x, sw: 0x%x, tw: 0x%x\n", 
	  vos_fpus.fpu_env.fpu_cw, 
	  vos_fpus.fpu_env.fpu_sw, 
	  vos_fpus.fpu_env.fpu_tw);
}


/* 
 * if haven't started using the FPU the record that we're using it and init
 * it. If already using it, then restore its state 
 */
static void fpu_device_handler(int trapno, u_int eip) 
{
  u_short cpu = sys_get_cpu_id();
  assert(cpu < 32);

  cpulock_acquire();

  /* clear the TS bit */
  sys_clts();

  if ((UAREA.u_fpu & (1<<cpu)) == 0)
  {
    asm volatile ("fninit\n");
    UAREA.u_fpu |= (1<<cpu);
  }
  else
  {
    asm volatile ("frstor _vos_fpus\n");
  }

  vos_fpu_used_ctxt = 1;

  cpulock_release();
}


void fpu_init() 
{
  uidt_register_cfunc(7, (u_int)fpu_device_handler);
}


