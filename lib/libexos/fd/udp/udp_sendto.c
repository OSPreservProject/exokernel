
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

//#undef PRINTF_LEVEL
//#define PRINTF_LEVEL 99

#include <xok/ae_recv.h>
#include <exos/net/ae_net.h>

#include <string.h>
#include "assert.h"
#include "errno.h"

#include "netinet/in.h"
#include "udp_socket.h"

#include <exos/netinet/in.h>
#include <exos/net/if.h>

#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

#if 0
#define DEBUG printf(":%d:",__LINE__)
#else
#define DEBUG
#endif

#include <exos/debug.h>

#define kprintf(format,args...) 

#define MAXSEND 8872		/* 8872 = 1472 + 5*1840 */

static int identification = 1;

#if 0   /* AE_RECV procedures */
static void pr_ae_recv(struct ae_recv *send_recv) {
    int i,j = 0;
    printf("ae_recv: %08x, ae_recv->n: %d\n",(int)send_recv,send_recv->n);
    for (i=0; i < send_recv->n; i++) {
	printf("[%02d] data: %08x, sz: %2d\t",
	       i, (int)send_recv->r[i].data, send_recv->r[i].sz);
	j += send_recv->r[i].sz;
	demand (j <= (8192+300), too much data to send);
    }
    printf("total size: %d\n",j);
}
#endif

inline static void
ae_eth_send_wrap (struct ae_recv *send_recv, int cardno);

static int 
get_ae_recv_total(struct ae_recv *send_recv);

static int 
ip_split_recv(struct ae_recv *orig,
	      struct ae_recv *remainder,
	      int *copied);

static void
ip_send_fragment(struct ae_recv *send_recv,
		 int offset,int last,
		 struct eth *eth, struct ip *ipptr, int total_length,int cardno);

static int 
send_ip(struct sockaddr_in *to, 
	 struct ae_recv *send_recv, int total_length);


static inline int 
my_ip_eq_addr(char *a1, char *a2) {
    return ((a1[0] == a2[0]) && (a1[1] == a2[1]) && 
	    (a1[2] == a2[2]) && (a1[3] == a2[3]));
}

static int 
send_ip (struct sockaddr_in *to, 
	 struct ae_recv *send_recv,int total_length) {
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
    int ifnum;
    extern char eth_bcast_addr[6];
    struct ae_recv send_recv_new;
    int copied = 0;
    int tmp_offset;
    int cardno = 1;
    int i;
#if 0
    printf("send_ip eth: %08x, ip: %08x\n",(int)&eth,(int)&ipstruc);
    pr_ae_recv(send_recv);
#endif
    /* ETHER HEADER */
    /*     printf("looking in cache..."); */
	 broadcasting:
    if (bcmp(&to->sin_addr,IPADDR_BROADCAST,4) == 0) {
      if (broadcasting_flag == 0)
	init_iter_ifnum();
      else 
	broadcasting_flag = 1;
      ifnum = iter_ifnum(IF_UP|IF_BROADCAST);
      if (ifnum < 0) return 0;
      
      get_ifnum_ethernet(ifnum,eth.src_addr );
      memcpy(&eth.dst_addr[0], eth_bcast_addr, 6);
      get_ifnum_ipaddr(ifnum, (char *)&ipstruc.source[0]);
      memcpy(&ipstruc.destination[0],(char *)&to->sin_addr,sizeof ipstruc.destination);
      cardno = get_ifnum_cardno(ifnum);
    } else {
      if (bcmp(&to->sin_addr, netcache.ipdst, 4) != 0) {
	if (get_dsteth((char *)&to->sin_addr,netcache.ethdst, &ifnum) == 0) {
	  bcopy((char *)&to->sin_addr,netcache.ipdst,4);
	  get_ifnum_ethernet(ifnum, netcache.ethsrc);
	  get_ifnum_ipaddr(ifnum, netcache.ipsrc);
	  netcache.cardno = get_ifnum_cardno(ifnum);
	} else {
	  kprintf("Host Unreachable\n");
	  errno = EHOSTUNREACH;	  /* propagate error  */
	  return -1;
	}
      }
      memcpy(&eth.dst_addr[0], &netcache.ethdst, 6);
      memcpy(&eth.src_addr[0],&netcache.ethsrc, 6); 
      memcpy(&ipstruc.source[0],&netcache.ipsrc,sizeof ipstruc.source);
      memcpy(&ipstruc.destination[0],(char *)&to->sin_addr,sizeof ipstruc.destination);
      cardno = netcache.cardno;
    }
    kprintf("(S%d)",__envid);

    /* IP HEADER */
    eth.proto = htons(EP_IP);

    ipstruc.vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);
    ipstruc.tos = 0;
    ipstruc.identification = identification++;
    ipstruc.ttl = 0xFF;
    ipstruc.protocol = IP_PROTO_UDP;
    /* need to make sure this value is 0 when computing checksum */

    tmp_offset = 0;
    do {
#if 0
	printf("\n* before split:\n");
	pr_ae_recv(send_recv);
#endif

	i = ip_split_recv(send_recv,&send_recv_new,&copied);
#if 0
	printf("* split: %d\n",i);
	printf("copied: %d\n",copied);
	printf("**** orig:\n");
	pr_ae_recv(send_recv);
#endif
	DEBUG;
	ip_send_fragment(send_recv,tmp_offset,i,&eth,&ipstruc,total_length,cardno);
	DEBUG;
#if 0
	printf("offset: %08x (%d)\n",tmp_offset,tmp_offset);
	printf("* remainder:\n");
	pr_ae_recv(&send_recv_new);
#endif
	tmp_offset += copied;
	*send_recv = send_recv_new;
    } while(i == 0);

    if (broadcasting_flag) goto broadcasting;
    return 0;
}

