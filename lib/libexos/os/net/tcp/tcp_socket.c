
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

#include <malloc.h>
#include <assert.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>

#include <string.h>

#include "tcp_socket.h"

#include <errno.h>

#include "netinet/in.h"

#ifdef AEGIS
#include "tcp/libtcp.h"
#endif /* AEGIS */
#ifdef EXOPC
#include "libtcp.h"

extern const char *get_ether_from_netcardno (int netcardno);

#endif /* EXOPC */

#include "../eth_if.h"

#include <exos/netinet/hosttable.h>
#include <xok/sysinfo.h>
#include "exos/net/ether.h"

#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

extern int 
tcp_non_blocking_read(struct tcb *tcb, void *addr, uint16 sz);

/* this file exports the following functions:
   tcpsocket_init
   tcpsocket
   tcpsocket_close
   tcpsocket_bind
   tcpsocket_fcntl
   tcpsocket_ioctl
   tcpsocket_select

   we need 
   tcpsocket_connect
   tcpsocket_read
   tcpsocket_write

 */


/* Helpers */

/* Takes a pointer to ether-ip-udp structure to be used as template
   for outgoing packets.  eth_src, ip_src and src_port(net order),
   just fills in the eiu template. */
#if 0
struct tcb *			
tcb_acquire(void)
{
    struct tcb *tcb;

    for (tcb = tcp_shared_data->connection; 
	 tcb < tcp_shared_data->connection + MAX_CONNECTION; tcb++) {
	if (tcb->state == TCB_ST_FREE)
	    return(tcb);
    }
    demand(0, to many open connections);
    return (struct tcb *)0;
}


#endif

void 
eth_praddr(eaddr)
        char   *eaddr;
{

        printf("%x:%x:%x:%x:%x:%x",
            eaddr[0] & 0xFF,
            eaddr[1] & 0xFF,
            eaddr[2] & 0xFF,
            eaddr[3] & 0xFF,
            eaddr[4] & 0xFF,
            eaddr[5] & 0xFF
            );
}

#if 0
static int
pr_used_ports(void) {
    int k,count = 0;
    printf("pr_used_ports:\n");
    return 99;			/* for now */
    for (k = 0; k < NR_SOCKETS; k++) {

	if (tcp_shared_data->used_ports[k] != 0) count++;

	printf("[%02d]: %05d ",
	       k,tcp_shared_data->used_ports[k]);
	if (k % 6 == 5) printf("\n");
    }
    return count;
}
#endif
/* TCP socket creator */
int 
tcp_socket(struct file * filp) {
    struct tcp_socket_data *sock;

    DPRINTF(CLU_LEVEL,("tcp_socket\n"));
    demand(filp, bogus filp);
    filp->f_mode = 0;
    filp->f_pos = 0;
    filp->f_flags = 0;
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = __current->pid;
    filp->op_type = TCP_SOCKET_TYPE;
    sock = (struct tcp_socket_data *) &filp->data;

    sock->tcb = 0;
    sock->port = 0;

    return 0;
}

/* udp_close - deallocates the port from the port table, removes the 
   filter from dpf, and returns the shared buffer */
int 
tcpsocket_close(struct file *filp) {
    struct tcp_socket_data *sock;
    int error;

    DPRINTF(CLU_LEVEL,("tcp_close\n"));
    demand(filp, bogus filp);

    sock = (struct tcp_socket_data *)&filp->data;

    DPRINTF(CLU_LEVEL,("close: demux_id: %d\n",sock->tcb->demux_id));

    /* remember to remove filter id */
    error =  tcp_close(sock->tcb);
    /*if (error) {ae_putc('-');return -1;}*/
    return error;
}

     
/* binds a filp to a socket address */
int 
tcpsocket_bind(struct file *filp, struct sockaddr *reg_name, int namelen)
{
  unsigned short src_port;
  struct tcp_socket_data *sock;
  struct sockaddr_in *name;
    
  name = (struct sockaddr_in *)reg_name;

  DPRINTF(CLU_LEVEL,("tcp_bind\n"));
  demand(filp, bogus filp);

  if (name == 0) { errno = EFAULT; return -1;}
  if (namelen != sizeof (struct sockaddr)) { 
      DPRINTF(CLU_LEVEL,("tcp_bind: incorrect namelen it is %d should be %d\n",
			 namelen, sizeof(struct sockaddr)));
      errno = EINVAL; 
      return -1;}

  sock = (struct tcp_socket_data *) &filp->data;

  demand(sizeof(filp->data) >= sizeof (struct tcp_socket_data),
	 tcp_socket_data greater than filp->data);
  /*   src_port = 
       bind_getport(ntohs(name->sin_port),ip_my_addr,&sock->demux_id); */

  if (name->sin_port == 0) {
      src_port = htons(tcp_shared_data->next_port++);
  } else {
      src_port = name->sin_port;
  }
  if (src_port == 0) {
      errno = EADDRINUSE;
      return -1;
  }
  
  sock->port = src_port;	/* stored in net order */

#if 0		/* GROK - this is wrong. the copy goes in the other way, if
		   at all */
  memcpy((char *)&name->sin_addr,ip_my_addr,4);
#endif

  name->sin_port = src_port;

  return 0;
}

