
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

#include <xok/network.h>
#include <xok/ae_recv.h>
#include <xok/sys_ucall.h>

#include <vos/kprintf.h>
#include <vos/assert.h>
#include <vos/errno.h>
#include <vos/fd.h>

#include <vos/net/iptable.h>
#include <vos/net/ae_ether.h>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "udp_fd_ops.h"
#include "udp_struct.h"


#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

#define dprintf if (0) kprintf
#define dprint_ipaddr if (0) print_ipaddr
#define dprint_ethaddr if (0) print_ethaddr


static int identification = 1;

static inline int 
my_ip_eq_addr(char *a1, char *a2) 
{
  return ((a1[0] == a2[0]) && (a1[1] == a2[1]) && 
          (a1[2] == a2[2]) && (a1[3] == a2[3]));
}


/* checksum replicated here for code locality */
static uint16 
inet_cksum(uint16 *addr, uint16 count) 
{
  register long sum = 0;
    
  /*  this is the inner loop */
  while( count > 1 )  {
    sum += * (unsigned short *) addr++;
    count -= 2;
  }
    
  /*  add left-over byte, if any */
  if(count > 0)
    sum += * (unsigned char *) addr;
  
  /*  fold 32-bit sum to 16 bits */
  while (sum>>16)
    sum = (sum & 0xffff) + (sum >> 16);

  return ~sum;
}


static int 
get_ae_recv_total(struct ae_recv *send_recv) 
{
  int i,total = 0;
  for (i = 0; i < send_recv->n; i++) 
  {
    total += send_recv->r[i].sz;
  }
  return total;
}


static int 
ip_split_recv(struct ae_recv *orig,
	      struct ae_recv *remainder,
	      int *copied) 
{
  int total,i,j,saved_i,seen = 0;
  remainder->n = 0;

  if ((total = get_ae_recv_total(orig)) <= IP_FRAG_SIZE) 
  {
    *copied = total;
    return 1;
  } 
  
  else 
  {
    for (i = 0; i < orig->n ; i++) 
    {
      seen += orig->r[i].sz;
      if (seen >= IP_FRAG_SIZE) 
	break;
    }
    
    *copied = IP_FRAG_SIZE;
    orig->r[i].sz = orig->r[i].sz - (seen - IP_FRAG_SIZE);
    saved_i = i;
 
    if (seen != IP_FRAG_SIZE) 
    {
      remainder->r[0].data = (char *)(orig->r[i].data + orig->r[i].sz);
      remainder->r[0].sz = (seen - IP_FRAG_SIZE);
      j = 1;
    } 
    
    else 
    {
      j = 0;
    }

    i++;

    for (; i < orig->n ; i++,j++) 
    {
      remainder->r[j] = orig->r[i];
    }
    
    remainder->n = j;
    orig->n = saved_i + 1;
  }
  return 0;
}


static inline void
ae_eth_send_wrap (struct ae_recv *send_recv, int cardno)
{
  while((ae_eth_sendv(send_recv, cardno)) < 0);
}


static void 
ip_send_fragment(struct ae_recv *send_recv,
		 int offset,int last,
		 struct eth *eth, 
		 struct ip *ipptr,
		 int total_length,
		 int cardno) 
{
  struct ae_recv real_send;
  int i;

  if (offset % 8 != 0) { printf("WARNING NOT MULTIPLE OF 8\n"); }

  if (last) 
    ipptr->fragoffset = htons((offset)/8);
  else 
    ipptr->fragoffset = htons((offset)/8 | IP_FRG_MF);

  dprintf("ip_send_fragment: OFFSET: %d\n",offset);
  dprintf("ip_send_fragment: fragoffset: %08x\n",htons(ipptr->fragoffset));
    
  ipptr->totallength = htons(get_ae_recv_total(send_recv) + 20); 
  ipptr->checksum = 0;
  ipptr->checksum = inet_cksum((uint16 *)ipptr, sizeof (struct ip)); 
    
  real_send.n = send_recv->n + 2;
  real_send.r[0].data = (char *) eth;
  real_send.r[0].sz   = 14;
  real_send.r[1].data = (char *) ipptr;
  real_send.r[1].sz   = 20;

  for (i = 0; i < send_recv->n ; i++) 
    real_send.r[i+2] = send_recv->r[i];
 
  ae_eth_send_wrap(&real_send,cardno);
}


