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

#include <xok/defs.h>
#include <xuser.h>
#include <sys/types.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <sys/asm.h>
#include <stdio.h>
#include <assert.h>
#include <exos/net/ether.h>
#include <exos/net/ae_net.h>
#include "ash_ae_net.h"
#include <stdio.h>
#include <xok/pctr.h>

#define pkt_copy(dst, sseg, src, len)                           \
do {                                                            \
  if (__builtin_constant_p (len) && ! (len & 0x3))              \
    asm volatile ("cld\n"                                       \
                  "\tshrl $2,%%ecx\n"                           \
                  "\trep\n"                                     \
                  "\t "sseg"movsl\n"                            \
                  :: "S" (src), "D" (dst), "c" (len)            \
                  : "ecx", "esi", "edi", "cc", "memory");       \
  else                                                          \
    asm volatile ("cld\n"                                       \
                  "\tmovl %2,%%ecx\n"                           \
                  "\tshrl $2,%%ecx\n"                           \
                  "\trep\n"                                     \
                  "\t "sseg"movsl\n"                            \
                  "\tmovl %2,%%ecx\n"                           \
                  "\tandl $3,%%ecx\n"                           \
                  "\trep\n"                                     \
                  "\t "sseg"movsb"                              \
                  :: "S" (src), "D" (dst), "g" (len)            \
                  : "ecx", "esi", "edi", "cc", "memory");       \
} while (0)


#define DPF_MAXFILT 256		/* this should be the same as dpf-internals.h */
#define FID_HEAD_QUEUE asn.queue_table[fid].head_queue
#define FID_TAIL_QUEUE asn.queue_table[fid].tail_queue
#define FID_QUEUE asn.queue_table[fid].queue

/* Flags indicating whether the filter belongs to a fast web client */
/* application -- the app sets this flag during its initialization  */
int ganger_info[DPF_MAXFILT];

u_int __envid = 0;                       /* Cached environment id */

/* Memory to be shared between ash and library.
 *
 * Wait free circular queue to communicate between ash and library. 
 * Start with a empty queue, i.e., all pkts are owned by library.
 */

extern struct ash_sh_network {
  struct eth_queue_entry {
    struct ae_eth_pkt *queue[NENTRY];
    int head_queue;
    int tail_queue;
  } queue_table[DPF_MAXFILT];
  struct ae_eth_pkt ae_packets[NENTRY];
  char ash_pkt_data[NENTRY * MAXPKT];
  struct ae_eth_pkt_queue ash_alloc_pkt;
} asn;


#ifdef LB

#include <string.h>

int volatile lb_fid;
int volatile lb_isserver;
int volatile lb_cnt;
int lb_time_distribution[11];
char lb_req[ETHER_MIN_LEN];
char lb_rep[ETHER_MIN_LEN];
#endif


void
_____symbols (void)
{
  DEF_SYM (__ash_envid, &__envid);
  DEF_SYM(asn,ASH_PKT_DATA_ADDR);
  DEF_SYM (ash_u, ASH_UADDR);
}

void __die (void) __attribute__ ((noreturn));
void
__die (void)
{
  printf ("$$ I'm dying (0x%x)\n", sys_geteid ());
  sys_env_free (0, sys_geteid ());
  printf ("$$ I'm dead, but I don't feel dead.\n");
  for (;;)
    ;
}


void pkt_recv (u_int, u_int) __attribute__ ((regparm (3)));
void
pkt_recv (u_int len, u_int fid)
{
    struct ae_eth_pkt *pkt;
    struct ether_pkt *epkt;
    char *d;

#ifdef LB
    int edlb(int, int);

    if (edlb(fid, len)) {
	asm volatile ("int %0" :: "i" (T_ASHRET));
	return;
    }
#endif

    if (FID_HEAD_QUEUE != FID_TAIL_QUEUE) {
	assert_die(len <= MAXPKT);
	assert_die(FID_QUEUE[FID_TAIL_QUEUE]);

	pkt = FID_QUEUE[FID_TAIL_QUEUE];
	FID_QUEUE[FID_TAIL_QUEUE] = 0;
	if (pkt->flag >= 0) {
	    printf("#fid: %d, hd: %d, tl: %d pkt: %08x env: %d, fl %d #",
	     fid,
	     FID_HEAD_QUEUE,
	     FID_TAIL_QUEUE,
	     (int)FID_QUEUE[FID_TAIL_QUEUE],
	     sys_geteid(),
	     pkt->flag);
	}
	assert_die(pkt->flag < 0);
	d = pkt->recv.r[0].data;
	asm volatile ("pushl %0; popl %%fs" :: "i" (GD_AW));

#ifdef GANGER_ASH_HACK
		/* Small hack to eliminate copies and checksumming for */
		/* the fast HTTP client app. */
        if (ganger_info[fid] == 1) {
           pkt_copy (d, fso, NULL, (min(len, 64)));
           *((uint16 *) &d[50]) = 0;
        } else {
	   pkt_copy (d, fso, NULL, len);
        }
#else
	pkt_copy (d, fso, NULL, len);
#endif

	epkt = (void *) d;
	pkt->recv.r[0].sz = len;
	pkt->flag = len;	/* the libos owns it now */
	FID_TAIL_QUEUE = INC(FID_TAIL_QUEUE);
    } else {
      /* no structure to receive the packet, dropping it */
#if 0
      printf("#fid: %d, hd: %d, tl: %d pkt: %08x env: %d, #",
	     fid,
	     FID_HEAD_QUEUE,
	     FID_TAIL_QUEUE,
	     (int)FID_QUEUE[FID_TAIL_QUEUE],
	     sys_geteid());
#else 
      printf("#");
#endif
    }
    asm volatile ("int %0" :: "i" (T_ASHRET));
}




#ifdef LB

static pctrval last_v;

int
edlb(int fid, int len)
{
  pctrval v, v2;
  int i;

  if (fid == lb_fid && lb_isserver) {
      /* Send the packet. */
      asm volatile ("pushl %0; popl %%es" :: "i" (GD_ED0));
      bcopy (lb_rep, NULL, ETHER_MIN_LEN);
      asm volatile ("pushl %0; popl %%es" :: "i" (GD_UD));
      sys_ed0xmit (ETHER_MIN_LEN);
      lb_cnt--;
      return 1;
  } else if (fid == lb_fid) {
      lb_cnt--;
      if (lb_cnt > 0) {
	  asm volatile ("pushl %0; popl %%es" :: "i" (GD_ED0));
	  bcopy (lb_req, NULL, ETHER_MIN_LEN);
	  asm volatile ("pushl %0; popl %%es" :: "i" (GD_UD));
	  sys_ed0xmit (ETHER_MIN_LEN);

	  v2 = rdtsc();
	  v = v2 - last_v;
	  last_v = v2;
	  
	  i = v / 10000;

	  if (i > 9) i = 10;

	  lb_time_distribution[i]++;
      }
      return 1;
  }
  return 0;
}
#endif

