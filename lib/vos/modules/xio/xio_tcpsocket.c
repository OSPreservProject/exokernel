
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


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>

#include <xok/mmu.h>	/* for NBPG */
#include <xok/sysinfo.h>

#include <vos/proc.h>
#include <vos/cap.h>
#include <vos/vm.h>
#include <vos/sbuf.h>
#include <vos/wk.h>

#include "xio_tcpsocket.h"
#include "xio_tcpcommon.h"
#include "../ports/ports.h"

int xio_tcpsocket_skipdpf = 0;

#if 0
#define FDPRINT(a)	kprintf a
#else
#define FDPRINT(a)
#endif


#define XIO_TCPSOCKET_BUFMAX	(4 * XIO_TCPBUFFER_DATAPERBUF)

#ifdef __XIO_USE_PRI_PORTS
/* GROK -- this needs to be done in some appropriate way!!!! */
static int xio_tcpsocket_next_port = -1;
#endif

/******** Initialization functionality -- only do when actually used! *********/


void xio_tcpsocket_initsock (struct tcpsocket *sock, xio_tcpsock_info_t *info)
{
   sock->sock_state = XIOSOCK_FULLOPEN;
   sock->flags = 0;
   sock->write_offset = 0;
   sock->read_offset = 0;
   sock->buf_max = XIO_TCPSOCKET_BUFMAX;
   sock->demux_id = -1;
   sock->netcardno = -1;
   sock->info = info;
   xio_tcp_inittcb (&sock->tcb);
   xio_tcp_setrcvwnd (&sock->tcb, sock->buf_max);
}


#if 0
xio_nwinfo_t main_nwinfo;
#endif

void xio_tcpsocket_initinfo (xio_tcpsock_info_t *info, char * (*pagealloc)(void *, int), int usetwinfo)
{
   StaticAssert (sizeof(xio_tcpsock_info_t) <= NBPG);

   assert (info->inited == 0);
   info->inited = 1;

   spinlock_reset(&info->lock);

   info->lasttime = 0;
   info->pagealloc = pagealloc;

   info->active_tcpsockets = 0;
   info->tcpsock_pages = 0;
   info->livelist = NULL;
   info->freelist = NULL;

   xio_tcpsocket_initsock (&info->firstsock, info);
   info->firstsock.livenext = info->firstsock.liveprev = NULL;
   info->firstsock.gen = 0;
   info->firstsock.next = NULL;
   info->freelist = &info->firstsock;

   xio_tcpbuffer_init (&info->tbinfo, pagealloc);
   xio_tcp_demux_init (&info->dmxinfo, 0);
   info->usetwinfo = usetwinfo;
   if (usetwinfo) {
      xio_tcp_timewait_init (&info->twinfo, 0, pagealloc);
   } else {
      xio_tcp_timewait_init (&info->twinfo, 0, NULL);
   }
     
   xio_net_wrap_init(&info->nwinfo);
}


void xio_tcpsocket_freesock (struct tcpsocket *tcpsock)
{
   xio_tcpsock_info_t *info = tcpsock->info;

   assert (info != NULL);
   assert (xio_tcp_closed (&tcpsock->tcb));

   tcpsock->sock_state = XIOSOCK_FREE;

#if 0 /* done earlier, at regular close - benjie */
   if (tcpsock->demux_id != -1) {
      int ret = xio_net_wrap_freedpf (tcpsock->demux_id);
      assert (ret == 0);
      tcpsock->demux_id = -1;
   }
#endif

   xio_tcpbuffer_reclaimBuffers (&info->tbinfo, &tcpsock->tcb.inbuffers);
   xio_tcpbuffer_reclaimBuffers (&info->tbinfo, &tcpsock->tcb.outbuffers);

   if (tcpsock->livenext) {
      tcpsock->livenext->liveprev = tcpsock->liveprev;
   }
   if (tcpsock->liveprev) {
      tcpsock->liveprev->livenext = tcpsock->livenext;
   }
   if (info->livelist == tcpsock) {
      assert (tcpsock->liveprev == NULL);
      info->livelist = tcpsock->livenext;
   }
   tcpsock->livenext = NULL;
   tcpsock->liveprev = NULL;

   tcpsock->next = info->freelist;
   info->freelist = tcpsock;

   tcpsock->info->active_tcpsockets--;
   assert (tcpsock->info->active_tcpsockets >= 0);

   if (tcpsock->info->active_tcpsockets == 0)
   {
     xio_net_wrap_shutdown(&(tcpsock->info->nwinfo));
   }
}


struct tcpsocket * xio_tcpsocket_allocsock (xio_tcpsock_info_t *info)
{
   struct tcpsocket *tcpsock = info->freelist;

