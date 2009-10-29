
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
 * Hard-wired simple (probably broken) implementation of TCP/IP protocol.
 *
 * Written by Frans Kaashoek (10/13/95)
 *
 *
 * Structure of this file:
 *	1. Bundle of help functions
 *	2. Simple buffering functions
 *	3. Initialization of tcb
 *	4. Processing routines for ack, rst, syn, and data.
 *	5. Send routines
 *	6. Functions, corresponding to each of the states of the tcp protocol.
 *      7. Timeout functions
 *	8. The functions that constitute the interface to this module.
 *
 * The implementation of the protocol follows closely RFC793. The implementation
 * attempts to be conservative in what it sends out; it implements
 *	- delayed acks
 *	- slow start
 *	- congestion avoidance
 *
 * The implementation uses a delay acknowledgement strategy until the next
 * data segment arrives, until this client sends data, or until a timer runs
 * off (the latter needs to be implemented). This strategy works fine for
 * any channel, except a low-bandwidth channel.
 * 
 * Slow start and congestion avoidance are implemented as described in Steven's
 * RFC draft. The only exception is that cwnd is increased on every ack instead
 * of one increase per round trip during congestion avoidance. When we implement
 * RTT, it will be changed. We have not studied the performance of this tcp
 * library across different kind of links, so it remains to be seen whether
 * this implementation is correct under widely-varying links.
 *
 */

/*
 * TODO: 
 * - reduce number of copies
 * - detect connection breaks etc. (keep alive segments)
 * - improve buffering scheme
 * - time_wait state has to wait two mss
 *
 * - fix iss
 * - initial port needs to be globally unique (like iss)
 * - implement other timers besides retransmission timer
 * - implement estimate RTT
 * - data on a syn segment is deleted
 * - implement fast retransmit and fast recovery
 * - determine MSS dynamically.
 * - implement reassembly (not really needed)
 */


#ifdef EXOPC
#include <assert.h>
#include <string.h>
#include <xok/defs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/tick.h>
#include "types.h"
#include <sys/types.h>
#include <unistd.h>
#include <exos/critical.h>
#include <xok/sysinfo.h>
#include <exos/conf.h>

extern int getnetcardno (char *eth_addr);

#endif

/* For movable ash */
#include <xok/env.h>
extern struct Env * __curenv;
#define ADD_UVA(x) ((typeof(x))(((u_int)(x)) + ((u_int)__curenv->env_ashuva)))

#include "tcp.h"
#include <exos/debug.h>
#include "global.h"

#define HEADERPRD		/* Tcp header precdiction enabled */
/*#define NONBLOCKINGWRITE*/

#ifdef MEMCPY_DEBUG
#define kmemcpy(p1, p2, n) do {                                   \
			 extern (*putchr)(int c);                 \
			 int (*tmp_putchr)(int c);                \
			 tmp_putchr = putchr;                     \
			 putchr = ae_putc;                        \
			 printf("%s:%d cpy %p %p %d\n",           \
				__FILE__, __LINE__, p1, p2, n);   \
			 memcpy(p1, p2, n);                       \
			 putchr = tmp_putchr;                     \
			 } while(0)
#else
#define kmemcpy memcpy
#endif

static void tcp_done(struct tcb *tcb);

struct tcb connection[MAX_CONNECTION];
extern int __envid;
uint16 next_port;

static int st_illegal(struct tcb *tcb);
static int st_listen(struct tcb *tcb);
static int st_syn_sent(struct tcb *tcb);
static int st_syn_received(struct tcb *tcb);
static int st_established(struct tcb *tcb);
static int st_fin_wait1(struct tcb *tcb);
static int st_fin_wait2(struct tcb *tcb);
static int st_close_wait(struct tcb *tcb);
static int st_closing(struct tcb *tcb);
static int st_last_ack(struct tcb *tcb);
static int st_time_wait(struct tcb *tcb);

