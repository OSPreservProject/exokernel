
/*
 * Interrupt handling, receiving, and transmitting part of the device handler.
 * Mainly for 21140 cards. May work on others, but I am not sure  - benjie
 */


/* $OpenBSD: if_de.c,v 1.26 1997/11/16 07:46:46 millert Exp $      */
/* $NetBSD: if_de.c,v 1.45 1997/06/09 00:34:18 thorpej Exp $       */

/*
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com) All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Id: if_de.c,v 1.89 1997/06/03 19:19:55 thomas Exp
 */


/*
 * DEC 21040 PCI Ethernet Controller
 *
 * Written by Matt Thomas
 * BPF support code stolen directly from if_ec.c
 *
 *   This driver supports the DEC DE435 or any other PCI
 *   board which support 21040, 21041, or 21140 (mostly).
 */

#define	TULIP_HDR_DATA
#define __NETWORK_MODULE__

#include <xok/sysinfo.h>
#include <xok/picirq.h>
#include <xok/types.h>
#include <xok/defs.h>
#include <xok/printf.h>
#include <xok_include/string.h>
#include <xok_include/assert.h>
#include <xok/cpu.h>
#include <xok/pctr.h>
#include <xok/pkt.h>
#include <xok/bios32.h>
#include <xok/scheduler.h>
#include <xok/ae_recv.h>
#include "if_devar.h"

#define TULIP_UNIT_TO_SOFTC(unit) \
  ((tulip_softc_t *)&(SYSINFO_PTR_AT(si_networks,(unit))->cardstruct))

#define ifnet ifnet2
#define dprintf if (0) printf
#define ddprintf if (0) printf

extern struct xokpkt *tulip_pending_pkts;
extern struct xokpkt *tulip_reserve_pkts;
extern void tulip_reset(tulip_softc_t * const sc);
extern ifnet_ret_t tulip_ifstart(struct ifnet *ifp);
extern void tulip_media_print(tulip_softc_t * const sc);
extern ifnet_ret_t tulip_ifstart(struct ifnet * const ifp);
extern void tulip_init(tulip_softc_t * const sc);


/*
 * tulip rx interrupt handler
 */
