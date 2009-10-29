
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
#include <stdio.h>
#include <string.h>
#include <sys/defs.h>
#include <sys/sysinfo.h>
#include <sys/mmu.h>
#include <sys/syscall.h>
#include <sys/env.h>
#include <sys/types.h>
#include <sys/pmap.h>
#include <os/osdecl.h>
#include <exos/conf.h>
#include <exos/dyn_process.h>
#include <exos/vm.h>	
#include <net/ae_net.h>
#include <fcntl.h>
#include <endian.h>
#include <unistd.h>
#include <malloc.h>

#define dprintf if (0) slsprintf
#define d2printf slskprintf
#define d3printf if (0) kprintf

#define DYNAMIC True
#undef geteid()
#define geteid() (dyn__envid)

#include <sls_stub.h>
#include "common.h"
#include "dyn_misc.h"

void start (void) asm ("start") __attribute__ ((noreturn));
extern void exit (int) __attribute__ ((noreturn));
extern int main (int, char **, char **);
extern int __main (int, char **, char **);
extern void LoadSharedLibs ();
extern proc_info *dyn_ProcInfo;
extern char *dyn_Shared;
/*
 * Program variables.  These must all be initialized, because in the
 * BSS they might cause a page fault before the fault handler is
 * setup.
 */
u_int dyn__envid = 0;                       /* Cached environment id */
struct Env *dyn__curenv = 0;                /* Current environment */
u_int dyn__brkpt = 0;                       /* Break point */
u_int dyn__stkbot = USTACKTOP-NBPG;         /* Lowest stack point */
u_int dyn__stktop = 0;                      /* Top of exception stack */

char **dyn_environ;		
static char *dyn_envp[UNIX_NR_ENV];
static char *dyn_argv[UNIX_NR_ARGV];
static int dyn_argc;
char *__dyn_progname = "";
struct Sysinfo *dyn_sip = 0;
Pte            *dyn_my_vpt = 0;
struct Ppage   *dyn_ppp = 0;


void __die (void) __attribute__ ((noreturn));
void
__die (void)
{
  slskprintf ("$$ I'm dying \n");
/*  for(;;) yield(-1); XXX remove me when panic ppage_remove is fixed */
  sys_env_free (0, geteid ());
  slskprintf ("$$ I'm dead, but I don't feel dead.\n");
  for (;;);
}

static inline void
setup_ash (void)
{
  extern u_char ash_bin[];
  extern const unsigned int ash_bin_size;
  extern u_int __ash_envid;
  extern void pkt_recv (void);
  extern void disk_done (void);
  extern char ash_end[];
  extern char ash_edata[];
  u_int va;

  if (sys_self_insert_pte (0, PG_P|PG_U|PG_W, NBPD-NBPG) < 0) {
    slskprintf ("couldn't make ash stack\n");
    __die ();
  }
  for (va = 2 * NBPG; va < 2 * NBPG + ash_bin_size; va += NBPG)
    if (sys_self_insert_pte (0, PG_P|PG_U|PG_W, va) < 0) {
      slskprintf ("couldn't insert ash pte's\n");
      __die ();
  }
  bcopy (ash_bin, (void *) (2*NBPG), ash_bin_size);
  for (; va < (u_int) ash_end; va += NBPG) {
    if (sys_self_insert_pte (0, PG_P|PG_U|PG_W, va) < 0) {
      slskprintf ("couldn't insert ash pte's\n");
      __die ();
    }
  }
  bzero (ash_edata, ((char *) va) - ash_edata);

  /* map uarea into ash area */
 if (sys_self_insert_pte (0, vpt[UADDR >> PGSHIFT], ASH_UADDR) < 0) {
    slskprintf ("couldn't insert ash pte's\n");
    __die ();  
  }

  /* ash region for network allocated in ae_eth_init */
  __ash_envid = dyn__envid;
  UAREA.u_ashsp = NBPD;
  UAREA.u_ashnet = (u_int) pkt_recv;
  UAREA.u_ashdisk = (u_int) disk_done;
  __dyn_progname = UAREA.name;
}

