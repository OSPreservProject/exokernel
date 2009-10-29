
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

#include <xok/wk.h>
#include <xok/sys_ucall.h>
#include <xok/ae_recv.h>
#include <xok/env.h>
#include <xok/wk.h>

#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>

#include <vos/net/iptable.h>
#include <vos/dpf/dpf.h>
#include <vos/proc.h>
#include <vos/cap.h>
#include <vos/fd.h>
#include <vos/assert.h>
#include <vos/vm.h>
#include <vos/sbuf.h>

#include "../ports/ports.h"
#include "udp_struct.h"

#define dprintf if (0) kprintf


static int
bind_getport(unsigned short port, unsigned char *ip_src, 
             int *fid, int ring_id)
{
  dprintf("bind: want port %d, ring %d\n", port, ring_id);
  *fid = ports_bind_udp(&port, ip_src, ring_id);
  dprintf("dpf_bind_udpfilter %d, %p %d --> fid %d\n",port,ip_src,ring_id,*fid);
  
  if (*fid < 0) 
    RETERR(V_ADDRNOTAVAIL);

  return port;
}



/* checks that either the address is 0.0.0.0 (any) or 
 * that of one of our interfaces 
 */
static int
check_local_address(char *ip_src) 
{
  int ifnum;
  if (memcmp(ip_src,IPADDR_ANY,4) != 0) 
  {
    int ifnumiter = 0;
    
    while((ifnum = iptable_find_if(IF_UP, &ifnumiter)) != -1) 
    {
      ipaddr_t ifip;
      if_get_ipaddr(ifnum,ifip);
      
      if (memcmp(ip_src,ifip,4) == 0) 
	return 0;
    }
    return -1;
  }
  return 0;
}


/* 
 * udp_freeringbuf: frees a ringbuf. return 0 on success, returns -1 on error
 */
int
udp_freeringbuf(ringbuf_t *r) 
{ 
  ringbuf_t *t;
  int total;

  if (r == NULL) 
    return -1;

  for(t = r, total = 1; 
      t->next != NULL && total <= NR_RINGBUFS && t->next != r;
      t = t->next)
    total++;

  if (total > NR_RINGBUFS) 
    assert(0);
  
  if (t->next == NULL) 
    assert(0);

  while(total > 0) 
  {
    if (r->next == NULL) 
      break;
    t = r->next;

    shared_free(r, CAP_USER, sizeof(ringbuf_t));

    r = t;
    total--;
  }
  return 0;
}



/* 
 * udp_getringbuf: size is the number of ring buffers, depending on the load
 * of the port. returns a pointer to a ring, or NULL in case of none
 * available.
 */
ringbuf_t* 
udp_getringbuf(int size)
{
  ringbuf_t *r, *r_tail, *r_head;
  int c;

  assert(size > 0);
  c = 0;
  r = r_head = r_tail = NULL; 
  
  while(size-- > 0) 
  {
    r = NULL;

    /* malloc shared memory so we can share ring bufs */
    r = (ringbuf_t*) shared_malloc(CAP_USER, sizeof(ringbuf_t)); 
    
    if (r == NULL) 
    {
      if (r_head != NULL) 
	udp_freeringbuf(r_head);
      return NULL;
    }

    r->pollptr = &(r->poll);
    r->poll = 0;
    r->n = 1;
    r->sz = RECVFROM_BUFSIZE;
    r->data = &r->space[0];
    
    if (r_tail == NULL) 
      r_head = r_tail = r; 

    r_tail->next = r;
    r->next = r_head;
    r_head = r;
  }

  return r_head;
}


int 
fd_udp_socket(S_T(fd_entry)* fd, int domain, int type, int protocol) 
{
  socket_data_t *sock;

  sock = (socket_data_t*) malloc(sizeof(socket_data_t));
  fd->state = (void*) sock;
  
  sock->tosockaddr.sin_port = 0; /* socket disconnected */
  sock->port = 0; /* socket unbound */
  sock->demux_id = -1;
  sock->recvfrom.ring_id = -1;
  sock->recvfrom.r = NULL;

  return fd->fd;
}


