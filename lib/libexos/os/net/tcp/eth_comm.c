
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
 * Routines to send and receive packets over Aegis's ethernet interface.
 *
 * Frans Kaashoek
 *
 * 11/29/95
 */

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef EXOPC
#include <exos/process.h>
#include <xuser.h>
#include <exos/net/ae_net.h>
#endif
#include "tcp.h"
#include <exos/debug.h>
#include "global.h"
#include "exos/netinet/cksum.h"
#include "timer.h"

#define TCPCKSUM
#define IPCKSUM

extern struct tcb connection[];

#ifdef TESTDROP
static int last_sent;
#endif

void
tcp_net_init(void)
{
    ae_eth_init();
}


/* 
 * Init tcb. Done once per tcp library invocation!
 */
void
tcb_net_tcb_init(struct tcb *tcb)
{
    struct tcp *tcp;
    struct ip *ip;
    struct eth *eth;
    char *d;

    /* Set up pointers to headers in snd_recv; worry about alignment */
    d = malloc(DEFAULT_MSG + START_ALIGN);
    demand(d != 0, tcp_open: malloc failed);
    memset(d, 0, ETHER_ALIGN+sizeof(struct eth) + sizeof(struct ip) +
	   sizeof(struct tcp));  /* unfortunately, some headers aren't initialized */

    tcb->snd_recv.n = 1;
    tcb->snd_recv.r[0].sz = DEFAULT_MSG;
    tcb->snd_recv.r[0].data = &d[ETHER_ALIGN];

    tcb->snd_eit.msg = &d[ETHER_ALIGN];
    tcb->snd_eit.eth = (struct eth *) tcb->snd_eit.msg;
    tcb->snd_eit.ip = (struct ip *) (tcb->snd_eit.msg + sizeof(struct eth)); 
    tcb->snd_eit.tcp = (struct tcp *) (tcb->snd_eit.msg + sizeof(struct eth) +
			  sizeof(struct ip));
    tcb->snd_eit.data = tcb->snd_eit.msg + sizeof(struct eth) + 
	sizeof(struct ip) + sizeof(struct tcp);

    /* Ethernet header initialization. */
    eth = tcb->snd_eit.eth;
    eth->proto = htons(EP_IP);

    ip = tcb->snd_eit.ip;
    tcp = tcb->snd_eit.tcp;

    /* IP initialization */
    ip->vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);
    ip->tos = 0;		/* we should allow other values */
    ip->identification = 0;
    ip->fragoffset = 0;
    ip->ttl = 0;		/* set to zero, pseudo hdr value */
    ip->protocol = IP_PROTO_TCP;
    ip->checksum = 0;
    ip->totallength = 0;

    /* TCP initialization */
    tcp->seqno = 0;
    tcp->ackno = 0;
    tcp->offset = (sizeof(struct tcp) >> 2) << 4;
    tcp->flags = 0;
    tcp->window = 0;
    tcp->urgentptr = 0;

    tcb->demux_id = -1;
    tcb->rcv_queue.head = 0;
    tcb->rcv_queue.tail = 0;
    
}


/* 
 * Init network. 
 */
