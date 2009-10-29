
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


#ifndef __WEB_TCP_H__
#define __WEB_TCP_H__

#include <sys/types.h>

#define WEB_TCP_DEMULTIPLEXING_FIELDS \
    unsigned int ipsrc; 	/* source IP address */ \
    unsigned short tcpsrc; 	/* source TCP port number */ \
    struct tcb *hash_next; 	/* TCB hash pointer */ \
    struct tcb *hash_prev;	/* TCB hash pointer */

/* TCP control block, see RFC 793 */
struct tcb {

    WEB_TCP_DEMULTIPLEXING_FIELDS

    uint16      state;          /* State of this connection (free, etc.) */
    uint16      flags;          /* Control flags (timeout etc.) */

    /* Send sequence variables */
    uint32      snd_una;        /* Send unacknowledged */
    uint32      snd_next;       /* Send next */
    uint32      snd_wnd;        /* Send window */
    uint32      snd_up;         /* Send urgent point */
    uint32      snd_wls;        /* Segment sequence number used for last window
                                 * update */
    uint32      snd_wla;        /* Segment acknowledgement number used for last
                                 * window update. */
    uint32      snd_iss;        /* Initial sequence number */

    /* Receive sequence variables */
    uint32      rcv_next;       /* Receive next */
    uint32      rcv_wnd;        /* Receive window */
    uint32      rcv_irs;        /* Initial receive sequence number */

    /* MSS for this connection */
    uint16      mss;

    /* Number of attempts without any response from the other side. */
    int         retries;

    /* Timers */
    uint32      timer_retrans;  /* Retransmission timer (ticks) */

    /* Identification of ethernet card used for this TCP connection */
    int		netcardno;

    /* Implementation specific structures for sending and receiving packets */
    /* The recv's allow for scatter/gather.  We use gather writes of two    */
    /* entries and no scatter reads.                                        */
    int		snd_recv_n;		/* number of entries (2) */
    int		snd_recv_r0_sz;		/* size of first entry in bytes */
    char *	snd_recv_r0_data;	/* pointer to first entry's data */
    int		snd_recv_r1_sz;		/* size of second entry in bytes */
    char *	snd_recv_r1_data;	/* pointer to second entry's data */
    int		snd_recv_r2_sz;		/* size of third entry in bytes */
    char *	snd_recv_r2_data;	/* pointer to third entry's data */

    char * nextdata;
    void * pollbuf;

    /* Implementation specific send stuff */
    char        *usr_snd_buf;   /* pointer to application's write buf */
    char        *usr_snd_buf2;  /* pointer to application's 2nd write buf */

    /* Implementation specific receive stuff */
    char        *usr_rcv_buf;   /* pointer to application's read buf */
    int         usr_rcv_off;    /* offset in usr_rcv_buf */
    int         usr_rcv_sz;     /* size of usr_rcv_buf */
};


/* prototypes */

void web_tcp_inittcb (struct tcb *tcb, char *buf, int len);
void web_tcp_resettcb (struct tcb *tcb);
void web_tcp_getmainport (int portno, struct tcb * (*gettcb)());
int web_tcp_pollforevent (struct tcb *tcb);
int web_tcp_readable (struct tcb *tcb);
int web_tcp_getdata (struct tcb *tcb, char **bufptr);
int web_tcp_writable (struct tcb *tcb);
int web_tcp_senddata (struct tcb *tcb, char *buf, int len, char *buf2, int len2, int close, int *more, int *checksums);
int web_tcp_initiate_close (struct tcb *tcb);
int web_tcp_closed (struct tcb *tcb);
void web_tcp_timeadvanced (int seconds);
void web_tcp_shutdown (void *crap);

#ifdef CLIENT_TCP
#include <netinet/hosttable.h>
int web_tcp_initiate_connect (struct tcb *new_tcb, int srcportno, map_t *serverinfo, map_t *clientinfo);
#endif

#endif  /* __WEB_TCP_H__ */

