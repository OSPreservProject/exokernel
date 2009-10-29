
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

#ifndef __VOS_FAST_UDP_H__
#define __VOS_FAST_UDP_H__

#include <vos/net/fast_ip.h>	/* includes types */
#include <vos/net/fast_eth.h>


#define IP_PSEUDO_HDR_SIZE 12

#define UDP_TTL	64

/* UDP header, see RFC 768 */
struct udp 
{
  uint16 src_port;
  uint16 dst_port;
  uint16 length;
  uint16 cksum;
};

#define MAX_GATHER 10

/* the fast ETH IP UDP header --- horizonal encoding. */
struct eiu 
{
  struct eth eth;		/* ethernet header */
  struct ip ip;           /* this udp is carried on ip */
  struct udp udp;         /* standard udp */
  uint8 data[0];           /* struct hack */
};

#define MAXUDP	1514


struct udp_session 
{
  /* recv structure, pre-initialized to udp/ip header */
  struct ae_recv recv[MAX_GATHER+1];	/* maximum number of data elements
					 * that we are willing to gather plus
					 * room for the header. */
  int demux_id;/* filter id for filter that is supposed to demux this
		* protocol (used for deletion, and address returned?) */
  void *buffer; /* a pointer to buffering system */

  /* the pre-initialized header */
  struct eiu eiu;
  uint8 msg[MAXUDP]; /* (actually data) */
};


void udp_open(struct udp_session *s, uint16 dst_port,  uint16 src_port,
              uint32 ip_dst_addr, uint8 *eth_dst_addr,
              uint32 ip_src_addr, uint8 *eth_src_addr);
int udp_close(struct udp_session *s);
int udp_read(struct udp_session *s, void *addr, uint16 sz);
int udp_readv(struct udp_session *s, struct ae_recv *recv);
int udp_write(struct udp_session *s, void *addr, uint16 sz);
int udp_writev(struct udp_session *s, struct ae_recv *recv);


#endif /* __VOS_FAST_UDP_H__ */

