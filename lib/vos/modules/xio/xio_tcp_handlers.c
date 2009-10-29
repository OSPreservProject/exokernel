
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
 * TODO: 
 * - clean up prepDataPacket
 * - allow more incremental combining of packets (adding flags and data)
 * - detect connection breaks etc. (keep alive segments)
 * - eliminate interdependence on ether and ip
 * - externalize delayed ack control better
 * - data on a syn segment is deleted
 * - implement congestion control
 * - implement reassembly (not really needed)
 */

#include <sys/types.h>
#include <xok/pctr.h>		/* use cycle counters for tcp's clock */
#include <xok/sysinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "xio_tcp.h"
#include "xio_tcpsocket.h"
#include "xio_tcp_handlers.h"
#include "xio_tcp_timewait.h"
#include "xio_tcp_stats.h"

#include <netinet/in.h>

#include <vos/net/fast_ip.h>
#include <vos/net/fast_eth.h>
#include <vos/net/ae_ether.h>
#include <vos/net/cksum.h>

tcp_stat *tcpstats = NULL;


/* Some internal constants */
#define TCP_TIMEOUT(tcb)	(tcb->rtt * USECS_PER_TCP_TICK) /* in useconds */
#define TCP_RETRY	4


/* TCB control flags: */
#define TCB_FL_DELACK		0x0010
#define TCB_FL_DOWINDPROBE	0x0020

/*
#define NO_HEADER_PREDICTION
*/
#define USEDELACKS


/* ----------------------- timer functions --------------------------- */

/* need to add functions for incremental add and subtract of cksums */

void xio_tcp_clobbercksum (char *packet, int len)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

   if (len >= XIO_EIT_DATA) {
      tcp->cksum = 0;
   }
}


/* ----------------------- timer functions --------------------------- */


static void timer_set(struct tcb *tcb, unsigned int microseconds)
{
  tcb->timer_retrans =  __ticks2usecs (__sysinfo.si_system_ticks) + microseconds;
}


static void timer_unset (struct tcb *tcb)
{
   tcb->timer_retrans = 0;
}


/* return the value of the 4 micro-second tcp clock */
unsigned int xio_tcp_read_clock () {
  pctrval cycle_count;
  static int cc2tcp_clock_divisor = -1; /* map from cycle counters to tcp's
					       4 microsecond clock */

  if (cc2tcp_clock_divisor == -1) {
    /* one time initialization */

    int rate = __sysinfo.si_mhz * 1000000;
    cc2tcp_clock_divisor = 0;
    while (rate > TCP_RATE_HZ) {
      cc2tcp_clock_divisor++;
      rate = rate >> 1;
    }

    /* prefer to actually run faster rather than slower than the tcp clock */
    if (rate < TCP_RATE_HZ) {
      cc2tcp_clock_divisor--;	
      assert (cc2tcp_clock_divisor >= 0);
    }
  }
    
  /* read the cycle counter */
  cycle_count = rdtsc ();
  /* divide it so that it has the right frequency */
  cycle_count = cycle_count >> cc2tcp_clock_divisor;
  return (cycle_count & 0xffffffff);
}


void xio_update_rtt (struct tcb *tcb, unsigned int seqno) {
  unsigned int newest_rtt;
  unsigned int oldest_rtt;

  /* is this an ack for the last packet sent? */
  /* Note, snd_next is the NEXT sequence number to be used */
  if (SEQ_GT (tcb->snd_next, seqno)) {
    return;
  }

  newest_rtt = xio_tcp_read_clock () - tcb->send_time;
  oldest_rtt = tcb->rtt_history[tcb->rtt_next_ndx];
     
  /* instead of recomputing the entire average for
     every update we just incrementally pull off the
     influence of the oldest rtt measurement and add in
     the influence of the newest measurement. */

  tcb->rtt -= oldest_rtt/RTT_HISTORY_SZ;
  tcb->rtt += newest_rtt/RTT_HISTORY_SZ;
  tcb->rtt_history[tcb->rtt_next_ndx] = newest_rtt;

  /* increment the index to point to the new oldest element */
  tcb->rtt_next_ndx++;
  if (tcb->rtt_next_ndx == RTT_HISTORY_SZ) {
    tcb->rtt_next_ndx = 0;
  }
}
    
    

/* ---------------------- TCB initialization functions ---------------------- */


/* TCP control block initialization */
void xio_tcp_resettcb (struct tcb *tcb)
{
  int i;

  tcb->snd_iss = xio_tcp_read_clock ();
  tcb->snd_next = tcb->snd_iss;
  tcb->snd_fin = 0;
  tcb->snd_wla = tcb->snd_iss;
  tcb->snd_up = tcb->snd_iss;
  tcb->send_offset = 0;
  timer_unset (tcb);		/* resets timer_retrans */
  tcb->retries = 0;
  tcb->mss = TCP_MSS;		/* set default MSS */
  tcb->flags = TCB_FL_DELACK;
  tcb->state = TCB_ST_CLOSED;

  tcb->inbuffers = NULL;
  tcb->outbuffers = NULL;
  tcb->inpackets = 0;
  tcb->outpackets = 0;

  tcb->rcv_wnd = 8192;

  tcb->rtt = START_RTT;		/* no rtt measurements yet */
  tcb->rtt_next_ndx = 0;
  for (i = 0; i < RTT_HISTORY_SZ; i++) {
    tcb->rtt_history[i] = START_RTT;
  }

  tcb->hashed = 0;
}


void xio_tcp_inittcb (struct tcb *tcb)
{
   struct eth *eth;
   struct ip *ip;
/*
printf ("xio_tcp_inittcb: tcb %d, buf %d, buflen %d\n", (uint)tcb, (uint)buf, buflen);
*/
   /* this bzero eliminates the need for setting many ind. values to zero */
   bzero ((char *)tcb, sizeof(struct tcb));

   StaticAssert (64 >= (XIO_EIT_DATA + ETHER_ALIGN));

   /* Set up pointers to headers in snd_recv; worry about alignment */
   tcb->snd_recv_n = 2;
   tcb->snd_recv_r0_sz = XIO_EIT_DATA;
   tcb->snd_recv_r0_data = &tcb->snd_buf[ETHER_ALIGN];

   /* Note that XIO_EIT_DATA bytes were actually set aside! */

   /* non-zero Ethernet header initialization. */
   eth = (struct eth *) &tcb->snd_recv_r0_data[XIO_EIT_ETH];
   eth->proto = htons(EP_IP);

   /* non-zero IP initialization */
   ip = (struct ip *) &tcb->snd_recv_r0_data[XIO_EIT_IP];
   ip->vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);
   ip->tos = 0;		/* we should allow other values */
   ip->protocol = IP_PROTO_TCP;

   /* no non-zero TCP initialization needed */

   xio_tcp_resettcb (tcb);
}


/* struct tcb copying routine that fixes up internal pointers */

void xio_tcp_copytcb (struct tcb *oldtcb, struct tcb *newtcb)
{
   *newtcb = *oldtcb;
   newtcb->snd_recv_r0_data = &newtcb->snd_buf[ETHER_ALIGN];
}


void xio_tcp_initlistentcb (struct listentcb *listentcb, uint32 ipaddr,
			    int portno)
{
   /* this bzero eliminates the need for setting many ind. values to zero */
   bzero ((char *)listentcb, sizeof(struct listentcb));
   listentcb->tcpdst = htons((uint16)portno);
   listentcb->ipdst = ipaddr;
   listentcb->state = TCB_ST_LISTEN;
}


/* ------------------ general packet construction functions ----------------- */


/*
 * Prepare a packet (in the tcb) to be sent, including the specified flags
 * and data (via tcb) at the specified seqno.  Provided checksum for data
 * can be used in place of computing it here.
 * We can avoid the copy by using a recv structure with 3 ptrs: (1) header
 * (2) user data, (3) user data.
 */
