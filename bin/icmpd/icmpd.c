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

#include <xok/sys_ucall.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>
#include <exos/netinet/fast_eth.h>
#include <exos/netinet/fast_ip.h>
#include <exos/netinet/fast_icmp.h>
#include <exos/netinet/cksum.h>
#include <exos/net/if.h>
#include <exos/cap.h>
#include <exos/uwk.h>
#include <exos/net/ae_net.h>

#include <exos/net/ether.h>

typedef struct ringbuf {
  struct ringbuf *next;		
  u_int *pollptr;		/* should point to flag */
  u_int flag;			/* set by kernel to say we have a packet */
  int n;			/* should be 1 */
  int sz;			/* amount of buffer space */
  char *data;			/* should point to space */
  char space[ETHER_MAX_LEN];	/* actual storage space for packet */
} ringbuf_t, *ringbuf_p;

#define RINGSZ 16		/* how many packets we can buffer at once */
ringbuf_t pkts[RINGSZ];		/* allocate space for incoming packets */
ringbuf_p pkt;

int ringid;			/* packet ring id */

#define MAXFILTS 16
int demuxid[MAXFILTS];		/* array of filters we're using */
int last_demuxid = 0;		/* index of last filter in demuxid */

int setup_packet_rings () {
  int i;

  /* setup all the pointers in the ring */
  for(i = 0; i < RINGSZ; i++) {
    pkt = &pkts[i];
    pkt->next = &pkts[i+1];
    pkt->pollptr = &pkts[i].flag;
    pkt->n = 1;
    pkt->sz = ETHER_MAX_LEN;
    pkt->data = &pkt->space[0];
    pkt->flag = 0;
  }
  /* fix last entry */
  pkt->next = &pkts[0];

  if ((ringid = sys_pktring_setring (CAP_ROOT, 0, (struct pktringent *) pkt)) 
      <= 0) {
    return -1;
  }

  return 0;
}

int setup_packet_filter () {
  struct dpf_ir ir;
  int ifnum;
  unsigned int ip;

  /* we want a bunch of different types of packets. 

     -- First, we want any packet with ip protocol of ICMP
     -- Second, we want any ip or tcp packet not picked up by
     any other filter. This is so we can send "protocol unreachable"
     and "port unreachable" messages back. DPF gives packets
     to the longest matching filter so when a particular
     program binds a more specific filter than ours they will
     get the proper packets instead of us.

     It turns out that to do this, we just grab all ip packets sent to
     us. In the presence of multiple interfaces, we might have
     multiple ip addresses, so we insert one filter per ip address,
     all pointing to the same packet ring */

  /* XXX -- currently checks for packets on the loopback interface
     but I don't know if these packets have an ether header which
     we presume */

  init_iter_ifnum ();
  while ((ifnum = iter_ifnum (IF_UP)) != -1 && last_demuxid < MAXFILTS) {
    get_ifnum_ipaddr (ifnum, (char *)&ip);

    dpf_begin (&ir);
    dpf_eq16 (&ir, eth_offset(proto), htons(EP_IP)); /* eth proto = ip */
    dpf_meq16 (&ir, eth_sz + ip_offset(fragoffset), htons(IP_FRAGOFF), htons(0)); /* offset = 0 */
    dpf_eq32 (&ir, eth_sz + ip_offset(destination), ip);  /* ip dst = us */

    demuxid[last_demuxid] = sys_self_dpf_insert (CAP_ROOT, CAP_ROOT, &ir, ringid);
    if (demuxid[last_demuxid] < 0) {
      return -1;
    }
    last_demuxid++;

  } /* while */

  return 0;
}

int setup_network () {

  /* allocate space for incoming packets and tell the kernel
     about them */

  if (setup_packet_rings () < 0)
    return -1;

  /* allocate a packet filter and tell the kernel to put matching
     packets into the above ring */

  if (setup_packet_filter () < 0)
    return -1;

  return 0;
}
 