   if (tcpsock) {
      info->freelist = tcpsock->next;
      assert (tcpsock->livenext == NULL);
      assert (tcpsock->liveprev == NULL);
      tcpsock->livenext = info->livelist;
      info->livelist = tcpsock;
      if (tcpsock->livenext) {
         assert (tcpsock->livenext->liveprev == NULL);
         tcpsock->livenext->liveprev = tcpsock;
      }

   } else {
      int alloccnt = NBPG / sizeof(struct tcpsocket);
      int i;
      StaticAssert (sizeof(struct tcpsocket) < NBPG);
      tcpsock = (struct tcpsocket *) info->pagealloc (info, NBPG);
      info->tcpsock_pages++;
      for (i=1; i<alloccnt; i++) {
         tcpsock[i].livenext = tcpsock[i].liveprev = NULL;
         tcpsock[i].gen = 0;
         xio_tcpsocket_initsock (&tcpsock[i], info);
         info->active_tcpsockets++;
         xio_tcpsocket_freesock (&tcpsock[i]);
      }
      tcpsock->liveprev = NULL;
      tcpsock->livenext = info->livelist;
      info->livelist = tcpsock;
      if (tcpsock->livenext) {
         assert (tcpsock->livenext->liveprev == NULL);
         tcpsock->livenext->liveprev = tcpsock;
      }
   }

   assert (tcpsock);
   xio_tcpsocket_initsock (tcpsock, info);
   tcpsock->gen++;

   assert (tcpsock->info->active_tcpsockets >= 0);
   tcpsock->info->active_tcpsockets++;

   if (tcpsock->info->active_tcpsockets == 1)
   {
     xio_net_wrap_init(&(tcpsock->info->nwinfo));
     /* allocate 4K at a time */
     xio_net_wrap_add
       (&(tcpsock->info->nwinfo), shared_malloc(CAP_USER,NBPG),NBPG);
     xio_net_wrap_add
       (&(tcpsock->info->nwinfo), shared_malloc(CAP_USER,NBPG),NBPG);
     xio_net_wrap_add
       (&(tcpsock->info->nwinfo), shared_malloc(CAP_USER,NBPG),NBPG);
     xio_net_wrap_add
       (&(tcpsock->info->nwinfo), shared_malloc(CAP_USER,NBPG),NBPG);
     xio_net_wrap_setring(&(tcpsock->info->nwinfo));
     tcpsock->nwinfo = &(tcpsock->info->nwinfo);
   }
   else
     tcpsock->nwinfo = &(tcpsock->info->nwinfo);
   
   return (tcpsock);
}


static struct tcb * xio_tcpsocket_gettcb (struct tcb *listentcb)
{
   struct tcpsocket *listensock = (struct tcpsocket *) listentcb;
   /* GROK -- place at SYN_RECV time or at accept() time?? */
   struct tcpsocket *newsock = xio_tcpsocket_allocsock(listensock->info);

   newsock->next = listensock->next;
   listensock->next = newsock;
   xio_tcp_inittcb (&newsock->tcb);
/*
kprintf ("xio_tcpsocket_gettcb: listentcb %p, tcpsock %p\n", listentcb, newsock);
*/
   return (&newsock->tcb);
}


static int xio_tcpsocket_getnetcardno (xio_nwinfo_t *nwinfo, char *packet, int len)
{
   int netcardno = xio_net_wrap_getnetcardno (nwinfo, packet);
   if (netcardno < 0) {
      int ret;
      kprintf ("Unknown netcardno: %d (len %d)\n", netcardno, len);
      kprint_ether_addrs (packet);
      kprint_ip_addrs (packet);
      kprint_tcp_addrs (packet);
      ret = xio_tcp_packetcheck (packet, len, NULL);
      kprintf ("TCP packet check: %d\n", ret);
   }
   assert (netcardno >= 0);
   return (netcardno);
}


  /* send (or try to send) some buffered data out */
	/* GROK -- clean this up!! */
static void xio_tcpsocket_senddata (struct tcpsocket *tcpsock)
{
   struct tcb *tcb = &tcpsock->tcb;
   xio_tcpbuf_t *tmp = (xio_tcpbuf_t *) tcb->outbuffers;
   int offset;

   /* can't send yet if still connecting */
   if (xio_tcp_connecting(&tcpsock->tcb)) {
      return;
   }

   while (tmp) {
      int datamax;
      int ret;

      while (((tmp->start + tmp->offset) <= tcb->send_offset) && ((tmp->start + tmp->offset + tmp->len) > tcb->send_offset)) {
         xio_tcpbuf_t *next = tmp->next;
         offset = tcb->send_offset - tmp->start - tmp->offset;
         if ((next == NULL) || ((next->start + next->offset) != (tmp->start + tmp->offset + tmp->len))) {
            datamax = tmp->len - offset;
            assert (next == NULL);
            ret = xio_tcp_prepDataPacket (tcb, &tmp->data[offset], (tmp->len - offset), 0, 0, TCP_SEND_MAXSIZEONLY, -1);
         } else {
            datamax = tmp->len + next->len - offset;
            ret = xio_tcp_prepDataPacket (tcb, &tmp->data[offset], (tmp->len - offset), next->data, next->len, TCP_SEND_MAXSIZEONLY, -1);
         }
/*
kprintf ("back from prepDataPacket: sendlen %d (%d) and ret %d\n", (tmp->len - offset), ((ret > (tmp->len - offset)) ? next->len : 0), ret);
*/
		/* the comparison to mss provides higher performance by */
		/* avoiding the sending of multiple small packets.      */
/* GROK -- I'm concerned.  This is all a yucky mess and may now be bogus... */
         if ((ret <= 0) || (ret < min (min (tcb->mss, tcb->snd_wnd), datamax))) {
if ((ret) && (tcb->send_ready)) {
   tcb->snd_next -= ret;	// back up to prepared point
   kprintf ("(%d) xio_tcpsocket_senddata INFO: tossing prepared packet (ret %d)\n", getpid(), ret);
   kprintf ("tmp->len %d, offset %d, next %p, next->len %d\n", tmp->len, offset, next, ((next)?next->len:-1));
   kprintf ("snd_next %d, snd_una %d, snd_wnd %d, mss %d\n", tcb->snd_next, tcb->snd_una, tcb->snd_wnd, tcb->mss);
   kprintf ("datamax %d, send_ready %x\n", datamax, tcb->send_ready);
}
#if 1
            if (ret) {
               tcb->send_ready = 0;
            } else {
               xio_tcpcommon_sendPacket (tcb, 0);
            }
#else
            tcb->send_ready = 0;
#endif
            return;
         }
         xio_tcpcommon_sendPacket (tcb, 0);
      }
      tmp = tmp->next;
   }
}