static void xio_tcp_preparePacket (struct tcb *tcb, int flag, uint32 seqno, int len, int len2, int datasum)
{
   struct ip *ip = (struct ip *) &tcb->snd_recv_r0_data[XIO_EIT_IP];
   struct tcp *tcp = (struct tcp *) &tcb->snd_recv_r0_data[XIO_EIT_TCP];
   int sum;

   if (tcb->send_ready & (~TCP_FL_ACK)) {
      dprintf ("send_ready %x, flag %x, len %d, len2 %d\n", tcb->send_ready, flag, len, len2);
   }
   assert ((tcb->send_ready & (~TCP_FL_ACK)) == 0);
	/* GROK -- clean this up */
   tcb->send_ready = flag | ((len+len2) ? 0x10000000 : 0);
    
   if (len + len2) {
      tcb->snd_recv_r1_sz = len;
      tcb->snd_recv_r2_sz = len2;
      tcb->snd_recv_n = (len2) ? 3 : 2;
   } else {
      tcb->snd_recv_n = 1;
   }
   tcb->outpackets++;

   tcp->flags = flag;
   tcp->offset = ((tcb->snd_recv_r0_sz - XIO_EIT_TCP) >> 2) << 4;

   tcp->seqno = htonl(seqno);
   tcp->ackno = htonl(tcb->rcv_next);
   tcp->window = htons(tcb->rcv_wnd);

   ip->totallength = htons(tcb->snd_recv_r0_sz - XIO_EIT_IP + len + len2);
   tcp->cksum = 0;

   /* Compute cksum on pseudo header and tcp header */
   sum = htons(ntohs(ip->totallength)-sizeof(struct ip));
   if (len + len2) {
      sum += datasum;
   }
   sum += tcb->pseudoIP_cksum;
   tcp->cksum = inet_checksum ((uint16 *) tcp, (tcb->snd_recv_r0_sz - XIO_EIT_TCP), sum, 1);

    /* Merge and fold precomputed cksum on ip header and new totallength */
   ip->checksum = fold_32bit_checksum((tcb->ip_cksum + (uint)ip->totallength));
}


/* Prepare a control packet, simply based on the current tcb state */

void xio_tcp_prepCtlPacket (struct tcb *tcb)
{
   if ((tcb->state != TCB_ST_LISTEN) && (tcb->state != TCB_ST_TIME_WAIT) && (tcb->state < TCB_ST_CLOSED) && (tcb->state != TCB_ST_SYN_SENT) && (tcb->state != TCB_ST_SYN_RECEIVED)) {
      xio_tcp_preparePacket (tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
   } else {
      dprintf ("xio_tcp warning: prepCtlPacket called for wierd state (%d)\n", tcb->state);
   }
}


/*
 * Prepare a response packet (with no tcb).
 */
static void xio_tcp_prepRespPacket (char *newpacket, char *origpacket, int flag, uint32 seqno, int ackno)
{
   struct eth *snd_eth = (struct eth *) &newpacket[XIO_EIT_ETH];
   struct ip *snd_ip = (struct ip *) &newpacket[XIO_EIT_IP];
   struct tcp *snd_tcp = (struct tcp *) &newpacket[XIO_EIT_TCP];
   struct eth *rcv_eth = (struct eth *) &origpacket[XIO_EIT_ETH];
   struct ip *rcv_ip = (struct ip *) &origpacket[XIO_EIT_IP];
   struct tcp *rcv_tcp = (struct tcp *) &origpacket[XIO_EIT_TCP];


   snd_tcp->flags = flag;
   snd_tcp->offset = (sizeof(struct tcp) >> 2) << 4;
   snd_tcp->seqno = htonl(seqno);
   snd_tcp->ackno = htonl(ackno);
   snd_tcp->window = 0;
   snd_tcp->urgentptr = 0;
   snd_tcp->dst_port = rcv_tcp->src_port;
   snd_tcp->src_port = rcv_tcp->dst_port;

   snd_ip->vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);
   snd_ip->tos = 0;
   snd_ip->fragoffset = 0;
   snd_ip->identification = 0;
   snd_ip->ttl = 0;
   snd_ip->protocol = IP_PROTO_TCP;
   snd_ip->source[0] = rcv_ip->destination[0];
   snd_ip->source[1] = rcv_ip->destination[1];
   snd_ip->destination[0] = rcv_ip->source[0];
   snd_ip->destination[1] = rcv_ip->source[1];

   snd_tcp->cksum = 0;
   /* Fill in pseudo header */
   snd_ip->ttl = 0;
   snd_ip->checksum = htons(sizeof(struct tcp));

   /* Compute cksum on pseudo header and tcp header */
   snd_tcp->cksum = inet_checksum ((uint16 *) &(snd_ip->ttl), (sizeof(struct tcp) + IP_PSEUDO_HDR_SIZE), 0, 1);

   /* Repair ip header, after screwing it up to construct pseudo header. */
   snd_ip->ttl = TCP_TTL;
   snd_ip->totallength = htons(sizeof(struct ip) + sizeof(struct tcp));

   snd_ip->checksum = 0;

   /* Compute cksum on ip header */
   snd_ip->checksum = inet_checksum((uint16 *) &(snd_ip->vrsihl), sizeof(struct ip), 0, 1);

   snd_eth->proto = htons(EP_IP);
   bcopy (rcv_eth->dst_addr, snd_eth->src_addr, 6);
   bcopy (rcv_eth->src_addr, snd_eth->dst_addr, 6);
}


static void xio_tcp_prepOptionsPacket (struct tcb *tcb, int flag, uint32 seqno, int len, int len2, int datasum)
{
   char *option = &tcb->snd_recv_r0_data[XIO_EIT_DATA];
   uint16 mss = htons(tcb->mss);

   option[0] = TCP_OPTION_MSS;
   option[1] = TCP_OPTION_MSS_LEN;
   memcpy (&option[2], &mss, sizeof(uint16));

   tcb->snd_recv_r0_sz += TCP_OPTION_MSS_LEN;
   xio_tcp_preparePacket (tcb, flag, seqno, len, len2, datasum);
   tcb->snd_recv_r0_sz -= TCP_OPTION_MSS_LEN;
   tcb->snd_recv_r1_data = option;
   tcb->snd_recv_r1_sz = TCP_OPTION_MSS_LEN;
   assert (tcb->snd_recv_n == 1);
   tcb->snd_recv_n = 2;
}


/* ------------------- action initiating external routines ------------------ */


