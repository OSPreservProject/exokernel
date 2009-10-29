
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


#include <sys/types.h>
#include <assert.h>

#ifdef EXOPC
#include <xok/sysinfo.h>
#include <exos/cap.h>
#include <exos/uwk.h>
#include <exos/tick.h>
#else
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "xio_tcp_waitfor.h"


#ifdef EXOPC
int xio_tcp_wkpred (struct tcb *tcb, struct wk_term *t)
{
   int next = 0;

   u_quad_t current_time = __ticks2usecs (__sysinfo.si_system_ticks);
   u_int timeout = tcb->timer_retrans - current_time;

   /* timer_retrans is in microseconds if it is set */

   if (tcb->timer_retrans) {
     if (tcb->timer_retrans < current_time)
       timeout = 0;
     //timeout = 250000;
     next += wk_mkusleep_pred (&t[next], timeout);
   }

   return (next);
}
#endif


/* timeout, in micro-seconds, is max time to wait for change 
   timeout is a absolute time in the future that we want to wakeup
*/

void xio_tcp_waitforChange (xio_nwinfo_t *nwinfo, u_quad_t timeout)
{
#ifdef EXOPC
#define XIO_PRED_LEN	64
   struct wk_term t[XIO_PRED_LEN];
   int predlen;
   u_int delta;

   u_quad_t current_time = __ticks2usecs (__sysinfo.si_system_ticks);
   delta = timeout - current_time;

   /* don't sleep if we're past or almost at our
      timeout. */

   //kprintf ("timeout = %qd current_time = %qd\n", timeout, current_time);

   predlen = xio_net_wrap_wkpred_packetarrival (nwinfo, t);

   if (predlen != -1) {
     if (timeout) {
       if (timeout < current_time)
	 delta = 0;
       predlen = wk_mkop (predlen, t, WK_OR);
       //delta = 250000;
       predlen += wk_mkusleep_pred (&t[predlen], delta);
     }
     assert ((predlen > 0) && (predlen <= XIO_PRED_LEN));
     wk_waitfor_pred (t, predlen);
   }
#else
	/* do a blocking select on the fd that is nwinfo->ringid, */
	/* with max time equal to the timeout time                */
   fd_set fds;
   struct timeval tv;
   int timeout = ((tcb) && (tcb->timer_retrans));
   int ret;

   if (nwinfo->ringid < 0) {
      return;
   }
   FD_ZERO (&fds);
   FD_SET (nwinfo->ringid, &fds);
   if (timeout) {
     assert (0); /* timer_retrans is now in usecs */
      tv.tv_sec = max (0, tcb->timer_retrans - time(NULL));
      tv.tv_usec = 0;
   }
#if 0
     else {
      tv.tv_sec = 10;
      tv.tv_usec = 0;
   }
   ret = select ((nwinfo->ringid + 1), &fds, NULL, NULL, &tv);
#else
   ret = select ((nwinfo->ringid + 1), &fds, NULL, NULL, ((timeout) ? &tv : NULL));
#endif
#if 0
   if (ret <= 0) {
      printf ("xio_tcp_waitfor timeout (ret %d, tcb %p, timeout %d)\n", ret, tcb, (int)((tcb) ? (tcb->timer_retrans - time(NULL)) : -1));
   }
#endif
#endif
}

