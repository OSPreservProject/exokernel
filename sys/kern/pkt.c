

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

#define __NETWORK_MODULE__

#include <xok/pmap.h>
#include <xok/sysinfo.h>
#include <xok/defs.h>
#include <xok/types.h>
#include <xok/kerrno.h>
#include <xok/bitvector.h>
#include <xok_include/assert.h>
#include <xok/queue.h>
#include <xok/env.h>
#include <xok/sys_proto.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>
#include <xok/pkt.h>
#include <xok/pctr.h>
#include <xok/pktring.h>
#include <xok/init.h>
#include <xok/printf.h>

/* variables to support xokpkt_consume, which in turn supports ASHes */
struct xokpkt_list pktq;
struct xokpkt *lastpkt = NULL;

int xok_ed_xmit (void *, struct ae_recv *, int);


int
xoknet_init (void)
{
  int i;
  TAILQ_INIT (&pktq);

  bzero (SYSINFO_GET (si_networks), (XOKNET_MAXNETS * sizeof (struct network)));

  /* assign ownership of all network interfaces to zero length cap */
  for (i = 0; i < XOKNET_MAXNETS; i++)
    {
      cap_init_zerolength (&(SYSINFO_PTR_AT (si_networks, i)->netcap));
      /* cap_init_zerolength (&si->si_networks[i].netcap); */
    }

  /* Initialize the loopback interface */
  loopback_init ();

  return 0;
}


/* xokpkt_consume and the pktq really only exist to support ASHes. Packets */
/* that are placed into packet rings are handled right away, whereas ASHes */
/* must be invoked at points when nothing important in the kernel is       */
/* only partly complete.  This is because ASHes exit the kernel at the     */
/* point they are invoked, but return via a system call interface (thereby */
/* blowing away the one kernel stack) and *not* to the point at which they */
/* left...                                                                 */
void
xokpkt_consume (void)
{
#if 0
  int f;
  struct Env *e;
  int ringid;

  MP_SPINLOCK_GET (GLOCK(PKTQ_LOCK));

  for (;;)
    {

      if (lastpkt)
	{
	  if (lastpkt->freeFunc)
	    lastpkt->freeFunc (lastpkt);
	  lastpkt = NULL;
	}

      lastpkt = pktq.tqh_first;

      if (!lastpkt)
	{
	  MP_SPINLOCK_RELEASE (GLOCK(PKTQ_LOCK));
	  return;
	}

      TAILQ_REMOVE (&pktq, lastpkt, link);

      f = lastpkt->filterid;

      if (f > 0)
	{
	  e = dpf_fid_lookup (f);
	  if (e)
	    {
	      if ((ringid = dpf_fid_getringval (f)) > 0)
		{
		  pktring_handlepkt (ringid, (struct ae_recv *) &lastpkt->count);
		}

#ifdef ASH_ENABLE
	      else if (e->env_u && e->env_u->u_ashnet)
		{
		  struct seg_desc *sdp = &gdt[GD_AW >> 3];
		  /* XXX: len-1 for limit? */

		  *sdp = smallseg (0, (u_int) lastpkt->data, lastpkt->len, 2);

		  MP_SPINLOCK_RELEASE (GLOCK(PKTQ_LOCK));
		  ash_invoke (e, e->env_u->u_ashnet, lastpkt->len, f);
		  /* We never return here; instead, we return in ash_return,
		   * which calls pf_consume to process more packets.
		   */
		}
#endif

	    }
	  else
	    {
	      /* XXX -- should we ever get NULL env's? */
	      dpf_delete (f);
	    }
	}
      else if (lastpkt->discardcntP)
	{
	  (*lastpkt->discardcntP)++;
	}
    }

  MP_SPINLOCK_RELEASE (GLOCK(PKTQ_LOCK));
#endif
}