static void
pre_init_env (void)
{
  u_int va;
  int *iptr,offset;
  int envc;

  /* No reason for this to be in a function, except that gcc seems to
   * require it. */
  DEF_SYM (u, UADDR);
  DEF_SYM (__u_status, &UAREA.u_status);
  DEF_SYM (__u_entipc2, &UAREA.u_entipc2);
  DEF_SYM (__u_xesp, &UAREA.u_xsp);
  DEF_SYM (__u_ppc, &UAREA.u_ppc);
  DEF_SYM (__envs, UENVS);
  DEF_SYM (__ppages, UPPAGES);
  DEF_SYM (__sysinfo, SYSINFO);
#ifdef PROCESS_TABLE
  DEF_SYM (__u_pending_sigs, &UAREA.pending_sigs);
#endif
  DEF_SYM (__u_in_critical, &UAREA.u_in_critical);
  DEF_SYM (__u_interrupted, &UAREA.u_interrupted);
  DEF_SYM (__u_fpu, &UAREA.u_fpu);
  DEF_SYM (__u_next_timeout, &UAREA.u_next_timeout);
  DEF_SYM (__si_system_ticks, &__sysinfo.si_system_ticks);
  DEF_SYM (__bc, UBC);

  DEF_SYM (vpt, UVPT);
#define SRL(val, shamt) (((val) >> (shamt)) & ~(-1 << (32 - (shamt))))
  DEF_SYM (vpd, UVPT + SRL(UVPT, 10));

  dyn__envid = sys_geteid ();
  dyn__brkpt = ((u_int) &end + PGMASK) & ~PGMASK;
  UAREA.u_xsp = dyn__stktop;
  UAREA.u_entepilogue = (u_int) sls_xue_epilogue;
  UAREA.u_entprologue = (u_int) sls_xue_prologue;
  UAREA.u_entfault = (u_int) sls_xue_fault;
  UAREA.u_entipc1 = UAREA.u_entipc2 = 0;

  for (va = ((u_int) &edata & ~PGMASK); va < dyn__brkpt; va += NBPG)
    sys_self_insert_pte (0, PG_P | PG_U | PG_W, va);
  bzero (&edata, dyn__brkpt - (u_int) &edata);

  for (va = dyn__brkpt;; va += NBPG) {
    if (va >= UTOP
	|| (vpd[PDENO (va)] & (PG_P|PG_U)) != (PG_P|PG_U)
	|| !(vpt[PGNO (va)] & PG_P))
      break;
    bzero ((void *) va, NBPG);
  }

  setup_ash ();
  /* slskprintf ("This is environment 0x%x envidx %d\n", dyn__envid,envidx(dyn__envid)); */
  if (!(dyn__curenv = env_id2env (dyn__envid)))
    slskprintf ("$$ could not set dyn__curenv\n");
  if (envidx(dyn__envid) != 1 && envidx(dyn__envid) != 0) {
    /* not first process nor idle loop */
    /* copy argv */
#if 0
    do {
      unsigned char *cp;
      int i;
      slskprintf("&cu.u_argv_lengths;\n");
      cp = (char *)&UAREA.u_argv_lengths;
      for (i = 0; i <  UNIX_NR_ARGV*sizeof(int); i++) {
	slskprintf("%02x ",*cp++);
      }
      slskprintf("\n&cu.u_env_lengths;\n");
      cp = (char *)&UAREA.u_env_lengths;
      for (i = 0; i <  UNIX_NR_ENV*sizeof(int); i++) {
	slskprintf("%02x ",*cp++);
      }
      slskprintf("\n&cu.u_argv_space;\n");
      cp = (char *)&UAREA.u_argv_space;
      for (i = 0; i <  UNIX_ARGV_SIZE; i++) {
	slskprintf("%02x ",*cp++);
      }
      slskprintf("\n&cu.u_env_space;\n");
      cp = (char *)&UAREA.u_env_space;
      for (i = 0; i <  UNIX_ENV_SIZE; i++) {
	if ((int)*cp == 4) continue;
	slskprintf("%3d:%02x ",i,*cp++);
      }
      slskprintf("\n----\n");

    } while(0);
#endif

    for (iptr = &UAREA.u_argv_lengths[0], dyn_argc = 0, offset = 0;
	 *iptr != -1 && dyn_argc < UNIX_NR_ARGV; 
	 iptr++,dyn_argc++) {
      /* printf("%d len %d \"%s\"  ",dyn_argc,*iptr,&UAREA.u_argv_space[offset]); */
      dyn_argv[dyn_argc] = &UAREA.u_argv_space[offset];
      offset += *iptr;
    }
    dyn_argv[dyn_argc] = (char *)0;
    /* copy unix env */
    for (iptr = &UAREA.u_env_lengths[0], envc = 0, offset = 0;
	 *iptr != -1 && envc < UNIX_NR_ENV; 
	 iptr++,envc++) {
      /* printf("%d len %d \"%s\"  ",envc,*iptr,&UAREA.u_env_space[offset]); */
      dyn_envp[envc] = &UAREA.u_env_space[offset];
      offset += *iptr;
    }
    dyn_envp[envc] = (char *)0;
    dyn_environ = (char **)&dyn_envp[0];
  } else {
    /* first process or idle loop */
    dyn_envp[0] = (char *)0;
    dyn_environ = (char **)&dyn_envp[0];
    dyn_argc = 1;
    dyn_argv[0] = "FIRST";
    dyn_argv[1] = (char *)0;
  }
}