void
tcp_net_tcb_init(struct tcb *tcb)
{
    void *mk_tcp(int *sz, int src_port, int dst_port, short *src_ip);
    void *filter;
    int len;
    struct tcp *tcp;
    struct ip *ip;
    struct eth *eth;
    struct ae_eth_pkt *pkt;
    int i;
    int n;


    /* Ethernet header initialization. */
    eth = tcb->snd_eit.eth;
    memcpy(eth->dst_addr, tcb->eth_dst, sizeof eth->dst_addr);
    memcpy(eth->src_addr, tcb->eth_src, sizeof eth->src_addr);

    /* IP initialization */
    ip = tcb->snd_eit.ip;
    memcpy(&ip->destination[0], &tcb->ip_dst, sizeof tcb->ip_dst);
    memcpy(&ip->source[0], &tcb->ip_src, sizeof tcb->ip_src);

    /* TCP initialization */
    tcp = tcb->snd_eit.tcp;
    tcp->src_port = htons(tcb->port_src);
    tcp->dst_port = htons(tcb->port_dst);
    tcp->seqno = 0;
    tcp->ackno = 0;
    tcp->offset = (sizeof(struct tcp) >> 2) << 4;
    tcp->flags = 0;
    tcp->window = 0;
    tcp->urgentptr = 0;

    /* For receiving we should check is dst_port is in the source port field
     * source and destination need to be network order.
     */

    filter = mk_tcp(&len, tcp->dst_port, tcp->src_port, &ip->destination[0]);

    if(((int)(tcb->demux_id = sys_self_dpf_insert_old(CAP_ROOT, 0, filter, len, 0))) < 0)
	demand(0, tcp_open: unable to insert filter!);
    else
	DPRINTF(2, ("tcp_open %d: got filter %d\n", tcb - connection,
		    tcb->demux_id));

    n = MIN(ae_eth_nr_free_pkt(), N_PER_CONNECTION);
    for (i = 0; i < n; i++) {
	pkt = ae_eth_get_pkt();
	assert(pkt);
	pkt->flag = -1;
#ifdef EXOPC
	ae_eth_poll(tcb->demux_id, pkt);
#endif
#ifdef AEGIS
	len = ae_eth_poll(tcb->demux_id, &(pkt->recv), &(pkt->flag));
	demand(len <= 0, "packet is too early");
#endif
	ae_append_pkt(&tcb->rcv_queue, pkt);
    }
}


void
tcp_net_close(struct tcb *tcb)
{
    struct ae_eth_pkt *p;
    struct ae_eth_pkt *n;

    /* delete filter */
    sys_self_dpf_delete(0, tcb->demux_id);

    /* tell network interface we are done */
    ae_eth_close(tcb->demux_id);    

    /* release packets owned by library */
    for (p = tcb->rcv_queue.head; p != 0; p = n) {
	n = p->next;
	ae_eth_release_pkt(p);
    }
    tcb->rcv_queue.head = 0;
    tcb->rcv_queue.tail = 0;

    tcb->demux_id = -1;
}



/*
 * Send a segment with options.
 * We also ack what we have received from the other side by sending the
 * sequence number we expect.
 * We can avoid the copy by using a recv structure with 2 ptrs: (1) header
 * (2) user data.
 */
void 
tcp_net_send_option(struct tcb *tcb, int flag, uint32 seqno, char *o, int olen, 
	     int len)
{
    struct eth *eth;
    struct ip *ip;
    struct tcp *tcp;
    char *data;
    int tcp_len;
    int l;

    eth = tcb->snd_eit.eth;
    l = sizeof(struct eth) + sizeof(struct ip) + sizeof(struct tcp);
    ip = tcb->snd_eit.ip;
    tcp = tcb->snd_eit.tcp;
    data = (char *) (tcp + 1);
    
    /* copy options */
    if (olen > 0) {
	memcpy(data, o, olen);
	data += olen;
    }
    
     /* copy user data */
    if (len > 0) memcpy(data, tcb->usr_snd_buf + tcb->usr_snd_off, len);

    DPRINTF(2, ("tcp_net_send_option %d: seqno %u ackno %u wnd %u\n", 
		tcb - connection, tcb->snd_next, tcb->rcv_next, tcb->rcv_wnd));

    tcp_len = sizeof(struct tcp) + olen;

    tcb->flags &= ~TCB_FL_DELACK;

    tcp->seqno = seqno;
    tcp->ackno = tcb->rcv_next;
    tcp->window = tcb->rcv_wnd;
    tcp->flags = flag;
    tcp->offset = (tcp_len >> 2) << 4;

#ifdef LOGGING
    log_insert(tcb, tcp, olen + len, 's');
#endif

    tcp->seqno = htonl(seqno);
    tcp->ackno = htonl(tcb->rcv_next);
    tcp->window = htons(tcb->rcv_wnd);

    tcp->cksum = 0;
#ifdef TCPCKSUM
    /* Fill in pseudo header */
    ip->ttl = 0;
    ip->checksum = htons(tcp_len + len);

    /* Compute cksum on pseudo header and tcp header */
#ifdef EXOPC
    {
	long sum = 0;

	ip_sum(sum, &(ip->ttl), tcp_len + IP_PSEUDO_HDR_SIZE + len);
	tcp->cksum = ~fold_32bit_sum(sum);
    }
    if (tcp->cksum == 0) tcp->cksum = 0xFFFF;
#else
    tcp->cksum = inet_checksum((unsigned short *) &(ip->ttl), 
				 tcp_len + IP_PSEUDO_HDR_SIZE + len, 0, 1);
#endif
#endif

    /* Repair ip header, after screwing it up to construct pseudo header. */
    ip->ttl = TCP_TTL;
    ip->totallength = htons(sizeof(struct ip) + tcp_len + len);

    ip->checksum = 0;
    /* Compute cksum on ip header */
#ifdef IPCKSUM
#ifdef EXOPC
    {
	long sum = 0;

	ip_sum(sum, &(ip->vrsihl), sizeof(struct ip));
	ip->checksum = ~fold_32bit_sum(sum);
    }
    if (ip->checksum == 0) ip->checksum = 0xFFFF;
#else
    ip->checksum = inet_checksum((unsigned short *) &(ip->vrsihl), 
				   sizeof(struct ip), 0, 1);
#endif
#endif

#ifdef TESTDROP
    if (last_sent) {
	STINC(ndropsnd);
	last_sent = 0;
	DPRINTF(2, ("tcp_send_net_option: drop it\n"));
	return;
    } else last_sent = 1;
#endif
    
    tcb->snd_recv.r[0].sz = l + olen + len;

    while(ae_eth_send(tcb->snd_recv.r[0].data, tcb->snd_recv.r[0].sz, tcb->netcardno) < 0) {
	int r = 0;
	    
	r++;
	if (r > 10000) {
	   DPRINTF(2, ("looping?\n"));
	   r = 0;
	}
	STINC(nethretrans);
   }
}


