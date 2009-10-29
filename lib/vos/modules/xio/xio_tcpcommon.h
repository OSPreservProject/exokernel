
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


#ifndef __XIO_TCPCOMMON_H__
#define __XIO_TCPCOMMON_H__

#include "xio_tcp.h"
#include "xio_tcp_handlers.h"
#include "xio_tcp_demux.h"
#include "xio_tcp_timewait.h"
#include "xio_tcp_waitfor.h"
#include "xio_tcpbuffer.h"
#include "xio_net_wrap.h"
#include <assert.h>

static inline int xio_tcpcommon_sendPacket (struct tcb *tcb, int voloff)
{
   int ret = 0;
   if (tcb->send_ready) {
	/* GROK -- replace this with a few us of retry and the toss it?? */
      while ((ret = xio_net_wrap_send (tcb->netcardno, (struct ae_recv *) &tcb->snd_recv_n, (tcb->snd_recv_r0_sz + voloff)))) ;
      tcb->send_ready = 0;
	/* GROK -- if using this to measure round-trip, really need to know */
	/* actual send time (from driver) and actual receive time...        */
      tcb->send_time = xio_tcp_read_clock ();
   }
   return (ret);
}


static inline struct tcb * xio_tcpcommon_findlisten (xio_dmxinfo_t *dmxinfo, char *packet, xio_nwinfo_t *nwinfo, int *netcardnoP)
{
   int ret = (xio_tcp_synpacket(packet)) ? xio_net_wrap_getnetcardno (nwinfo, packet) : -1;
   struct tcb *listentcb = (ret >= 0) ? xio_tcp_demux_searchlistens (dmxinfo, packet) : NULL;
   if (netcardnoP) { *netcardnoP = ret; }
   return (listentcb);
}


static inline void xio_tcpcommon_handleRandPacket (char *packet, xio_nwinfo_t *nwinfo)
{
   struct tcb tmptcb;
       /* compute netcardno from received packet's dst_ether addr */
   tmptcb.netcardno = xio_net_wrap_getnetcardno (nwinfo, packet);
   if (tmptcb.netcardno >= 0) {
      tmptcb.snd_recv_n = 1;
      tmptcb.snd_recv_r0_data = &tmptcb.snd_buf[ETHER_ALIGN];
      tmptcb.snd_recv_r0_sz = XIO_EIT_DATA;
      tmptcb.send_ready = 0;
      xio_tcp_handleRandPacket (&tmptcb, packet);
      xio_tcpcommon_sendPacket (&tmptcb, 0);
   }
}


static inline int xio_tcpcommon_handleTWPacket (xio_twinfo_t *twinfo, char *packet, xio_nwinfo_t *nwinfo)
{
   timewaiter_t *twtcb = (timewaiter_t *) xio_tcp_demux_searchconns (&twinfo->dmxinfo, packet);
   if (twtcb) {
      struct tcb tmptcb;
      //kprintf ("TWinfo: ipsrc %x ipdst %x portsrc %d portdst %d\n",
      //	 ntohl(twtcb->ipsrc), ntohl(twtcb->ipdst), ntohs(twtcb->tcpsrc), 
      //	 ntohs(twtcb->tcpdst));
	/* compute netcardno from received packet's dst_ether addr */
      tmptcb.netcardno = xio_net_wrap_getnetcardno (nwinfo, packet);
      assert (tmptcb.netcardno >= 0);
      tmptcb.snd_recv_n = 1;
      tmptcb.snd_recv_r0_data = &tmptcb.snd_buf[ETHER_ALIGN];
      tmptcb.snd_recv_r0_sz = XIO_EIT_DATA;
      tmptcb.state = TCB_ST_TIME_WAIT;
      tmptcb.snd_next = twtcb->snd_next;
      tmptcb.rcv_next = twtcb->rcv_next;
      tmptcb.send_ready = 0;
      xio_tcp_handlePacket (&tmptcb, packet, 0);
      xio_tcpcommon_sendPacket (&tmptcb, 0);
      if (xio_tcp_closed (&tmptcb)) {
         if (twtcb->demux_id) {
            int ret = xio_net_wrap_freedpf (twtcb->demux_id);
            assert (ret == 0);
         }
         xio_tcp_timewait_remove (twinfo, twtcb);
      }
   }
   return (twtcb != NULL);
}


static inline void xio_tcpcommon_TWtimeout (xio_twinfo_t *twinfo)
{
   timewaiter_t *done;
   u_quad_t curtime = __sysinfo.si_system_ticks;

   while ((done = twinfo->timewait_queue.tqh_first) && 
	  (done->timeout <= curtime)) {
     if (done->demux_id) {
       int ret = xio_net_wrap_freedpf (done->demux_id);
       assert (ret == 0);
     }
     xio_tcp_timewait_remove (twinfo, done);
   }
}

#endif /* __XIO_TCPCOMMON_H__ */

