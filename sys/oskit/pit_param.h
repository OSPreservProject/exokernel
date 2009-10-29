/*
 * Copyright (c) 1997-1998 University of Utah and the Flux Group.
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
 *  Timer support specific to x86/pc programmable interval timer
 *  This header file contains all parameters
 */

#ifndef _DEV_X86_PIT_PARAM_H_
#define _DEV_X86_PIT_PARAM_H_

#include <xok/mmu.h>
#include <xok/picirq.h>

#include <oskit/time.h>
#include <oskit/x86/pc/pit.h>

/*
 * for now, we have a clock with these parameters
 */
#define TIMER_FREQ	100
#define TIMER_VALUE	(PIT_HZ / TIMER_FREQ)

/* one clock tick is 10M nanoseconds */
#define NANOPERTICK	10000000

#define TIMESPEC2TICKS(t)	((t)->tv_sec * TIMER_FREQ + \
		(((t)->tv_nsec + NANOPERTICK - 1) / NANOPERTICK))

#define TICKS2TIMESPEC(ti, ts) { 				\
	(ts)->tv_sec = (ti)/TIMER_FREQ;				\
	(ts)->tv_nsec = ((ti) % TIMER_FREQ) * NANOPERTICK;	\
    }


#endif /* _DEV_X86_PIT_PARAM_H_ */