/*
 * Send a segment with options.
 * We also ack what we have received from the other side by sending the
 * sequence number we expect.
 * We can avoid the copy by using a recv structure with 2 ptrs: (1) header
 * (2) user data.
 */
void 
tcp_net_send(struct tcb *tcb, int flag, uint32 seqno, int len)
{
    struct eth *eth;
    struct ip *ip;
    struct tcp *tcp;

    eth = tcb->snd_eit.eth;
    ip = tcb->snd_eit.ip;
    tcp = tcb->snd_eit.tcp;
    
     /* copy user data */
    if (len > 0) 
	memcpy(tcb->snd_eit.data, tcb->usr_snd_buf + tcb->usr_snd_off, len);

    DPRINTF(2, ("tcp_net_send %d (%d): seqno %u ackno %u wnd %u\n", 
		tcb - connection, len,
		tcb->snd_next, tcb->rcv_next, tcb->rcv_wnd));

    tcp->flags = flag;
    tcp->offset = (sizeof(struct tcp) >> 2) << 4;


#ifdef LOGGING
    tcp->seqno = seqno;
    tcp->ackno = tcb->rcv_next;
    tcp->window = tcb->rcv_wnd;
    log_insert(tcb, tcp, len, 's');
#endif

    tcb->flags &= ~TCB_FL_DELACK;

    tcp->seqno = htonl(seqno);
    tcp->ackno = htonl(tcb->rcv_next);
    tcp->window = htons(tcb->rcv_wnd);

    tcp->cksum = 0;
#ifdef TCPCKSUM
    /* Fill in pseudo header */
    ip->ttl = 0;
    ip->checksum = htons(sizeof(struct tcp) + len);

    /* Compute cksum on pseudo header and tcp header */
#ifdef EXOPC
    {
	long sum = 0;

	ip_sum(sum, &(ip->ttl), sizeof(struct tcp) + IP_PSEUDO_HDR_SIZE + len);
	tcp->cksum = ~fold_32bit_sum(sum);
    }
    if (tcp->cksum == 0) tcp->cksum = 0xFFFF;
#else
    tcp->cksum = inet_checksum((unsigned short *) &(ip->ttl), 
			  sizeof(struct tcp) + IP_PSEUDO_HDR_SIZE + len, 0, 1);
#endif
#endif

    /* Repair ip header, after screwing it up to construct pseudo header. */
    ip->ttl = TCP_TTL;
    ip->totallength = htons(sizeof(struct ip) + sizeof(struct tcp) + len);

    ip->checksum = 0;

    /* Compute cksum on ip header */
#ifdef IPCKSUM
#ifdef EXOPC
    {
	long sum = 0;

	ip_sum(sum, &(ip->vrsihl), sizeof(struct ip));
	ip->checksum = ~fold_32bit_sum(sum);
    }
    if (ip->checksum == 0) ip->checksum = 0xFFFF;
#else
    ip->checksum = inet_checksum((unsigned short *) &(ip->vrsihl), 
				   sizeof(struct ip), 0, 1);
#endif
#endif

#ifdef TESTDROP
    if (last_sent) {
	STINC(ndropsnd);
	last_sent = 0;
	DPRINTF(2, ("tcp_net_send: drop it\n"));
	return;
    } else last_sent = 1;
#endif
    
    tcb->snd_recv.r[0].sz = sizeof(struct eth) + sizeof(struct ip) + 
	sizeof(struct tcp) + len;

    while(ae_eth_send(tcb->snd_recv.r[0].data, tcb->snd_recv.r[0].sz, tcb->netcardno) < 0) {
	int r = 0;
	    
	r++;
	if (r > 10000) {
	   DPRINTF(2, ("looping?\n"));
	   r = 0;
	}
	STINC(nethretrans);
   }
}


