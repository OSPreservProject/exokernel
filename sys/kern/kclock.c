
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

/* The Run Time Clock and other NVRAM access functions that go with it. */
/* The run time clock is hard-wired to IRQ8. */

#define __KCLOCK_MODULE__

#include <xok/sysinfo.h>
#include <xok/types.h>
#include <i386/isa/nvram.h>
#include <xok/picirq.h>
#include <xok/env.h>
#include <xok/kclock.h>
#include <xok/printf.h>

u_int rtc_current_rega;

int clock_init(time_t base) {
  /* 100 ticks per second */
  SYSINFO_ASSIGN(si_rate,10000); // si->si_rate = 10000; 
  startrtclock(); /* init the scheduler's clock (not actually part of
		     the real time clock), and check the BIOS diagnostic
		     byte that's part of the RTC's nvram */
  cpu_initclocks(); /* init the real time clock's periodic interrupt */
  inittodr(base); /* read the real time clock's time of day */
  return 0;
}

void
mhz_init(void) {
  calibrate_cyclecounter();
  SYSINFO_ASSIGN(si_mhz, pentium_mhz);
}

void 
rtc_intr (int trapno)
  __XOK_SYNC(UAREA only modified when localized)
{
  int stat;

  irq_eoi (8);

  /* MUST read to acknowledge the interrupt */
  stat = mc146818_read(NULL, MC_REGC);
  if (stat & MC_REGC_PF) 
    {
      if ((!UAREA.u_pendrtc) && UAREA.u_entrtc)
	{
	  tfp tf;
	  u_int *esp;
	  u_int tt_eip;
	  u_int old_pfm = page_fault_mode;
	  tfp_set (tf, trapno, tf_edi);

	  tt_eip = tf->tf_eip; /* trap time eip */

	  /* copy interrupt stack frame on to user stack */
	  page_fault_mode = PFM_PROP;
	  tf->tf_esp -= 12;
	  esp = trup ((u_int *)tf->tf_esp);
	  esp[2] = tf->tf_eflags;
	  esp[1] = tf->tf_cs;
	  esp[0] = tf->tf_eip;
	  page_fault_mode = old_pfm;

	  tf->tf_eip = UAREA.u_entrtc;
	  UAREA.u_pendrtc = 1; 

	  env_upcall();
	}
    }
}

#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