/* return when there's a packet for us to process */
void wait_for_packet () {

  /* poll once */
  if (pkt->flag)
    return;

  /* now sleep */
  wk_waitfor_value_neq (&pkt->flag, 0, CAP_ROOT);
}

void clean_up_network () {

  /* remove the filter */
  while (--last_demuxid >= 0) {
    if (sys_self_dpf_delete (CAP_ROOT, demuxid[last_demuxid]) < 0) {
      fprintf (stderr, "icmpd: could not remove filter on shutdown\n");
    }
  }
  
  /* remove the packet ring */
  if (sys_pktring_delring (CAP_ROOT, ringid) < 0) {
    fprintf (stderr, "icmpd: could not delete packet ring on shutdown\n");
  }
}

/* say we're done with the current packet */
void release_packet () {

  /* give the current packet back to the kernel */
  pkt->flag = 0;

  /* and get ready to process the next packet */
  pkt = pkt->next;
}

/* prepare dst to be sent in respone to src. That is, copy the ethernet
   and ip addresses and setup dst's ip header (except total length
   and checksum). returns a pointer to dst's ip header. */

struct ip *setup_packet (char *dst, char *src) {
  struct eth *dst_eth, *src_eth;
  struct ip *dst_ip, *src_ip;

  /* setup some usefull pointers */
  dst_eth = (struct eth *)dst;
  src_eth = (struct eth *)src;
  dst_ip = (struct ip *)((char *)dst_eth + eth_sz);
  src_ip = (struct ip *)((char *)src_eth + eth_sz);

  /* fill in destination ethernet address */
  /* don't have to fill in the dst eth addr since our caller will lookup
     the proper routing info and do that for us */
  memcpy (dst_eth->src_addr, src_eth->dst_addr, sizeof (src_eth->src_addr));
  dst_eth->proto = htons (EP_IP);
  
  /* fill in the destination ip address */
  dst_ip->vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);  
  dst_ip->tos = 0;
  dst_ip->identification = 1;
  dst_ip->fragoffset = 0;
  dst_ip->ttl = 0xff;
  dst_ip->protocol = IP_PROTO_ICMP;
  dst_ip->source[0] = src_ip->destination[0];
  dst_ip->source[1] = src_ip->destination[1];
  dst_ip->destination[0] = src_ip->source[0];
  dst_ip->destination[1] = src_ip->source[1];

  return ((struct ip *)dst_ip);
}

/* pass in pointer to ip header and length of user data beyond the
   ip header and update the ip header */

void finish_packet (struct ip *ip, int icmp_len) {
  ip->totallength = htons (icmp_len + ip_sz);
  ip->checksum = 0;
  ip->checksum = inet_checksum((uint16 *) ip, sizeof(struct ip), 0, 1);
}

void send_packet (char *data, int len, int ifnum, ethaddr_t dst) {
  struct eth *eth;
  int retrans;

  eth = (struct eth *)data;
  memcpy (eth->dst_addr, dst, sizeof (ethaddr_t));

  for (retrans = 0; retrans < 20; retrans++) {
    if (ae_eth_send (data, len, get_ifnum_cardno (ifnum)) >= 0) {
      break;
    }
  }
  if (retrans == 20) {
    fprintf (stderr, "icmpd: time-out sending packet\n");
  }
}

/* build a reply in packet to the echo request in pkt->data */