void 
tcp_net_send_dst(struct tcb *tcb, int flag, uint32 seqno, int len,
		 uint16 port_dst, uint32 ip_dst, uint8 *eth_dst)
{
    struct tcp *tcp;
    struct eth *eth;
    struct ip *ip;

    eth = tcb->snd_eit.eth;
    ip = tcb->snd_eit.ip;
    tcp = tcb->snd_eit.tcp;

    memcpy(tcb->eth_dst, eth_dst, sizeof tcb->eth_dst);
    tcb->ip_dst = ip_dst;
    tcb->port_dst = port_dst;

    memcpy(eth->dst_addr, tcb->eth_dst, sizeof eth->dst_addr);
    memcpy(&ip->destination[0], &tcb->ip_dst, sizeof tcb->ip_dst);
    memcpy(&ip->source[0], &tcb->ip_src, sizeof tcb->ip_src);

    tcp->dst_port = htons(tcb->port_dst);

    tcp_net_send(tcb, flag, seqno, len);
}


/*
 * Poll all the connection.
 */
void
tcp_net_poll_all(int once)
{
    struct tcb *tcb;
    struct ae_eth_pkt *pkt;
    int len;
    volatile int *flag;
    int last_iter = 0;

    last_iter = once;

    for (tcb = &connection[0]; tcb < &connection[MAX_CONNECTION]; tcb++) {

	if (tcb->rcv_queue.head == 0) continue;

	flag = &(tcb->rcv_queue.head->flag);

	while ((* (volatile int *) flag) > 0) {
	    /* Process all the packets on this connection */

	    pkt = ae_dequeue_pkt(&tcb->rcv_queue);
	    len = pkt->flag;

	    for (;;) {
		tcp_process(tcb, &(pkt->recv), len);

		/* Tcp_process may cleaned up this connection, so only
		 * wait for new pkts when the connection is still open.
		 */
		if (tcb->demux_id < 0) {
		    ae_eth_release_pkt(pkt);
		    return;
		}

		pkt->flag = -1;
#ifdef EXOPC
		len = ae_eth_poll(tcb->demux_id, pkt);
#endif
#ifdef AEGIS
		len = ae_eth_poll(tcb->demux_id, &(pkt->recv), &(pkt->flag));
#endif
		assert(len >= 0);

		if (len == 0) {
		    ae_append_pkt(&tcb->rcv_queue, pkt);
		    break;
		}
	    }
	    flag = &(tcb->rcv_queue.head->flag);

	    last_iter = 1;
	}

	timer_check(tcb);
    }

    if (!last_iter) yield(-1);
}


