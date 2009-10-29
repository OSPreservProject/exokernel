
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


/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (C) 1993, David Greenman.
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright
 * and these terms are retained.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 */

/* #define ED_DEBUG 1 /* */

/* #define LB			/* Code for lower-bound roundtrip times */

#include <xok/defs.h>
#include <kern/extern.h>
#include <xok/mmu.h>
#include <xok/pio.h>
#include <xok/trap.h>
#include <xok/sysinfo.h>
#include <xok/sys_proto.h>
#include <dev/dp8390reg.h>
#include <dev/smcreg.h>
#include <xok/pkt.h>

static u_int baseregs[] = {0x280, 0x300};
#define nbaseregs (sizeof (baseregs) / sizeof (int))
static u_int shmaddrs[nbaseregs] = {0xd0000, 0xcc000};

#define ETHER_MIN_LEN 64
#define ETHER_MAX_LEN 1514
#define IRQUNK -1

const int ed_wd584_irq[] = { 9, 3, 5, 7, 10, 11, 15, 4 };
const int ed_wd790_irq[] = { IRQUNK, 9, 3, 5, 7, 10, 11, 15 };

#define eds si->si_smc
#define lasted si->si_nsmc

void edintr (u_int);
void ed_start (struct smc_softc *);
void ed_rint (struct smc_softc *);

#define	NIC_PUT(sc, off, val)	outb(sc->nic_addr + off, val)
#define	NIC_GET(sc, off)	inb(sc->nic_addr + off)

/*
 * Probe and vendor-specific initialization routine for SMC/WD80x3 boards.
 */
static int
ed_probe(struct smc_softc *sc)
{
  int i;
  u_int memsize;
  u_char iptr, isa16bit, sum;

  sc->nic_addr = sc->asic_addr + ED_WD_NIC_OFFSET;
  sc->is790 = 0;

  /*
   * Attempt to do a checksum over the station address PROM.  If it
   * fails, it's probably not a SMC/WD board.  There is a problem with
   * this, though: some clone WD boards don't pass the checksum test.
   * Danpex boards for one.
   */
  for (sum = 0, i = 0; i < 8; ++i)
    sum += inb(sc->asic_addr + ED_WD_PROM + i);

  if (sum != ED_WD_ROM_CHECKSUM_TOTAL) {
    /*
     * Checksum is invalid.  This often happens with cheap WD8003E
     * clones.  In this case, the checksum byte (the eighth byte)
     * seems to always be zero.
     */
    if (inb(sc->asic_addr + ED_WD_CARD_ID) != ED_TYPE_WD8003E ||
	inb(sc->asic_addr + ED_WD_PROM + 7) != 0)
      return (0);
  }

  /* Reset card to force it into a known state. */
  outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_RST);
  delay(100);
  outb(sc->asic_addr + ED_WD_MSR,
       inb(sc->asic_addr + ED_WD_MSR) & ~ED_WD_MSR_RST);
  /* Wait in the case this card is reading it's EEROM. */
  delay(5000);

  sc->vendor = ED_VENDOR_WD_SMC;
  sc->type = inb(sc->asic_addr + ED_WD_CARD_ID);

  /* Set initial values for width/size. */
  memsize = 8192;
  isa16bit = 0;
  switch (sc->type) {
  case ED_TYPE_WD8003S:
    sc->type_str = "WD8003S";
    break;
  case ED_TYPE_WD8003E:
    sc->type_str = "WD8003E";
    break;
  case ED_TYPE_WD8003EB:
    sc->type_str = "WD8003EB";
    break;
  case ED_TYPE_WD8003W:
    sc->type_str = "WD8003W";
    break;
  case ED_TYPE_WD8013EBT:
    sc->type_str = "WD8013EBT";
    memsize = 16384;
    isa16bit = 1;
    break;
  case ED_TYPE_WD8013W:
    sc->type_str = "WD8013W";
    memsize = 16384;
    isa16bit = 1;
    break;
  case ED_TYPE_WD8013EP:		/* also WD8003EP */
    if (inb(sc->asic_addr + ED_WD_ICR) & ED_WD_ICR_16BIT) {
      isa16bit = 1;
      memsize = 16384;
      sc->type_str = "WD8013EP";
    } else
      sc->type_str = "WD8003EP";
    break;
  case ED_TYPE_WD8013WC:
    sc->type_str = "WD8013WC";
    memsize = 16384;
    isa16bit = 1;
    break;
  case ED_TYPE_WD8013EBP:
    sc->type_str = "WD8013EBP";
    memsize = 16384;
    isa16bit = 1;
    break;
  case ED_TYPE_WD8013EPC:
    sc->type_str = "WD8013EPC";
    memsize = 16384;
    isa16bit = 1;
    break;
  case ED_TYPE_SMC8216C:
  case ED_TYPE_SMC8216T:
    sc->type_str = (sc->type == ED_TYPE_SMC8216C) ?
      "SMC8216/SMC8216C" : "SMC8216T";
    outb(sc->asic_addr + ED_WD790_HWR,
	 inb(sc->asic_addr + ED_WD790_HWR) | ED_WD790_HWR_SWH);
    switch (inb(sc->asic_addr + ED_WD790_RAR) & ED_WD790_RAR_SZ64) {
    case ED_WD790_RAR_SZ64:
      memsize = 65536;
      break;
    case ED_WD790_RAR_SZ32:
      memsize = 32768;
      break;
    case ED_WD790_RAR_SZ16:
      memsize = 16384;
      break;
    case ED_WD790_RAR_SZ8:
      /* 8216 has 16K shared mem -- 8416 has 8K */
      sc->type_str = (sc->type == ED_TYPE_SMC8216C) ?
	"SMC8416C/SMC8416BT" : "SMC8416T";
      memsize = 8192;
      break;
    }
    outb(sc->asic_addr + ED_WD790_HWR,
	 inb(sc->asic_addr + ED_WD790_HWR) & ~ED_WD790_HWR_SWH);

    isa16bit = 1;
    sc->is790 = 1;
    break;
  default:
    sc->type_str = NULL;
    break;
  }
  /*
   * Make some adjustments to initial values depending on what is found
   * in the ICR.
   */
  if (isa16bit && (sc->type != ED_TYPE_WD8013EBT) &&
      ((inb(sc->asic_addr + ED_WD_ICR) & ED_WD_ICR_16BIT) == 0)) {
    isa16bit = 0;
    memsize = 8192;
  }

