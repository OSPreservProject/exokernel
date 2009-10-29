
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
#include <kern/extern.h>
#include <xok/mmu.h>
#include <xok/pio.h>
#include <xok/trap.h>
#include <xok/sys_proto.h>
#include <xok/ae_recv.h>
#include <xok/pctr.h>
#include <xok_include/net/ip.h>
#include <xok_include/net/ether.h>
#include <xok_include/assert.h>
#include <xok/sysinfo.h>


/*
 * Code to measure driver-driver roundtrips with a minimal udp packet.
 * The code is invoked by two system calls: lb_client and lb_server.
 * There is a user program that calls these system calls and initiates
 * the experiment.  Make sure the driver calls edlb.
 * The packet exchanged is a valid upd packet so that it can be
 * observed with tcpdump (or something equivalent).
 */

#define LB_MAGIC_REQ  0x4444
#define LB_MAGIC_REP  0x5555
#define LB_MAGIC_REQU 0x6666
#define LB_MAGIC_REPU 0x7777

static struct ae_recv recv_req;
static struct ae_recv recv_rep;

static char req[ETHER_MIN_LEN];
static char rep[ETHER_MIN_LEN];
static int volatile lb_cnt;
static pctrval lb_v1;
static int bandwidth;
static int irq = -1;

void edintr (int irq);
void tulip_intr (int irq);

int
edlb(char *p, int n, int cardno)
{
    struct udp_pkt *udp;
    pctrval v;
    pctrval v2;
/*
printf ("?");
*/
    udp = (struct udp_pkt *) 
	(p + sizeof(struct ether_pkt) + sizeof(struct ip_pkt));
  
    if ((udp->udp_dport == LB_MAGIC_REQ) && (udp->udp_sport == LB_MAGIC_REP)) {
        if (lb_cnt > 0) {
           lb_cnt--;
        }
	if (bandwidth == 100) {
	    int ret = sys_de_xmit(0, cardno, &recv_rep, NULL, 1);
            assert (ret == 0);
	} else if (bandwidth == 10) {
	    /* Send the packet. */
	    asm volatile ("pushl %0; popl %%es" :: "i" (GD_ED0));
	    bcopy (rep, NULL, ETHER_MIN_LEN);
	    asm volatile ("pushl %0; popl %%es" :: "i" (GD_KD));
	    sys_ed0xmit (0, ETHER_MIN_LEN);
	}
/*
printf ("Y%d", bandwidth);
*/
	return 1;
    } else if ((lb_cnt > 0) && (udp->udp_dport == LB_MAGIC_REP) && 
	       (udp->udp_sport == LB_MAGIC_REQ)) {
	lb_cnt--;
	if (lb_cnt > 0) {
	    if (bandwidth == 100) {
		int ret = sys_de_xmit(0, cardno, &recv_req, NULL, 1);
                assert (ret == 0);
	    } else if (bandwidth == 10) {
		/* Send the packet. */
		asm volatile ("pushl %0; popl %%es" :: "i" (GD_ED0));
		bcopy (req, NULL, ETHER_MIN_LEN);
		asm volatile ("pushl %0; popl %%es" :: "i" (GD_KD));
		sys_ed0xmit (0, ETHER_MIN_LEN);
	    }
	} else {
	    v2 = rdtsc();
	    v = v2 - lb_v1;

	    printf("time: %u %u (%x%08x)\n",
		(u_int) (v >> 32) & 0XFFFFFFFF, (u_int) (v & 0xFFFFFFFF),
		(u_int) (v >> 32) & 0XFFFFFFFF, (u_int) (v & 0xFFFFFFFF));
	}
/*
printf ("y%d", bandwidth);
*/
	return 1;
    }
/*
printf ("N");
*/
    return 0;
}


