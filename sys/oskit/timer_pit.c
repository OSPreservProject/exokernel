/*
 * Copyright (c) 1996-1998 University of Utah and the Flux Group.
 * All rights reserved.
 * 
 * The following are the copyrights and redistribution conditions that apply
 * to this portion of the OSKit software.  To arrange for alternate terms,
 * contact the University at csl-dist@cs.utah.edu or +1-801-585-3271.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that:
 * 1. These copyright, permission, and disclaimer notices are retained
 *    in all source files and reproduced in accompanying documentation.
 * 2. The name of the University may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 3. Redistributions in any form must be accompanied by information on
 *    how to obtain complete source code both for this software and for any
 *    accompanying software that uses this software.  The source code must
 *    either be included in the distribution or be available at no more than
 *    the cost of distribution plus a nominal fee, and must be freely
 *    redistributable under reasonable conditions.
 * 
 * THIS SOFWARE IS PROVIDED "AS IS" AND WITHOUT ANY WARRANTY, INCLUDING
 * WITHOUT LIMITATION THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THE COPYRIGHT HOLDER DISCLAIMS
 * ALL LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.  NO USE OF THIS SOFTWARE IS AUTHORIZED
 * EXCEPT UNDER THIS DISCLAIMER.
 */
/*
 * Timer interrupt support.
 * This contains helper functions for the fdev_timer interface.
 * This is in a separate file for the convenience of the Unix code
 * so it only has to override half the fdev_timer support.
 */

#include <oskit/dev/dev.h>
#include <oskit/debug.h>
#include <oskit/x86/pio.h>
#include <oskit/x86/proc_reg.h>
#include <oskit/x86/pc/pit.h>

static void(*handler)();

int
osenv_timer_pit_init(int freq, void (*timer_intr)())
{
	/*
	 * Initialize timer to interrupt at TIMER_FREQ Hz.
	 */
	pit_init(freq);

	/*
	 * Install interrupt handler.
	 */
	/*	if (osenv_irq_alloc(0, timer_intr, 0, 0))*/
	if (osenv_irq_alloc(14, timer_intr, 0, 0))

		osenv_panic("osenv_timer_init: couldn't install intr handler");
	handler = timer_intr;

	return freq;
}


void
osenv_timer_pit_shutdown()
{
	osenv_irq_free(0, handler, 0);
}


int
osenv_timer_pit_read()
{
	int enb = osenv_intr_enabled();
	int value;

	osenv_intr_disable();
	value = pit_read(0);
	if (enb)
		osenv_intr_enable();
	return value;
}