int send_echo_reply_packet (char *packet) {
  struct ip *src_ip, *dst_ip;
  struct icmp_echo *icmp;
  struct eth *src_eth, *dst_eth;

  src_ip = (struct ip *)(pkt->data+eth_sz);
  dst_ip = (struct ip *)(packet+eth_sz);
  dst_eth = (struct eth *)(packet);
  src_eth = (struct eth *)(pkt->data);
  icmp = (struct icmp_echo *)(packet+eth_sz+ip_sz);

  /* drop any fragmented packets */
  if (ntohs (src_ip->fragoffset)) {
    fprintf (stderr, "icmpd: dropping fragmented echo request\n");
    return 0;
  }

  /* an echo request is just a copy of the incoming data with a few
     fields changed */

  memcpy (packet, pkt->data, ntohs (src_ip->totallength)+eth_sz);
  memcpy (dst_eth->src_addr, src_eth->dst_addr, sizeof (src_eth->dst_addr));
  memcpy (dst_ip->destination, src_ip->source, sizeof (src_ip->source));
  memcpy (dst_ip->source, src_ip->destination, sizeof (src_ip->destination));
  icmp->type = ICMP_ECHO_REPLY;
  icmp->checksum = 0;
  icmp->checksum = inet_checksum ((uint16 *)icmp, 
				  ntohs (src_ip->totallength)+eth_sz, 0, 1);

  return (ntohs (src_ip->totallength)+eth_sz);
}

int process_icmp_packet (char *packet) {
  struct icmp_generic *icmp_in;
  int len;

  icmp_in = (struct icmp_generic *)(pkt->data+eth_sz+ip_sz);

  switch (icmp_in->type) {
  case ICMP_ECHO: len = send_echo_reply_packet (packet); break;
  default: fprintf (stderr, "icmpd: bogus ICMP packet. dropping it.\n"); 
    len = 0; break;
  }

  return len;
}

int process_port_unreachable (char *packet) {
  struct icmp_unreach *icmp;
  struct ip *dst_ip, *src_ip;

  /* pass in new packet and incoming packet and setup the 
     new packet's eth and ip headers */

  dst_ip = setup_packet (packet, pkt->data);

  /* setup some other usefull pointers */

  icmp = (struct icmp_unreach *)((char *)dst_ip + ip_sz);
  src_ip = (struct ip *)((char *)pkt->data+eth_sz);
  
  /* and actually fill in this packet */

  icmp->type = ICMP_DST_UNREACHABLE;
  icmp->code = ICMP_CODE_PORTUNREACH;
  icmp->unused = 0;
  icmp->ip = *src_ip;

  /* copy first part of incoming packet back out */
  memcpy (icmp->data, pkt->data+eth_sz+ip_sz, sizeof (icmp->data));

  icmp->checksum = 0;
  icmp->checksum = inet_checksum ((uint16 *)icmp, sizeof (struct icmp_unreach),
				  0, 1);

  /* set ip length and compute ip checksum */
  finish_packet (dst_ip, sizeof (struct icmp_unreach));

  return (sizeof (struct icmp_unreach) + eth_sz + ip_sz);
}

int process_protocol_unreachable (char *packet) {
kprintf ("got protocol unreachable packet\n");
return 0; 
}

int main (int argc, char *argv[]) {
  struct ip *ip;
  char packet[256];		/* better be bigger than max icmp packet */
  int len;
  int ifnum;
  ethaddr_t dsteth;

  /* setup buffering and packet filters */
  if (setup_network () < 0) {
    fprintf (stderr, "icmpd: could not setup network. exiting.\n");
    exit (1);
  }

  /* clean up network filters and buffers when we exit */
  atexit (clean_up_network);

  /* main server loop. wait for a packet and process it. */

  while (1) {
    wait_for_packet ();

    /* cast the packet data to an ip header struct */

    ip = (struct ip *)((char *)pkt->data + eth_sz);

    /* lookup how to route our response back to the source */
    assert (get_dsteth ((unsigned char *)ip->source, dsteth, &ifnum) == 0);

    /* and demux what type of packet this is */

    switch (ip->protocol) {
    case IP_PROTO_ICMP: len = process_icmp_packet (packet); break;
    case IP_PROTO_TCP:
    case IP_PROTO_UDP: len = process_port_unreachable (packet); break;
    default: len = process_protocol_unreachable (packet); break;
    }

    /* we're done with the incoming packet */
    release_packet ();

    /* send the outgoing packet if there is one */
    if (len) {
      send_packet (packet, len, ifnum, dsteth);
    }

  } /* while */

  exit (0);
}