void 
xmit_done (struct xokpkt *xokpkt)
{
  int i;

  if (xokpkt->freeArg != NULL)
    {
      int *resptr = xokpkt->freeArg;
      (*resptr)--;
      ppage_unpin (kva2pp ((u_long) resptr));
    }

  for (i = 0; i < xokpkt->recv.n; i++)
    {
      ppage_unpin (kva2pp ((u_long) xokpkt->recv.r[i].data));
    }

  free (xokpkt);
}


struct network *
xoknet_getstructure (int cardtype, int (*xmitfunc) (void *, struct ae_recv *, int), void *cardstruct)
{
  int i = SYSINFO_GET (si_nnetworks);
  struct network *xoknet = SYSINFO_PTR_AT (si_networks, i);

  assert (i != XOKNET_MAXNETS);
  Sysinfo_si_nnetworks_atomic_inc(si); 

  xoknet->cardtype = cardtype;
  xoknet->cardstruct = cardstruct;
  xoknet->xmitfunc = xmitfunc;
  xoknet->netno = i;
  MP_SPINLOCK_INIT(&xoknet->slock);
  return (xoknet);
}


/****************************************************************************/
/* xok's system call for transmitting a packet on a network interface       */
/* The four parameters are the interface number, an ae_recv structure       */
/* identifying the data to be transmitted, a pointer (can be NULL) to an    */
/* integer that should be decremented after the transmission has completed, */
/* and an integer specifying whether a post-transmission interrupt is       */
/* desired.  The third parameter is needed because when the system call     */
/* returns, the transmission has been set up but not initiated or completed. */
/* The card may DMA the data from the specified memory locations at         */
/* transmission time -- so the caller should treat it as inviolate.  A call */
/* to sys_net_xmit can fail if any of the specified memory locations are    */
/* not properly accessible by the caller, or if the total transmission size */
/* is outside the allowed limits (i.e., smaller than the minimum or larger  */
/* than the maximum).  The last parameter, "xmitintr", was added to tell    */
/* the driver whether or not a distinct interrupt is wanted at transmission */
/* completion time.  Not having the interrupt involves the danger of        */
/* not being informed of transmission completion for an unbounded amount    */
/* of time (when the next packet arrives or transmission interrupt occurs)  */
/****************************************************************************/


