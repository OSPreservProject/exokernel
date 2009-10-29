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


#ifndef __XIO_TCP_H__
#define __XIO_TCP_H__

#ifndef EXOPC
#define uint8	u_int8_t
#define uint16	u_int16_t
#define uint32	u_int32_t
#define StaticAssert	assert
#define kprintf		printf
#define geteid		getpid
#ifndef max
#define max(a,b)        (((a) < (b)) ? (b) : (a))
#endif
#else
#include <exos/osdecl.h>

#endif

#include <sys/types.h>
#include <netinet/in.h>
#include <exos/netinet/fast_ip.h>
#include <exos/netinet/fast_eth.h>
#include <exos/net/ether.h>
#include <xok/ae_recv.h>
#include <xok/queue.h>

#define TCP_RETRY       4



#define XIO_TCP_DEMULTIPLEXING_FIELDS \
    uint32 ipsrc; 		/* source IP address */ \
    uint32 ipdst;		/* destination IP address */ \
    union { \
       struct { \
          uint16 ts; 		/* source TCP port number */ \
          uint16 td; 		/* destination TCP port number */ \
       } tps; \
       uint32 tb;		/* source and destination ports */ \
    } tcpports; \
    LIST_ENTRY(tcb) hash_link; \
    LIST_ENTRY(tcb) listen_link; \
    uint8 hashed;

#define tcpboth tcpports.tb
#define tcpsrc  tcpports.tps.ts
#define tcpdst  tcpports.tps.td

/* TCP control block, see RFC 793 */
struct tcb {

   XIO_TCP_DEMULTIPLEXING_FIELDS

   uint16      state;          /* State of this connection (free, etc.) */
   uint16      flags;          /* Control flags (timeout etc.) */

   /* Send sequence variables */
   uint32      snd_una;        /* Send unacknowledged */
   uint32      snd_next;       /* Send next */
   uint32      snd_max;        /* Maximum sequence number sent */
   uint32      snd_fin;        /* Sequence number of fin (if sent) */
   uint32      snd_wnd;        /* Send window */
   uint32      snd_up;         /* Send urgent point */
   uint32      snd_wls;        /* Sequence number used for last window update */
   uint32      snd_wla;        /* Ack number used for last window update. */
   uint32      snd_iss;        /* Initial sequence number */

   /* Receive sequence variables */
   uint32      rcv_next;       /* Receive next */
   uint32      rcv_wnd;        /* Receive window */
   uint32      rcv_irs;        /* Initial receive sequence number */

   /* MSS for this connection */
   uint16      mss;

   /* precomputed checksums (partial) over IP information */
   int         ip_cksum;
   int         pseudoIP_cksum;

   /* Number of attempts without any response from the other side. */
   int         retries;

   /* Timers */
   u_quad_t      timer_retrans;  /* Retransmission timer (in micro-seconds)
				    (absolute time in the future) */

   /* Identification of ethernet card used for this TCP connection */
   int  	netcardno;

   /* Implementation specific structures for sending and receiving packets */
   /* The recv's allow for scatter/gather.  We use gather writes of two    */
   /* entries and no scatter reads.                                        */
   int  	snd_recv_n;		/* number of entries (2) */
   int  	snd_recv_r0_sz;		/* size of first entry in bytes */
   u_char *	snd_recv_r0_data;	/* pointer to first entry's data */
   int  	snd_recv_r1_sz;		/* size of second entry in bytes */
   u_char *	snd_recv_r1_data;	/* pointer to second entry's data */
   int  	snd_recv_r2_sz;		/* size of third entry in bytes */
   u_char *	snd_recv_r2_data;	/* pointer to third entry's data */

  /* fields used for rtt estimation -- note all times are in tcp 4usec
     clock ticks. */
#define TCP_RATE_HZ 250000
#define USECS_PER_TCP_TICK 4

