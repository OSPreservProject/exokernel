
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

/* #define PRINTF_LEVEL 3 */
#include <malloc.h>

#include <sys/types.h>
#include <sys/time.h>

#include <string.h>
#include "assert.h"
#include <exos/debug.h>
#include <errno.h>


#include "netinet/in.h"
#include "tcp_socket.h"
#ifdef AEGIS
#include <tcp/libtcp.h>
#endif /* AEGIS */
#ifdef EXOPC
#include "libtcp.h"
#endif /* EXOPC */



#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

/* this file exports the following functions:
   tcpsocket_listen
   tcpsocket_accept
 */


/* does not handle blocking and nonblocking flags yet, and need better
 checks to make sure we can do an accept at this point. */
int
tcpsocket_accept(struct file *filp, struct file *newfilp,
		 struct sockaddr *newsockaddr0, int *addr_len, int flags) {
  struct sockaddr_in *newsockaddr = (struct sockaddr_in *)newsockaddr0;
    struct tcp_socket_data *sock;
    struct tcp_socket_data *newsock;

    struct tcb *tcb;
    struct tcp *rcv_tcp;
    struct ip *rcv_ip;

    sock = (struct tcp_socket_data *) &filp->data;

    newfilp->f_mode = 0;
    newfilp->f_pos = 0;
    newfilp->f_flags = 0;
    filp_refcount_init(newfilp);
    filp_refcount_inc(newfilp);
    newfilp->f_owner = __current->pid;
    newfilp->op_type = TCP_SOCKET_TYPE;
    newsock = (struct tcp_socket_data *) &newfilp->data;

    newsock->tcb = 0;
    newsock->port = sock->port;
    
    /* we have to look at the filp->flags here */
    tcb = tcp_accept(sock->tcb);
    if ((tcb)) {
	/* need to figure out who I am connected to. */
	newsock->tcb = tcb;
	rcv_tcp = tcb->rcv_eit.tcp;
	rcv_ip = tcb->rcv_eit.ip;
	if (newsockaddr) {
	    memcpy((char *)&newsockaddr->sin_addr, 
		   (char *)&rcv_ip->source[0], 
		   sizeof (newsockaddr->sin_addr));
	    newsockaddr->sin_port = rcv_tcp->src_port;
	    *addr_len = sizeof (struct sockaddr);

	    DPRINTF(2, ("ACCEPT: dst port: %d\n",htons(newsockaddr->sin_port)));
	    DPRINTF(2, ("ip dst: %08x\n",*((int *)(&newsockaddr->sin_addr.s_addr))));
	}
	return 0;
    } else {
	errno = EWOULDBLOCK;
	return -1;
    }
}

int 
tcpsocket_listen(struct file *filp, int backlog) {
    struct tcp_socket_data *sock;

    sock = (struct tcp_socket_data *) &filp->data;

    sock->tcb = tcp_listen(backlog, 0, 0, 0, htons(sock->port), 0, 0);

    if (sock->tcb == 0) return -1;

    return 0;
}
