
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


#ifndef __XIO_TCP_HANDLERS_H__
#define __XIO_TCP_HANDLERS_H__

#include "xio_tcp.h"

/* prototypes */

unsigned int xio_tcp_read_clock ();
void xio_update_rtt (struct tcb *tcb, unsigned int seqno);
void xio_tcp_inittcb (struct tcb *tcb);
void xio_tcp_resettcb (struct tcb *tcb);
void xio_tcp_copytcb (struct tcb *oldtcb, struct tcb *newtcb);
void xio_tcp_initlistentcb (struct listentcb *listentcb, uint32 ipaddr,
			    int portno);

void xio_tcp_clobbercksum (char *packet, int len);
void xio_tcp_initiate_close (struct tcb *tcb);
int xio_tcp_timeout (struct tcb *tcb);

/* Recv flags */
#define TCP_RECV_ADJUSTRCVWND	1

/* Send flags */
#define TCP_SEND_ALSOCLOSE	1
#define TCP_SEND_MAXSIZEONLY	2

int xio_tcp_prepDataPacket (struct tcb *tcb, char *buf, int len, char *buf2, int len2, uint flags, uint sum);
void xio_tcp_prepCtlPacket (struct tcb *tcb);

int xio_tcp_packetcheck (char *packetdata, int len, int *datasum);
void xio_tcp_handlePacket (struct tcb *tcb, char *packet, uint flags);
int xio_tcp_handleListenPacket (struct tcb *new_tcb, char *packet);
void xio_tcp_handleRandPacket (struct tcb *tmptcb, char *packet);

void xio_tcp_bindsrc (struct tcb *tcb, int portno, char *ip_addr, char *eth_addr);
void xio_tcp_initiate_connect (struct tcb *tcb, int srcportno, int dstportno, char *ip_addr, char *eth_addr);

#endif  /* __XIO_TCP_HANDLERS_H__ */