  u_quad_t send_time;	/* time in tcp ticks of last packet sent */
#define RTT_HISTORY_SZ 8	/* how many rtt's we average at once */
  unsigned int rtt_history[RTT_HISTORY_SZ]; /* history of recent RTT times */
  short rtt_next_ndx;	        /* index into above pointing to oldest rtt */
#define START_RTT (TCP_RATE_HZ*1)
  unsigned int rtt;

   /* Implementation specific send stuff */

   uint32	send_offset;	/* current data byte offset (next to send) */

   int send_ready;	/* indicates whether a packet has been prepared for xfer */
   int send_suggest;	/* indicates whether sending it is suggested */
   int send_important;	/* indicates that packet is important enough to resend */

   char *indata;	/* pointer to data found in packet */
   uint  indatalen;	/* amount of data found in packet */
   uint  inoffset;	/* offset (in connection seqnos) of data found in packet */

   uint inpackets;
   uint outpackets;

	/* GROK -- move these out of the PCB into separate socket structure */
struct tcpbuffer;
   struct tcpbuffer *inbuffers;
   struct tcpbuffer *outbuffers;

   char snd_buf[64];
};

/* Default maximum segment life time. */
#define TCP_MSL         10      /* 30 secs */

/* The default TTL value for IP ttl field. */
#define TCP_TTL         64                                             

/* From tcp/global.h */
#define IP_PSEUDO_HDR_SIZE      12


/* TCP control block, see RFC 793 */
struct listentcb {

   XIO_TCP_DEMULTIPLEXING_FIELDS

   uint16      state;          /* State of this connection (free, etc.) */
   uint16      flags;          /* Control flags (timeout etc.) */
};


/* packet components */

#define XIO_EIT_ETH     0
#define XIO_EIT_IP      (XIO_EIT_ETH + sizeof(struct eth))
#define XIO_EIT_TCP     (XIO_EIT_IP + sizeof(struct ip))
#define XIO_EIT_DATA    (XIO_EIT_TCP + sizeof(struct tcp))


/* Start send buffers on short-aligned address, so that the ip header after
 * the ethernet header is word-aligned.
 */                
#define ETHER_ALIGN     2


/* Default maximum segment size, which we advertise */
#define DEFAULT_MSS     1460    /* ethernet packet - headers */


/* TCP header, see RFC 793 */
struct tcp {
   union {
      struct {
         uint16      src;       /* Source port */
         uint16      dst;       /* Destination port */
      } prts;
      uint32 bth;
   } tprts;
#define both_ports tprts.bth
#define src_port   tprts.prts.src
#define dst_port   tprts.prts.dst
   uint32      seqno;          /* Sequence number */
   uint32      ackno;          /* Acknowledgement number */
   uint8       offset;         /* Offset; start of data (4 bits) */
   uint8       flags;          /* Flags (6 bits, staring at bit 2) */
   uint16      window;         /* The number of bytes wiling to accept */
   uint16      cksum;          /* Checksum over data and pseudoheader */
   uint16      urgentptr;      /* Pointer to urgent data */
};


#define xio_tcp_synpacket(packet) \
	(((packet)[XIO_EIT_TCP+13] & TCP_FL_SYN) && \
	 !((packet)[XIO_EIT_TCP+13] & (TCP_FL_ACK|TCP_FL_RST)))


/* Defines for tcp option kinds: */
#define TCP_OPTION_EOL  0
#define TCP_OPTION_NOP  1
#define TCP_OPTION_MSS  2

/* Defines for tcp option lengths: */
#define TCP_OPTION_MSS_LEN      4

/* The default maximum segment size (excl. ip and tcp header). */
#define TCP_MSS         536

/* TCP control flags: */
#define TCP_FL_URG      0x20    /* Urgent pointer */
#define TCP_FL_ACK      0x10    /* Acknowledgement */
#define TCP_FL_PSH      0x8     /* Push */
#define TCP_FL_RST      0x4     /* Reset */
#define TCP_FL_SYN      0x2     /* Synchronize */
#define TCP_FL_FIN      0x1     /* No more data */


