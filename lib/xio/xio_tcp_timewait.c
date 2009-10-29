
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
#include <sys/time.h>
#include <assert.h>

#include "xio_tcp.h"
#include "xio_tcp_timewait.h"
#include "xio_tcp_demux.h"

#ifdef EXOPC
#include <xok/sysinfo.h>
#include <exos/tick.h>

static int tcp_msl_ticks = 1000;
#endif


void xio_tcp_timewait_init (xio_twinfo_t *twinfo, uint hashsize, char * (*pagealloc)(void *info, int len))
{
   TAILQ_INIT (&twinfo->timewait_queue);
   LIST_INIT (&twinfo->timewait_free_list);
   twinfo->timewaiter_cnt = 0;
   twinfo->pagealloc = pagealloc;
#ifdef EXOPC
   tcp_msl_ticks = __usecs2ticks (2 * TCP_MSL * 1000000);
#endif
   xio_tcp_demux_init (&twinfo->dmxinfo, hashsize);
}

void xio_tcp_timewait_add (xio_twinfo_t *twinfo, struct tcb *tcb, int demux_id)
{
   timewaiter_t *new = twinfo->timewait_free_list.lh_first;
   int ret;

   if (new == NULL) {
      int i;
      new = (timewaiter_t *) twinfo->pagealloc (twinfo, 4096);
      for (i=0; i < (4096/sizeof(timewaiter_t)); i++) {
	LIST_INSERT_HEAD (&twinfo->timewait_free_list, &new[i], free_link);
      }
      new = twinfo->timewait_free_list.lh_first;
   }

   LIST_REMOVE (new, free_link);

   new->ipsrc = tcb->ipsrc;
   new->ipdst = tcb->ipdst;
   new->tcpboth = tcb->tcpboth;
   new->hashed = 0;
   new->snd_next = tcb->snd_next;
   new->rcv_next = tcb->rcv_next;
   new->demux_id = demux_id;
#ifdef EXOPC
   new->timeout = tcp_msl_ticks + __sysinfo.si_system_ticks;
#else
   assert (0);
   /* XXX -- change xio_tcpcommon TWtimeout to call time(NULL)
      rather than reading __sysinfo.si_system_ticks */

   new->timeout = time(NULL) + (2 * TCP_MSL);
#endif

   ret = xio_tcp_demux_addconn (&twinfo->dmxinfo, (struct tcb *) new, 0);
   assert (ret == 0);

   TAILQ_INSERT_TAIL (&twinfo->timewait_queue, new, link);
   
   twinfo->timewaiter_cnt++;
}


void xio_tcp_timewait_remove (xio_twinfo_t *twinfo, timewaiter_t *timewaiter)
{

   TAILQ_REMOVE (&twinfo->timewait_queue, timewaiter, link);

   xio_tcp_demux_removeconn (&twinfo->dmxinfo, (struct tcb *) timewaiter);

   LIST_INSERT_HEAD (&twinfo->timewait_free_list, timewaiter, free_link);

   twinfo->timewaiter_cnt--;
}

