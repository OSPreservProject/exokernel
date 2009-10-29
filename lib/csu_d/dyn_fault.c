
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


#include <xuser.h>
#include <exos/dyn_process.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mmu.h>
#include <signal.h>
#include <os/osdecl.h>
#include <string.h>
#include <exos/vm.h>
#include <sys/env.h>
#include <sls_stub.h>

int __sls_in_assert = 0;

extern u_int dyn__stkbot;

/* this determines the boundry between a segfault and stack expansion */
#define MAX_STACK_GROW 512 /* HBXX - gcc can use a lot of stack!! */
                           /* we should try to map more than one page at */
                           /* a time, in cc1 it can touch at the very bottom of */
			   /* stack and still be valid  */


void
sls_page_fault_handler (u_int va, u_int errcode, u_int eflags, u_int eip)
{
  u_int page = va & ~PGMASK;
  int ret;


  UAREA.u_in_pfault++;

  if ((vpd[PDENO(va)] & (PG_P|PG_U)) == (PG_P|PG_U)) {
    Pte pte = vpt[va >> PGSHIFT];
    if ((pte & PG_COW) && (errcode & FEC_WR)) {	
      sys_cputs("$$ sls_page_fault_handler: encountered cow fault\n");
      exit (-1);
    }
  } 

  if (page >= ((dyn__stkbot-MAX_STACK_GROW*NBPG) & ~PGMASK) && page < UTOP) {
    if ((ret = sys_self_insert_pte (0, PG_U|PG_W|PG_P, page)) < 0) {
      sys_cputs ("$$ page_fault_handler: self_insert_pte failed\n");
      slskprintf ("__stkbot = ");
      slskprinth (dyn__stkbot);
      slskprintf (" ret = ");
      slskprintd (ret);
      slskprintf ("eip = ");
      slskprinth (eip);
      slskprintf ("\n");
      exit (-1);
    }
    if (page < dyn__stkbot)
      dyn__stkbot = page;
    goto leave;
  }
  /* sleep(1); */
  slsprintf("$$ pfault_handler : used signal_vec :");
  slsprintf ("Pid #");
  slsprintd(getpid());
  slsprintf(" at pc 0x");
  slsprinth(eip);
  slsprintf(" ref\'d addr 0x");
  slsprinth(va);
  slsprintf("\n");
  exit(-1);
leave:
  UAREA.u_in_pfault--;
}

