
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
#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>

#include <sys/tick.h>
#include <sys/sysinfo.h>

#include <xuser.h>

#include <exos/process.h>

#include <netinet/fast_ip.h>
#include <netinet/fast_eth.h>

#include "netinet/ip.h"

#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>

#include "ash/ash_ae_net.h"
#include <net/ae_net.h>

#include <netinet/cksum.h>

#include "debug.h"

#include "web_general.h"
#include "web_tcp.h"

int inpackets = 0;
int outpackets = 0;

#define TCPCKSUM		/* Tcp checksumming enabled */
#define IPCKSUM			/* Ip checksumming enabled */

/* TCP header, see RFC 793 */
struct tcp {
    uint16	src_port;	/* Source port */
    uint16	dst_port;	/* Destination port */
    uint32	seqno;		/* Sequence number */
    uint32	ackno;		/* Acknowledgement number */
    uint8	offset;		/* Offset; start of data (4 bits) */
    uint8	flags;		/* Flags (6 bits, staring at bit 2) */
    uint16	window;		/* The number of bytes wiling to accept */
    uint16	cksum;		/* Checksum over data and pseudoheader */
    uint16	urgentptr;	/* Pointer to urgent data */
};

/* Defines for tcp option kinds: */
#define TCP_OPTION_EOL	0
#define TCP_OPTION_NOP	1
#define TCP_OPTION_MSS	2

/* Defines for tcp option lengths: */
#define TCP_OPTION_MSS_LEN	4

/* The default maximum segment size (excl. ip and tcp header). */
#define TCP_MSS		536

/* The default TTL value for IP ttl field. */
#define TCP_TTL		64

/* Default maximum segment life time. */
#define TCP_MSL		30	/* 30 secs */

/* TCP control flags: */
#define TCP_FL_URG	0x20	/* Urgent pointer */
#define TCP_FL_ACK	0x10	/* Acknowledgement */
#define TCP_FL_PSH	0x8	/* Push */
#define TCP_FL_RST	0x4	/* Reset */
#define TCP_FL_SYN	0x2	/* Synchronize */
#define TCP_FL_FIN	0x1	/* No more data */

/* Default maximum segment size, which we advertise */

#define MIN_ETHER_LEN	64
#define DEFAULT_MSS	1460	/* ethernet packet - headers */
/* Default size of receive buffer */
#define DEFAULT_MSG	(sizeof(struct eth) + sizeof(struct ip) + sizeof(struct tcp) + DEFAULT_MSS)

/* Start receive buffer on short-aligned address, so that the ip header after
 * the ethernet header is word-aligned.
 */
#define START_ALIGN	4
#define ETHER_ALIGN	2

/* Some internal constants */
#define TCP_TIMEOUT	10000000 /* microseconds */
#define TCP_RETRY	10

/* 
 * Macros to compare sequence numbers. Assumption is that sequence numbers
 * a and b differ by no more than one-half of the largest sequence number.
 */
#define	SEQ_LT(a,b)	((int)((a)-(b)) < 0)
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	SEQ_GT(a,b)	((int)((a)-(b)) > 0)
#define	SEQ_GEQ(a,b)	((int)((a)-(b)) >= 0)

/* Connection states */
#define TCB_ST_FREE		0 
#define TCB_ST_LISTEN		1
#define TCB_ST_SYN_SENT		2
#define TCB_ST_SYN_RECEIVED	3
#define TCB_ST_ESTABLISHED	4
#define TCB_ST_FIN_WAIT1	5
#define TCB_ST_FIN_WAIT2	6
#define TCB_ST_CLOSE_WAIT	7
#define TCB_ST_CLOSING		8
#define TCB_ST_LAST_ACK		9
#define TCB_ST_TIME_WAIT	10
#define TCB_ST_CLOSED		11

#define NSTATES			12

/* TCB control flags: */
#define TCB_FL_TIMEOUT		0x0001
#define TCB_FL_RESET		0x0002
#define TCB_FL_OUTONLY		0x0004
#define TCB_FL_RESEND		0x0008
#define TCB_FL_DELACK		0x0010

#define WEB_EIT_ETH	0
#define WEB_EIT_IP	(WEB_EIT_ETH + sizeof(struct eth))
#define WEB_EIT_TCP	(WEB_EIT_IP + sizeof(struct ip))
#define WEB_EIT_DATA	(WEB_EIT_TCP + sizeof(struct tcp))

#ifndef NOSTATISTICS

#define	STINC(field)		tcp_stat.field++
#define	STDEC(field)		tcp_stat.field--
#define STINC_SIZE(field, n)	tcp_stat.field[n]++
#define NLOG2			13	/* round of 2 log MSS */

struct statistics {
    int		nacksnd;	/* # normal acks */
    int		nack;		/* # acks received */
    int		nvack;		/* # volunteerly acks */
    int		ndelayack;	/* # delayed acks */
    int		nackprd;	/* # predicted acks */
    int		ndataprd;	/* # predicted data */
    int		nzerowndsnt;	/* # acks advertising zero windows */
    int		nretrans;	/* # retransmissions */
    int		nethretrans;	/* # ether retransmissions */
    int		nfinsnd;	/* # fin segments */
    int		nfinrcv;	/* # fin good segments received */
    int		ndatarcv;	/* # segments with data */
    int		norder;		/* # segments in order */
    int		nbuf;		/* # segments buffered */
    int		noutspace;	/* # segments with data but no usr buffer */
    int		noutorder;	/* # segments out of order */
    int		nprobercv;	/* # window progbes */
    int		nprobesnd;	/* # window probes send */
    int		nseg;		/* # segments received */
    int		nincomplete;	/* # incomplete segments */
    int		nsynsnd;	/* # syn segments sent */
    int		nsynrcv;	/* # syn segments received */
    int		nopen;		/* # tcp_open calls */
    int		nlisten;	/* # tcp_listen calls */
    int		naccepts;	/* # tcp_accept calls */
    int		nread;		/* # tcp_read calls */
    int		nwrite;		/* # tcp_write calls */
    int		ndata;		/* # segments sent with data */
    int		nclose;		/* # tcp_close calls */
    int		ndropsnd;	/* # segments dropped when sending */
    int		ndroprcv;	/* # segments dropped when receiving */
    int		nrstsnd;	/* # reset segments send */
    int		nrstrcv;	/* # reset segments received */
    int		naccept;	/* # acceptable segments */
    int		nunaccept;	/* # unacceptable segments */
    int		nbadipcksum;	/* # segments with bad ip checksum */
    int		nbadtcpcksum;	/* # segments with bad tcp checksum */
    int		rcvdata[NLOG2];
    int		snddata[NLOG2];
} tcp_stat;

#else
#define	STINC(field)		
#define	STDEC(field)		
#define STINC_SIZE(field, n)	
#endif

/* From tcp/global.h */
#define IP_PSEUDO_HDR_SIZE	12

int droprate = 0;

struct tcb * (*web_tcp_gettcb)() = NULL;

/************* send wrapper for abstracting interface differences ************/

/* The ed interface (for the ISA card) copies data from the user-supplied */
/* buffers immediately, so no wrapping is necessary.  The de interface	  */
/* (for the PCI card) uses DMA at the time of the actual trasnmission.	  */
/* As a result, the buffers passed thru the send interface must remain	  */
/* inviolate until the transmission has occurred.  The de interface	  */
/* asynchronously notifies us of this fact by decrementing an integer	  */
/* (we pass it a pointer to this integer).				  */

/* So, most of this section of code is for handling the de interface	  */
/* semantics.  We do this by assuming that the first entry of the recv's  */
/* gather list is transient and the remainder is inviolate.  (Note: the   */
/* caller is responsible for making certain that this is the case -- this */
/* library treats this as a subset of the "no retransmission pool" rules.)*/
/* A dedicated bit of memory is used as a circular buffer to hold copies  */
/* of transient data and small structures (dmaholder) to support reclaim. */

typedef struct dmaholder {
   struct dmaholder *next;
   int pending;
} dmaholder;

static dmaholder *dmaholders = NULL;

#define SENDBUFLEN 65536
static char send_buf[SENDBUFLEN];
static int freehead = 0;
static int freetail = SENDBUFLEN;

static int ae_eth_send_wrap (int netcardno, struct ae_recv *send_recv)
{
   dmaholder *newdma;
   char *tmpptr = send_recv->r[0].data;
   int tmpsz = send_recv->r[0].sz;
   int tmpn = send_recv->n;
   int copyall = 0;
   int ret;
   int sz;
/*
printf ("ae_eth_send_wrap: netcardno %d, send_recv %p\n", netcardno, send_recv);
*/

   /* ed interface doesn't need any of this */
   if (netcardno < (MAXSMC+1)) {
	/* GROK - change around the netcardno calculation! */
      return (ae_eth_sendv (send_recv, (netcardno-1)));
   }

   /* reclaim space holding transmitted data */
   while ((dmaholders) && (dmaholders->next->pending <= 0)) {
      newdma = dmaholders->next;
      dmaholders->next = newdma->next;
      dmaholders = (newdma != dmaholders) ? dmaholders : NULL;
/*
printf ("reclaiming a dmaholder: newdma %p, dmaholders %p, &send_buf[0] %p\n", newdma, dmaholders, &send_buf[0]);
*/
      if ((dmaholders) && (((uint)dmaholders->next - (uint)&send_buf[0]) > freetail)) {
	 freetail = (uint)dmaholders->next - (uint)&send_buf[0];
      }
   }

   /* reset if nothing pending */
   if (dmaholders == NULL) {
      freehead = 0;
      freetail = SENDBUFLEN;
   }

   /* Make certain that packet will be minimum ethernet size (de interface */
   /* will reject it otherwise).					   */
   sz = send_recv->r[0].sz;
   if (send_recv->n == 1) {
      sz = max (MIN_ETHER_LEN, sz);
   } else if ((send_recv->n == 2) && ((sz + send_recv->r[1].sz) < MIN_ETHER_LEN)) {
      sz = MIN_ETHER_LEN;
      copyall = 1;
   } else {
      int i;
      int totsz = sz;
      for (i=1; i<send_recv->n; i++) {
	 totsz += send_recv->r[i].sz;
      }
      if (totsz < MIN_ETHER_LEN) {
	 sz = MIN_ETHER_LEN;
	 copyall = 1;
      }
   }

   /* Find free space for the transient send data (default is freehead) */
   if ((freetail - freehead - (int) sizeof(dmaholder)) < sz) {
/*
printf ("at end of buffer.  try it from the beginning: dmaholders %p (pend %d)\n", dmaholders->next, dmaholders->next->pending);
*/
      freehead = 0;
      freetail = (uint)dmaholders->next - (uint)&send_buf[0];
      if ((freetail - freehead - (int) sizeof(dmaholder)) < sz) {
	 return (-1);
      }
   }

   /* initialize the data structure and add to the tail-pointed circle-q */
   newdma = (dmaholder *) &send_buf[freehead];
   newdma->pending = 1;
   if (copyall == 0) {
      bcopy (send_recv->r[0].data, &send_buf[freehead+sizeof(dmaholder)], send_recv->r[0].sz);
   } else {
      int i;
      int tmpsz = 0;
      for (i=0; i<tmpn; i++) {
	 bcopy (send_recv->r[i].data, &send_buf[freehead+sizeof(dmaholder)+tmpsz], send_recv->r[i].sz);
	 tmpsz += send_recv->r[i].sz;
      }
   }
   if (dmaholders) {
      newdma->next = dmaholders->next;
      dmaholders->next = newdma;
   } else {
      newdma->next = newdma;
   }
   dmaholders = newdma;

   /* reset recv to point to steady version of transient data, call send, */
   /* and restore the pointers. */
   send_recv->r[0].data = &send_buf[freehead+sizeof(dmaholder)];
   send_recv->r[0].sz = sz;
   ret = sys_de_xmit ((netcardno-(MAXSMC+1)), send_recv, &newdma->pending, 1);
   send_recv->r[0].data = tmpptr;
   send_recv->r[0].sz = tmpsz;
   send_recv->n = tmpn;

   /* update freehead to reflect space used */
   freehead += sizeof(dmaholder) + sz;
   freehead += (freehead % sizeof(int)) ? sizeof(int) - (freehead % sizeof(int)) : 0;
   assert ((freehead >= 0) && (freehead <= SENDBUFLEN));

   return (ret);
}