void xio_tcpsocket_handlePackets (xio_tcpsock_info_t *info)
{
   struct tcb *tcb;
   struct ae_recv *packet;
   time_t curtime;
   int ret;

   struct tcpsocket *ts = info->livelist;

   while (ts != NULL)
   {

   while ((packet = xio_net_wrap_getPacket(ts->nwinfo)) != NULL) {
     
     tcb = xio_tcp_demux_searchconns (&info->dmxinfo, packet->r[0].data);
      if (!xio_tcp_packetcheck (packet->r[0].data, packet->r[0].sz, NULL)) {
         kprintf ("tcp packetcheck failed -- toss it\n");
      } else if (tcb) {
         xio_tcp_handlePacket (tcb, packet->r[0].data, TCP_RECV_ADJUSTRCVWND);
         if ((tcb->indata) && (((struct tcpsocket *)tcb)->sock_state & XIOSOCK_READABLE)) {
            int livedatalen = xio_tcp_getLiveInDataLen (tcb);
            char *livedata = xio_tcp_getLiveInData (tcb);
            xio_tcpbuffer_putdata (&info->tbinfo, &tcb->inbuffers, livedata, livedatalen, xio_tcp_getLiveInDataStart(tcb), 0);
         }
	 /* try to combine this with data/close sends below! */
         if (tcb->send_ready != TCP_FL_ACK) {
            xio_tcpcommon_sendPacket (tcb, 0);
         }
         if ((tcb->state == TCB_ST_TIME_WAIT) && (info->usetwinfo)) {
            xio_tcp_timewait_add (&info->twinfo, tcb, ((struct tcpsocket *)tcb)->demux_id);
            ((struct tcpsocket *)tcb)->demux_id = -1;
            tcb->state = TCB_ST_CLOSED;
         }
         if (((struct tcpsocket *)tcb)->write_offset > tcb->send_offset) {
            xio_tcpsocket_senddata ((struct tcpsocket *)tcb);
         } else if (((struct tcpsocket *)tcb)->flags & XIOSOCK_SNDFINAFTERWRITES) {
            xio_tcp_initiate_close (tcb);
            xio_tcpcommon_sendPacket (tcb, 0);
            ((struct tcpsocket *)tcb)->flags &= ~XIOSOCK_SNDFINAFTERWRITES;
         }
         xio_tcpcommon_sendPacket (tcb, 0);
         if (xio_tcp_closed (tcb)) {
            xio_tcp_demux_removeconn (&info->dmxinfo, tcb);
            if (((struct tcpsocket *)tcb)->sock_state == XIOSOCK_CLOSED) {
               xio_tcpsocket_freesock ((struct tcpsocket *)tcb);
            }
         }
      } else if (xio_tcpcommon_handleTWPacket (&info->twinfo, packet->r[0].data, ts->nwinfo)) {
      } else {
         int netcardno;
         struct tcb *listentcb = xio_tcpcommon_findlisten (&info->dmxinfo, packet->r[0].data, ts->nwinfo, &netcardno);
      
         if (listentcb) {
           tcb =  (listentcb) ? xio_tcpsocket_gettcb(listentcb) : NULL;
	   if (tcb != NULL) {
	      ret = xio_tcp_handleListenPacket (tcb, packet->r[0].data);
              assert (ret != 0);
              tcb->netcardno = xio_tcpsocket_getnetcardno (ts->nwinfo, packet->r[0].data, packet->r[0].sz);
              assert (tcb->netcardno == netcardno);
              xio_tcpcommon_sendPacket (tcb, 0);
              ret = xio_tcp_demux_addconn (&info->dmxinfo, tcb, 0);
	      assert (ret == 0);
           }
         } else {
            xio_tcpcommon_handleRandPacket (packet->r[0].data, ts->nwinfo);
         }
      }
      xio_net_wrap_returnPacket (ts->nwinfo, packet);
   }

   ts = ts->livenext;
   }

   curtime = time(NULL);
   if (curtime > info->lasttime) {
      struct tcb *next = &info->livelist->tcb;
      while ((tcb = next)) {
         next = &((struct tcpsocket *)tcb)->livenext->tcb;
         assert (tcb != next);
         assert (next != (struct tcb *) info->livelist);
         if (!xio_tcp_timedout (tcb)) {
            continue;
         }
         ret = xio_tcp_timeout (tcb);
	 /* try to combine this with data/close sends below! */
         if ((ret) && (tcb->send_ready != TCP_FL_ACK)) {
            xio_tcpcommon_sendPacket (tcb, 0);
         }
         if (ret == 0) {
            continue;
         }
         if (xio_tcpbuffer_countBufferedData (&info->tbinfo, &tcb->outbuffers, xio_tcp_acked_offset(tcb))) {
            xio_tcpsocket_senddata ((struct tcpsocket *)tcb);
         }
         xio_tcpcommon_sendPacket (tcb, 0);
         if (xio_tcp_closed (tcb)) {
            xio_tcp_demux_removeconn (&info->dmxinfo, tcb);
            if (((struct tcpsocket *)tcb)->sock_state == XIOSOCK_CLOSED) {
               xio_tcpsocket_freesock ((struct tcpsocket *)tcb);
            }
         }
      }

      xio_tcpcommon_TWtimeout (&info->twinfo);
      info->lasttime = curtime;
   }
}