static int (*state[NSTATES])(struct tcb *tcb) = {
    st_illegal,
    st_listen,
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

/*----------------------------------------------------------------------------*/

#ifndef NOSTATISTICS
/*
 * Computes log2 of size.
 */
static int
log2(int size)
{
    int r = 0;
    
    size = size >> 1;

    while (size != 0) {
	size = size >> 1;
	r++;
    }

    demand(r < NLOG2, log2: we cannot deal with this result);

    return r;
}
#endif

/* 
 * Print tcp header.
 */
static void 
tcp_print(struct tcp *tcp)
{
    printf("tcp: src_port %u dst_port %u\n", tcp->src_port, tcp->dst_port);
    printf("seqno %u\n", tcp->seqno);
    printf("ackno %u\n", tcp->ackno);
    printf("offset 0x%x flags 0x%x\n", tcp->offset, tcp->flags);
    printf("window %u cksum 0x%x\n", tcp->window, tcp->cksum);
    printf("\n");
}


/*
 * Print salient part of the header.
 */
static void
tcp_print_seq(struct tcp *tcp)
{
    printf("tcp: seqno %u ackno %u window %u flags 0x%x\n",
	   tcp->seqno, tcp->ackno, tcp->window, tcp->flags);
}


/*
 * Print salient part of TCB.
 */
static void
tcb_print(struct tcb *tcb)
{
    printf("tcb %d(%d): una %u snxt %u smax %u swnd %u wl1 %u wl2 %u rnxt %u rwnd %u \n", 
	   tcb - connection,
	   tcb->state, 
	   tcb->snd_una, tcb->snd_next, tcb->snd_max, tcb->snd_wnd, 
	   tcb->snd_wls, tcb->snd_wla,
	   tcb->rcv_next, tcb->rcv_wnd); 
}

/*----------------------------------------------------------------------------*/

/* 
 * Extreme simple buffer management.
 * Needs improvement:
 * 1. Per tcb
 * 2. Tie into window management
 */

buffer_p		buf_free_list;
buffer_t		buffer[NBUF];

void
tcp_buf_init(void)
{
    buffer_p p;

    for (p = &buffer[0]; p < &buffer[NBUF - 1]; p++) {
	p->b_next = p + 1;
    }
    p->b_next = 0;
    buf_free_list = &buffer[0];
}


buffer_p
tcp_buf_get(void)
{
    buffer_p p;

    demand(buf_free_list != 0, "buf_get: out of buffers");

    p = buf_free_list;
    buf_free_list = p->b_next;
    p->b_next = 0;

    return p;
}


void
tcp_buf_free(buffer_p p)
{
    p->b_next = buf_free_list;
    buf_free_list = p;
}

/*----------------------------------------------------------------------------*/

int tcp_library_initialized = 0;

/* initialize n tcbs */
void tcp_init(void)
{
    struct tcb *tcb;
    static void tcp_exit(void);

    if(tcp_library_initialized) return;
    atexit(tcp_exit);
#if !defined(SEED_RANDOM_IN_EXECINIT) || !defined(EXOPC)
    srandom(ae_gettick());
#endif
    next_port = (getpid() << 9) | random();
    tcp_net_init();
    tcp_buf_init();
    tcb_iter_init();
    while((tcb = tcb_iter())) {
	tcb_net_tcb_init(tcb);
    }
    tcp_library_initialized = 1;
}


/*
 * Clean up connections (i.e., turn half-close in full close or complete
 * time-wait state).
 */
static void 
tcp_exit(void)
{
    struct tcb *tcb;
    int tcp_close(struct tcb *tcb);

    DPRINTF(2, ("tcp_exit: clean up\n"));

    tcb_iter_init();
    while((tcb = tcb_iter())) {
	while (tcb->state != TCB_ST_FREE) {
	    tcp_net_poll_all(0);
	    if (tcb->state == TCB_ST_CLOSE_WAIT || 
		tcb->state == TCB_ST_ESTABLISHED ||
		tcb->state == TCB_ST_LISTEN) {
		tcp_close(tcb);
	    }
	    if (tcb->state == TCB_ST_CLOSED) {
		tcb_release(tcb);
	    }
	}
    }

    DPRINTF(2, ("tcp_exit: connection is free \n"));
}

/*----------------------------------------------------------------------------*/

/*
 * Open an connection: allocate control block and init it.
 * Argument is unspecified, if 0 is passed in; if src_port is zero,
 * the library will select a source port.
 */
static struct tcb*
tcp_open(uint16 dst_port, uint32 ip_dst_addr, uint8 *eth_dst_addr,
	 uint16 src_port, uint32 ip_src_addr, uint8 *eth_src_addr,  
	 int new)
{
    struct tcb *tcb;
    int netcardno = -1;

    if ((eth_src_addr != NULL) && ((netcardno = getnetcardno(eth_src_addr)) == -1)) {
       printf ("tcp_open: unknown ethernet address: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", eth_src_addr[0], eth_src_addr[1], eth_src_addr[2], eth_src_addr[3], eth_src_addr[4], eth_src_addr[5]);
       assert (0);
    }

    STINC(nopen);

    if(!tcp_library_initialized) tcp_init();

    /* Allocate a control block */
    tcb = tcb_acquire();

    DPRINTF(2, ("tcp_open: allocated tcb %d\n", tcb - connection));

    tcb->next = 0;

    tcb->usr_snd_buf = 0;

    /* TCP control block initialization */
    tcb->snd_iss = 483;				/* HACK: random number */
/*#define RANDOM_ISS*/
#ifdef RANDOM_ISS
    {
	int r = random();
	tcb->snd_iss = (r&0xffff)*10000;
    }
#endif
    tcb->netcardno = netcardno;
    tcb->snd_next = tcb->snd_iss;
    tcb->snd_max = tcb->snd_iss;
    tcb->snd_wla = tcb->snd_iss;
    tcb->snd_up = tcb->snd_iss;
    tcb->rcv_wnd = RCVWND_DEFAULT;
    tcb->usr_rcv_buf = 0;	/* application is not ready to accept data */
    tcb->usr_rcv_off = 0;
    tcb->droprate = 0;
    tcb->retries = 0;
    tcb->timer_retrans = 0;
    tcb->rate = ae_getrate();	/* clock rate */
    tcb->mss = TCP_MSS;		/* set default MSS */
    tcb->snd_cwnd = tcb->mss;	/* set up congestion window to 1 mss */
    tcb->snd_ssthresh = TCP_SSTHRESH; /* set up slow start threshold size */
    
    tcb->flags = 0;

    /* Store addresses also in tcb.  Needed for tcp_handoff. */
    if (eth_dst_addr)
	memcpy(tcb->eth_dst, eth_dst_addr, sizeof tcb->eth_dst);
    if (eth_src_addr)
	memcpy(tcb->eth_src, eth_src_addr, sizeof tcb->eth_src);


    tcb->ip_dst = ip_dst_addr;
    tcb->ip_src = ip_src_addr;

    /* Allocate a source port. */
    if (src_port == 0) tcb->port_src = next_port++;
    else tcb->port_src = src_port;

    if (dst_port != 0) tcb->port_dst = dst_port;

    tcp_net_tcb_init(tcb);

    tcb->buf_list = 0;
    tcb->buf_list_tail = 0;

    return(tcb);
}

/*----------------------------------------------------------------------------*/

/*
 * First check of RFC: check is the segment is acceptable.
 */
static int
process_acceptable(struct tcb *tcb)
{
    struct tcp *tcp;
    struct ip *ip;
    int len;
    int hlen;

    ip = tcb->rcv_eit.ip;
    tcp = tcb->rcv_eit.tcp;
    hlen = ((tcp->offset >> 4) & 0xF) << 2;
    len = ip->totallength - sizeof(struct ip) - hlen;

    if ((tcp->flags & TCP_FL_SYN) || (tcp->flags & TCP_FL_FIN)) len++;

    /* Acceptable? */
    if (
	((len == 0) && (tcb->rcv_wnd == 0) && (tcp->seqno == tcb->rcv_next)) ||

	((len == 0) && (tcb->rcv_wnd > 0) && 
	 (SEQ_GEQ(tcp->seqno, tcb->rcv_next) && 
	  SEQ_LEQ(tcp->seqno, tcb->rcv_next + tcb->rcv_wnd))) ||

	((len > 0) && (tcb->rcv_wnd > 0) &&
	 ((SEQ_GEQ(tcp->seqno, tcb->rcv_next) && 
	   SEQ_LT(tcp->seqno, tcb->rcv_next + tcb->rcv_wnd)) ||
	  (SEQ_GEQ(tcp->seqno + len - 1, tcb->rcv_next) &&
	   SEQ_LT(tcp->seqno + len - 1, tcb->rcv_next + tcb->rcv_wnd))))) {

	STINC(naccept);

	/* Yes, acceptable */
	return 1;
    } else {
	/* No, not acceptable: if not rst, send ack. */

	STINC(nunaccept);

	printf("process_acceptable %d: seq %u not accepted; expect %u wnd %d\n",
	       len, tcp->seqno, tcb->rcv_next, tcb->rcv_wnd);

	if (!(tcp->flags & TCP_FL_RST)) {
	    STINC(nacksnd);
	    if (tcp->flags & TCP_FL_SYN)
		tcp_net_send(tcb, TCP_FL_ACK|TCP_FL_SYN, tcb->snd_iss, 0);
	    else
		tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
	}
	return 0;
    }
}


/*
 * Check 4 and first part of 5 of RFC.
 */
static int
process_syn_ack(struct tcb *tcb)
{
    struct tcp *tcp;

    tcp = tcb->rcv_eit.tcp;

    if (tcp->flags & TCP_FL_SYN) {
	STINC(nsynrcv);
	STINC(nrstsnd);
        if (tcp->flags & TCP_FL_ACK) 
	    tcp_net_send(tcb, TCP_FL_RST|TCP_FL_ACK, tcp->ackno, 0);
	else
	    tcp_net_send(tcb, TCP_FL_RST, 0, 0);
        tcp_done(tcb);
	return 0;
    }

    if (!(tcp->flags & TCP_FL_ACK)) return 0;

    return 1;
}


/*
 * Check 2 of RFC and call function for checks 4 and first part 5. Check 3
 * is for security stuff, which we do not implement.
 */
static int
process_rst_syn_ack(struct tcb *tcb)
{
    struct tcp *tcp;

    tcp = tcb->rcv_eit.tcp;

    if (tcp->flags & TCP_FL_RST) {
	STINC(nrstrcv);
        tcp_done(tcb);
	return 0;
    }

    return process_syn_ack(tcb);
}


/* 
 * The last part of check 5 of RFC.
 */
static void 
process_ack(struct tcb *tcb)
{
    int dup;
    struct tcp *tcp;
    int n;

    tcp = tcb->rcv_eit.tcp;

    STINC(nack);

    dup = 1;
    /* If an acknowledgement and it is interesting, update una. */
    if ((tcp->flags & TCP_FL_ACK) && SEQ_GEQ(tcp->ackno, tcb->snd_una)
	&& SEQ_LEQ(tcp->ackno, tcb->snd_max)) { 
	DPRINTF(2, ("process_ack %d ack %d: una %u max %u next %u\n", 
		    tcb - connection, tcp->ackno, tcb->snd_una, 
		    tcb->snd_max, tcb->snd_next));
	tcb->snd_una = tcp->ackno;
	if (SEQ_GT(tcb->snd_una, tcb->snd_next)) {
	    assert(SEQ_LT(tcb->snd_next, tcb->snd_max));
	    n = tcb->snd_una - tcb->snd_next;
	    tcb->usr_snd_off += n;
	    tcb->snd_next = tcb->snd_una;
	}
	assert(SEQ_LEQ(tcb->snd_una, tcb->snd_next));
	assert(SEQ_LEQ(tcb->snd_next, tcb->snd_max));

	/* Reschedule time out event; we have received an interesting ack. */
	timer_set(tcb, TCP_TIMEOUT);
	dup = 0;

	/* If this completes a non-blocking write, call the user-supplied
	 * release function.
	 */
	if ((tcb->flags & TCB_FL_ASYNWRITE) &&
	    SEQ_GEQ(tcb->snd_una, tcb->usr_snd_done)) { 
	    tcb->flags &= ~TCB_FL_ASYNWRITE;
	    if (tcb->usr_snd_release) (*(tcb->usr_snd_release))();
        }
    }
    
    /* If an acknowledgement and it is interesting, update snd_wnd. */
    if ((tcp->flags & TCP_FL_ACK) && 
	(SEQ_LT(tcb->snd_wls, tcp->seqno) ||
	 (tcb->snd_wls == tcp->seqno && SEQ_LT(tcb->snd_wla, tcp->ackno)) ||
	 (tcb->snd_wla == tcp->ackno && tcp->window > tcb->snd_wnd))) {
	DPRINTF(2, ("process_ack %d: old wnd %u wnd update %u\n", 
		    tcb - connection, tcb->snd_wnd, tcp->window));
	tcb->snd_wnd = tcp->window;
	tcb->snd_wls = tcp->seqno;
	tcb->snd_wla = tcp->ackno;
	dup = 0;
    }

    if (dup) {  /* Duplicate ack? */
	STINC(nackdup);

	/* Lower slow start threshold. */
	tcb->snd_ssthresh = MIN(tcb->snd_cwnd, tcb->snd_wnd) / 2;
	tcb->snd_ssthresh = MAX(tcb->snd_ssthresh, 2 * tcb->mss);
    }
}


/* 
 * Deal with options. We check only for MSS option. Heh, processing
 * one option is better than processing no options.
 * If there are no options, TCP_MSS is used and process_options returns 0;
 */
static int
process_options(struct tcb *tcb, char *option)
{
    uint16 mss;

    DPRINTF(2, ("process_options: options present %d\n", (int) option[0]));
    if (option[0] == TCP_OPTION_MSS) {
	switch(option[1]) {
	case 4:			/* kind, length, and a 2-byte mss */
	    kmemcpy(&mss, &option[2], sizeof(short));
	    mss = ntohs(mss);
	    DPRINTF(2, ("proposed mss %d we %d\n", mss, DEFAULT_MSS));
	    tcb->mss = MIN(DEFAULT_MSS, mss);
	    tcb->snd_cwnd = tcb->mss;
	    return 1;
	default:
	    DPRINTF(2, ("process_option: unrecognized length %d\n",
			option[1]));
	    return 0;
	}
    }
    return 0;
}


/*
 * Process incoming data. fin records is a the last byte has been sent.
 * Process_data returns whether all data has been received
 */
static int
process_data(struct tcb *tcb, int fin)
{
    struct tcp *tcp;
    struct ip *ip;
    int datalen;
    uint32 off;
    int all;
    int n;

    tcp = tcb->rcv_eit.tcp;
    ip = tcb->rcv_eit.ip;
    all = 0;
    datalen = ip->totallength - sizeof(struct ip) - sizeof(struct tcp);

    if (fin) STINC(nfinrcv);
    if (datalen > 0) {
	STINC_SIZE(rcvdata, log2(datalen));
	STINC(ndatarcv);
    }

    DPRINTF(2, ("process_data: %d bytes user data\n", datalen));

    if (SEQ_LEQ(tcp->seqno, tcb->rcv_next) && 
	SEQ_GEQ(tcp->seqno + datalen, tcb->rcv_next)) {
	/* Segment contains interesting data */
	if (datalen > 0) {

	    /* Copy the right data */
	    STINC(norder);
	    datalen = tcp->seqno + datalen - tcb->rcv_next;
	    n = datalen;
	    off =  tcb->rcv_next - tcp->seqno;
#ifndef AN2
	    net_copy_user_data(tcb, tcb->rcv_eit.data + off, datalen);
#else

assert(off < tcb->rcv_eit.a->r[0].sz); /* just for now, kerr */
tcb->rcv_eit.a->r[0].data = (char *)tcb->rcv_eit.a->r[0].data+off;
tcb->rcv_eit.a->r[0].sz = tcb->rcv_eit.a->r[0].sz-off;

	    net_copy_user_data(tcb, tcb->rcv_eit.a, datalen);

/* restore */
tcb->rcv_eit.a->r[0].data = (char *)tcb->rcv_eit.a->r[0].data-off;
tcb->rcv_eit.a->r[0].sz = tcb->rcv_eit.a->r[0].sz+off;

#endif /* AN2 */

	    /* Check for a fin */
	    if (fin && n == datalen) {
		DPRINTF(2, ("process_data: process fin\n"));
		tcb->rcv_next++;
		all = 1;
		STINC(nacksnd);
		tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
	    } else {
		/* Send an acknowledgement for every other ack, except
		 * if psh flag is set (then send immediately).
		 */
		if ((tcp->flags & TCP_FL_PSH) || (tcb->flags & TCB_FL_DELACK)) {
		    STINC(nacksnd);
		    tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
		} else {
		    /* Delay ack */
                    tcb->flags |= TCB_FL_DELACK;
		    STINC(ndelayack);
		}
	    }
	} else if (fin) {
	    /* A segment just carrying fin. */
	    DPRINTF(2,("process_data: process fin\n"));
	    tcb->rcv_next++;
	    all = 1;
	    STINC(nacksnd);
	    tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
	}
    } else if (SEQ_GT(tcp->seqno, tcb->rcv_next)) {
	/* Segment contains data passed rcv_next, keep it.
	 * (For now, drop it.).
	 */
	STINC(noutorder);
	printf("process_data out of order %d: segment seq %d is out of order n %d\n", 
		    tcb - connection, tcp->seqno, tcb->rcv_next);
    } else {
	demand(0, process_acceptable should have filtered this segment);
    }
    return all;
}

/*----------------------------------------------------------------------------*/

/*
 * Send data until window is closed.
 */
static void 
send_data(struct tcb *tcb)
{
    int window;
    int data_len;
    int n;

    /* Compute first how much space is left in the window, but dp not
     * send more data than congestion window allows.
     */
    window = MIN(tcb->snd_una + tcb->snd_wnd - tcb->snd_next, tcb->snd_cwnd);
    n = tcb->usr_snd_sz - tcb->usr_snd_off;

    /* Fire a number of MSS segments to other side, as long as the
     * send window has space.
     */

    while (n > 0 && window > 0) {
	
	DPRINTF(2, ("send_data %d: done %d una %u next %u max %u wnd %d cwnd %d\n",
		tcb - connection, tcb->usr_snd_done, tcb->snd_una, tcb->snd_next,
		tcb->snd_max, tcb->snd_wnd, tcb->snd_cwnd));


	/* Compute number of bytes on segment: never more than the window
	 * allows us and never more than 1 MSS.
	 */
	data_len = MIN(n, window);
	data_len = MIN(data_len, tcb->mss);
	demand(data_len > 0, send_data: expect wnd > 0);

	/* If data_len is smaller than some value, we should probably
	 * wait if sending to avoid silly window syndrom.
	 */
	STINC(ndata);
	STINC_SIZE(snddata, log2(data_len));
	n -= data_len;

	/* Send packet; if this packet is the last packet a blocking write, set
         * psh flag to indicate that we want to have an ack.  The ack will 
         * allow a blocking write to complete.
	 */
	if ((n > 0) || (tcb->flags & TCB_FL_ASYNWRITE)) {
	    tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, data_len);
	} else {
	    tcp_net_send(tcb, TCP_FL_ACK | TCP_FL_PSH, tcb->snd_next, data_len);
	}
        tcb->snd_next += data_len;
	if (SEQ_GT(tcb->snd_next, tcb->snd_max)) {
	    tcb->snd_max = tcb->snd_next;
	}
	tcb->usr_snd_off += data_len;
	window -= data_len;
    }

    timer_set(tcb, TCP_TIMEOUT);
}

