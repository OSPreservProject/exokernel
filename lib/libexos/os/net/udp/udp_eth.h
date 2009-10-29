
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

#ifndef _UDP_ETH_H_
#define _UDP_ETH_H


#include <exos/net/ether.h>
#include <exos/netinet/fast_eth.h>

#include "os/net/ip.h"

#define MAXUDP  1024

struct udp_session {
    struct ae_eth_pkt *pkt;
    int demux_id;
    int netcardno;
    void *buffer;
    struct ether_pkt *outeth;   /* ethernet header */
    struct ip_pkt *outip;       /* ip header */
    struct udp_pkt *outudp;     /* udp header */
    char *outdata;              /* user data */
    uint8 outmsg[MAXUDP];       
};

void udp_open(struct udp_session *s, 
                          uint16 dst_port,  uint16 src_port,
                          uint32 ip_dst_addr, uint8 *eth_dst_addr,
                          uint32 ip_src_addr, uint8 *eth_src_addr);

int udp_close(struct udp_session *s);
int udp_read(struct udp_session *s, void *addr, uint16 sz);
int udp_write(struct udp_session *s, void *addr, uint16 sz);

#endif /* _UDP_ETH_H_ */
