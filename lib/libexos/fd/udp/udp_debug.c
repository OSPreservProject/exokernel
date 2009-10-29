
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

#include "udp_socket.h"
#include <exos/osdecl.h>		/* for ADD_UVA */

/* DEBUGGING PROCEDURES TO PRINT OUT DIFFERENT DATA STRUCTURES */

/* Debugging
   printfs ports in use 
   */
int
pr_used_ports(void) {
    int k,count = 0;
    printf("pr_used_ports:\n");
    return 99;			/* for now */
    for (k = 0; k < NR_SOCKETS; k++) {

	if (udp_shared_data->used_ports[k] != 0) count++;

	printf("[%02d]: %05d ",
	       k,udp_shared_data->used_ports[k]);
	if (k % 6 == 5) printf("\n");
    }
    return count;
}

/* Debugging
 pr_ringbuf prints out information pointed by p, but not the whole ring.
 pr_ringbufs, prints out information of the whole ringbuf
 pr_ringbufs_all, prints out information of the shared ringbufs */
void
pr_ringbuf(ringbuf_p r) {
  assert(r);
  fprintf(stderr,"p:%-10p nxt:%-10p pollp:%-10p ",
	 r, r->next, r->pollptr);
  fprintf(stderr,"poll:%d n:%01d l:%04d d:%-10p\n",
	 r->poll, r->n, r->sz, r->data);
}

void
pr_ringbufs(ringbuf_p r) {
  ringbuf_p t;
  t = r;
  assert(r);
  for(;;) {
    pr_ringbuf(t);
    if (t->next == r) break;
    t = t->next;
  } 
}

void
pr_ringbufs_all(void ) {
  int i;
  for (i = 0; i < NR_RINGBUFS ; i++) 
    pr_ringbuf(&udp_shared_data->ringbufs[i]);
}

void 
pr_recvfrom(struct recvfrom * recvfrom) {
  printf("sockflag: %d, srcport: %d ipsrc: %08lx\n",
	 recvfrom->sockflag, recvfrom->srcport, recvfrom->ipsrc);
  printf("nrpkt: %d, curpkt: %d\n",recvfrom->nrpkt, recvfrom->curpkt);
}
void 
kpr_recvfrom(struct recvfrom * recvfrom) {
  kprintf("sockflag: %d, srcport: %d ipsrc: %08lx\n",
	 recvfrom->sockflag, recvfrom->srcport, recvfrom->ipsrc);
  kprintf("nrpkt: %d, curpkt: %d\n",recvfrom->nrpkt, recvfrom->curpkt);
}

void 
pr_udpfilp(socket_data_p data) {
  recvfrom_def *recvfrom = &data->recvfrom;
  //  sockaddr_def *tosockaddr = &data->tosockaddr;

   printf("  port: %d, demux id: %d, sockflag: %08x\n  nrpkt: %d curpkt: %d ringid: %d r: %p\n",
	 htons(data->port), data->demux_id, recvfrom->sockflag, recvfrom->nrpkt, recvfrom->curpkt,recvfrom->ring_id,recvfrom->r);
}





#if 0
/* Code that can be used for testing the ringbufs 
   allocation/deallocation procedures */
void
test_putgetringbuf(char ch) {
  int i;
  ringbuf_p t[15];
extern void pr_ringbuf(ringbuf_p r);
extern void pr_ringbufs(ringbuf_p r);
  
  switch(ch) {
  case 'u':
    for (i = 1; i < 12; i++) {
      t[i] = udp_getringbuf(i,0);
      printf("udp_getringbuf(%d) --> %p\n",i,t[i]);
    }
    udp_putringbuf(t[3]);
    udp_putringbuf(t[5]);
    t[5] = udp_getringbuf(5,0);
    t[3] = udp_getringbuf(3,0);
    
    printf("\n\nPR RINGBUFS ALL\n");
    pr_ringbufs_all();
    printf("\n\n");
    for (i = 1; i < 12; i++) {
      if (t[i]) {
	printf("\ni: %d %p\n",i,t[i]);
	pr_ringbuf(t[i]);
	printf("RINGBUFS %d\n",i);
	
	if (i > 2) {
	  if (t[i]->next) {
	    pr_ringbufs(t[i]->next);
	    udp_putringbuf(t[i]->next);   
	  }
	} else if (t[i]) {
x`	  pr_ringbufs(t[i]);
	  udp_putringbuf(t[i]);
	}
      }
    }
  case 'v':
    printf("\n\nPR RINGBUFS ALL\n");
    pr_ringbufs_all();
    break;
  }
	
}
#endif
