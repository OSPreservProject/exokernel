
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

#ifndef _TCP_H_

#define _TCP_H_

#include "types.h"
#include "ip.h"
#include "eth.h"
#include "exos/net/pkt.h"


/* TCP header, see RFC 793 */
struct tcp {
    uint16 	src_port;	/* Source port */
    uint16 	dst_port;	/* Destination port */
    uint32 	seqno;		/* Sequence number */
    uint32 	ackno;		/* Acknowledgement number */
    uint8  	offset;		/* Offset; start of data (4 bits) */
    uint8  	flags;		/* Flags (6 bits, staring at bit 2) */
    uint16 	window;		/* The number of bytes wiling to accept */
    uint16 	cksum;		/* Checksum over data and pseudoheader */
    uint16 	urgentptr;	/* Pointer to urgent data */
};

/* Defines for tcp option kinds: */
#define TCP_OPTION_EOL	0
#define TCP_OPTION_NOP	1
#define TCP_OPTION_MSS	2
#define TCP_OPTION_TS	8

/* Defines for tcp option lengths: */
#define TCP_OPTION_MSS_LEN	4

/* The default maximum segment size (excl. ip and tcp header). */
#define TCP_MSS		536

/* The default TTL value for IP ttl field. */
#define TCP_TTL		64

/* Default maximum segment life time. */
#define TCP_MSL		30000000 	/* 30 secs */

#define TCP_SSTHRESH	65353	/* default slow start threshold size */

/* TCP control flags: */
#define TCP_FL_URG	0x20	/* Urgent pointer */
#define TCP_FL_ACK	0x10	/* Acknowledgement */
#define TCP_FL_PSH	0x8	/* Push */
#define TCP_FL_RST	0x4	/* Reset */
#define TCP_FL_SYN 	0x2	/* Synchronize */
#define TCP_FL_FIN	0x1	/* No more data */


/* The fast ETH IP TCP header --- horizontal encoding */
struct eit {
#ifndef AN2
    struct eth 	*eth; 	/* Ethernet header */
#endif
    struct ip 	*ip; 	/* IP header */
    struct tcp 	*tcp;  	/* TCP header */
#ifdef AN2
    int vc;             /* vc */
    struct ae_recv *a;	/* User data */
#else
    char        *data;        /* User data */
#endif
    char 	*msg;	/* char * to first header */
};

#ifdef AN2
#define MAX_SNDBUF	8

struct padbuf_t {
    char pad[50];
    short len;      /* bytes 43 and 44 of final packet */
    char padend[4];    /* remainder. */
};

struct an2_recv {
    struct ae_recv 	r;
    int			notify;
    void           *ip;         /* ip header */
    void           *tcp;        /* tcp header */
    void           *options;    /* if needed */
    void           *padbuf;     /* an2 buffer of all zeroes for padding */
    int                  vc;    /* the vc this message is supposed to be sent to */
    struct ae_recv 	rbackup;
};

struct sndbuft {
    /* one per tcb */
    struct an2_recv *sndbuf_current_p;
    struct an2_recv snd_buf[MAX_SNDBUF];
};
#endif /* AN2 */

/* Default minimum segment size, which we advertise */
#ifdef AN2
#define MAXOPTIONLEN    16
#define DEFAULT_MSS	3*OTTO_RECVBUFSIZ-sizeof(struct ip)-sizeof(struct tcp)-MAXOPTIONLEN
#define DEFAULT_MSG	(sizeof(struct ip) + sizeof(struct tcp) + DEFAULT_MSS)
#define AN2_PAD		48

#else

#define DEFAULT_MSS	1460	/* ethernet packet - headers */
/* Default size of receive buffer */
#define DEFAULT_MSG	(sizeof(struct eth) + sizeof(struct ip) + sizeof(struct tcp) + DEFAULT_MSS)

#endif

#ifndef AN2

/* Start receive buffer on short-aligned address, so that the ip header after
 * the ethernet header is word-aligned. Note necessary for Intel.
 */
#ifdef AEGIS
#define START_ALIGN	4
#define ETHER_ALIGN	2
#endif
#ifdef EXOPC
#define START_ALIGN	0
#define ETHER_ALIGN	0
#endif

#endif


/* TCP control block, see RFC 793 */
struct tcb {
    uint16	state;		/* State of this connection (free, etc.) */
    uint16	flags;		/* Control flags (timeout etc.) */

    /* Send sequence variables */
    uint32	snd_una;	/* Send unacknowledged */
    uint32	snd_next;	/* Send next */
    uint32      snd_max;	/* Maximum sequence number sent */
    uint32	snd_wnd;	/* Send window */
    uint32	snd_cwnd;	/* Congestion window */
    uint32	snd_ssthresh;	/* Slow start threshold size */
    uint32	snd_up;		/* Send urgent point */
    uint32      snd_wls;        /* Segment sequence number used for last window
                                 * update */
    uint32      snd_wla;        /* Segment acknowledgement number used for last
				 * window update. */
    uint32	snd_iss;	/* Initial sequence number */

    /* Receive sequence variables */
    uint32	rcv_next;	/* Receive next */
    uint32	rcv_wnd;	/* Receive window */
    uint32	rcv_irs;	/* Initial receive sequence number */

    /* MSS for this connection */
    uint16	mss;

    /* Number of attempts without any response from the other side. */
    int		retries;

    /* Timers */
    uint32	timer_retrans;	/* Retransmission timer (ticks) */
    int		rate;		/* Clock rate */