/* this functions allows you to poll for a filp to see if you can read 
   or write without blocking. */
int 
tcpsocket_select (struct file * filp, int rw)
{
  struct tcp_socket_data *sock;
  DPRINTF(CLU_LEVEL,("tcp_select\n"));
  demand(filp, bogus filp);
  switch (rw) {
  case SELECT_READ:
    sock = (struct tcp_socket_data *) &filp->data;
    DPRINTF(CLU_LEVEL,("sock->tcb.demux_id = %d\n", sock->tcb->demux_id));
    return   (tcp_peek(sock->tcb) > 0 || sock->tcb->state == TCB_ST_CLOSE_WAIT)
      ? 1 : 0;
  case SELECT_WRITE:
    return 1;
  default:
    assert(0);
    return 0;
  }
}


int tcpsocket_select_pred (struct file *filp, int rw, struct wk_term *t) {
   struct tcp_socket_data *sock;
   int next = 0;

   DPRINTF(CLU_LEVEL,("tcp_select\n"));
   demand(filp, bogus filp);

   sock = (struct tcp_socket_data *) &filp->data;

	/* GROK -- when TCBs become shared, this will need to also check */
	/* that the current incoming buffer queue has not changed!!!!    */
   next = wk_mkvar (next, t, wk_phys (&(sock->tcb->rcv_queue.head->flag)), 0);
   next = wk_mkimm (next, t, 0xFFFFFFFF);
   next = wk_mkop (next, t, WK_NEQ);
   if (sock->tcb->timer_retrans > 0) {
      next = wk_mkop (next, t, WK_OR);
      next = wk_mkvar (next, t, wk_phys (&__sysinfo.si_system_ticks), 0);
      next = wk_mkimm (next, t, sock->tcb->timer_retrans);
      next = wk_mkop (next, t, WK_GTE);
   }

   return (next);
}


int 
tcpsocket_ioctl(struct file *filp, unsigned int request, char *argp) {
  struct tcp_socket_data *sock;
  sock = (struct tcp_socket_data *) &filp->data;

  switch(request) {
  case FIONREAD:
    *(int *)argp = tcp_peek(sock->tcb);
    return (0);
  default:
    printf("tcp ioctl filp: %p, request: %d, argp: %p\n",
	   filp,request,argp);
  }
  return -1;

}

inline int 
tcpsocket_recv(struct file *filp, void *buffer, int length, int nonblock,
	       unsigned flags)
{
  struct tcp_socket_data *sock;
  int n;

  demand(filp, bogus filp);

  sock = (struct tcp_socket_data *) &filp->data;

  if (buffer == NULL) { 
      errno = EFAULT;
      return -1;
  }
  /* printf("using handle to: %08x\n",(int)sock->tcb); */

  if (CHECKNB(filp)) { /* blocking for now */
    n = tcp_non_blocking_read(sock->tcb,buffer,length);
    if (n < 0) errno = EWOULDBLOCK;
    return n;
  } else {
#if 1
      n = tcp_nread(sock->tcb);
      if (n <= 0) return n;
      else return tcp_read(sock->tcb,buffer,MIN(n, length));
#else 
      int status;
      while((status = tcp_non_blocking_read(sock->tcb,buffer,length)) <= 0); 
      return status;
#endif
  }
}

int 
tcpsocket_read(struct file *filp, char *buffer, int length, int nonblocking) {
  return tcpsocket_recv(filp,buffer,length,nonblocking,0);
}

int
tcpsocket_write(struct file *filp, char *buffer, int length, int nonblocking) {
  struct tcp_socket_data *sock;
  DPRINTF(CLU_LEVEL,("tcp_write\n"));
  demand(filp, bogus filp);

  sock = (struct tcp_socket_data *) &filp->data;

  if (buffer == NULL) { 
      errno = EFAULT;
      return -1;
  }
  return tcp_write(sock->tcb,buffer,length);
}


int 
tcpsocket_send(struct file *filp, void *buffer, int length, int nonblock, 
	       unsigned flags)
{
  struct tcp_socket_data *sock;
  printf("tcp_send %d %d\n", flags & MSG_OOB, flags & MSG_PEEK);
  demand(filp, bogus filp);

  sock = (struct tcp_socket_data *) &filp->data;

  if (buffer == NULL) { 
      errno = EFAULT;
      return -1;
  }
  return tcp_write(sock->tcb,buffer,length);
}


