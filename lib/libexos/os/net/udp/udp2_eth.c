
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
 * Implements a simple, hard-wired UDP/IP protocol.  Functionality is small,
 * but so is the implementation ;-)
 * 
 */
#include <stddef.h>
#include <exos/debug.h>
#include <assert.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <exos/process.h>
#include <xuser.h>
#include <exos/net/ae_net.h>

#include "exos/netinet/cksum.h"
#include "udp2_eth.h"

#define IP_PSEUDO_HDR_SIZE 12

#define UDPCKSUM
#define IPCKSUM

#define MIN(a, b)  ((a < b) ? a : b)

/* compare two ethernet addresses */
static inline int ethcmp(char p[6], char q[6]) {
	return ((p[0] == q[0]) && (p[1] == q[1]) && (p[2] == q[2]) && 
		(p[3] == q[3]) && (p[4] == q[4]) && (p[5] == q[5]));
}

/* open a udp session: returns a pointer to the resultant structure. */
void
udp2_open(struct udp2_session *s, 
	      uint16 dst_port,	uint16 src_port,
	      uint32 ip_dst_addr, uint8 *eth_dst_addr,
	      uint32 ip_src_addr, uint8 *eth_src_addr) {
    extern void *mk_udp(int *sz, int src_port, int dst_port);
    struct ip_pkt *ip;
    struct udp_pkt *udp;
    struct ether_pkt *eth;
    void *filter;
    int sz;

    s->ineth = (struct ether_pkt *) (s->inmsg);
    eth = s->outeth = (struct ether_pkt *) (s->outmsg);
    memcpy(eth->ether_dhost, eth_dst_addr, sizeof eth->ether_dhost);
    memcpy(eth->ether_shost, eth_src_addr, sizeof eth->ether_shost);

    s->netcardno = getnetcardno (eth_src_addr);
    if (s->netcardno == -1) {
       printf ("udp_open: unknown ethernet address 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", eth_src_addr[0], eth_src_addr[1], eth_src_addr[2], eth_src_addr[3], eth_src_addr[4], eth_src_addr[5]);
       assert (0);
    }

    eth->ether_type = htons(ETHERTYPE_IP);

    s->inip = (struct ip_pkt *) (s->inmsg + sizeof(*eth));
    ip = s->outip = (struct ip_pkt *) (s->outmsg + sizeof(*eth));
    ip->ip_hl = sizeof(struct ip_pkt) >> 2;
    ip->ip_v = 0x4;

    ip->ip_tos = 0;
    ip->ip_id = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 0xFF;
    ip->ip_p = IPPROTO_UDP;
    ip->ip_sum = 0;
    ip->ip_len = 0;
    ip->ip_src = ip_src_addr;
    ip->ip_dst = ip_dst_addr;

    s->inudp = (struct udp_pkt *) (s->inmsg + sizeof(*eth) + sizeof(*ip));
    udp = s->outudp = (struct udp_pkt *) (s->outmsg + sizeof(*eth) + sizeof(*ip));
    udp->udp_sport = htons(src_port);
    udp->udp_dport = htons(dst_port);
    udp->udp_sum = 0;

    s->indata = s->inmsg + sizeof(*eth) + sizeof(*ip) + sizeof(*udp);
    s->outdata = s->outmsg + sizeof(*eth) + sizeof(*ip) + sizeof(*udp);

    filter = mk_udp(&sz,  udp->udp_dport, udp->udp_sport);
	
    if(((int)(s->demux_id = sys_self_dpf_insert_old(CAP_ROOT, 0, filter, sz, 1))) < 0)
	demand(0, error!);

    s->pkt.owner = &(s->pkt.ownval);
    s->pkt.ownval = 0;
    s->pkt.recv.n = 1;
    s->pkt.recv.r[0].data = s->inmsg;
    s->pkt.recv.r[0].sz = MAXUDP;
    s->pkt.next = &(s->pkt);
    if (sys_pktring_setring (0, s->demux_id, &(s->pkt)) < 0) {
       printf ("setring failed: no soup for you!\n");
       assert (0);
    }
}    


/* close a udp session */
int 
udp2_close(struct udp2_session *s) {
	demand(0, udp_close: not implemented);
	return -1;
}



