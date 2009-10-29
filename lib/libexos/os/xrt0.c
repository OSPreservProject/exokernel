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
#include <xok/defs.h>
#include <xok/mmu.h>
#include <xok/syscall.h>
#include <xok/scheduler.h>
#include <exos/osdecl.h>
#include <exos/conf.h>
#include <exos/uidt.h>
#include <xok/env.h>
#include <exos/process.h>
#include <xok/sysinfo.h>
#include <sys/types.h>
#include <xok/sys_ucall.h>

#include <exos/shm.h>
#include <exos/bufcache.h>
#include <exos/kprintf.h>
#include <exos/fpu.h>
#include <exos/net/ae_net.h>
#include <exos/callcount.h>
#include <exos/pager.h>
#include <exos/ptrace.h>
#include <exos/vm.h>		/* for PG_SHARED */
#include <exos/vm-layout.h>
#include <fd/fdstat.h>		/* for proctime */
#include <exos/cap.h>
#include <exos/page_replacement.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <exos/exec.h>
#include <sys/exec.h>

void start (void) asm ("__start") __attribute__ ((noreturn));
extern void exit (int) __attribute__ ((noreturn));
extern int __main (int, char **, char **);

#ifdef EXOPROFILE
extern void monstartup (u_long, u_long);
extern u_int etext; 
extern void rtc_setup(void);
#endif


/*
 * Program variables.  These must all be initialized, because in the
 * BSS they might cause a page fault before the fault handler is
 * setup.
 */
u_int __envid = 0;                       /* Cached environment id */
struct Env *__curenv = 0;                /* Current environment */
u_int __brkpt = 0;                       /* Break point */
u_int __stkbot = USTACKTOP-NBPG;         /* Lowest stack point */
u_int __xstktop = 0;                     /* Top of exception stack */

char **environ;		

int __returned_from_main;
char *__progname = "";

struct _exos_exec_args *__eea = (struct _exos_exec_args*)
     (USRSTACK - (ALIGN(sizeof(struct ps_strings)) +
		  ALIGN(sizeof(struct _exos_exec_args))));

/* this has to be initialized so that it will fall in the data segment
   rather than the bss. The initial code of each process fills this
   in and then calls __start below which zeros our bss which would
   overwrite the value written here. */

int (*__main_trampoline)(int, char **, char **) = (void *)0x77;

#if 0
static void print_argv(int argc,char **argv) {
    int i;
    printf("argc: %d\n",argc);
    for (i = 0; i < argc; i++) {
	printf("argv[%d] = %08x \"%s\"\n",i,(int)argv[i], argv[i]);
    }
    printf("argv[argc] is %sequal to NULL\n",(argv[argc] == 0) ? "" : "not ");
}
#endif

void __die (void) __attribute__ ((noreturn));
void
__die (void)
{
  ProcessFreeQuanta(__envid);
  sys_env_free (0, __envid);
  sys_cputs ("$$ I'm dead, but I don't feel dead.\n");
  for (;;);
}