int
sys_net_xmit (u_int callno, int cardno, struct ae_recv *recv, int *resptr, int xmitintr)
{
  struct network *xoknet = SYSINFO_PTR_AT (si_networks, cardno);
  int segcnt;
  int ret;
  int i, j;
  struct ae_recv *new_recv;
  struct xokpkt *xokpkt;

  if ((cardno < 0) || (cardno >= SYSINFO_GET (si_nnetworks)))
    {
      printf ("sys_net_xmit: unfound cardno %d\n", cardno);
      return (-2);
    }

  /* check if the recv is really accessible to the caller */
  /* Only internal kernel calls can pass a callno of 0 */
  if (callno != 0)
    {
      if ((!isreadable_varange ((u_int) recv, sizeof (int))) || (!isreadable_varange ((u_int) & recv->r[0], (recv->n * sizeof (struct rec)))))
	{
	  printf ("sys_net_xmit: invalid recv address (%p)\n", recv);
	  return (-2);
	}
    }

  if (resptr != NULL)
    {
      /* Only internal kernel calls can pass a callno of 0 */
      if (callno != 0)
	{
	  if (iswriteable_varange ((u_int) resptr, sizeof (int)) == 0)
	    {
	      printf ("sys_net_xmit: invalid resptr address (%p)\n", resptr);
	      return (-2);
	    }
	  resptr = (int *) pa2kva (va2pa (resptr));
	}
    }

  /* check the partial and total lengths of the gathered portions */
  segcnt = 0;
  for (i = 0; i < recv->n; i++)
    {
      if (recv->r[i].sz < 0)
	{
	  printf ("net_xmit: bad size #%d: %d\n", i, recv->r[i].sz);
	  return (-2);
	}
      segcnt += recv->r[i].sz;
    }
  if ((segcnt < ETHER_MIN_LEN) || (segcnt > ETHER_MAX_LEN))
    {
      printf ("net_xmit: trying to send packet that is too large or too small (%d)\n", segcnt);
      return (-2);
    }

  xokpkt = malloc (sizeof (struct xokpkt));
  xokpkt->freeArg = resptr;
  xokpkt->interface = cardno;
  xokpkt->freeFunc = xmit_done;
  new_recv = &xokpkt->recv;
  new_recv->n = 0;

  j = 0;
  for (i = 0; i < recv->n; i++)
    {
      int len = recv->r[i].sz;
      caddr_t addr = recv->r[i].data;
      unsigned pagebound = NBPG - (((u_long) addr) & (NBPG - 1));

      while (len > 0)
	{
	  unsigned slen = min (len, pagebound);

	  if (new_recv->n == AE_RECV_MAXSCATTER)
	    {
	      printf ("net_xmit: too many discontiguous gather list entries\n");
	      return (-2);
	    }
	  new_recv->r[j].sz = slen;
	  new_recv->r[j].data = addr;
	  if (callno != 0)
	    {
	      if (!isreadable_varange ((u_int) addr, slen))
		{
		  /* physical page is not accessible */
		  printf ("net_xmit: send of non-readable addr (%p, slen %d)\n", addr, slen);
		  return (-2);
		}
	      new_recv->r[j].data = (unsigned char *) (pa2kva (va2pa (addr)));
	    }
	  new_recv->n++;
	  j++;
	  len -= slen;
	  addr += slen;
	  pagebound = NBPG;
	}
    }

  for (i = 0; i < new_recv->n; i++)
    {
      ppage_pin (kva2pp ((u_int) new_recv->r[i].data));
    }

  if (resptr != NULL)
    {
      ppage_pin (kva2pp ((u_int) resptr));
    }

  xoknet->xmits++;

  /* let any nettaps grab a copy too */
  xokpkt_send (new_recv, xoknet->netno);

  if ((resptr != NULL) && (xmitintr != 0))
    {
      xmitintr = 1;
    }

  ret = xoknet->xmitfunc (xoknet->cardstruct, new_recv, xmitintr);

  if (ret < 0)
    {
      for (i = 0; i < new_recv->n; i++)
	{
	  ppage_unpin (kva2pp ((u_int) new_recv->r[i].data));
	}
      if (resptr != NULL)
	{
	  ppage_unpin (kva2pp ((u_int) resptr));
	}
      free (xokpkt);
    }

  return (ret);
}


/* variables and functions related to the "nettap" support, which allows */
/* applications to get copies of all packets from specific interfaces... */
struct nettap
{
  int tap_id;
  int ringid;
  u_int interfaces;
  struct nettap *next;
};

static u_int next_tap_id = 1;
static struct nettap *nettaps = NULL;