/********************* buffers for incoming data ****************************/

#define RINGBUFFER	/* Use app/kernel ring instead of ASH buffers */

/* web_tcpeth buffers */
typedef struct webtcpbuf {
   struct webtcpbuf *next;
#ifdef RINGBUFFER
   int *pollptr;
   int poll;
#else
   struct ae_eth_pkt *poll;
#endif
   int n;
   int sz;
   char *data;
   char space[1516];
} webtcpbuf_t;

#define MAXPOLLEECNT	32	/* Should match RECVQ_MAX in kernel/net.c */
static int maxpolleecnt = MAXPOLLEECNT;
#ifndef RINGBUFFER
static int pollbufcnt = 0;
#endif
webtcpbuf_t *extrapollbufs = NULL;
webtcpbuf_t *pollbuflist = NULL;
webtcpbuf_t *pollbuftail = NULL;
int webtcp_demuxid = -1;


#ifndef RINGBUFFER
static webtcpbuf_t * web_tcp_getfrompolllist (webtcpbuf_t **head, webtcpbuf_t **tail)
{
   webtcpbuf_t *retbuf = *head;
/*
printf ("getfrompolllist: head %x, tail %x, *head %x, *tail %x, retbuf %x\n", (uint) head, (uint) tail, (uint) *head, (uint) *tail, (uint) retbuf);
*/
   if (retbuf) {
      *head = retbuf->next;
      if (retbuf->next == NULL) {
	 assert (*tail == retbuf);
	 *tail = NULL;
      }
      pollbufcnt--;
      assert (pollbufcnt >= 0);
   }
   return (retbuf);
}


static void web_tcp_appendtopolllist (webtcpbuf_t *new, webtcpbuf_t **head, webtcpbuf_t **tail)
{
   assert ((new != NULL) && (tail != NULL) && (head != NULL));
/*
printf ("appendtolist: new %x, head %x, tail %x, *head %x, *tail %x\n", (uint) new, (uint) head, (uint) tail, (uint) *head, (uint) *tail);
*/
   new->next = NULL;
   if (*tail != NULL) {
      (*tail)->next = new;
      *tail = new;
   } else {
      *tail = new;
      assert (*head == NULL);
      *head = new;
   }
}


static void web_tcp_resetpollbuf (webtcpbuf_t *dead)
{
   int ret;

   if (dead == NULL) {
      if (extrapollbufs == NULL) {
	 webtcpbuf_t *newpollbufs = (webtcpbuf_t *) malloc (3 * 4096);
	 int i;

	 for (i=0; i < (3*4096/sizeof(webtcpbuf_t)); i++) {
	    newpollbufs[i].poll = NULL;
	    newpollbufs[i].n = 1;
	    newpollbufs[i].sz = DEFAULT_MSG; /* 1514 */
	    newpollbufs[i].data = &newpollbufs[i].space[2];
	    newpollbufs[i].next = extrapollbufs;
	    extrapollbufs = &newpollbufs[i];
	 }
      }
      dead = extrapollbufs;
      extrapollbufs = dead->next;
   }

   if (pollbufcnt >= maxpolleecnt) {
      if (dead->poll) {
	 ae_eth_release_pkt(dead->poll);
      }
      dead->poll = NULL;
      dead->next = extrapollbufs;
      extrapollbufs = dead;
      return;
   }

   if (dead->poll == NULL) {
      dead->poll = ae_eth_get_pkt();
      if (dead->poll == NULL) {
	 return;
      }
   }
   dead->n = 1;
   pollbufcnt++;
   if ((ret = ae_eth_poll (webtcp_demuxid, dead->poll)) < 0) {
      printf ("web_tcp_resetpollbuf: unable to setup poll buffer %x (ret %d)\n", (uint)dead, ret);
      exit (0);
   }
   web_tcp_appendtopolllist (dead, &pollbuflist, &pollbuftail);
}
#endif


static void web_tcp_initpollbufs (int demux_id)
{
   int i;

#ifndef RINGBUFFER
   for (i=0; i<maxpolleecnt; i++) {
      web_tcp_resetpollbuf (NULL);
   }
#else
   webtcpbuf_t *newpollbufs = (webtcpbuf_t *) malloc ((maxpolleecnt+1) * sizeof (webtcpbuf_t));
   for (i=0; i<=maxpolleecnt; i++) {
      newpollbufs[i].poll = 0;
      newpollbufs[i].pollptr = &(newpollbufs[i].poll);
      newpollbufs[i].n = 1;
      newpollbufs[i].sz = DEFAULT_MSG;	/* 1514 */
      newpollbufs[i].data = &(newpollbufs[i].space[2]);
      newpollbufs[i].next = &(newpollbufs[(i+1)]);
   }
   newpollbufs[(maxpolleecnt-1)].next = &(newpollbufs[0]);
   pollbuflist = newpollbufs;
   pollbuftail = &(newpollbufs[(maxpolleecnt-1)]);
   assert (pollbuftail->next == pollbuflist);
   extrapollbufs = &(newpollbufs[maxpolleecnt]);
   extrapollbufs->next = NULL;
   if (sys_pktring_setring (0, demux_id, (struct pktringent *) pollbuflist) < 0) {
      printf ("setring failed: no soup for you!\n");
      assert (0);	
   }		    
#endif
}


#ifdef RINGBUFFER
static void web_tcp_stealpollbuf (webtcpbuf_t *steal, int demux_id, webtcpbuf_t *prevbuf)
{
   webtcpbuf_t *replacebuf;

   if (extrapollbufs == NULL) {
      webtcpbuf_t *newbuf = (webtcpbuf_t *) malloc (sizeof (webtcpbuf_t));
      assert (newbuf != NULL);
      newbuf->poll = 0;
      newbuf->pollptr = &(newbuf->poll);
      newbuf->n = 1;
      newbuf->sz = DEFAULT_MSG;	/* 1514 */
      newbuf->data = &(newbuf->space[2]);
      newbuf->next = NULL;
      extrapollbufs = newbuf;
   }

   replacebuf = extrapollbufs;
   extrapollbufs = replacebuf->next;

   if (pollbuftail == steal) {
      pollbuftail = replacebuf;
   } else if (pollbuflist == steal) {
      pollbuflist = replacebuf;
   }
/*
printf ("before modring: prev %p, curr %p, next %p\n", prevbuf, prevbuf->next, prevbuf->next->next);
*/
   assert (prevbuf->next == steal);
   replacebuf->next = steal->next;
   steal->next = NULL;
   prevbuf->next = replacebuf;
   replacebuf->poll = 0;	/* give space to kernel */
   assert (replacebuf->n == 1);
   assert (replacebuf->sz == DEFAULT_MSG);

	/* replace stolen buffer with replacement in app/kernel ring */
   if (sys_pktring_modring (0, demux_id, (struct pktringent *)replacebuf, (struct pktringent *)steal, 1) < 0) {
      printf ("web_tcp_stealpollbuf: modring failed\n");
      assert (0);
   }
/*
printf ("after modring (pollbuflist %p): prev %p, curr %p, next %p\n", pollbuflist, prevbuf, prevbuf->next, prevbuf->next->next);
*/
}


static void web_tcp_returnpollbuf (webtcpbuf_t *retbuf)
{
   if (retbuf->next) {
      retbuf->poll = 0;

   } else {
	/* later, add buffer to app/kernel ring if dropping packets! */
      retbuf->next = extrapollbufs;
      extrapollbufs = retbuf;
   }
}
#endif


/****************** demultiplexing individual connections ******************/

#define WEB_TCP_HASHTABLE_ENTRIES	(4096/sizeof(void *))

struct tcb *timewait_hashtable[WEB_TCP_HASHTABLE_ENTRIES];
struct tcb *webtcp_hashtable[WEB_TCP_HASHTABLE_ENTRIES];

#define	WEB_TCP_HASH(srcip, srctcp) \
		(((((unsigned int) srcip)>>24) + (unsigned short) srctcp) \
			& (WEB_TCP_HASHTABLE_ENTRIES-1))

static void web_tcp_hashinit (struct tcb **hashtable)
{
   int i;

   for (i=0; i<WEB_TCP_HASHTABLE_ENTRIES; i++) {
      hashtable[i] = NULL;
   }
}


static int web_tcp_addnewconn (struct tcb **hashtable, struct tcb *tcb)
{
   int entry = WEB_TCP_HASH (tcb->ipsrc, tcb->tcpsrc);
   struct tcb *temp = hashtable[entry];

/*
   printf ("web_tcp_addnewconn: tcb %x, ipsrc %x, tcpsrc %d, entry %d, temp %x\n", (uint) tcb, tcb->ipsrc, tcb->tcpsrc, entry, (uint)temp);
*/
   if (temp != NULL) {
      do {
	 if ((temp->tcpsrc == tcb->tcpsrc) && (temp->ipsrc == tcb->ipsrc)) {
	    printf ("new connection equals existing connection: temp %x, ipsrc %x, tcpsrc %d\n", (uint) temp, temp->ipsrc, temp->tcpsrc);
	    return (-1);
	 }
	 temp = temp->hash_next;
      } while (temp != hashtable[entry]);
      tcb->hash_next = temp->hash_next;
      tcb->hash_prev = temp;
      temp->hash_next = tcb;
      tcb->hash_next->hash_prev = tcb;
   } else {
      tcb->hash_next = tcb;
      tcb->hash_prev = tcb;
      hashtable[entry] = tcb;
   }
   return (0);
}


static void web_tcp_removeconn (struct tcb **hashtable, struct tcb *tcb)
{
   int entry = WEB_TCP_HASH (tcb->ipsrc, tcb->tcpsrc);
   struct tcb *temp = hashtable[entry];
/*
printf ("web_tcp_removeconn: tcb %x, ipsrc %x, tcpsrc %d, entry %d, temp %x\n", (uint)tcb, tcb->ipsrc, tcb->tcpsrc, entry, (uint)temp);
   assert ((tcb->hash_next) && (tcb->hash_prev) && (temp));
*/
   if ((tcb->hash_next == NULL) || (tcb->hash_prev == NULL) || (temp == NULL)) {
      return;
   }
   tcb->hash_next->hash_prev = tcb->hash_prev;
   tcb->hash_prev->hash_next = tcb->hash_next;
   if (temp == tcb) {
      hashtable[entry] = (tcb->hash_next != tcb) ? tcb->hash_next : NULL;
   }
   tcb->hash_next = NULL;   /* GROK: auxilliary self-prot */
   tcb->hash_prev = NULL;
}


static struct tcb * web_tcp_demultiplex (struct tcb **hashtable, char *buf)
{
   struct tcp *rcv_tcp = (struct tcp *) &buf[WEB_EIT_TCP];
#ifdef CLIENT_TCP
   unsigned int ipsrc = 0;
   unsigned short tcpsrc = rcv_tcp->dst_port;
#else
   struct ip *rcv_ip = (struct ip *) &buf[WEB_EIT_IP];
   unsigned int ipsrc = *((unsigned int *) rcv_ip->source);
   unsigned short tcpsrc = rcv_tcp->src_port;
#endif
   int entry = WEB_TCP_HASH (ipsrc, tcpsrc);
   struct tcb *temp = hashtable[entry];

DPRINTF (2, ("web_tcp_demultiplex: ipsrc %x, tcpsrc %d, entry %d, temp %x\n", ipsrc, tcpsrc, entry, (uint)temp));

   if (temp != NULL) {
      do {
	 if ((temp->ipsrc == ipsrc) && (temp->tcpsrc == tcpsrc)) {
	    return (temp);
	 }
	 temp = temp->hash_next;
      } while (temp != hashtable[entry]);
   }
   return (NULL);
}


/****************** low-overhead TIME_WAIT support *************************/

typedef struct tmwait {
   WEB_TCP_DEMULTIPLEXING_FIELDS
   struct tmwait *next;
   struct tmwait *prev;
   int timeout;
   uint32 snd_next;
   uint32 rcv_next;
   struct eth ethaddr;
} timewaiter_t;

timewaiter_t *extra_timewaiters = NULL;
timewaiter_t *timewait_listhead = NULL;
timewaiter_t *timewait_listtail = NULL;
int timewaiter_cnt = 0;

int tcptime = 0;


void web_tcp_addnewtimewaiter (struct tcb *tcb)
{
   timewaiter_t *new = extra_timewaiters;

   if (new == NULL) {
      int i;
      new = (timewaiter_t *) malloc (4096);
      for (i=0; i < (4096/sizeof(timewaiter_t)); i++) {
	 new[i].next = extra_timewaiters;
	 extra_timewaiters = &new[i];
      }
      new = extra_timewaiters;
   }
   extra_timewaiters = new->next;

#ifdef CLIENT_TCP
   printf ("web_tcp_addnewtimewaiter: shuld this be possible??\n");
   assert (0);
#endif
   new->ipsrc = tcb->ipsrc;
   new->tcpsrc = tcb->tcpsrc;
   new->hash_next = NULL;
   new->hash_prev = NULL;
   new->snd_next = tcb->snd_next;
   new->rcv_next = tcb->rcv_next;
   bcopy (&tcb->snd_recv_r0_data[WEB_EIT_ETH], (char *) &new->ethaddr, sizeof (struct eth));
   new->timeout = tcptime + (2 * TCP_MSL);

   web_tcp_addnewconn (timewait_hashtable, (struct tcb *) new);

   new->next = NULL;
   new->prev = timewait_listtail;
   timewait_listtail = new;
   if (new->prev) {
      new->prev->next = new;
   }
   if (timewait_listhead == NULL) {
      timewait_listhead = new;
   }
   
   timewaiter_cnt++;
}


void web_tcp_freetimewaiter (timewaiter_t *timewaiter)
{
   timewaiter->next = extra_timewaiters;
   extra_timewaiters = timewaiter;
   
   timewaiter_cnt--;
}


/*
 * TODO:
 *   - improve timers (do not poll; update them at each context switch).
 *   - do not use system calls
 */

#ifndef NOTIMERS
/* 
 * Compute the time in ticks on which the timer should expire.
 */
static void timer_set(struct tcb *tcb, unsigned int microseconds)
{
    unsigned int wait_ticks = microseconds / __sysinfo.si_rate;

    tcb->flags &= ~TCB_FL_TIMEOUT;

    if (wait_ticks == 0) wait_ticks += 1; /* wait at least 1 tick */

    tcb->timer_retrans = wait_ticks + ae_gettick();
}


/* 
 * Check timer. If timer expired, set flag in tcb.
 */
static int timer_check(struct tcb *tcb)
{
    int t = ae_gettick();

    if (tcb->timer_retrans > 0 && t >= tcb->timer_retrans) {
	DPRINTF(2, ("tcb %x: time out\n", (uint)tcb));
	printf("timer_check: timeout\n");
	tcb->timer_retrans = 0;
	tcb->flags |= TCB_FL_TIMEOUT;
	return(1);
    }
    return(0);
}
#else
static void timer_set(struct tcb *tcb, unsigned int microseconds) {}
static void timer_check(struct tcb *tcb) {}
#endif


/*
 * Send a packet.
 * We also ack what we have received from the other side by sending the
 * sequence number we expect.
 * We can avoid the copy by using a recv structure with 2 ptrs: (1) header
 * (2) user data.
 */
static int tcp_net_send(struct tcb *tcb, int flag, uint32 seqno, int len, int len2, int datasum)
{
   struct ip *ip = (struct ip *) &tcb->snd_recv_r0_data[WEB_EIT_IP];
   struct tcp *tcp = (struct tcp *) &tcb->snd_recv_r0_data[WEB_EIT_TCP];
   int sum;
    
   if (len + len2) {
      tcb->snd_recv_r1_data = tcb->usr_snd_buf;
      tcb->snd_recv_r1_sz = len;
      tcb->snd_recv_r2_data = tcb->usr_snd_buf2;
      tcb->snd_recv_r2_sz = len2;
      tcb->snd_recv_n = (len2) ? 3 : 2;
   } else {
      tcb->snd_recv_n = 1;
   }
/*
   printf ("send: len %d, flags %x\n", (len+len2), flag);
*/
   DPRINTF(2, ("send_segment %x: cardno %d, datalen %d, len2 %d, flags %x, seqno %u ackno %u wnd %u\n", (uint)tcb, tcb->netcardno, len, len2, flag, tcb->snd_next, tcb->rcv_next, tcb->rcv_wnd));

outpackets++;

   tcp->flags = flag;
   tcp->offset = ((tcb->snd_recv_r0_sz - WEB_EIT_TCP) >> 2) << 4;

   tcp->seqno = htonl(seqno);
   tcp->ackno = htonl(tcb->rcv_next);
   tcp->window = htons(tcb->rcv_wnd);

   tcp->cksum = 0;
#ifdef TCPCKSUM
   /* Fill in pseudo header */
   ip->ttl = 0;
   ip->checksum = htons(tcb->snd_recv_r0_sz - WEB_EIT_TCP + len + len2);

   /* Compute cksum on pseudo header and tcp header */
   sum = (len + len2) ? datasum : 0;
   if (sum == -1) {
      sum = inet_checksum((uint16 *)tcb->snd_recv_r1_data, len, 0, 0);
      if (len2) {
	 sum = inet_checksum ((unsigned short *)tcb->snd_recv_r2_data, len2, sum, 0);
      }
   }
/*
   if (len2) {
      sum = inet_checksum ((unsigned short *)tcb->snd_recv_r2_data, len2, 0, 0);
   }
   if (len) {
      sum = inet_checksum((uint16 *)tcb->snd_recv_r1_data, len, sum, 0);
   }
if (((len + len2) != 0) && (datasum != sum)) {
   printf ("datasum %x, sum %x, len %d, len2 %d, equal %d\n", datasum, sum, len, len2, (datasum == sum));
}
*/
   if ((tcp->cksum = inet_checksum ((unsigned short *) &(ip->ttl), (tcb->snd_recv_r0_sz - WEB_EIT_TCP + IP_PSEUDO_HDR_SIZE), sum, 1)) == 0) {
      tcp->cksum = 0xFFFF;
   }
#endif

    /* Repair ip header, after screwing it up to construct pseudo header. */
    ip->ttl = TCP_TTL;
    ip->totallength = htons(tcb->snd_recv_r0_sz - WEB_EIT_IP + len + len2);

    ip->checksum = 0;

#ifdef IPCKSUM
    /* Compute cksum on ip header */
    if ((ip->checksum = inet_checksum((unsigned short *) &(ip->vrsihl), 
				   sizeof(struct ip), 0, 1)) == 0) {
	ip->checksum = 0xFFFF;
    }
#endif

#ifdef TESTDROP
    /* Check whether we should drop this packet for testing purposes :-) */
    r = random();
    if ((r % 100) < droprate) {
	STINC(ndropsnd);
	return (0);
    }
#endif
   if (ae_eth_send_wrap(tcb->netcardno, (struct ae_recv *) &tcb->snd_recv_n) < 0) {
/*
printf ("ae_eth_send loop\n");
*/
      tcb->flags |= TCB_FL_RESEND;
      return (TCB_FL_RESEND);
      STINC(nethretrans);
   }
   return (0);
}


static void net_copy_user_data(struct tcb *tcb, char *data, int datalen)
{
/*
   printf ("net_copy_user_data: tcb %x, data %x, datalen %d\n", (uint)tcb, (uint)data, datalen);
*/
#ifndef CLIENT_TCP
   assert (datalen <= (tcb->usr_rcv_sz - tcb->usr_rcv_off));

   if (tcb->usr_rcv_buf == NULL) {
      tcb->usr_rcv_buf = data;
   } else {
      memcpy(tcb->usr_rcv_buf+tcb->usr_rcv_off, data, datalen);
   }
#endif
   tcb->usr_rcv_off += datalen;
#ifndef CLIENT_TCP
   tcb->rcv_wnd = tcb->usr_rcv_sz - tcb->usr_rcv_off;
#endif
   assert (tcb->rcv_wnd != 0);
   tcb->rcv_next += datalen;
}


/*
 * TODO: 
 * - detect connection breaks etc. (keep alive segments)
 * - time_wait state has to wait two mss
 *
 * - delayed ack correctly
 * - fix iss
 * - initial port needs to be globally unique (like iss)
 * - implement other timers besides retransmission timer
 * - implement estimate RTT
 * - data on a syn segment is deleted
 * - implement congestion control
 * - determine MSS dynamically.
 * - implement reassembly (not really needed)
 */

static void st_illegal(struct tcb *tcb);
static void st_syn_sent(struct tcb *tcb);
static void st_syn_received(struct tcb *tcb);
static void st_established(struct tcb *tcb);
static void st_fin_wait1(struct tcb *tcb);
static void st_fin_wait2(struct tcb *tcb);
static void st_close_wait(struct tcb *tcb);
static void st_closing(struct tcb *tcb);
static void st_last_ack(struct tcb *tcb);
static void st_time_wait(struct tcb *tcb);

static void (*state[NSTATES])(struct tcb *tcb) = {
    st_illegal,
    st_illegal,
    st_syn_sent,
    st_syn_received,
    st_established,
    st_fin_wait1,
    st_fin_wait2,
    st_close_wait,
    st_closing,
    st_last_ack,
    st_time_wait,
    st_illegal,
};

/*----------------------------------------------------------------------------*/

#ifndef NOSTATISTICS
/*
 * Computes log2 of size.
 */
static int log2(int size)
{
    int r = 0;
    
    size = size >> 1;

    while (size != 0) {
	size = size >> 1;
	r++;
    }

    demand(r < NLOG2, log2: we cannot deal with this result);

    return r;
}
#endif

/* 
 * Print tcp header.
 */
static void tcp_print(struct tcp *tcp)
{
    printf("tcp: src_port %u dst_port %u\n", tcp->src_port, tcp->dst_port);
    printf("seqno %u\n", tcp->seqno);
    printf("ackno %u\n", tcp->ackno);
    printf("offset 0x%x flags 0x%x\n", tcp->offset, tcp->flags);
    printf("window %u cksum 0x%x\n", tcp->window, tcp->cksum);
    printf("\n");
}


/*
 * Print salient part of the header.
 */
static void tcp_print_seq(struct tcp *tcp)
{
    printf("tcp: seqno %u ackno %u window %u flags 0x%x\n",
	   tcp->seqno, tcp->ackno, tcp->window, tcp->flags);
}


/*
 * Print salient part of TCB.
 */
static void tcb_print(struct tcb *tcb)
{
    printf("tcb %d(%d): una %u snext %u swnd %u wl1 %u wl2 %u rnext %u rwnd %u \n", 
	   (uint)tcb,
	   tcb->state, 
	   tcb->snd_una, tcb->snd_next, tcb->snd_wnd, 
	   tcb->snd_wls, tcb->snd_wla,
	   tcb->rcv_next, tcb->rcv_wnd); 
}


/*----------------------------------------------------------------------------*/

/* 
 * Change to ST_CLOSED; This state is used to signal the client that we are
 * done. Tcb_release, which should be called by the client, will release the 
 * storage and change the state to ST_FREE.
 */
static void tcp_done(struct tcb *tcb)
{
   DPRINTF(2, ("tcp_done: %x go to ST_CLOSED\n", (uint)tcb));

   if (tcb->state == TCB_ST_TIME_WAIT) {
      web_tcp_addnewtimewaiter (tcb);
   }
   
   web_tcp_removeconn (webtcp_hashtable, tcb);
   if (tcb->usr_rcv_buf != NULL) {
#ifdef RINGBUFFER
      web_tcp_returnpollbuf (tcb->pollbuf);
#else
      web_tcp_resetpollbuf (tcb->pollbuf);
#endif
      tcb->pollbuf = NULL;   /* wasteful self-protection */
   }
   tcb->timer_retrans = 0;
   tcb->flags = TCB_FL_RESET;
   tcb->state = TCB_ST_CLOSED;
}


/*
 * First check of RFC: check is the segment is acceptable.
 */
static int process_acceptable(struct tcb *tcb)
{
    struct ip *ip = (struct ip *) &tcb->nextdata[WEB_EIT_IP];
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];
    int len;

    len = ip->totallength - sizeof(struct ip) - sizeof(struct tcp);

    /* Acceptable? */
    if (
	((len == 0) && (tcb->rcv_wnd == 0) && (tcp->seqno == tcb->rcv_next)) ||

	((len == 0) && (tcb->rcv_wnd > 0) && 
	 (SEQ_GEQ(tcp->seqno, tcb->rcv_next) && 
	  SEQ_LT(tcp->seqno, tcb->rcv_next + tcb->rcv_wnd))) ||

	((len > 0) && (tcb->rcv_wnd > 0) &&
	 ((SEQ_GEQ(tcp->seqno, tcb->rcv_next) && 
	   SEQ_LT(tcp->seqno, tcb->rcv_next + tcb->rcv_wnd)) ||
	  (SEQ_GEQ(tcp->seqno + len - 1, tcb->rcv_next) &&
	   SEQ_LT(tcp->seqno + len - 1, tcb->rcv_next + tcb->rcv_wnd))))) {

	STINC(naccept);

	/* Yes, acceptable */
	return 1;
    } else {
	/* No, not acceptable: if not rst, send ack. */

	STINC(nunaccept);

	printf("process_acceptable %d: segment %d not accepted; expect %d (tcb->state %d)\n", len, tcp->seqno, tcb->rcv_next, tcb->state);
	if (!(tcp->flags & TCP_FL_RST)) {
	    STINC(nacksnd);
	    tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, -1);
	}
	return 0;
    }
}


/*
 * Check 4 and first part of 5 of RFC.
 */
static int process_syn_ack(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    if (tcp->flags & TCP_FL_SYN) {
	STINC(nsynrcv);
	STINC(nrstsnd);
	if (tcp->flags & TCP_FL_ACK) 
	    tcp_net_send(tcb, TCP_FL_RST|TCP_FL_ACK, tcp->ackno, 0, 0, -1);
	else
	    tcp_net_send(tcb, TCP_FL_RST, 0, 0, 0, -1);
	tcp_done(tcb);
	return 0;
    }

    if (!(tcp->flags & TCP_FL_ACK)) return 0;

    return 1;
}


/*
 * Check 2 of RFC and call function for checks 4 and first part 5. Check 3
 * is for security stuff, which we do not implement.
 */
static int process_rst_syn_ack(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    if (tcp->flags & TCP_FL_RST) {
	STINC(nrstrcv);
	tcp_done(tcb);
	return 0;
    }

    return process_syn_ack(tcb);
}


/* 
 * The last part of check 5 of RFC.
 */
static void process_ack(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    STINC(nack);

    /* If an acknowledgement and it is interesting, update una. */
    if ((tcp->flags & TCP_FL_ACK) && SEQ_GEQ(tcp->ackno, tcb->snd_una)
	&& SEQ_LEQ(tcp->ackno, tcb->snd_next)) { 
	DPRINTF(2, ("process_ack %x ack %d: una %u next %u\n", 
		    (uint)tcb, tcp->ackno, tcb->snd_una, tcb->snd_next));
	tcb->snd_una = tcp->ackno;
	/* Reschedule time out event; we have received an interesting ack. */
	timer_set(tcb, TCP_TIMEOUT);
    }
    
    /* If an acknowledgement and it is interesting, update snd_wnd. */
    if ((tcp->flags & TCP_FL_ACK) && 
	(SEQ_LT(tcb->snd_wls, tcp->seqno) ||
	 (tcb->snd_wls == tcp->seqno && SEQ_LT(tcb->snd_wla, tcp->ackno)) ||
	 (tcb->snd_wla == tcp->ackno && tcp->window > tcb->snd_wnd))) {
	DPRINTF(2, ("process_ack %x: old wnd %u wnd update %u\n", 
		    (uint)tcb, tcb->snd_wnd, tcp->window));
	tcb->snd_wnd = tcp->window;
	tcb->snd_wls = tcp->seqno;
	tcb->snd_wla = tcp->ackno;
    }
}


/* 
 * Deal with options. We check only for MSS option. Heh, processing
 * one option is better than processing no options.
 * If there are no options, TCP_MSS is used and process_options returns 0;
 */
static int process_options(struct tcb *tcb, char *option)
{
    uint16 mss;

    DPRINTF(2, ("process_options: options present %d\n", (int) option[0]));
    if (option[0] == TCP_OPTION_MSS) {
	switch(option[1]) {
	case 4:			/* kind, length, and a 2-byte mss */
	    memcpy(&mss, &option[2], sizeof(short));
	    mss = ntohs(mss);
	    DPRINTF(2, ("proposed mss %d we %d\n", mss, DEFAULT_MSS));
	    tcb->mss = min(DEFAULT_MSS, mss);
	    return 1;
	default:
	    DPRINTF(2, ("process_option: unrecognized length %d\n", option[1]));
	    return 0;
	}
    }
    return 0;
}


/*
 * Process incoming data. fin records is a the last byte has been sent.
 * Process_data returns whether all data has been received
 */
static int process_data(struct tcb *tcb, int fin)
{
    struct ip *ip = (struct ip *) &tcb->nextdata[WEB_EIT_IP];
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];
    char *data = &tcb->nextdata[WEB_EIT_DATA];
    int datalen;
    uint32 off;
    int all;
    int n;

    all = 0;
    datalen = ip->totallength - sizeof(struct ip) - sizeof(struct tcp);

    if (fin) STINC(nfinrcv);
    if (datalen > 0) {
	STINC_SIZE(rcvdata, log2(datalen));
	STINC(ndatarcv);
    }

    DPRINTF(2, ("process_data: %d bytes user data\n", datalen));

    if (SEQ_LEQ(tcp->seqno, tcb->rcv_next) && 
	SEQ_GEQ(tcp->seqno + datalen, tcb->rcv_next)) {
	/* Segment contains interesting data */
	if (datalen > 0) {

	    /* Copy the right data */
	    STINC(norder);
	    datalen = tcp->seqno + datalen - tcb->rcv_next;
	    n = datalen;
	    off = tcb->rcv_next - tcp->seqno;
	    net_copy_user_data(tcb, data + off, datalen);

	    /* Check for a fin */
	    if (fin && n == datalen) {
		DPRINTF(2, ("process_data: process fin\n"));
		tcb->rcv_next++;
		all = 1;
		STINC(nacksnd);
		tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, -1);
	    } else {
#ifdef CLIENT_TCP
		if (tcb->flags & TCB_FL_DELACK) {
		   STINC(nacksnd);
		   tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, -1);
		} else {
#endif
		/* Delay ack */
		STINC(ndelayack);
#ifdef CLIENT_TCP
		}
#endif
	    }

	} else if (fin) {
	    /* A segment just carrying fin. */
	    DPRINTF(2,("process_data: process fin\n"));
	    tcb->rcv_next++;
	    all = 1;
	    STINC(nacksnd);
	    tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, -1);
	}
    } else if (SEQ_GT(tcp->seqno, tcb->rcv_next)) {
	/* Segment contains data passed rcv_next, keep it.
	 * (For now, drop it.).
	 */
	STINC(noutorder);
	DPRINTF(2, ("process_data %x: segment seq %d is out of order n %d\n", 
		    (uint)tcb, tcp->seqno, tcb->rcv_next));
    } else {
	demand(0, process_acceptable should have filtered this segment);
    }
    return all;
}

/*----------------------------------------------------------------------------*/


static void st_illegal(struct tcb *tcb)
{
    printf("tcp_process %d: illegal state to receive msgs in: %d\n", 
	   (uint)tcb, tcb->state);

    tcb_print(tcb);
}


/*
 * We have sent our syn. Wait for an ack and a syn from the other side.
 */
static void st_syn_sent(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];
    char *option = &tcb->nextdata[WEB_EIT_DATA];
    int hlen;

    DPRINTF(3, ("st_synsent\n"));

    /* Check ack; if not acceptable send rst */
    if ((tcp->flags & TCP_FL_ACK) && (SEQ_LEQ(tcp->ackno, tcb->snd_iss) ||
				      SEQ_GT(tcp->ackno, tcb->snd_next))) {
	STINC(nrstsnd);
	tcp_net_send(tcb, TCP_FL_RST|TCP_FL_ACK, tcp->ackno, 0, 0, -1);
	return;
    }

    /* Check for rst, and drop the connection */
    if (tcp->flags & TCP_FL_RST) {
	STINC(nrstrcv);
	tcp_done(tcb);						      
	return;
    }

    /* Check for syn and set irs and next */
    if (tcp->flags & TCP_FL_SYN) {
	STINC(nsynrcv);
	tcb->rcv_irs = tcp->seqno;
	tcb->rcv_next = tcp->seqno + 1; /* y + 1 */
    }

    /* Acceptable ack, set una and window variables */
    if (tcp->flags & TCP_FL_ACK) {
	STINC(nack);
	tcb->snd_una = tcp->ackno;
	tcb->snd_wnd = tcp->window;
	tcb->snd_wls = tcp->seqno;
    }

    /* Check if our syn has been acked, if so go to ESTABLISHED. */
    if (SEQ_GT(tcb->snd_una, tcb->snd_iss)) {
	/* Our syn has been acked */
	DPRINTF(2, ("st_syn_sent %x: go to ESTABLISHED\n", (uint)tcb));
	tcb->state = TCB_ST_ESTABLISHED;

	/* Reset timer */
	tcb->timer_retrans = 0;

	hlen = ((tcp->offset >> 4) & 0xF) << 2;
	if (hlen > sizeof(struct tcp)) { /* options? */
	    process_options(tcb, option);
	}

	/* Complete handshake: send ack */
	STINC(nacksnd);
	tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, -1);
    } else {
	/* Received a syn on our syn but without ack, send ack on other side's
	 * syn.
	 */
	DPRINTF(2, ("st_syn_sent %x: go to SYN_RECEIVED\n", (uint)tcb));
	tcb->state = TCB_ST_SYN_RECEIVED;
	/* Send seq == iss, ack == rcv_next. Perhaps send options too. */
	STINC(nacksnd);
	STINC(nsynsnd);
	tcp_net_send(tcb, TCP_FL_SYN|TCP_FL_ACK, tcb->snd_iss, 0, 0, -1);
    }
}



/*
 * We have received a syn s = x, we have sent syn y ack x +1, and are now
 * waiting for ack s = x, a = y + 1.
 * Or, we have sent a syn, and have received a syn, but our still
 * waiting for the ack on our syn.
 */
static void st_syn_received(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    DPRINTF(3, ("st_synrecv (flags %x)\n", tcp->flags));

    if (!process_acceptable(tcb)) return;

    if (!process_rst_syn_ack(tcb)) return;

    /* If acceptable ack, move to established; otherwise send reset */
    if ((tcp->flags & TCP_FL_ACK) && SEQ_GEQ(tcp->ackno, tcb->snd_una)
	&& SEQ_LEQ(tcp->ackno, tcb->snd_next)) {
	DPRINTF(2, ("st_syn_received: go to ESTABLISHED\n"));
	tcb->snd_una = tcp->ackno;
	tcb->snd_wnd = tcp->window;
	tcb->snd_wls = tcp->seqno;
	tcb->snd_wla = tcp->ackno;
	tcb->state = TCB_ST_ESTABLISHED;
    } else {
	DPRINTF(2, ("st_syn_received: send RST\n"));
	while (tcp_net_send(tcb, TCP_FL_RST, tcp->ackno, 0, 0, -1) == TCB_FL_RESEND);
    }
    
    process_ack(tcb);

    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	DPRINTF(2, ("st_syn_received: go to ST_CLOSE_WAIT (flags %x)\n", tcp->flags));
	tcb->state = TCB_ST_CLOSE_WAIT;
    }
}


/*
 * Connection is established; deal with incoming packet.
 */
static void st_established(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    DPRINTF(3, ("st_established\n"));

    if (!process_acceptable(tcb)) return;

    if (!process_rst_syn_ack(tcb)) return;

    process_ack(tcb);

    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	DPRINTF(2, ("st_established: go to ST_CLOSE_WAIT\n"));
	tcb->state = TCB_ST_CLOSE_WAIT;
    }
}


/*
 * We have received a fin and acked it, but our side hasn't sent a fin
 * yet, since tcp_close has not been called yet.
 * Update the send window; we could still be sending data.
 */
static void st_close_wait(struct tcb *tcb)
{
   DPRINTF(3, ("st_closewait\n"));

   if ((process_acceptable(tcb)) && (process_rst_syn_ack(tcb))) {
      process_ack(tcb);
   }
}


/*
 * We have called tcp_close and we are waiting for an ack.
 * Process data and wait for the other side to close.
 */
static void st_fin_wait1(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    DPRINTF(3, ("st_finwait1: tcp->flags %x\n", tcp->flags));
    
    if (!process_acceptable(tcb)) return;

    if (!process_rst_syn_ack(tcb)) return;

    process_ack(tcb);

    if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
	DPRINTF(2, ("st_fin_wait1 %x: go to FIN_WAIT2\n", (uint)tcb));
	tcb->state = TCB_ST_FIN_WAIT2;
    }

    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	if(tcb->state == TCB_ST_FIN_WAIT2) {
	    DPRINTF(2, ("st_fin_wait1 %x: go to TIME_WAIT\n", (uint)tcb));
	    tcb->state = TCB_ST_TIME_WAIT;
	} else {
	    DPRINTF(2, ("st_fin_wait1 %x: go to CLOSING\n", (uint)tcb));
	    tcb->state = TCB_ST_CLOSING;
	}
    }
}


/*
 * We have sent a fin and received an ack on it.
 * Process data and wait for the other side to close.
 */
static void st_fin_wait2(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];
    
    DPRINTF(3, ("st_finwait2: tcp->flags %x\n", tcp->flags));

    if (!process_acceptable(tcb)) return;
    
    if (!process_rst_syn_ack(tcb)) return;
    
    process_ack(tcb);
    
    if (process_data(tcb, tcp->flags & TCP_FL_FIN)) {
	DPRINTF(2, ("st_fin_wait2 %x: go to TIME_WAIT\n", (uint)tcb));
	tcb->state = TCB_ST_TIME_WAIT;
    }
}



/*
 * Both sides have sent fin. We have acked the other side's fin.
 * Wait until our fin is acked.
 */
static void st_last_ack(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];
    
    DPRINTF(3, ("st_lastack\n"));
    
    if (!process_acceptable(tcb)) return;

    if (tcp->flags & TCP_FL_RST) {
	tcp_done(tcb);
    }

    if (!process_syn_ack(tcb)) return;
    
    process_ack(tcb);
    
    if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
	DPRINTF(2, ("st_last_ack %x: go to ST_CLOSED\n", (uint)tcb)); 
	tcp_done(tcb);
    }
}


/*
 * We sent out a fin and have received a fin in response. We
 * are waiting for the ack.
 */
static void st_closing(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    DPRINTF(3, ("st_closting\n"));

    if (!process_acceptable(tcb)) return;

    if (tcp->flags & TCP_FL_RST) {
	tcp_done(tcb);
    }

    if (!process_syn_ack(tcb)) return;

    process_ack(tcb);

    if (SEQ_GEQ(tcb->snd_una, tcb->snd_next)) {
	DPRINTF(2, ("st_closing %x: received ack segment, to to TIME_WAIT\n", 
	       (uint)tcb));
	tcb->state = TCB_ST_TIME_WAIT;
    }
}


/*
 * We have sent a fin and have received an ack on it. We also have
 * received a fin from the other side and acked it.
 */
static void st_time_wait(struct tcb *tcb)
{
    struct tcp *tcp = (struct tcp *) &tcb->nextdata[WEB_EIT_TCP];

    DPRINTF(3, ("st_timewait\n"));

    if (!process_acceptable(tcb)) return;

    if (tcp->flags & TCP_FL_RST) {
	tcp_done(tcb);
    }

    if (!process_syn_ack(tcb)) return;

    process_ack(tcb);

    /* Check for a retransmission of a fin, and send an ack. */
    if (tcp->flags & TCP_FL_FIN) {
	tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, -1);
    }
}


static int tcp_process_packetcheck (char *packetdata, int len)
{
   struct tcp *rcv_tcp = (struct tcp *) &packetdata[WEB_EIT_TCP];
   struct ip *rcv_ip = (struct ip *) &packetdata[WEB_EIT_IP];
#ifndef CLIENT_TCP
   char *data = &packetdata[WEB_EIT_DATA];
   uint16 n;
   long sum;
#endif

   if (rcv_ip->checksum != 0) {
      /* Check ip cksum */
      if ((rcv_ip->checksum = inet_checksum((unsigned short *) &(rcv_ip->vrsihl), sizeof(struct ip), 0, 1))) {
	 STINC(nbadipcksum);
	 DPRINTF(2, ("tcp_process_packetcheck: bad ip checksum\n"));
	 printf ("tcp_process_packetcheck: bad ip checksum\n");
	 return(0);
      }
   }

   rcv_ip->totallength = ntohs(rcv_ip->totallength);
/*
   rcv_ip->fragoffset = ntohs(rcv_ip->fragoffset);
*/

   STINC(nseg);

   /* We expect that tcp segments will never be fragmented since
    * the other end is not to send something bigger than the negotiated
    * MSS.
    */
   if (rcv_ip->totallength > len || rcv_ip->fragoffset != 0) {
      /* Segment is not complete */
      STINC(nincomplete);
      DPRINTF(2, ("tcp_proces_packetcheck: segment with len %d off %d is not complete\n", rcv_ip->totallength, rcv_ip->fragoffset));
      printf ("tcp_proces_packetcheck: segment with len %d off %d is not complete\n", rcv_ip->totallength, rcv_ip->fragoffset);
      return(0);
   }

#ifndef CLIENT_TCP
   if (rcv_tcp->cksum != 0) {
      /* Repair pseudo header */
      n = rcv_ip->totallength - sizeof(struct ip);
      rcv_ip->ttl = 0;
      rcv_ip->checksum = htons(n);

      /* Check checksum on pseudo header */
      sum = inet_checksum((unsigned short *) &(rcv_ip->ttl), IP_PSEUDO_HDR_SIZE + sizeof(struct tcp), 0, 0);
      if ((rcv_tcp->cksum = inet_checksum((unsigned short *) data, n - sizeof(struct tcp), sum, 1))) {
	 STINC(nbadtcpcksum);
	 DPRINTF(2, ("tcp_process_packetcheck: bad tcp inet checksum\n"));
	 printf ("tcp_process_packetcheck: bad tcp inet checksum\n");
	 return(0);
      }
   }
#endif
    
   rcv_tcp->ackno = ntohl(rcv_tcp->ackno);
   rcv_tcp->seqno = ntohl(rcv_tcp->seqno);
   rcv_tcp->window = ntohs(rcv_tcp->window);

   return (1);
}


/*
 * Process a segment. The segment has been received in rcv_eit.
 */
static void tcp_process(struct tcb *tcb, char *packetdata, int len)
{
    struct tcp *tcp = (struct tcp *) &packetdata[WEB_EIT_TCP];
    struct ip *rcv_ip = (struct ip *) &packetdata[WEB_EIT_IP];
    char *data = &packetdata[WEB_EIT_DATA];
    int datalen;

DPRINTF (2, ("tcp_process: tcb %x, r %x, len %d, tcp %x, ip %x\n", (uint)tcb, (uint)packetdata, len, (uint)tcp, (uint)rcv_ip));

    if (tcp_process_packetcheck (packetdata, len) == 0) {
       return;
    }

DPRINTF (2, ("tcp_process: src_port %d, ackno %u, seqno %u, window %u\n", (uint)ntohs(tcp->src_port), tcp->ackno, tcp->seqno, tcp->window));
DPRINTF (4, ("for comparison: tcb->snd_una %d, tcb->snd_wnd %d, tcb->snd_wls %d, tcb->snd_wla %d\n", tcb->snd_una, tcb->snd_wnd, tcb->snd_wls, tcb->snd_wla));

    tcb->retries = 0;		/* reset retries */

    /* Header Prediction */
    if ((tcb->state == TCB_ST_ESTABLISHED) && 
	((tcp->flags & (TCP_FL_FIN | TCP_FL_SYN | TCP_FL_RST | TCP_FL_URG |
		       TCP_FL_ACK)) == TCP_FL_ACK) &&
	(tcp->seqno == tcb->rcv_next)) {

	datalen = rcv_ip->totallength - sizeof(struct ip) - sizeof(struct tcp);

	/* Test for expected ack */
	if (SEQ_GEQ(tcp->ackno, tcb->snd_una) && SEQ_LEQ(tcp->ackno, 
							 tcb->snd_next)) {
	    STINC(nackprd);
	    tcb->snd_una = tcp->ackno;
	    timer_set(tcb, TCP_TIMEOUT);
	    /* If an acknowledgement and it is interesting, update snd_wnd. */
	    if (
	     (tcp->flags & TCP_FL_ACK) && 
	     (SEQ_LT(tcb->snd_wls, tcp->seqno) ||
	     (tcb->snd_wls == tcp->seqno && SEQ_LT(tcb->snd_wla, tcp->ackno)) ||
	     (tcb->snd_wla == tcp->ackno && tcp->window > tcb->snd_wnd))) {
		tcb->snd_wnd = tcp->window;
		tcb->snd_wls = tcp->seqno;
		tcb->snd_wla = tcp->ackno;
	    }

	    /* If datalen is 0, it was a pure ack; return. */
	    if (datalen == 0) return;
	}

	/* Test for expected data */
	if (datalen > 0 && tcp->ackno == tcb->snd_una) {
	    STINC(ndataprd);
	    STINC_SIZE(rcvdata, log2(datalen));
	    net_copy_user_data(tcb, data, datalen);

#ifdef CLIENT_TCP
	    if (tcb->flags & TCB_FL_DELACK) {
		STINC(nacksnd);
		tcp_net_send(tcb, TCP_FL_ACK, tcb->snd_next, 0, 0, -1);
	    } else {
#endif
	    tcb->flags |= TCB_FL_DELACK;
	    STINC(ndelayack);
#ifdef CLIENT_TCP
	    }
#endif

	    return;
	} 
    }

    state[tcb->state](tcb);
}


int web_tcp_process_listenbuf(char *buf, int readylen);

static int tcp_net_poll (struct tcb *tcb)
{
   int ret = 0;

   if ((tcb) && (tcb->flags & TCB_FL_RESEND)) {
/*
printf ("resending\n");
*/
      if (ae_eth_send_wrap (tcb->netcardno, (struct ae_recv *) &tcb->snd_recv_n) < 0) {
	 return (0);
      }
      tcb->flags &= ~TCB_FL_RESEND;
   }

#ifndef RINGBUFFER
   if (pollbuflist == NULL) {
      printf ("HEY!  You've run out of pollbufs\n");
      assert (0);
   }
/*
   printf ("pollbuflist %p, pollbuflist->poll %p, pollbvflist->poll->flag %d\n", pollbuflist, pollbuflist->poll, pollbuflist->poll->flag);
*/

#else
/*
   printf ("pollbuflist %p, pollbuflist->poll %d\n", pollbuflist, pollbuflist->poll);
*/
#endif

#ifdef RINGBUFFER
   while (pollbuflist->poll > 0) {
#else
   while ((pollbuflist) && (pollbuflist->poll->flag >= 0)) {
#endif
      webtcpbuf_t *nextbuf;
#ifdef RINGBUFFER
      webtcpbuf_t *prevbuf;
#endif
      struct tcb *owntcb;
/*
#ifdef RINGBUFFER
printf ("tcp_net_poll: pollbuflist %p, len %d\n", pollbuflist, pollbuflist->poll);
#else
printf ("tcp_net_poll: pollbuflist %p, pollbuflist->poll %x, len %d\n", pollbuflist, (uint) pollbuflist->poll, pollbuflist->poll->flag);
#endif
*/
inpackets++;
#ifdef RINGBUFFER
      nextbuf = pollbuflist;
      pollbuflist = nextbuf->next;
      assert (pollbuftail->next == nextbuf);
      prevbuf = pollbuftail;
      pollbuftail = nextbuf;
      assert (pollbuflist);
#else
      nextbuf = web_tcp_getfrompolllist (&pollbuflist, &pollbuftail);
      nextbuf->n = nextbuf->poll->flag;
#ifdef CLIENT_TCP
      bcopy (nextbuf->poll->recv.r[0].data, nextbuf->data, min(nextbuf->n,64));
#else
      bcopy (nextbuf->poll->recv.r[0].data, nextbuf->data, nextbuf->n);
#endif
/*
printf ("recv: len %d, flags %x, addr %p\n", nextbuf->n, ((struct tcp *) &nextbuf->data[WEB_EIT_TCP])->flags, nextbuf->poll);
*/
      ae_eth_release_pkt (nextbuf->poll);
      nextbuf->poll = NULL;
#endif
      owntcb = web_tcp_demultiplex (webtcp_hashtable, nextbuf->data);

      if (owntcb) {
	 owntcb->nextdata = nextbuf->data;
#ifdef RINGBUFFER
	 tcp_process (owntcb, nextbuf->data, nextbuf->poll);
#else
	 tcp_process (owntcb, nextbuf->data, nextbuf->n);
#endif
	 if (((char *) nextbuf < owntcb->usr_rcv_buf) && ((owntcb->usr_rcv_buf - sizeof(webtcpbuf_t)) < (char *) nextbuf)) {
	    assert (owntcb->pollbuf == NULL);
	    owntcb->pollbuf = nextbuf;
#ifdef RINGBUFFER
	    web_tcp_stealpollbuf (nextbuf, webtcp_demuxid, prevbuf);
	 } else {
	    web_tcp_returnpollbuf (nextbuf);
#else
	    web_tcp_resetpollbuf (NULL);
	 } else {
	    web_tcp_resetpollbuf (nextbuf);
#endif
	 }
	 ret = (owntcb == tcb) ? 1 : 0;
      } else if ((owntcb = web_tcp_demultiplex (timewait_hashtable, nextbuf->data)) != NULL) {
	 printf ("tcp_net_poll: NEED TO HANDLE TIME_WAIT HITS\n");
#ifdef RINGBUFFER
	 web_tcp_returnpollbuf (nextbuf);
#else
	 web_tcp_resetpollbuf (nextbuf);
#endif
      } else {
#ifdef CLIENT_TCP
	 printf ("Throwing away packet that doesn't match any TCB\n");
#else
#ifdef RINGBUFFER
	 web_tcp_process_listenbuf (nextbuf->data, nextbuf->poll);
	 web_tcp_returnpollbuf (nextbuf);
#else
	 web_tcp_process_listenbuf (nextbuf->data, nextbuf->n);
	 web_tcp_resetpollbuf (nextbuf);
#endif
#endif
      }
   }

   return (ret);
}


/*----------------------------------------------------------------------------*/
/*
 * Synchronous write.
 */
static inline int tcp_write(struct tcb *tcb, void *addr, uint sz, void *addr2, uint sz2, int close, int *more, int *checksums)
{
    int data_len;
    int n;
    int window;
    int len1;
    int packno = 0;
    int sum;

    STINC(nwrite);

    /* We are allowed to send on a connection, even if the other side closed */
    if ((tcb->state != TCB_ST_ESTABLISHED) && (tcb->state != TCB_ST_CLOSE_WAIT)) {
       printf ("tcb->state %d\n", tcb->state);
    }
    demand(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_CLOSE_WAIT,
       tcp_write: bad tcb);
    demand( (int) tcb->snd_recv_r0_data % 4 == 2, tcp_write: p not proper aligned);

    tcb->usr_snd_buf = addr;
    tcb->usr_snd_buf2 = addr2;

/* GROK - can be used to disable "combining"
if ((close) && (sz2)) { close = 0; }
sz2 = 0;
*/

    n = sz + sz2;
    tcb->retries = 0;

	DPRINTF(2, ("tcp_write %x: n %d una %u next %u snd_wnd %d\n", 
		    (uint)tcb, n, tcb->snd_una, tcb->snd_next, tcb->snd_wnd));

	/* Compute first how much space is left in the window. */
	window = tcb->snd_una + tcb->snd_wnd - tcb->snd_next;

DPRINTF (4, ("tcp_write: n %d, window %d, mss %d, close %d\n", n, window, tcb->mss, close));

	/* Fire a number of MSS segments to other side, as long as the
	 * send window has space.
	 */
/*
	while (n > 0 && window > 1) {
	while (n > 0 && window >= tcb->mss) {
*/
#ifdef CLIENT_TCP
	while (n > 0) {
#else
	while (((n > 0) && ((min(window,n) >= tcb->mss) || ((close) && (window >= n)))) || ((close) && (sz == 0) && (sz2 == 0))) {
#endif

	    /* Compute number of bytes on segment: never more than the window
	     * allows us and never more than 1 MSS.
	     */
/*
	    data_len = min(n, window);
	    data_len = min(data_len, tcb->mss);
*/
	    data_len = min(n, tcb->mss);
	    data_len = (data_len == n) ? n : (data_len & ~(0x1));
	    demand(((data_len > 0) || ((n == 0) && (close))), tcp_write: expect wnd > 0);

	    if ((n <= sz2) || (n >= (sz2 + data_len))) {
	       len1 = data_len;
	    } else {
	       len1 = n - sz2;
	    }
	    /* If data_len is smaller than some value, we should probably
	     * wait if sending to avoid silly window syndrom.
	     */

	    STINC(ndata);
	    STINC_SIZE(snddata, log2(data_len));

	    /* quick and dirty -- can be prettier */
	    if (checksums) {
	       sum = checksums[packno];
	    } else {
	       sum = 0;
	       if (data_len-len1) {
		  sum = inet_checksum ((unsigned short *)tcb->usr_snd_buf2, (data_len-len1), 0, 0);
	       }
	       if (len1) {
		  sum = inet_checksum((uint16 *)tcb->usr_snd_buf, len1, sum, 0);
	       }
	    }

	    if (close && (data_len == n)) {
	       if (tcp_net_send(tcb, (TCP_FL_ACK | TCP_FL_PSH | TCP_FL_FIN), tcb->snd_next, len1, (data_len-len1), sum) == TCB_FL_RESEND) {
		  tcb->flags &= ~TCB_FL_RESEND;
		  return (sz + sz2 - n);
	       }
	       tcb->snd_next++;
	       close = 0;
	       if (tcb->state == TCB_ST_CLOSE_WAIT) {
		  DPRINTF(2, ("tcp_close %x: go to LAST_ACK\n", (uint)tcb));
		  tcb->state = TCB_ST_LAST_ACK;
	       } else {
		  DPRINTF(2, ("tcp_close %x: go to FIN_WAIT1\n", (uint)tcb));
		  tcb->state = TCB_ST_FIN_WAIT1;
	       }
	    } else {
	       if (tcp_net_send(tcb, (TCP_FL_ACK | TCP_FL_PSH), tcb->snd_next, len1, (data_len-len1), sum) == TCB_FL_RESEND) {
		  tcb->flags &= ~TCB_FL_RESEND;
		  return (sz + sz2 - n);
	       }
	    }

	    timer_set(tcb, TCP_TIMEOUT);

	    if ((n > sz2) && ((n - data_len) <= sz2)) {
	       tcb->usr_snd_buf = tcb->usr_snd_buf2 + (data_len - len1);
	    } else {
	       tcb->usr_snd_buf += data_len;
	    }

	    window -= data_len;
	    n -= data_len;
	    tcb->snd_next += data_len;
	    packno++;
	}

	return (sz+sz2-n);

#if 0
	    if (tcb->flags & TCB_FL_TIMEOUT) { /* timeout? */
		tcb->flags &= ~TCB_FL_TIMEOUT;

		tcb->retries++;
		if (tcb->retries > TCP_RETRY) { /* is the connection broken? */
		    tcp_done(tcb);
/*
		    tcb_release(tcb);
*/
		    return -1;
		}

		/* If window is zero, we have data to send, and everything
		 * has been acknowledged sofar, then send a window proble.
		 *
		 * This code should also take care of the case, when the
		 * receiver shrinks the window (window will be negative or 0).
		 * If the window is negative now, the tests succeed and we will
		 * send a probe.
		 *
		 * If not all data has been acked, we consider it a 
		 * retransmission, and reset snd_next. If the window has
		 * shrunk and the sender cannot retransmit, the code will get 
		 * back here and the sender will send a probe.
		 */
		if (window <= 0 && n > 0 && 
		    SEQ_GEQ(tcb->snd_una, tcb->snd_next)) { 
		    /* Send a window probe */
		    STINC(nprobesnd);

		    tcp_net_send(tcb, TCP_FL_ACK|TCP_FL_PSH, tcb->snd_next, 1, 0, -1);

		    timer_set(tcb, TCP_TIMEOUT);

		    tcb->snd_next++;
		    tcb->usr_snd_off += 1;
		    n -= 1;
		} else {

		    /* Retransmission: set the window back and start sending
		     * again.
		     */

		    /* We should do some congestion control here. Later. */

		    STINC(nretrans);
		    n = done - tcb->snd_una;
		    tcb->usr_snd_off = sz - n;

		    /* Start resending from una. This statement will
		     * also open the send window (if the receiver hasn't
		     * shrunk the window.
		     */
		    tcb->snd_next = tcb->snd_una;
		    break;
		}
	    }
#endif
}


/*
#define TCP_SEP_CLOSE
#ifdef TCP_SEP_CLOSE
*/
/*
 * Close connection: implements another three-way handshake.
 * Two cases:
 *	1. we have received a fin from the other side (CLOSE_WAIT)
 *	2. we are initiating the close and are waiting for a fin (FIN_WAIT1)
 * In both cases, we send a fin. The fin has its own sequence number.
 * Close returns whether the full close is done or only this side closed.
 */
static void tcp_close(struct tcb *tcb)
{
    STINC(nclose);

    if (!(tcb->state == TCB_ST_ESTABLISHED || tcb->state == TCB_ST_CLOSE_WAIT))
    {
	printf("tcp_close: illegal state %d\n", tcb->state);
	return;
    }

    if (tcb->state == TCB_ST_CLOSE_WAIT) {
	DPRINTF(2, ("tcp_close %x: go to LAST_ACK\n", (uint)tcb));
	tcb->state = TCB_ST_LAST_ACK;
    } else {
	DPRINTF(2, ("tcp_close %x: go to FIN_WAIT1\n", (uint)tcb));
	tcb->state = TCB_ST_FIN_WAIT1;
    }

    tcb->retries = 0;

    timer_set(tcb, TCP_TIMEOUT);

    STINC(nfinsnd);
    tcp_net_send(tcb, TCP_FL_ACK|TCP_FL_FIN, tcb->snd_next, 0, 0, -1);

    tcb->snd_next++;	/* fin has a place in sequence space */
}
/*
#endif
*/

#define NOSTATISTICS
#ifndef NOSTATISTICS
/*
 * Print out statistics.
 */
static void tcp_statistics(void)
{
    int i;
    int r;

    printf("Tcp receive statistics:\n");
    printf(" # segments received		  %7d\n", tcp_stat.nseg);
    printf(" # incomplete segments		  %7d\n", tcp_stat.nincomplete);
    printf(" # acceptable segments		  %7d\n", tcp_stat.naccept);
    printf(" # unacceptable segments		  %7d\n", tcp_stat.nunaccept);
    printf(" # acks received			  %7d\n", tcp_stat.nack);
    printf(" # acks predicted			  %7d\n", tcp_stat.nackprd);
    printf(" # data predicted			  %7d\n", tcp_stat.ndataprd);
    printf(" # syns received			  %7d\n", tcp_stat.nsynrcv);
    printf(" # fins received			  %7d\n", tcp_stat.nfinrcv);
    printf(" # resets received			  %7d\n", tcp_stat.nrstrcv);
    printf(" # segments with data		  %7d\n", tcp_stat.ndatarcv);
    printf(" # segments in order		  %7d\n", tcp_stat.norder);
    printf(" # segments buffered		  %7d\n", tcp_stat.nbuf);
    printf(" # segments with data but no usr buf  %7d\n", tcp_stat.noutspace);
    printf(" # segments out of order		  %7d\n", tcp_stat.noutorder);
    printf(" # window probes received		  %7d\n", tcp_stat.nprobercv);
    printf(" # segments with bad ip cksum	  %7d\n", tcp_stat.nbadipcksum);
    printf(" # segments with bad tcp cksum	  %7d\n", tcp_stat.nbadtcpcksum);
    printf(" # segments dropped on receive	  %7d\n", tcp_stat.ndroprcv);
    r = 1;
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", r);
	r = r << 1;
    }
    printf("\n");
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", tcp_stat.rcvdata[i]);
    }
    printf("\n");

    printf("Tcp send statistics:\n");
    printf(" # acks sent			  %7d\n", tcp_stat.nacksnd);
    printf(" # volunteerly acks			  %7d\n", tcp_stat.nvack);
    printf(" # acks sent zero windows		  %7d\n", tcp_stat.nzerowndsnt);
    printf(" # acks delayed			  %7d\n", tcp_stat.ndelayack);
    printf(" # fin segments sent		  %7d\n", tcp_stat.nfinsnd);
    printf(" # syn segments sent		  %7d\n", tcp_stat.nsynsnd);
    printf(" # reset segments sent		  %7d\n", tcp_stat.nrstsnd);
    printf(" # segments sent with data		  %7d\n", tcp_stat.ndata);
    printf(" # window probes send		  %7d\n", tcp_stat.nprobesnd);
    printf(" # ether retransmissions		  %7d\n", tcp_stat.nethretrans);
    printf(" # retransmissions			  %7d\n", tcp_stat.nretrans);
    printf(" # segments dropped on send		  %7d\n", tcp_stat.ndropsnd);
    r = 1;
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", r);
	r = r << 1;
    }
    printf("\n");
    for (i = 0; i < NLOG2; i++) {
	printf("%6d", tcp_stat.snddata[i]);
    }
    printf("\n");

    printf("Tcp call statistics:\n");
    printf(" # tcp_open calls			  %7d\n", tcp_stat.nopen);
    printf(" # tcp_listen calls			  %7d\n", tcp_stat.nlisten);
    printf(" # tcp_accept calls			  %7d\n", tcp_stat.naccepts);
    printf(" # tcp_read calls			  %7d\n", tcp_stat.nread);
    printf(" # tcp_write calls			  %7d\n", tcp_stat.nwrite);
    printf(" # tcp_close calls			  %7d\n", tcp_stat.nclose);
}
/*
#else
void tcp_statistics(void) { }
*/
#endif


