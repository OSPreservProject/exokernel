
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
#include <netinet/in.h>
#include "udp_socket.h"

#include <xok/wk.h>
#include <xok/sys_ucall.h>

#include <exos/netinet/dpf_lib.h>
#include <dpf/old-dpf.h>
#include <dpf/dpf-internal.h>
#include <dpf/dpf.h>
#include <dpf/action.h>

#include <xok/ae_recv.h>
#include <exos/net/ae_net.h>
#include <exos/process.h>

#include <stdlib.h>		/* for malloc */
#include <xok/env.h>
#include <xok/wk.h>
#include <exos/critical.h>

#include <exos/netinet/in.h>
#include <exos/net/if.h>
#include <exos/mallocs.h>
#include <exos/cap.h>

#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

#define kprintf(format,args...) 

#include <exos/debug.h>
#include <unistd.h>


/* bind_udp_port - tries to actually do the binding of an udp port by
 * inserting a udp filter (at ring_id) returns the filter id. pass in a port
 * in net order and returns the filter id for that udp port, or a negatie
 * number in case of errors 
 */

static int 
bind_udp_port(unsigned short src_port, unsigned char *ip_src, unsigned short dst_port, unsigned char *ip_dst, int *fidset, int ring_id)
{
  struct dpf_ir filter1;
  int fid;
#ifdef UDP_FRAG 
  int fid2;
  struct dpf_ir filter2, filter3;
#endif
  DPRINTF(CLUHELP_LEVEL,("bind_udp_port, port %d (dec)\n",htons(src_port)));

  assert(ring_id != 0);
  if(bcmp(ip_src, IPADDR_ANY, 4) == 0) 
  {
    dpf_begin(&filter1);  /* must be done before use */
    dpf_eq16(&filter1, 12, 0x8);
    dpf_eq8(&filter1, 14, 0x45);
    dpf_meq16(&filter1, 20, 0x2000, 0x00);
    dpf_eq8(&filter1, 23, 0x11);
    dpf_eq16(&filter1, 36, src_port);
    
    fid = sys_self_dpf_insert(CAP_ROOT, UDP_CAP, &filter1, ring_id);
    fidset[0] = fid;

#ifdef UDP_FRAG
    dpf_begin(&filter2);  /* must be done before use */
    dpf_eq16(&filter2, 12, 0x8);
    dpf_eq8(&filter2, 14, 0x45);
    dpf_meq8(&filter2, 20, 0x20, 0x20);
    dpf_eq8(&filter2, 23, 0x11);
    dpf_eq16(&filter2, 36, src_port);
    dpf_actioneq16(&filter2, 38);

    dpf_begin(&filter3);  /* must be done before use */
    dpf_eq16(&filter3, 12, 0x8);
    dpf_eq8(&filter3, 14, 0x45);
    dpf_stateeq16(&filter3, 18, 0);
    dpf_eq8(&filter3, 23, 0x11);

   
    fid2 = sys_self_dpf_insert(CAP_ROOT, UDP_CAP, &filter2, ring_id);
    fidset[1] = fid2;
   
    fid2 = sys_self_dpf_insert(CAP_ROOT, UDP_CAP, &filter3, ring_id);
    fidset[2] = fid2;
#else
    fidset[1] = -1;
    fidset[2] = -1;
#endif
    fidset[3] = -1;
  } 
  
  else 
  {
    dpf_begin(&filter1);
    dpf_eq16(&filter1, 12, 0x8);
    dpf_eq8(&filter1, 14, 0x45);
    dpf_meq16(&filter1, 20, 0x2000, 0x00);
    dpf_eq8(&filter1, 23, 0x11);
    dpf_eq8(&filter1, 30, ip_src[0]);
    dpf_eq8(&filter1, 31, ip_src[1]);
    dpf_eq8(&filter1, 32, ip_src[2]);
    dpf_eq8(&filter1, 33, ip_src[3]);
    dpf_eq16(&filter1, 36, src_port);

#ifdef UDP_FRAG
    dpf_begin(&filter2);
    dpf_eq16(&filter2, 12, 0x8);
    dpf_eq8(&filter2, 14, 0x45);
    dpf_meq16(&filter2, 20, 0x2000, 0x2000);
    dpf_eq8(&filter2, 23, 0x11);
    dpf_eq8(&filter2, 30, ip_src[0]);
    dpf_eq8(&filter2, 31, ip_src[1]);
    dpf_eq8(&filter2, 32, ip_src[2]);
    dpf_eq8(&filter2, 33, ip_src[3]);
    dpf_eq16(&filter2, 36, src_port);
    dpf_actioneq16(&filter2, 38);

    dpf_begin(&filter3);
    dpf_eq16(&filter3, 12, 0x8);
    dpf_eq8(&filter3, 14, 0x45);
    dpf_stateeq16(&filter3, 20, 0);
    dpf_eq8(&filter3, 23, 0x11);
    dpf_eq8(&filter3, 30, ip_src[0]);
    dpf_eq8(&filter3, 31, ip_src[1]);
    dpf_eq8(&filter3, 32, ip_src[2]);
    dpf_eq8(&filter3, 33, ip_src[3]);
#endif

    if (ip_dst != 0L) /* also has addr of sender */
    {
      dpf_eq8(&filter1, 26, ip_dst[0]);
      dpf_eq8(&filter1, 27, ip_dst[1]);
      dpf_eq8(&filter1, 28, ip_dst[2]);
      dpf_eq8(&filter1, 29, ip_dst[3]);
      dpf_eq16(&filter1, 34, dst_port);

#ifdef UDP_FRAG
      dpf_eq8(&filter2, 26, ip_dst[0]);
      dpf_eq8(&filter2, 27, ip_dst[1]);
      dpf_eq8(&filter2, 28, ip_dst[2]);
      dpf_eq8(&filter2, 29, ip_dst[3]);
      dpf_eq16(&filter2, 34, dst_port);
      
      dpf_eq8(&filter3, 26, ip_dst[0]);
      dpf_eq8(&filter3, 27, ip_dst[1]);
      dpf_eq8(&filter3, 28, ip_dst[2]);
      dpf_eq8(&filter3, 29, ip_dst[3]);
#endif
    }

    fid = sys_self_dpf_insert(CAP_ROOT, UDP_CAP, &filter1, ring_id);
    fidset[0] = fid; 

#ifdef UDP_FRAG
    fid2 = sys_self_dpf_insert(CAP_ROOT, UDP_CAP, &filter2, ring_id);
    fidset[1] = fid2;
    
    fid2 = sys_self_dpf_insert(CAP_ROOT, UDP_CAP, &filter3, ring_id);
    fidset[2] = fid2;
#else
    fidset[1] = -1;
    fidset[2] = -1;
#endif
    fidset[3] = -1;
  }

  CHECK(fid);

  DPRINTF(CLUHELP_LEVEL,
	  ("bind_udp_port: tried port: (net)%04x (host)%04x, got fid %d\n",
	   src_port,htons(src_port),fid));
  return fid;
}