int xio_tcp_prepDataPacket (struct tcb *tcb, char *addr, int sz, char *addr2, 
			    int sz2, uint flags, uint sum)
{
   int data_len = 0;
   int window;
   int len1;
   int n = sz + sz2;
   int close = flags & TCP_SEND_ALSOCLOSE;
   uint flag = TCP_FL_ACK | TCP_FL_PSH;

   if (n == 0) {
      if (close) {
         xio_tcp_initiate_close (tcb);
      }
      return (0);
   }

   STINC(tcpstats, nwrite);

#if 1		/* for debugging only */
    /* if not in expected state, print message, but construct packet anyway! */
   if ((!xio_tcp_writeable(tcb)) && (!xio_tcp_rewriting(tcb))) {
      dprintf ("preparing a data packet from a non-standard TCP state (%d)\n", tcb->state);
      dprintf ("snd_next %d, snd_max %d, sz %d, sz2 %d, send_offset %d\n", tcb->snd_next, tcb->snd_max, sz, sz2, tcb->send_offset);
      dprintf ("n %d, snd_una %d, snd_wnd %d, flags %x, close %d, mss %d\n", n, tcb->snd_una, tcb->snd_wnd, flags, close, tcb->mss);
   }
#endif

   tcb->snd_recv_r1_data = addr;
   tcb->snd_recv_r2_data = addr2;

   dprintf("xio_tcp_senddata %p: n %d una %u next %u snd_wnd %d\n", tcb, n, tcb->snd_una, tcb->snd_next, tcb->snd_wnd);

   /* Compute first how much space is left in the window. */
   window = xio_tcp_windowsz (tcb);

   dprintf ("xio_tcp_senddata: window %d, mss %d, close %d\n", window, tcb->mss, close);

   /* GROK -- need to do something for case of window < mss */
   /*         - answer may be to have a real mss and an effective mss */

   /* GROK -- clean up this legacy if statement */
   if ((window >= min(2,n)) && ((min(n,tcb->mss) <= window) || 
				(!(flags & TCP_SEND_MAXSIZEONLY)) || 
				((close) && (n < min(window,tcb->mss))))) {

	/* Compute number of bytes on segment: never more than the window
	 * allows us and never more than 1 MSS.
	 */
      data_len = min(n, window);
      data_len = min(data_len, tcb->mss);
	/* GROK -- is this needed for correct operation (see below)?? */
      //data_len = (data_len == n) ? n : (data_len & ~(0x1));

      assert (data_len > 0);

      len1 = min (data_len, sz);

      STINC(tcpstats, ndata);
      STINC_SIZE(tcpstats, snddata, log2(data_len));

      /* quick and dirty -- can be prettier */
	/* GROK -- will this function correctly if one of the lens is odd */
      if (sum == -1) {
         sum = 0;
         if (data_len-len1) {
            if (len1 & 0x1) {
               u_char border[2];
               sum = inet_checksum ((uint16 *)(&tcb->snd_recv_r2_data[1]), (data_len-len1-1), sum, 0);
               border[0] = tcb->snd_recv_r1_data[len1-1];
               border[1] = tcb->snd_recv_r2_data[0];
               sum = inet_checksum ((uint16 *)(&border[0]), 2, sum, 0);
            } else {
               sum = inet_checksum ((uint16 *)tcb->snd_recv_r2_data, (data_len-len1), sum, 0);
            }
         }
         if (len1) {
            if ((len1 & 0x1) && (data_len - len1)) {
               sum = inet_checksum((uint16 *)tcb->snd_recv_r1_data, (len1-1), sum, 0);
            } else {
               sum = inet_checksum((uint16 *)tcb->snd_recv_r1_data, len1, sum, 0);
            }
         }
      }

      if ((close && (data_len == n)) || (xio_tcp_finsent(tcb) && ((tcb->snd_next + data_len) == tcb->snd_fin))) {
         flag |= TCP_FL_FIN;
      }

      xio_tcp_preparePacket (tcb, flag, tcb->snd_next, len1, (data_len-len1), sum);

      if (flag & TCP_FL_FIN) {
         tcb->snd_fin = tcb->snd_next + data_len;
         tcb->snd_next++;
         if (tcb->state == TCB_ST_CLOSE_WAIT) {
            dprintf("xio_tcp_senddata %p: go to LAST_ACK\n", tcb);
            tcb->state = TCB_ST_LAST_ACK;
         } else if (tcb->state == TCB_ST_ESTABLISHED) {
            dprintf("xio_tcp_senddata %p: go to FIN_WAIT1\n", tcb);
            tcb->state = TCB_ST_FIN_WAIT1;
         } else if (xio_tcp_finsent(tcb)) {
            dprintf("xio_tcp_senddata %p: resending FIN (state %d)\n", tcb, tcb->state);
         } else {
            dprintf("xio_tcp_senddata %p: stay in current state (%d)\n", tcb, tcb->state);
            dprintf ("xio_tcp_senddata %p: must be retransmitting if in state %d\n", tcb, tcb->state);
         }
      }

      tcb->snd_next += data_len;
      tcb->snd_max = SEQ_MAX(tcb->snd_next, tcb->snd_max);
      tcb->send_offset += data_len;

   } else if ((tcb->snd_wnd == 0) && (tcb->flags & TCB_FL_DOWINDPROBE)) {
      /* Send a window probe with one-byte data */
      STINC(tcpstats, nprobesnd);
      xio_tcp_preparePacket (tcb, flag, tcb->snd_next, 1, 0, (uint)tcb->snd_recv_r1_data[0]);
      tcb->snd_max = SEQ_MAX((tcb->snd_next+1), tcb->snd_max);
   }
   tcb->flags &= ~TCB_FL_DOWINDPROBE;

   if ((SEQ_GT(tcb->snd_next, tcb->snd_una)) || ((tcb->snd_wnd == 0) && (SEQ_GEQ(tcb->snd_una, tcb->snd_max)))) { 
      timer_set(tcb, TCP_TIMEOUT(tcb));
   }

   return (data_len);
}


/*
 * Close connection: requires another three-way handshake.
 * Two cases:
 *	1. we have received a fin from the other side (CLOSE_WAIT)
 *	2. we are initiating the close and are waiting for a fin (FIN_WAIT1)
 * In eitehr cases, we prepare a fin to be sent. The fin has its own sequence
 * number.
 */
void xio_tcp_initiate_close(struct tcb *tcb)
{
   STINC(tcpstats, nclose);

   if (!xio_tcp_writeable(tcb)) {
	/* GROK -- this assert should not be here.  It is a temporary thing */
	/* to help in debugging an unknown occurrance of this problem...    */
      //      assert (0);
      return;
   }

   if (tcb->state == TCB_ST_CLOSE_WAIT) {
      dprintf("xio_tcp_initiate_close %p: go to LAST_ACK\n", tcb);
      tcb->state = TCB_ST_LAST_ACK;
   } else if (tcb->state == TCB_ST_ESTABLISHED) {
      dprintf("xio_tcp_initiate_close %p: go to FIN_WAIT1\n", tcb);
      tcb->state = TCB_ST_FIN_WAIT1;
   }

   assert (xio_tcp_finsent(tcb));

   timer_set(tcb, TCP_TIMEOUT(tcb));

   STINC(tcpstats, nfinsnd);
   xio_tcp_preparePacket(tcb, TCP_FL_ACK|TCP_FL_FIN, tcb->snd_next, 0, 0, 0);

   tcb->snd_fin = tcb->snd_next;
   tcb->snd_next++;	/* fin has a place in sequence space */
   tcb->snd_max = SEQ_MAX(tcb->snd_next, tcb->snd_max);
}


void xio_tcp_bindsrc (struct tcb *tcb, int portno, char *ip_addr, char *eth_addr)
{
   struct ip *ip = ((struct ip *) &tcb->snd_recv_r0_data[XIO_EIT_IP]);

   /* set src routing values in snd_recv */
   memcpy(((struct eth *) &tcb->snd_recv_r0_data[XIO_EIT_ETH])->src_addr, eth_addr, 6);
   memcpy(ip->source, ip_addr, sizeof (ip->source));
   ((struct tcp *) &tcb->snd_recv_r0_data[XIO_EIT_TCP])->src_port = htons((uint16)portno);
   
   tcb->ipdst = *((uint32 *)&ip_addr);
   tcb->tcpdst = htons((uint16)portno);
}


/*
 *  Start to open a connection to the predetermined server.
 */
void xio_tcp_initiate_connect (struct tcb *tcb, int srcportno, int dstportno, char *ip_addr, char *eth_addr)
{
   struct ip *ip = ((struct ip *) &tcb->snd_recv_r0_data[XIO_EIT_IP]);

   /* set routing values in snd_recv */
   memcpy (((struct eth *) &tcb->snd_recv_r0_data[XIO_EIT_ETH])->dst_addr, eth_addr, 6);
   memcpy (ip->destination, ip_addr, sizeof (ip->destination));
   ((struct tcp *) &tcb->snd_recv_r0_data[XIO_EIT_TCP])->dst_port = htons((uint16)dstportno);
   ((struct tcp *) &tcb->snd_recv_r0_data[XIO_EIT_TCP])->src_port = htons((uint16)srcportno);

   /* Set the precomputed IP checksums */
   ip->ttl = 0;
   ip->totallength = 0;
   ip->checksum = 0;
   tcb->pseudoIP_cksum = inet_checksum ((ushort *) &(ip->ttl), IP_PSEUDO_HDR_SIZE, 0, 0);
   ip->ttl = TCP_TTL;
   tcb->ip_cksum = inet_checksum ((ushort *) ip, sizeof(struct ip), 0, 0);

   /* new scheme: demultiplex in user space (reverse as will be in packets) */
   tcb->ipsrc = *((uint32 *)&ip->destination);
   tcb->ipdst = *((uint32 *)&ip->source);
   tcb->tcpdst = htons((uint16)srcportno);
   tcb->tcpsrc = htons((uint16)dstportno);

   /* Prepare a syn (the first step in the 3-way handshake), with MSS option. */
   tcb->mss = DEFAULT_MSS;
   xio_tcp_prepOptionsPacket (tcb, TCP_FL_SYN, tcb->snd_iss, 0, 0, 0);

   tcb->snd_next = tcb->snd_iss + 1;
   tcb->snd_max = tcb->snd_next;
   tcb->snd_una = tcb->snd_iss;
   tcb->state = TCB_ST_SYN_SENT;

   timer_set (tcb, TCP_TIMEOUT(tcb));
}