/* Macros to compare sequence numbers. Assumption is that sequence numbers *
 * a and b differ by no more than one-half of the largest sequence number. */
#define SEQ_LT(a,b)     ((int)((a)-(b)) < 0)
#define SEQ_LEQ(a,b)    ((int)((a)-(b)) <= 0)
#define SEQ_GT(a,b)     ((int)((a)-(b)) > 0)
#define SEQ_GEQ(a,b)    ((int)((a)-(b)) >= 0)
#define SEQ_MAX(a,b)    (((int)((a)-(b)) >= 0) ? (a) : (b))


/* Connection states */
#define TCB_ST_FREE             0
#define TCB_ST_LISTEN           1
#define TCB_ST_SYN_SENT         2
#define TCB_ST_SYN_RECEIVED     3
#define TCB_ST_ESTABLISHED      4
#define TCB_ST_FIN_WAIT1        5
#define TCB_ST_FIN_WAIT2        6
#define TCB_ST_CLOSE_WAIT       7
#define TCB_ST_CLOSING          8
#define TCB_ST_LAST_ACK         9
#define TCB_ST_TIME_WAIT        10
#define TCB_ST_CLOSED           11
#define NSTATES                 12


#define xio_tcp_readable(tcb) \
	(((tcb)->state == TCB_ST_ESTABLISHED) || \
	 ((tcb)->state == TCB_ST_FIN_WAIT1) || \
	 ((tcb)->state == TCB_ST_FIN_WAIT2))

#define xio_tcp_writeable(tcb) \
	(((tcb)->state == TCB_ST_ESTABLISHED) || \
	 ((tcb)->state == TCB_ST_CLOSE_WAIT))

	/* exploit knowledge of state values. */
#define xio_tcp_connecting(tcb) \
	((tcb)->state < TCB_ST_ESTABLISHED)

#define xio_tcp_finrecv(tcb) \
	((tcb)->state >= TCB_ST_CLOSE_WAIT)

#define xio_tcp_finsent(tcb) \
	(((tcb)->state >= TCB_ST_FIN_WAIT1) && \
	 ((tcb)->state != TCB_ST_CLOSE_WAIT))

#define xio_tcp_finacked(tcb) \
	(((tcb)->snd_fin != 0) && ((tcb)->snd_una == (tcb)->snd_fin))

#define xio_tcp_closed(tcb)	((tcb)->state == TCB_ST_CLOSED)

#define xio_tcp_rewriting(tcb) \
	(((tcb)->snd_next < (tcb)->snd_max) && \
	 ((tcb)->state != TCB_ST_FIN_WAIT2) && \
	 ((tcb)->state != TCB_ST_TIME_WAIT) && \
	 ((tcb)->state != TCB_ST_CLOSED))

/* GROK -- I'm a bit concerned about the frequency/cost of this comp. as is */
#define xio_tcp_timedout(tcb) \
	(((tcb)->timer_retrans != 0) && \
	 (__sysinfo.si_system_ticks >= __usecs2ticks ((tcb)->timer_retrans)))

/* reusable macros for printing TCP info */

#define kprint_ether_addrs(pack) \
   kprintf ("packet sent to %02x:%02x:%02x:%02x:%02x:%02x from %02x:%02x:%02x:%02x:%02x:%02x\n", \
	(unsigned char)(pack)[0], (unsigned char)(pack)[1], \
	(unsigned char)(pack)[2], (unsigned char)(pack)[3], \
	(unsigned char)(pack)[4], (unsigned char)(pack)[5], \
	(unsigned char)(pack)[6], (unsigned char)(pack)[7], \
	(unsigned char)(pack)[8], (unsigned char)(pack)[9], \
	(unsigned char)(pack)[10], (unsigned char)(pack)[11]);

