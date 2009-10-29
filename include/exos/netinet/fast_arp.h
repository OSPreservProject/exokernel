
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

#ifndef _FAST_ARP_H_
#define _FAST_ARP_H_

#include <exos/netinet/fast_eth.h>

struct arp {
  uint16		arp_hrd;	/* 1 for Ethernet */
  uint16		arp_pro;	/* EP_IP */
  uint8			arp_hln;	/* 6 for Ethernet */
  uint8			arp_pln;	/* 4 for IP */
  uint16		arp_op;	/* see below */
  unsigned char		arp_sha[6];
  unsigned char		arp_spa[4];
  unsigned char		arp_tha[6];
  unsigned char		arp_tpa[4];
};

/* UDP header, see RFC 768 */
struct arprarp {
  struct eth eth;
  struct arp arp;
};

#define ARP_REQ	1	/* ARP request */
#define ARP_REP	2	/* ARP reply */
#define RARP_REVREQ	3	/* Reverse request */
#define RARP_REVREP	4	/* Reverse reply */

#define RARP_PACKET_LENGTH sizeof(struct arprarp)
#define ARP_PACKET_LENGTH sizeof(struct arprarp)



int arp_init(void);

void *mk_arp_getall(int *sz);
void *mk_arp_getreplies(int *sz);
/* if ipaddr == NULL, gets all the requests */
void *mk_arp_getrequests(int *sz, unsigned char *ipaddr);
char *mk_arp_string(int *sz, unsigned char *ipaddr);

#endif /* _FAST_ARP_H_ */