#ifdef ED_DEBUG
  printf("type=%x type_str=%s isa16bit=%d memsize=%d\n",
	 sc->type, sc->type_str ?: "unknown", isa16bit, memsize);
  for (i = 0; i < 8; i++)
    printf("%x -> %x\n", i, inb(sc->asic_addr + i));
#endif

  /*
   * Get the assigned interrupt number from the card and use it.
   */
  if (sc->is790) {
    u_char x;
    /* Assemble together the encoded interrupt number. */
    outb(sc->asic_addr + ED_WD790_HWR,
	 inb(sc->asic_addr + ED_WD790_HWR) | ED_WD790_HWR_SWH);
    x = inb(sc->asic_addr + ED_WD790_GCR);
    iptr = ((x & ED_WD790_GCR_IR2) >> 4) |
      ((x & (ED_WD790_GCR_IR1|ED_WD790_GCR_IR0)) >> 2);
    outb(sc->asic_addr + ED_WD790_HWR,
	 inb(sc->asic_addr + ED_WD790_HWR) & ~ED_WD790_HWR_SWH);
    /*
     * Translate it using translation table, and check for
     * correctness.
     */
    sc->irq = ed_wd790_irq[iptr];
    /* Enable the interrupt. */
    outb(sc->asic_addr + ED_WD790_ICR,
	 inb(sc->asic_addr + ED_WD790_ICR) | ED_WD790_ICR_EIL);
  } else if (sc->type & ED_WD_SOFTCONFIG) {
    /* Assemble together the encoded interrupt number. */
    iptr = (inb(sc->asic_addr + ED_WD_ICR) & ED_WD_ICR_IR2) |
      ((inb(sc->asic_addr + ED_WD_IRR) &
	(ED_WD_IRR_IR0 | ED_WD_IRR_IR1)) >> 5);
    /*
     * Translate it using translation table, and check for
     * correctness.
     */
    sc->irq = ed_wd584_irq[iptr];
    /* Enable the interrupt. */
    outb(sc->asic_addr + ED_WD_IRR,
	 inb(sc->asic_addr + ED_WD_IRR) | ED_WD_IRR_IEN);
  } else {
    printf("ed: device does not have soft configuration\n");
    return (0);
  }

  sc->isa16bit = isa16bit;
  sc->mem_shared = 1;

  /* Allocate one xmit buffer if < 16k, two buffers otherwise. */
  if (memsize < 16384)
    sc->txb_cnt = 1;
  else
    sc->txb_cnt = 2;

  sc->tx_page_start = ED_WD_PAGE_OFFSET;
  sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
  sc->rec_page_stop = sc->tx_page_start + (memsize >> ED_PAGE_SHIFT);
  sc->mem_ring = sc->mem_start + (sc->rec_page_start << ED_PAGE_SHIFT);
  sc->mem_size = memsize;
  sc->mem_end = sc->mem_start + memsize;

  /* Get station address from on-board ROM. */
  for (i = 0; i < ETHER_ADDR_LEN; ++i)
    sc->macaddr[i] = inb(sc->asic_addr + ED_WD_PROM + i);

  /*
   * Set upper address bits and 8/16 bit access to shared memory.
   */
  if (isa16bit) {
    if (sc->is790) {
      sc->wd_laar_proto =
	inb(sc->asic_addr + ED_WD_LAAR) &
	~ED_WD_LAAR_M16EN;
    } else {
      sc->wd_laar_proto =
	ED_WD_LAAR_L16EN |
	((kva2pa(sc->mem_start) >> 19) &
	 ED_WD_LAAR_ADDRHI);
    }
    outb(sc->asic_addr + ED_WD_LAAR,
	 sc->wd_laar_proto | ED_WD_LAAR_M16EN);
  } else  {
    if ((sc->type & ED_WD_SOFTCONFIG) ||
	(sc->type == ED_TYPE_WD8013EBT) && !sc->is790) {
      sc->wd_laar_proto =
	((kva2pa(sc->mem_start) >> 19) &
	 ED_WD_LAAR_ADDRHI);
      outb(sc->asic_addr + ED_WD_LAAR,
	   sc->wd_laar_proto);
    }
  }

  /*
   * Set address and enable interface shared memory.
   */
  if (!sc->is790) {
    sc->wd_msr_proto =
      (kva2pa(sc->mem_start) >> 13) & ED_WD_MSR_ADDR;
    sc->cr_proto = ED_CR_RD2;
  } else {
    outb(sc->asic_addr + 0x04,
	 inb(sc->asic_addr + 0x04) | 0x80);
    outb(sc->asic_addr + 0x0b,
	 ((kva2pa(sc->mem_start) >> 13) & 0x0f) |
	 ((kva2pa(sc->mem_start) >> 11) & 0x40) |
	 (inb(sc->asic_addr + 0x0b) & 0xb0));
    outb(sc->asic_addr + 0x04,
	 inb(sc->asic_addr + 0x04) & ~0x80);
    sc->wd_msr_proto = 0x00;
    sc->cr_proto = 0;
  }
  outb(sc->asic_addr + ED_WD_MSR,
       sc->wd_msr_proto | ED_WD_MSR_MENB);

  (void) inb(0x84);
  (void) inb(0x84);

  /* Now zero memory and verify that it is clear. */
  bzero(sc->mem_start, memsize);

  for (i = 0; i < memsize; ++i)
    if (sc->mem_start[i]) {
      printf("ed: failed to clear shared memory at %lx "
	     "- check configuration\n", kva2pa(sc->mem_start + i));

      /* Disable 16 bit access to shared memory. */
      outb(sc->asic_addr + ED_WD_MSR,
	   sc->wd_msr_proto);
      if (isa16bit)
	outb(sc->asic_addr + ED_WD_LAAR,
	     sc->wd_laar_proto);
      (void) inb(0x84);
      (void) inb(0x84);
      return (0);
    }