/* bind_getport - tries to find an non used port in case of 0, otherwise tests
 * passed in port. port passed in host order, and returned in host order */

unsigned short 
fd_bind_getport(unsigned short port, unsigned char *ip_src, int *fid, int *fidset, int ring_id)
{

  int k;
  int next = 0;
  unsigned short new_port;

  DPRINTF(CLUHELP_LEVEL,("bind_getport: %u\n",port));
  DPRINTF(CLUHELP_LEVEL,("USED_PORTS: %d\n",pr_used_ports()));

  kprintf("fd_bind_getport %d, %p %p %d\n",port,ip_src,fid,ring_id);
  if (port == 0)
    {
	/* for (new_port = 4096+(64*pid); 
	   new_port < 4096+(64*pid) ; new_port += 1)	*/
	
      for (new_port = 2048 + 32*(__envid % 64)+(getpid() % 13); 
	   new_port < 10000 ; new_port += 1)	/* increases by one in net
						   order */
	{
	  DPRINTF(CLUHELP_LEVEL,("testing port (host)%d\n",new_port));
	  
	  for (k = 0; k < NR_SOCKETS; k++) {
	      next = 0;
	      if (udp_shared_data->used_ports[k] == new_port) {
		  next = 1; 
		  break;
	      }
	  }
	  if (next) {continue;}
	  else {
	      *fid = bind_udp_port(htons(new_port), ip_src, 0, 0L, fidset, ring_id);
	      kprintf("bind_upd_port %d, %p %d --> fid %d\n",new_port,ip_src,
		      ring_id,*fid);
	      if (*fid < 0) {
		  DPRINTF(CLUHELP_LEVEL,
			  ("I thought this port %d was usable\n",new_port));
		  kprintf("%s checked port in use: %d\n",__FUNCTION__,new_port);
		  continue;
	      } 

	      for(k = 0; k < NR_SOCKETS ; k++)
		  if (udp_shared_data->used_ports[k] == 0) {
		      udp_shared_data->used_ports[k] = new_port;
		      return (new_port);
		  }
	      DPRINTF(CLUHELP_LEVEL,
		      ("out of ports!!, check your constants\n"));
	      return 0;
	  }
      }
      return 0;
  } else {
      for (k = 0; k < NR_SOCKETS; k++)
	  if (udp_shared_data->used_ports[k] == port) {
	    kprintf("port already in use (via udp_shared_data->used_ports[%d]\n",k);
	    return 0;
	  }
      
      *fid = bind_udp_port(htons(port), ip_src, 0, 0L, fidset, ring_id);
      kprintf("bind_upd_port %d, %p %d --> fid %d\n",port,ip_src,
	      ring_id,*fid);
      if (*fid < 0) return 0;

      for(k = 0; k < NR_SOCKETS ; k++)
	  if (udp_shared_data->used_ports[k] == 0) {
	      udp_shared_data->used_ports[k] = port;
	      return (port);
	  }
      DPRINTF(CLUHELP_LEVEL,
	      ("out of ports!!, check your constants\n"));
      
      return port;
    }
}


/* udp_getringbuf: size is the number of ring buffers, 
   depending on the load of the port.  
   returns a pointer to a ring, or NULL in case of none available
   */
ringbuf_p 
udp_getringbuf(int size, int private) {
  ringbuf_p r,r_tail, r_head;
  int i;
  int c;

  assert(size > 0);
  c = 0;
  r = r_head = r_tail = NULL; 
  EnterCritical();
  while(size-- > 0) {
    r = NULL;

    if (private == UDP_PRIVATE_BUFFERS) {
      r = (ringbuf_p)__malloc(sizeof(ringbuf_t));
    } else {
      /* must be shared */
      for (i = c ; i < NR_RINGBUFS; i++) {
	if (udp_shared_data->ringbufs[i].next == NULL) {
	  r = &udp_shared_data->ringbufs[i];
	  break;
	}
      }
      c = i;
    }
    if (r == NULL) {
      if (r_head != NULL) udp_putringbuf(r_head);
      return NULL;
    }
    r->pollptr = &(r->poll);
    r->poll = 0;
    r->private = private;
    r->n = 1;
    r->sz = RECVFROM_BUFSIZE;
    r->data = &r->space[0];
    if (r_tail == NULL) r_head = r_tail = r; 
    r_tail->next = r;
    r->next = r_head;
    r_head = r;
  }
  ExitCritical();
  return r_head;
}


/* udp_putringbuf:
 frees a ringbuf. return 0 on success and returns -1 on error*/
