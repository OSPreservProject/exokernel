
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

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/mmu.h>

#include <vos/vm.h>
#include <vos/cap.h>
#include <vos/kprintf.h>
#include <vos/assert.h>
#include <vos/proc.h>
#include <vos/fd.h>
#include <vos/fdtypes.h>
#include <vos/errno.h>

#include <vos/net/iptable.h>
#include <vos/net/arptable.h> /* SECONDS2TICKS */
#include "xio_tcpsocket.h"



static u_int xio_sockbuf_va = XIO_SOCKBUF_START;

static char* 
vos_tcpsocket_pagealloc (void *ptr, int len)
{
  dprintf("vos_tcpsocket_pagealloc called: %d\n",len);
  len = PGROUNDUP(len);
  xio_sockbuf_va += len;
  assert(xio_sockbuf_va < XIO_SOCKBUF_END);
  bzero((char*)(xio_sockbuf_va-len), len);
  return (char*) (xio_sockbuf_va - len);
}



/* this is for compat before we can link with libc */
time_t 
time(time_t* t)
{
  if (t != 0L)
  {
    *t = (__sysinfo.si_system_ticks * __sysinfo.si_rate) / 1000000;
    return *t;
  }
  return __sysinfo.si_system_ticks * __sysinfo.si_rate / 1000000;
}


static int 
fd_tcpsocket(S_T(fd_entry)* fd, int domain, int type, int protocol)
{
  xio_tcpsock_info_t *info;
  info = (xio_tcpsock_info_t*) 
    shared_malloc(CAP_USER,sizeof(xio_tcpsock_info_t));
  info->inited = 0;
  xio_tcpsocket_initinfo(info, vos_tcpsocket_pagealloc, 1);

  fd->state = xio_tcpsocket_allocsock(info);
  return 0;
}


static int 
fd_tcpsocket_close_final(S_T(fd_entry) *fd)
{
  int r;
  tcpsocket_t *sock;
  xio_tcpsock_info_t *info;
  int nonblocking;
  
  sock = (tcpsocket_t*)fd->state;
  if (sock == 0L) /* was never initialized */
    return 0;

  nonblocking = FD_ISNB(fd);
  info = sock->info;
  
  spinlock_acquire(&sock->info->lock);
  r = xio_tcpsocket_close(sock, nonblocking);
  spinlock_release(&sock->info->lock);
 
  if (info->active_tcpsockets == 0) 
    shared_free(info, CAP_USER, sizeof(xio_tcpsock_info_t));
  return r;
}


static int 
fd_tcpsocket_close(S_T(fd_entry) *fd)
{
  tcpsocket_t *sock;
  
  sock = (tcpsocket_t*)fd->state;
  if (sock == 0L) /* was never initialized */
    return 0;

  if (sock->demux_id != -1) 
  {
    if (sys_dpf_refcnt(CAP_DPF_REF, sock->demux_id) > 1)
    {
      int ret = xio_net_wrap_freedpf(sock->demux_id);
      assert (ret == 0);
    }
  }
  return 0;
}


static int 
fd_tcpsocket_verify(S_T(fd_entry) *fd)
{
  return 0;
}


static int 
fd_tcpsocket_incref(S_T(fd_entry) *fd, u_int new_envid)
{
  tcpsocket_t *sock = (tcpsocket_t*)fd->state;
  if (sock == 0L) /* was never initialized */
    return 0;

  if (sock->demux_id != -1) 
  {
    int ret = sys_dpf_ref(CAP_DPF_REF, sock->demux_id, CAP_USER, new_envid);
    assert (ret == 0);
  }
  return 0;
}

  
static int
fd_tcpsocket_read(S_T(fd_entry) *fd, void *buffer, int len)
{
  tcpsocket_t *sock;
  int nonblocking;
  int r;

  sock = (tcpsocket_t*)fd->state;
  if (sock == 0L)
    RETERR(V_BADFD);

  // vos_tcpsocket_handlePackets();

  nonblocking = FD_ISNB(fd);
  
  spinlock_acquire(&sock->info->lock);
  r = xio_tcpsocket_read (sock, buffer, len, nonblocking);
  spinlock_release(&sock->info->lock);
  return r;
}


static int 
fd_tcpsocket_write(S_T(fd_entry) *fd, const void *buffer, int len)
{
  tcpsocket_t *sock;
  int nonblocking;
  int r;

  sock = (tcpsocket_t*)fd->state;
  if (sock == 0L)
    RETERR(V_BADFD);

  // vos_tcpsocket_handlePackets();

  nonblocking = FD_ISNB(fd);
  spinlock_acquire(&sock->info->lock);
  r = xio_tcpsocket_write (sock, buffer, len, nonblocking);
  spinlock_release(&sock->info->lock);
  return r;
}


static int 
fd_tcpsocket_bind(S_T(fd_entry) *fd, const struct sockaddr *reg_name, int namelen)
{
  int r;

  tcpsocket_t *sock = (tcpsocket_t*)fd->state;
  if (sock == 0L)
    RETERR(V_BADFD);
  
  spinlock_acquire(&sock->info->lock);
  r = xio_tcpsocket_bind (sock, reg_name, namelen);
  spinlock_release(&sock->info->lock);
  return r;
}


static int 
fd_tcpsocket_listen(S_T(fd_entry) *fd, int backlog)
{
  int r;

  tcpsocket_t *sock = (tcpsocket_t*)fd->state;
  if (sock == 0L)
    RETERR(V_BADFD);
  
  spinlock_acquire(&sock->info->lock);
  r = xio_tcpsocket_listen (sock, backlog);
  spinlock_release(&sock->info->lock);
  return r;
}


static int 
fd_tcpsocket_accept(S_T(fd_entry) *fd, S_T(fd_entry) *newfd, 
                    struct sockaddr *newsockaddr0, int *addr_len)
{
  tcpsocket_t *sock;
  tcpsocket_t *newsock;
  int flags = 0;

  sock = (tcpsocket_t*)fd->state;
  if (sock == 0L)
    RETERR(V_BADFD);

  spinlock_acquire(&sock->info->lock);
  // vos_tcpsocket_handlePackets ();
  newsock = xio_tcpsocket_accept(sock, newsockaddr0, addr_len, 
                                 flags, FD_ISNB(fd));
  spinlock_release(&sock->info->lock);

  if (newsock != NULL)
  {
    newfd->state = newsock;
    return newfd->fd;
  }

  else
    return -1;
}





static int 
fd_tcpsocket_connect(S_T(fd_entry) *fd, 
                     const struct sockaddr *uservaddr, int namelen)
{
  tcpsocket_t *sock;
  const struct sockaddr_in *name;
  int flags = 0;
  int r;

  ipaddr_t myip, theirip;
  ethaddr_t myeth,theireth;
  int ifnum;

  name = (struct sockaddr_in *)uservaddr;

  if (name == 0) 
    RETERR(V_INVALID);
  
  if (namelen != sizeof (struct sockaddr)) 
  { 
    printf("fd_udp_bind: incorrect namelen it is %d should be %d\n",
	   namelen, sizeof(struct sockaddr));
    RETERR(V_INVALID);
  }

  if (ipaddr_get_dsteth((char *)&name->sin_addr,theireth,&ifnum) != 0) 
  {
    printf("ipaddr_get_dsteth: can't get destination ethernet addr\n");
    RETERR(V_HOSTUNREACH);
  }

  sock = (tcpsocket_t*)fd->state;
  if (sock == 0L)
    RETERR(V_BADFD);

  memcpy(theirip, (char *)&name->sin_addr,4);
  if_get_ethernet(ifnum,myeth);
  if_get_ipaddr(ifnum, myip);

  /* key: don't use ip of the interface, 
   * but ip that we specified using bind, if any! */
  if (memcmp((char*)&sock->tcb.ipdst,IPADDR_ANY,4) != 0)
    memcpy(myip, (char *)&sock->tcb.ipdst, 4);

#if 0
  printf("CONNECT\n");
  printf("myip:     "); print_ipaddr(myip); printf("\n");
  printf("myeth:    "); print_ethaddr(myeth); printf("\n");
  printf("theirip:  "); print_ipaddr(theirip); printf("\n");
  printf("theireth: "); print_ethaddr(theireth); printf("\n");
#endif

  spinlock_acquire(&sock->info->lock);
  
  // vos_tcpsocket_handlePackets();
  r = xio_tcpsocket_connect
    (sock, uservaddr, namelen, flags, FD_ISNB(fd), ntohs(sock->tcb.tcpdst), 
     myip, theirip, myeth, theireth);
  
  spinlock_release(&sock->info->lock);
  return r;
}





fd_op_t const tcp_fd_ops =
{
  fd_tcpsocket_verify,		/* verify */
  fd_tcpsocket_incref,		/* inc ref */
  NULL,				/* open */
  fd_tcpsocket_read,		/* read */
  fd_tcpsocket_write,		/* write */
  NULL, 			/* readv */
  NULL, 			/* writev */
  NULL, 			/* lseek */
  NULL, 			/* select */
  NULL, 			/* select_pred */
  NULL, 			/* ioctl */	
  NULL, 			/* fcntl */
  NULL, 			/* flock */
  fd_tcpsocket_close,		/* close */
  fd_tcpsocket_close_final,	/* close_final */
  NULL, 			/* dup */
  NULL, 			/* fstat */
  fd_tcpsocket, 		/* socket */
  fd_tcpsocket_bind, 		/* bind */
  fd_tcpsocket_connect, 	/* connect */
  fd_tcpsocket_accept,		/* accept */
  fd_tcpsocket_listen, 		/* listen */
  NULL, 			/* sendto */
  NULL  			/* recvfrom */
};


int
tcp_socket_init (void)
{
  int ret;

  ret = vm_alloc_region(XIO_SOCKBUF_START, XIO_SOCKBUF_SZ, 
                        CAP_USER, (PG_U|PG_P|PG_W|PG_SHARED));
  
  if (ret < 0)
    RETERR(V_NOMEM);
  
  /*
  kprintf("tcpsocket %d, info %d, nwinfo %d\n", sizeof(struct tcpsocket), sizeof(xio_tcpsock_info_t), sizeof(xio_nwinfo_t));
  */

  /* register fd operations */
  register_fd_ops (FD_TYPE_TCPSOCKET, &tcp_fd_ops);
  return 0;
}