int
sys_lb_server(u_int sn, u_long is, u_long ic, char *es, char *ec, int n, int b,
	      int p)
{
    struct ip_pkt *ip;
    struct udp_pkt *udp;
    struct ether_pkt *eth;
    int cardno;

    eth = (struct ether_pkt *) rep;
    copyin(ec, eth->ether_dhost, 6);
    copyin(es, eth->ether_shost, 6);
    eth->ether_type = htons(ETHERTYPE_IP);

    ip = (struct ip_pkt *) (rep + sizeof(*eth));
    ip->ip_hl = sizeof(struct ip_pkt) >> 2;
    ip->ip_v = 0x4;

    ip->ip_tos = 0;
    ip->ip_id = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 0xFF;
    ip->ip_p = IPPROTO_UDP;
    ip->ip_sum = 0;
    ip->ip_src = is;
    ip->ip_dst = ic;
    ip->ip_len = htons(sizeof *ip + sizeof *udp);

    udp = (struct udp_pkt *) (rep + sizeof(*eth) + sizeof(*ip));
    udp->udp_dport = LB_MAGIC_REP;
    udp->udp_sport = LB_MAGIC_REQ;
    udp->udp_sum = 0;
    udp->udp_len = htons(sizeof *udp);

    recv_rep.n = 1;
    recv_rep.r[0].sz = ETHER_MIN_LEN;
    recv_rep.r[0].data = rep;

    lb_cnt = n;
    bandwidth = b;

    irq = -1;
    if (bandwidth == 10) {
       for (cardno=0; cardno<si->si_nsmc; cardno++) {
          if (bcmp (eth->ether_shost, si->si_smc[cardno].macaddr, 6) == 0) {
             irq = si->si_smc[cardno].irq;
             break;
          }
       }

    } else if (bandwidth == 100) {
       for (cardno=0; cardno<si->si_ntulips; cardno++) {
          if (bcmp (eth->ether_shost, si->si_tulips[cardno].tulip_enaddr, 6) == 0) {
             irq = (int) si->si_tulips[cardno].irq;
             break;
          }
       }
    }

    if (irq == -1) {
       printf ("Bad source ethernet address: %02x %02x %02x %02x %02x %02x\n",
		eth->ether_shost[0], eth->ether_shost[1],
		eth->ether_shost[2], eth->ether_shost[3],
		eth->ether_shost[4], eth->ether_shost[5]);
       return (-1);
    }

    printf("lb_server %d\n", n);

    if (p) {
	while (lb_cnt > 0) {
           if (bandwidth == 10) {
              edintr(irq);
           } else if (bandwidth == 100) {
              tulip_intr (irq);
           }
        }
    }
    return 0;
}


int
sys_lb_client(u_int sn, u_long is, u_long ic, char *es, char *ec, int n, int b,
	      int p)
{
    struct ip_pkt *ip;
    struct udp_pkt *udp;
    struct ether_pkt *eth;
    int cardno = -1;

    eth = (struct ether_pkt *) req;
    eth->ether_type = htons(ETHERTYPE_IP);
    copyin (es, eth->ether_dhost, 6);
    copyin (ec, eth->ether_shost, 6);

    ip = (struct ip_pkt *) (req + sizeof(*eth));
    ip->ip_hl = sizeof(struct ip_pkt) >> 2;
    ip->ip_v = 0x4;

    ip->ip_tos = 0;
    ip->ip_id = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 0xFF;
    ip->ip_p = IPPROTO_UDP;
    ip->ip_sum = 0;
    ip->ip_src = ic;
    ip->ip_dst = is;
    ip->ip_len = htons(sizeof *ip + sizeof *udp);

    udp = (struct udp_pkt *) (req + sizeof(*eth) + sizeof(*ip));
    udp->udp_sport = LB_MAGIC_REP;
    udp->udp_dport = LB_MAGIC_REQ;
    udp->udp_sum = 0;
    udp->udp_len = htons(sizeof *udp);

    recv_req.n = 1;
    recv_req.r[0].sz = ETHER_MIN_LEN;
    recv_req.r[0].data = req;

    bandwidth = b;
    lb_cnt = n;

    irq = -1;
    if (bandwidth == 10) {
       for (cardno=0; cardno<si->si_nsmc; cardno++) {
          if (bcmp (eth->ether_shost, si->si_smc[cardno].macaddr, 6) == 0) {
             irq = si->si_smc[cardno].irq;
             break;
          }
       }

    } else if (bandwidth == 100) {
       for (cardno=0; cardno<si->si_ntulips; cardno++) {
          if (bcmp (eth->ether_shost, si->si_tulips[cardno].tulip_enaddr, 6) == 0) {
             irq = (int) si->si_tulips[cardno].irq;
             break;
          }
       }
    }

    if (irq == -1) {
       printf ("Bad source ethernet address: %02x %02x %02x %02x %02x %02x\n",
		eth->ether_shost[0], eth->ether_shost[1],
		eth->ether_shost[2], eth->ether_shost[3],
		eth->ether_shost[4], eth->ether_shost[5]);
       return (-1);
    }

    printf("lb_client: %d\n", n);

    if (bandwidth == 100) {
        int ret;
	while ((ret = sys_de_xmit(0, cardno, &recv_req, NULL, 1)) != 0) {
           if (ret != -1) {
              return (-1);
           }
        }
    } else {
	/* Send the packet. */
	asm volatile ("pushl %0; popl %%es" :: "i" (GD_ED0));
	bcopy (req, NULL, ETHER_MIN_LEN);
	asm volatile ("pushl %0; popl %%es" :: "i" (GD_KD));
	sys_ed0xmit (0, ETHER_MIN_LEN);
    }
    lb_v1 = rdtsc();

    if (p) {
	while (lb_cnt > 0) {
           if (bandwidth == 10) {
              edintr(irq);
           } else if (bandwidth == 100) {
              tulip_intr (irq);
           }
        }
    }

    return 0;
}


#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

