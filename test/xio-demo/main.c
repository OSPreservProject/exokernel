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

#include "xio/xio_tcpcommon.h"
#include "stdlib.h"
#include "assert.h"

static char *page_alloc_wrapper (void *ptr, int len) {
  ptr = (void *)malloc (len);
  assert (ptr);
  return (ptr);
}

void xio_init (xio_dmxinfo_t *dmxinfo, xio_twinfo_t *twinfo, 
		xio_nwinfo_t *nwinfo) {

  xio_tcp_demux_init (dmxinfo, 0);

  /* pass in space for hash-table of packets in time-wait */
  xio_tcp_timewait_init (twinfo, 4096, page_alloc_wrapper);

  /* pass in pages for receive rings */
  xio_net_wrap_init (nwinfo, (char *)malloc(32*NBPG), (32 * NBPG));
}  

void xio_listen (int port, xio_nwinfo_t *nwinfo, 
		 xio_dmxinfo_t *dmxinfo)
{
   struct listentcb *listentcb = 
     (struct listentcb *) malloc (sizeof(struct listentcb));
   int demux_id = xio_net_wrap_getdpf_tcp (nwinfo, port, -1);
   assert (demux_id != -1);
   xio_tcp_initlistentcb (listentcb, INADDR_ANY, port);
   xio_tcp_demux_addlisten (dmxinfo, (struct tcb *) listentcb, 0);
}

struct ae_recv *xio_get_next_packet (xio_nwinfo_t *nwinfo,
				     xio_dmxinfo_t *dmxinfo) {

  struct ae_recv *packet;
  
  xio_tcp_waitforChange (nwinfo, 0);

  packet = xio_net_wrap_getPacket(nwinfo);
  assert (packet);

  assert (xio_tcp_packetcheck (packet->r[0].data, packet->r[0].sz, NULL));

  return packet;
}

void xio_handle_next_packet (struct tcb **tcb, xio_nwinfo_t *nwinfo,
			     xio_dmxinfo_t *dmxinfo) {
  struct ae_recv *packet;

  packet = xio_get_next_packet (nwinfo, dmxinfo);
  xio_tcp_handlePacket (*tcb, packet->r[0].data, 0);
  xio_tcpcommon_sendPacket (*tcb, 0);
}

struct tcb *xio_start_accept (struct ae_recv *packet, 
			       xio_nwinfo_t *nwinfo, 
			       xio_dmxinfo_t *dmxinfo) {
  int netcardno;
  struct tcb *tcb;

  tcb = (struct tcb *)malloc (sizeof (struct tcb));
  assert (tcb);
  xio_tcp_inittcb (tcb);

  if (!xio_tcpcommon_findlisten (dmxinfo, packet->r[0].data, nwinfo, 
				 &netcardno)) {
    return 0;
  }

  tcb->netcardno = netcardno;
  xio_tcp_handleListenPacket (tcb, packet->r[0].data);

  xio_tcp_demux_addconn (dmxinfo, tcb, 0);

  return (tcb);
}

int xio_finish_accept (struct tcb *tcb, xio_nwinfo_t *nwinfo, 
		       xio_dmxinfo_t *dmxinfo) {
  struct ae_recv *packet;

  xio_tcpcommon_sendPacket (tcb, 0); /* we send SYN & ACK to their SYN */
  packet = xio_get_next_packet (nwinfo, dmxinfo); 
  xio_tcp_handlePacket (tcb, packet->r[0].data, 0); /* we get their ACK to our SYN */
  return (tcb->state = TCB_ST_ESTABLISHED);
}

/* Example of a tcp server written using XIO. It accepts connections,
   sends back a static message, and then closes the connection */

int main () {
  xio_dmxinfo_t dmxinfo;
  xio_twinfo_t *twinfo = NULL;
  xio_nwinfo_t nwinfo;
  struct tcb *tcb;
  struct ae_recv *packet;

  /* initialize everything */
  twinfo = (xio_twinfo_t *) malloc (xio_twinfo_size (4096));
  assert (twinfo);
  xio_init (&dmxinfo, twinfo, &nwinfo);

  /* add a listen struct and insert a filter */
  xio_listen (10102, &nwinfo, &dmxinfo);

  while (1) {

    /* get a connect request */
    packet = xio_get_next_packet (&nwinfo, &dmxinfo);

    /* returns a tcb if there was a pending connect request */
    tcb = xio_start_accept (packet, &nwinfo, &dmxinfo);

    if (tcb) {
      char msg[] = "hi there from XIO\n";
    
      /* finish establishing the connection */
      assert (xio_finish_accept (tcb, &nwinfo, &dmxinfo));

      /* and send them the data and close it all off */
      xio_tcp_prepDataPacket (tcb, msg, strlen (msg), NULL, 0, TCP_SEND_ALSOCLOSE, -1);
      xio_tcpcommon_sendPacket (tcb, 0);

      /* don't we need to handle one more packet? */
    }
  }
}