int 
tcpsocket_connect(struct file *filp, struct sockaddr *uservaddr,
		  int namelen, int flags) {
  unsigned short src_port;
  unsigned long  ip_src_addr, ip_dst_addr;
  unsigned char *ether_dst_addr;
  struct tcp_socket_data *sock;
  struct sockaddr_in *name;
  void *handle;
  map_t *hostent;
  map_t *xmithostent;

  name = (struct sockaddr_in *)uservaddr;

  DPRINTF(CLU_LEVEL,("tcpsocket_connect\n"));
  demand(filp, bogus filp);

  if (name == 0) { errno = EFAULT; return -1;}
  if (namelen != sizeof (struct sockaddr)) { 
      DPRINTF(CLU_LEVEL,("tcp_bind: incorrect namelen it is %d should be %d\n",
			 namelen, sizeof(struct sockaddr)));
      errno = EINVAL; 
      return -1;}

  sock = (struct tcp_socket_data *) &filp->data;

  src_port = sock->port;

  memcpy(&ip_dst_addr,(char *)&name->sin_addr,4);
  hostent = get_hostent_from_ip ((char *)&name->sin_addr);
  if (hostent == NULL) {
      errno = ENETUNREACH;
      return (-1);
  }
  ether_dst_addr = hostent->eth_addr;

  xmithostent = get_hostent_from_ether (get_ether_from_netcardno (hostent->xmitcardno));
  if (xmithostent == NULL) {
      errno = ENETUNREACH;
      return (-1);
  }
  memcpy(&ip_src_addr,xmithostent->ip_addr,4);
  
#if 0
  printf("tcp_connect\n");
  printf("dst port: %d\n",htons(name->sin_port));
  printf("src port: %d\n",htons(src_port));
  printf("ip dst: %08x ip src: %08x\n",(int)ip_dst_addr,(int)ip_src_addr);
  printf("etherdst: ");
  eth_praddr(ether_dst_addr);
  printf(" ethersrc: ");
  eth_praddr(my_eth_addr);
  printf("done\n");
#endif

  handle = tcp_connect(htons(name->sin_port),ip_dst_addr,ether_dst_addr,
		       htons(src_port), ip_src_addr, xmithostent->eth_addr);	/* missing src_port */

  if (handle != (void *)0) {
#if 0
      printf("setting handle to: %08x\n",(int)handle);
#endif
      sock->tcb = handle;
      filp->f_flags |= O_RDWR;
      return (0);
  } else {
      errno = EADDRINUSE;	/* just to say something */
      return (-1);
  }
}

/* udp_fcntl - allows you to change the socket to non blocking.  need
   to allow functionality for duping, since you can dup using fcntl!*/
int 
tcpsocket_fcntl (struct file * filp, int request, int arg)
{
  DPRINTF(CLU_LEVEL,("tcpsocket_fcntl\n"));
  demand(filp, bogus filp);

  if (request == F_SETFL && arg == O_NDELAY)
  {
      DPRINTF(CLU_LEVEL,("setting socket nonblocking\n"));
      filp->f_flags = (filp->f_flags | O_NDELAY);
  } else {
      printf("fcntl: unimplemented request (%d) and argument (%d)"
	     "from socket.\n",request,arg);
      demand(0, unimplemented fcntl);
  }
  return 0;
}



int
tcpsocket_getname(struct file * filp, 
	       struct sockaddr *bound, int *boundlen, int peer) {
  struct tcp_socket_data *sock;
  struct sockaddr_in *name;

  DPRINTF(CLU_LEVEL,("tcpsocket_getname\n"));
  demand(filp, bogus filp);

  if (bound == 0) { errno = EFAULT; return -1;}
  name = (struct sockaddr_in *)bound;
  sock = (struct tcp_socket_data *) &filp->data;

  name->sin_family = AF_INET;
  if (peer == 0) {
    /* getsockname */
    name->sin_port = sock->port;	/* stored in net order */
	/* GROK - this is wrong.  Need to have an "inaddr" field in the */
	/* tcp_sock_struct, since one cannot assume that only the first */
	/* slow ethernet card is used for TCP sockets... */
    {extern char ip_my_addr[4];
    memcpy((char *)&name->sin_addr,ip_my_addr,4);
    }
  } else {
    /* getpeername */
    /* Frans put code here to copy into name->sin_port and 
     name->sin_addr, the port and address of our peer */
    tcp_getpeername(sock->tcb, &name->sin_port, 
		    (unsigned char *) &name->sin_addr);
  }
  *boundlen = sizeof (struct sockaddr_in);
  
  return 0;
}

int 
tcpsocket_setsockopt(struct file *filp, int level, int optname,
		     const void *optval, int optlen) {
  struct tcp_socket_data *sock;

  DPRINTF(CLU_LEVEL,("tcpsocket_setsockopt\n"));
  demand(filp, bogus filp);
  sock = (struct tcp_socket_data *) &filp->data;

  return tcp_setsockopt(sock->tcb, level, optname, optval, optlen);
}


int 
tcpsocket_getsockopt(struct file *filp, int level, int optname,
		     void *optval, int *optlen) {
  struct tcp_socket_data *sock;

  DPRINTF(CLU_LEVEL,("tcpsocket_getsockopt\n"));
  demand(filp, bogus filp);
  sock = (struct tcp_socket_data *) &filp->data;

  return tcp_getsockopt(sock->tcb, level, optname, optval, optlen);
}

int 
tcpsocket_shutdown(struct file *filp, int flags) {
  struct tcp_socket_data *sock;

  DPRINTF(CLU_LEVEL,("tcpsocket_shutdown\n"));
  demand(filp, bogus filp);
  sock = (struct tcp_socket_data *) &filp->data;
  return tcp_close(sock->tcb);
}