/* Set up a "super-filter" to copy all packets sent to the network interfaces */
/* whose corresponding bits are set in `interfaces' to ring `id'.  If         */
/* `interfaces' is 0, then `id' identifies an existing network tap and the    */
/* system call serves to turn it off.  The caller must have a capability (#k  */
/* in their capring) that dominates each tapped interface's netcap.           */
int 
sys_nettap (u_int sn, u_int k, int id, u_int interfaces)
{
  cap c;
  struct nettap *nettap = NULL;
  int i;
  int r;

  if ((r = env_getcap (curenv, k, &c)) < 0)
    {
      printf ("unable to get cap #%d for curenv\n", k);
      return r;
    }

  MP_SPINLOCK_GET (GLOCK(NETTAP_LOCK));

  /* deleting a tap */
  if (interfaces == 0)
    {
      /* find the nettap in question */
      nettap = nettaps;
      while ((nettap != NULL) && (nettap->tap_id != id))
	{
	  nettap = nettap->next;
	}
      if (nettap == NULL)
	{
	  MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
	  return -E_NOT_FOUND;
	}
      /* set interfaces for which rights will be checked */
      interfaces = nettap->interfaces;
    }

  /* must have access to each network interface tapped */
  StaticAssert (XOKNET_MAXNETS <= (8 * sizeof (interfaces)));
  StaticAssert (BV_SZ (XOKNET_MAXNETS) == 1);	

  for (i = 0; i < XOKNET_MAXNETS; i++)
    {
      if ((bv_bt (&interfaces, i) &&
	 ((r = acl_access (&c, &(SYSINFO_PTR_AT (si_networks, i)->netcap), 1,
			   ACL_ALL)) < 0)))
	{
	  MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
	  return r;
	}
    }

  /* complete the tap deletion */
  if (nettap != NULL)
    {
      struct nettap *tmp = nettaps;
      if (tmp == nettap)
	{
	  nettaps = nettap->next;
	}
      else
	{
	  while ((tmp != NULL) && (tmp->next != nettap))
	    {
	      tmp = tmp->next;
	    }
	  if (tmp == NULL)
	    {
	      warn ("sys_nettap: couldn't find tap to delete");

	      MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
	      return -E_INVAL;
	    }
	  tmp->next = nettap->next;
	}
      /* successful completion */
      Sysinfo_si_num_nettaps_atomic_dec(si);
      free (nettap);

      MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
      return (0);
    }

  if (SYSINFO_GET (si_num_nettaps) >= MAX_NETTAPS)
    {
      MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
      return -E_NO_NETTAPS;
    }

  /* make sure pktring `ringid' is valid */
  /* GROK - should also require access to pktring?? */

  nettap = (struct nettap *) malloc (sizeof (struct nettap));
  if (nettap == NULL)
    {
      MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
      return -E_NO_MEM;
    }

  Sysinfo_si_num_nettaps_atomic_inc(si);
  nettap->tap_id = next_tap_id;
  next_tap_id++;
  nettap->ringid = id;
  nettap->interfaces = interfaces;
  nettap->next = nettaps;
  nettaps = nettap;

  MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
  return (nettap->tap_id);
}

/* this callback is used for eliminating nettaps that place packets into */
/* pktrings that are being (or have been) deleted.                       */
int 
xokpkt_nettap_delpktring (int ringid)
{
  struct nettap *nettap = nettaps;

  MP_SPINLOCK_GET (GLOCK(NETTAP_LOCK));

  while ((nettap != NULL) && (ringid == nettap->ringid))
    {
      nettaps = nettap->next;
      Sysinfo_si_num_nettaps_atomic_dec(si);
      free (nettap);
      nettap = nettaps;
    }
  if (nettap != NULL)
    {
      while (nettap->next != NULL)
	{
	  if (ringid == nettap->next->ringid)
	    {
	      struct nettap *del = nettap->next;
	      nettap->next = del->next;
      	      Sysinfo_si_num_nettaps_atomic_dec(si);
	      free (del);
	    }
	  else
	    {
	      nettap = nettap->next;
	    }
	}
    }
  else
    {
      MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
      return (-E_NOT_FOUND);
    }

  MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
  return (0);
}

void 
xokpkt_nettap (struct xokpkt *pkt)
{
  struct nettap *nettap = nettaps;

  MP_SPINLOCK_GET (GLOCK(NETTAP_LOCK));

  while (nettap != NULL)
    {
      if (bv_bt (&nettap->interfaces, pkt->interface))
	{
	  pktring_handlepkt (nettap->ringid, (struct ae_recv *) &pkt->count);
	}
      nettap = nettap->next;
    }

  MP_SPINLOCK_RELEASE (GLOCK(NETTAP_LOCK));
}


void 
xokpkt_free(struct xokpkt *pkt)
{
  free(pkt);
}


#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif
