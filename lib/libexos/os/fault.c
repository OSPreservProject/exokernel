
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


#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <xok/mmu.h>
#include <signal.h>
#include <exos/osdecl.h>
#include <string.h>
#include <exos/vm.h>
#include <stdlib.h>
#include <xok/env.h>

#include <string.h>
#include <exos/vm-layout.h>
#include <debug/i386-stub.h>
#include <exos/exec.h>
#include <exos/signal.h>
#include <exos/page_io.h>
#include <exos/uwk.h>
#include <exos/mallocs.h>
#include <exos/mregion.h>
#include <exos/cap.h>
#include <xok/bc.h>
#include <xok/sys_ucall.h>

int __in_assert = 0;

/* support for registering pagefault_handlers for specific VM regions */

typedef struct handler {
   struct handler *next;
   uint vastart;
   uint vaend;
   int (*handler)(uint,int);
} handler_t;

static handler_t *handlers = NULL;


int register_pagefault_handler (uint vastart, int len,
				int (*handler)(uint,int))
{
   handler_t *tmp = handlers;
   uint vaend = PGROUNDUP (vastart+len);

   vastart = PGROUNDDOWN (vastart);

   while (tmp) {
      if ((vastart >= tmp->vastart) && (vastart < tmp->vaend)) {
         return (-1);
      }
      if ((vaend >= tmp->vastart) && (vaend < tmp->vaend)) {
         return (-1);
      }
      if ((vastart < tmp->vastart) && (vaend >= tmp->vaend)) {
         return (-1);
      }
      tmp = tmp->next;
   }

   tmp = (handler_t *) __malloc (sizeof(handler_t));
   assert(tmp);
   tmp->vastart = vastart;
   tmp->vaend = vaend;
   tmp->handler = handler;
   tmp->next = handlers;
   handlers = tmp;

   /* kprintf ("(%d) pagefault_handler registered for %x -- %x\n", geteid(),
      tmp->vastart, tmp->vaend); */

   return (0);
}


static int registered_pagefault_handler (uint va, int errcode)
{
   handler_t *tmp = handlers;

   /* kprintf ("registered_pagefault_handler: handlers %p, va %x, "
      "errcode %x\n", handlers, va, errcode); */

   while (tmp) {
      if ((va >= tmp->vastart) && (va < tmp->vaend)) {
         return (tmp->handler (va, errcode));
      }
      tmp = tmp->next;
   }

   return (0);
}

void
__replinish(void)
{
  u_int i;
  static int __in_replinishment = 0;

  if (__in_replinishment) return;

  if (UAREA.u_reserved_pages == __eea->eea_reserved_pages) return;

  __in_replinishment = 1;

  for (i=0; i < __eea->eea_reserved_pages; i++) {
    if (!(vpt[PGNO((u_int)__eea->eea_reserved_first) + i] & PG_P)) {
      if (_exos_self_insert_pte(CAP_ROOT, PG_P | PG_U | PG_W,
				(u_int)__eea->eea_reserved_first + i * NBPG, 0,
				NULL) < 0) {
	sys_cputs("__replinish: can't get new page\n");
      } else {
	if (++UAREA.u_reserved_pages == __eea->eea_reserved_pages) break;
      }
    }
  }

  __in_replinishment = 0;
}

int
__remap_reserved_page(u_int va, u_int pte_flags)
{
  u_int i;

  for (i=0; i < __eea->eea_reserved_pages; i++) {
    if ((vpt[PGNO((u_int)__eea->eea_reserved_first) + i] & PG_P)) {
      if (_exos_self_insert_pte(CAP_ROOT,
				ppnf2pte(PGNO(vpt[PGNO((u_int)
						       __eea->
						       eea_reserved_first) +
						 i]), pte_flags),
				va, 0, NULL) < 0 ||
	  _exos_self_unmap_page(CAP_ROOT,
				(u_int)__eea->eea_reserved_first +
				i * NBPG) < 0) {
	sys_cputs("__remap_reserved_page: can't remap\n");
	return -1;
      }
      UAREA.u_reserved_pages--;
      return 0;
    }
  }

  sys_cputs("__remap_reserved_page: none left\n");
  return -1;
}

u_int __fault_test_enable = 0;
u_int __fault_test_va = 0;
/* this determines the boundry between a segfault and stack expansion */
#define MAX_STACK_GROW 512 /* HBXX - gcc can use a lot of stack!! */
                           /* we should try to map more than one page at */
                           /* a time, in cc1 it can touch at the very bottom */
                           /* of stack and still be valid  */
