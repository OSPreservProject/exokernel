
#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/pmap.h>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <pwafer/pwafer.h>
#include <pwafer/kprintf.h>


/* this determines the boundry between a segfault and stack expansion */
#define MAX_STACK_GROW 512 /* HBXX - gcc can use a lot of stack!! */
                           /* we should try to map more than one page at */
                           /* a time, in cc1 it can touch at the very bottom */
                           /* of stack and still be valid  */
static int stop = 0;
int
page_fault_handler (u_int va, u_int errcode, u_int eflags, u_int eip,
		    u_int esp)
{
  u_int page = va & ~PGMASK;
  int ret;

  // kprintf("pf: %d %x %x\n", __envid, va, eip);

  /* bss fault */
  if ((va < PGROUNDUP((u_int)&end)) && (va >= PGROUNDUP((u_int)&edata))
      && !isvamapped(va)) {
    if ((ret = sys_self_insert_pte (0, PG_U | PG_W | PG_P, page)) < 0) {
      sys_cputs ("$$ page_fault_handler(bss): self_insert_pte failed\n");
      goto fatal;
    }
    bzero((void*)page, NBPG);
    goto leave;
  }

  /* heap fault */
  if (va < __brkpt && !isvamapped(va) && va >= PGROUNDUP((u_int)&end)) {
    if ((ret = sys_self_insert_pte (0, PG_U | PG_W | PG_P, page)) < 0) {
      sys_cputs ("$$ page_fault_handler(heap): self_insert_pte failed\n");
      goto fatal;
    }
  }

  /* stack fault */
  if (page >= ((__stkbot-MAX_STACK_GROW*NBPG) & ~PGMASK) && page < __stktop) 
  {
    if (page < __stkbot)
    {
      if (__stkbotlim != 0 && page < __stkbotlim)
      {
	sys_cputs("$$ page_fault_handler(stack): less than stkbotlim\n");
        goto fatal;
      }
      if ((ret = sys_self_insert_pte (0, PG_U|PG_W|PG_P, page)) < 0) {
        kprintf("$$ %d %d: page_fault_handler(stack): self_insert_pte failed\n",
	        __envid,ret);
        goto fatal;
      }
      __stkbot = page;
    }
    goto leave;
  }

  /* if it's not the stack causing the fault, then we can start using the full
     regular stack instead of the little exception stack */
  UAREA.u_xsp = __xstktop;
  asm volatile ("movl %0, %%esp\n" : : "r" (esp));

fatal:
  /* put your page fault handling code here... for now we just die */
  if (stop < 5) 
    kprintf("%d: page fault dying %x, %x\n",__envid,va,eip);
  if (va == 0) stop++;
  __die ();
  goto fatal;
  
leave:
  return 1;
}

