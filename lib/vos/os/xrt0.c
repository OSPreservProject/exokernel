
/*
 * Copyright MIT 1999
 */


/*
 * these must be in the front: we read start_text_addr in loader to determine
 * where to start!
 */
#ifdef __INIT_PROG__
void __start();
#else
void __start(int argc, char **argv, char **environ_vars);
#endif
static void *start_text_addr = __start;

#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/sysinfo.h>
#include <xok/sys_syms.h>
#include <sys/types.h>
#include <vos/proc.h>


/*
 * Program variables.  These must all be initialized, because in the
 * BSS they might cause a page fault before the fault handler is
 * setup.
 */
u_int __brkpt = 0;                       /* Break point */
u_int __stkbot = USTACKTOP-NBPG;         /* Lowest stack point */
u_int __xstktop = 0;                     /* Top of exception stack */
extern int main ();


void 
#ifdef __INIT_PROG__
__start ()
#else
__start (int argc, char *argv[], char **environ_vars) 
#endif
{
  char xstack[2048];   

#ifndef __INIT_PROG__
  extern char **environ;
#endif
  extern void __proc_start (void);
  extern void exit (int);
#ifdef __INIT_PROG__
  extern void __init_start (void);
#endif
  

  /* no reason for this to be in a function, except that gcc seems to
   * require it. */

#if 1
  DEF_SYM (UAREA, UADDR);
#endif
  DEF_SYM (__u_xesp, &(*(struct Uenv*)UADDR).u_xsp);
  DEF_SYM (__u_ppc, &(*(struct Uenv*)UADDR).u_ppc);
  DEF_SYM (__u_donate, &(*(struct Uenv*)UADDR).u_donate);
  DEF_SYM (__u_in_critical, &(*(struct Uenv*)UADDR).u_in_critical);
  DEF_SYM (__u_interrupted, &(*(struct Uenv*)UADDR).u_interrupted);

  DEF_SYM (__envs, UENVS);
  DEF_SYM (__ppages, UPPAGES);
  DEF_SYM (__sysinfo, SYSINFO);
  DEF_SYM (__si_system_ticks, &__sysinfo.si_system_ticks);
  DEF_SYM (__bc, UBC);
  DEF_SYM (__xr, UXN);
  DEF_SYM (__xn_free_map, UXNMAP);

  DEF_SYM (vpt, UVPT);
#define SRL(val, shamt) (((val) >> (shamt)) & ~(-1 << (32 - (shamt))))
  DEF_SYM (vpd, UVPT + SRL(UVPT, 10));


  UAREA.u_xsp = (u_int) xstack + sizeof (xstack);
  UAREA.u_entfault = (u_int) xue_fault;
  UAREA.u_entipc1 = UAREA.u_entipc2 = 0;
  UAREA.u_entprologue = (u_int) xue_prologue;
  UAREA.u_entepilogue = (u_int) xue_epilogue;
  UAREA.u_fpu = 0;
  
  UAREA.pid = sys_geteid();

  __xstktop = UAREA.u_xsp;
  __brkpt = ((u_int) &end + PGMASK) & ~PGMASK;
#ifndef __INIT_PROG__
  environ = environ_vars;
#endif

  /* UAREA.u_ppc is inited to 0 in parent so if it is none zero it means that
     the quantum expired since the beginning of the program and we need to
     yield before our slice gets yanked */

  if (UAREA.u_ppc) 
    env_yield(-1);

#ifdef __INIT_PROG__
  __init_start();
  __proc_start();
  main ();
#else
  __proc_start();

#ifdef __CPP_PROG__
  {
    extern void __do_global_ctors (void); 
    __do_global_ctors();
  }
#endif
  main (argc, argv);
#endif

  start_text_addr = __start; /* prevent compiler warning */
  exit(0);
}


