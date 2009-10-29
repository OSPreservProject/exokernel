
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

/*
 * Timer routines.
 *
 * Frans Kaashoek
 *
 * 11/29/95
 */

/*
 * TODO:
 *   - improve timers (do not poll; update them at each context switch).
 *   - do not use system calls
 */

#include <assert.h>
#include <string.h>
#include "tcp.h"
#include <exos/debug.h>
#include "global.h"
#ifdef EXOPC
#include "sys/tick.h"
#endif

extern struct tcb connection[];

#ifndef NOTIMERS
/* 
 * Compute the time in ticks on which the timer should expire.
 */
void
timer_set(struct tcb *tcb, unsigned int microseconds)
{
    unsigned int wait_ticks = microseconds / tcb->rate;

    tcb->flags &= ~TCB_FL_TIMEOUT;

    if (wait_ticks == 0) wait_ticks += 1; /* wait at least 1 tick */

    tcb->timer_retrans = wait_ticks + ae_gettick();
}


/* 
 * Check timer. If timer expired, call the entry point for timeouts in tcp code.
 */
int
timer_check(struct tcb *tcb)
{
    int t = ae_gettick();

    if (tcb->timer_retrans > 0 && t >= tcb->timer_retrans) {
	tcp_timeout(tcb);
	return(1);
    }
    return(0);
}
#else
void timer_set(struct tcb *tcb, unsigned int microseconds) {}
int timer_check(struct tcb *tcb) {}
#endif
