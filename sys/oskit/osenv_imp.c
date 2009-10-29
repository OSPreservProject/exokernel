/*
 * ExoKernel include files
 */

#include <xok/types.h>
#include <xok/defs.h>
#include <xok/picirq.h>
#include <xok/printf.h>

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
#include <xok_include/stdarg.h>
#include <xok/pctr.h>
#include <xok/sys_syms.h>

/*
 * Oskit include files 
 */
#include <dev/dev.h>
#include <oskit/machine/types.h>
#include <oskit/types.h>
#include <oskit/error.h>
#include <dev/device.h>

#include "pit_param.h"

/*
 * there is no header with this function, and
 * I need it.
 */
void kprintf (const char *fmt, va_list ap);

/*
 * Display or otherwise record a message from this driver set.
 * The log priorities below indicate its general nature.
 */
void osenv_log(int priority, const char *fmt, ...)
{
  /* use a vprintf in the kernel */
  va_list args;

  va_start(args, fmt);  
  osenv_vlog(priority, fmt, args);
  va_end(args);
}

void osenv_vlog(int priority, const char *fmt, void *vl)
{
  /* 
   * again, some vprintf in the exo kernel 
   * priorities not supported right now.
   */
  
  kprintf(fmt, vl);
}

/*
 * This function is called if there's an error in the driver set
 * that's so critical we just have to bail immediately.
 * Note that this may not necessarily terminate the whole OS;
 * it might just terminate the driver set it's called from.
 */
void osenv_panic(const char *fmt, ...)
{
  /* maps directly to the exokernel's panic function... */
  va_list args;
  va_start(args, fmt);
  osenv_panic(fmt, args);
}

void osenv_vpanic(const char *fmt, void *vl)
{
  /* again, just call panic. */
  /*  char buf[100];
  char *p;
  int bufpos;

  for(bufpos = 0, p = fmt;
	  bufpos < 100 && *p; 
	  bufpos, p++)
	{
	  
	}
  */
  printf("panic message: ");
  kprintf(fmt, vl);
  panic(fmt);
}

/*
 * make sure this is initilized before use.  It was
 * created to set the jiffies variable when the oskit
 * gets initialized.
 */
u_long exokern_get_jiffies_start()
{
  /*
   * get the number of micro seconds the processor
   * has been running from the processor itself.
   *
   * quite easy to have a divide by zero error here,
   * if MHZ is not inited.
   */ 
  long long usecs; 
  printf("called set jiffies\n");
  printf("mhz = %d\n",  si->si_mhz);
  usecs = rdtsc() / si->si_mhz; 


  /*
   * now, use the rate to convert from micro seconds
   * to ticks.  This is the number of ticks that  have
   * occured since the processor started running.
   */
  return  (usecs / (quad_t) si->si_rate);; 
}