/* ---------------------------- timeout functions --------------------------- */


/*
 * Timeout while TCB is connecting.
 */
static int connect_timeout(struct tcb *tcb)
{
   uint flags = TCP_FL_SYN;
   if (tcb->state == TCB_ST_SYN_RECEIVED) {
      flags |= TCP_FL_ACK;
   }

   STINC(tcpstats, nretrans);
   STINC(tcpstats, nsynsnd);

   dprintf("retransmit connect (state %d)\n", tcb->state);

   /* Prepare syn (first step in the 3-way handshake), with mss option. */
   xio_tcp_prepOptionsPacket (tcb, flags, tcb->snd_iss, 0, 0, 0);

   timer_set(tcb, TCP_TIMEOUT(tcb));
   return (1);
}


/*
 * Timeout while sending data.
 */
static int write_timeout(struct tcb *tcb)
{

   /* If window is zero, we have data to send, and everything
    * has been acknowledged sofar, then send a window probe.
    *
    * If not all data has been acked, we provoke retransmission,
    * by resetting snd_next. If the window has shrunk and the
    * sender cannot retransmit, the code will get back here and
    * the sender will eventually send a probe.
    */

   if ((tcb->snd_wnd == 0) && (SEQ_GEQ(tcb->snd_una,tcb->snd_max))) {
      tcb->flags |= TCB_FL_DOWINDPROBE;
   }

   assert ((SEQ_GT(tcb->snd_max, tcb->snd_una)) || (tcb->snd_wnd == 0));
   assert ((int)(tcb->snd_next - tcb->snd_una) <= tcb->send_offset);
   tcb->send_offset -= tcb->snd_next - tcb->snd_una;
   tcb->snd_next = tcb->snd_una;
   xio_tcp_preparePacket (tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
   return (1);
}


/*
 * Timeout while closing the connection.
 */
static int
close_timeout(struct tcb *tcb)
{

   /* reset snd_next and try again */
   assert ((int)(tcb->snd_next - tcb->snd_una) <= (tcb->send_offset+1));
   STINC(tcpstats, nretrans);
   if (tcb->snd_next == (tcb->snd_una + 1)) {
      STINC(tcpstats, nfinsnd);
      xio_tcp_preparePacket (tcb, TCP_FL_ACK|TCP_FL_FIN, tcb->snd_una, 0, 0, 0);
   } else {
      if ((tcb->snd_fin) && (tcb->snd_next == (tcb->snd_fin+1))) {
         tcb->snd_next--;		/* adjust for FIN bit */
      }
      write_timeout (tcb);
   }

   timer_set(tcb, TCP_TIMEOUT(tcb));
   return (1);
}


/*
 * TCP tcb received a timeout.  The return value indicates whether or not
 * any TCB state has been modified/affected (0 says no).
 */
int xio_tcp_timeout(struct tcb *tcb)
{
   int ret = 0;
   dprintf("tcb %p: time out; state %d\n", tcb, tcb->state);


   timer_unset (tcb);

   tcb->retries++;
   if (tcb->retries > TCP_RETRY) { /* is the connection broken? */
      int ack = (tcb->state != TCB_ST_SYN_SENT) ? TCP_FL_ACK : 0;
      xio_tcp_preparePacket(tcb, TCP_FL_RST|ack, tcb->snd_una, 0, 0, 0);
      tcb->state = TCB_ST_CLOSED;
      kprintf ("xio_tcp_timeout: closing connection due to retries\n");
      return (1);
   }

   switch(tcb->state) {
   case TCB_ST_SYN_SENT:
   case TCB_ST_SYN_RECEIVED:
      ret = connect_timeout(tcb);
      break;
   case TCB_ST_ESTABLISHED:
   case TCB_ST_CLOSE_WAIT:
      ret = write_timeout(tcb);
      break;
   case TCB_ST_LAST_ACK:
   case TCB_ST_FIN_WAIT1:
   case TCB_ST_FIN_WAIT2:
   case TCB_ST_CLOSING:
      ret = close_timeout(tcb);
      break;
   case TCB_ST_TIME_WAIT:
      tcb->state = TCB_ST_CLOSED;
      ret = 1;
      break;
   case TCB_ST_CLOSED:
      break;
   default:
      kprintf("tcp_timeout %p: unexpected timeout in state %d\n", tcb, tcb->state);
      assert (0);
      break;
   }

   return (ret);
}


/* ------------------------ packet handling functions ----------------------- */


/* 
 * Deal with options. We check only for MSS option. Heh, processing
 * one option is better than processing no options.
 * If there are no options, TCP_MSS is used and process_options returns 0;
 */
static int process_options(struct tcb *tcb, char *option)
{
   uint16 mss;

   dprintf("process_options: options present %d\n", (int) option[0]);
   if (option[0] == TCP_OPTION_MSS) {
      switch(option[1]) {
         case 4:			/* kind, length, and a 2-byte mss */
            memcpy(&mss, &option[2], sizeof(short));
            mss = ntohs(mss);
            dprintf("proposed mss %d we %d\n", mss, DEFAULT_MSS);
            tcb->mss = min(DEFAULT_MSS, mss);
            return 1;
         default:
            dprintf("process_option: unrecognized length %d\n", option[1]);
            return 0;
      }
   }
   return 0;
}


static void xio_tcp_prepSynRespPacket (struct tcb *tcb, char *packet)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   int hlen = ((tcp->offset >> 4) & 0xF) << 2;
   int mss_option = 0;
   if (hlen > sizeof(struct tcp)) { /* options? */
      mss_option = process_options(tcb, &packet[XIO_EIT_DATA]);
   }
   /* Send an syn + ack (the second step in the 3-way handshake). */
   if (mss_option) {
      /* Append options to reply. */
      xio_tcp_prepOptionsPacket (tcb, TCP_FL_SYN|TCP_FL_ACK, tcb->snd_iss, 0, 0, 0);
   } else {
      xio_tcp_preparePacket (tcb, TCP_FL_SYN|TCP_FL_ACK, tcb->snd_iss, 0, 0, 0);
   }
}


/*
 * First check of RFC: check is the segment is acceptable.
 */
static int process_acceptable(struct tcb *tcb, char *packet)
{
   struct ip *ip = (struct ip *) &packet[XIO_EIT_IP];
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   int hlen = ((tcp->offset >> 4) & 0xF) << 2;
   int len;

   len = ip->totallength - sizeof(struct ip) - hlen;

/*
   if (tcp->flags & TCP_FL_SYN) {
      len++;
   }
*/
   if (tcp->flags & TCP_FL_FIN) {
      len++;
   }

    /* Acceptable? */
    if (
	((len == 0) && (tcb->rcv_wnd == 0) && (tcp->seqno == tcb->rcv_next)) ||

	/* GROK -- the LEQ on the last part of this check is LT in the spec */
	/* why does Frans want it to be a LEQ?? */
	((len == 0) && (tcb->rcv_wnd > 0) && 
	 (SEQ_GEQ(tcp->seqno, tcb->rcv_next) && 
	  SEQ_LT(tcp->seqno, tcb->rcv_next + tcb->rcv_wnd))) ||

	((len > 0) && (tcb->rcv_wnd > 0) &&
	 ((SEQ_GEQ(tcp->seqno, tcb->rcv_next) && 
	   SEQ_LT(tcp->seqno, tcb->rcv_next + tcb->rcv_wnd)) ||
	  (SEQ_GEQ(tcp->seqno + len - 1, tcb->rcv_next) &&
	   SEQ_LT(tcp->seqno + len - 1, tcb->rcv_next + tcb->rcv_wnd))))) {

	STINC(tcpstats, naccept);

	/* Yes, acceptable */
	return 1;

   } else {
      /* No, not acceptable: if not rst, send ack. */

      STINC(tcpstats, nunaccept);

#if 0
      printf("process_acceptable %d: segment %x not accepted; expect %x (tcb->state %d)\n", len, tcp->seqno, tcb->rcv_next, tcb->state);
      printf ("   rcv_wnd %d, len %d, hlen %d, flags %x, rcv_irs %x\n", tcb->rcv_wnd, len, hlen, tcp->flags, tcb->rcv_irs);
#endif
      if (!(tcp->flags & TCP_FL_RST)) {
         STINC(tcpstats, nacksnd);
         if ((tcp->flags & TCP_FL_SYN) && (!(tcp->flags & TCP_FL_ACK)) && (tcp->seqno == tcb->rcv_irs)) {
            xio_tcp_prepSynRespPacket (tcb, packet);
         } else {
            xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
         }
      }
      return 0;
   }
}