/*----------------------------------------------------------------------------*/

/*
 * Timeout while TCB is connecting.
 */
static void
connect_timeout(struct tcb *tcb)
{
    char option[TCP_OPTION_MSS_LEN];
    uint16 mss;

    /* We currently use the default MSS value, but we should check
     * if the destination is on the same network and what network it
     * is.
     */
    option[0] = TCP_OPTION_MSS;
    option[1] = TCP_OPTION_MSS_LEN;
    mss = htons(DEFAULT_MSS);
    kmemcpy(option + 2, &mss, sizeof(uint16));

    tcb->retries++;
    if (tcb->retries > TCP_RETRY) {
      tcp_done(tcb);
    } else {
      STINC(nretrans);
      STINC(nsynsnd);

      printf("retransmit connect\n");

      tcp_net_send_option(tcb, TCP_FL_SYN, tcb->snd_iss, option, 
			  TCP_OPTION_MSS_LEN, 0); 
      timer_set(tcb, TCP_TIMEOUT);
    }
}


/*
 * Timeout while sending data.
 */
static void 
write_timeout(struct tcb *tcb)
{
    int n;
    int window;

    /* Lower slow start threshold, but never lower than 2 mss */
    tcb->snd_ssthresh = MIN(tcb->snd_cwnd, tcb->snd_wnd) / 2;
    tcb->snd_ssthresh = MAX(tcb->snd_ssthresh, 2 * tcb->mss);

    tcb->snd_cwnd = tcb->mss; 	/* back to slow start */

    tcb->flags &= ~TCB_FL_TIMEOUT;

    tcb->retries++;
    if (tcb->retries > TCP_RETRY) { /* is the connection broken? */
	tcp_done(tcb);
	return;
    }

    /* If window is zero, we have data to send, and everything
     * has been acknowledged sofar, then send a window 
     *
     * This code should also take care of the case, when the
     * receiver shrinks the window (window will be negative or 0).
     * If the window is negative now, the tests succeed and we will
     * send a probe.
     *
     * If not all data has been acked, we consider 
     * retransmission, and reset snd_next. If the window has
     * shrunk and the sender cannot retransmit, the code will get 
     * back here and the sender will send a probe.
     */

    window = tcb->snd_una + tcb->snd_wnd - tcb->snd_next;

    if (window <= 0 && tcb->usr_snd_off < tcb->usr_snd_sz && 
	SEQ_GEQ(tcb->snd_una, tcb->snd_max)) { 
	/* Send a window probe with one-byte data */
	STINC(nprobesnd);

	tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 1);

	timer_set(tcb, TCP_TIMEOUT);

	tcb->snd_next++;
	tcb->snd_max++;
	tcb->usr_snd_off += 1;
    } else {
	/* Retransmission: set the window back and start sending again. */

	STINC(nretrans);
	n = tcb->usr_snd_done - tcb->snd_una;
	tcb->usr_snd_off = tcb->usr_snd_sz - n;
	demand(tcb->usr_snd_off >= 0, send_data: off > 0);

	/* Start resending from una. This statement will
	 * also open the send window (if the receiver hasn't
	 * shrunk the window.
	 */
	tcb->snd_next = tcb->snd_una;
	send_data(tcb);
    }
}


/*
 * Timeout while closing the connection.
 */
static void
close_timeout(struct tcb *tcb)
{
    DPRINTF(2, ("close_timeout %d\n", tcb - connection));

    tcb->retries++;
    if (tcb->retries > TCP_RETRY) {
	tcp_done(tcb);
    } else {

	demand(tcb->snd_una + 1 == tcb->snd_next, tcp_close: wrong);

	/* reset snd_next and try again */
	tcb->snd_next = tcb->snd_una;

	STINC(nretrans);
	STINC(nfinsnd);
	tcp_net_send(tcb, TCP_FL_ACK|TCP_FL_FIN, tcb->snd_next, 0);
	tcb->snd_next++;	/* fin has a place in sequence space */
	tcb->snd_max++;
	timer_set(tcb, TCP_TIMEOUT);
    }
}


/*
 * Timeout while in ST_TIME_WAIT state.
 * We should wait for twice MSL. Later.
 */
static void
timewait_timeout(struct tcb *tcb)
{
    DPRINTF(2, ("timewait %d\n", tcb - connection));
    tcp_done(tcb);
    tcb_release(tcb);
}


/*
 * TCB tcb received a timeout.
 */
void
tcp_timeout(struct tcb *tcb)
{
    DPRINTF(2, ("tcb %d: time out; state %d\n", tcb - connection, tcb->state));

    printf("timeout %d %d\n", tcb - connection, tcb->state);

    log_dump();

    tcb->timer_retrans = 0;

    switch(tcb->state) {
    case TCB_ST_SYN_SENT:
	connect_timeout(tcb);
	break;
    case TCB_ST_ESTABLISHED:
	if (tcb->usr_snd_buf) write_timeout(tcb);
	break;
    case TCB_ST_CLOSE_WAIT:
	write_timeout(tcb);
	break;
    case TCB_ST_LAST_ACK:
    case TCB_ST_FIN_WAIT1:
    case TCB_ST_FIN_WAIT2:
    case TCB_ST_CLOSING:
	close_timeout(tcb);
	break;
    case TCB_ST_TIME_WAIT:
	timewait_timeout(tcb);
	break;
    default:
	printf("tcp_timeout %d: unexpected timeout in state %d\n", 
	       tcb - connection, tcb->state);
	break;
    }
}


/*----------------------------------------------------------------------------*/

/* 
 * Change to ST_CLOSED; This state is used to signal the client that we are
 * done. Tcb_release, which should be called by the client, will release the 
 * storage and change the state to ST_FREE.
 */
static void
tcp_done(struct tcb *tcb)
{
    DPRINTF(2, ("tcp_done: %d go to ST_CLOSED\n", tcb - connection));

    tcp_net_close(tcb);
    tcb->timer_retrans = 0;
    if (tcb->flags & TCB_FL_ASYNCLOSE) tcb_release(tcb);
    else tcb->state = TCB_ST_CLOSED;
    tcb->flags = 0;
}


static int
st_illegal(struct tcb *tcb)
{
    printf("st_illegal %d: illegal state to receive msgs in: %d\n", 
	   tcb - connection, tcb->state);

    tcb_print(tcb);
    return 1;
}


/* 
 * Waiting for a connection request.
 */