int
page_fault_handler (u_int va, u_int errcode, u_int eflags, u_int eip,
		    u_int esp)
{
  u_int page = va & ~PGMASK;
  int ret;
  u_int oldxsp;

  if (UAREA.u_in_pfault > 5) {
    static int done_before = 0;
    
    if (!done_before) {
      sys_cputs("<< Recursive page fault to at least 6 levels. "
		"Continuing... >>\n");
      done_before = 1;
    }
  }

  UAREA.u_in_pfault++;

  /* fault in the BSS */
  if (va >= PGROUNDUP((u_int)&edata) && va < (u_int)&end &&
      !(errcode & FEC_PR)) {
    if ((ret = _exos_self_insert_pte (0, PG_U|PG_W|PG_P, page, ESIP_URGENT,
				      NULL)) < 0) {
      sys_cputs ("page_fault_handler(bss): _exos_self_insert_pte failed\n");
      exit(-1);
    }
    bzero((char*)page, NBPG);
    goto leave;
  }

  /* copy-on-write fault */
  if ((vpd[PDENO(va)] & (PG_P|PG_U)) == (PG_P|PG_U)) {
    Pte pte = vpt[va >> PGSHIFT];    
    if (((pte & (PG_COW | PG_P | PG_U)) == (PG_COW | PG_P | PG_U)) &&
	(errcode & FEC_WR)) {
      do_cow_fault (va, pte, eip);
      goto leave;
    }
  } 

  /* stack fault */
  if (page >= ((__stkbot-MAX_STACK_GROW*NBPG) & ~PGMASK) && page < USTACKTOP) {
    if ((ret = _exos_self_insert_pte (0, PG_U|PG_W|PG_P, page, 0, NULL)) < 0) {
      sys_cputs ("$$ page_fault_handler(stack): self_insert_pte failed\n");
      kprintf ("__stkbot = %x USTACKTOP = %x ret = %d eip = %x\n", __stkbot,
	       USTACKTOP, ret, eip);
      {
	__stacktrace(0);
	__stacktrace(1);
      }
      exit (-1);
    }
    if (page < __stkbot)
      __stkbot = page;
    goto leave;
  }

  /* if it's not the stack causing the fault, then we can start using the full
     regular stack instead of the little exception stack */
  oldxsp = UAREA.u_xsp;
  asm volatile ("movl %%esp, %0\n" : "=g" (UAREA.u_xsp));  
  asm volatile ("movl %0, %%esp\n" : : "r" (esp));  

  /* __sbrk, mmap (including bss, sbrk, and demand loading), other faults */
  if (!mregion_fault((void*)va, errcode) &&
      !registered_pagefault_handler(va,errcode)) {
    static int segv_before = 0;
    static int debug_segv  = 0;

    if (is_being_ptraced()) {
      asm volatile ("movl %0, %%esp\n" : : "r" (UAREA.u_xsp));  
      asm("int $3");
    }

    if (__fault_test_enable && __fault_test_va == va) {
      __fault_test_enable = 0;
      if ((ret = _exos_self_insert_pte (0, PG_U|PG_W|PG_P, page, 0, NULL)) < 0) {
	sys_cputs("$$ page_fault_handler(fault test): "
		  "self_insert_pte failed\n");
	exit(-1);
      }
      goto leave;
    }

    if (__in_debugger) 
      {
	UAREA.u_in_pfault--;
	return 0;
      }

    if (!segv_before)
      {
	char c = 0;
	int i;
	segv_before = 1;
	kprintf("Segfault: (%s envid:%d pid:%d, eip:0x%x, va:0x%x)\n"
		"Start debugger [y/n]?", UAREA.name, __envid, getpid(),
		eip, va);
#define PROMPT_SECS 10
	for (i=0; i<10*PROMPT_SECS && c==0; i++) {
	  usleep(100000);  /* .1 secs */
	  sys_cgets (&c, 1);
	}
	kprintf("%c\n", c);
	debug_segv = (c=='y');
      }

    if (debug_segv)
      {
	UAREA.u_in_pfault--;
	return 0;
      }
    
    if (UAREA.u_in_pfault == 1) {
       __stacktrace (0);
       __stacktrace (1);
    }

    {
      siginfo_t si;
      struct sigcontext sc;

      sc.sc_eip = eip;
      sc.sc_esp = esp;
      sc.sc_eflags = eflags;
      sc.sc_trapno = 0x0e; /* page fault trap number */
      sc.sc_err = errcode; /* possibly incorrect use of field */
      si.si_signo = SIGSEGV;
      si.si_code = (errcode & FEC_PR) ? SEGV_ACCERR : SEGV_MAPERR;
      si.si_errno = 0;
      si.si_addr = (void*)va;
      si.si_trapno = 0x0e; /* page fault trap number */
      _kill(&si, &sc);
    }
    /* signal_vec[SIGSEGV].sa_handler (eip, va); */
  }

  /* revert to the little exception stack */
  asm volatile ("movl %0, %%esp\n" : : "r" (UAREA.u_xsp));  
  UAREA.u_xsp = oldxsp;

leave:
  __replinish();

  UAREA.u_in_pfault--;
  return 1;
}