void
tulip_rx_intr(tulip_softc_t * const sc)
{
  tulip_ringinfo_t *const ri = &sc->tulip_rxinfo;
  struct ifnet *const ifp = &sc->tulip_if;
  int npkts = 0;

  dprintf("tulip_rx_intr\n");

  for (;;)
  {
    struct ether_header eh;
    tulip_desc_t *eop = ri->ri_nextin;
    int total_len = 0;
    int accept = 0;

    /* 
     * If still owned by the TULIP, don't touch it. 
     */
    if (((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER) 
      break;

    /* 
     * Explicitly not including BIG_PACKET support 
     */
    if (!((((volatile tulip_desc_t *) eop)->d_status & 
	    (TULIP_DSTS_OWNER|TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) 
	  == (TULIP_DSTS_RxFIRSTDESC | TULIP_DSTS_RxLASTDESC)))
    {
      printf("multi-frame packet not supported: eop %p, first %p\n", 
	     eop, ri->ri_first);
      assert(0);
    }

    /*
     *  Now get the size of received packet (minus the CRC).
     */
    total_len = ((eop->d_status >> 16) & 0x7FFF) - 4;

    if ((sc->tulip_flags & TULIP_RXIGNORE) == 0
	&& ((eop->d_status & TULIP_DSTS_ERRSUM) == 0))
    {
      sc->tulip_flags |= TULIP_RXACT;
      if ((sc->tulip_flags & (TULIP_PROMISC | TULIP_HASHONLY))
	  && (eh.ether_dhost[0] & 1) == 0
	  && !TULIP_ADDREQUAL(eh.ether_dhost, sc->tulip_enaddr))
	goto next;
      accept = 1;
    } 
    
    else
      /* detected an error */
    {
      ifp->if_ierrors++;
      sc->xoknet->inerrs++;
     
      if (eop->d_status & 
	  (TULIP_DSTS_RxBADLENGTH|TULIP_DSTS_RxOVERFLOW|TULIP_DSTS_RxWATCHDOG))
	sc->tulip_dot3stats.dot3StatsInternalMacReceiveErrors++;
      
      else
      {
	const char *error = NULL;
	if (eop->d_status & TULIP_DSTS_RxTOOLONG)
	{
	  sc->tulip_dot3stats.dot3StatsFrameTooLongs++;
	  error = "frame too long";
	}

	if (eop->d_status & TULIP_DSTS_RxBADCRC)
	{
	  if (eop->d_status & TULIP_DSTS_RxDRBBLBIT)
	  {
	    sc->tulip_dot3stats.dot3StatsAlignmentErrors++;
	    error = "alignment error";
	  } 
	  
	  else
	  {
	    sc->tulip_dot3stats.dot3StatsFCSErrors++;
	    error = "bad crc";
	  }
	}
	
	if (error != NULL && (sc->tulip_flags & TULIP_NOMESSAGES) == 0)
	{
	  printf(TULIP_PRINTF_FMT ": receive: " TULIP_EADDR_FMT ": %s\n",
		 TULIP_PRINTF_ARGS,
		 TULIP_EADDR_ARGS(pa2kva(eop->d_addr1) + 6),
		 error);
	  sc->tulip_flags |= TULIP_NOMESSAGES;
	}
      }
    }

next:
    if (accept)
    {
      npkts++;
      sc->xoknet->rcvs++;
      {
	/*
	 * grab the xokpkt, put it on a pending list, then reset the dma list
         */

	struct xokpkt  *newq;
	struct xokpkt  *q = (struct xokpkt *)
	((char *) (pa2kva(eop->d_addr1)) - sizeof(struct xokpkt));
	q->len = total_len;

dequeue_retry:
	newq = tulip_reserve_pkts;

	if (newq == 0L)
	{
	  extern void     tulip_return_buffer(struct xokpkt *);
	  newq = (struct xokpkt *) malloc(TULIP_RX_FULLLEN);
	  assert(newq);

	  newq->count = 1;
	  newq->data = (char *) newq + sizeof(struct xokpkt);
	  newq->interface = sc->tulip_unit;
	  newq->discardcntP = &sc->xoknet->discards;
	  newq->freeFunc = tulip_return_buffer;
	  newq->freeArg = 0L;
	  dprintf("created pkt %p\n", newq);
	} 
	
	else
	{
	  if (compare_and_swap((int *) &(tulip_reserve_pkts),
			       (int) newq,
			       (int) newq->freeArg) != (int) newq)
	    goto dequeue_retry;
	  newq->freeArg = 0L;
	  dprintf("obtained pkt %p from reserves list\n", newq);
	}

	/* reset descriptor ring */
	eop->d_addr1 = kva2pa(newq->data);
	eop->d_status = TULIP_DSTS_OWNER;
          
	q->freeArg = 0L;
        dprintf("handling pkt %p\n", q);
	xokpkt_recv(q);
      }
    }

    ifp->if_ipackets++;
    if (++eop == ri->ri_last)
      eop = ri->ri_first;
    ri->ri_nextin = eop;
  }
  ddprintf("tulip_rx_intr: got %d packets\n", npkts);
}



/* 
 * tulip_return_buffer: puts xokpkt back on reserves list 
 */
void
tulip_return_buffer(struct xokpkt * pkt)
{
queue_retry:
  pkt->freeArg = tulip_reserve_pkts;

  if (compare_and_swap((int *) &(tulip_reserve_pkts),
		       (int) pkt->freeArg,
		       (int) pkt) != (int) pkt->freeArg)
    goto queue_retry;
  dprintf("pkt %p placed back on reserves list\n", pkt);
}


/*
 * tx interrupt handler
 */
int
tulip_tx_intr(tulip_softc_t * const sc)
{
  tulip_ringinfo_t *const ri = &sc->tulip_txinfo;
  int             xmits = 0;
  int             descs = 0;

  ddprintf("in tulip_tx_intr, cpu %d\n", get_cpu_id());

  while (ri->ri_free < ri->ri_max)
  {
    u_int32_t d_flag;
    
    if (((volatile tulip_desc_t *) ri->ri_nextin)->d_status & TULIP_DSTS_OWNER)
      break;

    d_flag = ri->ri_nextin->d_flag;

    if (d_flag & TULIP_DFLAG_TxLASTSEG)
    {
      if (d_flag & TULIP_DFLAG_TxSETUPPKT)
      {
	/*
	 * We've just finished processing a setup packet. Mark that we
	 * finished it. If there's not another pending, startup the TULIP
	 * receiver.  Make sure we ack the RXSTOPPED so we won't get an
	 * abormal interrupt indication.
         */
	dprintf("finish setup packet\n");

	sc->tulip_flags &= ~(TULIP_DOINGSETUP | TULIP_HASHONLY);
	if (ri->ri_nextin->d_flag & TULIP_DFLAG_TxINVRSFILT)
	  sc->tulip_flags |= TULIP_HASHONLY;

	if ((sc->tulip_flags & (TULIP_WANTSETUP | TULIP_TXPROBE_ACTIVE)) == 0)
	{
	  tulip_rx_intr(sc);
	  sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
	  sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;

	  dprintf("writing csr_status in tx_intr\n");
	  
	  TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
	  TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	  TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	}
      } 
      
      else
      {
	const u_int32_t d_status = ri->ri_nextin->d_status;
	struct xokpkt *xokpkt = sc->tulip_txdones[ri->ri_nextin-ri->ri_first];

	dprintf("notify about completion of tx\n");
	if (xokpkt)
	  xokpkt->freeFunc(xokpkt);

	sc->tulip_txdones[ri->ri_nextin - ri->ri_first] = NULL;

	ri->ri_nextin->d_addr1 = 0;
	if (ri->ri_nextin->d_addr2)
	  ri->ri_nextin->d_addr2 = 0;
	
	if (sc->tulip_flags & TULIP_TXPROBE_ACTIVE)
	{
	  tulip_mediapoll_event_t event = TULIP_MEDIAPOLL_TXPROBE_OK;
	  
	  if (d_status & (TULIP_DSTS_TxNOCARR | TULIP_DSTS_TxEXCCOLL))
	    event = TULIP_MEDIAPOLL_TXPROBE_FAILED;

	  (*sc->tulip_boardsw->bd_media_poll) (sc, event);

	  /* Escape from the loop before media poll has reset the TULIP! */
	  break;
	} 
	
	else
	{
	  xmits++;
	  if (d_status & TULIP_DSTS_ERRSUM)
	  {
	    sc->tulip_if.if_oerrors++;
	    sc->xoknet->outerrs++;
	    if (d_status & TULIP_DSTS_TxEXCCOLL)
	      sc->tulip_dot3stats.dot3StatsExcessiveCollisions++;
	    if (d_status & TULIP_DSTS_TxLATECOLL)
	      sc->tulip_dot3stats.dot3StatsLateCollisions++;
	    if (d_status & (TULIP_DSTS_TxNOCARR | TULIP_DSTS_TxCARRLOSS))
	      sc->tulip_dot3stats.dot3StatsCarrierSenseErrors++;
	    if (d_status & (TULIP_DSTS_TxUNDERFLOW | TULIP_DSTS_TxBABBLE))
	      sc->tulip_dot3stats.dot3StatsInternalMacTransmitErrors++;
	    if (d_status & TULIP_DSTS_TxUNDERFLOW)
	      sc->tulip_dot3stats.dot3StatsInternalTransmitUnderflows++;
	    if (d_status & TULIP_DSTS_TxBABBLE)
	      sc->tulip_dot3stats.dot3StatsInternalTransmitBabbles++;
	  } 
	  
	  else
	  {
	    u_int32_t       collisions =
	    (d_status & TULIP_DSTS_TxCOLLMASK)
	    >> TULIP_DSTS_V_TxCOLLCNT;
	    sc->tulip_if.if_collisions += collisions;
	    
	    if (collisions == 1)
	      sc->tulip_dot3stats.dot3StatsSingleCollisionFrames++;
	    else if (collisions > 1) 
	      sc->tulip_dot3stats.dot3StatsMultipleCollisionFrames++; 
	    else if (d_status & TULIP_DSTS_TxDEFERRED) 
	      sc->tulip_dot3stats.dot3StatsDeferredTransmissions++;
	    
	    /*
             * SQE is only valid for 10baseT/BNC/AUI when not
             * running in full-duplex.  In order to speed up the
             * test, the corresponding bit in tulip_flags needs to
             * set as well to get us to count SQE Test Errors.
             */
	    if (d_status & TULIP_DSTS_TxNOHRTBT & sc->tulip_flags)
	      sc->tulip_dot3stats.dot3StatsSQETestErrors++;
	  }
	}
      }
    } 
    
    else
    {

      dprintf("asking for more... \n");

      /* ! LASTSEG, so unpin the finished "portions" */
      ri->ri_nextin->d_addr1 = 0;
      
      if (ri->ri_nextin->d_addr2)
	ri->ri_nextin->d_addr2 = 0;
    }

    if (++ri->ri_nextin == ri->ri_last)
      ri->ri_nextin = ri->ri_first;

    ri->ri_free++;
    descs++;
    if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0)
      sc->tulip_if.if_flags &= ~IFF_OACTIVE;
  }

  /*
   * If nothing left to transmit, disable the timer.
   * Else if progress, reset the timer back to 2 ticks.
   */
  if (ri->ri_free == ri->ri_max || (sc->tulip_flags & TULIP_TXPROBE_ACTIVE))
    sc->tulip_txtimer = 0;
  else if (xmits > 0) 
    sc->tulip_txtimer = TULIP_TXTIMER;
  
  sc->tulip_if.if_opackets += xmits;
  return descs;
}


  
void
tulip_print_abnormal_interrupt (tulip_softc_t * const sc, u_int32_t csr)
{
  const char *const * msgp = tulip_status_bits;
  const char *sep;
  u_int32_t  mask;
  const char thrsh[] = "72|128\0\0\096|256\0\0\0128|512\0\0160|1024\0";

  csr &= (1 << (sizeof(tulip_status_bits)/sizeof(tulip_status_bits[0])))-1;

  printf(TULIP_PRINTF_FMT ": abnormal interrupt:", TULIP_PRINTF_ARGS);

  for (sep = " ", mask = 1; mask <= csr; mask <<= 1, msgp++)
  {
    if ((csr & mask) && *msgp != NULL)
    {
      printf("%s%s", sep, *msgp);
      if (mask == TULIP_STS_TXUNDERFLOW && 
	  (sc->tulip_flags & TULIP_NEWTXTHRESH))
      {
	sc->tulip_flags &= ~TULIP_NEWTXTHRESH;
	if (sc->tulip_cmdmode & TULIP_CMD_STOREFWD)
	  printf(" (switching to store-and-forward mode)");
	
	else
	{
	  printf(" (raising TX threshold to %s)",
	  &thrsh[9 * ((sc->tulip_cmdmode & TULIP_CMD_THRESHOLDCTL) >> 14)]);
	}
      }
      sep = ", ";
    }
  }
  printf("\n");
}


/*
 * interrupt handler - figure out if it is rx or tx interrupt and dispatch the
 * right handler.
 */
void 
tulip_intr_handler (tulip_softc_t * const sc, int *progress_p, int realintr)
{
  TULIP_PERFSTART(intr)
  u_int32_t csr;
  int intr_charged = (realintr) ? 0 : 1;

  dprintf("in tulip_intr_handler: %d\n", get_cpu_id());

  while ((csr = TULIP_CSR_READ(sc, csr_status)) & sc->tulip_intrmask)
  {
    *progress_p = 1;
  
    dprintf("csr read once %x\n",csr);

    /* should do this at the end to avoid extra interrupts */
    TULIP_CSR_WRITE(sc, csr_status, csr);

    if (csr & TULIP_STS_SYSERROR)
    {
      dprintf("sys error\n");
      sc->tulip_last_system_error = 
	(csr & TULIP_STS_ERRORMASK) >> TULIP_STS_ERR_SHIFT;
      
      if (sc->tulip_flags & TULIP_NOMESSAGES)
	sc->tulip_flags |= TULIP_SYSTEMERROR;
      
      else
      {
	printf(TULIP_PRINTF_FMT ": system error: %s\n",
	       TULIP_PRINTF_ARGS,
	       tulip_system_errors[sc->tulip_last_system_error]);
      }

      sc->tulip_flags |= TULIP_NEEDRESET;
      sc->tulip_system_errors++;
      break;
    }

    if (csr & (TULIP_STS_LINKPASS | TULIP_STS_LINKFAIL))
    {
      dprintf("link pass/fail\n");

      if (sc->tulip_boardsw->bd_media_poll != NULL)
      {
	(*sc->tulip_boardsw->bd_media_poll) (sc, csr & TULIP_STS_LINKFAIL
					     ? TULIP_MEDIAPOLL_LINKFAIL
					     : TULIP_MEDIAPOLL_LINKPASS);
	csr &= ~TULIP_STS_ABNRMLINTR;
      }
      tulip_media_print(sc);
    }

    if (csr & (TULIP_STS_RXINTR | TULIP_STS_RXNOBUF))
    {
      u_int32_t misses = TULIP_CSR_READ(sc, csr_missed_frames);

      if (csr & TULIP_STS_RXNOBUF)
      {
	dprintf("rxnobuf\n");
	sc->tulip_dot3stats.dot3StatsMissedFrames += misses & 0xFFFF;
      }
      else dprintf("rxintr\n");

      /*
       * Pass 2.[012] of the 21140A-A[CDE] may hang and/or corrupt data
       * on receive overflows.
       */
      if ((misses & 0x0FFE0000) && 
	  (sc->tulip_features & TULIP_HAVE_RXBADOVRFLW))
      {
	dprintf("dealing with missed frames\n");
	sc->tulip_dot3stats.dot3StatsInternalMacReceiveErrors++;
	/*
         * Stop the receiver process and spin until it's stopped.
         * Tell rx_intr to drop the packets it dequeues.
         */
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode & ~TULIP_CMD_RXRUN);
	while ((TULIP_CSR_READ(sc, csr_status) & TULIP_STS_RXSTOPPED) == 0);
	TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
	sc->tulip_flags |= TULIP_RXIGNORE;
      }

      if (intr_charged == 0)
      {
	sc->xoknet->rxintrs++;
	intr_charged = 1;
      }

      tulip_rx_intr(sc);
      if (sc->tulip_flags & TULIP_RXIGNORE)
      {
	dprintf("restart receiver\n");
	/*
         * Restart the receiver.
         */
	sc->tulip_flags &= ~TULIP_RXIGNORE;
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
      }
    }

    if (csr & TULIP_STS_ABNRMLINTR)
    {
      u_int32_t tmp = csr & sc->tulip_intrmask 
	                  & ~(TULIP_STS_NORMALINTR | TULIP_STS_ABNRMLINTR);
      dprintf("abnrmlintr\n");

      if (csr & TULIP_STS_TXUNDERFLOW)
      {
	if ((sc->tulip_cmdmode & TULIP_CMD_THRESHOLDCTL) 
	    != TULIP_CMD_THRSHLD160)
	{
	  sc->tulip_cmdmode += TULIP_CMD_THRSHLD96;
	  sc->tulip_flags |= TULIP_NEWTXTHRESH;
	} 
	
	else if (sc->tulip_features & TULIP_HAVE_STOREFWD)
	{
	  sc->tulip_cmdmode |= TULIP_CMD_STOREFWD;
	  sc->tulip_flags |= TULIP_NEWTXTHRESH;
	}
      }

      if (sc->tulip_flags & TULIP_NOMESSAGES)
      {
	sc->tulip_statusbits |= tmp;
      } 
      
      else
      {
	tulip_print_abnormal_interrupt(sc, tmp);
	sc->tulip_flags |= TULIP_NOMESSAGES;
      }
      TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
    }

#if 0 
    /* benjie: not sure what the WANTTXSTART is for... but we don't use it */
    if (sc->tulip_flags & (TULIP_WANTTXSTART|TULIP_TXPROBE_ACTIVE|
	                   TULIP_DOINGSETUP | TULIP_PROMISC))
#else
    if ((sc->tulip_flags&(TULIP_TXPROBE_ACTIVE|TULIP_DOINGSETUP|TULIP_PROMISC))
	|| (csr & (TULIP_STS_TXINTR|TULIP_STS_TXNOBUF|TULIP_STS_TXSTOPPED)))
#endif
    {
      dprintf("tx stuff\n");
      tulip_tx_intr(sc);
      if (intr_charged == 0)
      {
	sc->xoknet->txintrs++;
	intr_charged = 1;
      }

      if ((sc->tulip_flags & 
	    (TULIP_WANTSETUP|TULIP_TXPROBE_ACTIVE)) == TULIP_WANTSETUP)
	tulip_ifstart(&sc->tulip_if);
    }
  }
 
  if (intr_charged == 0)
    sc->xoknet->otherintrs++;
  
  if (sc->tulip_flags & TULIP_NEEDRESET)
  {
    tulip_reset(sc);
    tulip_init(sc);
  }
}


/*
 * interrupt entry point for a specific irq: find the card that caused the
 * interrupt and call the interrupt handler. this is a level triggered
 * interrupt, it is masked by the kernel, so no need for synchronization.
 */
int 
tulip_intr (u_int irq)   
{
    struct network *xoknet;
    int progress = 0;
    int intrmatched = 0;
    int i;

    for (i=0; i< SYSINFO_GET(si_nnetworks); i++) 
    {
       xoknet = SYSINFO_PTR_AT(si_networks, i); // &si->si_networks[i];
       
       if (xoknet->irq == irq) 
       {
#ifdef __SMP__
  	 extern int irq2trigger[];
#endif
         intrmatched = 1;
#ifdef __SMP__
         if (irq2trigger[irq] != TRIGGER_LEVEL)
	 {
           test_and_set((unsigned long)&(xoknet->intr_pending));
           while (xoknet->intr_pending > 0 &&
               test_and_set((unsigned long)&(xoknet->in_intr)) == 0)
           {
             while(xoknet->intr_pending > 0)
             {
#endif
               xoknet->intrs++;
               xoknet->intr_pending = 0;
  
               ddprintf ("tulip_intr: irq %d cpu %d\n",irq,get_cpu_id());
  
               if (xoknet->cardtype == XOKNET_DE)
               {
                 tulip_intr_handler(xoknet->cardstruct,
                                    &progress,
                                    (xoknet->irq == irq));
               }
#ifdef __SMP__
             }
             xoknet->in_intr = 0;
           }
	 } else {
    
	   ddprintf ("tulip_intr: irq %d cpu %d %qd\n",irq,get_cpu_id(),rdtsc());
	 
	   xoknet->intrs++;
          
	   if (xoknet->cardtype == XOKNET_DE) 
	   {
	     tulip_intr_handler(xoknet->cardstruct, 
		                &progress, 
			        (xoknet->irq == irq));
	   }
	 }
#endif
       }
    }

     
    if (intrmatched == 0)
       printf ("tulip_intr: unmatched IRQ %d\n", irq);

#if 0
    else 
    {
      while(tulip_pending_pkts != 0)
      {
	struct xokpkt *pkt;

dequeue_retry:
	pkt = tulip_pending_pkts;

	if (pkt != 0)
	{
	  if (compare_and_swap((int*)&(tulip_pending_pkts),
	                       (int)pkt,
			       (int)pkt->freeArg) != (int)pkt)
	    goto dequeue_retry;
        
          pkt->freeArg = 0L;
	  dprintf("handling pkt %p on pending list\n", pkt);
	  xokpkt_recv(pkt);
	}
      }
    }
#endif

#ifdef ASH_ENABLE
    else 
      xokpkt_consume();
#endif /* ASH_ENABLE */
    return 0;
}


/*
 * The de interface's send interface for transmitting over a tulip card The
 * three parameters are the card structure (multiple tulip cards can be
 * present), an ae_recv strcuture identifying the data to be transmitted and
 * an integer that tells the driver whether or not a distinct interrupt is
 * wanted at transmission completion time.  Not having the interrupt involves
 * the danger of not being informed of transmission completion for an
 * unbounded amount of time (when the next packet arrives or transmission
 * interrupt occurs).  This can be important because the card will DMA the
 * data from the specified memory locations at transmission time -- so the
 * caller should treat it as inviolate.
 */

int
xok_de_xmit(tulip_softc_t * sc, struct ae_recv * recv, int xmitintr)
{
  tulip_desc_t *eop, *nextout;
  int segcnt, free, i;
  tulip_uint32_t d_status;
  tulip_ringinfo_t *const ri = &sc->tulip_txinfo;
  int ret;

  if (sc->xoknet->inited == 0)
  {
    printf("sys_de_xmit: card/link %d not yet inited\n", sc->tulip_unit);
    return (-1);
  }

  if (ri->ri_free < 3)
  {
    printf("sys_de_xmit: dangerously short on send buffers (only %d)\n", 
	   ri->ri_free);
    return (-1);
  }

  /*
   * Now we try to fill in our transmit descriptors. This is a bit reminiscent
   * of going on the Ark two by two since each descriptor for the TULIP can
   * describe two buffers. So we advance through packet filling each of the
   * two entries at a time to to fill each descriptor. Clear the first and
   * last segment bits in each descriptor (actually just clear everything but
   * the end-of-ring or chain bits) to make sure we don't get messed up by
   * previously sent packets.
   *
   * We may fail to put the entire packet on the ring if there is either not
   * enough ring entries free or if the packet has more than MAX_TXSEG
   * segments. In the former case we will just wait for the ring to empty.
   * In the latter case we have to recopy.
   */
  
  d_status = 0;
  eop = nextout = ri->ri_nextout;
  eop->d_flag &= ~TULIP_DFLAG_TxWANTINTR;
  segcnt = 0;
  free = ri->ri_free;
  
  for (i = 0; i < recv->n; i++)
  {
    int len = recv->r[i].sz;
    caddr_t addr = recv->r[i].data;
    unsigned pagebound = NBPG - (((u_long) addr) & (NBPG - 1));

    while (len > 0)
    {
      unsigned slen = min(len, pagebound);

      segcnt++;
      if (segcnt > TULIP_MAX_TXSEG)
      {
	/*
	 * The packet exceeds the number of transmit buffer entries that we
	 * can use for one packet, so caller has recopy it into fewer gather
	 * units and then try again. (Note: This really should never happen in
	 * practice)
         */
	
	/*
         * Make sure the next descriptor after this packet is owned
         * by us since it may have been set up during the looping.
         */
	nextout->d_status = 0;
	printf("de_xmit: too many gather entries (%d)\n", segcnt);
	ret = -2;
	goto sys_de_xmit_cleanup;
      }

      if (segcnt & 1)
      {
	if (--free == 0)
	{
	  /*
           * There's no more room but since
           * nothing                   has been
           * committed at this point, just
           * leave the recv and return.
           */
	  ret = -1;
	  segcnt++;
	  printf("de_xmit: no more free out segments (segcnt %d)\n", segcnt);
	  goto sys_de_xmit_cleanup;
	}
	eop = nextout;

	if (++nextout == ri->ri_last)
	  nextout = ri->ri_first;
	eop->d_flag &= 
	  TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN|TULIP_DFLAG_TxWANTINTR;
	eop->d_status = d_status;
	eop->d_addr1 = kva2pa(addr);
	eop->d_length1 = slen;
      } 
      
      else
      {
	/* Fill in second half of descriptor */
	eop->d_addr2 = kva2pa(addr);
	eop->d_length2 = slen;
      }

      d_status = TULIP_DSTS_OWNER;
      len -= slen;
      addr += slen;
      pagebound = NBPG;
    }
  }

  /*
   * The descriptors have been filled in.  Now get ready to transmit.
   */

  /*
   * If we only used the first segment of the last descriptor,
   * make sure the second segment will not be used.
   */
  if (segcnt & 1)
  {
    eop->d_addr2 = 0;
    eop->d_length2 = 0;
  }
  
  /*
   * Mark the last and first segments, indicate we want a transmit
   * complete interrupt, give the descriptors to the TULIP, and tell
   * it to transmit!
   */
  eop->d_flag |= TULIP_DFLAG_TxLASTSEG;
  if (xmitintr != 0)
    eop->d_flag |= TULIP_DFLAG_TxWANTINTR;

  /*
   * Note that ri->ri_nextout is still the start of the packet and
   * until we set the OWNER bit, we can still back out of everything we
   * have done.
   */
  ri->ri_nextout->d_flag |= TULIP_DFLAG_TxFIRSTSEG;
  ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
  
  /*
   * Save the result location for latter usage.
   */
  sc->tulip_txdones[(eop - ri->ri_first)] = (struct xokpkt *) recv;

  /*
   * This advances the ring for us.
   */
  ri->ri_nextout = nextout;
  ri->ri_free = free;

  TULIP_CSR_WRITE(sc, csr_txpoll, 1);

  if (sc->tulip_txtimer == 0)
    sc->tulip_txtimer = TULIP_TXTIMER;

  return (0);

sys_de_xmit_cleanup:		/* decrement faulty DMA counts */
  eop = nextout = ri->ri_nextout;
  for (i = 0; i < (segcnt - 1); i++)
  {
    if ((i & 1) == 0)
    {
      eop = nextout;
      if (++nextout == ri->ri_last)
      {
	nextout = ri->ri_first;
      }
      eop->d_addr1 = 0;
    } else
    {
      eop->d_addr2 = 0;
    }
  }
  printf("de_xmit: failed to send\n");
  return (ret);
}