static int
st_listen(struct tcb *tcb)
{
    struct tcb *new_tcb;
    struct tcp *rcv_tcp;
    struct ip *rcv_ip;
#ifndef AN2
    struct eth *rcv_eth;
#endif
    uint32 srcip;
    uint32 dstip;
    int hlen;
    char option[TCP_OPTION_MSS_LEN];
    int mss_option;
    uint16 mss;
    int freep = 1;

    DPRINTF(3, ("st_listen\n"));

    mss_option = 0;
    rcv_tcp = tcb->rcv_eit.tcp;
    rcv_ip = tcb->rcv_eit.ip;

    /* Fill in the destination fields of the tcb and template headers. */
#ifndef AN2
    rcv_eth = tcb->rcv_eit.eth = (struct eth *) tcb->rcv_eit.msg;
#endif

    /* Check for rst and ignore */
    if (rcv_tcp->flags & TCP_FL_RST) {
	return(freep);
    }

    /* Check for ack, which shouldn't arrive on connection in listen state. */
    /* Be careful: destination fields are not set yet, since we do not have
     * a connection setup.  Use tcp_net_send_dst().
     */
    if (rcv_tcp->flags & TCP_FL_ACK) {
	kmemcpy(&dstip, &rcv_ip->source[0], sizeof dstip);
	tcp_net_send_dst(tcb, TCP_FL_RST, rcv_tcp->ackno, 0, 
			 ntohs(rcv_tcp->src_port), dstip, rcv_eth->src_addr);
	return(freep);
    }

    /* Check for syn and send reply */
    if ((rcv_tcp->flags & TCP_FL_SYN) && tcb->queue_len < tcb->nlisten) {
	DPRINTF(2, ("st_listen: received SYN\n"));

	kmemcpy(&dstip, &rcv_ip->source[0], sizeof dstip);
	kmemcpy(&srcip, &rcv_ip->destination[0], sizeof srcip);

#ifndef AN2
	/* Get a tcb for this connection and init it. */
	if ((new_tcb = tcp_open(ntohs(rcv_tcp->src_port), 
				dstip, rcv_eth->src_addr, 
				ntohs(rcv_tcp->dst_port), 
				srcip, rcv_eth->dst_addr, 0)) == 0) {
	    demand(0, st_listen: out of tcbs);
	}
#else
	/* Get a tcb for this connection and init it. */
	if ((new_tcb = tcp_open(ntohs(rcv_tcp->src_port), 
				dstip, 0, ntohs(rcv_tcp->dst_port), 
				srcip, 0,
				tcb->rcv_eit.vc)) == 0) {
	    demand(0, st_listen: out of tcbs);
	}
#endif

	new_tcb->rcv_irs = rcv_tcp->seqno;
	new_tcb->rcv_next = rcv_tcp->seqno + 1;

	/* Process options */
	hlen = ((rcv_tcp->offset >> 4) & 0xF) << 2;
	if (hlen > sizeof(struct tcp)) { /* options? */
#ifdef AN2
	    mss_option = process_options(new_tcb, 
					 (char *)(tcb->rcv_eit.tcp + 1));
#else
	    mss_option = process_options(new_tcb, tcb->rcv_eit.data);
#endif
	}

	if (mss_option) {
	    /* Append options to reply. */
	    option[0] = TCP_OPTION_MSS;
	    option[1] = TCP_OPTION_MSS_LEN;
	    mss = htons(new_tcb->mss);
	    kmemcpy(option + 2, &mss, sizeof(uint16));
	}

	/* Send an syn + ack (the second step in the 3-way handshake). */
	tcp_net_send_option(new_tcb, TCP_FL_SYN|TCP_FL_ACK, new_tcb->snd_iss,
			    option, TCP_OPTION_MSS_LEN, 0);

	new_tcb->snd_next = new_tcb->snd_iss + 1;
	new_tcb->snd_max = new_tcb->snd_next;
	new_tcb->snd_una = new_tcb->snd_iss;
	new_tcb->state = TCB_ST_SYN_RECEIVED;

	/* Append new_tcb to the list of tcbs ready to be accepted. */
	if (tcb->next == 0) {
	    tcb->next = new_tcb;
	    tcb->last = new_tcb;
	} else {
	    tcb->last->next = new_tcb;
	    tcb->last = new_tcb;
	}
	tcb->queue_len++;
    }
    return(freep);
}


/* 
 * We have sent our syn. Wait for an ack and a syn from the other side.
 */
static int
st_syn_sent(struct tcb *tcb)
{
    struct tcp *tcp;
    char *option;
    int hlen;
    int freep = 1;

    DPRINTF(3, ("st_syn_sent\n"));

    tcp = tcb->rcv_eit.tcp;
#ifdef AN2
    option = (char *) (tcp + 1);
#else
    option = tcb->rcv_eit.data;
#endif

    /* Check ack; if not acceptable send rst */
    if ((tcp->flags & TCP_FL_ACK) && (SEQ_LEQ(tcp->ackno, tcb->snd_iss) ||
				      SEQ_GT(tcp->ackno, tcb->snd_next))) {
	STINC(nrstsnd);
	tcp_net_send(tcb, TCP_FL_RST|TCP_FL_ACK, tcp->ackno, 0);
	return(freep);
    }

    /* Check for rst, and drop the connection */
    if (tcp->flags & TCP_FL_RST) {
	STINC(nrstrcv);
	tcp_done(tcb);
	return(freep);
    }

    /* Check for syn and set irs and next */
    if (tcp->flags & TCP_FL_SYN) {
	STINC(nsynrcv);
	tcb->rcv_irs = tcp->seqno;
	tcb->rcv_next = tcp->seqno + 1; /* y + 1 */
    }

    /* Acceptable ack, set una and window variables */
    if (tcp->flags & TCP_FL_ACK) {
	STINC(nack);
        tcb->snd_una = tcp->ackno;
	tcb->snd_wnd = tcp->window;
	tcb->snd_wls = tcp->seqno;
    }

    /* Check if our syn has been acked, if so go to ESTABLISHED. */
    if (SEQ_GT(tcb->snd_una, tcb->snd_iss)) {
        /* Our syn has been acked */
	DPRINTF(2, ("st_syn_sent %d: go to ESTABLISHED\n", tcb - connection));
	tcb->state = TCB_ST_ESTABLISHED;

	/* Reset timer */
	tcb->timer_retrans = 0;
      
	hlen = ((tcp->offset >> 4) & 0xF) << 2;
	if (hlen > sizeof(struct tcp)) { /* options? */
	    process_options(tcb, option);
	}

	/* Complete handshake: send ack */
	STINC(nacksnd);
	tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
    } else {
	/* Received a syn on our syn but without ack, send ack on other side's
	 * syn.
	 */
	DPRINTF(2, ("st_syn_sent %d: go to SYN_RECEIVED\n", tcb - connection));
	tcb->state = TCB_ST_SYN_RECEIVED;
	/* Send seq == iss, ack == rcv_next. Perhaps send options too. */
	STINC(nacksnd);
	STINC(nsynsnd);
	tcp_net_send(tcb, TCP_FL_SYN|TCP_FL_ACK, tcb->snd_iss, 0);
      
    }
    return(freep);
}


/*
 * We have received a syn s = x, we have sent syn y ack x +1, and are now
 * waiting for ack s = x, a = y + 1.
 * Or, we have sent a syn, and have received a syn, but our still
 * waiting for the ack on our syn.
 */
static int
st_syn_received(struct tcb *tcb)
{
    struct tcp *tcp;
    int freep = 1;

    DPRINTF(3, ("st_synrecv\n"));

    tcp = tcb->rcv_eit.tcp;

    if (!process_acceptable(tcb)) return(freep);

    if (!process_rst_syn_ack(tcb)) return(freep);

    /* If acceptable ack, move to established; otherwise send reset */
    if ((tcp->flags & TCP_FL_ACK) && SEQ_GEQ(tcp->ackno, tcb->snd_una)
	&& SEQ_LEQ(tcp->ackno, tcb->snd_next)) {
	DPRINTF(2, ("st_syn_received: go to ESTABLISHED\n"));
        tcb->snd_una = tcp->ackno;
	tcb->usr_snd_done = tcb->snd_una;
	tcb->snd_wnd = tcp->window;
	tcb->snd_wls = tcp->seqno;
	tcb->snd_wla = tcp->ackno;
	tcb->state = TCB_ST_ESTABLISHED;
    } else {
	tcp_net_send(tcb, TCP_FL_RST, tcp->ackno, 0);
	return(freep);
    }
    
    process_ack(tcb);

    freep = 0;
    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	DPRINTF(2, ("st_syn_received: go to ST_CLOSE_WAIT\n"));
      	tcb->state = TCB_ST_CLOSE_WAIT;
    }
    return(freep);
}


/*
 * Connection is established; deal with incoming packet.
 */
static int
st_established(struct tcb *tcb)
{
    struct tcp *tcp;
    int freep = 1;
    tcp = tcb->rcv_eit.tcp;

    DPRINTF(3, ("st_established\n"));

    if (!process_acceptable(tcb)) return(freep);

    if (!process_rst_syn_ack(tcb)) return(freep);

    process_ack(tcb);

    freep = 0;
    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	DPRINTF(2, ("st_established: go to ST_CLOSE_WAIT\n"));
      	tcb->state = TCB_ST_CLOSE_WAIT;
    }

    return(freep);
}


/*
 * We have received a fin and acked it, but our side hasn't sent a fin
 * yet, since tcp_close has not been called yet.
 * Update the send window; we could still be sending data.
 */
static int
st_close_wait(struct tcb *tcb)
{
    int freep = 1;

    DPRINTF(3, ("st_closewait\n"));

    if (!process_acceptable(tcb)) return(freep);

    if (!process_rst_syn_ack(tcb)) return(freep);

    process_ack(tcb);
    return(freep);
}


/*
 * We have called tcp_close and we are waiting for an ack.
 * Process data and wait for the other side to close.
 */
static int
st_fin_wait1(struct tcb *tcb)
{
    struct tcp *tcp;
    int freep = 1;

    DPRINTF(3, ("st_finwait1\n"));

    tcp = tcb->rcv_eit.tcp;
    
    if (!process_acceptable(tcb)) return(freep);

    if (!process_rst_syn_ack(tcb)) return(freep);

    process_ack(tcb);

    if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
	DPRINTF(2, ("st_fin_wait1 %d: go to FIN_WAIT2\n", tcb - connection));
	tcb->state = TCB_ST_FIN_WAIT2;
    }

    freep = 0;
    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	if(tcb->state == TCB_ST_FIN_WAIT2) {
	    DPRINTF(2, ("st_fin_wait1 %d: go to TIME_WAIT\n",
			tcb - connection));
	    tcb->state = TCB_ST_TIME_WAIT;
	} else {
	    DPRINTF(2, ("st_fin_wait1 %d: go to CLOSING\n", tcb - connection));
	    tcb->state = TCB_ST_CLOSING;
	}
    }
    return(freep);
}


/*
 * We have sent a fin and received an ack on it.
 * Process data and wait for the other side to close.
 */
static int
st_fin_wait2(struct tcb *tcb)
{
    struct tcp *tcp;
    int freep = 1;
    
    DPRINTF(3, ("st_finwait2\n"));

    tcp = tcb->rcv_eit.tcp;

    if (!process_acceptable(tcb)) return(freep);
    
    if (!process_rst_syn_ack(tcb)) return(freep);
    
    process_ack(tcb);
    
    freep = 0;
    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	DPRINTF(2, ("st_fin_wait2 %d: go to TIME_WAIT\n", tcb - connection));
	tcb->state = TCB_ST_TIME_WAIT;
    }
    return(freep);
}



/*
 * Both sides have sent fin. We have acked the other side's fin.
 * Wait until our fin is acked.
 */
static int
st_last_ack(struct tcb *tcb)
{
    struct tcp *tcp;
    int freep = 1;
    
    DPRINTF(3, ("st_lastack\n"));

    tcp = tcb->rcv_eit.tcp;
    
    if (!process_acceptable(tcb)) return(freep);

    if (tcp->flags & TCP_FL_RST) {
	tcp_done(tcb);
	return(freep);
    }

    if (!process_syn_ack(tcb)) return(freep);
    
    process_ack(tcb);
    
    if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
	DPRINTF(2, ("st_last_ack %d: go to ST_CLOSED\n", tcb - connection)); 
	tcp_done(tcb);
    }
    return(freep);
}


/*
 * We sent out a fin and have received a fin in response. We
 * are waiting for the ack.
 */
static int
st_closing(struct tcb *tcb)
{
    struct tcp *tcp;
    int freep = 1;

    DPRINTF(3, ("st_closting\n"));

    tcp = tcb->rcv_eit.tcp;

    if (!process_acceptable(tcb)) return(freep);

    if (tcp->flags & TCP_FL_RST) {
	tcp_done(tcb);
	return(freep);
    }

    if (!process_syn_ack(tcb)) return(freep);

    process_ack(tcb);

    if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
	DPRINTF(2, ("st_closing %d: received ack segment, to to TIME_WAIT\n", 
	       tcb - connection));
	tcb->state = TCB_ST_TIME_WAIT;
    }
    return(freep);
}


/*
 * We have sent a fin and have received an ack on it. We also have
 * received a fin from the other side and acked it.
 */
static int
st_time_wait(struct tcb *tcb)
{
    struct tcp *tcp;
    int freep = 1;

    DPRINTF(3, ("st_timewait\n"));

    tcp = tcb->rcv_eit.tcp;

    if (!process_acceptable(tcb)) return(freep);

    if (tcp->flags & TCP_FL_RST) {
	tcp_done(tcb);
	return(freep);
    }

    if (!process_syn_ack(tcb)) return(freep);

    process_ack(tcb);

    /* Check for a retransmission of a fin, and send an ack. */
    if (tcp->flags & TCP_FL_FIN) {
	tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
    }
    return(freep);
}


char sumtstmsg[] = "sumtstmsg: 0x%x %d\n";
char gotintr[] = "GOT AN INTERRUPT\n";
void ip_slow_sum(long *sum, void *adr, const int len)
{
    long mysum = *sum;
    u_int onintr = (u_int) __sysinfo.si_smc[0].nintr;

    kprintf ("addr is %p, len is %d\n", adr, len);
    kprintf ("nintr = %u, ticks = %u\n",
	     *(u_int *) &__sysinfo.si_smc[0].nintr,
	     *(u_int *) &__sysinfo.si_system_ticks);

    assert (! (len & 3));
    if (len > 0)
    asm ("testl %%eax,%%eax        # Clear CF\n"
	 "\tjmp 1f\n"
	 "2:\n"
	 "\tpushl %7\n"
	 "\tcall _kprintf\n"
	 "\taddl $16,%%esp\n"
	 "\tjmp 3f\n"
	 "1:\n"
#if 0
	 "\tpushfl\n"
	 "\tpushl %%eax\n"
	 "\tpushl %%ecx\n"
	 "\tpushl %%edx\n"
	 "\tmovl %6,%%eax\n"
	 "\tcmpl %%eax,%8\n"
	 "\tjne 2b\n"
#if 0
	 "\tpushl %4\n"
	 "\tpushl %%esi\n"
	 "\tpushl %5\n"
	 "\tcall _kprintf\n"
	 "\taddl $12,%%esp\n"
#endif
	 "\tpopl %%edx\n"
	 "\tpopl %%ecx\n"
	 "\tpopl %%eax\n"
	 "\tpopfl\n"
#endif
	 "\tmovl (%1),%%eax\n"
	 "\tleal 4(%1),%1\n"
	 "\tadcl %%eax,%0\n"
	 "\tdecl %4\n"
	 "\tjnz 1b\n"
	 "\tadcl $0,%0             # Add the carry bit back in\n"
	 "3:"
	 : "=d" (mysum), "=S" (adr)
	 : "0" (mysum), "1" (adr), "c" (len>>2), "i" (sumtstmsg),
	 "m" (onintr), "i" (gotintr), "m" (__sysinfo.si_smc[0].nintr)
	 : "memory", "cc", "eax", "ecx");

    kprintf ("nintr = %u, ticks = %u\n",
	     *(u_int *) &__sysinfo.si_smc[0].nintr,
	     *(u_int *) &__sysinfo.si_system_ticks);

    // printf ("The bug happens before here.\n");

    // kprintf ("addr is %p, len is %d\n", adr, len);

    *sum = mysum;
}


/*
 * Process a segment. The segment has been received in rcv_eit.
 */
void
tcp_process(struct tcb *tcb, struct ae_recv *r, int len)
{
    struct tcp *tcp;
    struct ip *rcv_ip;
    char *d;
    uint16 n;
    int datalen;
    int freep = 1;

    DPRINTF(2, ("tcp_process %d %u:\n", tcb - connection, len));

#ifdef AEGIS
#ifdef AN2
    /* Set pointers to headers in recv structure. */
    d = r->r[0].data;
    tcb->rcv_eit.msg = &d[0];
    rcv_ip = tcb->rcv_eit.ip = (struct ip *) (tcb->rcv_eit.msg); 
    tcp = tcb->rcv_eit.tcp = (struct tcp *) (tcb->rcv_eit.msg +
					     sizeof(struct ip));
    tcb->rcv_eit.a = r;  /* note we're not copying r here */
    tcb->rcv_eit.a->r[0].data = tcb->rcv_eit.msg + sizeof(struct ip) + 
	sizeof(struct tcp);
    tcb->rcv_eit.a->r[0].sz = r->r[0].sz - 
	(sizeof(struct ip) + sizeof(struct tcp));
    assert(tcb->rcv_eit.a->r[0].sz > 0);
#else
    /* Set pointers to headers in recv structure. */
    d = r->r[0].data;
    tcb->rcv_eit.msg = &d[-ETHER_ALIGN];
    rcv_ip = tcb->rcv_eit.ip = (struct ip *) (tcb->rcv_eit.msg + 
                                              sizeof(struct eth)); 
    tcp = tcb->rcv_eit.tcp = (struct tcp *) (tcb->rcv_eit.msg + 
                                             sizeof(struct eth) +
                                             sizeof(struct ip));
    tcb->rcv_eit.data = tcb->rcv_eit.msg + sizeof(struct eth) + 
        sizeof(struct ip) + sizeof(struct tcp) + ETHER_ALIGN;

    /* Move the headers to a short-aligned address, yuck. */
    kmemcpy(tcb->rcv_eit.msg, &d[0], sizeof(struct eth) + 
           sizeof(struct ip) + sizeof(struct tcp));
#endif
#endif
#ifdef EXOPC
    /* Set pointers to headers in recv structure. */

    assert(sizeof(struct ip) == 20);
    assert(sizeof(struct eth) == 14);
    assert(sizeof(struct tcp) == 20);

    d = ADD_UVA(r->r[0].data);
    tcb->rcv_eit.msg = &d[-ETHER_ALIGN];
    rcv_ip = tcb->rcv_eit.ip = (struct ip *) (tcb->rcv_eit.msg + 
					      sizeof(struct eth));
    tcp = tcb->rcv_eit.tcp = (struct tcp *) (tcb->rcv_eit.msg + 
					     sizeof(struct eth) + 
					     sizeof(struct ip));
    tcb->rcv_eit.data = tcb->rcv_eit.msg + sizeof(struct eth) + 
	                sizeof(struct ip) + sizeof(struct tcp);
#endif

    /* Check ip cksum? */
    if (rcv_ip->checksum != 0) {
#ifdef EXOPC
	long sum = 0;

	ip_sum(sum, &(rcv_ip->vrsihl), sizeof(struct ip));
	rcv_ip->checksum = fold_32bit_sum(sum);
#else
	rcv_ip->checksum = inet_checksum(
	    (unsigned short *) &(rcv_ip->vrsihl),
	    sizeof(struct ip), 0, 1);
#endif

	if (rcv_ip->checksum) {
	    STINC(nbadipcksum);
	    DPRINTF(2, ("tcp_process: bad ip checksum\n"));
	    return;
	}
    }

    rcv_ip->totallength = ntohs(rcv_ip->totallength);
    rcv_ip->fragoffset = ntohs(rcv_ip->fragoffset);
    rcv_ip->identification = ntohs(rcv_ip->identification);

    STINC(nseg);

    /* We expect that tcp segments will never be fragmented since
     * the other end is not to send something bigger than the negotiated
     * MSS.
     */
    if (rcv_ip->totallength > len || (rcv_ip->fragoffset & IP_FRAGOFF) > 0) {
	/* Segment is not complete */
	STINC(nincomplete);
	DPRINTF(2, ("tcp_proces: segment with len %d off %d is not complete\n",
		    rcv_ip->totallength, rcv_ip->fragoffset));
	return;
    }

    /* Tcp cksum? */
    if (tcp->cksum != 0) {
	long sum = 0;

	/* Repair pseudo header */
	n = rcv_ip->totallength - sizeof(struct ip);
	rcv_ip->ttl = 0;
	rcv_ip->checksum = htons(n);

#if EXOPC
	ip_sum(sum, &(rcv_ip->ttl), IP_PSEUDO_HDR_SIZE + sizeof(struct tcp));
	ip_sum(sum, tcb->rcv_eit.data, n - sizeof(struct tcp));
	tcp->cksum = fold_32bit_sum(sum);
#else
	sum = inet_sum((unsigned short *) &(rcv_ip->ttl), 
		       IP_PSEUDO_HDR_SIZE + sizeof(struct tcp), 0);

	if (n - sizeof(struct tcp) < 40)
	    tcp->cksum = inet_cksum((unsigned short *) tcb->rcv_eit.data, 
				    n - sizeof(struct tcp), sum);
	else
	    tcp->cksum = inet_checksum((unsigned short *) tcb->rcv_eit.data, 
				       n - sizeof(struct tcp), sum, 1);
#endif
	if (tcp->cksum) {
		
	    STINC(nbadtcpcksum);
	    DPRINTF(2, ("tcp_process: bad tcp checksum\n"));
	    return;
	
	}
    }
    
    tcp->ackno = ntohl(tcp->ackno);
    tcp->seqno = ntohl(tcp->seqno);
    tcp->window = ntohs(tcp->window);


    tcb->retries = 0;		/* reset retries */

#ifdef LOGGING
    log_insert(tcb, tcp, 
	       rcv_ip->totallength - sizeof(struct ip) - sizeof(struct tcp),
	       'r');
#endif

#ifdef HEADERPRD
    if ((tcb->state == TCB_ST_ESTABLISHED) && 
	((tcp->flags & (TCP_FL_FIN | TCP_FL_SYN | TCP_FL_RST | TCP_FL_URG |
		       TCP_FL_ACK)) == TCP_FL_ACK) &&
	(tcp->seqno == tcb->rcv_next)) {

	datalen = rcv_ip->totallength - sizeof(struct ip) - sizeof(struct tcp);

	/* Test for expected ack */
	if (SEQ_GEQ(tcp->ackno, tcb->snd_una) && 
	    SEQ_LEQ(tcp->ackno, tcb->snd_next) && 
	    tcb->snd_next == tcb->snd_max) {
	    
	    STINC(nackprd);
	    tcb->snd_una = tcp->ackno; 		/* Update una */

	    /* If this completes a non-blocking write, call the user-supplied
	     * release function.
	     */
	    if ((tcb->flags & TCB_FL_ASYNWRITE) &&
		SEQ_GEQ(tcb->snd_una, tcb->usr_snd_done)) {
		tcb->flags &= ~TCB_FL_ASYNWRITE;
		if (tcb->usr_snd_release) (*(tcb->usr_snd_release))();
	    }


	    if (tcb->snd_cwnd <= tcb->snd_ssthresh) { /* slow start? */
		tcb->snd_cwnd += tcb->snd_cwnd;   /* Increase congestion wnd */
	    } else {		/* congestion avoidance */
		/* BUG: should do this once per RTT */
		tcb->snd_cwnd += tcb->mss * (tcb->mss/tcb->snd_cwnd);
	    }

	    timer_set(tcb, TCP_TIMEOUT); 	/* Reset timer */

	    /* If an acknowledgement and it is interesting, update snd_wnd. */
	    if (
		(tcp->flags & TCP_FL_ACK) && 
		(SEQ_LT(tcb->snd_wls, tcp->seqno) ||
		 (tcb->snd_wls == tcp->seqno && SEQ_LT(tcb->snd_wla, tcp->ackno)) ||
		 (tcb->snd_wla == tcp->ackno && tcp->window > tcb->snd_wnd))) {
		tcb->snd_wnd = tcp->window;
		tcb->snd_wls = tcp->seqno;
		tcb->snd_wla = tcp->ackno;
	    }

	    /* If datalen is 0, it was a pure ack; return. */
	    if (datalen == 0) {

		if (tcb->usr_snd_buf != 0) send_data(tcb);

		return;
	    }
	}

	/* Test for expected data */
	if (datalen > 0 && tcp->ackno == tcb->snd_una) {
	    STINC(ndataprd);
	    STINC_SIZE(rcvdata, log2(datalen));
#ifndef AN2
	    net_copy_user_data(tcb, tcb->rcv_eit.data, datalen);
#else /* AN2 */
	    net_copy_user_data(tcb, tcb->rcv_eit.a, datalen);
#endif /* AN2 */

	    /* Send an acknowledgement for every other ack */
	    if ((tcp->flags & TCP_FL_PSH) || (tcb->flags & TCB_FL_DELACK)) {
		STINC(nacksnd);
		tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
	    } else {
		/* Delay ack */
		tcb->flags |= TCB_FL_DELACK;
		STINC(ndelayack);
	    }

	    if (tcb->usr_snd_buf != 0) send_data(tcb);
	    return;
	} 
    }
#endif /* HEADERPRD */

    assert(state[tcb->state] != 0);

    freep = state[tcb->state](tcb);
    if (tcb->usr_snd_buf != 0) send_data(tcb);
}