int
udp_putringbuf(ringbuf_p r) { 
  ringbuf_p t;
  int total;

  if (r == NULL) {return -1;}

  /* for shared memory */
  /* sanity check */
  for(t = r, total = 1; 
      t->next != NULL && total <= NR_RINGBUFS && t->next != r;
      t = t->next) 
    if (r->private) total++;

  if (total > NR_RINGBUFS) assert(0);
  if (t->next == NULL) assert(0);
  /* ok so it must be a ring */
  EnterCritical();
  for(;;) {
    if (r->next == NULL) break;
    t = r->next;

    if (r->private == UDP_PRIVATE_BUFFERS) {
      kprintf("Freeing %p\n",r);
      __free(r);
      r->next = NULL;
    } else {
      r->next = NULL;
    }

    r = t;
  }
  ExitCritical();
  return 0;
}
    
static int 
udp_putport(unsigned short port) {
  int i;
  for (i = 0; i < NR_SOCKETS; i++) {
    if (udp_shared_data->used_ports[i] == htons(port)) {
      udp_shared_data->used_ports[i] = 0;
      /* we could if we wanted to memset the receive buffer to 0,
	 but that is not my responsibility!!*/
      return 0;
    }
  }
  DPRINTF(CLUHELP_LEVEL,
	  ("udp_putport, hmm used a buffer that was not allocated to me\n"));
  return -1;
}

/* exported interfaces */

/* udp_socket - this is the socket creator */
int 
udp_socket(struct file * filp) {
  socket_data_p sock;

  DPRINTF(CLU_LEVEL,("udp_socket\n"));
  demand(filp, bogus filp);
  filp->f_mode = 0;
  filp->f_pos = 0;
  filp->f_flags = 0;
  filp_refcount_init(filp);
  filp_refcount_inc(filp);
  filp->f_owner = __current->pid;
  filp->op_type = UDP_SOCKET_TYPE;

#ifdef INLINE_SOCKETS
  sock = GETSOCKDATA(filp);
#else
  sock = udp_getsocket();
  assert(sock);
  GETSOCKDATA(filp) = sock;
#endif
  sock->tosockaddr.sin_port = 0; /* socket disconnected */
  sock->port = 0;		/* socket unbound */
  sock->demux_id = 0;
  sock->recvfrom.ring_id = -1;
  sock->recvfrom.r = NULL;
  return 0;
}

/* udp_close - deallocates the port from the port table, removes the 
   filter from dpf, and returns the shared buffer */
int 
fd_udp_close(struct file *filp) {
  socket_data_p sock;
  int status;
  DPRINTF(CLU_LEVEL,("fd_udp_close\n"));
  demand(filp, bogus filp);
  
  sock = GETSOCKDATA(filp);
    
  DPRINTF(CLU_LEVEL,("close: demux_id: %d\n",sock->demux_id));
  kprintf("(%d) fd_udp_close demux_id: %d, ring_id %d\n",__envid,
	  sock->demux_id,sock->recvfrom.ring_id);


  demand(sock->demux_id >= 0, closing bogus sock->demux_id);
  /* remember to remove filter id */
  if (sock->demux_id != 0) {
#if 0
    status = sys_self_dpf_delete(UDP_CAP, sock->demux_id);
    CHECK(status);
    kprintf("sys_self_dpf_delete %d %d --> %d\n",
	    UDP_CAP,sock->demux_id,status);
#endif
    int i;
    for(i=0; i<DPF_MAXFILT; i++)
    {
      if (sock->aux_fids[i] == -1)
	break;
      status = sys_self_dpf_delete(UDP_CAP, sock->aux_fids[i]);
      CHECK(status);
      kprintf("sys_self_dpf_delete %d %d --> %d\n",
	      UDP_CAP,sock->aux_fids[i],status);
      sock->aux_fids[i] = -1;
    }

    status = sys_pktring_delring (UDP_CAP, sock->recvfrom.ring_id);
    CHECK(status);
    kprintf("%s sys_pktring_delring %d %d --> %d\n",__FUNCTION__,
	    UDP_CAP,sock->recvfrom.ring_id,status);
    udp_putringbuf(sock->recvfrom.r);
    status = udp_putport(sock->port);
    CHECK(status);
    sock->demux_id = 0;
  }
#ifndef INLINE_SOCKETS
  udp_putsocket(sock);
#endif
  return 0;
}
/* undercover mechanisms to set the number of receive packets of the next bind.
 * it gets reset after each bind 
 */
