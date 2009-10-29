
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


#ifndef __XOK_PKT_H__
#define __XOK_PKT_H__

#include <xok/defs.h>
#include <xok/types.h>
#include <xok/queue.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>
#include <dpf/action.h>
#include <xok/pktring.h>
#include <xok/kdebug.h>
#include <xok/malloc.h>
#include <xok/mmu.h>
#include <xok/ae_recv.h>

struct xokpkt {
  struct ae_recv recv;
  TAILQ_ENTRY (xokpkt) link;
  int filterid;			/* ID of matched DPF filter */
  u_int interface;		/* interface number at which packet arrived */
  void (*freeFunc)(struct xokpkt *); /* function called when done */
  void *freeArg;		/* 2nd argument to freeFunc */
  uint64 *discardcntP;		/* counter to be incremented when discarding
				   packets */
  u_int count;			/* cnt of packet fragments (always 1 on rcv) */
  u_int len;			/* length of packet */
  char *data;			/* pointer to packet contents */
};
TAILQ_HEAD (xokpkt_list, xokpkt);

extern struct xokpkt_list pktq;

/* variables and functions related to the "nettap" support, which allows */
/* applications to get copies of all packets from specific interfaces... */
#define MAX_NETTAPS	8
void xokpkt_nettap (struct xokpkt *pkt);
int xokpkt_nettap_delpktring (int ringid);

/* kernel support functions */
int xoknet_init (void);
void xokpkt_consume (void);

extern void xokpkt_free(struct xokpkt*);

/* This function mallocs space for a xokpkt plus its data, and should */
/* be used only for non-optimized cases (others should do re-use...   */
static inline struct xokpkt * xokpkt_alloc (u_int len, u_int interface,
					    uint64 *discardcntP, int *error)
{
   struct xokpkt *pkt;

   if (len > NBPG) { /* XXX - need better size check */
     *error = -E_INVAL;
     return NULL;
   }
   pkt = (struct xokpkt *) malloc (sizeof(struct xokpkt) + len + 3);
   if (!pkt) {
     *error = -E_NO_MEM;
     return NULL;
   }
   pkt->count = 1;
   pkt->data = (char *)pkt + sizeof(struct xokpkt);
   StaticAssert (((uint)sizeof(struct xokpkt) % 4) == 0);
   pkt->discardcntP = discardcntP;
   pkt->interface = interface;
   pkt->freeFunc = xokpkt_free;
   pkt->freeArg = pkt;
   pkt->len = len;
   return (pkt);
}

/* This function initializes the fields of the xokpkt structure */
static inline int xokpkt_pktinit (struct xokpkt *pkt, u_int len,
				  u_int interface, uint64 *discardcntP)
{
  /* XXX - need better size check */
  if (len > NBPG || !pkt) return (-E_INVAL);

  StaticAssert (((uint)sizeof(struct xokpkt) % 4) == 0);
  pkt->count = 1;
  pkt->data = (char *)pkt + sizeof(struct xokpkt);
  pkt->discardcntP = discardcntP;
  pkt->interface = interface;
  pkt->freeFunc = NULL;
  pkt->freeArg = NULL;
  pkt->len = len;

  return (0);
}

/* This function, which should eventually replace the card specific send      */
/* functions, is called from same in order to give nettaps a shot at outgoing */
/* packets.  Other general functionalities should also go here...             */
static inline int xokpkt_send (struct ae_recv *recv, int interface)
{
   if (SYSINFO_GET(si_num_nettaps) > 0) {
     int error;
     struct xokpkt *pkt = xokpkt_alloc (sizeof (struct ae_recv), interface,
					NULL, &error);
     if (!pkt) return error;
     ae_recv_structcopy (recv, (struct ae_recv *) &pkt->count);
     xokpkt_nettap (pkt);
     free (pkt);
   }

   return 0;
}

struct msghold {
        int m;
	unsigned long long time;
        struct pkthold *pktlist;
        struct msghold *n;
};

extern void defilter(int msg, struct xokpkt *pkt);
extern void refilter(struct msghold *cur, struct msghold *last);
extern void refilterold(int msg);
extern struct msghold *msglist;
extern void msgclean(void);

/* This function hands a packet from a network device driver to xok. */
/* It is expected that all fields, except filterid and link, are     */
/* initialized before the call.  xok expects that it owns the memory */
/* holding the packet and pktqe header.  xok promises to call the    */
/* specified freeFunc (with freeArg as an argument) when it is done  */
/* processing the packet.                                            */
static inline void xokpkt_recv (struct xokpkt *pkt)
{
  int filterid;
  int ringid;
  struct frag_return result;    /* result of pkt filter */

#ifdef KDEBUG
  /* 
   * if the packet is for kernel debugging
   * dont even let it get into the packet queue
   */
  if (kdebug_is_debug_pkt(pkt->data, pkt->len)) {
      kdebug_pkt(pkt->data, pkt->len);
      if (pkt->freeFunc) pkt->freeFunc (pkt);
      return;
    }
#endif

  if (SYSINFO_GET(si_num_nettaps) > 0) {
    xokpkt_nettap (pkt);
  }

  filterid = dpf_iptr (pkt->data, pkt->len, &result);

  if (filterid <= 0) 
  {
#if DPF_FRAGMENTATION
    if (result.headtail == 2) 
    {
      int len = pkt->len;
      struct xokpkt *pkt_new =
	(struct xokpkt*) malloc(sizeof(struct xokpkt)+len+3);
      pkt_new->data = (char *)pkt_new + sizeof(struct xokpkt);
      memmove(pkt_new,pkt,sizeof(struct xokpkt));
      memmove(pkt_new->data,pkt->data,len);
      pkt_new->freeFunc = xokpkt_free;
      pkt_new->freeArg = pkt_new;
      pkt_new->discardcntP = 0L;
      defilter(result.msgid, pkt_new);
    } 
    else 
#endif
    {
      /* nobody wants this packet */
      if (pkt->discardcntP) (*pkt->discardcntP)++;
    }
  } 
  
  else if ((ringid = dpf_fid_getringval(filterid)) > 0) 
  {
    /* somebody with a valid ring buffer wants this packet */
    pktring_handlepkt (ringid, (struct ae_recv *) &pkt->count);

#if DPF_FRAGMENTATION
    if (result.headtail == 1)
      refilterold(result.msgid);
#endif
  } 
  
  else 
  {
    printf("no one wants the packet...\n");

#if ASH_ENABLE
    /* XXX no longer uses ASHes */

    /* 
     * Somebody without a valid ring buffer wants this packet.  Until ASHes
     * are officially deleted, this should be saved in a queue and forwarded
     * upward at the very end of the interrupt routine (since ASHes do not
     * return to the kernel where they left off...) 
     */
    pkt->filterid = filterid;
    TAILQ_INSERT_TAIL (&pktq, pkt, link);
#endif /* ASH_ENABLE */
  }
    
  if (pkt->freeFunc) pkt->freeFunc (pkt);
}

#endif /* __XOK_PKT_H__ */