uint16 web_tcp_mainportnumber = 0;


#ifdef CLIENT_TCP

#include "netinet/hosttable.h"

/*
 *  Start to open a connection to the predetermined server.
 */
int web_tcp_initiate_connect (struct tcb *new_tcb, int srcportno, map_t *serverinfo, map_t *clientinfo)
{
    char option[TCP_OPTION_MSS_LEN];
    uint16 mss;

    /* set server routing values in snd_recv */
    memcpy(((struct eth *) &new_tcb->snd_recv_r0_data[WEB_EIT_ETH])->dst_addr, serverinfo->eth_addr, sizeof (serverinfo->eth_addr));
    memcpy(((struct ip *) &new_tcb->snd_recv_r0_data[WEB_EIT_IP])->destination, serverinfo->ip_addr, sizeof (serverinfo->ip_addr));
    ((struct tcp *) &new_tcb->snd_recv_r0_data[WEB_EIT_TCP])->dst_port = web_tcp_mainportnumber;

    /* set client routing values in snd_recv */
    memcpy(((struct eth *) &new_tcb->snd_recv_r0_data[WEB_EIT_ETH])->src_addr, clientinfo->eth_addr, sizeof (clientinfo->eth_addr));
    memcpy(((struct ip *) &new_tcb->snd_recv_r0_data[WEB_EIT_IP])->source, clientinfo->ip_addr, sizeof (clientinfo->ip_addr));
    ((struct tcp *) &new_tcb->snd_recv_r0_data[WEB_EIT_TCP])->src_port = srcportno;

    /* Identify ethernet card that received the SYN */
    new_tcb->netcardno = getnetcardno(clientinfo->eth_addr);
    assert (new_tcb->netcardno != -1);
/*
    printf ("netcardno: %d\n", new_tcb->netcardno);
*/

    /* new scheme: demultiplex in user space */
    new_tcb->ipsrc = 0;
    new_tcb->tcpsrc = srcportno;
    if (web_tcp_addnewconn (webtcp_hashtable, new_tcb) == -1) {
       demand (0, unable to add new connection);
    }

    /* Append options to reply. */
    option[0] = TCP_OPTION_MSS;
    option[1] = TCP_OPTION_MSS_LEN;
    new_tcb->mss = DEFAULT_MSS;
    mss = htons(DEFAULT_MSS);
    memcpy(option + 2, &mss, sizeof(uint16));

    /* Send an syn + ack (the second step in the 3-way handshake). */
    memcpy (&new_tcb->snd_recv_r0_data[WEB_EIT_DATA], option, TCP_OPTION_MSS_LEN);
    new_tcb->snd_recv_r0_sz += TCP_OPTION_MSS_LEN;
    while (tcp_net_send (new_tcb, TCP_FL_SYN, new_tcb->snd_iss, 0, 0, -1) != 0) {
       printf ("Must resend SYN request\n");
    }
    new_tcb->snd_recv_r0_sz -= TCP_OPTION_MSS_LEN;

    new_tcb->snd_next = new_tcb->snd_iss + 1;
    new_tcb->snd_una = new_tcb->snd_iss;
    new_tcb->state = TCB_ST_SYN_SENT;

    return (1);
}
#endif /* CLIENT_TCP */


/*
 * Process a segment. The segment has been received in rcv_eit.
 */
int web_tcp_process_listenbuf(char *buf, int readylen)
{
    struct eth *rcv_eth = (struct eth *) buf;
    struct ip *rcv_ip = (struct ip *) &buf[WEB_EIT_IP];
    struct tcp *rcv_tcp = (struct tcp *) &buf[WEB_EIT_TCP];
    char *rcv_data = &buf[WEB_EIT_DATA];
    int hlen;
    char option[TCP_OPTION_MSS_LEN];
    int mss_option = 0;
    uint16 mss;
    struct tcb *new_tcb;

    if (tcp_process_packetcheck (buf, readylen) == 0) {
       printf ("listenbuf: packet check failed -- toss it\n");
       return (0);
    }

DPRINTF (2, ("web_tcp_process_listenbuf: src_port %d, ackno %u, seqno %u, window %u\n", (uint)ntohs(rcv_tcp->src_port), rcv_tcp->ackno, rcv_tcp->seqno, rcv_tcp->window));

    /* Check for rst and ignore */
    if ((rcv_tcp->flags & TCP_FL_RST) || (!(rcv_tcp->flags & TCP_FL_SYN))) {
       printf ("listenbuf: RST or !SYN (flags 0x%x) -- toss it\n", rcv_tcp->flags);
       printf ("web_tcp_process_listenbuf: src_port %d, ackno %u, seqno %u, window %u\n", (uint)ntohs(rcv_tcp->src_port), rcv_tcp->ackno, rcv_tcp->seqno, rcv_tcp->window);
       return (0);
    }

    new_tcb = web_tcp_gettcb ();

    /* set client routing values in snd_recv */
    memcpy(((struct eth *) &new_tcb->snd_recv_r0_data[WEB_EIT_ETH])->dst_addr, rcv_eth->src_addr, sizeof rcv_eth->src_addr);
    ((struct ip *) &new_tcb->snd_recv_r0_data[WEB_EIT_IP])->destination[0] = rcv_ip->source[0];
    ((struct ip *) &new_tcb->snd_recv_r0_data[WEB_EIT_IP])->destination[1] = rcv_ip->source[1];
    ((struct tcp *) &new_tcb->snd_recv_r0_data[WEB_EIT_TCP])->dst_port = rcv_tcp->src_port;

    /* set server routing values in snd_recv */
    memcpy(((struct eth *) &new_tcb->snd_recv_r0_data[WEB_EIT_ETH])->src_addr, rcv_eth->dst_addr, sizeof rcv_eth->dst_addr);
    ((struct ip *) &new_tcb->snd_recv_r0_data[WEB_EIT_IP])->source[0] = rcv_ip->destination[0];
    ((struct ip *) &new_tcb->snd_recv_r0_data[WEB_EIT_IP])->source[1] = rcv_ip->destination[1];

    /* Identify ethernet card that received the SYN */
    new_tcb->netcardno = getnetcardno(rcv_eth->dst_addr);
    assert (new_tcb->netcardno != -1);
/*
    printf ("netcardno: %d\n", new_tcb->netcardno);
*/

    /* new scheme: demultiplex in user space */
    new_tcb->ipsrc = *((unsigned int *) rcv_ip->source);
    new_tcb->tcpsrc = rcv_tcp->src_port;
    if (web_tcp_addnewconn (webtcp_hashtable, new_tcb) == -1) {
       demand (0, unable to add new connection);
    }

    new_tcb->rcv_irs = rcv_tcp->seqno;
    new_tcb->rcv_next = rcv_tcp->seqno + 1;

    /* Process options */
    hlen = ((rcv_tcp->offset >> 4) & 0xF) << 2;
    if (hlen > sizeof(struct tcp)) { /* options? */
	mss_option = process_options(new_tcb, rcv_data);
    }

    if (mss_option) {
	/* Append options to reply. */
	option[0] = TCP_OPTION_MSS;
	option[1] = TCP_OPTION_MSS_LEN;
	mss = htons(new_tcb->mss);
	memcpy(option + 2, &mss, sizeof(uint16));
    }

    /* Send an syn + ack (the second step in the 3-way handshake). */
    memcpy (&new_tcb->snd_recv_r0_data[WEB_EIT_DATA], option, TCP_OPTION_MSS_LEN);
    new_tcb->snd_recv_r0_sz += TCP_OPTION_MSS_LEN;
    while (tcp_net_send (new_tcb, TCP_FL_SYN|TCP_FL_ACK, new_tcb->snd_iss, 0, 0, -1) != 0) {
       printf ("Must resend response to SYN request\n");
    }
    new_tcb->snd_recv_r0_sz -= TCP_OPTION_MSS_LEN;

    new_tcb->snd_next = new_tcb->snd_iss + 1;
    new_tcb->snd_una = new_tcb->snd_iss;
    new_tcb->state = TCB_ST_SYN_RECEIVED;

    return (1);
}