void
net_copy_user_data(struct tcb *tcb, char *data, int datalen)
{
    int fulllen;
    buffer_p p;

    DPRINTF(2, ("net_copy_user_data: copy %d bytes\n", datalen));

    fulllen = datalen;
    if (tcb->usr_rcv_buf) {
	datalen = MIN(datalen, tcb->usr_rcv_sz - tcb->usr_rcv_off);
	memcpy(tcb->usr_rcv_buf+tcb->usr_rcv_off, data, datalen);
	tcb->usr_rcv_off += datalen;
    } else {
	/* Out of space to receive  */
	datalen = 0;
    }

    if (fulllen > datalen) {
	DPRINTF(2, ("process_data: buffer %d bytes\n", fulllen - datalen));
	STINC(nbuf);
	p = tcp_buf_get();
	demand(fulllen - datalen <= BUFSIZE, 
	   net_copy_user_data: data illegal length);
	memcpy(p->b_data, data + datalen, fulllen - datalen);
	p->b_current = p->b_data;
	p->b_n = fulllen - datalen;
	tcb->rcv_wnd -= BUFSIZE;

	/* Hook buffer into the tcb buf list. */
	if (tcb->buf_list_tail) {
	    ((buffer_p)(tcb->buf_list_tail))->b_next = p;
	}
	else {
	    tcb->buf_list = p;
	}
	tcb->buf_list_tail = p;
	p->b_next = 0;
    }

    tcb->rcv_next += fulllen;
}


/***********************************************************************/

#ifdef TIMING
    
#define MAXHIST 25
struct range {
    int type;
    int low;
    int high;
    int hits[1000];
} ranget [MAXHIST];

int last = 0;

void tdeal(unsigned time, int type)
{
    int i;
    for(i=0; i<last; i++) {
	if(ranget[i].type == type) {
	    if((time < ranget[i].high) && (time >=ranget[i].low)) {
		ranget[i].hits[time-ranget[i].low]++;
		return;
	    }
	}
    }
    printf("%d: %d cs\n", type, time);
}

void itinit(int type, int low, int high)
{
    int i;
    if(last >= MAXHIST) { printf("too many timing request!!!!!!!\n"); }
    ranget[last].low = low;
    ranget[last].high = high;
    ranget[last].type = type;
    for(i=0; i<high-low; i++) {
	ranget[last].hits[i] = 0;
    }
    last++;
}
    

void tinit()
{
    int i;
    for(i=0; i<MAXHIST; i++) {
	ranget[i].type = 0;
    }
#ifdef TIMING
    itinit(1, 0, 450);
    itinit(2, 0, 1000);

    itinit(3, 0, 1000);
    itinit(4, 0, 1000);
    itinit(5, 0, 1000);
    itinit(6, 0, 1000);
    itinit(7, 0, 1000);

    itinit(8, 0, 1000);
    itinit(9, 0, 1000);
    itinit(10, 0, 1000);

    itinit(11, 0, 1000);
    itinit(12, 0, 1000);
    itinit(13, 0, 1000);
#endif
#ifdef TIMING
    itinit(14, 0, 1000);
    itinit(15, 500, 1500);
    itinit(16, 0, 1000);
    itinit(17, 0, 1000);
#endif
    itinit(20, 500, 1500);
    itinit(21, 0, 1000);
}

void itprint(int type, int hits[], int low, int high)
{
    int i, times, sum, ahigh;
    int times0;
    int start = 0;
    while(start<high-low) {
	for(i=start, sum=0, times=0, times0=0; i<high-low; i++) {
	    if(hits[i] != 0) {
		times0 = 0;
		sum += ((i+low)*hits[i]);
		times += hits[i];
	    }
	    else {
		times0++;
	    }
	    if(times0 == 15) break;
	}
#define min(a,b) ((a<b)?(a):(b))
	ahigh = min(i, high-low);
	if(times > 0) {
	    printf("%d <%d,%d>: %d/%d cs ", type, start+low, ahigh+low, sum, times);
	    if (times != 0)
		printf("    avg %d", sum/times);
	    printf("\n");
	}
	start = ahigh;
    }
}

void tprint()
{
    int i;
    for(i=0; i<last; i++) {
	itprint(ranget[i].type, ranget[i].hits, ranget[i].low, ranget[i].high);
    }
}
#else
void tprint(void) {}
#endif
/***********************************************************************/
