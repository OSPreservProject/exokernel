
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
#include <xok/sys_proto.h>
#include <xok/defs.h>
#include <xok/types.h>
#include <xok/printf.h>
#include <machine/endian.h>
#include <xok_include/net/ip.h>
#include <xok_include/net/ether.h>
#include <xok_include/assert.h>
#include <xok/pkt.h>

#define KDEBUG_PORT 9999
#define ETHER(pkt) ((struct ether_pkt *) (pkt))
#define IP(pkt)    ((struct ip_pkt *)    (((char*)(pkt)) + 14))
#define UDP(pkt)   ((struct udp_pkt *)   (((char*)(pkt)) + 14 + 20))

extern void edintr(int);
extern int kdebug_gdb_init(void);
extern int in_exception_handler; 

static void enq(char);
static int  deq(char *);
void kdebug_flush_input_buffer(void);

void kdebug_pkt (char *, int);
static int send_pkt(char *, int);
static void set_pkt_data (char);
static short cksum(u_short *, int);

static int kdebug_didinit = 0;
/* 
 * kdebug_return_pkt holds the default ether, including ip/udp, pkt in
 * network byte order, The destination fields (ether/ip/port) are
 * copied from the source fields of the incoming packet.  
 */
static char kdebug_return_pkt[64];
/* 
 * kdebug_return_didinit says where we have all the parameters needed
 * to send packets back to gdb.
 */
static int kdebug_return_didinit = 0;

#define MIN(a, b) ((a<b) ? (a) : (b))
#define DBG(s)
/* #define DBG(s) s; */


/* 
 * This will be a really simple buffer for input characters.  If the
 * buffer is fills we simply discard the buffer.
 *
 * Having a fixed sized buffer also so to avoid using malloc.  This
 * allows pmap.c to be debugged.  oops..not exactly because malloc is
 * still used in if_ed.c */

#define KBUFSIZ 256
static char char_buf[KBUFSIZ];
static int buf_head = 0; /* where to read from */
static int buf_tail = 0; /* where to read to */
/* when buf_head == buf_tail we cant tell whether the buffer is empty or full */
static int buf_filled_p = 0; 

static void enq(char c)
{
  if (buf_head!=buf_tail)    assert(!buf_filled_p);
  if (buf_filled_p)          assert(buf_head==buf_tail);

  char_buf[buf_tail] = c;
  buf_tail = (buf_tail+1) % KBUFSIZ;
  
  if (buf_filled_p)
    buf_filled_p = 0; 
  if (buf_head==buf_tail)
    buf_filled_p = 1;
}

/* 
 * return 1 if deq was successful and store char at c
 * else return 0
 */
static int deq(char *c)
{
  if (buf_head!=buf_tail)    assert(!buf_filled_p);
  if (buf_filled_p)          assert(buf_head==buf_tail);

  if ((buf_head == buf_tail) && !buf_filled_p)
    return 0; 

  *c = char_buf[buf_head];
  buf_head = (buf_head+1) % KBUFSIZ;
  buf_filled_p = 0;
  return 1;
}


void kdebug_flush_input_buffer(void)
{
  buf_head = buf_tail;
  buf_filled_p = 0;
}


void kdebug_init(void)
{
  kdebug_gdb_init();
  kdebug_didinit = 1; 
  DBG(printf("kdebug_init done!..."));
}


/* ******************************************
 * Modifies kdebug_return_pkt so that it
 * has the it can be sentg back. This means
 * exchanging the src/dst ether/ip/udp port/addrs.
 *
 * Also, sets kdebug_return_didinit to 1.
 * (it's assumed that pkt "is_for_us")
 * ***************************************** */
void kdebug_return_init(char *pkt)
{
  struct ether_pkt *eth  = (struct ether_pkt *) pkt;
  struct ip_pkt    *ip   = (struct ip_pkt *)   (pkt + sizeof(*eth));
  struct udp_pkt   *udp  = (struct udp_pkt *)  (pkt + sizeof(*eth) + sizeof(*ip));

  struct ether_pkt *keth = (struct ether_pkt *) kdebug_return_pkt;
  struct ip_pkt    *kip  = (struct ip_pkt *)   (kdebug_return_pkt + sizeof(*keth));
  struct udp_pkt   *kudp = (struct udp_pkt *)  (kdebug_return_pkt + sizeof(*keth) + sizeof(*kip));

  bzero(kdebug_return_pkt, 64);
  bcopy(&eth->ether_shost,  &keth->ether_dhost, 6);
  bcopy(&eth->ether_dhost,  &keth->ether_shost, 6);
  keth->ether_type = htons(0x0800);

  kip->ip_hl   = 0x5;
  kip->ip_v    = 0x4;
  kip->ip_tos  = 0x0;
  kip->ip_len  = htons(0x001d);
  kip->ip_id   = htons(0x0);
  kip->ip_id   = htons(0x0);
  kip->ip_off  = 0x0;
  kip->ip_mf   = 0x0;
  kip->ip_df   = 0x0;
  kip->ip_ttl  = 0x40;
  kip->ip_p    = 0x11;
  kip->ip_sum  = 0x0;
  bcopy(&ip->ip_src,  &kip->ip_dst,  4);
  bcopy(&ip->ip_dst,  &kip->ip_src,  4);

  bcopy(&udp->udp_sport, &kudp->udp_dport, 2);
  bcopy(&udp->udp_dport, &kudp->udp_sport, 2);
  kudp->udp_len = htons(0x9);
  /* the udp cksum is optional so forget it for now */
  kudp->udp_sum = htons(0x0);

  kdebug_return_didinit = 1;
}