static unsigned int __secret_udp_nr_bufs = 0;
static unsigned int __secret_udp_private_buf = 0;
void 
udp_use_private_buffers(void) {
  //kprintf("UDP_USE_PRIVATE_BUFFERS\n");
  __secret_udp_private_buf = 1;
}

int 
udp_set_nr_buffers(int nr) {
  //kprintf("UDP_SET_NR_BUFFERS %d\n",nr);
  __secret_udp_nr_bufs = nr;
  return nr;
}

/* checks that either the address is 0.0.0.0 (any) or that of one of our interfaces */
static int 
check_local_address(char *ip_src) {
  int ifnum;
  if (bcmp(ip_src,IPADDR_ANY,4) != 0) {
    /* make sure is the address of one of our interfaces */
    init_iter_ifnum();
    while((ifnum = iter_ifnum(IF_UP)) != -1) {
      ipaddr_t ifip;
      get_ifnum_ipaddr(ifnum,ifip);
      if (bcmp(ip_src,ifip,4) == 0) return 0;
    }
    return -1;
  }
  return 0;
}


/* binds a filp to a socket address */
int 
fd_udp_bind(struct file *filp, struct sockaddr *reg_name, int namelen)
{
  unsigned short src_port;
  socket_data_p sock;
  struct sockaddr_in *name;
  int nrudpbufs = DEFAULT_NRUDPBUFS;
  int status;
  ringbuf_p r;
  int ring_id = 0;
  char *ip_src;
  name = (struct sockaddr_in *)reg_name;
  kprintf("AT FD_UDP_BIND: %d\n",__secret_udp_private_buf);
  DPRINTF(CLU_LEVEL,("fd_udp_bind\n"));
  demand(filp, bogus filp);

  if (name == 0) { errno = EFAULT; return -1;}
  if (namelen != sizeof (struct sockaddr)) { 
      DPRINTF(CLU_LEVEL,("fd_udp_bind: incorrect namelen it is %d should be %d\n",
			 namelen, sizeof(struct sockaddr)));
      errno = EINVAL; 
      return -1;}

  sock = GETSOCKDATA(filp);

  /* in case we are rebinding */
  if (sock->demux_id != 0) {
    kprintf("Rebinding\n");
      errno = EINVAL;
      return -1;
  }

  ip_src = (char *)&name->sin_addr;
  /* VERIFY THAT IS LOCAL ADDRESS like 127.0.0.1, 0.0.0.0 or any or our local interfaces */
  if (check_local_address(ip_src) != 0) {
    errno = EADDRNOTAVAIL;
    return -1;
  }

  if (__secret_udp_nr_bufs) {
    nrudpbufs = __secret_udp_nr_bufs;
    __secret_udp_nr_bufs = 0;
  }
  if (__secret_udp_private_buf > 0) {
    r = udp_getringbuf(nrudpbufs,UDP_PRIVATE_BUFFERS);
    __secret_udp_private_buf = 0; /* reset it */
  } else {
    r = udp_getringbuf(nrudpbufs,UDP_SHARED_BUFFERS);
  }

  if (r == NULL) {
    kprintf("Out of buffers\n");
    errno = ENOBUFS;
    return -1;
  }
  
  if ((ring_id = sys_pktring_setring (UDP_CAP, 0, (struct pktringent *) r)) <= 0) {
      kprintf ("setring failed: no soup for you!\n");
      assert (0);
  }
  kprintf("got ring_id : %d (ring size: %d)\n",ring_id,nrudpbufs);

  src_port = fd_bind_getport(ntohs(name->sin_port),
			     ip_src,&sock->demux_id, &sock->aux_fids[0], ring_id);
  DPRINTF(CLU_LEVEL,("bind: demux_id: %d ring_id %d\n",sock->demux_id,ring_id));
  
  if (src_port == 0 || sock->demux_id < 0) {
    kprintf("src_port %d, sock->demux_id: %d\n",src_port,sock->demux_id);
    sock->demux_id = 0;
    status = sys_pktring_delring(UDP_CAP, ring_id);
    CHECK(status);
    kprintf("%s sys_pktring_delring %d %d --> %d\n",__FUNCTION__,
	    UDP_CAP, ring_id, status);
    errno = EADDRINUSE;
    return -1;
  }

  sock->recvfrom.r = r;
  sock->recvfrom.ring_id = ring_id;
  sock->port = htons(src_port);	/* stored in net order */
  name->sin_port = htons(src_port);
  memcpy(&sock->fromsockaddr,name,sizeof(struct sockaddr_in));

  return 0;
}