/* checksum replicated here for code locality */
static uint16 
inet_cksum(uint16 *addr, uint16 count) {
    register long sum = 0;
    
    /*  This is the inner loop */
    while( count > 1 )  {
	sum += * (unsigned short *) addr++;
	count -= 2;
    }
    
    /*  Add left-over byte, if any */
    if(count > 0)
    sum += * (unsigned char *) addr;
    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16)
    sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}


static inline int 
sendto_udp(char *addr, int sz, int flags,
	   struct sockaddr_in *to, int tolen, unsigned short src_port) {
    struct udp udp;
    struct ae_recv send_recv;
    int status;
    send_recv.n = 2;
    send_recv.r[0].data = (char *)&udp;
    send_recv.r[0].sz = 8;
    send_recv.r[1].data = addr;
    send_recv.r[1].sz = sz;

    udp.length = htons(sz + 8); 
    udp.src_port = src_port;
    udp.dst_port = to->sin_port;
    udp.cksum = 0;

#if 0
    pr_ae_recv(&send_recv);
    printf("done sendto_udp\n");
#endif
    status = send_ip(to,&send_recv,sz + 8);
    return (status == 0) ? sz : status;
    
}



int 
fd_udp_sendtov(struct file *filp, struct ae_recv *msg_recv, int nonblocking, 
	      unsigned flags, struct sockaddr *to, int tolen) {
  socket_data_p sock;
  struct udp udp;
  struct ae_recv send_recv;
  int i,len;
  int status;
  //    DPRINTF(CLU_LEVEL,("fd_udp_sendto\n"));
  demand(filp, bogus filp);
    
  if ((len = get_ae_recv_total(msg_recv)) > MAXSEND) {
    kprintf("Warning trying to send upd packet larger than %d: %d\n",
	    MAXSEND,len);
    errno = EMSGSIZE;
    return -1;
  }

  sock = GETSOCKDATA(filp);

  if (to == NULL) {		/* using send, connected operation */
    if (sock->tosockaddr.sin_port == 0) {
      errno = ENOTCONN;
      return -1;
    } else {
      tolen = sizeof sock->tosockaddr;
      to = (struct sockaddr *)&sock->tosockaddr;
    }
  }

  if (sock->demux_id == 0) {
    struct sockaddr name;
    memset(&name,0,sizeof name);
    demand(!fd_udp_bind(filp,&name,sizeof name),runtime bind failed);
    //printf("runtime bind: port: %d, demux: %d\n",sock->port,sock->demux_id);
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

  status = send_ip((struct sockaddr_in *)to,&send_recv,len + 8);
  return (status == 0) ? len : status;
}

int 
fd_udp_sendto(struct file *filp, void *msg, int len, int nonblocking, 
	      unsigned flags, struct sockaddr *to, int tolen) {
  struct ae_recv msg_recv = { 
    .n = 1, 
    {[0] = {len,msg}} };
  return fd_udp_sendtov(filp,&msg_recv, nonblocking,flags,to,tolen);
}
int 
fd_udp_write(struct file *filp, char *msg, int len, int nonblocking) {
  return fd_udp_sendto(filp,msg,len,nonblocking, 0, (struct sockaddr *)0,0);
}

inline static void
ae_eth_send_wrap (struct ae_recv *send_recv, int cardno)
{
#if 1
  while((ae_eth_sendv(send_recv, cardno)) < 0);
#else
   int i,j;
   char send_buf[1514];
   extern int retran; 
   j=0;
   /*    printf("ae_eth_send_wrap\n"); */
   for (i=0; i < send_recv->n; i++) {
       /*        printf("[%02d] data: %08x, sz: %d\n", */
       /*        i, (int)send_recv->r[i].data, send_recv->r[i].sz); */
       memcpy((char *)&send_buf[j],send_recv->r[i].data,
	      send_recv->r[i].sz);
       j += send_recv->r[i].sz;
	demand (j <= 1514, too much data for ethernet);
   }
#if 0
/* print out packets as they are sent */
do { int i,k,*x = (void *)send_buf; unsigned char *y = (char *)x;
    k = (j > 50) ? 50 : j;
    printf("\nlength: %d\n",j);
    for(i = 0 ; i < 14; i++) printf("%02x ",y[i]);	y += 14;
    for(i = 0; i <= (k - 14)/4; i++) {
					      if(i % 4 == 0) printf("\n");
					      printf("[%02d] ", i);  printf("%02x%02x %02x%02x  ",y[0],y[1],y[2],y[3]); y += 4;
					  } printf("\n"); } while(0);
#endif
	DEBUG;
	if (j < 64) {
	  while((i = ae_eth_send(send_buf, 64, cardno)) < 0);
	} else {
	  while((i = ae_eth_send(send_buf, j, cardno)) < 0)
	    retran++;  /* and retransmit, if necessary */
	}
	retran++;
	DEBUG;
#endif
}

static int 
get_ae_recv_total(struct ae_recv *send_recv) {
    int i,total = 0;
    for (i = 0; i < send_recv->n; i++) {
	total += send_recv->r[i].sz;
    }
    return total;
}

#define IP_FRAG_SIZE 1480
static int 
ip_split_recv(struct ae_recv *orig,
	      struct ae_recv *remainder,
	      int *copied) {
    int total,i,j,saved_i,seen = 0;
    remainder->n = 0;

    if ((total = get_ae_recv_total(orig)) <= IP_FRAG_SIZE) {
	*copied = total;
	return 1;
    } else {
	for (i = 0; i < orig->n ; i++) {
	    seen += orig->r[i].sz;
	    if (seen >= IP_FRAG_SIZE) break;
	}
	*copied = IP_FRAG_SIZE;
	orig->r[i].sz = orig->r[i].sz - (seen - IP_FRAG_SIZE);
	saved_i = i;
	if (seen != IP_FRAG_SIZE) {
	    remainder->r[0].data = (char *)(orig->r[i].data + orig->r[i].sz);
	    remainder->r[0].sz = (seen - IP_FRAG_SIZE);
	    j = 1;
	} else {
	    j = 0;
	}
	i++;
	for (   ; i < orig->n ; i++,j++) {
	    remainder->r[j] = orig->r[i];
	}
	remainder->n = j;
	orig->n = saved_i + 1;


    }
    return 0;
}

static void 
ip_send_fragment(struct ae_recv *send_recv,
		 int offset,int last,
		 struct eth *eth, struct ip *ipptr,int total_length,
		 int cardno) {
    struct ae_recv real_send;
    int i;

    if (offset % 8 != 0) {printf("WARNING NOT MULTIPLE OF 8\n");}

    if (last) {
	ipptr->fragoffset = htons((offset)/8);
    } else {
	ipptr->fragoffset = htons((offset)/8 | IP_FRG_MF);
    }

#if 0
    printf("OFFSET: %d\n",offset);
    printf("fragoffset: %08x\n",htons(ipptr->fragoffset));
#endif
    
    ipptr->totallength = htons(get_ae_recv_total(send_recv) + 20); 
    ipptr->checksum = 0;
    ipptr->checksum = inet_cksum((uint16 *)ipptr, sizeof (struct ip)); 
    
    real_send.n = send_recv->n + 2;
    real_send.r[0].data = (char *) eth;
    real_send.r[0].sz   = 14;
    real_send.r[1].data = (char *) ipptr;
    real_send.r[1].sz   = 20;

    for (i = 0; i < send_recv->n ; i++) {
	real_send.r[i+2] = send_recv->r[i];
    }
    ae_eth_send_wrap(&real_send,cardno);
    DEBUG;
}