int kdebug_is_debug_pkt(const char *data, int size)
{
  /*
   * check that packet is an IP pkt contain an UDP packet 
   * whose destination port is KDEBUG_PORT
   */

  return ((size >= ETHER_MIN_LEN) &&
	  (size <= ETHER_MAX_LEN) &&
	  (ntohs(ETHER(data)->ether_type) == ETHERTYPE_IP) &&
	  (IP(data)->ip_p == IPPROTO_UDP) &&
	  (ntohs(UDP(data)->udp_dport) == KDEBUG_PORT));
}



/* ************************************************
 * this function is the entry point for kernel
 * debuging.  It is called by xokpkt_recv in pkt.h
 * It is assumed that data "kdebug_is_debug_pkt".
 * *************************************************/
void kdebug_pkt (char *data, int size)
{
  int i, min; 
  char c;
  assert (kdebug_didinit);

  if(!kdebug_return_didinit)
    {
      kdebug_return_init(data);
      if (!in_exception_handler)
	{
	  asm("int $3"); 
	  /*
	   * When you connnect to the target from gdb
	   * you end up here.  
	   */
	  asm("nop");  /* Now, set the brkpts you want and continue. */
	}
    }

  /* the udp_len field might lie, so must be careful */     
  min = MIN(ntohs(UDP(data)->udp_len) - 8, size - 14 - 20 - 8);
  for (i=0 ; i < min; i++)
    {
      char c = UDP(data)->udp_data[i];
      if (c == '\003' && !in_exception_handler)
	asm ("int $3");  /* you end up here if you typed Ctrl-C, into gdb */
      else
	enq(c);
    }
}


static int send_pkt(char *pkt, int len)
{
  struct ae_recv recv;
  int ret;

  recv.n = 1;
  recv.r[0].sz = len;
  recv.r[0].data = pkt;

  do {
    ret = sys_net_xmit (0, 1, &recv, NULL, 0);
    if (ret < 0) {
      DBG(printf ("SYS_ED0XMIT FAILED! ret=%d\n", ret));
      edintr(0); 
    }
  } while (ret < 0);


  DBG(printf("SENT>\n");)
  return 1;
}


int getDebugChar(void)
{
  char c;
  unsigned int r = 0;
  DBG(printf("<GET "));

  do {
    if (!(r = (++r > 2000000000) ? 0 : r)) {
      DBG(printf("<GET spinning>"));
      panic("spinning");
    }
    edintr(0);
  } while (!deq(&c));
  DBG(printf("***%c>", c));
  return c;
}


/*
 * returns 0 on failure and 1 on successs
 */
int putDebugChar(char c) 
{
  int ret;

  if (kdebug_return_didinit)
    {
      DBG(printf("<PUT c=%c...", c));
      set_pkt_data(c); 
      ret = send_pkt(kdebug_return_pkt,64);
      DBG(printf("PUT_done!>"));
    }
  else
    {
      DBG(printf("KDEBUG: no return addr! unable to send: %c", c));
      ret = 0;
    }

  return ret;
}


static void set_pkt_data (char c)
{
  IP(kdebug_return_pkt)->ip_sum   = 0x0;
  /* 10 words in ip header */
  IP(kdebug_return_pkt)->ip_sum   = cksum((unsigned short *) IP(kdebug_return_pkt), 10);
  *(UDP(kdebug_return_pkt)->udp_data) = c;
}

/* ************************************
 * ip cksum routine stolen from 
 * Internetworking with TCP/IP Vol. II 
 * ************************************/
static short cksum(u_short *buf, int nwords)
{
  u_long sum;

  for (sum=0; nwords>0; nwords--)
    sum += *buf++;

  sum = (sum>>16) + (sum&0xffff);
  sum += (sum>>16);
  return ~sum;
}

#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