/* update the receive window and, if heuristically appropriate, send a */
/* window update packet to the other side.                             */

static void xio_tcpsocket_setrcvwnd (struct tcpsocket *sock)
{
   int oldwin = xio_tcp_rcvwnd (&sock->tcb);
   int newwin = sock->buf_max - xio_tcpbuffer_countBufferedData (&sock->info->tbinfo, &sock->tcb.inbuffers, 0);
   if (newwin < 0) {
      newwin = 0;
   }
   xio_tcp_setrcvwnd (&sock->tcb, newwin);

   /* send window update if window size dropped or it now allows a full */
   /* ethernet packet to be sent (where it didn't before).              */
   if ((newwin < oldwin) || ((oldwin < DEFAULT_MSS) && (newwin >= DEFAULT_MSS))) {
      xio_tcp_prepCtlPacket (&sock->tcb);
      xio_tcpcommon_sendPacket (&sock->tcb, 0);
   }
}


static int xio_tcpsocket_tcpclose (struct tcpsocket *sock, int noblock)
{
   int gen = sock->gen;

   if (xio_tcp_closed (&sock->tcb)) {
      return (0);
   }

   if (! xio_tcp_finsent(&sock->tcb)) {
      sock->flags |= XIOSOCK_SNDFINAFTERWRITES;
   }

   xio_tcpsocket_handlePackets (sock->info);

   /* while still data to send ... */
   while (sock->write_offset > sock->tcb.send_offset) {
      if (noblock) {
         return (0);
      }
      xio_tcp_waitforChange (sock,
	                     sock->nwinfo,
			     sock->tcb.timer_retrans);
      xio_tcpsocket_handlePackets (sock->info);
   }

   if (sock->flags & XIOSOCK_SNDFINAFTERWRITES) {
      sock->flags &= ~XIOSOCK_SNDFINAFTERWRITES;
      xio_tcp_initiate_close(&sock->tcb);
      xio_tcpcommon_sendPacket (&sock->tcb, 0);
   }

   if (!noblock) {
	/* the gen value prevents a race condition with socket re-use */
      while ((sock->gen == gen) && (!xio_tcp_closed (&sock->tcb)) && (sock->tcb.state != TCB_ST_TIME_WAIT)) {
         xio_tcp_waitforChange (sock,
	                        sock->nwinfo,
				sock->tcb.timer_retrans);
         xio_tcpsocket_handlePackets (sock->info);
      }
   }

   return (0);
}


int xio_tcpsocket_close (struct tcpsocket *sock, int noblock)
{
   if ((sock == NULL) || (sock->sock_state == XIOSOCK_CLOSED)) {
      errno = ENOTCONN;
      return (-1);
   }

   dprintf("xio_tcpsocket_close: demux_id %d, noblock %d\n",
	 sock->demux_id, noblock);

#ifndef __XIO_USE_PRI_PORTS
   if (ntohs(sock->tcb.tcpdst) > 0)
     ports_unbind(ntohs(sock->tcb.tcpdst));
#endif

   if (sock->tcb.state == TCB_ST_LISTEN) {
      kprintf("xio_tcpsocket_close, calling removelisten!!!\n");
      xio_tcp_demux_removelisten (&sock->info->dmxinfo, &sock->tcb);
      /* GROK -- this is wrong!  need to handle this correctly */
      sock->demux_id = -1;
      sock->tcb.state = TCB_ST_CLOSED;
      xio_tcpsocket_freesock (sock);
      return (0);
   }

   assert (sock->sock_state != XIOSOCK_CLOSED);
   sock->sock_state = XIOSOCK_CLOSED;

   if (xio_tcp_closed (&sock->tcb)) {
      xio_tcpsocket_freesock (sock);
   } else {
      return (xio_tcpsocket_tcpclose(sock, noblock));
   }
   return (0);
}

     
int xio_tcpsocket_bind(struct tcpsocket *sock, const struct sockaddr *reg_name, int namelen)
{
   unsigned short _src_port;
   const struct sockaddr_in *name;
   int r;
   char *ip_src;

   name = (struct sockaddr_in *)reg_name;

   FDPRINT (("tcp_bind\n"));

   if (name == 0) {
      kprintf ("xio_tcpsocket_bind: NULL sockaddr structure not legal\n");
      errno = EFAULT;
      return (-1);
   }

   if (namelen != sizeof (struct sockaddr)) { 
      kprintf ("xio_tcpsocket_bind: incorrect namelen it is %d should be %d\n", namelen, sizeof(struct sockaddr));
      errno = EINVAL; 
      return (-1);
   }
   
   /* here we should check that the local address is valid, like 0.0.0.0
    * (choose any), 127.0.0.1, 18.... etc...  */
   ip_src = (char *)&name->sin_addr;
   if (xio_net_wrap_check_local_address(ip_src) != 0)
   {
     kprintf("xio_tcpsocket_bind: address not available\n");
     RETERR(V_ADDRNOTAVAIL);
   }

   assert(name->sin_port != 0);

   _src_port = ntohs(name->sin_port);

#ifndef __XIO_USE_PRI_PORTS
   r = ports_reserve(&_src_port);
   if (r != 0)
   {
     errno = EADDRINUSE;
     return -1;
   }
#endif

   if (_src_port == 0) 
   {
      errno = EADDRINUSE;
      return -1;
   }

   sock->tcb.tcpdst = htons(_src_port);
   memcpy ((char *)&sock->tcb.ipdst, (char *)&name->sin_addr, 4);

//kprintf ("(%d) xio_tcpsocket_bind: sock %p, tcpport %d, ipaddr %x\n", getpid(), sock, ntohs(name->sin_port), *((uint *)&name->sin_addr));

   return (0);
}


