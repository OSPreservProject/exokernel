
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

#ifndef _TCP_SOCKET_H_

#define _TCP_SOCKET_H_

#include "fd/proc.h"

#include "tcp.h"
#include "global.h"
#include "tcp_struct.h"

#include <assert.h>
#include <exos/debug.h>



int 
tcpsocket_ioctl(struct file *filp, unsigned int request, char *argp);

int 
tcpsocket_recv(struct file *filp, void *buffer, int length, int nonblock,
	       unsigned flags);

int
tcpsocket_read(struct file *filp, char *buffer, int nbyte, int blocking);

int 
tcpsocket_send(struct file *filp, void *buffer, int length, int nonblock, 
	       unsigned flags);

int
tcpsocket_write(struct file *filp, char *buffer, int nbyte, int blocking);

int 
tcpsocket_bind(struct file *, struct sockaddr *name, int namelen);

int 
tcpsocket(struct file *);

int 
tcpsocket_close(struct file *);

int 
tcpsocket_select_pred(struct file *,int, struct wk_term *);

int 
tcpsocket_select(struct file *,int);

int
tcpsocket_connect(struct file *filp, struct sockaddr *uservaddr,
		  int namelen, int flags);

int 
tcpsocket_fcntl (struct file * filp, int request, int arg);

int
tcpsocket_accept(struct file *filp, struct file *newfilp, 
		 struct sockaddr *newsock, int *addr_len, int flags);

int 
tcpsocket_listen(struct file *sock, int backlog);

int
tcpsocket_getname(struct file * filp, struct sockaddr *bound,
		  int *boundlen, int peer);

int 
tcpsocket_setsockopt(struct file *sock, int level, int optname,
		     const void *optval, int optlen);

int 
tcpsocket_getsockopt(struct file *sock, int level, int optname,
		     void *optval, int *optlen);

int 
tcpsocket_shutdown(struct file *sock, int flags);

#define NR_SOCKETS	32
struct tcp_shared_data {
    unsigned short used_ports[NR_SOCKETS];	
    /* we cannot have more ports open than fds
       assumes only one process for now.
       ports in host order. */
    struct tcb connection[MAX_CONNECTION];
    unsigned short next_port;
} *tcp_shared_data;

#endif /* _TCP_SOCKET_H_ */


