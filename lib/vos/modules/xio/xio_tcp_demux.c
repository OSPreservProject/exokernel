
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


#include <sys/types.h>

#include "xio_tcp.h"
#include "xio_tcp_demux.h"


/* it seems silly to have this here...*/
static int is_power_of_two (uint val)
{
   int i;
   int cnt = 0;
   for (i=0; i<(8 * sizeof(uint)); i++) {
      cnt += (val >> i) & 0x1;
   }
   return (cnt == 1);
}


void xio_tcp_demux_init (xio_dmxinfo_t *dmxinfo, uint hashsize)
{
   int i;

   dmxinfo->hashsize = (hashsize) ? hashsize : XIO_TCP_DEMUX_HASHSIZE;
   assert (is_power_of_two (dmxinfo->hashsize));
   for (i=0; i<dmxinfo->hashsize; i++) {
      LIST_INIT (&dmxinfo->hashtable[i]);
   }
   LIST_INIT (&dmxinfo->listenlist);
   dmxinfo->numhashed = 0;
}


int xio_tcp_demux_addconn (xio_dmxinfo_t *dmxinfo, struct tcb *tcb, int chkuniq)
{
   int entry = XIO_TCP_DEMUX_HASH (tcb->tcpsrc, tcb->tcpdst, dmxinfo->hashsize);
   struct tcb_list *head = &dmxinfo->hashtable[entry];

/*
   printf ("xio_tcp_demux_addconn: tcb %x, ipsrc %x, ipdst %x, tcpsrc %d, tcpdst %d, entry %d, temp %x\n", (uint) tcb, tcb->ipsrc, tcb->ipdst, tcb->tcpsrc, tcb->tcpdst, entry, (uint)temp);
*/

   assert (tcb->hashed == 0);

   if (chkuniq) {
     struct tcb *current;
     for (current = head->lh_first; current; current = current->hash_link.le_next) {
       if ((current->tcpboth == tcb->tcpboth) && (current->ipsrc == tcb->ipsrc) && (current->ipdst == tcb->ipdst)) {
	 return (-1);
       }
     }
   }

   LIST_INSERT_HEAD (head, tcb, hash_link);

   dmxinfo->numhashed++;
   tcb->hashed = 1;
   return (0);
}


void xio_tcp_demux_removeconn (xio_dmxinfo_t *dmxinfo, struct tcb *tcb)
{
/*
printf ("xio_tcp_demux_removeconn: tcb %x, ipsrc %x, ipdst %x, tcpsrc %d, tcpdst %d, entry %d, temp %x\n", (uint)tcb, tcb->ipsrc, tcb->ipdst, tcb->tcpsrc, tcb->tcpdst, entry, (uint)temp);
*/

   assert (tcb->hashed == 1);
   LIST_REMOVE (tcb, hash_link);
   tcb->hashed = 0;

   dmxinfo->numhashed--;
}


struct tcb * xio_tcp_demux_searchconns (xio_dmxinfo_t *dmxinfo, char *buf)
{
   struct tcp *rcv_tcp = (struct tcp *) &buf[XIO_EIT_TCP];
   struct ip *rcv_ip = (struct ip *) &buf[XIO_EIT_IP];
   uint32 ipsrc = *((unsigned int *) rcv_ip->source);
   uint32 ipdst = *((unsigned int *) rcv_ip->destination);
   uint32 tcpportnos = rcv_tcp->both_ports;

   int entry = XIO_TCP_DEMUX_HASH (rcv_tcp->src_port, rcv_tcp->dst_port, dmxinfo->hashsize);
   struct tcb_list *head = &dmxinfo->hashtable[entry];
   struct tcb *current;

   for (current = head->lh_first; current; current = current->hash_link.le_next) {
     assert (current->hashed == 1);
     if ((current->tcpboth == tcpportnos) && (current->ipsrc == ipsrc) && (current->ipdst == ipdst)) {
            return (current);
     }
   }
   return (NULL);
}


int xio_tcp_demux_addlisten (xio_dmxinfo_t *dmxinfo, struct tcb *tcb, int chkuniq)
{
  struct tcb_list *head = &dmxinfo->listenlist;
  
   if (chkuniq) {
     struct tcb *current;
     for (current = head->lh_first; current; current = current->listen_link.le_next) {
         if (tcb->tcpdst == current->tcpdst) {
            return (-1);
         }
     }
   }

   LIST_INSERT_HEAD (head, tcb, listen_link);
   tcb->hashed = 1;		/* for xio_tcp_demux_isconn */
   return (0);
}


void xio_tcp_demux_removelisten (xio_dmxinfo_t *dmxinfo, struct tcb *tcb)
{
   LIST_REMOVE (tcb, listen_link);
   tcb->hashed = 0;		/* for xio_tcp_demux_isconn */
}


struct tcb *xio_tcp_demux_searchlistens (xio_dmxinfo_t *dmxinfo, char *buf)
{
   struct tcp *rcv_tcp = (struct tcp *) &buf[XIO_EIT_TCP];
   struct tcb_list *head = &dmxinfo->listenlist;
   struct tcb *current;
   struct tcb *found = NULL;
   struct ip *rcv_ip = (struct ip *) &buf[XIO_EIT_IP];

   for (current = head->lh_first; current; current = current->listen_link.le_next) {
     if (rcv_tcp->dst_port == current->tcpdst)
       if (current->ipdst == *(u_int*)&rcv_ip->destination[0])
	 break;
       else {
	 if (!found && current->ipdst == INADDR_ANY) 
	   found = current;
       }
     }

   if (!current && found) 
     current = found;

   assert (current->hashed == 1);

   return (current);
}

