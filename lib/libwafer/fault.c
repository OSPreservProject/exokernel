
#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/pmap.h>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <wafer/wafer.h>

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

  /* put your page fault handling code here... for now we just die */
  sys_cputs("page fault\n");

  /* bss fault */
  if ((va < PGROUNDUP((u_int)&end)) && (va >= PGROUNDUP((u_int)&edata))
      && !isvamapped(va)) {
    if ((ret = sys_self_insert_pte (0, PG_U | PG_W | PG_P, page)) < 0) {
      sys_cputs ("$$ page_fault_handler(bss): self_insert_pte failed\n");
      __die ();
    }
    bzero((void*)page, NBPG);
    goto leave;
  }

  /* heap fault */
  if (va < __brkpt && !isvamapped(va) && va >= PGROUNDUP((u_int)&end)) {
    if ((ret = sys_self_insert_pte (0, PG_U | PG_W | PG_P, page)) < 0) {
      sys_cputs ("$$ page_fault_handler(heap): self_insert_pte failed\n");
      __die ();
    }
  }

  /* stack fault */
  if (page >= ((__stkbot-MAX_STACK_GROW*NBPG) & ~PGMASK) && page < UTOP) {
    if ((ret = sys_self_insert_pte (0, PG_U|PG_W|PG_P, page)) < 0) {
      sys_cputs ("$$ page_fault_handler(stack): self_insert_pte failed\n");
      __die ();
    }
    if (page < __stkbot)
      __stkbot = page;
    goto leave;
  }

  /* if it's not the stack causing the fault, then we can start using the full
     regular stack instead of the little exception stack */
  asm volatile ("movl %%esp, %0\n" : "=g" (UAREA.u_xsp));  
  asm volatile ("movl %0, %%esp\n" : : "r" (esp));  

  /* put your page fault handling code here... for now we just die */
  sys_cputs("page fault dying\n");
  __die ();
  
  /* revert to the little exception stack */
  asm volatile ("movl %0, %%esp\n" : : "r" (UAREA.u_xsp));  

leave:
  return 1;
}