/* udp_fcntl - allows you to change the socket to non blocking.  need
   to allow functionality for duping, since you can dup using fcntl!*/
int 
fd_udp_fcntl (struct file * filp, int request, int arg)
{
  socket_data_p sock;

  DPRINTF(CLU_LEVEL,("fd_udp_fcntl\n"));
  demand(filp, bogus filp);

  sock = GETSOCKDATA(filp);

  if (request == F_SETFL && arg == O_NDELAY)
  {
      DPRINTF(CLU_LEVEL,("setting socket nonblocking\n"));
      sock->recvfrom.sockflag = 4; 
      filp->f_flags = (filp->f_flags | O_NDELAY);
  } else {
      fprintf(stderr,"fcntl: unimplemented request (%d) and argument (%d)"
	     "from socket.\n",request,arg);
      demand(0, unimplemented fcntl);
  }
  return 0;
}

/* udp_connect - allows you to use send and recv instead of sendto and
   recvfrom */
int 
fd_udp_connect(struct file *filp, struct sockaddr *servaddr0,
	       int sockaddr_len, int flags) {
    void *filter;
    int i;
    int fid;
    int sz;
    int status;
    struct sockaddr_in *servaddr = (struct sockaddr_in *)servaddr0;
    socket_data_p sock;
    
    filter = filter; sz = sz; status = status; fid = fid;

    DPRINTF(CLU_LEVEL,("fd_udp_connect\n"));
    demand(filp, bogus filp);
    
  sock = GETSOCKDATA(filp);

  sock->tosockaddr = *((struct sockaddr_in *)servaddr);
  if (sizeof sock->tosockaddr != sockaddr_len) {
    fprintf(stderr,"different sockadd lengths: %d != %d\n",
	    sizeof sock->tosockaddr,sockaddr_len);
    errno = EINVAL;
    return -1;
  }

  /* 
   * unbound socket: we bind to INADDR_ANY for now and create a packet ring.
   * In a few moment we will change the filter, but keep the same ring.
   */
  if (sock->port == 0) 
  {
    struct sockaddr_in name;
    memset((char *)&name,0,sizeof(name));
    name.sin_family = AF_INET;
      
    if (fd_udp_bind(filp,(struct sockaddr*)&name,sizeof(name)) == -1)
      return -1;
  }

  assert(sock->demux_id);	/* must be true 'cause we pre-bound */
  assert(sock->recvfrom.ring_id);/* must be true 'cause we pre-bound */

  for(i=0; i<DPF_MAXFILT; i++)
  {
    if (sock->aux_fids[i] == -1) 
      break;
    status = sys_self_dpf_delete(UDP_CAP, sock->aux_fids[i]);
    CHECK(status);
    kprintf("sys_self_dpf_delete %d %d --> %d\n",
	    UDP_CAP,sock->aux_fids[i],status);
    sock->aux_fids[i] = -1;
  }

  fid = bind_udp_port(sock->port, (char*)&sock->fromsockaddr.sin_addr, 
                      servaddr->sin_port, (char*)&servaddr->sin_addr,
		      sock->aux_fids, sock->recvfrom.ring_id);

  kprintf("got new fid %d\n",fid);

  if (fid < 0) 
  {
    errno = EADDRINUSE;
    return -1;
  }

  sock->demux_id = fid;
  return 0;
}


