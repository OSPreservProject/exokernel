
/*
 * Copyright MIT 1999
 */

#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/pmap.h>

#include <vos/vm-layout.h>
#include <vos/proc.h>
#include <vos/cap.h>
#include <vos/vm.h>
#include <vos/kprintf.h>


#define PPAGE_EFFICIENT_COW

fault_handler_t fault_handlers[NUM_FAULT_HANDLERS];

int
fault_handler_add(vos_fault_handler_t h, u_int va, u_int size)
{
  int i;

  for(i=0; i<NUM_FAULT_HANDLERS; i++)
  {
    if (fault_handlers[i].h == 0)
    {
      fault_handlers[i].h = h;
      fault_handlers[i].va = va;
      fault_handlers[i].size = size;
      return 0;
    }
  }
  return -1;
}

int
fault_handler_remove(u_int va)
{
  int i;

  for(i=0; i<NUM_FAULT_HANDLERS; i++)
  {
    if (fault_handlers[i].h == 0 &&
	fault_handlers[i].va == va)
    {
      fault_handlers[i].h = 0L;
      return 0;
    }
  }
  return -1;
}



/* remove COW bit from pte, if any, and tag on PG_W bit */
static inline Pte 
uncow_pte (Pte pte) 
{
  pte &= ~PG_COW;
  pte |= PG_W;
  return pte;
}


/* COW fault handler */
static int
do_cow_fault (u_int va, Pte pte, u_int eip) 
{
#ifdef PPAGE_EFFICIENT_COW
  u_int orig_ppn = pte >> PGSHIFT;

  /* if we're the only one refing this page, we don't really need
     to copy it */

  if (__ppages[orig_ppn].pp_refcnt == 1 && __ppages[orig_ppn].pp_buf == NULL) 
  {
    if (sys_self_mod_pte_range (CAP_USER, PG_W, PG_COW, va, 1) < 0)
      return -1;

    /* check if condition is still true */
    if (__ppages[orig_ppn].pp_refcnt == 1 && __ppages[orig_ppn].pp_buf == NULL)
      return 0;

    /* condition failed, back off */
    if (sys_self_mod_pte_range(CAP_USER, PG_COW, PG_W, va, 1) < 0)
      return -1;
  }
#endif

  if (vm_self_insert_pte 
      (CAP_USER, PG_P|PG_U|PG_W, COW_TEMP_PG, 0, NULL) < 0)
    return -1;

  bcopy ((char *)(va & ~PGMASK), (char *)(COW_TEMP_PG), NBPG);
  pte = (vpt[(COW_TEMP_PG) >> PGSHIFT] & ~PGMASK) | (pte & PGMASK);

  if (vm_self_insert_pte (CAP_USER, uncow_pte (pte), va, 0, NULL) < 0)
    return -1;

  if (vm_self_unmap_page (CAP_USER, COW_TEMP_PG) < 0)
    return -1;

  return 0;
}


/* this determines the boundry between a segfault and stack expansion */
#define MAX_STACK_GROW 4096 /* some programs can use a lot of stack!! */
                            /* we should try to map more than one page at */
                            /* a time, in cc1 it can touch at the very bottom */
                            /* of stack and still be valid  */

/* default fault handler */
int
page_fault_handler (u_int va, u_int errcode, u_int eflags, u_int eip,
		    u_int esp)
{
  u_int page = va & ~PGMASK;
  int ret, i;
  extern void __start();
  extern void exit(int);

  if (UAREA.u_in_pfault > 5) 
  { 
    static int done_before = 0; 
    if (!done_before) { 
      sys_cputs("<< Recursive page fault to at least 6 levels. " 
	        "Continuing... >>\n"); 
      done_before = 1; 
    } 
  }
  
  UAREA.u_in_pfault++;

  /* caution: in pmap, u_xsp is set to 0 before kernel upcalls to here. if a
   * page fault occurs before u_xsp is reset (below), kernel will use esp
   * instead of u_xsp as the exception stack. If this fault keeps on occuring
   * and u_xsp is never reset, eventually we will cause a stack fault in
   * kernel. */

  /* text and initialized data fault */
  if (va > (u_int)&__start && !isvamapped(va) && va < PGROUNDUP((u_int)&edata)) 
  {
    if ((ret = sys_self_insert_pte (CAP_USER, PG_U | PG_W | PG_P, page)) < 0) 
    {
      kprintf ("$$ page_fault_handler(text): self_insert_pte failed %d\n",ret);
      goto fatal;
    }
    goto leave;
  }

  /* COW fault */
  if ((vpd[PDENO(va)] & (PG_P|PG_U)) == (PG_P|PG_U)) 
  {
    Pte pte = vpt[va >> PGSHIFT];    
    if (((pte & (PG_COW | PG_P | PG_U)) == (PG_COW | PG_P | PG_U)) && 
	(errcode & FEC_WR)) 
      /* if pte has PG_COW tag and fault caused by a write */
    {
      if (do_cow_fault (va, pte, eip) < 0)
      {
	/* can't use kprintf here because it will likely be using COW pages
	 * anyways */
	sys_cputs("COW fault handler failed\n");
	goto fatal;
      }
      goto leave;
    }
  } 

  /* bss fault */
  if ((va < PGROUNDUP((u_int)&end)) && (va >= PGROUNDUP((u_int)&edata))
      && !isvamapped(va)) 
  {
    if ((ret = sys_self_insert_pte (CAP_USER, PG_U | PG_W | PG_P, page)) < 0) {
      kprintf ("$$ page_fault_handler(bss): self_insert_pte failed %d\n",ret);
      goto fatal;
    }
    bzero((void*)page, NBPG);
    goto leave;
  }

  /* stack fault */
  if (page >= ((__stkbot-MAX_STACK_GROW*NBPG) & ~PGMASK) && page < USTACKTOP) 
  {
    if ((ret = sys_self_insert_pte (CAP_USER, PG_U|PG_W|PG_P, page)) < 0) {
      kprintf ("$$ page_fault_handler(stack): self_insert_pte failed %d\n",ret);
      goto fatal;
    }
    if (page < __stkbot)
      __stkbot = page;
    goto leave;
  }

  /* if it's not the stack causing the fault, then we can start using the full
     regular stack instead of the little exception stack */
  /* asm volatile ("movl %%esp, %0\n" : "=g" (UAREA.u_xsp)); */
  UAREA.u_xsp = __xstktop;
  asm volatile ("movl %0, %%esp\n" : : "r" (esp));  

  /* see if we can find a fault handler for this */
  for(i=0; i<NUM_FAULT_HANDLERS; i++)
  {
    if (fault_handlers[i].h != 0 &&
	va >= fault_handlers[i].va &&
	va < (fault_handlers[i].va+fault_handlers[i].size))
    {
      if ((ret = (fault_handlers[i].h)(va, errcode, eflags, eip)) < 0)
      {
        kprintf ("$$ page_fault_handler(stack): register fault handler failed %d\n",ret);
        goto fatal;
      }
      goto leave;
    }
  }

fatal:
  /* put your page fault handling code here... for now we just die */
  kprintf("env %d: %s: fatal fault, dying: va 0x%x, eip 0x%x\n",
      getpid(),UAREA.name,va,eip);
  
  /* revert to the little exception stack */
  asm volatile ("movl %0, %%esp\n" : : "r" (UAREA.u_xsp));  
  _exit (-1);
  
leave:
  UAREA.u_in_pfault--;
  return 1;
}