/*
 * Check 4 and first part of 5 of RFC.
 */
static int process_syn_ack(struct tcb *tcb, char *packet)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

   if (tcp->flags & TCP_FL_SYN) {
      STINC(tcpstats, nsynrcv);
      STINC(tcpstats, nrstsnd);
      dprintf ("process_syn_ack: unexpected SYN in state %d\n", tcb->state);
      if (tcp->flags & TCP_FL_ACK) {
         xio_tcp_preparePacket(tcb, TCP_FL_RST|TCP_FL_ACK, tcp->ackno, 0, 0, 0);
      } else {
         xio_tcp_preparePacket(tcb, TCP_FL_RST, 0, 0, 0, 0);
      }
      tcb->send_important = 0;
      tcb->state = TCB_ST_CLOSED;
      return 0;
   }

   if (!(tcp->flags & TCP_FL_ACK)) return 0;

   return 1;
}


/*
 * Check 2 of RFC and call function for checks 4 and first part 5. Check 3
 * is for security stuff, which we do not implement.
 */
static int process_rst_syn_ack(struct tcb *tcb, char *packet)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

   if (tcp->flags & TCP_FL_RST) {
      STINC(tcpstats, nrstrcv);
      tcb->state = TCB_ST_CLOSED;
      return 0;
   }

   return process_syn_ack(tcb, packet);
}


/* 
 * The last part of check 5 of RFC.
 */
static void process_ack(struct tcb *tcb, char *packet)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

   STINC(tcpstats, nack);

   /* If an acknowledgement and it is interesting, update una. */
   if ((tcp->flags & TCP_FL_ACK) && SEQ_GEQ(tcp->ackno, tcb->snd_una) && SEQ_LEQ(tcp->ackno, tcb->snd_max)) { 

      dprintf("process_ack %x ack %d: una %u next %u, max %u\n", (uint)tcb, tcp->ackno, tcb->snd_una, tcb->snd_next, tcb->snd_max);

      tcb->snd_una = tcp->ackno;

      /* XXX shouldn't we only update rtt if this is an ack to the last
	 packet we sent? */

      xio_update_rtt (tcb, tcp->ackno);

      /* this deals with other side buffering packets out of order, even if */
      /* we had to resort to retransmission.                                */
      if (SEQ_GT(tcb->snd_una, tcb->snd_next)) {
         assert (SEQ_LT(tcb->snd_next, tcb->snd_max));

         tcb->send_offset += tcb->snd_una - tcb->snd_next;
         tcb->snd_next = tcb->snd_una;
      }
      assert (SEQ_LEQ(tcb->snd_una, tcb->snd_next));
      assert (SEQ_LEQ(tcb->snd_next, tcb->snd_max));

      /* Update time out event; we have received an interesting ack. */

      /* XXX -- fixme: this should compare against tcb->snd_next
	 instead of snd_max */

      if (tcb->snd_una == tcb->snd_max) {
         timer_unset (tcb);
      } else {
         timer_set(tcb, TCP_TIMEOUT(tcb));
      }
   }
    
    /* If an acknowledgement and it is interesting, update snd_wnd. */
   if ((tcp->flags & TCP_FL_ACK) && (SEQ_LT(tcb->snd_wls, tcp->seqno) ||
       (tcb->snd_wls == tcp->seqno && SEQ_LT(tcb->snd_wla, tcp->ackno)) ||
       (tcb->snd_wla == tcp->ackno && tcp->window > tcb->snd_wnd))) {

      dprintf("process_ack %x: old wnd %u wnd update %u\n", (uint)tcb, tcb->snd_wnd, tcp->window);
      tcb->snd_wnd = tcp->window;
      tcb->snd_wls = tcp->seqno;
      tcb->snd_wla = tcp->ackno;
   }
}


/*
 * Process incoming data. fin records is a the last byte has been sent.
 * Process_data returns whether all data has been received
 */
static int process_data(struct tcb *tcb, char *packet, int fin, int flags)
{
   struct ip *ip = (struct ip *) &packet[XIO_EIT_IP];
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   char *data = &packet[XIO_EIT_DATA];
   uint datalen;
   int off;
   int all;
   int hlen = ((tcp->offset >> 4) & 0xF) << 2;

   all = 0;
   datalen = ip->totallength - sizeof(struct ip) - hlen;

   if (fin) STINC(tcpstats, nfinrcv);
   if (datalen > 0) {
      STINC_SIZE(tcpstats, rcvdata, log2(datalen));
      STINC(tcpstats, ndatarcv);
   }

   dprintf("process_data: %d bytes user data\n", datalen);

   /* XXX hhmmm, so what would happen with a packet with data we've already seen
      but now a FIN too? should we bump datalen by one if there's a fin like process_acceptable does? */

   if (SEQ_LEQ(tcp->seqno, tcb->rcv_next) && SEQ_GEQ(tcp->seqno + datalen, tcb->rcv_next)) {
	/* Segment contains interesting data */
      if (datalen > 0) {

	/* Copy the right data */
         STINC(tcpstats, norder);
         off = tcb->rcv_next - tcp->seqno;

         tcb->indata = data;
         tcb->indatalen = datalen;
         tcb->inoffset = off;
         assert (xio_tcp_getLiveInDataLen(tcb) == max(0,(datalen - off)));
         if (off >= 0) {
            tcb->rcv_next += (datalen - off);
            if ((off >= 0) && (flags & TCP_RECV_ADJUSTRCVWND)) {
               tcb->rcv_wnd = max (0, (tcb->rcv_wnd-(datalen-off)));
            }
         }

	/* Check for a fin */
         if (fin) {
            dprintf("process_data: process fin\n");
            tcb->rcv_next++;
            all = 1;
            STINC(tcpstats, nacksnd);
            xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
         } else {
	/* GROK - what about PSH flags?? */
#ifdef USEDELACKS
            if (tcb->flags & TCB_FL_DELACK) {
               STINC(tcpstats, nacksnd);
               xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
               tcb->flags &= ~TCB_FL_DELACK;
            } else {
	   /* Delay ack */
               tcb->flags |= TCB_FL_DELACK;
               STINC(tcpstats, ndelayack);
            }
#else
            xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
#endif
	 }

      } else if (fin) {
	/* A segment just carrying fin. */
         dprintf("process_data: process fin\n");
         tcb->rcv_next++;
         all = 1;
         STINC(tcpstats, nacksnd);
         xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
      }
   } else if (SEQ_GT(tcp->seqno, tcb->rcv_next)) {
	/* Segment contains data past rcv_next, keep it.  For now, drop it. */
      STINC(tcpstats, noutorder);
      dprintf("process_data %p: segment seq %d is out of order n %d\n", tcb, tcp->seqno, tcb->rcv_next);
   } else {
      dprintf ("tcb %p, state %d, tcpflags %x, seqno %d, rcv_next %d datalen %d\n", tcb, tcb->state, tcp->flags, tcp->seqno, tcb->rcv_next, datalen);
	/* process_acceptable should have filtered this segment */
      assert (0);
   }
   return all;
}

