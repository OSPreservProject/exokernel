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


#include <xok/cpu.h>
#include <xok/smp.h>
#include <xok/mmu.h>



int sys_get_cpu_id(u_int sn) 
  __XOK_NOSYNC
  /* get the current CPU logical id */
{
  return cpu_id;
}



int sys_get_num_cpus(u_int sn) 
  __XOK_NOSYNC
  /* get the total number of CPUs */
{
  return get_cpu_count();
}



int
sys_cpu_revoke (u_int sn, u_int k, u_int cpu, int eid, int neweid) 
{
#ifdef __SMP__
  return 0;
#else
  return -E_UNAVAIL;
#endif
}


#ifdef __SMP__

void
mp_halt()
{
  int i;

  for(i=0; i<smp_cpu_count; i++)
  {
    if (i == cpu_id) continue;
    printf("sending halt signal to cpu %d via IPI\n", i);
    ipi_send_message(i, IPI_CTRL_HALT, 0);
  }
  delay(1000000);
}

#endif

void
sys_mp_shutdown(u_int sn)
{
#ifdef __SMP__
  mp_halt();
#endif
}