/*----------------------------------------------------------------------------*/


/* 
 * Setup the connection. It implements the three-way handshake
 * protocol.
 */
struct tcb *
tcp_connect(uint16 dst_port, uint32 ip_dst_addr, uint8 *eth_dst_addr,
	    uint16 src_port, uint32 ip_src_addr, uint8 *eth_src_addr)
{
    struct tcb *tcb;
    char option[TCP_OPTION_MSS_LEN];
    uint16 mss;

    DPRINTF(2, ("tcp_connect: connect to port %d\n", dst_port));

    /* Get a tcb and init it. */
    if ((tcb = tcp_open(dst_port, ip_dst_addr, eth_dst_addr, src_port, 
			ip_src_addr, eth_src_addr, 1)) == 0) {
        return 0;
    }

    DPRINTF(2, ("tcp_connect %d: go to SYN_SENT\n", tcb - connection));
    tcb->state = TCB_ST_SYN_SENT;  

    /* We currently use the default MSS value, but we should check
     * if the destination is on the same network and what network it
     * is.
     */
    option[0] = TCP_OPTION_MSS;
    option[1] = TCP_OPTION_MSS_LEN;
    mss = htons(DEFAULT_MSS);
    kmemcpy(option + 2, &mss, sizeof(uint16));

    tcb->snd_next = tcb->snd_iss + 1;
    tcb->snd_max = tcb->snd_next;
    tcb->snd_una = tcb->snd_iss;
    tcb->retries = 0;
    
    /* Phase 1: send SYN seq x and MSS option. */
    STINC(nsynsnd);
    tcp_net_send_option(tcb, TCP_FL_SYN, tcb->snd_iss, option, 
			TCP_OPTION_MSS_LEN, 0);
    timer_set(tcb, TCP_TIMEOUT);

    /* Wait for SYN+ACK seq y ack x+1. */
    for (;;) {
	tcp_net_poll_all(0);

	if (SEQ_GT(tcb->snd_una, tcb->snd_iss)) {
	    demand(tcb->state == TCB_ST_ESTABLISHED, tcp_connect: weird);
	    tcb->timer_retrans = 0;
	    DPRINTF(2, ("tcp_connect: connect to port %d done\n", 
			dst_port));	
	    return tcb;
	}

	if (tcb->state == TCB_ST_CLOSED) {
	    tcb_release(tcb);
	    return 0;
	}
    }
}


/*
 * Handoff connection.  Parent contains the current state of the connection.
 * Handoff builds a copy of the state so that this process can take
 * over the connection.
 */
struct tcb *
tcp_handoff(struct tcb *parent)
{
    struct tcb *tcb;

    DPRINTF(2, ("tcp_handoff\n"));

    tcb_print(parent);

    /* Get a tcb and init it. */
    if ((tcb = tcp_open(parent->port_dst, parent->ip_dst, parent->eth_dst, 
			parent->port_src, parent->ip_src, parent->eth_src, 1))
	== 0) {
        return 0;
    }

    tcb->state = parent->state;
    tcb->snd_una = parent->snd_una;
    tcb->snd_next = parent->snd_next;
    tcb->snd_max = parent->snd_max;
    tcb->snd_wnd = parent->snd_wnd;
    tcb->snd_cwnd = parent->snd_cwnd;
    tcb->snd_ssthresh = parent->snd_ssthresh;
    tcb->snd_up = parent->snd_up;
    tcb->snd_wls = parent->snd_wls;
    tcb->snd_wla = parent->snd_wla;
    tcb->snd_iss = parent->snd_iss;

    tcb->rcv_next = parent->rcv_next;
    tcb->rcv_wnd = parent->rcv_wnd;
    tcb->rcv_irs = parent->rcv_irs;
    
    tcb->mss = parent->mss;
    
    tcb_print(tcb);
    
    return tcb;
}


void
tcp_free(struct tcb *tcb)
{
    DPRINTF(2, ("tcp_free\n"));

    tcp_net_close(tcb);
    tcb->timer_retrans = 0;
    tcb_release(tcb);
    tcb->flags = 0;
}

/*
 * Listen for connections requests.
 */
struct tcb *
tcp_listen(int nlisten,
	   uint16 dst_port, uint32 ip_dst_addr, uint8 *eth_dst_addr,
	   uint16 src_port, uint32 ip_src_addr, uint8 *eth_src_addr)
{
    struct tcb *tcb;

    STINC(nlisten);

    DPRINTF(2, ("tcp_listen: src_port %d  dst_port %d\n", src_port, dst_port));

    /* Get a tcb and init it. */
    if ((tcb = tcp_open(dst_port, ip_dst_addr, eth_dst_addr, src_port, 
			ip_src_addr, eth_src_addr, 1)) == 0) {
      return 0;
    }

#ifdef AEGIS
#ifndef AN2
    demand( (int) tcb->snd_eit.msg % 4 == 2, tcp_connect: p not proper aligned);
#endif
#endif

    tcb->nlisten = nlisten + 1;	/* always willing to accept 1 connection */
    tcb->queue_len = 0;
    tcb->state = TCB_ST_LISTEN;
    
    return tcb;
}


/*
 * Accept an established connection.
 */
struct tcb *
tcp_accept(struct tcb *list_tcb)
{
    struct tcb *tcb;

    DPRINTF(3, ("tcp_accept: %d\n", list_tcb - connection));

    STINC(naccept);

    for (;;) {

	tcp_net_poll_all(1);

	tcb = list_tcb->next;

	if (tcb != 0 && (tcb->state == TCB_ST_ESTABLISHED || 
			 tcb->state == TCB_ST_CLOSE_WAIT)) {
	    break;
	}
    }

    /* Dequeue tcb from the list. */
    list_tcb->queue_len--;
    list_tcb->next = tcb->next;
    tcb->next = 0;


    return tcb;
}


/* check to see if accept would be successful */
static int tcp_accept_peek (struct tcb *list_tcb)
{
   struct tcb *tcb;

   tcp_net_poll_all(1);

   tcb = list_tcb->next;

   if ((tcb == NULL) || ((tcb->state != TCB_ST_ESTABLISHED) && (tcb->state != TCB_ST_CLOSE_WAIT))) {
      return (0);
   }

   return (1);
}


/*
 * Accept an established connection.
 */
struct tcb *
tcp_non_blocking_accept(struct tcb *list_tcb)
{
    struct tcb *tcb;

    DPRINTF(3, ("tcp_non_blocking_accept: %d\n", list_tcb - connection));

    STINC(naccept);

    tcp_net_poll_all(1);

    tcb = list_tcb->next;

    if ((tcb == 0) || ((tcb->state != TCB_ST_ESTABLISHED)
                        && (tcb->state != TCB_ST_CLOSE_WAIT))) {
        return (NULL);
    }

    /* Dequeue tcb from the list. */
    list_tcb->queue_len--;
    list_tcb->next = tcb->next;
    tcb->next = 0;

    return tcb;
}


/*
 * Poll for segment.
 */
void
tcp_poll(int once)
{
    tcp_net_poll_all(once);
}


/* won't work as long as this is a polling libray!!!! */
/* returns number of waiting bytes */
int 
tcp_peek(struct tcb *tcb)
{
    int sz = 0;
    buffer_p p;

    if (tcb->state == TCB_ST_LISTEN) {
       return (tcp_accept_peek(tcb));
    }

    /* We are allowed to read from connection after we closed it for writing */
    if (tcb->state != TCB_ST_ESTABLISHED && tcb->state != TCB_ST_FIN_WAIT2 && tcb->state != TCB_ST_CLOSE_WAIT && tcb->state != TCB_ST_TIME_WAIT) {
       kprintf ("tcp_peek: state %d\n", tcb->state);
    }
    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_FIN_WAIT2
	   ||tcb->state == TCB_ST_CLOSE_WAIT|| tcb->state == TCB_ST_TIME_WAIT,
	 tcp_read: bad tcb);

    tcp_net_poll_all(1);
    /* Count number of bytes in buffers. */
    for (p = tcb->buf_list; p != 0; p = p->b_next) {
	sz += p->b_n;
    }
    return(sz);
}


/* 
 * Asynchronous (i.e., blocking) read of segment on the given connection.
 */
