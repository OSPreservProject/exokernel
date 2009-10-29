
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

#include <xok/ae_recv.h>
#include <xok/env.h>

#include <vos/net/ae_ether.h>
#include <vos/fd.h>
#include <vos/wk.h>
#include <vos/errno.h>
#include <vos/cap.h>

#include "udp_struct.h"

#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

#define dprintf if (0) kprintf


int 
fd_udp_recvfrom(S_T(fd_entry) *fd, void *buffer, size_t length, 
	        unsigned flags, struct sockaddr *reg_rfrom, int *rfromlen) 
{
  struct eiu *eiu;
  ringbuf_t *r;
  socket_data_t *sock;
  struct sockaddr_in *rfrom;
  int return_length;
    
  rfrom = (struct sockaddr_in *)reg_rfrom;

  if (fd->type != FD_TYPE_UDPSOCKET)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  sock = (socket_data_t*)fd->state;

  if (sock->port == 0)
    RETERR(V_NOTCONN);

  /* remove from ring */
  r = sock->recvfrom.r;

  eiu = (struct eiu *)r->data;
  if (!eiu) 
    assert(0);

  if (r->poll == 0 && FD_ISNB(fd)) 
    RETERR(V_WOULDBLOCK);

  {
    int tmp = r->poll;
    r->poll = 0;
    r->poll = tmp;
  }
  dprintf("%d: waiting on ring buf 0x%x, %d\n",
      getpid(),(u_int)r,va2ppn((u_int)r));
  dprintf("waiting for dpf\n");
  wk_waitfor_value_neq ((unsigned int *)&r->poll, 0, CAP_PKTRING);
  dprintf("came back from waiting for dpf\n");

  /*  pull the message out */
  memcpy((char *)buffer, &eiu->data[0], 
         MIN(length, htons(eiu->udp.length) - sizeof(struct udp)));
  return_length = MIN(length, htons(eiu->udp.length) - sizeof(struct udp));

  if (rfrom != (struct sockaddr_in *)0)
  {
    rfrom->sin_family = 2;
    *rfromlen = sizeof(struct sockaddr);
    memcpy((char *)&rfrom->sin_addr,
           (char *)&eiu->ip.source[0],
           sizeof eiu->ip.destination);
    rfrom->sin_port = eiu->udp.src_port;
  }
  
  if (!(flags & MSG_PEEK))  /* we don't add new structure */
  {
    r->poll = 0;
    sock->recvfrom.r = r->next;
  }
  return return_length;
}


  
int
fd_udp_read(S_T(fd_entry) *fd, char *buffer, int length)
{
  socket_data_t *sock;
    
  if (fd->type != FD_TYPE_UDPSOCKET)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  if (length > 1472) 
  {
    printf("warning trying to read upd packet larger than 1472: %d,\n"
	   "reassembly not supported yet\n",length);
    RETERR(V_MSGSIZE);
  }
  
  sock = (socket_data_t*)fd->state;

  if (sock->tosockaddr.sin_port == 0) 
    RETERR(V_NOTCONN);

  return 
    fd_udp_recvfrom(fd,buffer,length,0,0,0);
}

