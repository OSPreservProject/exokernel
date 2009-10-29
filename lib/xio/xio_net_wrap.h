
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

/* The interface defined by this header file is intended to generale  */
/* an application's view to a given OS's app-level network interface. */
/* One can then have many difference instances of a foo_net_wrap.c    */
/* implementation of this interface, depending on the host system.    */

#ifndef __XIO_NET_WRAP_H__
#define __XIO_NET_WRAP_H__

#include <exos/net/ether.h>
#include <xok/ae_recv.h>

/* xio_net_wrap buffers */
typedef struct xio_net_buf {
   struct xio_net_buf *next;
   int *pollptr;
   volatile int poll;
   int n;
   int sz;
   char *data;
   char space[ETHER_MAX_LEN + 2];
} xio_net_buf_t;

#define recvtopollbuf(packet)   (xio_net_buf_t *)((char *)packet - sizeof(xio_net_buf_t *) - sizeof(int *) - sizeof(int))

/* potentially non-local info for xio_net_wrap.c functionality */ 
typedef struct xio_net_wrap_info {
   xio_net_buf_t *pollbuflist;
   xio_net_buf_t *pollbuftail;       
   xio_net_buf_t *extrapollbufs;
   int ringid;
} xio_nwinfo_t;


extern int inpackets, outpackets;


/* prototypes for foo_net_wrap.c */

int xio_net_wrap_init (xio_nwinfo_t *nwinfo, char *bufferSpace, uint size);
void xio_net_wrap_giveBufferSpace (xio_nwinfo_t *nwinfo, char *bufferSpace, uint size);
void xio_net_wrap_shutdown (xio_nwinfo_t *nwinfo);
void xio_net_wrap_setdroprate (xio_nwinfo_t *nwinfo, int dropperc);

int xio_net_wrap_getnetcardno (xio_nwinfo_t *nwinfo, char *eth_addr);
int xio_net_wrap_send (int netcardno, struct ae_recv *send_recv, int copysz);

int xio_net_wrap_checkforPacket (xio_nwinfo_t *nwinfo);
void xio_net_wrap_printPacket (xio_nwinfo_t *nwinfo);
struct ae_recv * xio_net_wrap_getPacket (xio_nwinfo_t *nwinfo);
void xio_net_wrap_keepPacket (xio_nwinfo_t *nwinfo, struct ae_recv *packet);
void xio_net_wrap_returnPacket (xio_nwinfo_t *nwinfo, struct ae_recv *packet);

#define XIO_WAITFORPACKET_PREDLEN 7
struct wk_term;
int xio_net_wrap_wkpred_packetarrival (xio_nwinfo_t *nwinfo, struct wk_term *t);
void xio_net_wrap_waitforPacket (xio_nwinfo_t *nwinfo);


int xio_net_wrap_getdpf_udp (xio_nwinfo_t *nwinfo, int srcportno, int dstportno);
int xio_net_wrap_getdpf_tcp (xio_nwinfo_t *nwinfo, int srcportno, int dstportno);
int xio_net_wrap_getdpf_tcprange (xio_nwinfo_t *nwinfo, int firstsrcport, int lastsrcport, int firstdstport, int lastdstport);
int xio_net_wrap_freedpf (xio_nwinfo_t *nwinfo, int demux_id);
int xio_net_wrap_getnettap (xio_nwinfo_t *nwinfo, u_int capno, u_int interfaces);
int xio_net_wrap_delnettap (xio_nwinfo_t *nwinfo, u_int capno, int tapno);
int xio_net_wrap_reroutedpf (xio_nwinfo_t *nwinfo, int demux_id);

#endif  /* __XIO_NET_WRAP_H__ */