int 
fd_udp_bind(S_T(fd_entry)* fd, const struct sockaddr *reg_name, int namelen)
{
  unsigned short src_port;
  int ret;
  socket_data_t *sock;
  const struct sockaddr_in *name;
  int nrudpbufs = DEFAULT_NR_UDPBUFS;
  int status;
  ringbuf_t *r;
  int ring_id = 0;
  char *ip_src;
  
  name = (struct sockaddr_in *)reg_name;

  if (name == 0) 
    RETERR(V_INVALID);
  
  if (namelen != sizeof (struct sockaddr)) 
  { 
    printf("fd_udp_bind: incorrect namelen it is %d should be %d\n",
	   namelen, sizeof(struct sockaddr));
    RETERR(V_INVALID);
  }

  sock = (socket_data_t*)fd->state;

  /* in case we are rebinding */
  if (sock->demux_id != -1) 
  {
    printf("rebinding\n");
    RETERR(V_INVALID);
  }

  ip_src = (char *)&name->sin_addr;

  /* VERIFY THAT IS LOCAL ADDRESS like 127.0.0.1, 
   * 0.0.0.0 or any or our local interfaces 
   */
  if (check_local_address(ip_src) != 0) 
  {
    printf("address not available\n");
    RETERR(V_ADDRNOTAVAIL);
  }

  r = udp_getringbuf(nrudpbufs);
  dprintf("%d: ring buf created at 0x%x, %d\n",
      getpid(),(u_int)r,va2ppn((u_int)r));

  if (r == NULL) 
  {
    printf("out of ring buffers\n");
    RETERR(V_NOMEM);
  }
  
  if ((ring_id = sys_pktring_setring(CAP_PKTRING, 0, (struct pktringent *)r))<= 0) 
  {
    printf ("setring failed: no soup for you!\n");
    assert (0);
  }
  dprintf("got ring_id : %d (ring size: %d)\n",ring_id,nrudpbufs);

  ret = bind_getport(ntohs(name->sin_port), ip_src, &sock->demux_id, ring_id);
  src_port = ret;
  
  if (ret < 0 || sock->demux_id < 0) 
  {
    dprintf("src_port %d, sock->demux_id: %d\n",src_port,sock->demux_id);
    sock->demux_id = -1;
    status = sys_pktring_delring(CAP_PKTRING, ring_id);
    
    if(status < 0)
      printf("sys_pktring_delring returned negative value!!!\n");

    dprintf("%s sys_pktring_delring %d %d --> %d\n",__FUNCTION__,
	   CAP_PKTRING, ring_id, status);
    
    RETERR(V_ADDRINUSE);
  }

  dprintf("bound to port %d\n", src_port);

  sock->recvfrom.r = r;
  sock->recvfrom.ring_id = ring_id;
  sock->port = htons(src_port);	/* stored in net order */
  memcpy(&sock->fromsockaddr,name,sizeof(struct sockaddr_in));

  return 0;
}


/* 
 * udp_close - deallocates the port from the port table, removes the 
 * filter from dpf, and returns the shared buffer 
 */
int 
fd_udp_close_final(S_T(fd_entry)* fd)
{
  socket_data_t *sock;
  int status;
  u_short port;

  dprintf("udp_close_final called by %d\n",getpid());

  sock = (socket_data_t*)fd->state;

  /* remove from table */
  port = ntohs(sock->port);
  ports_unbind(port);

  /* remember to remove ring */
  if (sock->demux_id >= 0) 
  {
    status = sys_pktring_delring (CAP_PKTRING, sock->recvfrom.ring_id);
    if (status < 0)
      printf("WARNING: sys_pktring_delring returned negative number\n");

    dprintf("sys_pktring_delring %d %d --> %d\n",
	    CAP_PKTRING,sock->recvfrom.ring_id, status);

    udp_freeringbuf(sock->recvfrom.r);
    sock->demux_id = -1;
  }

  free(sock);
  return 0;
}


int 
fd_udp_close(S_T(fd_entry) *fd)
{
  socket_data_t *sock;
  int status;

  sock = (socket_data_t*)fd->state;

  if (sock->demux_id >= 0) 
  {
    status = sys_self_dpf_delete(CAP_DPF_REF, sock->demux_id);
    if (status < 0)
      printf("WARNING: sys_dpf_ref in fd_udp_incref returned negative number\n");
  }

  return 0;
}


int 
fd_udp_verify(S_T(fd_entry) *fd)
{
  return 0;
}


int 
fd_udp_incref(S_T(fd_entry) *fd, u_int new_envid)
{
  socket_data_t *sock;
  int status;

  sock = (socket_data_t*)fd->state;

  if (sock->demux_id >= 0) 
  {
    status = sys_dpf_ref(CAP_DPF_REF, sock->demux_id, CAP_USER, new_envid);
    if (status < 0)
      printf("WARNING: sys_dpf_ref in fd_udp_incref returned negative number\n");
  }

  return 0;
}