/*
 * Wait for packet.
 */
int 
udp2_read(struct udp2_session *s, void *addr, uint16 sz) {
    register struct ip_pkt *ip;
    register struct udp_pkt *udp;
    int len;
    volatile int *flag;

    flag = &(s->pkt.ownval);
    while ((* (volatile int *) flag) <= 0) ;

    ip = s->inip;
    udp = s->inudp;

    len = *flag;

    if (ip->ip_sum != 0) {
#ifdef EXOPC
	long sum = 0;

	ip_sum(sum, ip, sizeof(struct ip_pkt));
	ip->ip_sum = fold_32bit_sum(sum);
#else
	/* Check ip cksum */
	ip->ip_sum = inet_checksum((unsigned short *) ip, sizeof(struct ip_pkt), 0, 1);

#endif
	if (ip->ip_sum) {
	    printf("fast_udp_read: bad ip checksum\n");
	    return -1;
	}
    }


    if (udp->udp_sum != 0) {
	long sum = 0;

	ip->ip_len = ntohs(ip->ip_len);
	len = ip->ip_len - sizeof(struct ip_pkt);
	ip->ip_ttl = 0;
	ip->ip_sum = htons(len);

#ifdef EXOPC
	ip_sum(sum, &(ip->ip_ttl), IP_PSEUDO_HDR_SIZE + len);
	udp->udp_sum = fold_32bit_sum(sum);
#else
	udp->udp_sum = inet_checksum((unsigned short *) &(ip->ip_ttl), IP_PSEUDO_HDR_SIZE + len, 0, 1);
#endif
	if (udp->udp_sum) {
	    printf("udp_read: bad inet_checksum\n");
	    return -1;
	}
    } else {
	len = ip->ip_len - sizeof(struct ip_pkt);
    }

    sz = MIN(len, sz);
    memcpy(addr, s->indata, sz);

    *flag = 0;

    return sz;
}



/*
 * Send packet
 */
int 
udp2_write(struct udp2_session *s, void *addr, uint16 sz) {
    register struct ip_pkt *ip;
    register struct udp_pkt *udp;
    int len;

    ip = s->outip;
    udp = s->outudp;

    /* don't fragment yet */
    if(sz >= MAXUDP)
	return -1;

    memcpy(s->outdata, addr, sz);		 /* copy data */

    ip->ip_len = htons(sizeof *ip + sizeof *udp + sz);
    udp->udp_len = htons(sizeof *udp + sz);


#ifdef UDPCKSUM
    /* Fill in pseudo header */
    ip->ip_ttl = 0;
    ip->ip_sum = udp->udp_len;
    udp->udp_sum = 0;
#ifdef EXOPC
    {
	long sum = 0;
	
	ip_sum(sum, (unsigned short *) &(ip->ip_ttl), 
	       sz + IP_PSEUDO_HDR_SIZE + sizeof *udp);
	udp->udp_sum = ~fold_32bit_sum(sum);
    }
#else
    /* Compute cksum on pseudo header and tcp header */
    udp->udp_sum = inet_checksum((unsigned short *) &(ip->ip_ttl), sz + IP_PSEUDO_HDR_SIZE + sizeof *udp, 0, 1);
#endif
#endif

#ifdef IPCKSUM
    /* Repair ip header, after screwing it up to construct pseudo header. */
    ip->ip_ttl = 0xFF;

    ip->ip_sum = 0;
#ifdef EXOPC
    {
	long sum = 0;

	ip_sum(sum, ip, sizeof(struct ip_pkt));
	ip->ip_sum = ~fold_32bit_sum(sum);
    }

#else
    /* Compute cksum on ip header */
    ip->ip_sum = inet_checksum((unsigned short *) ip, sizeof(struct ip_pkt), 0, 1);
#endif
#endif

    len = sz + sizeof(struct ip_pkt) + sizeof(struct ether_pkt) + 
	sizeof(struct udp_pkt);

    /* transmit */
    while(ae_eth_send(s->outmsg, len, s->netcardno) < 0) {
	extern int retran;

	retran++;	/* and retransmit, if necessary */

	if (retran > 1000) {
	    retran = 0;
	    printf("spinning?\n");
	}
    }
    return sz;
}
