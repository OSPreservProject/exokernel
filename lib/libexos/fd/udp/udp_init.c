
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

#include <exos/debug.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "udp_socket.h"
#include "udp_file_ops.h"
#include "errno.h"

#include <malloc.h>
#include "exos/vm-layout.h"

#include <exos/process.h>
#include <exos/critical.h>

#include <fd/proc.h>
#include <xok/sys_ucall.h>

#define kprintf(format,args...) 

int 
udp_pass_filp_ref(struct file *filp, u_int k, int envid, int ExecOnlyOrNewpid) {
  socket_data_p sock;
  int status;

  sock = GETSOCKDATA(filp);
  /* only pass a reference if it shared (non private) */
  if (sock->demux_id > 0 && sock->recvfrom.r && sock->recvfrom.r->private == UDP_SHARED_BUFFERS) {
    status = sys_dpf_ref(UDP_CAP,sock->demux_id, k,envid);
    CHECK(status);
    kprintf("(%d) SYS_DPF_REF %d %d %d %d --> %d (ring_id %d)\n",
	    __envid,
	    UDP_CAP,sock->demux_id, k,envid,status,sock->recvfrom.ring_id);
    
  }
  return 0;
}

static int
udp_pass_all_ref(u_int k, int envid, int ExecOnlyOrNewpid) {
  struct file *filp;
  int i;
  for (i = 0; i < NR_OPEN; i++)
    if (__current->fd[i]) {
      filp = __current->fd[i];
      if (filp->op_type == UDP_SOCKET_TYPE) 
	udp_pass_filp_ref(filp,k,envid,ExecOnlyOrNewpid);
    }
  return 0;
}


#ifndef INLINE_SOCKETS
socket_data_p 
udp_getsocket(void) {
  int i;
  EnterCritical();
  for (i = 0; i < NR_SOCKETS; i++) {
    if (udp_shared_data->sockets[i].inuse == 0) {
      udp_shared_data->sockets[i].inuse = 1;
      ExitCritical();
      return &udp_shared_data->sockets[i];
    }
  }
  ExitCritical();
  return (socket_data_p) 0;
}

void
udp_putsocket(socket_data_p p) {
  assert(p->inuse == 1);
  EnterCritical();
  p->inuse = 0;
  ExitCritical();
}
#endif

struct udp_shared_data * const udp_shared_data = (struct udp_shared_data *) UDP_SHARED_REGION;

int
udp_socket_init(void) {
    int i;
    int status;

    START(fd_op[UDP_SOCKET_TYPE],init);
    DPRINTF(CLUHELP_LEVEL,("udp_socket_init\n"));

    status = fd_shm_alloc(FD_SHM_OFFSET + UDP_SOCKET_TYPE,
			   (sizeof(struct udp_shared_data)),
			   (char *)UDP_SHARED_REGION);

    StaticAssert((sizeof(struct udp_shared_data)) <= UDP_SHARED_REGION_SZ);

    if (status == -1) {
	demand(0, problems attaching shm);
	STOP(fd_op[UDP_SOCKET_TYPE],init);
	return -1;
    }
    OnFork(udp_pass_all_ref);
    OnExec(udp_pass_all_ref);

    /* wasteful situation when NR_SOCKETS > NR_OPEN  */
    StaticAssert(NR_SOCKETS < NR_OPEN);

    if (status) {
      /* printf("Initializing udp shared data structture\n"); */
#ifdef INLINE_SOCKETS
      /* if fails, not enough space to store socket data in the file data
	 increase file data to 88 */
      StaticAssert(FILE_DATA_SIZE >= sizeof (socket_data_t));
#endif
      for (i = 0; i < NR_SOCKETS; i++) {
	udp_shared_data->used_ports[i] = 0;
#ifndef INLINE_SOCKETS
	udp_shared_data->sockets[i].inuse = 0;
#endif	  
      }
      //printf("Using PKT RINGS\n");
      for (i = 0; i < NR_RINGBUFS; i++)
	udp_shared_data->ringbufs[i].next = NULL;
    } else {
/*	printf("This is not first process, just attaching memory\n");*/
    }

    /* AF_UNIX, SOCK_DGRAM, UDP */
    register_family_type_protocol(2,2,17,udp_socket);
    /* AF_UNIX, SOCK_DGRAM, IP */
    register_family_type_protocol(2,2,0,udp_socket);
    register_file_ops((struct file_ops *)&udp_file_ops, UDP_SOCKET_TYPE);
    STOP(fd_op[UDP_SOCKET_TYPE],init);

    return 0;
}


