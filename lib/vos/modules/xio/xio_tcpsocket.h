
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


#ifndef __XIO_TCPSOCKET_H__
#define __XIO_TCPSOCKET_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "xio_tcpcommon.h"
#include <vos/locks.h>

#define dprintf if (0) kprintf


/* types for xio_tcpsocket_select[_pred] calls */
#define SELECT_READ	1
#define SELECT_WRITE	2
#define SELECT_EXCEPT	3


#define XIO_TCPSOCKET_BUFMAX	(4 * XIO_TCPBUFFER_DATAPERBUF)


/* close states for TCP sockets */
#define XIOSOCK_READABLE	1
#define XIOSOCK_WRITEABLE	2
#define XIOSOCK_FULLOPEN	3
#define XIOSOCK_CLOSED		4
#define XIOSOCK_FREE		8

/* flags for TCP sockets */
#define XIOSOCK_SNDFINAFTERWRITES	0x10

typedef struct tcpsocket {
   struct tcb tcb;
   xio_nwinfo_t *nwinfo;
   struct xio_tcpsock_info *info;
   struct tcpsocket *next;
   struct tcpsocket *livenext;
   struct tcpsocket *liveprev;
   int gen;
   uint flags;
   int sock_state;
   int write_offset;
   int read_offset;
   int buf_max;
   int demux_id;
   int netcardno;
} tcpsocket_t;

typedef struct xio_tcpsock_info {
   int inited;
   time_t lasttime;
   char * (*pagealloc)(void *info, int len);
   int active_tcpsockets;
   int tcpsock_pages;
   struct tcpsocket *livelist;
   struct tcpsocket *freelist;
   xio_tbinfo_t tbinfo;
   xio_dmxinfo_t dmxinfo;
   int usetwinfo;
   xio_nwinfo_t nwinfo;
   xio_twinfo_t twinfo;
   struct tcpsocket firstsock;
   spinlock_t lock;
   char space[4];
} xio_tcpsock_info_t;

char *xio_tcpsocket_default_pagealloc (void *, int);
void xio_tcpsocket_initinfo (xio_tcpsock_info_t *info, char * (*pagealloc)(void *, int), int usetwinfo);
void xio_tcpsocket_freesock (struct tcpsocket *tcpsock);
struct tcpsocket * xio_tcpsocket_allocsock (xio_tcpsock_info_t *info);
void xio_tcpsocket_handlePackets (xio_tcpsock_info_t *info);
int xio_tcpsocket_close (struct tcpsocket *sock, int noblock);
int xio_tcpsocket_bind (struct tcpsocket *sock, const struct sockaddr *reg_name, int namelen);
int xio_tcpsocket_getname(struct tcpsocket *sock, struct sockaddr *reg_name, int *namelen, int peer);

int xio_tcpsocket_select (struct tcpsocket *sock, int rw);

struct wk_term;
int xio_tcpsocket_select_pred (struct tcpsocket *sock, int rw, struct wk_term *t);
#define XIO_TCPSOCKET_PRED_SIZE (XIO_TCP_PRED_SIZE+XIO_WAITFORPACKET_PREDLEN+1)
int xio_tcpsocket_pred (struct tcpsocket *sock, struct wk_term *t);

int xio_tcpsocket_ioctl (struct tcpsocket *sock, unsigned int request, char *argp);
int xio_tcpsocket_recv (struct tcpsocket *sock, void *buffer, int length, int nonblock, unsigned flags);
int xio_tcpsocket_read (struct tcpsocket *sock, char *buffer, int length, int nonblocking);
int xio_tcpsocket_stat (struct tcpsocket *filp, struct stat *buf);
int xio_tcpsocket_write (struct tcpsocket *sock, const char *buffer, int length, int nonblocking);
int xio_tcpsocket_send (struct tcpsocket *sock, void *buffer, int length, int nonblock, unsigned flags);
int xio_tcpsocket_connect (struct tcpsocket *sock, const struct sockaddr *uservaddr, int namelen, int flags, int nonblock, uint16 _src_port, char *src_ip, char * dst_ip, char *src_eth, char *dst_eth);
int xio_tcpsocket_setsockopt (struct tcpsocket *sock, int level, int optname, const void *optval, int optlen);
int xio_tcpsocket_getsockopt (struct tcpsocket *sock, int level, int optname, void *optval, int *optlen);
int xio_tcpsocket_shutdown (struct tcpsocket *sock, int flags);
struct tcpsocket * xio_tcpsocket_accept(struct tcpsocket *sock, struct sockaddr *newsockaddr0, int *addr_len, int flags, int nonblock);
int xio_tcpsocket_listen (struct tcpsocket *sock, int backlog);

#endif  /* __XIO_TCPSOCKET_H__ */

