
void __start();
static void *start_text_addr = __start;

#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/sysinfo.h>
#include <xok/sys_syms.h>

#include <sys/types.h>
#include <wafer/wafer.h>


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
extern int main ();

/* This is the first code a process runs by virtue of it being linked in at
 * the start of the program. Seeing how libwafer applications use the
 * EXO_MAGIC executable format, this means it needs to be at 0x800020. The
 * makefile specifies this starting address, but you need to make sure a few
 * more things:
 * 
 * 1. __start is the only routine in this file
 * 2. there are no static data defined in this file (static variables,
 *    strings, etc).
 */

void 
__start () 
{
  char xstack[2048];   
  int r;

  /* No reason for this to be in a function, except that gcc seems to
   * require it. */
  DEF_SYM (UAREA, UADDR);
  DEF_SYM (__u_status, &UAREA.u_status);
  DEF_SYM (__u_entipc2, &UAREA.u_entipc2);
  DEF_SYM (__u_xesp, &UAREA.u_xsp);
  DEF_SYM (__u_ppc, &UAREA.u_ppc);
  DEF_SYM (__u_donate, &UAREA.u_donate);
  DEF_SYM (__u_yield_count, &UAREA.u_yield_count);
  DEF_SYM (__u_epilogue_count, &UAREA.u_epilogue_count);
  DEF_SYM (__u_epilogue_abort_count, &UAREA.u_epilogue_abort_count);
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

  DEF_SYM (vpt, UVPT);
#define SRL(val, shamt) (((val) >> (shamt)) & ~(-1 << (32 - (shamt))))
  DEF_SYM (vpd, UVPT + SRL(UVPT, 10));

  UAREA.u_entipc1 = UAREA.u_entipc2 = 0;
  UAREA.u_xsp = (u_int) xstack + sizeof (xstack);
  UAREA.u_entfault = (u_int) xue_fault;
  UAREA.u_entprologue = (u_int) xue_prologue;
  UAREA.u_entepilogue = (u_int) xue_epilogue;
  UAREA.u_fpu = 0;

  __xstktop = UAREA.u_xsp;
  __envid = sys_geteid ();
  __brkpt = ((u_int) &end + PGMASK) & ~PGMASK;
  __curenv = env_id2env (__envid, &r);

  /* UAREA.u_ppc is inited to 0 in parent so if it is none zero it means that
     the quantum expired since the beginning of the program and we need to
     yield before our slice gets yanked */

  if (UAREA.u_ppc) 
    yield(-1);

  /* and off we go */
  main ();

  __die();
  start_text_addr = __start;
}


