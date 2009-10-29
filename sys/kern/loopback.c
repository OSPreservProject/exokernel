
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

#include <xok/sysinfo.h>
#include <xok/types.h>
#include <xok/syscall.h>
#include <xok/sys_proto.h>
#include <xok/syscallno.h>
#include <xok/pkt.h>
#include <xok/ae_recv.h>
#include <xok/printf.h>
#include <xok_include/assert.h>
#include <xok_include/string.h>
#include <xok_include/net/ether.h>

/* XXX - need better size determination */
static char pktinfo[sizeof(struct xokpkt) + NBPG + 3];
static struct xokpkt *pkt = (struct xokpkt *)&pktinfo;


static int loopback_xmit (void *cardstruct, struct ae_recv *recv, int xmitintr)
{
   struct network *xoknet = cardstruct;
   int len;
   int ret;

   pkt->data = (char *)pkt + sizeof(struct xokpkt);
   len = ae_recv_datacpy (recv, pkt->data, 0, ETHER_MAX_LEN);
   ret = xokpkt_pktinit(pkt, len, xoknet->netno, &xoknet->discards);
   pkt->freeFunc = ((struct xokpkt *) recv)->freeFunc;
   pkt->freeArg = recv;
   if (ret < 0) return (ret);
   xokpkt_recv (pkt);

   xoknet->xmits++;
   xoknet->rcvs++;

   /* If the packet is handled by an ash then we won't return from
      xokpkt_consume, so we preset the return value to 0. */
   /* XXX - are user registers properly restored with ash code? */
   utf->tf_eax = 0;
   xokpkt_consume ();

   return (0);
}


void loopback_init ()
{
   struct network * xoknet = xoknet_getstructure (XOKNET_LOOPBACK, loopback_xmit, NULL);
   xoknet->irq = 0xffffffff;
   bzero (xoknet->ether_addr, ETHER_ADDR_LEN);
   sprintf (xoknet->cardname, "lo0");
   xoknet->inited = 1;
   xoknet->cardstruct = xoknet;
}


#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

