
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


#include <stdio.h>
#include <string.h>
#include <sys/defs.h>
#include <sys/sysinfo.h>
#include <sys/mmu.h>
#include <sys/syscall.h>
#include <sys/env.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/pmap.h>
#include <os/osdecl.h>
#include <exos/conf.h>
#include <exos/dyn_process.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <net/ae_net.h>
#include <fd/procaux.h>
#include <fd/proc.h>
#include <xuser.h>
#include <fcntl.h>
#include <endian.h>
#include <unistd.h>
#include <malloc.h>

#define dprintf if (0) slsprintf
#define d2printf slskprintf

#include <sls_stub.h>
#include "dyn_misc.h"
extern int main (int, char **, char **);
extern int __main (int, char **, char **);
extern char **environ;
extern int shit_bag;
extern struct proc_struct *__current;
extern int dougs_new_problematic_int;

extern int __returned_from_main;
int random_proc1(); 
int random_proc2();
int random_proc3();
int silly(int);
void fix_handlers();
void shared_print_current();

void shared_print_shit_bag();
void shared_modify_dougs_int(int i);

#define ekprintf if (0) kprintf

int post_dyn_libs(char **envp, char **argv, int argc, proc_info *dyn_ProcInfo,
		  char *__dyn_progname, u_int dyn__envid,struct Env *dyn__ce,
		  u_int dyn__brkpt, u_int dyn__stkbot, u_int dyn__stktop,
		  struct LocalSegmentInfo *LSI, char *dyn_Shr, char **nenv) {

  int ret;

  u_int randomseed;
  
  shared_print_current();
  kprintf("PREcurrent : 0x%x\n",__current);
  current = (struct proc_struct *) PROC_STRUCT;
  kprintf("POSTcurrent : 0x%x\n",__current);
  shared_print_current();
  current = (struct proc_struct *) (PROC_STRUCT + 1);
  kprintf("MESSEDcurrent : 0x%x\n",__current);
  shared_print_current();
  current = (struct proc_struct *) PROC_STRUCT;
  kprintf("GOODcurrent : 0x%x\n",__current);
  shared_print_current();

  kprintf("Pre_shit_bag (uninited) : %d\n",shit_bag);
  shit_bag = 666;
  kprintf("Pre_shit_bag (should be 666) : %d\n",shit_bag);
  UAREA.u_xsp = dyn__stktop;
 
  if (random_proc1() == 0)
    kprintf("False\n");
  kprintf("After fp1, shit_bag (should be 777) = %d\n",shit_bag);
  shared_print_shit_bag(); 
  fix_handlers();
  

  dougs_new_problematic_int = 666;
  kprintf("dougs_new_problematic_int = %d\n",dougs_new_problematic_int);
  shared_modify_dougs_int(777);
  kprintf("dougs_new_problematic_int = %d\n",dougs_new_problematic_int);
  

  SetProgramVars(dyn__envid,dyn__ce,dyn__brkpt,dyn__stkbot,dyn__stktop);


  SetShared(dyn_Shr);
  SetProcInfo(dyn_ProcInfo);
  SetLocalSegmentInfo(LSI);
  Set__progname(__dyn_progname);
  SetEnviron(nenv);
  environ = nenv;
  
  if (silly(10) != 20) 
    kprintf("  False\n");
  if (random_proc3() == 0) 
    kprintf("False\n");
  FDRegisterHandlers(); 

  SysVShmRegisterHandlers(); 


  CloseOnExecFD();
  kprintf("In post_dyn_libs6.3\n");
  ftable_init();
  kprintf("In post_dyn_libs6.4\n");
  PostExecInit();
  

  kprintf("In post_dyn_libs6.5\n");
  /* SeedRandomInit */  
  randomseed = 
    (u_int)(__envid) ^ (u_int)(__sysinfo.si_system_ticks) ^ (u_int)(__envid<<16)
    ^ (u_int)(__sysinfo.si_diskintrs) 
    ^ (u_int)(__sysinfo.si_loopback_packets<<16);
  kprintf("In post_dyn_libs2.1\n");
  srandom(randomseed);

  slskprintf("Done fixing environment\n");
  
  __main(argc,argv,envp);
  __returned_from_main = 0;	/* used in process.c */
  ret = main (argc, argv, envp);
  __returned_from_main = 1;
  return (ret);

  asm ("__callmain:");   /* Defined for the benefit of debuggers */
  return (1);
}

void fix_handlers() {
  UAREA.u_entepilogue = (u_int) xue_epilogue;
  UAREA.u_entprologue = (u_int) xue_prologue;
  UAREA.u_entfault = (u_int) xue_fault;
}


void ipc2bogus (u_int, u_int, u_int) __attribute__ ((regparm (3)));
void
ipc2bogus (u_int wanted_id, u_int was_id, u_int esp)
{
  char *a;
  a = (char *)malloc(70);
  strcpy(a,"$$ Env #");
  strcat(a,ui2s((unsigned int)geteid()));
  strcat(a,": bogus IPC2 wanted ");
  strcat(a,ui2s((unsigned int)wanted_id));
  strcat(a," got ");
  strcat(a,ui2s((unsigned int)was_id));
  strcat(a,"\n");
  slskprintf (a);
}