int xio_tcpsocket_getname(struct tcpsocket *sock, struct sockaddr *reg_name, int *namelen, int peer)
{
   struct sockaddr_in *name = (struct sockaddr_in *) reg_name;

   if ((reg_name == NULL) || (namelen == NULL)) {
      kprintf ("xio_tcpsocket_getname: NULL sockaddr structure (or NULL length) not legal\n");
      errno = EFAULT;
      return (-1);
   }

   assert (sock != NULL);

   if (peer == 0) {
      /* getsockname */
      name->sin_port = sock->tcb.tcpdst;	/* tcpdst in net order */
      memcpy ((char *)&name->sin_addr, (char *)&sock->tcb.ipdst, 4);
       
   } else {
      /* getpeername */
      if (sock->tcb.tcpsrc == 0) {
         kprintf ("xio_tcpsocket_getname: not connected in getpeername\n");
         errno = ENOTCONN;
         return (-1);
      }
      name->sin_port = sock->tcb.tcpsrc;	/* tcpsrc in net order */
      memcpy ((char *)&name->sin_addr, (char *)&sock->tcb.ipsrc, 4);
   }

   name->sin_family = AF_INET;
   *namelen = sizeof (struct sockaddr_in);

//kprintf ("(%d) xio_tcpsocket_getname: sock %p, peer %d, tcpport %d, ipaddr %x\n", getpid(), sock, peer, ntohs(name->sin_port), *((uint *)&name->sin_addr));

   return (0);
}


static int select_body (struct tcpsocket *sock, int rw)
{
   int buffered;

   xio_tcpsocket_handlePackets (sock->info);

   if (sock->tcb.state == TCB_ST_LISTEN) {
      return (sock->next != NULL);
   }

   switch (rw) {
      case SELECT_READ:
		/* inbuffers is non-NULL only if data avail... */
         if ((!xio_tcp_readable(&sock->tcb)) && (!xio_tcp_finrecv(&sock->tcb))) {
            //kprintf ("TCP select_body INFO: SELECT_READ but not readable (state %d, inbuffers %p)\n", sock->tcb.state, sock->tcb.inbuffers);
         }
         return ((sock->tcb.inbuffers != NULL) || (xio_tcp_finrecv(&sock->tcb)) || !(sock->sock_state & XIOSOCK_READABLE));
      case SELECT_WRITE:
         if ((!xio_tcp_writeable(&sock->tcb)) && (!xio_tcp_closed(&sock->tcb))) {
            //kprintf ("TCP select_body INFO: SELECT_WRITE but not readable (state %d, snd_next %d, snd_una %d)\n", sock->tcb.state, sock->tcb.snd_next, sock->tcb.snd_una);
         }
         buffered = xio_tcpbuffer_countBufferedData(&sock->info->tbinfo, &sock->tcb.outbuffers, xio_tcp_acked_offset(&sock->tcb));
         return ((xio_tcp_closed(&sock->tcb)) || ((sock->tcb.state > TCB_ST_SYN_SENT) && ((sock->buf_max > buffered) || !(sock->sock_state & XIOSOCK_WRITEABLE))));
      default:
         assert(0);
   }

   return (0);
}


int xio_tcpsocket_select (struct tcpsocket *sock, int rw)
{
   FDPRINT (("tcp_select: sock %p, rw %d\n", sock, rw));

   return (select_body (sock, rw));
}


#include <term.h> /* XXX - to prevent kprintf for TIOCGETA. isatty code legitimately
		     uses this ioctl to test for tty. */
int xio_tcpsocket_ioctl(struct tcpsocket *sock, unsigned int request, char *argp)
{
   xio_tcpsocket_handlePackets (sock->info);

   switch(request) {
      case FIONREAD:
         *(int *)argp = xio_tcpbuffer_countBufferedData (&sock->info->tbinfo, &sock->tcb.inbuffers, 0);
         return (0);
      default:
         if (request != TIOCGETA)
            kprintf("tcp ioctl sock: %p, request: %d, argp: %p\n", sock,request,argp);
   }

   return (-1);
}