int 
tcp_non_blocking_read(struct tcb *tcb, void *addr, size_t sz)
{
    buffer_p p, q;
    int r, not_copied_all;

    /* We are allowed to read from connection after we closed it for writing */
    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_FIN_WAIT2
	   ||tcb->state == TCB_ST_CLOSE_WAIT|| tcb->state == TCB_ST_TIME_WAIT,
       tcp_read: bad tcb);

    STINC(nread);

    DPRINTF(2, ("tcp_non_blocking_read %d: status %d addr 0x%p size %u\n", 
	   tcb - connection, tcb->state, addr, sz));

    tcb->usr_rcv_buf = addr;	/* application is ready to accept data */
    tcb->usr_rcv_off = 0;
    tcb->usr_rcv_sz = sz;

    not_copied_all = 0;
    /* First copy buffered data and release buffers. */
    for (p = tcb->buf_list, q = 0; p != 0; p = q) {
	if (p->b_n > (tcb->usr_rcv_sz - tcb->usr_rcv_off)) {
           kmemcpy(tcb->usr_rcv_buf + tcb->usr_rcv_off, p->b_current, (tcb->usr_rcv_sz - tcb->usr_rcv_off));
	   p->b_current += tcb->usr_rcv_sz - tcb->usr_rcv_off;
	   p->b_n -= tcb->usr_rcv_sz - tcb->usr_rcv_off;
	   tcb->usr_rcv_off += tcb->usr_rcv_sz - tcb->usr_rcv_off;
	   tcb->buf_list = p;
	   not_copied_all = 1;
	   break;
	} else {
	   kmemcpy(tcb->usr_rcv_buf + tcb->usr_rcv_off, p->b_current, p->b_n);
	   tcb->usr_rcv_off += p->b_n;
	   tcb->rcv_wnd += BUFSIZE;
	   q = p->b_next;
	   tcp_buf_free(p);
	}
    }
    if (!not_copied_all) {
	tcb->buf_list = 0;
	tcb->buf_list_tail = 0;
    }
	
    if(tcb->usr_rcv_off < sz) {
	tcp_net_poll_all(1);

	if (tcb->state == TCB_ST_CLOSED) {
	    tcb_release(tcb);
	    return -1;
        }

	/* If the other side closed, then return what we have received sofar.
         * The next read will return 0. 
	 */
	if (tcb->state == TCB_ST_CLOSE_WAIT || tcb->state == TCB_ST_TIME_WAIT) {
	    DPRINTF(2, 
		    ("tcp_read %d: other side closed connection; return %d\n", 
		   tcb - connection, tcb->usr_rcv_off));
	    tcb->usr_rcv_buf = 0;	/* application is not accepting data */
	    return tcb->usr_rcv_off;
	}
    }

    tcb->usr_rcv_buf = 0;	/* application is not accepting data */
    r = (tcb->usr_rcv_off > 0) ? tcb->usr_rcv_off : -1;
    
    DPRINTF(2, ("tcp_non_blocking_read %d -> %d\n", tcb - connection, r));
    
    return(r);
}


/* 
 * Synchronous (i.e., blocking) read of segment on the given connection.
 * Reads until sz bytes have been received or until the connection
 * is closed.
 * Tcp_read cannot return with less than sz bytes, because it advertised
 * sz bytes as part of its available receive window.
 */
int 
tcp_read(struct tcb *tcb, void *addr, size_t sz)
{
    uint16 old;
    buffer_p p, q;
    int not_copied_all;

    /* We are allowed to read from connection after we closed it for writing */
    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_FIN_WAIT2
	   ||tcb->state == TCB_ST_CLOSE_WAIT|| tcb->state == TCB_ST_TIME_WAIT,
       tcp_read: bad tcb);

    STINC(nread);

    DPRINTF(2, ("tcp_read %d: status %d addr 0x%p size %u\n", 
	   tcb - connection, tcb->state, addr, sz));

    tcb->usr_rcv_buf = addr;	/* application is ready to accept data */
    tcb->usr_rcv_off = 0;
    tcb->usr_rcv_sz = sz;

    old = tcb->rcv_wnd;

    not_copied_all = 0;
    /* First copy buffered data and release buffers. */
    for (p = tcb->buf_list, q = 0; p != 0; p = q) {
	if (p->b_n > (tcb->usr_rcv_sz - tcb->usr_rcv_off)) {
           kmemcpy(tcb->usr_rcv_buf + tcb->usr_rcv_off, p->b_current, (tcb->usr_rcv_sz - tcb->usr_rcv_off));
	   p->b_current += tcb->usr_rcv_sz - tcb->usr_rcv_off;
	   p->b_n -= tcb->usr_rcv_sz - tcb->usr_rcv_off;
	   tcb->usr_rcv_off += tcb->usr_rcv_sz - tcb->usr_rcv_off;
	   tcb->buf_list = p;
	   not_copied_all = 1;
	   break;
	} else {
	   kmemcpy(tcb->usr_rcv_buf + tcb->usr_rcv_off, p->b_current, p->b_n);
	   tcb->usr_rcv_off += p->b_n;
	   tcb->rcv_wnd += BUFSIZE;
	   q = p->b_next;
	   tcp_buf_free(p);
	}
    }
    if (!not_copied_all) {
	tcb->buf_list = 0;
	tcb->buf_list_tail = 0;
    }

    if ((old < (RCVWND_DEFAULT / 4)) && (tcb->rcv_wnd >= (RCVWND_DEFAULT / 4)))
    {
	STINC(nvack);
	tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0);
    }

    while(tcb->usr_rcv_off < sz) {
	/* Wait for data. */
	int once = (tcb->state == TCB_ST_CLOSE_WAIT || 
		    tcb->state == TCB_ST_TIME_WAIT);
	tcp_net_poll_all(once);

	if (tcb->state == TCB_ST_CLOSED) {
	    tcb_release(tcb);
	    return -1;
        }

	/* If the other side closed, then return what we have received sofar.
         * The next read will return 0. 
	 */
	if (tcb->state == TCB_ST_CLOSE_WAIT || tcb->state == TCB_ST_TIME_WAIT) {
	    DPRINTF(2, 
		    ("tcp_read %d: other side closed connection; return %d\n", 
		   tcb - connection, tcb->usr_rcv_off));

	    tcb->usr_rcv_buf = 0;	/* application is not accepting data */
	    return tcb->usr_rcv_off;
	}
    }

    tcb->usr_rcv_buf = 0;	/* application is not accepting data */

    DPRINTF(2, ("tcp_read %d -> %d\n", tcb - connection, tcb->usr_rcv_off));
    
    return(tcb->usr_rcv_off);
}


/*
 * Block until some data is received and then return the number of bytes
 * that have been received.  Higher layers can read the data with tcp_read.
 */
int
tcp_nread(struct tcb *tcb)
{
    int n;
    buffer_p p;


    DPRINTF(2, ("tcp_nread: %d\n", tcb - connection));

    /* We are allowed to read from connection after we closed it for writing */
    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_FIN_WAIT2
	   ||tcb->state == TCB_ST_CLOSE_WAIT|| tcb->state == TCB_ST_TIME_WAIT,
       tcp_read: bad tcb);

    while (tcb->buf_list == 0) {
	tcp_net_poll_all(1);

	if (tcb->state == TCB_ST_CLOSED) {
	    tcb_release(tcb);
	    return -1;
        }

	/* If the other side closed, then return what we have received sofar.
         * The next read will return 0. 
	 */
	if (tcb->state == TCB_ST_CLOSE_WAIT || tcb->state == TCB_ST_TIME_WAIT) {
	    DPRINTF(2, 
		    ("tcp_nread %d: other side closed connection; return %d\n", 
		   tcb - connection, tcb->usr_rcv_off));

	    tcb->usr_rcv_buf = 0;	/* application is not accepting data */
	    return tcb->usr_rcv_off;
	}
    }

    n = 0;
    /* Count the bytes buffered. */
    for (p = tcb->buf_list; p != 0; p = p->b_next) {
	   n += p->b_n;
    }

    return (n);
}


/* 
 * Nonblocking write.  One outstanding write at the time.
 */
int 
tcp_non_blocking_write(struct tcb *tcb, void *addr, size_t sz, 
		       void (*release)())
{
    STINC(nwrite);

    /* We are allowed to send on a connection, even if the other side closed */
    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_CLOSE_WAIT,
       tcp_non_blocking_write: bad tcb);
    
    demand((tcb->flags & TCB_FL_ASYNWRITE) == 0, tcp_non_blocking_write:
	   one at the time);

    tcb->flags |= TCB_FL_ASYNWRITE;
    tcb->usr_snd_buf = addr;
    tcb->usr_snd_off = 0;
    tcb->usr_snd_done = tcb->snd_next + sz;
    tcb->usr_snd_sz = sz;
    tcb->usr_snd_release = release;
    tcb->retries = 0;

    send_data(tcb);

    return 1;
}


/*
 * Synchronous write.
 */
int 
tcp_write(struct tcb *tcb, void *addr, size_t sz)
{
    assert(tcb->snd_next == tcb->snd_max);
    assert(tcb->snd_next == tcb->snd_una);

    /* We are allowed to send on a connection, even if the other side closed */
    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_CLOSE_WAIT,
       tcp_non_blocking_write: bad tcb);
    
    tcb->usr_snd_buf = addr;
    tcb->usr_snd_off = 0;
    tcb->usr_snd_done = tcb->snd_next + sz;
    tcb->usr_snd_sz = sz;
    tcb->retries = 0;

    send_data(tcb);

    for(;;) {
	tcp_net_poll_all(0);

	if (tcb->state == TCB_ST_CLOSED) {
	    tcb_release(tcb);
	    return -1;
	}

	if (SEQ_GEQ(tcb->snd_una, tcb->usr_snd_done)) { /* done? */
	    /* Reset timer and return bytes sent. */
	    tcb->timer_retrans = 0;
	    tcb->usr_snd_buf = 0;
	    assert(tcb->snd_next == tcb->snd_una);
	    assert(tcb->snd_next == tcb->snd_max);
	    return sz;
	}
    }
}


/*
 * Close connection: implements another three-way handshake.
 * Two cases:
 *	1. we have received a fin from the other side (CLOSE_WAIT)
 *	2. we are initiating the close and are waiting for a fin (FIN_WAIT1)
 * In both cases, we send a fin. The fin has its own sequence number.
 * Close returns whether the full close is done or only this side closed.
 *
 * Assumption: we have successfully sent all our data.
 */