    /* Implementation specific status */
    struct eit  snd_eit;
#ifndef AN2
    struct ae_recv snd_recv;	/* recv to build pkt */
#endif
    struct ae_eth_pkt_queue rcv_queue;
    struct eit	rcv_eit;

    /* Implementation specific send stuff */
    char 	*usr_snd_buf;	/* pointer to application's write buf */
    int		usr_snd_off;	/* offset in usr_snd_buf */
    int		usr_snd_sz;	/* # bytes in usr_snd_buf */
    int		usr_snd_done;   /* the TCP seqno for last byte in usr_snd_buf */
    void	(*usr_snd_release)(); /* release func for nonblocking comm */
#ifdef AN2
    struct sndbuft *send_buffers; /* pool of filled-in send buffers  */
#endif

    /* Implementation specific receive stuff */
    char	*usr_rcv_buf;	/* pointer to application's read buf */
    int		usr_rcv_off;	/* offset in usr_rcv_buf */
    int		usr_rcv_sz;	/* size of usr_rcv_buf */

#ifndef AN2
    int		demux_id;	/* Filter id that demuxs this connection */
#endif

    int		netcardno;	/* the network card ID */

    /* Buffer list */
    void	*buf_list;	/* List of buffers with data */
    void	*buf_list_tail;	/* Tail of the list of buffers with data */

    /* Drop rate */
    int		droprate;	/* Testing */

    /* Queue of tcbs that are ready to be accepted */
    int		nlisten;
    int		queue_len;
    struct tcb	*next;
    struct tcb  *last;

    /* Addresses: */
    uint8		eth_dst[6];
    uint8		eth_src[6];
    uint32              ip_src;
    uint32              ip_dst;
    uint16              port_src;
    uint16              port_dst;

};


/* Some internal constants */
#define TCP_TIMEOUT	10000000 /* microseconds */
#define TCP_RETRY	10

/* 
 * Macros to compare sequence numbers. Assumption is that sequence numbers
 * a and b differ by no more than one-half of the largest sequence number.
 */
#define	SEQ_LT(a,b)	((int)((a)-(b)) < 0)
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	SEQ_GT(a,b)	((int)((a)-(b)) > 0)
#define	SEQ_GEQ(a,b)	((int)((a)-(b)) >= 0)


/* Connection states */
#define TCB_ST_FREE 		0 
#define TCB_ST_LISTEN 		1
#define TCB_ST_SYN_SENT 	2
#define TCB_ST_SYN_RECEIVED	3
#define TCB_ST_ESTABLISHED	4
#define TCB_ST_FIN_WAIT1 	5
#define TCB_ST_FIN_WAIT2 	6
#define TCB_ST_CLOSE_WAIT 	7
#define TCB_ST_CLOSING 		8
#define TCB_ST_LAST_ACK 	9
#define TCB_ST_TIME_WAIT 	10
#define TCB_ST_CLOSED		11

#define NSTATES			12

/* TCB control flags: */
#define TCB_FL_TIMEOUT		0x1
#define TCB_FL_DELACK		0x2
#define TCB_FL_ASYNCLOSE        0x4
#define TCB_FL_ASYNWRITE        0x8

#ifndef NOSTATISTICS

#define	STINC(field)		tcp_stat.field++
#define	STDEC(field)		tcp_stat.field--
#define STINC_SIZE(field, n)	tcp_stat.field[n]++
#define NLOG2			13 	/* round of 2 log MSS */

extern struct statistics {
    int		nacksnd;	/* # normal acks */
    int		nack;		/* # acks received */
    int 	nvack;		/* # volunteerly acks */
    int 	ndelayack;	/* # delayed acks */
    int		nackprd;	/* # predicted acks */
    int		nackdup;	/* # duplicate acks */
    int		ndataprd;	/* # predicted data */
    int		nzerowndsnt;	/* # acks advertising zero windows */
    int		nretrans;	/* # retransmissions */
    int		nethretrans;	/* # ether retransmissions */
    int		nfinsnd;	/* # fin segments */
    int		nfinrcv;	/* # fin good segments received */
    int		ndatarcv;	/* # segments with data */
    int		norder;		/* # segments in order */
    int		nbuf;		/* # segments buffered */
    int		noutspace;	/* # segments with data but no usr buffer */
    int		noutorder;	/* # segments out of order */
    int		nprobercv;	/* # window progbes */
    int		nprobesnd;	/* # window probes send */
    int		nseg;		/* # segments received */
    int		nincomplete;	/* # incomplete segments */
    int 	nsynsnd;	/* # syn segments sent */
    int         nsynrcv;	/* # syn segments received */
    int		nopen;		/* # tcp_open calls */
    int		nlisten;	/* # tcp_listen calls */
    int		naccepts;	/* # tcp_accept calls */
    int		nread;		/* # tcp_read calls */
    int		nwrite;		/* # tcp_write calls */
    int		ndata;		/* # segments sent with data */
    int		nclose;		/* # tcp_close calls */
    int		ndropsnd;	/* # segments dropped when sending */
    int		ndroprcv;	/* # segments dropped when receiving */
    int         nrstsnd;	/* # reset segments send */
    int         nrstrcv;	/* # reset segments received */
    int		naccept;	/* # acceptable segments */
    int		nunaccept;	/* # unacceptable segments */
    int		nbadipcksum;	/* # segments with bad ip checksum */
    int		nbadtcpcksum;	/* # segments with bad tcp checksum */
    int		rcvdata[NLOG2];
    int		snddata[NLOG2];
} tcp_stat;

#else

#define	STINC(field)		
#define	STDEC(field)		
#define STINC_SIZE(field, n)	
#endif

#endif _TCP_H_
