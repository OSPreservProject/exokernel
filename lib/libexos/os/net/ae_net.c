
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
#include <xok/mmu.h>
#include <xok/env.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <xok/defs.h>
#include <stdio.h>
#include <xok/sys_ucall.h>
#include <xok/ae_recv.h>
#include "../../../ash/ash_ae_net.h"
#include <exos/net/ae_net.h>
#include <exos/net/ether.h>

#include <xok/env.h>
#include <xok/mmu.h>
#include <exos/vm.h>
#include <exos/osdecl.h>

#include "exos/critical.h"

#include <xok/ash.h>

#ifdef ASH_NET

/*
 * Simple network HAL that is almost identical to the Aegis one. 
 * The kernel side is implemented in an ash in lib/ash.
 */

extern struct ash_sh_network {
  struct eth_queue_entry {
    struct ae_eth_pkt *queue[NENTRY];
    int head_queue;
    int tail_queue;
  } queue_table[DPF_MAXFILT];
  struct ae_eth_pkt_queue ash_alloc_pkt;
  struct ae_eth_pkt ae_packets[NENTRY];
  char ash_pkt_data[NENTRY * MAXPKT];
  int nfree;
} asn;


/* use this instead of 'asn' in most cases */
#define ASN (*((struct ash_sh_network*)((unsigned int)&asn + \
					(unsigned int)__curenv->env_ashuva)))
					
#define FID_HEAD_QUEUE ASN.queue_table[fid].head_queue
#define FID_TAIL_QUEUE ASN.queue_table[fid].tail_queue
#define FID_QUEUE ASN.queue_table[fid].queue

void AllocAshShareRegion(void);

void 
ae_eth_init()
{
    int i,fid;
    extern void print_physical(int page);

    if (envidx(__envid) == 1) {
      AllocAshShareRegion();
      ASN.nfree = 0;
      for (i = 0; i < NENTRY; i++) {
	ASN.ae_packets[i].recv.r[0].data = &asn.ash_pkt_data[i * MAXPKT];
	ASN.ae_packets[i].recv.r[0].sz = MAXPKT;
	ASN.ae_packets[i].recv.n = 1;
	ASN.nfree++;
	ae_ash_append_pkt(&asn.ash_alloc_pkt, &ASN.ae_packets[i]);
      }
      for (fid = 0; fid < DPF_MAXFILT; fid++) {
	FID_HEAD_QUEUE = 0;
	FID_TAIL_QUEUE = 0;
      }
    }
}

#endif /* ASH_NET */

int
ae_eth_nr_pkt(void)
{
    return NENTRY - 1;
}

#ifdef ASH_NET

int
ae_eth_nr_free_pkt(void)
{
    return ASN.nfree;
}


/* UD means that if the ASH area has been moved, then the pointer won't be */
/* between 0 and 4meg */
/* (returns UD(user data segment) pointer) */
struct ae_eth_pkt*
ae_eth_get_pkt(void)
{
    struct ae_eth_pkt *p;

    p = ae_ash_dequeue_pkt(&asn.ash_alloc_pkt);
    if (p) ASN.nfree--;

    return p;
}

/* (takes UD pointer) */
void
ae_eth_release_pkt(struct ae_eth_pkt* pkt)
{
    if (pkt) ASN.nfree++;

    ae_ash_append_pkt(&asn.ash_alloc_pkt, pkt);
}

void
ae_eth_close(int fid)
{
  int i;
  assert(fid >= 0);
  assert(fid < DPF_MAXFILT);

  /* Make queue empty, so that ash won't insert new pkts for this filter. */
  /* BUG: should be atomic */
  FID_HEAD_QUEUE = FID_TAIL_QUEUE;
  
  /* Return any packets that are still in the queue. */
  for (i = 0; i < NENTRY; i++) {
    if ((FID_QUEUE[i] != 0) && (ADD_UVA(FID_QUEUE[i])->flag < 0)) {
      ADD_UVA(FID_QUEUE[i])->flag = 1;        /* libos owns it now */
      FID_QUEUE[i] = 0;
    }
  }
}
#endif /* ASH_NET */


int
ae_eth_send(void *d, int sz, int netcardno)
{
   int ret = 0;
   volatile uint pending = 1;
   char filler[ETHER_MIN_LEN];
   struct ae_recv recv;

   recv.r[0].data = d;
   recv.r[0].sz = sz;
   recv.n = 1;

   assert (netcardno < XOKNET_MAXNETS);

   if (sz < ETHER_MIN_LEN) {
      recv.r[1].data = filler;
      recv.r[1].sz = ETHER_MIN_LEN - sz;
      recv.n = 2;
   }

   ret = sys_net_xmit (netcardno, &recv, (uint *) &pending, 1);
   assert ((ret == 0) || (ret == -1));

   while ((ret == 0) && (pending > 0)) 
   { 
     sys_geteid(); 
     asm volatile ("" : : : "memory");
   }

   return (ret);
}


int
ae_eth_sendv(struct ae_recv *outgoing, int netcardno)
{
   int ret = 0;
   int sz, i;
   char filler[ETHER_MIN_LEN];
   volatile uint pending = 1;

   assert (netcardno < XOKNET_MAXNETS);

   /* make sure size is >= ETHER_MIN_LEN */
   for (sz = 0, i = 0; i < outgoing->n; i++) {
      sz += outgoing->r[i].sz;
   }

   if (sz < ETHER_MIN_LEN) {
      outgoing->r[outgoing->n].sz = ETHER_MIN_LEN - sz;
      outgoing->r[outgoing->n].data = &filler[0];
      outgoing->n++;
   }

   ret = sys_net_xmit (netcardno, outgoing, (uint *) &pending, 1);
   assert ((ret == 0) || (ret == -1));

   while ((ret == 0) && pending > 0) 
   { 
     sys_geteid(); 
     asm volatile ("" : : : "memory");
   }

   return (ret);
}


#ifdef ASH_NET

/* takes a UD pointer */
int
ae_eth_poll(int fid, struct ae_eth_pkt *p)
{
    /*
     * This code might be interrupted by an ash at any point in time,
     * but that is OK.  It is wait free.
     */

    assert(fid >= 0);
    assert(fid < DPF_MAXFILT);

    p->flag = -1;		/* the ash owns it now */

    if (INC(FID_HEAD_QUEUE) != FID_TAIL_QUEUE) {
	FID_QUEUE[FID_HEAD_QUEUE] = SUB_UVA(p);
	FID_HEAD_QUEUE = INC(FID_HEAD_QUEUE);
    } else assert(0);

    return 0;
}

/* Makes the ash region shared */
void 
AllocAshShareRegion(void) {
  /* printf("AllocAshShareRegion sz: %d\n",sizeof(struct ash_sh_network)); */
  if (__vm_alloc_region(ADD_UVA(ASH_PKT_DATA_ADDR),
			sizeof(struct ash_sh_network), 0,
			PG_SHARED|PG_P|PG_U|PG_W) < 0) {
    printf ("couldn't insert ash data pte's\n");
    __die();
  }
}

int 
ShareAshRegion(u_int k, int envid) {
  /* printf("ShareAshRegion sz: %d\n",sizeof(struct ash_sh_network)); */
  if (__vm_share_region(ADD_UVA(ASH_PKT_DATA_ADDR),
			sizeof(struct ash_sh_network), k, k, envid,
			ASH_PKT_DATA_ADDR + env_id2env(envid)->env_ashuva)
      < 0) {
    sys_env_free (k, envid);
    kprintf("ShareAshRegion: __vm_share_region failed\n");
    return (-1);
  }
  
  return 0;
}

#endif /* ASH_NET */
