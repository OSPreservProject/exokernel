
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

#ifndef _UDP_SOCKET_H_

#define _UDP_SOCKET_H_

#include "fd/proc.h"
#include "udp_struct.h"

#include <xok/pktring.h>


#define RECVFROM_WAITING  0
#define RECVFROM_GOT      1

#define UDP_CAP 0

#define MSG_DONTBLOCK  0x8	/* additional option for recvfrom */

/* global shared data structure */
extern struct udp_shared_data {
  /* HBXX leave this defined for now for easier transition */
  ringbuf_t ringbufs[NR_RINGBUFS];

  unsigned short used_ports[NR_SOCKETS];	
#ifndef INLINE_SOCKETS
  socket_data_t sockets[NR_SOCKETS];
#endif
  /* we cannot have more ports open than fds
     assumes only one process for now.
     ports in host order. */
} * const udp_shared_data;



int fd_udp_sendto(struct file *, void *msg, int len, int nonblock, 
	       unsigned flags, struct sockaddr *to, int tolen);

int fd_udp_write(struct file *filp, char *msg, int len, int nonblocking);

int fd_udp_recvfrom(struct file *, void *buf, int len, int nonblock, 
		 unsigned flags, struct sockaddr *from, int *fromlen);

int fd_udp_setsockopt(struct file *sock, int level, int optname,
		      const void *optval, int optlen);

int fd_udp_read(struct file *filp, char *buffer, int length, int nonblocking);

int fd_udp_bind(struct file *, struct sockaddr *name, int namelen);

int fd_udp_fcntl (struct file *, int request, int arg);

int udp_socket(struct file *);

int fd_udp_close(struct file *);

int fd_udp_select_pred (struct file *, int, struct wk_term *);

int fd_udp_select(struct file *,int);

int udp_socket_init(void);

int fd_udp_connect(struct file *sock, struct sockaddr *servaddr,
	       int sockaddr_len, int flags);

int fd_udp_getname(struct file * filp, 
		   struct sockaddr *bound, int *boundlen, int peer);


/* Internals */
extern unsigned short 
fd_bind_getport(unsigned short port, unsigned char *ip_src, int *fid, int *fidset, int ring_id);


/* if private = 1 it will malloc the ringbuffer, otherwise it will 
   allocate it from shared memory */
ringbuf_p 
udp_getringbuf(int size, int flags); 
#define UDP_PRIVATE_BUFFERS 1
#define UDP_SHARED_BUFFERS 0
int
udp_putringbuf(ringbuf_p r);


/* allocation of sockets */
socket_data_p 
udp_getsocket(void);
void
udp_putsocket(socket_data_p p);
#ifdef INLINE_SOCKETS
#define GETSOCKDATA(filp) ((socket_data_p)&filp->data)
#else
struct local_socket_data {
  socket_data_p data;
};
#define GETSOCKDATA(filp) (((struct local_socket_data *)filp->data)->data)
#endif

/* extended interface for setting of bind time options */
void 
udp_use_private_buffers(void);
int 
udp_set_nr_buffers(int nr);


/* DEBUGGING
 pr_ringbuf prints out information pointed by p, but not the whole ring.
 pr_ringbufs, prints out information of the whole ringbuf
 pr_ringbufs_all, prints out information of the shared ringbufs */
extern int
pr_used_ports(void);
extern void
pr_ringbuf(ringbuf_p r);
extern void
pr_ringbufs(ringbuf_p r);
extern void
pr_ringbufs_all(void );
extern void 
pr_recvfrom(struct recvfrom * recvfrom);
extern void 
kpr_recvfrom(struct recvfrom * recvfrom);
extern void 
pr_udpfilp(struct socket_data *data);

#define CHECK(status) {\
  if (status < 0) kprintf("WARNING %d at %s %s:%d\n",\
			  status,__FUNCTION__,__FILE__,__LINE__);\
								   }

#endif /* _UDP_SOCKET_H_ */