/* -------------------------------------------------------------------------- */


static void st_illegal(struct tcb *tcb, char *packet, int flags)
{
    printf("tcp_process %d: illegal state to receive msgs in: %d\n", 
	   (uint)tcb, tcb->state);

    xio_tcp_printtcb (tcb);
}


/*
 * We have sent our syn. Wait for an ack and a syn from the other side.
 */
static void st_syn_sent(struct tcb *tcb, char *packet, int flags)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   char *option = &packet[XIO_EIT_DATA];
   int hlen;

   dprintf("st_synsent\n");

   /* Check ack; if not acceptable send rst */
   if ((tcp->flags & TCP_FL_ACK) && (SEQ_LEQ(tcp->ackno, tcb->snd_iss) ||
                                     SEQ_GT(tcp->ackno, tcb->snd_next))) {
      STINC(tcpstats, nrstsnd);
      dprintf("st_syn_sent: bad ACK on SYN\n");
      xio_tcp_preparePacket(tcb, TCP_FL_RST|TCP_FL_ACK, tcp->ackno, 0, 0, 0);
      tcb->send_important = 0;
      return;
   }

   /* Check for rst, and drop the connection */
   if (tcp->flags & TCP_FL_RST) {
      STINC(tcpstats, nrstrcv);
      tcb->state = TCB_ST_CLOSED;
      dprintf("st_synsent: RST received -- dropping connection\n");
      return;
   }

   /* Check for syn and set irs and next */
   if (tcp->flags & TCP_FL_SYN) {
      STINC(tcpstats, nsynrcv);
      tcb->rcv_irs = tcp->seqno;
      tcb->rcv_next = tcp->seqno + 1; /* y + 1 */
   }

   /* Acceptable ack, set una and window variables */
   if (tcp->flags & TCP_FL_ACK) {
      STINC(tcpstats, nack);
      tcb->snd_una = tcp->ackno;
      xio_update_rtt (tcb, tcp->ackno);
      tcb->snd_wnd = tcp->window;
      tcb->snd_wls = tcp->seqno;
   }

   /* Check if our syn has been acked, if so go to ESTABLISHED. */
   if (SEQ_GT(tcb->snd_una, tcb->snd_iss)) {
      /* Our syn has been acked */
      dprintf("st_syn_sent %x: go to ESTABLISHED\n", (uint)tcb);
      tcb->state = TCB_ST_ESTABLISHED;

      /* Reset timer */
      timer_unset (tcb);

      hlen = ((tcp->offset >> 4) & 0xF) << 2;
      if (hlen > sizeof(struct tcp)) { /* options? */
          process_options(tcb, option);
      }

      /* Complete handshake: send ack */
      STINC(tcpstats, nacksnd);
      xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
   } else {
      /* Received a syn on our syn but without ack, send ack on other side's
       * syn.
       */
      dprintf("st_syn_sent %x: go to SYN_RECEIVED\n", (uint)tcb);
      printf ("st_syn_sent %x: go to SYN_RECEIVED\n", (uint)tcb);
      tcb->state = TCB_ST_SYN_RECEIVED;
      /* Send seq == iss, ack == rcv_next. Perhaps send options too. */
      STINC(tcpstats, nacksnd);
      STINC(tcpstats, nsynsnd);
      xio_tcp_preparePacket(tcb, TCP_FL_SYN|TCP_FL_ACK, tcb->snd_iss, 0, 0, 0);
   }
}


/*
 * We have received a syn s = x, we have sent syn y ack x +1, and are now
 * waiting for ack s = x, a = y + 1.
 * Or, we have sent a syn, and have received a syn, but our still
 * waiting for the ack on our syn.
 */
static void st_syn_received(struct tcb *tcb, char *packet, int flags)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

   dprintf("st_synrecv (flags %x)\n", tcp->flags);
/*
   printf ("st_synrecv (flags %x, irs %x, seqno %x, rcvnext %x)\n", tcp->flags, tcb->rcv_irs, tcp->seqno, tcb->rcv_next);
*/

   if (!process_acceptable(tcb, packet)) return;

   if (!process_rst_syn_ack(tcb, packet)) return;

   /* If acceptable ack, move to established; otherwise send reset */
   if ((tcp->flags & TCP_FL_ACK) && SEQ_GEQ(tcp->ackno, tcb->snd_una) && SEQ_LEQ(tcp->ackno, tcb->snd_next)) {
      dprintf("st_syn_received: go to ESTABLISHED\n");
      tcb->snd_una = tcp->ackno;
      xio_update_rtt (tcb, tcp->ackno);
      tcb->snd_wnd = tcp->window;
      tcb->snd_wls = tcp->seqno;
      tcb->snd_wla = tcp->ackno;
      tcb->state = TCB_ST_ESTABLISHED;
   } else {
      dprintf("st_syn_received: send RST\n");
      dprintf ("st_syn_received: send RST\n");
      xio_tcp_preparePacket(tcb, TCP_FL_RST, tcp->ackno, 0, 0, 0);
      tcb->send_important = 0;
      tcb->state = TCB_ST_CLOSED;
      return;
   }
    
   process_ack(tcb, packet);

   if (process_data(tcb, packet, tcp->flags & TCP_FL_FIN, flags)) {
      dprintf("st_syn_received: go to ST_CLOSE_WAIT (flags %x)\n", tcp->flags);
      tcb->state = TCB_ST_CLOSE_WAIT;
   }
}


/*
 * Connection is established; deal with incoming packet.
 */
static void st_established(struct tcb *tcb, char *packet, int flags)
{
    struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

    dprintf("st_established\n");

    if (!process_acceptable(tcb, packet)) return;

    if (!process_rst_syn_ack(tcb, packet)) return;

    process_ack(tcb, packet);

    if (process_data(tcb, packet, tcp->flags & TCP_FL_FIN, flags)) {
	dprintf("st_established: go to ST_CLOSE_WAIT\n");
      	tcb->state = TCB_ST_CLOSE_WAIT;
    }
}


/*
 * We have received a fin and acked it, but our side hasn't sent a fin
 * yet, since tcp_close has not been called yet.
 * Update the send window; we could still be sending data.
 */
static void st_close_wait(struct tcb *tcb, char *packet, int flags)
{
   dprintf("st_closewait\n");

   if ((process_acceptable(tcb, packet)) && (process_rst_syn_ack(tcb, packet))) {
      process_ack(tcb, packet);
   }
}


/*
 * We have called tcp_close and we are waiting for an ack.
 * Process data and wait for the other side to close.
 */
static void st_fin_wait1(struct tcb *tcb, char *packet, int flags)
{
    struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

    dprintf("st_finwait1: tcp->flags %x\n", tcp->flags);
    
    if (!process_acceptable(tcb, packet)) return;

    if (!process_rst_syn_ack(tcb, packet)) return;

    process_ack(tcb, packet);

    if (SEQ_GEQ(tcb->snd_una, tcb->snd_max)) {
	dprintf("st_fin_wait1 %x: go to FIN_WAIT2\n", (uint)tcb);
	tcb->state = TCB_ST_FIN_WAIT2;
    }

    if (process_data(tcb, packet, tcp->flags & TCP_FL_FIN, flags)) {
	if(tcb->state == TCB_ST_FIN_WAIT2) {
	    dprintf("st_fin_wait1 %x: go to TIME_WAIT\n", (uint)tcb);
	    tcb->state = TCB_ST_TIME_WAIT;
	} else {
	    dprintf("st_fin_wait1 %x: go to CLOSING\n", (uint)tcb);
	    tcb->state = TCB_ST_CLOSING;
	}
    }
}


/*
 * We have sent a fin and received an ack on it.
 * Process data and wait for the other side to close.
 */
static void st_fin_wait2(struct tcb *tcb, char *packet, int flags)
{
    struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
    
    dprintf("st_finwait2: tcp->flags %x\n", tcp->flags);

    if (!process_acceptable(tcb, packet)) return;
    
    if (!process_rst_syn_ack(tcb, packet)) return;
    
    process_ack(tcb, packet);
    
    if (process_data(tcb, packet, tcp->flags & TCP_FL_FIN, flags)) {
	dprintf("st_fin_wait2 %x: go to TIME_WAIT\n", (uint)tcb);
	tcb->state = TCB_ST_TIME_WAIT;
    }
}