/* for backtracing code. do not touch */

void __start_start () {}

void
start (void)
{
  /* Leave some room for an exception stack at the top of the real
   * stack */
  char xstack[1024];   
  int ret;
  
  dyn__stktop = (u_int) xstack + sizeof (xstack);
  UAREA.u_fpu = 0;
  d3printf("PreInitEnv\n");
  pre_init_env ();
  d3printf("PreExecInit\'ing...\n");
  PreExecInit ();
  slskprintf("...Got out of PreExecInit\n");
  dyn_sip = &__sysinfo;
  slskprintf("...Set dyn_sip\n");
  dyn_my_vpt = vpt;
  slskprintf("...Set dyn_my_vpt\n");
  dyn_ppp = __ppages;
  slskprintf("...About to load dyn libs\n");
  
#ifdef stupid_gcc
  if (&_DYNAMIC) { __load_rtld(&_DYNAMIC); }
#else
  { volatile caddr_t x = (caddr_t)&_DYNAMIC;
  if (x) { __load_rtld(&_DYNAMIC);}}
#endif
  slskprintf("------------------\n");

  ret = post_dyn_libs(dyn_envp, dyn_argv, dyn_argc, dyn_ProcInfo,
		      __dyn_progname, dyn__envid, dyn__curenv,
		      dyn__brkpt, dyn__stkbot, dyn__stktop,
		      dyn_LocalSegmentInfo, dyn_Shared, dyn_environ);
  exit(ret);
}

/* for back stack backtracing code -- we stop backtracing if we see an eip
   between start and __start_end...

   DO NOT MOVE THIS. */

void __end_start () {}

void sls_ipc2bogus (u_int, u_int, u_int) __attribute__ ((regparm (3)));
void
sls_ipc2bogus (u_int wanted_id, u_int was_id, u_int esp)
{
  slskprintf("$$ Env #");
  slskprintd(         geteid());
  slskprintf(                 " : bogus IPC2 wanted ");
  slskprintd(                                       wanted_id);
  slskprintf(                                                " got ");
  slskprintd(                                                      was_id);
  slskprintf(                                                            "\n");
}


#include "common.c"