#ifdef ASH_NET
void
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
  char buffer[80];
  int ret;
  u_int num_completed;
  static int ash_already_setup = 0;

  if (ash_already_setup) return;
  ash_already_setup = 1;

  /* create stack for ash */
  if (_exos_self_insert_pte (0, PG_P|PG_U|PG_W, NBPD-NBPG,
			     ESIP_DONTPAGE, NULL ) < 0) {
    sys_cputs ("couldn't make ash stack\n");
    __die ();
  }

  /* map the ash code and data into the ash region */
  num_completed = 0;
  if (_exos_self_insert_pte_range(CAP_ROOT, &vpt[PGNO(ash_bin)],
				  PGNO(PGROUNDUP(ash_bin_size)), 2*NBPG,
				  &num_completed, 0, NULL) < 0) {
    sys_cputs ("couldn't remap ash's code and data\n");
    __die ();
  }
  /* XXX-make the ash code and data copy-on-write */
  if (sys_self_mod_pte_range(CAP_ROOT, PG_W, 0, 2*NBPG,
			     PGNO(PGROUNDUP(ash_bin_size))) < 0) {
    sys_cputs ("couldn't remap ash's code and data\n");
    __die ();
  }

  /* zero the ash's bss region */
  for (va = PGROUNDUP((u_int)ash_edata); va < (u_int) ash_end; va += NBPG) {
    if ((ret = _exos_self_insert_pte (0, PG_P|PG_U|PG_W, va,
	 ESIP_DONTPAGE, NULL)) < 0) {
      sprintf (buffer, "ash_end (%p) = %p %x %d\n\n", &ash_end, ash_end, va,
	       ret);
      sys_cputs (buffer);
      sys_cputs ("couldn't insert ash bss pages\n");
      __die ();
    }
  }
  bzero (ash_edata, ((char *) va) - ash_edata);

  /* map uarea into ash area */
  StaticAssert (sizeof (struct Uenv) <= NBPG);
  if (_exos_self_insert_pte (0, vpt[UADDR >> PGSHIFT], ASH_UADDR, ESIP_DONTPAGE, NULL) < 0) {
    sys_cputs ("couldn't insert ash's uarea\n");
    __die ();  
  }

  /* ash region for network allocated in ae_eth_init */
  __ash_envid = __envid;

  UAREA.u_ashsp = NBPD;
  UAREA.u_ashnet = (u_int) pkt_recv;
  UAREA.u_ashdisk = (u_int) disk_done;
}
#endif

static void
init_env (void)
{
  u_int va;
  char buffer[80];
  int r;

  /* No reason for this to be in a function, except that gcc seems to
   * require it. */
  DEF_SYM (UAREA, UADDR);
  DEF_SYM (__u_status, &UAREA.u_status);
  DEF_SYM (__u_entipc2, &UAREA.u_entipc2);
  DEF_SYM (__u_xesp, &UAREA.u_xsp);
  DEF_SYM (__u_ppc, &UAREA.u_ppc);
#ifdef __HOST__
  DEF_SYM (__u_cs, &UAREA.u_cs);
  DEF_SYM (__u_ss, &UAREA.u_ss);
  DEF_SYM (__u_ds, &UAREA.u_ds);
  DEF_SYM (__u_es, &UAREA.u_es);
  DEF_SYM (__u_eflags, &UAREA.u_eflags);
#endif
  DEF_SYM (__u_donate, &UAREA.u_donate);
  DEF_SYM (__u_yield_count, &UAREA.u_yield_count);
  DEF_SYM (__u_epilogue_count, &UAREA.u_epilogue_count);
  DEF_SYM (__u_epilogue_abort_count, &UAREA.u_epilogue_abort_count);
  DEF_SYM (__u_ptrace_flags, &UAREA.u_ptrace_flags);
  DEF_SYM (__envs, UENVS);
  DEF_SYM (__ppages, UPPAGES);
  DEF_SYM (__sysinfo, SYSINFO);
  DEF_SYM (__u_in_critical, &UAREA.u_in_critical);
  DEF_SYM (__u_interrupted, &UAREA.u_interrupted);
  DEF_SYM (__u_fpu, &UAREA.u_fpu);
  DEF_SYM (__u_next_timeout, &UAREA.u_next_timeout);
  DEF_SYM (__si_system_ticks, &__sysinfo.si_system_ticks);
  DEF_SYM (__bc, UBC);
  DEF_SYM (__xr, UXN);
  DEF_SYM (__xn_free_map, UXNMAP);
  DEF_SYM (__u_revoked_pages, &UAREA.u_revoked_pages);

  DEF_SYM (vpt, UVPT);
#define SRL(val, shamt) (((val) >> (shamt)) & ~(-1 << (32 - (shamt))))
  DEF_SYM (vpd, UVPT + SRL(UVPT, 10));

  __envid = sys_geteid ();
  __brkpt = ((u_int) &end + PGMASK) & ~PGMASK;

  if (!(__curenv = env_id2env (__envid, &r)))
    printf ("$$ could not set __curenv\n");

  /* XXX -- our parent (except when parent is kernel) zereos this */
  if (envidx(__envid) == 1 || envidx(__envid) == 0) {
    for (va = ((u_int) &edata & ~PGMASK); va < __brkpt; va += NBPG) {
      int ret;
      if ((ret = _exos_self_insert_pte (0, PG_P | PG_U | PG_W, va, 0, NULL)) != 0) {
	sprintf ((char *)buffer, "ret = %d\n", ret);
	kprintf ((char *)buffer);
	break;
      }     
    }
    bzero (&edata, __brkpt - (u_int) &edata); 
  }
  for (va = __brkpt;; va += NBPG) {
    if (va >= UTOP
	|| (vpd[PDENO (va)] & (PG_P|PG_U)) != (PG_P|PG_U)
	|| !(vpt[PGNO (va)] & PG_P))
      break;
    bzero ((void *) va, NBPG);
  }
#ifdef ASH_NET
  setup_ash();
#endif

  uidt_initialize();
}

