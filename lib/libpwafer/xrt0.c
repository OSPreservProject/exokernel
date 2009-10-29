
void __start ();
static void *start_text_addr = __start;

#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/sysinfo.h>
#include <xok/sys_syms.h>
#include <sys/types.h>
#include <pwafer/pwafer.h>

u_int __brkpt = 0;

extern int main();
extern void __exit();

void 
__start () 
{
  char xstack[2048];   

  /* No reason for this to be in a function, except that gcc seems to
   * require it. */
  DEF_SYM (UAREA, UADDR);

  DEF_SYM (__u_xesp, &UAREA.u_xsp);
  DEF_SYM (__u_ppc, &UAREA.u_ppc);
  DEF_SYM (__u_donate, &UAREA.u_donate);
  DEF_SYM (__u_in_critical, &UAREA.u_in_critical);
  DEF_SYM (__u_interrupted, &UAREA.u_interrupted);
  
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
  UAREA.u_entipc1 = UAREA.u_entipc2 = 0;
  UAREA.u_entfault = (u_int) xue_fault;
  UAREA.u_entprologue = (u_int) xue_prologue;
  UAREA.u_entepilogue = (u_int) xue_epilogue;
  UAREA.u_fpu = 0;
  UAREA.pid = sys_geteid();

  __brkpt = ((u_int) &end + PGMASK) & ~PGMASK;

  /* UAREA.u_ppc is inited to 0 in parent so if it is none zero it means that
     the quantum expired since the beginning of the program and we need to
     yield before our slice gets yanked */

  if (UAREA.u_ppc) 
    yield(-1);

  pw_init ();

  /* and off we go */
  main ();

  __exit();

  start_text_addr = __start;
}