/* this functions allows you to poll for a filp to see if you can read 
   or write without blocking. */
int 
fd_udp_select (struct file * filp,int flag)
{
  socket_data_p sock;
  ringbuf_p r;
  /* DPRINTF(CLU_LEVEL,("fd_udp_select\n"));*/
  demand(filp, bogus filp);
  sock = GETSOCKDATA(filp);
  if (sock->demux_id == 0)  fprintf(stderr,"warning: unbound socket\n");
  r = sock->recvfrom.r;
//  kprintf("%d.%d.%d",__envid,sock->demux_id,sock->recvfrom.ring_id);
//  kprintf("%d %p %d %p %d",__envid,r,r->poll,r->next,r->next->poll);
  switch(flag) {
  case SELECT_READ:
    return (r->poll > 0) ? 1 : 0;
  case SELECT_WRITE:
    return 1;
  case SELECT_EXCEPT:			/* exception case */
    return 0;
  default:
    assert(0);			/* nobody should be selecting this */
    return 0;
  }
}

int
fd_udp_select_pred (struct file *filp, int rw, struct wk_term *t) {
  socket_data_p sock;
  ringbuf_p r;
  int next = 0;

  demand(filp, bogus filp);
  sock = GETSOCKDATA(filp);
  demand (sock->demux_id != 0, unbound socket);
  r = sock->recvfrom.r;
  switch (rw) {
  case SELECT_WRITE:
    demand (0, fd is always ready);
  case SELECT_READ:
    next = wk_mkvar (next, t, wk_phys (&(r->poll)), 0);
    next = wk_mkimm (next, t, 0);
    next = wk_mkop (next, t, WK_NEQ);
    return (next);
  default:
    demand (0, invalid operation);
  }
}
int
fd_udp_getname(struct file * filp, 
	       struct sockaddr *bound, int *boundlen, int peer) {
  socket_data_p sock;
  struct sockaddr_in *name;

  DPRINTF(CLU_LEVEL,("fd_udp_getname\n"));
  demand(filp, bogus filp);

  if (bound == 0) { errno = EFAULT; return -1;}
  name = (struct sockaddr_in *)bound;
  sock = GETSOCKDATA(filp);

  if (peer == 0) {
    /* getsockname */
    name->sin_port = sock->fromsockaddr.sin_port;/* stored in net order */
    memcpy((char *)&name->sin_addr,(char *)&sock->fromsockaddr.sin_addr,4);
  } else {
    /* getpeername */
    if (sock->tosockaddr.sin_port == 0) {
	errno = ENOTCONN;
	return -1;
    }
    name->sin_port = sock->tosockaddr.sin_port;	/* stored in net order */
    memcpy((char *)&name->sin_addr,(char *)&sock->tosockaddr.sin_addr,4);
  }
  *boundlen = sizeof (struct sockaddr_in);
  return 0;
}

int 
fd_udp_setsockopt(struct file *sock, int level, int optname,
		      const void *optval, int optlen) {
  if (level == SOL_SOCKET) {
    switch(optname) {
    case  SO_BROADCAST:
      /* we allow broadcast if the proper address is used */
      /* if they set broadcast increment size of next ring to be bound */
      if (*((int *)optval) == 1)
	udp_set_nr_buffers(NRUDPBUFS);
      return 0;
    default:
    }
  }
  errno = EINVAL;
  return -1;
}