static int 
send_ip (struct sockaddr_in *from, struct sockaddr_in *to, 
         struct ae_recv *send_recv, int total_length) 
{
  struct eth eth;
  struct ip ipstruc;
  
  static struct netcache {
    int cardno;
    ipaddr_t  ipdst;
    ethaddr_t ethdst;
    ipaddr_t  ipsrc;
    ethaddr_t ethsrc;
  } netcache = {0,{0,0,0,0},{0,0,0,0,0,0},{0,0,0,0},{0,0,0,0,0,0}};

  int broadcasting_flag = 0;
  u_int ifiter;
  int ifnum;
  extern char eth_bcast_addr[6];
  struct ae_recv send_recv_new;
  int copied = 0;
  int tmp_offset;
  int cardno = 1;
  int i;

  dprintf("send_ip eth: 0x%08x, ip: 0x%08x\n",(int)&eth,(int)&ipstruc);
  dprintf("send_recv has %d entries\n",send_recv->n);
  dprintf("checking if addr is broadcast addr...\n");

  /* broadcast! */
broadcasting:
  if (memcmp(&to->sin_addr,IPADDR_BROADCAST,4) == 0) 
  { 
    if (broadcasting_flag == 0)
    {
      ifiter = 0;
      broadcasting_flag = 1;
    }
   
    /* look for an iterface that do broadcast and up */
    ifnum = iptable_find_if(IF_UP|IF_BROADCAST, &ifiter);

    if (ifnum < 0) 
      return 0;
    
    /* get ethernet src and set dest address */
    if_get_ethernet(ifnum,eth.src_addr);
    memcpy(&eth.dst_addr[0], eth_bcast_addr, 6);
  
    /* get ip src address and set dest address */
    if_get_ipaddr(ifnum, (char *)&ipstruc.source[0]);
    memcpy(&ipstruc.destination[0],
	   (char *)&to->sin_addr, sizeof(ipstruc.destination));
    cardno = if_get_cardno(ifnum);
  } 
  
  else /* not broadcast */
  {
    dprintf("no, not broadcast!\n");

    if (memcmp(&to->sin_addr, netcache.ipdst, 4) != 0) 
      /* new ip destination */
    {
      dprintf("can't find it in cache, search\n");
	
      dprintf("ip: %d.%d.%d.%d\n", 
	  ((char*)&to->sin_addr)[0],
	  ((char*)&to->sin_addr)[1],
	  ((char*)&to->sin_addr)[2],
	  ((char*)&to->sin_addr)[3]);

      if (ipaddr_get_dsteth((char *)&to->sin_addr,netcache.ethdst, &ifnum)==0) 
      { 
        /* now, cache the entry */
	memcpy(&netcache.ipdst[0],(char *)&to->sin_addr,4);
	if_get_ethernet(ifnum, netcache.ethsrc);
	if_get_ipaddr(ifnum, netcache.ipsrc);
	netcache.cardno = if_get_cardno(ifnum);

	dprintf("card: %d; if: %d; eth: %x.%x.%x.%x.%x.%x\n", 
	    netcache.cardno,
	    ifnum,
	    netcache.ethdst[0],
	    netcache.ethdst[1],
	    netcache.ethdst[2],
	    netcache.ethdst[3],
	    netcache.ethdst[4],
	    netcache.ethdst[5]);
	dprint_ipaddr(netcache.ipdst); 
	dprintf("\n");
      } 
      
      else 
      {
	printf("Host Unreachable\n");
	RETERR(V_HOSTUNREACH);	  /* propagate error  */
      }
    }

    memcpy(&eth.dst_addr[0], netcache.ethdst, 6);
    memcpy(&eth.src_addr[0], netcache.ethsrc, 6); 
    memcpy(&ipstruc.source[0], netcache.ipsrc, sizeof(ipstruc.source));
    memcpy(&ipstruc.destination[0], 
	   (char *)&to->sin_addr, sizeof(ipstruc.destination));
    cardno = netcache.cardno;

    dprintf("sending to: ");
    dprint_ipaddr(((unsigned char*)ipstruc.destination)); 
    dprintf("\n");
    dprint_ethaddr(eth.dst_addr); 
    dprintf("\n");
  }
  