void __start_start () {}

void __start (void *args) {
  int ret, argc;
  char **argv, **envp, *ap;

  __xstktop = UAREA.u_xsp;
  UAREA.u_fpu = 0;
  if (envidx(sys_geteid()) == 1 || envidx(sys_geteid()) == 0) {
    __vm_alloc_region(USTACKTOP - (__RESERVED_PAGES+2)*NBPG,
		      (__RESERVED_PAGES+1)*NBPG, 0,
		      PG_P | PG_W | PG_U);
    __xstktop -= (__RESERVED_PAGES+1)*NBPG;
    UAREA.u_xsp -= (__RESERVED_PAGES+1)*NBPG;
    asm volatile ("subl %0, %%esp\n" : : "i" ((__RESERVED_PAGES+1)*NBPG));
    asm volatile ("subl %0, %%ebp\n" : : "i" ((__RESERVED_PAGES+1)*NBPG));
    __eea->eea_prog_fd = -1;
    __eea->eea_reserved_pages = __RESERVED_PAGES;
    __eea->eea_reserved_first = (char*)(USTACKTOP - (__RESERVED_PAGES+1)*NBPG);
  }
  UAREA.u_reserved_pages = __eea->eea_reserved_pages;
  init_env ();

  __call_count_init();

  // if (envidx(__envid) != 1 && envidx(__envid) != 0) {
  if (args != 0L)
  {
    args = (void*)((u_int)args - sizeof(int));
    argc = ((int*)args)[0];
    argv = (char**)&(((int*)args)[1]);
    envp = argv + argc + 1;
  } else {
    /* no argument passed in, probably init program or something */
    argc = 1;
    argv = (char *[]) { "NULL", 0 };
    envp = (char *[]) { 0 };
  }
  environ = envp;
  __debug_copy_argv(argc, argv);

  {				
    if ((ap = argv[0]))
      if ((__progname = strrchr(ap, '/')) == NULL)
	__progname = ap;
      else
	++__progname;
  }

  _exos_fpu_init();
#ifdef EXOPROFILE
  monstartup(UTEXT, (u_int)&etext);
  rtc_setup();
#endif 
  /* ran here after environ has been set up */
  fd_stat_init();
  START(misc,proctime);
  START(misc,init);
  ProcessStartup ();
  _exos_pager_init();
  _exos_page_request_register();
  /* do ptrace setup later if we're in a shared library */
  if ((u_int)__start < SHARED_LIBRARY_START) _exos_ptrace_setup();
  STOP(misc,init);
  START(misc,init2);
  __main(argc, argv, envp);
  __returned_from_main = 0;	/* used in process.c */
  ret = __main_trampoline(argc, argv, envp);
  STOP(misc,init2);
  __returned_from_main = 1;
  exit(ret);
}

void __end_start () {}