/* GROK - try to move more of this to inittcb */

void web_tcp_resettcb (struct tcb *tcb)
{
   struct tcp *tcp = (struct tcp *) &tcb->snd_recv_r0_data[WEB_EIT_TCP];

   /* TCP control block initialization */
   tcb->snd_iss = 483;				/* GROK: random number */
   tcb->snd_next = tcb->snd_iss;
   tcb->snd_wla = tcb->snd_iss;
   tcb->snd_up = tcb->snd_iss;
   tcb->retries = 0;
   tcb->timer_retrans = 0;
   tcb->mss = TCP_MSS;		/* set default MSS */
   tcb->flags = 0;

   tcb->nextdata = NULL;  /* is this necessary? */

   assert (tcb->pollbuf == NULL);
   tcb->usr_rcv_buf = NULL;
   tcb->usr_rcv_off = 0;
   tcb->rcv_wnd = tcb->usr_rcv_sz;

   tcp->seqno = 0;
   tcp->ackno = 0;
   tcp->offset = (sizeof(struct tcp) >> 2) << 4;
   tcp->flags = 0;
   tcp->window = 0;
}


void web_tcp_inittcb (struct tcb *tcb, char *buf, int buflen)
{
   struct eth *eth;
   struct ip *ip;
   struct tcp *tcp;

/*
printf ("web_tcp_inittcb: tcb %d, buf %d, buflen %d\n", (uint)tcb, (uint)buf, buflen);
*/
   bzero ((char *)tcb, sizeof(struct tcb));

#ifdef CLIENT_TCP
   tcb->usr_rcv_sz = 8192;
#else
   tcb->usr_rcv_sz = DEFAULT_MSS;
#endif

   assert (buflen >= (WEB_EIT_DATA + ETHER_ALIGN));

   /* Set up pointers to headers in snd_recv; worry about alignment */
   tcb->snd_recv_n = 2;
   tcb->snd_recv_r0_sz = WEB_EIT_DATA;
   tcb->snd_recv_r0_data = &buf[ETHER_ALIGN];
   bzero (&buf[ETHER_ALIGN], WEB_EIT_DATA);

   /* Note that WEB_EIT_DATA bytes were actually set aside! */

   /* Ethernet header initialization. */
   eth = (struct eth *) &tcb->snd_recv_r0_data[WEB_EIT_ETH];
   eth->proto = htons(EP_IP);

   /* IP initialization */
   ip = (struct ip *) &tcb->snd_recv_r0_data[WEB_EIT_IP];
   ip->vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);
   ip->tos = 0;		/* we should allow other values */
   ip->fragoffset = 0;
   ip->identification = 0;
   ip->ttl = 0;		/* set to zero, pseudo hdr value */
   ip->protocol = IP_PROTO_TCP;

   /* TCP initialization */
   tcp = (struct tcp *) &tcb->snd_recv_r0_data[WEB_EIT_TCP];
   tcp->urgentptr = 0;
#ifdef CLIENT_TCP
   tcp->dst_port = web_tcp_mainportnumber;
#else
   tcp->src_port = web_tcp_mainportnumber;
#endif

   web_tcp_resettcb (tcb);
}


void web_tcp_getmainport (int portno, struct tcb * (*gettcb)())
{
   struct dpf_ir ir;
   int ret;

   web_tcp_gettcb = gettcb;

   web_tcp_mainportnumber = htons((uint16)portno);

   dpf_begin (&ir);
#ifdef CLIENT_TCP
	/* 34 is offset of source TCP port! */
   dpf_eq16 (&ir, 34, htons((uint16)portno));
#else
	/* 36 is offset of destination TCP port! */
   dpf_eq16 (&ir, 36, htons((uint16)portno));
#endif

#ifdef RINGBUFFER
   if (((int)(webtcp_demuxid = sys_self_dpf_insert (CAP_ROOT, 0, &ir, 1))) < 0) {
#else
   if (((int)(webtcp_demuxid = sys_self_dpf_insert (CAP_ROOT, 0, &ir, 0))) < 0) {
#endif
      printf ("Unable to listen at main TCP port number\n");
      exit(0);
   }
printf ("DPF FILTER #%d\n", webtcp_demuxid);

/*
   assert (sizeof(webtcpbuf_t) == 1536);
*/
   assert (DEFAULT_MSG == 1514);
   web_tcp_hashinit (webtcp_hashtable);
   web_tcp_hashinit (timewait_hashtable);
   maxpolleecnt = MAXPOLLEECNT;
   web_tcp_initpollbufs (webtcp_demuxid);
   ret = ExosExitHandler (web_tcp_shutdown, NULL);
   assert (ret == 0);
}


int web_tcp_pollforevent (struct tcb *tcb)
{
#if OLD_MULTIPLEX
   return (tcp_net_poll (tcb));
#endif
   tcp_net_poll (tcb);
   return (1);
}


int web_tcp_readable (struct tcb *tcb)
{
   DPRINTF (3, ("web_tcp_readable: tcb %p, state %d\n", tcb, tcb->state));
   tcp_net_poll (tcb);
   /* GROK -- some of the other states should also qualify here!!!! */
   return (tcb->state == TCB_ST_ESTABLISHED);
}


int web_tcp_getdata (struct tcb *tcb, char **bufptr)
{
/*
printf ("web_tcp_getdata: tcb %d, bufptr %x\n", (uint)tcb, (uint)bufptr);
*/
tcp_net_poll (tcb);
   if ((tcb->state != TCB_ST_ESTABLISHED) && (tcb->state != TCB_ST_FIN_WAIT2) && (tcb->state != TCB_ST_CLOSE_WAIT) && (tcb->state != TCB_ST_TIME_WAIT)) {
      return (0);
   }

   *bufptr = tcb->usr_rcv_buf;

   return (tcb->usr_rcv_off);
}


int web_tcp_writable (struct tcb *tcb)
{
   DPRINTF (3, ("web_tcp_writable: tcb %p, state %d\n", tcb, tcb->state));
   tcp_net_poll (tcb);
   /* GROK -- some of the other states should also qualify here!!!! */
   return (tcb->state == TCB_ST_ESTABLISHED);
}


int web_tcp_senddata (struct tcb *tcb, char *buf, int len, char *buf2, int len2, int close, int *more, int *checksums)
{
   int ret;

DPRINTF (2, ("web_tcp_senddata: tcb %x, buf %x, len %d, buf2 %x, len2 %d, close %d\n", (uint)tcb, (uint)buf, len, (uint) buf2, len2, close));

   tcp_net_poll (tcb);
   if (tcb->flags & TCB_FL_RESEND) {
      return (0);
   }

#ifdef TCP_SEP_CLOSE
   if ((ret = tcp_write (tcb, buf, len, buf2, len2, 0, more, checksums)) < 0) {
#else
   if ((ret = tcp_write (tcb, buf, len, buf2, len2, close, more, checksums)) < 0) {
#endif
      printf ("failed to write TCP data: ret %d, len %d\n", ret, len);
      exit (0);
   }

#ifdef TCP_SEP_CLOSE
if (close) {
   tcp_close (tcb);
}
#endif
/*
printf ("web_tcp_senddata: done %d\n", ret);
*/
   return (ret);
}


int web_tcp_initiate_close (struct tcb *tcb)
{
   DPRINTF (2, ("web_tcp_initiate_close: tcb %p, srcport %d\n", tcb, tcb->tcpsrc));

   tcp_net_poll (tcb);
   if (tcb->flags & TCB_FL_RESEND) {
      return (0);
   }

   tcp_close (tcb);

   return (1);
}


int web_tcp_closed (struct tcb *tcb)
{
   if (tcb->state == TCB_ST_TIME_WAIT) {
      tcp_done (tcb);
   }
   tcp_net_poll (tcb);
   return (tcb->state == TCB_ST_CLOSED);
}


void web_tcp_timeadvanced (int seconds)
{
   timewaiter_t *done = timewait_listhead;

   tcptime += seconds;

   while ((done) && (done->timeout <= tcptime)) {
      timewait_listhead = done->next;
      if (timewait_listhead) {
	 timewait_listhead->prev = NULL;
      }
      web_tcp_removeconn (timewait_hashtable, (struct tcb *) done);
      web_tcp_freetimewaiter (done);
      done = timewait_listhead;
   }
}


/* Taken from ae_net.c -- needs to be replaced... */

#include <sys/env.h>
#include <sys/mmu.h>
#include <exos/vm.h>
#include <os/osdecl.h>

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
} asn;		   

/* use this instead of 'asn' in most cases */
#define ASN (*((struct ash_sh_network*)((unsigned int)&asn + \
					(unsigned int)__curenv->env_ashuva)))
 
#define FID_HEAD_QUEUE ASN.queue_table[fid].head_queue
#define FID_TAIL_QUEUE ASN.queue_table[fid].tail_queue
#define FID_QUEUE ASN.queue_table[fid].queue

void web_tcp_shutdown (void *crap)
{
#ifndef RINGBUFFER
   webtcpbuf_t *tmp;
#endif
   int fid = webtcp_demuxid;
   int i;

   /* Make queue empty, so that ash won't insert new pkts for this filter. */
   /* BUG: should be atomic */
   FID_HEAD_QUEUE = FID_TAIL_QUEUE;

   /* Return any packets that are still in the queue. */
   for (i = 0; i < NENTRY; i++) {				 
      if ((FID_QUEUE[i] != 0) && (ADD_UVA(FID_QUEUE[i])->flag < 0)) {
	 ADD_UVA(FID_QUEUE[i])->flag = 1;   /* libos owns it now */
	 FID_QUEUE[i] = 0;
      }
   }

   maxpolleecnt = 0;
#ifndef RINGBUFFER
   while (pollbuflist) {
      tmp = web_tcp_getfrompolllist (&pollbuflist, &pollbuftail);
      web_tcp_resetpollbuf (tmp);
   }
#endif

   sys_self_dpf_delete (0, webtcp_demuxid);
   webtcp_demuxid = -1;

   /* GROK - Should also send resets on any live connections!!! */
}