inline int xio_tcpsocket_recv(struct tcpsocket *sock, void *buffer, int length, int nonblock, unsigned flags)
{
   int n;

   xio_tcpsocket_handlePackets (sock->info);

   FDPRINT (("tcp_read: buffer %p, length %d, nonblocking %d\n", buffer, length, nonblock));

   if (buffer == NULL) { 
      kprintf ("xio_tcpsocket_recv: buffer is NULL\n");
      errno = EFAULT;
      return (-1);
   }

   if ((sock->sock_state & XIOSOCK_READABLE) == 0) {
      kprintf ("xio_tcpsocket_recv: socket is not readable (sock_state %d)\n", sock->sock_state);
      errno = ENOTCONN;
      return (-1);
   }

   if (nonblock) {
      n = xio_tcpbuffer_getdata (&sock->info->tbinfo, &sock->tcb.inbuffers, buffer, length);
      if ((n == 0) && (xio_tcp_finrecv(&sock->tcb))) {
         return (0);
      } else if (n == 0) {
//kprintf ("recv returns 'cause would block\n");
         errno = EAGAIN;
         return (-1);
      } else if (n < 0) {
         errno = ENOTCONN;
         return (-1);
      }
      sock->read_offset += n;
      xio_tcpsocket_setrcvwnd (sock);
//kprintf ("returning %d bytes of data\n", n);
      return (n);
   } else {
	/* GROK - what is the rule here??  Does one wait for "length" bytes?? */
/*
kprintf ("xio_tcpsocket_read: len %d (offset %d, count %d, inbuffers %p)\n", length, sock->read_offset, xio_tcpbuffer_countBufferedData(&sock->info->tbinfo, &sock->tcb.inbuffers), sock->tcb.inbuffers);
*/
      do {
         n = xio_tcpbuffer_getdata (&sock->info->tbinfo, &sock->tcb.inbuffers, buffer, length);
/*
kprintf ("getdata returned %d (inbuffers %p)\n", n, sock->tcb.inbuffers);
*/
         if ((n > 0) || ((n == 0) && (xio_tcp_finrecv(&sock->tcb)))) {
            sock->read_offset += n;
            xio_tcpsocket_setrcvwnd (sock);
            return (n);
         } else if (n < 0) {
            errno = ENOTCONN;
            return (-1);
         }
         xio_tcp_waitforChange (sock, sock->nwinfo,
				sock->tcb.timer_retrans);
         xio_tcpsocket_handlePackets (sock->info);
      } while (1);
   }
   errno = ENOTCONN;
   return (-1);
}


int xio_tcpsocket_read(struct tcpsocket *sock, char *buffer, int length, int nonblocking)
{
   return (xio_tcpsocket_recv(sock,buffer,length,nonblocking,0));
}


int xio_tcpsocket_stat (struct tcpsocket *sock, struct stat *buf)
{
   xio_tcpsocket_handlePackets (sock->info);

   if (buf == NULL) {
      errno = EFAULT;
      return (-1);
   }

   if (sock->tcb.state == TCB_ST_LISTEN) {
      kprintf ("xio_tcpsocket_stat: can't stat a listen socket\n");
      errno = EIO;
      return (-1);
   }

   bzero (buf, sizeof (struct stat));
   buf->st_size = xio_tcpbuffer_countBufferedData (&sock->info->tbinfo, &sock->tcb.inbuffers, 0);
   buf->st_blksize = 4096;
   return (0);
}


int xio_tcpsocket_write(struct tcpsocket *sock, const char *buffer, int length, int nonblocking) {
   int retlen = 0;

   FDPRINT (("tcp_write: buffer %p, length %d, nonblocking %d\n", buffer, length, nonblocking));

   //kprintf ("tcp_write: sock %p, buffer %p, length %d, nonblocking %d\n", sock, buffer, length, nonblocking);
   //kprintf ("sock->info %p, tcbstate %d, demux_id %d, outbuffers %p\n", sock->info, sock->tcb.state, sock->demux_id, sock->tcb.outbuffers);

   xio_tcpsocket_handlePackets (sock->info);

   if (buffer == NULL) {
      errno = EFAULT;
      return (-1);
   }

   if (((! xio_tcp_writeable (&sock->tcb)) && (sock->tcb.state > TCB_ST_ESTABLISHED)) || !(sock->sock_state & XIOSOCK_WRITEABLE)) {
      errno = EPIPE;
      return (-1);
   }
/*
kprintf ("xio_tcpsocket_write: writing %d bytes at offset %d (nonblocking %d, flags %x)\n", length, sock->write_offset, nonblocking, filp->f_flags);
*/

   while (retlen < length) {
      int ret = xio_tcpbuffer_countBufferedData(&sock->info->tbinfo, &sock->tcb.outbuffers, xio_tcp_acked_offset(&sock->tcb));
      int allowed = sock->buf_max - ret;
      int tmplen = min ((length-retlen), allowed);
      if ((! xio_tcp_writeable (&sock->tcb)) && (sock->tcb.state > TCB_ST_ESTABLISHED)) {
         errno = EPIPE;
         return (-1);
      }
      if (tmplen > 0) {
         ret = xio_tcpbuffer_putdata (&sock->info->tbinfo, &sock->tcb.outbuffers, buffer, tmplen, sock->write_offset, xio_tcp_acked_offset(&sock->tcb));
         buffer += ret;
         retlen += ret;
         sock->write_offset += ret;
         xio_tcpsocket_handlePackets (sock->info);
         xio_tcpsocket_senddata (sock);
         if (allowed > ret) {
            continue;
         }

      } else if (nonblocking) {
/*
kprintf ("breaking early: ret %d, retlen %d, buf_max %d, allowed %d\n", ret, retlen, sock->buf_max, allowed);
*/
         if (retlen == 0) {
            errno = EAGAIN;
            return (-1);
         }
         break;
      }
      if (retlen < length) {
         xio_tcp_waitforChange (sock, sock->nwinfo, sock->tcb.timer_retrans); 
         xio_tcpsocket_handlePackets (sock->info);
      }
   }
/*
kprintf ("%d bytes written (out of %d)\n", retlen, length);
*/
   return (retlen);
}


