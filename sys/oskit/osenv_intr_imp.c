/* File: osenv_intr_imp.c
 * author: Michael Scheinholtz
 * email:  mseu@ece.cmu.edu
 *
 * decription:  
 *   This file implements the functions in the oskit
 * that deal with interrupts and interrupt handlers.
 * This implementation allows the oskit device driver
 * library to run inside the exokernel.
 */            

/*
 * ExoKernel include files
 */

#include <xok/types.h>
#include <xok/printf.h>
#include <xok/picirq.h> 
#include <xok/defs.h>
#include <xok/pmap.h>
#include <xok_include/assert.h>
#include <xok_include/errno.h>
#include <xok/sysinfo.h>
#include <xok/pkt.h>
#include <xok/bios32.h>
#include <machine/param.h>
#include <machine/endian.h>
#include <xok/ae_recv.h>
#include <xok/sys_proto.h>
#include <xok_include/string.h>
#include <xok/trap.h>

/*
 * Oskit include files 
 */
#include <dev/dev.h>
#include <oskit/machine/types.h>
#include <oskit/types.h>
#include <oskit/error.h>
#include <dev/device.h>


#define MAX_IRQ 0x10

/*
 * some macro's I'll fix later...
 */

#define save_cpu_flags(x) \
        asm volatile ("pushfl ; popl %0":"=g" (x): /* no input */ :"memory")
#define restore_cpu_flags(var) \
     	 asm volatile ("pushfl ; popl %0": /*no output */ : "g" (var) :"memory")

#define cli() asm volatile ("cli": : :"memory")
#define sti() asm volatile ("sti": : :"memory")

#define IF_FLAG_MASK 0x200   

/********************** External Global Variables ************************/

/*
 * this holds a pointer to the network structure assigned to us.
 */
extern struct network *oskit_network_ptr_gl;

/********************* Internal global variables *************************/

int oskit_irq_gl = -1;


/*
 ************ interrupt functions.
 */

/*
 * This allocation function should work because all that is 
 * passed to it is (irq, intr_handler, (void *) irq, flags);
 */
int osenv_irq_alloc(int irq, void (*handler)(void *), void *data, int flags)
{
  int (*func_ptr)(u_int);     /* an alternate type of function to beat casting */

  /*
   * Check parameters, first make sure irq
   * is valid, second check for null function.
   */

  if(irq < 0 || irq >= MAX_IRQ)
	{
	  return OSKIT_EINVAL;
	}

  if(handler == NULL)
	{
	  return -1;
	}
  
  func_ptr = (void *) handler;
  irq_sethandler(irq, func_ptr); 

  /*
	* going to attempt to unmask this irq level, hopefully this 
	* will work for all devices.
	*/
  irq_setmask_8259A (irq_mask_8259A & ~(1 << irq));
  delay(2500000);
  irq_eoi (irq);  

  /*
	* Make sure the global network structure has the correct irq.
	*/
  if(oskit_network_ptr_gl != NULL)
	 {
		/* 
		 * this must have been initialized in xok_net_interface
		 * oskit_init already, so its okay to set it.
		 */
		oskit_network_ptr_gl->irq = irq;
	 }else{
		/*
		 * this global variable was not ready, so I will set
		 * my irq. global variable
		 */
		oskit_irq_gl = irq;
	 }

  return 0;
}

void osenv_irq_free(int irq, void (*handler)(void *), void *data)
{
  printf("there is no free irq in the exokernel\n");

}

void osenv_irq_enable(int irq)
{
  u_int mask;
  u_int flags;
  
  mask = ~(1 << (irq & 7));
  save_cpu_flags(flags);
  cli();
  irq_setmask_8259A(mask);
  restore_cpu_flags(flags);
}

void osenv_irq_disable(int irq)
{
  /* mask IRQ */
  u_int mask;
  u_int flags;

  mask = 1 << (irq & 7);
  save_cpu_flags(flags);
  cli();
  irq_setmask_8259A(mask);
  restore_cpu_flags(flags);
}

int osenv_irq_pending(int irq)
{
  assert(0);

  return 0;
}


void osenv_intr_disable(void)
{
  /* 
   * both intr_enable and disable are 
   * null in the exokernel because interrupts are always off.
   */
 
}

void osenv_intr_enable(void)
{
  /* 
   * both intr_enable and disable are 
   * null in the exokernel because interrupts are always off.
   */
}

int osenv_intr_enabled(void)
{
  /* check interrupt status.*/
  u_long flags;
  save_cpu_flags(flags);
  return (flags & IF_FLAG_MASK) >> 9;
}



