/*
 * Copyright (c) 1996, 1998 University of Utah and the Flux Group.
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
 * Default implementation of osenv_sleep and friends.
 */ 

/* exokernel files */
#include <xok/printf.h>

#include <oskit/debug.h>
#include <oskit/dev/dev.h>

void
osenv_sleep_init(osenv_sleeprec_t *sr) 
{
	osenv_assert(sr);

	sr->data[0] = OSENV_SLEEP_WAKEUP;   /* Return code for osenv_sleep() */
	sr->data[1] = sr;	   /* Just a flag; anything non-null will do */
}

int
osenv_sleep(osenv_sleeprec_t *sr) 
{
	volatile osenv_sleeprec_t *vsr = sr;
	int was_enabled = osenv_intr_enabled();
	printf("inside osenv_sleep\n");
	osenv_assert(sr);

	/* We won't get anywhere if interrupts aren't enabled! */
	osenv_intr_enable();

	/* Busy-wait until osenv_wakeup() clears the flag in the sleeprec */
	while (vsr->data[1])
		/* NOTHING */;

	/* Restore the original interrupt enable state */
	if (!was_enabled)
		osenv_intr_disable();

	return (int) vsr->data[0];
}

void
osenv_wakeup(osenv_sleeprec_t *sr, int wakeup_status) 
{
	osenv_assert(sr);
	printf("inside osenv_sleep\n");
	panic("inside wakeup\n");
	sr->data[0] = (void *)wakeup_status;
	sr->data[1] = (void *)0;
}