int xio_tcpsocket_send(struct tcpsocket *sock, void *buffer, int length, int nonblock, unsigned flags)
{
	/* for now, the flags are being ignored */
   return (xio_tcpsocket_write (sock, buffer, length, nonblock));
}


int xio_tcpsocket_connect(struct tcpsocket *sock, const struct sockaddr *uservaddr, int namelen, int flags, int nonblock, uint16 _src_port, char *src_ip, char *dst_ip, char *src_eth, char *dst_eth) {
   const struct sockaddr_in *name = (struct sockaddr_in *) uservaddr;

   FDPRINT (("tcpsocket_connect\n"));

#ifdef __XIO_USE_PRI_PORTS
   if (_src_port == 0) {
      while (xio_tcpsocket_next_port < 1000) {
         xio_tcpsocket_next_port = (random() % 65536);
      }
      _src_port = xio_tcpsocket_next_port++;
   }
#endif

	/* GROK -- this skipdpf crap needs to go! */
   if (xio_tcpsocket_skipdpf == 0) {
      int localport = _src_port;
      dprintf("trying to get dpf: local port is %d\n", _src_port);
      sock->demux_id = xio_net_wrap_getdpf_tcp (sock->nwinfo, &localport, ntohs(name->sin_port)); // sock->info->nwinfo
      if (sock->demux_id < 0) {
         errno = EADDRINUSE;	/* just to say something */
		/* GROK -- clean up is needed! */
         kprintf ("unable to get DPF filter\n");
         return (-1);
      }
      _src_port = localport;
   }

   /* get network card ID for transmission */
   sock->tcb.netcardno = xio_tcpsocket_getnetcardno (sock->nwinfo, src_eth, 6);
   // sock->info->nwinfo

   xio_tcp_bindsrc (&sock->tcb, _src_port, src_ip, src_eth);
   xio_tcp_initiate_connect(&sock->tcb, _src_port, ntohs(name->sin_port), dst_ip, dst_eth);

   xio_tcpcommon_sendPacket (&sock->tcb, 0);

   if (xio_tcp_demux_addconn (&sock->info->dmxinfo, &sock->tcb, 1) == -1) {
      kprintf ("xio_tcpsocket_connect: unable to add new connection after initiate_connect\n");
      assert (0);
   }

   /* filp->f_flags |= O_RDWR;    GROK -- why is this needed?? */

   if (!nonblock) {
      do {
         xio_tcp_waitforChange (sock, sock->nwinfo,
				sock->tcb.timer_retrans);
         xio_tcpsocket_handlePackets (sock->info);
      } while (sock->tcb.state == TCB_ST_SYN_SENT);
   }

   if (xio_tcp_closed (&sock->tcb)) {
      errno = (sock->tcb.retries > TCP_RETRY) ? ETIMEDOUT : ECONNREFUSED;
      return (-1);
   }

   /* non-blocking connect that is not yet complete */
   if (sock->tcb.state == TCB_ST_SYN_SENT) {
      errno = EINPROGRESS;
      return (-1);
   }

   return (0);
}


int xio_tcpsocket_setsockopt(struct tcpsocket *sock, int level, int optname, const void *optval, int optlen)
{
   FDPRINT (("tcpsocket_setsockopt\n"));

	/* GROK - for now, just ignore socket options! */
   return (0);
}


int xio_tcpsocket_getsockopt(struct tcpsocket *sock, int level, int optname, void *optval, int *optlen)
{
   FDPRINT (("tcpsocket_getsockopt\n"));

	/* GROK - for now, just ignore socket options! */
   return (0);
}


int xio_tcpsocket_shutdown(struct tcpsocket *sock, int howto)
{
   int ret = 0;

   if ((sock == NULL) || (sock->sock_state == XIOSOCK_CLOSED)) {
      errno = ENOTCONN;
      return (-1);
   }

   FDPRINT (("xio_tcpsocket_shutdown: demux_id %d, howto %x\n",sock->demux_id, howto));

   if ((howto == SHUT_WR) || (howto == SHUT_RDWR)) {
      sock->sock_state &= ~XIOSOCK_WRITEABLE;
      ret = xio_tcpsocket_tcpclose (sock, 1);
   }

   if ((howto == SHUT_RD) || (howto == SHUT_RDWR)) {
      xio_tcpbuffer_reclaimBuffers (&sock->info->tbinfo, &sock->tcb.inbuffers);
      sock->sock_state &= ~XIOSOCK_READABLE;
   }

   return (ret);
}


