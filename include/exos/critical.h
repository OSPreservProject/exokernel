
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

#ifndef __EXOS_CRITICAL_H__
#define __EXOS_CRITICAL_H__

#include <exos/kprintf.h>
#include <exos/process.h>
#include <exos/locks.h>
#include <assert.h>

#ifdef __SMP__
#include <xok/sys_ucall.h>
#endif

static void inline EnterCritical () {
  asm ("cmpl $0, %1\n"		/* don't init if we're a nested EnterCritical */
       "\tjg 1f\n"
       "\tmovl $0, %0\n"	/* clear the interrupted flag */
       "\tcall 0f\n"	        /* save eip on first enter, using call to push eip on stack */
       "0:\n"
       "\tpopl %2\n"
       "1:\n"
       "\tincl %1\n"		/* inc count of nested of EnterCriticals */
       : "=m" (UAREA.u_interrupted), "=m" (UAREA.u_in_critical), "=m" (UAREA.u_critical_eip) :: "cc", "memory");
}

static void inline ExitCritical () {
  if (UAREA.u_in_critical <= 0) {
    kprintf ("ExitCritical: too many exits (in_critical %d)\n", UAREA.u_in_critical);
    assert (0);
  }

  asm ("decl %0\n"		/* dec critical ref count */
       "\tcmpl $0, %0\n"	/* if > 0 mean we're still in a critical pair */
       "\tjg 0f\n"
       "\tcmpl $0, %1\n"	/* we're the last exit critical so yield if needed */
       "\tje 0f\n"		/* not interrupted--no need to yield */
       "\tpushl %2\n"		/* setup our stack to look like an upcall */
       "\tpushfl\n"
       "\tjmp _xue_yield\n"	/* and go to yield */
       "0:\n"
       : "=m" (UAREA.u_in_critical): "m" (UAREA.u_interrupted), "g" (&&next)
       : "cc", "memory");
next:
}

/* how many nesting levels */
static int inline CriticalNesting(void) {
  return UAREA.u_in_critical;
}

#endif /* __EXO_CRITICAL_H__ */