/*
 * Both sides have sent fin. We have acked the other side's fin.
 * Wait until our fin is acked.
 */
static void st_last_ack(struct tcb *tcb, char *packet, int flags)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
    
   dprintf("st_lastack\n");
    
   if (!process_acceptable(tcb, packet)) return;

   if (tcp->flags & TCP_FL_RST) {
      tcb->state = TCB_ST_CLOSED;
      return;
   }

   if (!process_syn_ack(tcb, packet)) return;
    
   process_ack(tcb, packet);
    
   if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
      dprintf("st_last_ack %x: go to ST_CLOSED\n", (uint)tcb); 
      tcb->state = TCB_ST_CLOSED;
   }
}


/*
 * We sent out a fin and have received a fin in response. We
 * are waiting for the ack.
 */
static void st_closing(struct tcb *tcb, char *packet, int flags)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];

   dprintf("st_closting\n");

   if (!process_acceptable(tcb, packet)) return;

   if (tcp->flags & TCP_FL_RST) {
      tcb->state = TCB_ST_CLOSED;
      return;
   }

   if (!process_syn_ack(tcb, packet)) return;

   process_ack(tcb, packet);

   if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
      dprintf("st_closing %x: received ack segment, to to TIME_WAIT\n", (uint)tcb);
      tcb->state = TCB_ST_TIME_WAIT;
   }
}


/*
 * We have sent a fin and have received an ack on it. We also have
 * received a fin from the other side and acked it.
 */
static void st_time_wait(struct tcb *tcb, char *packet, int flags)
{
   struct ip *ip = (struct ip *) &packet[XIO_EIT_IP];
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   int len = ip->totallength - sizeof(struct ip) - (((tcp->offset >> 4) & 0xF) << 2);

   dprintf("st_time_wait\n");

   if ((len != 0) || (tcp->seqno != tcb->rcv_next)) {
		/* ignoring SYN packets to emulate BSD behavior */
      if (!(tcp->flags & (TCP_FL_RST|TCP_FL_SYN))) {
         STINC(tcpstats, nacksnd);
         xio_tcp_prepRespPacket (tcb->snd_recv_r0_data, packet, TCP_FL_ACK, tcb->snd_next, tcb->rcv_next);
         tcb->send_ready = TCP_FL_ACK;
      }
      if (tcp->flags & TCP_FL_SYN) {
	dprintf ("dropping SYN packet for connection in TIME_WAIT state\n");
      }
      return;
   }

   if (tcp->flags & TCP_FL_RST) {
      tcb->state = TCB_ST_CLOSED;
      return;
   }

#if 0	/* BSD doesn't reset in this case.  let 'em retransmit */
   if (tcp->flags & TCP_FL_SYN) {
      xio_tcp_prepRespPacket (tcb->snd_recv_r0_data, packet, TCP_FL_RST, tcb->snd_next, tcb->rcv_next);
      tcb->send_ready = TCP_FL_RST;
      return;
   }
#endif

   /* Check for a retransmission of a fin, and send an ack. */
   if (tcp->flags & TCP_FL_FIN) {
      xio_tcp_prepRespPacket (tcb->snd_recv_r0_data, packet, TCP_FL_ACK, tcb->snd_next, tcb->rcv_next);
      tcb->send_ready = TCP_FL_ACK;
   }
}


/* GROK -- change this to not need write access to the packet! */
int xio_tcp_packetcheck (char *packet, int len, int *datasum)
{
   struct tcp *rcv_tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   struct ip *rcv_ip = (struct ip *) &packet[XIO_EIT_IP];
   char *data = &packet[XIO_EIT_DATA];
   int totallength;
   uint16 n;
   uint ipsum;

   if (len < XIO_EIT_DATA) {
      dprintf ("packet too short (len %d)\n", len);
      return (0);
   }

   ipsum = rcv_ip->checksum;
   if (ipsum != 0) {
      /* Check ip cksum */
      uint sum2;
      rcv_ip->checksum = 0;
      sum2 = inet_checksum((uint16 *) &(rcv_ip->vrsihl), sizeof(struct ip), 0, 1);
      rcv_ip->checksum = ipsum;
      if (sum2 != ipsum) {
         STINC(tcpstats, nbadipcksum);
         dprintf ("xio_tcp_packetcheck: bad ip checksum (sum %x, rcv_ip_checksum %x\n", sum2, ipsum);
         return(0);
      }
   }

   totallength = ntohs(rcv_ip->totallength);

   STINC(tcpstats, nseg);

   /* We expect that tcp segments will never be fragmented since the
    * other end is not to send something bigger than the negotiated MSS.
    * So, no support for IP fragmentation!!!
    */
   if (totallength > len) {
	// || rcv_ip->fragoffset != 0) {
      /* Segment is not complete */
      STINC(tcpstats, nincomplete);
      dprintf ("tcp_proces_packetcheck: segment with len %d (packetlen %d) off %d is not complete\n", totallength, len, ntohs(rcv_ip->fragoffset));
      return(0);
   }

   if (rcv_tcp->cksum != 0) {
      /* Repair pseudo header */
      uint tcpsum = rcv_tcp->cksum;
      uint sum2;
      uint8 ipttl = rcv_ip->ttl;
      rcv_tcp->cksum = 0;
      n = totallength - sizeof(struct ip);
      rcv_ip->ttl = 0;
      rcv_ip->checksum = htons(n);

      /* Check checksum on pseudo header */
      sum2 = inet_checksum((uint16 *) data, n - sizeof(struct tcp), 0, 0);
      if (datasum) { *datasum = sum2; }
      sum2 = inet_checksum((uint16 *) &(rcv_ip->ttl), IP_PSEUDO_HDR_SIZE + sizeof(struct tcp), sum2, 1);
      rcv_tcp->cksum = tcpsum;
      rcv_ip->checksum = ipsum;
      rcv_ip->ttl = ipttl;
      if (sum2 != tcpsum) {
         STINC(tcpstats, nbadtcpcksum);
         dprintf ("xio_tcp_packetcheck: bad tcp inet checksum: sum %x, rcv_tcp->cksum %x, tcplen %d, totallen %d, data %p, packet %p\n", sum2, tcpsum, n, totallength, data, packet);
         dprintf ("ackno %d, seqno %d, tcpdst %x, tcpsrc %x\n", (uint)ntohl(rcv_tcp->ackno), (uint)ntohl(rcv_tcp->seqno), ntohs(rcv_tcp->dst_port), ntohs(rcv_tcp->src_port));
         return(0);
      }
   } else if (datasum) {
      *datasum = -1;
   }

   return (1);
}


/* packet handlers for each of the valid TCP connection states */

static void (*packet_handlers[NSTATES])(struct tcb *tcb, char *packet, int flags) = {
    st_illegal,
    st_illegal,
    st_syn_sent,
    st_syn_received,
    st_established,
    st_fin_wait1,
    st_fin_wait2,
    st_close_wait,
    st_closing,
    st_last_ack,
    st_time_wait,
    st_illegal,
};


/*
 * Process a segment. The segment has been received in rcv_eit.
 */
