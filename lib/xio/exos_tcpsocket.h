
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


#ifndef _EXOS_TCPSOCKET_H_
#define _EXOS_TCPSOCKET_H_

#include "fd/proc.h"

#include "exos/critical.h"
#define RETURNCRITICAL  ExitCritical(); return
#define ENTERCRITICAL   EnterCritical();
//#define ENTERCRITICAL   assert (u.u_in_critical == 0); EnterCritical();
#define EXITCRITICAL    ExitCritical();
//#define EXITCRITICAL    assert (u.u_in_critical == 1); ExitCritical();


#define FILEP_GETSOCKPTR(filep)		(*((struct tcpsocket **)(filep)->data))
#define FILEP_SETSOCKPTR(filep,ptr)	*((struct tcpsocket **)(filep)->data) = (ptr);
#define FILEP_TCPDATASIZE	(sizeof(void*))



/******* prototypes for fd-level functions provided by exos_tcpsocket.c *******/

int exos_tcpsocket_ioctl(struct file *filp, unsigned int request, char *argp);

int exos_tcpsocket_recvfrom(struct file *filp, void *buffer, int length, int nonblock, unsigned flags,struct sockaddr *ignoresock, int *ignoreaddrlen);

int exos_tcpsocket_read(struct file *filp, char *buffer, int nbyte, int blocking);

int exos_tcpsocket_sendto(struct file *filp, void *buffer, int length, int nonblock, unsigned flags, struct sockaddr *ignoresock, int ignoreaddlen);

int exos_tcpsocket_write(struct file *filp, char *buffer, int nbyte, int blocking);

int exos_tcpsocket_bind(struct file *, struct sockaddr *name, int namelen);

int exos_tcpsocket(struct file *);

int exos_tcpsocket_close(struct file *);

int exos_tcpsocket_select_pred(struct file *,int, struct wk_term *);

int exos_tcpsocket_select(struct file *,int);

int exos_tcpsocket_connect(struct file *filp, struct sockaddr *uservaddr, int namelen, int flags);

int exos_tcpsocket_fcntl (struct file * filp, int request, int arg);

int exos_tcpsocket_accept(struct file *filp, struct file *newfilp, struct sockaddr *newsock, int *addr_len, int flags);

int exos_tcpsocket_listen(struct file *sock, int backlog);

int exos_tcpsocket_getname(struct file * filp, struct sockaddr *bound, int *boundlen, int peer);

int exos_tcpsocket_setsockopt(struct file *sock, int level, int optname, const void *optval, int optlen);

int exos_tcpsocket_getsockopt(struct file *sock, int level, int optname, void *optval, int *optlen);

int exos_tcpsocket_shutdown(struct file *sock, int flags);

int exos_tcpsocket_stat (struct file *filp, struct stat *buf);

#endif /* _EXOS_TCPSOCKET_H_ */