#define kprint_ip_addrs(pack) \
   kprintf ("packet sent to %02x.%02x.%02x.%02x from %02x.%02x.%02x.%02x\n", \
	(unsigned char)(pack)[31], (unsigned char)(pack)[32], \
	(unsigned char)(pack)[33], (unsigned char)(pack)[34], \
	(unsigned char)(pack)[27], (unsigned char)(pack)[28], \
	(unsigned char)(pack)[29], (unsigned char)(pack)[30]);

#define kprint_tcp_addrs(pack) \
   kprintf ("packet sent to TCP port %d from TCP port %d\n", \
	(unsigned short)(pack)[36], (unsigned short)(pack)[34]);

#define xio_tcp_printtcb(tcb)	\
   printf("tcb %p (%d): una %u snext %u swnd %u wl1 %u wl2 %u rnext %u rwnd %u \n", \
	(tcb), (tcb)->state, (tcb)->snd_una, (tcb)->snd_next, (tcb)->snd_wnd, \
	(tcb)->snd_wls, (tcb)->snd_wla, (tcb)->rcv_next, (tcb)->rcv_wnd);

#define tcp_print(tcp) { \
   printf("tcp: src_port %u dst_port %u\n", (tcp)->src_port, (tcp)->dst_port); \
   printf("seqno %u\n", (tcp)->seqno); \
   printf("ackno %u\n", (tcp)->ackno); \
   printf("offset 0x%x flags 0x%x\n", (tcp)->offset, (tcp)->flags); \
   printf("window %u cksum 0x%x\n", (tcp)->window, (tcp)->cksum); \
   printf("\n"); \
   }

#define tcp_print_seq(tcp) \
   printf("tcp: seqno %u ackno %u window %u flags 0x%x\n", \
	tcp->seqno, tcp->ackno, tcp->window, tcp->flags); \

/* helper macros */

/* how much data has been acked (remember to adjust for SYN) */
static inline uint xio_tcp_acked_offset (struct tcb *tcb) {
   return ((tcb)->snd_una - (tcb)->snd_iss - 1 - xio_tcp_finacked(tcb));
}

/* how much data has been received (remember to adjust for SYN) */
static inline uint xio_tcp_received_offset (struct tcb *tcb) {
   return ((tcb)->rcv_next - (tcb)->rcv_irs - 1 - xio_tcp_finrecv(tcb));
}

static inline uint xio_tcp_getLiveInDataLen (struct tcb *tcb) {
   return (((tcb)->indata == NULL) ? 0 : \
				 max(0,((tcb)->indatalen - (tcb)->inoffset)));
}

static inline char * xio_tcp_getLiveInData (struct tcb *tcb) {
   return ((!xio_tcp_getLiveInDataLen(tcb)) ? 0 : \
				 ((tcb)->indata + (tcb)->inoffset));
}

static inline uint xio_tcp_getLiveInDataStart (struct tcb *tcb) {
   return ((!xio_tcp_getLiveInDataLen(tcb)) ? 0 : \
	 (xio_tcp_received_offset(tcb) - (tcb)->indatalen + (tcb)->inoffset));
}

unsigned int xio_tcp_read_clock ();

static inline int xio_tcp_rcvwnd (struct tcb *tcb) {
   return (tcb->rcv_wnd);
}

static inline void xio_tcp_setrcvwnd (struct tcb *tcb, int window) {
   tcb->rcv_wnd = window;
}

static inline int xio_tcp_windowsz (struct tcb *tcb) {
  return (tcb->snd_una + tcb->snd_wnd - tcb->snd_next);
}

/* the amount of data that has been sent but not acknowledged yet */
static inline int xio_tcp_unacked (struct tcb *tcb) {
  return (tcb->snd_max - tcb->snd_una);
}

static inline int xio_tcp_maxsendsz (struct tcb *tcb) {
  return (min (xio_tcp_windowsz (tcb), tcb->mss));
}

/* computes the amount of stuff we are currently planning to retransmit */
static inline int xio_tcp_retransz (struct tcb *tcb) {
  return (tcb->snd_max - tcb->snd_next);
}

#endif  /* __XIO_TCP_H__ */