void xio_tcp_handlePacket(struct tcb *tcb, char *packet, uint flags)
{
   struct tcp *tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   struct ip *rcv_ip = (struct ip *) &packet[XIO_EIT_IP];

   dprintf ("tcp_process: tcb %p, r %p, tcp %p, ip %p\n", tcb, packet, tcp, rcv_ip);

   tcb->inpackets++;
   tcb->indata = NULL;

   rcv_ip->totallength = ntohs(rcv_ip->totallength);
   tcp->ackno = ntohl(tcp->ackno);
   tcp->seqno = ntohl(tcp->seqno);
   tcp->window = ntohs(tcp->window);

   dprintf ("tcp_process: src_port %d, ackno %u, seqno %u, window %u, flag %x\n", (uint)ntohs(tcp->src_port), tcp->ackno, tcp->seqno, tcp->window, tcp->flags);
   dprintf ("for comparison: snd_una %d, snd_wnd %d, snd_wls %d, snd_wla %d\n", tcb->snd_una, tcb->snd_wnd, tcb->snd_wls, tcb->snd_wla);

    tcb->retries = 0;		/* reset retries */

#ifndef NO_HEADER_PREDICTION
    /* Header Prediction */
   if ((tcb->state == TCB_ST_ESTABLISHED) && (tcp->seqno == tcb->rcv_next) &&
       (!(tcp->flags & (TCP_FL_FIN | TCP_FL_SYN | TCP_FL_RST | TCP_FL_URG)))) {
      char *data = &packet[XIO_EIT_DATA];
      int datalen = rcv_ip->totallength - sizeof(struct ip) - sizeof(struct tcp);

	/* Test for expected ack */
      if (SEQ_GEQ(tcp->ackno, tcb->snd_una) && SEQ_LEQ(tcp->ackno, tcb->snd_next) && (tcb->snd_next == tcb->snd_max)) {
         STINC(tcpstats, nackprd);
         tcb->snd_una = tcp->ackno;
	 xio_update_rtt (tcb, tcp->ackno);

	/* Update time out event; we have received an interesting ack */
         if (tcb->snd_una == tcb->snd_next) {
            timer_unset (tcb);
         } else {
            timer_set(tcb, TCP_TIMEOUT(tcb));
         }

	/* If an acknowledgement and it is interesting, update snd_wnd. */
         if ((tcp->flags & TCP_FL_ACK) && (SEQ_LT(tcb->snd_wls, tcp->seqno) ||
             (tcb->snd_wls == tcp->seqno && SEQ_LT(tcb->snd_wla, tcp->ackno)) ||
             (tcb->snd_wla == tcp->ackno && tcp->window > tcb->snd_wnd))) {
            tcb->snd_wnd = tcp->window;
            tcb->snd_wls = tcp->seqno;
            tcb->snd_wla = tcp->ackno;
         }

	/* If datalen is 0, it was a pure ack; return. */
         if (datalen == 0) return;
      }

	/* Test for expected data */
      if (datalen > 0 && tcp->ackno == tcb->snd_una) {
         STINC(tcpstats, ndataprd);
         STINC_SIZE(tcpstats, rcvdata, log2(datalen));

         tcb->indata = data;
         tcb->indatalen = datalen;
         tcb->inoffset = 0;
         assert (xio_tcp_getLiveInDataLen(tcb) == datalen);
         tcb->rcv_next += datalen;
         if (flags & TCP_RECV_ADJUSTRCVWND) {
            tcb->rcv_wnd = max (0, (tcb->rcv_wnd-datalen));
         }

	/* GROK -- what about PSH flags?? */
#ifdef USEDELACKS
         if (tcb->flags & TCB_FL_DELACK) {
            STINC(tcpstats, nacksnd);
            xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
            tcb->flags &= ~TCB_FL_DELACK;
         } else {
            tcb->flags |= TCB_FL_DELACK;
            STINC(tcpstats, ndelayack);
         }
#else
         xio_tcp_preparePacket(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, 0);
#endif

         return;
      } 
   }
#endif

   packet_handlers[tcb->state](tcb, packet, flags);
}


void xio_tcp_handleRandPacket (struct tcb *tmptcb, char *packet)
{
   struct tcp *rcv_tcp = (struct tcp *) &packet[XIO_EIT_TCP];

	/* Send a RST only if the packet contains an ACK. */
   if ((!(rcv_tcp->flags & TCP_FL_RST)) && (rcv_tcp->flags & TCP_FL_ACK)) {
      //kprintf ("(%d) handleRandPacket: received an ACK -- sending a RST (src %d, dst %d)\n", geteid(), ntohs(rcv_tcp->src_port), ntohs(rcv_tcp->dst_port));
      xio_tcp_prepRespPacket (tmptcb->snd_recv_r0_data, packet, TCP_FL_RST, ntohl(rcv_tcp->ackno), ntohl(rcv_tcp->seqno));
      tmptcb->snd_recv_n = 1;
      tmptcb->send_ready = TCP_FL_RST;
   } else {
      //kprintf ("handleRandPacket: RST or !SYN (flags 0x%x) -- toss it\n", rcv_tcp->flags);
   }
}


/*
 * Process a segment. The segment has been received in rcv_eit.
 */
int xio_tcp_handleListenPacket(struct tcb *new_tcb, char *packet)
{
   struct eth *rcv_eth = (struct eth *) packet;
   struct ip *rcv_ip = (struct ip *) &packet[XIO_EIT_IP];
   struct tcp *rcv_tcp = (struct tcp *) &packet[XIO_EIT_TCP];
   struct ip *new_ip;

   dprintf("xio_tcp_handleListenPacket: new_tcp %p, packet %p\n",new_tcb,packet);

   /* Check for rst and ignore */
   if ((rcv_tcp->flags & (TCP_FL_RST|TCP_FL_ACK)) || 
       (!(rcv_tcp->flags & TCP_FL_SYN))) 
   {
      xio_tcp_handleRandPacket (new_tcb, packet);
      new_tcb->state = TCB_ST_CLOSED;
      return (0);
   }

   /* don't change packet until sure we're taking it!!! */
   rcv_ip->totallength = ntohs(rcv_ip->totallength);
   rcv_tcp->ackno = ntohl(rcv_tcp->ackno);
   rcv_tcp->seqno = ntohl(rcv_tcp->seqno);
   rcv_tcp->window = ntohs(rcv_tcp->window);

   new_ip = ((struct ip *) &new_tcb->snd_recv_r0_data[XIO_EIT_IP]);
   /* set client routing values in snd_recv */
   memcpy(((struct eth *) &new_tcb->snd_recv_r0_data[XIO_EIT_ETH])->dst_addr, rcv_eth->src_addr, sizeof rcv_eth->src_addr);
   new_ip->destination[0] = rcv_ip->source[0];
   new_ip->destination[1] = rcv_ip->source[1];
   ((struct tcp *) &new_tcb->snd_recv_r0_data[XIO_EIT_TCP])->dst_port = rcv_tcp->src_port;

   /* set server routing values in snd_recv */
   memcpy(((struct eth *) &new_tcb->snd_recv_r0_data[XIO_EIT_ETH])->src_addr, rcv_eth->dst_addr, sizeof rcv_eth->dst_addr);
   new_ip->source[0] = rcv_ip->destination[0];
   new_ip->source[1] = rcv_ip->destination[1];
   ((struct tcp *) &new_tcb->snd_recv_r0_data[XIO_EIT_TCP])->src_port = rcv_tcp->dst_port;

   /* Set the precomputed IP checksums */
   new_ip->ttl = 0;
   new_ip->totallength = 0;
   new_ip->checksum = 0;
   new_tcb->pseudoIP_cksum = inet_checksum ((ushort *) &(new_ip->ttl), IP_PSEUDO_HDR_SIZE, 0, 0);
   new_ip->ttl = TCP_TTL;
   new_tcb->ip_cksum = inet_checksum ((ushort *) new_ip, sizeof(struct ip), 0, 0);

   /* new scheme: demultiplex in user space */
   new_tcb->ipsrc = *((unsigned int *) rcv_ip->source);
   new_tcb->ipdst = *((unsigned int *) rcv_ip->destination);
   new_tcb->tcpboth = rcv_tcp->both_ports;

   new_tcb->rcv_irs = rcv_tcp->seqno;
   new_tcb->rcv_next = rcv_tcp->seqno + 1;

   /* Send an syn + ack (the second step in the 3-way handshake). */
   /* Process options and generate response */
   xio_tcp_prepSynRespPacket (new_tcb, packet);

   timer_set (new_tcb, TCP_TIMEOUT(new_tcb));

   new_tcb->snd_next = new_tcb->snd_iss + 1;
   new_tcb->snd_max = new_tcb->snd_next;
   new_tcb->snd_una = new_tcb->snd_iss;
   new_tcb->state = TCB_ST_SYN_RECEIVED;

   return (1);
}