struct tcpsocket * xio_tcpsocket_accept (struct tcpsocket *sock, struct sockaddr *newsockaddr0, int *addr_len, int flags, int nonblock)
{
   struct sockaddr_in *newsockaddr = (struct sockaddr_in *)newsockaddr0;
   struct tcpsocket *newsock;

/*
kprintf ("(%d) xio_tcpsocket_accept\n", getpid());
*/

	/* inited?? */
   if ((sock == NULL) || (sock->tcb.state != TCB_ST_LISTEN)) {
      errno = EINVAL;
      return (NULL);
   }

   do {
      xio_tcpsocket_handlePackets (sock->info);
      newsock = sock->next;
	/* GROK -- this needs much work.  The list should be cleaned and */
	/*         not solely head searched...                           */
      if ((newsock != NULL) && (!xio_tcp_connecting(&newsock->tcb))) {
         sock->next = newsock->next;
         newsock->next = NULL;
         /* need to figure out who I am connected to. */
         if (newsockaddr) {
            memcpy((char *)&newsockaddr->sin_addr, (char *)&newsock->tcb.ipsrc, sizeof (newsockaddr->sin_addr));
            newsockaddr->sin_port = newsock->tcb.tcpsrc;	/* tcpsrc in net order */
            *addr_len = sizeof (struct sockaddr);
         }
/*
kprintf ("connection accepted\n");
*/
         return (newsock);
      } else if (nonblock) {
/*  GROK -- cleanup! */
         kprintf ("would block\n");
         errno = EWOULDBLOCK;
         return (NULL);
      }
      xio_tcp_waitforChange (sock, sock->nwinfo,
			     sock->tcb.timer_retrans);
   } while (1);
}


int xio_tcpsocket_listen (struct tcpsocket *sock, int backlog)
{
   uint portno;
   uint32 ipaddr;

   assert (sock != NULL);

//kprintf ("(%d) xio_tcpsocket_listen: backlog %d, portno %d\n", getpid(), backlog, ntohs(sock->tcb.tcpsrc));

   ipaddr = sock->tcb.ipdst;
   portno = ntohs(sock->tcb.tcpdst);

   /* GROK -- this is temporary.  Really, we should be doing a bind here */
   assert (portno != 0);

   xio_tcp_inittcb (&sock->tcb);

   xio_tcp_initlistentcb ((struct listentcb *) &sock->tcb, ipaddr, portno);
   sock->demux_id = xio_net_wrap_getdpf_tcp (sock->nwinfo, (int*)&portno, -1);
   // sock->info->nwinfo
	/* GROK -- need to undo in failure cases! */
   if (sock->demux_id < 0) {
      sock->tcb.state = TCB_ST_CLOSED;
      xio_tcpsocket_freesock (sock);
      errno = EADDRINUSE;
      return (-1);
   }

   xio_tcp_demux_addlisten (&sock->info->dmxinfo, &sock->tcb, 0);
   sock->next = NULL;

/*
kprintf ("(%d) xio_tcpsocket_listen: listentcb %p, portno %d\n", getpid(), sock, portno);
*/

   return (0);
}


#ifdef EXOPC

int xio_tcpsocket_select_pred(struct tcpsocket *sock, int rw, struct wk_term *t)
{
   int next;

   FDPRINT (("xio_tcpsocket_select_pred: sock %p, rw %d\n", sock, rw));

   next = (select_body (sock, rw)) ? -1 : 0;

   if (next != -1) {
      next = xio_net_wrap_wkpred_packetarrival (sock->nwinfo, t);
   }

   if (next != -1) {
      int addon = next;
      addon = wk_mkop (addon, t, WK_OR);
      addon += xio_tcp_wkpred (&sock->info->livelist->tcb, &t[addon]);
      next = (addon == (next+1)) ? next : addon;
   }

#if 0
	/* this forces the select to be re-evaluated in 200 ticks */
   if (next != -1) {
      next = wk_mkop (next, t, WK_OR);
      next += wk_mksleep_pred (&t[next], (__sysinfo.si_system_ticks + 200));
   }
#endif

   if (next == -1) {	/* pin to true, since there is work to be done */
      next = wk_mksleep_pred (t, (__sysinfo.si_system_ticks - 1));
   }

   return (next);
}


int xio_tcpsocket_pred (tcpsocket_t *sock, struct wk_term *t)
{
   int next;

   FDPRINT (("xio_tcpsocket_pred: sock %p\n", sock));

   next = xio_net_wrap_wkpred_packetarrival (sock->nwinfo, t);

#if 0
   if ((next != -1) && (info->livelist != NULL)) {
      int addon = next;
      addon = wk_mkop (addon, t, WK_OR);
      addon += xio_tcp_wkpred (&info->livelist->tcb, &t[addon]);
      next = (addon == (next+1)) ? next : addon;
   }
#else
   if (next != -1) {
      int addon = next;
      addon = wk_mkop (addon, t, WK_OR);
      addon += xio_tcp_wkpred (&sock->tcb, &t[addon]);
      next = (addon == (next+1)) ? next : addon;
   }
#endif


#if 0
	/* this forces the select to be re-evaluated in 200 ticks */
   if (next != -1) {
      next = wk_mkop (next, t, WK_OR);
      next += wk_mksleep_pred (&t[next], (__sysinfo.si_system_ticks + 200));
   }
#endif

   assert (next <= XIO_TCPSOCKET_PRED_SIZE);

//kprintf ("xio_tcpsocket_pred: next %d\n", next);

   return (next);
}

#endif