#if 0
  /*
   * Disable 16bit access to shared memory - we leave it disabled
   * so that 1) machines reboot properly when the board is set 16
   * 16 bit mode and there are conflicting 8bit devices/ROMS in
   * the same 128k address space as this boards shared memory,
   * and 2) so that other 8 bit devices with shared memory can be
   * used in this 128k region, too.
   */
  outb(sc->asic_addr + ED_WD_MSR, sc->wd_msr_proto);
  if (isa16bit)
    outb(sc->asic_addr + ED_WD_LAAR, sc->wd_laar_proto);
  (void) inb(0x84);
  (void) inb(0x84);
#endif

  irq_sethandler (sc->irq, (int (*)(u_int))edintr);
  irq_setmask_8259A (irq_mask_8259A & ~(1 << sc->irq));
  irq_eoi (sc->irq);
  return (1);
}

/*
 * Initialize device.
 */
static void
ed_setup (struct smc_softc *sc)
{
#if 0
  struct ifnet *ifp = &sc->sc_arpcom.ac_if;
#endif
  int i;
  u_long mcaf[2];

  /*
   * Initialize the NIC in the exact order outlined in the NS manual.
   * This init procedure is "mandatory"...don't change what or when
   * things happen.
   */

#if 0
  /* Reset transmitter flags. */
  ifp->if_timer = 0;
#endif

  sc->tx_active = 0;
  sc->txb_inuse = 0;
  sc->txb_new = 0;
  sc->txb_next_tx = 0;

  sc->nintr = 0;
  sc->xmits = 0;
  sc->rcvs = 0;
  sc->discards = 0;

  /* Set interface for page 0, remote DMA complete, stopped. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

  if (sc->isa16bit) {
    /*
     * Set FIFO threshold to 8, No auto-init Remote DMA, byte
     * order=80x86, word-wide DMA xfers,
     */
    NIC_PUT(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
  } else {
    /* Same as above, but byte-wide DMA xfers. */
    NIC_PUT(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
  }

  /* Clear remote byte count registers. */
  NIC_PUT(sc, ED_P0_RBCR0, 0);
  NIC_PUT(sc, ED_P0_RBCR1, 0);

  /* Tell RCR to do nothing for now. */
  NIC_PUT(sc, ED_P0_RCR, ED_RCR_MON);

  /* Place NIC in internal loopback mode. */
  NIC_PUT(sc, ED_P0_TCR, ED_TCR_LB0);

  /* Set lower bits of byte addressable framing to 0. */
  if (sc->is790)
    NIC_PUT(sc, 0x09, 0);

  /* Initialize receive buffer ring. */
  NIC_PUT(sc, ED_P0_BNRY, sc->rec_page_start);
  NIC_PUT(sc, ED_P0_PSTART, sc->rec_page_start);
  NIC_PUT(sc, ED_P0_PSTOP, sc->rec_page_stop);

  /*
   * Clear all interrupts.  A '1' in each bit position clears the
   * corresponding flag.
   */
  NIC_PUT(sc, ED_P0_ISR, 0xff);

  /*
   * Enable the following interrupts: receive/transmit complete,
   * receive/transmit error, and Receiver OverWrite.
   *
   * Counter overflow and Remote DMA complete are *not* enabled.
   */
  NIC_PUT(sc, ED_P0_IMR,
	  ED_IMR_PRXE | ED_IMR_PTXE | ED_IMR_RXEE | ED_IMR_TXEE |
	  ED_IMR_OVWE);

  /* Program command register for page 1. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);

  /* Copy out our station address. */
  for (i = 0; i < ETHER_ADDR_LEN; ++i)
    NIC_PUT(sc, ED_P1_PAR0 + i, sc->macaddr[i]);

  /* Set multicast filter on chip. */
#if 0
  ed_getmcaf(&sc->sc_arpcom, mcaf);
#else
  bzero (mcaf, sizeof (mcaf));
  mcaf[0] = mcaf[1] = 0xffffffff;
#endif
  for (i = 0; i < 8; i++)
    NIC_PUT(sc, ED_P1_MAR0 + i, ((u_char *)mcaf)[i]);

  /*
   * Set current page pointer to one page after the boundary pointer, as
   * recommended in the National manual.
   */
  sc->next_packet = sc->rec_page_start + 1;
  NIC_PUT(sc, ED_P1_CURR, sc->next_packet);

  /* Program command register for page 0. */
  NIC_PUT(sc, ED_P1_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

  i = ED_RCR_AB | ED_RCR_AM;
#if 0
  if (ifp->if_flags & IFF_PROMISC) {
    /*
     * Set promiscuous mode.  Multicast filter was set earlier so
     * that we should receive all multicast packets.
     */
    i |= ED_RCR_PRO | ED_RCR_AR | ED_RCR_SEP;
  }
#endif
  NIC_PUT(sc, ED_P0_RCR, i);

  /* Take interface out of loopback. */
  NIC_PUT(sc, ED_P0_TCR, 0);

#if 0
  /*
   * If this is a 3Com board, the tranceiver must be software enabled
   * (there is no settable hardware default).
   */
  switch (sc->vendor) {
    u_char x;
  case ED_VENDOR_3COM:
    if (ifp->if_flags & IFF_LINK0)
      outb(sc->asic_addr + ED_3COM_CR, 0);
    else
      outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);
    break;
  case ED_VENDOR_WD_SMC:
    if ((sc->type & ED_WD_SOFTCONFIG) == 0)
      break;
    x = inb(sc->asic_addr + ED_WD_IRR);
    if (ifp->if_flags & IFF_LINK0)
      x &= ~ED_WD_IRR_OUT2;
    else
      x |= ED_WD_IRR_OUT2;
    outb(sc->asic_addr + ED_WD_IRR, x);
    break;
  }
#endif

  /* Fire up the interface. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

#if 0
  /* Set 'running' flag, and clear output active flag. */
  ifp->if_flags |= IFF_RUNNING;
  ifp->if_flags &= ~IFF_OACTIVE;
#endif

  /* ...and attempt to start output. */
  ed_start(sc);
}

/*
 * Take interface offline.
 */
static void
ed_halt (struct smc_softc *sc)
{
  int n = 5000;

  /* Stop everything on the interface, and select page 0 registers. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

  /*
   * Wait for interface to enter stopped state, but limit # of checks to
   * 'n' (about 5ms).  It shouldn't even take 5us on modern DS8390's, but
   * just in case it's an old one.
   */
  while (((NIC_GET(sc, ED_P0_ISR) & ED_ISR_RST) == 0) && --n);
}

static inline void
ed_xmit (struct smc_softc *sc)
{
  u_short len;

  len = sc->txb_len[sc->txb_next_tx];

  /* Set NIC for page 0 register access. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

  /* Set TX buffer start page. */
  NIC_PUT(sc, ED_P0_TPSR, sc->tx_page_start +
	  sc->txb_next_tx * ED_TXBUF_SIZE);

  /* Set TX length. */
  NIC_PUT(sc, ED_P0_TBCR0, len);
  NIC_PUT(sc, ED_P0_TBCR1, len >> 8);

  /* Set page 0, remote DMA complete, transmit packet, and *start*. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_TXP | ED_CR_STA);

  /* Point to next transmit buffer slot and wrap if necessary. */
  sc->txb_next_tx++;
  if (sc->txb_next_tx == sc->txb_cnt)
    sc->txb_next_tx = 0;
  sc->tx_active = 1;

#if 0
  /* Set a timer just in case we never hear from the board again. */
  ifp->if_timer = 2;
#endif
}

/*
 * Given a source and destination address, copy 'amount' of a packet from the
 * ring buffer into a linear destination buffer.  Takes into account ring-wrap.
 */
static inline char *
ed_ring_copy(struct smc_softc *sc, u_char *src, u_char *dst, u_short amount)
{
  u_short tmp_amount;

  /* Does copy wrap to lower addr in ring buffer? */
  if (src + amount > sc->mem_end) {
    tmp_amount = sc->mem_end - src;

    /* Copy amount up to end of NIC memory. */
    bcopy(src, dst, tmp_amount);

    amount -= tmp_amount;
    src = sc->mem_ring;
    dst += tmp_amount;
  }
  bcopy(src, dst, amount);
  return (src + amount);
}


static inline void
ed_read (struct smc_softc *sc, struct ed_ring *pkt_hdr, u_char *pkt, int len)
{
    u_char *pb;
    struct xokpkt *xokpkt;

    sc->rcvs++;
#ifdef LB
    {
    int edlb(char *p, int len, int cardno);
    if (!edlb(pkt, len, 0)) {
       xokpkt = xokpkt_alloc(len, &sc->discards);
       ed_ring_copy (sc, pkt, xokpkt->data, len);
       xokpkt_recv (xokpkt);
    }
    }
#else
    xokpkt = xokpkt_alloc(len, &sc->discards);
    ed_ring_copy (sc, pkt, xokpkt->data, len);
    xokpkt_recv (xokpkt);
#endif
}


void
ed_rint (struct smc_softc *sc)
{
  u_char boundary, current;
  u_short len;
  u_char nlen;
  struct ed_ring packet_hdr;
  char *packet_ptr;

loop:
  /* Set NIC to page 1 registers to get 'current' pointer. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);

  /*
   * 'sc->next_packet' is the logical beginning of the ring-buffer - i.e.
   * it points to where new data has been buffered.  The 'CURR' (current)
   * register points to the logical end of the ring-buffer - i.e. it
   * points to where additional new data will be added.  We loop here
   * until the logical beginning equals the logical end (or in other
   * words, until the ring-buffer is empty).
   */
  current = NIC_GET(sc, ED_P1_CURR);
  if (sc->next_packet == current)
    return;

  /* Set NIC to page 0 registers to update boundary register. */
  NIC_PUT(sc, ED_P1_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

  do {
    /* Get pointer to this buffer's header structure. */
    packet_ptr = sc->mem_ring +
      ((sc->next_packet - sc->rec_page_start) << ED_PAGE_SHIFT);

    /*
     * The byte count includes a 4 byte header that was added by
     * the NIC.
     */
#if 0
    if (sc->mem_shared)
      packet_hdr = *(struct ed_ring *)packet_ptr;
    else
      ed_pio_readmem(sc, (long)packet_ptr,
		     (caddr_t) &packet_hdr, sizeof(packet_hdr));
#else
    packet_hdr = *(struct ed_ring *)packet_ptr;
#endif
    len = packet_hdr.count;

    /*
     * Try do deal with old, buggy chips that sometimes duplicate
     * the low byte of the length into the high byte.  We do this
     * by simply ignoring the high byte of the length and always
     * recalculating it.
     *
     * NOTE: sc->next_packet is pointing at the current packet.
     */
    if (packet_hdr.next_packet >= sc->next_packet)
      nlen = (packet_hdr.next_packet - sc->next_packet);
    else
      nlen = ((packet_hdr.next_packet - sc->rec_page_start) +
	      (sc->rec_page_stop - sc->next_packet));
    --nlen;
    if ((len & ED_PAGE_MASK) + sizeof(packet_hdr) > ED_PAGE_SIZE)
      --nlen;
    len = (len & ED_PAGE_MASK) | (nlen << ED_PAGE_SHIFT);
#if defined(DIAGNOSTIC) || 1
    if (len != packet_hdr.count) {
      printf("ed_rint: length does not match next packet pointer\n");
      printf("len %04x nlen %04x start %02x first %02x curr %02x next %02x stop %02x\n",
	     (u_int) packet_hdr.count, (u_int) len,
	     sc->rec_page_start, sc->next_packet, current,
	     packet_hdr.next_packet, sc->rec_page_stop);
    }
#endif

    /*
     * Be fairly liberal about what we allow as a "reasonable"
     * length so that a [crufty] packet will make it to BPF (and
     * can thus be analyzed).  Note that all that is really
     * important is that we have a length that will fit into one
     * mbuf cluster or less; the upper layer protocols can then
     * figure out the length from their own length field(s).
     */
    if (len <= (1<<11) &&
	packet_hdr.next_packet >= sc->rec_page_start &&
	packet_hdr.next_packet < sc->rec_page_stop) {
	/* Go get packet. */
	ed_read(sc, &packet_hdr, packet_ptr + sizeof(struct ed_ring),
		min(ETHER_MAX_LEN, len - sizeof(struct ed_ring)));
    } else {
      /* Really BAD.  The ring pointers are corrupted. */
      printf ("ed_rint: NIC corrupt (%d bytes)!\n", (int) len);
      ed_halt (sc);
      ed_setup (sc);
      return;
    }

    /* Update next packet pointer. */
    sc->next_packet = packet_hdr.next_packet;

    /*
     * Update NIC boundary pointer - being careful to keep it one
     * buffer behind (as recommended by NS databook).
     */
    boundary = sc->next_packet - 1;
    if (boundary < sc->rec_page_start)
      boundary = sc->rec_page_stop - 1;
    NIC_PUT(sc, ED_P0_BNRY, boundary);
  } while (sc->next_packet != current);

  goto loop;
}

/* Ethernet interface interrupt processor. */
void
edintr(u_int irq)
{
  struct smc_softc *sc = &eds[0]; /* FIX ME - XXX */
  u_char isr;

  sc->nintr++;

  /* Set NIC to page 0 registers. */
  NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

  /* Loop until there are no more new interrupts. */
  for (;;) {
    isr = NIC_GET(sc, ED_P0_ISR);
    if (!isr)
      return;
    /*
     * Reset all the bits that we are 'acknowledging' by writing a
     * '1' to each bit position that was set.
     * (Writing a '1' *clears* the bit.)
     */
    NIC_PUT(sc, ED_P0_ISR, isr);

    /*
     * Handle transmitter interrupts.  Handle these first because
     * the receiver will reset the board under some conditions.
     */
    if (isr & (ED_ISR_PTX | ED_ISR_TXE)) {
      u_char collisions = NIC_GET(sc, ED_P0_NCR) & 0x0f;

      /*
       * Check for transmit error.  If a TX completed with an
       * error, we end up throwing the packet away.  Really
       * the only error that is possible is excessive
       * collisions, and in this case it is best to allow the
       * automatic mechanisms of TCP to backoff the flow.  Of
       * course, with UDP we're screwed, but this is expected
       * when a network is heavily loaded.
       */
      (void) NIC_GET(sc, ED_P0_TSR);
      if (isr & ED_ISR_TXE) {
	/*
	 * Excessive collisions (16).
	 */
	if ((NIC_GET(sc, ED_P0_TSR) & ED_TSR_ABT) && (collisions == 0)) {
	  /*
	   * When collisions total 16, the P0_NCR
	   * will indicate 0, and the TSR_ABT is
	   * set.
	   */
	  collisions = 16;
	}

#if 0
	/* Update output errors counter. */
	++ifp->if_oerrors;
      } else {
	/*
	 * Update total number of successfully
	 * transmitted packets.
	 */
	++ifp->if_opackets;
#endif
      }

      /* Done with the buffer. */
      sc->tx_active = 0;
      sc->txb_inuse--;

#if 0
      /* Clear watchdog timer. */
      ifp->if_timer = 0;
      ifp->if_flags &= ~IFF_OACTIVE;

      /*
       * Add in total number of collisions on last
       * transmission.
       */
      ifp->if_collisions += collisions;
#endif

      /*
       * Decrement buffer in-use count if not zero (can only
       * be zero if a transmitter interrupt occured while not
       * actually transmitting).
       * If data is ready to transmit, start it transmitting,
       * otherwise defer until after handling receiver.
       */
      if (sc->txb_inuse > 0)
	ed_xmit(sc);
    }

    /* Handle receiver interrupts. */
    if (isr & (ED_ISR_PRX | ED_ISR_RXE | ED_ISR_OVW)) {
      /*
       * Overwrite warning.  In order to make sure that a
       * lockup of the local DMA hasn't occurred, we reset
       * and re-init the NIC.  The NSC manual suggests only a
       * partial reset/re-init is necessary - but some chips
       * seem to want more.  The DMA lockup has been seen
       * only with early rev chips - Methinks this bug was
       * fixed in later revs.  -DG
       */
      if (isr & ED_ISR_OVW) {
#if 0
	++ifp->if_ierrors;
#endif
#ifdef DIAGNOSTIC
	log(LOG_WARNING,
	    "%s: warning - receiver ring buffer overrun\n",
	    sc->sc_dev.dv_xname);
#endif
	/* Stop/reset/re-init NIC. */
	printf ("edintr: umm... reseting board or something\n");
	ed_halt (sc);
	ed_setup (sc);
      } else {
	/*
	 * Receiver Error.  One or more of: CRC error,
	 * frame alignment error FIFO overrun, or
	 * missed packet.
	 */
	if (isr & ED_ISR_RXE) {
#if 0
	  ++ifp->if_ierrors;
#endif
#ifdef ED_DEBUG
	  printf("%s: receive error %x\n",
		 sc->sc_dev.dv_xname,
		 NIC_GET(sc, ED_P0_RSR));
#endif
	}

	/*
	 * Go get the packet(s).
	 * XXX - Doing this on an error is dubious
	 * because there shouldn't be any data to get
	 * (we've configured the interface to not
	 * accept packets with errors).
	 */

#if 0
	/*
	 * Enable 16bit access to shared memory first
	 * on WD/SMC boards.
	 */
	if (sc->vendor == ED_VENDOR_WD_SMC) {
	  if (sc->isa16bit)
	    outb (sc->asic_addr + ED_WD_LAAR,
		  sc->wd_laar_proto | ED_WD_LAAR_M16EN);
	  outb (sc->asic_addr + ED_WD_MSR,
		sc->wd_msr_proto | ED_WD_MSR_MENB);
	  (void) inb(0x84);
	  (void) inb(0x84);
	}
#endif

	ed_rint(sc);

#if 0
	/* Disable 16-bit access. */
	if (sc->vendor == ED_VENDOR_WD_SMC) {
	  outb (sc->asic_addr + ED_WD_MSR, sc->wd_msr_proto);
	  if (sc->isa16bit)
	    outb (sc->asic_addr + ED_WD_LAAR, sc->wd_laar_proto);
	  /* XXX */
	  inb (0x84);
	  inb (0x84);
	}
#endif
      }
    }

    /*
     * If it looks like the transmitter can take more data,	attempt
     * to start output on the interface.  This is done after
     * handling the receiver to give the receiver priority.
     */
    ed_start(sc);

    /*
     * Return NIC CR to standard state: page 0, remote DMA
     * complete, start (toggling the TXP bit off, even if was just
     * set in the transmit routine, is *okay* - it is 'edge'
     * triggered from low to high).
     */
    NIC_PUT(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

    /*
     * If the Network Talley Counters overflow, read them to reset
     * them.  It appears that old 8390's won't clear the ISR flag
     * otherwise - resulting in an infinite loop.
     */
    if (isr & ED_ISR_CNT) {
      (void) NIC_GET(sc, ED_P0_CNTR0);
      (void) NIC_GET(sc, ED_P0_CNTR1);
      (void) NIC_GET(sc, ED_P0_CNTR2);
    }

    isr = NIC_GET(sc, ED_P0_ISR);
    if (!isr)
      break;
  }
  /* 
   * Kernel debugging needs to poll the card  b/c during
   * kernel debugging interrupts are disabled.  If this function is
   * called from kernel debugging the argument (irq) is 0.  This will
   * never be the case if this function is called as the result of an
   * interrupt (b/c the clock intr uses IRQ 0).
   */

#ifdef KDEBUG
  if (irq != 0)
#endif
    xokpkt_consume ();
}

void
ed_start (struct smc_softc *sc)
{
  if (!sc->tx_active && sc->txb_inuse > 0)
    ed_xmit (sc);
  if (sc->txb_inuse == sc->txb_cnt)
    gdt[GD_ED0>>3] = mmu_seg_fault;
  else
    gdt[GD_ED0>>3] = smallseg (STA_W, (ASHDVM + kva2pa (sc->mem_start)
				       + ((sc->txb_new * ED_TXBUF_SIZE)
					  << ED_PAGE_SHIFT)),
			       (ED_TXBUF_SIZE << ED_PAGE_SHIFT) - 1, 3);

  /* asm volatile ("lgdt %P0" :: "i" (PD_ADDR (gdt_pd))); /* XXX */
}

int
sys_ed0xmit (u_int sn, u_int len)
{
  struct smc_softc *sc = &eds[0];

  if (! gdt[GD_ED0>>3].sd_p || len > ETHER_MAX_LEN)
    return (-1);
  sc->txb_len[sc->txb_new] = max (len, ETHER_MIN_LEN);
  /* Point to next buffer slot and wrap if necessary. */
  if (++sc->txb_new == sc->txb_cnt)
    sc->txb_new = 0;
  sc->txb_inuse++;
  sc->xmits++;
  ed_start (sc);
  return (0);
}

void
edinit (void)
{
  int i;

  lasted = 0;
  for (i = 0; i < nbaseregs && lasted < MAXSMC; i++) {
    eds[lasted].asic_addr = baseregs[i];
    eds[lasted].mem_start = ptov(shmaddrs[lasted]);
    if (ed_probe (&eds[lasted])) {
      printf ("   ed%d: %s, I/O: 0x%x, SHM: %ldK@0x%lx, IRQ %d\n",
	      lasted, eds[lasted].type_str, eds[lasted].asic_addr,
	      eds[lasted].mem_size/1024, kva2pa (eds[lasted].mem_start),
	      eds[lasted].irq);
      ed_halt (&eds[lasted]);
      ed_setup (&eds[lasted]);
      eds[lasted].ipaddr = 0xfe041a12;
      lasted++;
    }
  }
}