int
tcp_non_blocking_close(struct tcb *tcb)
{
    int done;

    STINC(nclose);

    if (tcb->state == TCB_ST_FREE || tcb->state == TCB_ST_CLOSED)
	return(0);

    if (tcb->state == TCB_ST_LISTEN) {
	tcp_done(tcb);
	tcb_release(tcb);
	return(0);
    }

    if (!(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_CLOSE_WAIT))
    {
	printf("tcp_close: illegal state %d\n", tcb->state);
	return -1;
    }

    if (tcb->state == TCB_ST_CLOSE_WAIT) {
	DPRINTF(2, ("tcp_close %d: go to LAST_ACK\n", tcb - connection));
	tcb->state = TCB_ST_LAST_ACK;
    } else {
	DPRINTF(2, ("tcp_close %d: go to FIN_WAIT1\n", tcb - connection));
	tcb->state = TCB_ST_FIN_WAIT1;
    }
    
#ifndef NONBLOCKINGWRITE
    demand(tcb->snd_una == tcb->snd_next, tcp_close: weird);
#endif

    DPRINTF(2, ("tcp_close: close %d\n", tcb - connection));

    done = tcb->snd_next + 1;
    tcb->retries = 0;

    STINC(nfinsnd);
    tcp_net_send(tcb, TCP_FL_ACK|TCP_FL_FIN, tcb->snd_next, 0);
    tcb->snd_next++;	/* fin has a place in sequence space */
    tcb->snd_max++;
    timer_set(tcb, TCP_TIMEOUT);

#ifndef NONBLOCKINGWRITE
    demand(tcb->snd_una + 1 == tcb->snd_next, tcp_close: wrong next);
#endif

    tcp_net_poll_all(1); /* too hairy to deal */

    if (tcb->state == TCB_ST_TIME_WAIT) {

	/* if there's still data, need to continue on */
	if((tcb->usr_rcv_off > 0) || (tcb->buf_list)) {
	    tcb->timer_retrans = 0;
	    return(1);
	}

	DPRINTF(2, ("tcp_close: close %d done\n", tcb - connection));
	return(0);
    }

    if (tcb->state == TCB_ST_CLOSED) {    /* done? */
	tcb_release(tcb);
	DPRINTF(2, ("tcp_close: close %d done\n", tcb - connection));
	return(0);
    }

    if (SEQ_GEQ(tcb->snd_una, done)) {
	/* When the other side closes, we will release the 
	 * connection. 
	 */
	demand(tcb->state == TCB_ST_FIN_WAIT2, illegal state);
	DPRINTF(2, ("tcp_close: close %d done\n", tcb - connection));
	return(1);
    }

    tcb->flags |= TCB_FL_ASYNCLOSE;

    return(-1);
}



/*
 * Close connection: implements another three-way handshake.
 * Two cases:
 *	1. we have received a fin from the other side (CLOSE_WAIT)
 *	2. we are initiating the close and are waiting for a fin (FIN_WAIT1)
 * In both cases, we send a fin. The fin has its own sequence number.
 * Close returns whether the full close is done or only this side closed.
 *
 * Assumption: we have successfully sent all our data.
 */
int
tcp_close(struct tcb *tcb)
{
    int done;

    STINC(nclose);

    if (!tcb)
      {
	printf("tcp_close: NULL argument\n");
	return 0;
      }
    if (tcb->state == TCB_ST_FREE || tcb->state == TCB_ST_CLOSED)
	return(0);

    if (tcb->state == TCB_ST_LISTEN) {
	tcp_done(tcb);
	tcb_release(tcb);
	return(0);
    }

    if (!(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_CLOSE_WAIT))
    {
	printf("tcp_close: illegal state %d\n", tcb->state);
	return -1;
    }

    if (tcb->state == TCB_ST_CLOSE_WAIT) {
	DPRINTF(2, ("tcp_close %d: go to LAST_ACK\n", tcb - connection));
	tcb->state = TCB_ST_LAST_ACK;
    } else {
	DPRINTF(2, ("tcp_close %d: go to FIN_WAIT1\n", tcb - connection));
	tcb->state = TCB_ST_FIN_WAIT1;
    }
    
#ifndef NONBLOCKINGWRITE
    demand(tcb->snd_una == tcb->snd_next, tcp_close: weird);
#endif

    DPRINTF(2, ("tcp_close: close %d\n", tcb - connection));

    done = tcb->snd_next + 1;
    tcb->retries = 0;

    STINC(nfinsnd);
    tcp_net_send(tcb, TCP_FL_ACK|TCP_FL_FIN, tcb->snd_next, 0);
    tcb->snd_next++;	/* fin has a place in sequence space */
    tcb->snd_max++;
    timer_set(tcb, TCP_TIMEOUT);

#ifndef NONBLOCKINGWRITE
    demand(tcb->snd_una + 1 == tcb->snd_next, tcp_close: wrong next);
#endif

    /* Spin until data is sent or retransmission timer expires */
    for (;;) {
	tcp_net_poll_all(1); /* too hairy to deal */

	if (tcb->state == TCB_ST_TIME_WAIT) {

	    /* if there's still data, need to continue on */
	    if((tcb->usr_rcv_off > 0) || (tcb->buf_list)) {
		tcb->timer_retrans = 0;
   	        DPRINTF(2, ("tcp_close: close timewait %d done\n", 
			    tcb - connection));
		return(1);
	    }

	    DPRINTF(2, ("tcp_close: close %d done\n", tcb - connection));
	    return(0);
	}

	if (tcb->state == TCB_ST_CLOSED) {    /* done? */
	    tcb_release(tcb);
	    DPRINTF(2, ("tcp_close: close %d done\n", tcb - connection));
	    return(0);
	}

	if (SEQ_GEQ(tcb->snd_una, done)) {
	    /* When the other side closes, we will release the 
	     * connection. 
	     */
	    demand(tcb->state == TCB_ST_FIN_WAIT2, illegal state);
	    DPRINTF(2, ("tcp_close: close %d done\n", tcb - connection));
	    return(1);
	}
    }
}


int
tcp_getpeername(struct tcb *tcb, uint16 *port, unsigned char *addr)
{
    struct tcp *tcp;
    struct ip *ip;

    DPRINTF(2, ("tcp_getpeername %d\n", tcb - connection));

    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_CLOSE_WAIT,
	   wrong state);

    tcp = tcb->snd_eit.tcp;
    ip = tcb->snd_eit.ip;

    *port = tcp->dst_port;
    memcpy(addr, &ip->destination[0], 4);

    printf("port %d\n", *port);

    return 1;
}


int
tcp_getsockopt(void *handle, int level, int optname, void *optval, int *optlen)
{
    return 0;
}


int
tcp_setsockopt(void *handle, int level, int optname, const void *optval,
	       int optlen)
{
    return 0;
}



#ifndef NOSTATISTICS
/*
 * Print out statistics.
 */
void
tcp_statistics(void)
{
    int i;
    int r;

    printf("Tcp receive statistics:\n");
    printf(" # segments received                  %7d\n", tcp_stat.nseg);
    printf(" # incomplete segments                %7d\n", tcp_stat.nincomplete);
    printf(" # acceptable segments                %7d\n", tcp_stat.naccept);
    printf(" # unacceptable segments              %7d\n", tcp_stat.nunaccept);
    printf(" # acks received                      %7d\n", tcp_stat.nack);
    printf(" # acks predicted                     %7d\n", tcp_stat.nackprd);
    printf(" # duplicate acks                     %7d\n", tcp_stat.nackdup);
    printf(" # data predicted                     %7d\n", tcp_stat.ndataprd);
    printf(" # syns received                      %7d\n", tcp_stat.nsynrcv);
    printf(" # fins received                      %7d\n", tcp_stat.nfinrcv);
    printf(" # resets received                    %7d\n", tcp_stat.nrstrcv);
    printf(" # segments with data                 %7d\n", tcp_stat.ndatarcv);
    printf(" # segments in order                  %7d\n", tcp_stat.norder);
    printf(" # segments buffered                  %7d\n", tcp_stat.nbuf);
    printf(" # segments with data but no usr buf  %7d\n", tcp_stat.noutspace);
    printf(" # segments out of order              %7d\n", tcp_stat.noutorder);
    printf(" # window probes received             %7d\n", tcp_stat.nprobercv);
    printf(" # segments with bad ip cksum         %7d\n", tcp_stat.nbadipcksum);
    printf(" # segments with bad tcp cksum        %7d\n", tcp_stat.nbadtcpcksum);
    printf(" # segments dropped on receive        %7d\n", tcp_stat.ndroprcv);
    r = 1;
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", r);
	r = r << 1;
    }
    printf("\n");
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", tcp_stat.rcvdata[i]);
    }
    printf("\n");

    printf("Tcp send statistics:\n");
    printf(" # acks sent                          %7d\n", tcp_stat.nacksnd);
    printf(" # voluntary acks                     %7d\n", tcp_stat.nvack);
    printf(" # acks sent zero windows             %7d\n", tcp_stat.nzerowndsnt);
    printf(" # acks delayed                       %7d\n", tcp_stat.ndelayack);
    printf(" # fin segments sent                  %7d\n", tcp_stat.nfinsnd);
    printf(" # syn segments sent                  %7d\n", tcp_stat.nsynsnd);
    printf(" # reset segments sent                %7d\n", tcp_stat.nrstsnd);
    printf(" # segments sent with data            %7d\n", tcp_stat.ndata);
    printf(" # window probes send                 %7d\n", tcp_stat.nprobesnd);
    printf(" # ether retransmissions              %7d\n", tcp_stat.nethretrans);
    printf(" # retransmissions                    %7d\n", tcp_stat.nretrans);
    printf(" # segments dropped on send           %7d\n", tcp_stat.ndropsnd);
    r = 1;
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", r);
	r = r << 1;
    }
    printf("\n");
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", tcp_stat.snddata[i]);
    }
    printf("\n");

    printf("Tcp call statistics:\n");
    printf(" # tcp_open calls                     %7d\n", tcp_stat.nopen);
    printf(" # tcp_listen calls                   %7d\n", tcp_stat.nlisten);
    printf(" # tcp_accept calls                   %7d\n", tcp_stat.naccepts);
    printf(" # tcp_read calls                     %7d\n", tcp_stat.nread);
    printf(" # tcp_write calls                    %7d\n", tcp_stat.nwrite);
    printf(" # tcp_close calls                    %7d\n", tcp_stat.nclose);
#ifdef AN2
    print_ash_stats();
#endif
}
#else
void
tcp_statistics(void)
{
}
#endif

/*
 * Set the drop rate on outgoing packets. Rate should be in the range
 * of 0 .. 100. 100 means 100% of packets is dropped.
 */
void
tcp_set_droprate(struct tcb *tcb, int rate)
{
    printf("tcp_set_droprate to %d\n", rate);

    tcb->droprate = rate;
}

#if 0
#include "../otto/shuffle.c"
#endif
