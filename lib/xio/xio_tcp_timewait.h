
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


#ifndef __XIO_TCP_TIMEWAIT_H__
#define __XIO_TCP_TIMEWAIT_H__

#include <sys/types.h>
#include <time.h>
#include "xio_tcp.h"
#include "xio_tcp_demux.h"

#ifdef EXOPC
#include <xok/sysinfo.h>
#include <xok/queue.h>
#endif

/* minimal information needed for a TCP PCB in the TIME_WAIT state */

typedef struct tmwait {
   XIO_TCP_DEMULTIPLEXING_FIELDS
   TAILQ_ENTRY(tmwait) link;
   LIST_ENTRY(tmwait) free_link;
   u_quad_t timeout;
   int demux_id;
   uint32 snd_next;
   uint32 rcv_next;
} timewaiter_t;

/* state for a set of minimized TIME_WAIT PCBs */

TAILQ_HEAD(Timewait_queue, tmwait);
LIST_HEAD(Timewait_free_list, tmwait);

typedef struct {
   struct Timewait_queue timewait_queue;
   struct Timewait_free_list timewait_free_list;
   int timewaiter_cnt;
   char * (*pagealloc)(void *info, int len);
   xio_dmxinfo_t dmxinfo;
} xio_twinfo_t;

#define xio_twinfo_size(hashsize) \
	((sizeof (xio_twinfo_t) - sizeof (xio_dmxinfo_t)) + \
	 (xio_dmxinfo_size (hashsize)))

/* xio_tcp_timewait.c prototypes */

void xio_tcp_timewait_init (xio_twinfo_t *twinfo, uint hashsize, char * (*pagealloc)(void *info, int len));
void xio_tcp_timewait_add (xio_twinfo_t *twinfo, struct tcb *tcb, int demux_id);
void xio_tcp_timewait_remove (xio_twinfo_t *twinfo, timewaiter_t *timewaiter);


static inline void xio_tcp_timewait_timeout (xio_twinfo_t *twinfo)
{
   timewaiter_t *done;
#ifdef EXOPC
   uint curtime = __sysinfo.si_system_ticks;
#else
   time_t curtime = time(NULL);
#endif

   while ((done = twinfo->timewait_queue.tqh_first) && 
	  (done->timeout <= curtime)) {

      xio_tcp_timewait_remove (twinfo, done);

   }
}

#endif /* __XIO_TCP_TIMEWAIT_H__ */