  /* IP HEADER */
  eth.proto = htons(EP_IP);

  /* key: we allow user specified IP address, so if they specified one,
   * we override the one gotten from the net interface table! */
  if (from != 0)
    memcpy(&ipstruc.source[0], (char*)&from->sin_addr, sizeof(ipstruc.source));
  ipstruc.vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);
  ipstruc.tos = 0;
  ipstruc.identification = identification++;
  ipstruc.ttl = 0xFF;
  ipstruc.protocol = IP_PROTO_UDP;

  /* need to make sure this value is 0 when computing checksum */

  tmp_offset = 0;
 
  do {
    i = ip_split_recv(send_recv,&send_recv_new,&copied);
    ip_send_fragment(send_recv,tmp_offset,i,&eth,&ipstruc,total_length,cardno);
    tmp_offset += copied;
    *send_recv = send_recv_new;
  } 
  while (i== 0);

  if (broadcasting_flag) 
    goto broadcasting;
  return 0;
}



int 
fd_udp_sendtov(S_T(fd_entry) *fd, struct ae_recv *msg_recv, 
	      unsigned flags, const struct sockaddr *to, int tolen) 
{
  socket_data_t *sock;
  struct udp udp;
  struct ae_recv send_recv;
  int i,len;
  int status;

  if (fd == 0)
    RETERR(V_BADFD);

  if (fd->type != FD_TYPE_UDPSOCKET)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  if ((len = get_ae_recv_total(msg_recv)) > MAXSEND) 
  {
    printf("Warning trying to send upd packet larger than %d: %d\n",
	   MAXSEND,len);
    RETERR(V_MSGSIZE);
  }

  sock = (socket_data_t*)fd->state;

  if (to == NULL) 
  {		/* using send, connected operation */
    if (sock->tosockaddr.sin_port == 0) 
    {
      errno = V_NOTCONN;
      return -1;
    } 
    
    else 
    {
      tolen = sizeof sock->tosockaddr;
      to = (struct sockaddr *)&sock->tosockaddr;
    }
  }

  dprintf("sock demux_id %d\n",sock->demux_id);

  if (sock->demux_id == 0) 
  {
    struct sockaddr_in name;
    int r;

    memset(&name,0,sizeof(name));
    if((r = fd_udp_bind(fd,(struct sockaddr *)&name,sizeof(name))) < 0)
      return r;
  }

  assert((msg_recv->n) + 1 < AE_RECV_MAXSCATTER);
  send_recv.n = msg_recv->n + 1;
  send_recv.r[0].data = (char *)&udp;
  send_recv.r[0].sz = 8;
  for (i = 0 ; i < msg_recv->n ; i++)
    send_recv.r[i+1] = msg_recv->r[i];

  udp.length = htons(len + 8); 
  udp.src_port = sock->port;
  udp.dst_port = ((struct sockaddr_in *)to)->sin_port;
  udp.cksum = 0;

  status = 
    send_ip(&(sock->fromsockaddr),(struct sockaddr_in *)to,&send_recv,len+8);
  return (status == 0) ? len : status;
}


int 
fd_udp_sendto(S_T(fd_entry) *fd, const void *msg, size_t len, 
	      unsigned flags, const struct sockaddr *to, int tolen) 
{

  /* same as ae_recv, but with const data field to avoid compile warning */
  struct my_ae_send 
  {
    u_int n;
    struct _send_rec 
    {
      u_int sz;
      const unsigned char *data;
    } r[AE_RECV_MAXSCATTER]; 
  };
  
  struct my_ae_send msg_send = {
    .n = 1, 
    {[0] = {len,msg}} 
  };

  return fd_udp_sendtov(fd,(struct ae_recv*)&msg_send,flags,to,tolen);
}


int 
fd_udp_write(S_T(fd_entry) *fd, const void *msg, int len)
{
  return fd_udp_sendto(fd,msg,len,0,(struct sockaddr *)0,0);
}

