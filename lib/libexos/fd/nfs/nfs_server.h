
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

#ifndef _NFS_SERVER_H_
#define _NFS_SERVER_H_


#include <fd/proc.h>		/* for definition of BLOCKING */
#include <xok/env.h>


struct file;

typedef struct generic_server {
  exos_lock_t lock;
  struct file *socketf;
  struct sockaddr_in  *addr; /* file server address */
  struct sockaddr_in  hidden_addr;
  struct nfs_fh  *fh;        /* File handle to be mounted */
  struct nfs_fh  hidden_fh;
  int        flags;      /* flags */
  int        wsize;      /* write size in bytes */
  int        rsize;      /* read size in bytes */
  int        timeo;      /* initial timeout in .1 secs */
  int        retrans;    /* times to retry send */
  int        acregmin;   /* attr cache file min secs */
  int        acregmax;   /* attr cache file max secs */
  int        acdirmin;   /* attr cache dir min secs */
  int        acdirmax;   /* attr cache dir max secs */
  dev_t  fakedevice;
  int nrwrites;
  int nrbcwrites;
  
} generic_server_t, *generic_server_p;

int 
initialize_hostname(void);


generic_server_p
make_server(char *host, unsigned int port);

void
free_server(generic_server_p serverp);

void 
change_server_port(generic_server_p server, int port);

/* 
 * Communication Procedures 
 */

#define SERVER (server->socketf)
static inline int 
server_send(generic_server_p server,
	    int *start,int len) {	
  extern int fd_udp_sendto(struct file *filp, void *msg, int len, int nonblocking, unsigned flags, struct sockaddr *to, int tolen);
  return fd_udp_sendto(SERVER,(void *)start,len,BLOCKING, 0, 
		       (struct sockaddr *)server->addr, sizeof(struct sockaddr));
}

static inline int 
server_sendv(generic_server_p server, struct ae_recv *msg_recv) {	
  extern int fd_udp_sendtov(struct file *filp, struct ae_recv *msg_recv, int nonblocking, unsigned flags, struct sockaddr *to, int tolen);
  return fd_udp_sendtov(SERVER,msg_recv,
			BLOCKING,0,
			(struct sockaddr *)server->addr,
			(sizeof(struct sockaddr)));
}

static inline int
server_recvfrom(generic_server_p server,int *start, int len, int flags) {
  extern int fd_udp_recvfrom(struct file *filp, void *buffer, int length, int nonblocking, unsigned flags, struct sockaddr *reg_rfrom, int *rfromlen);
  return fd_udp_recvfrom(SERVER,start,len, BLOCKING, flags,
			 (struct sockaddr *)0,(int *)0);
}

static inline int
server_select(generic_server_p server, struct timeval *timeout) {
  extern int __select_single_filp(struct file *filp, struct timeval *timeout);
  return __select_single_filp(SERVER,timeout);
}

/* for debugging */
void 
pr_server(generic_server_p serverp);


#endif _NFS_SERVER_H_
