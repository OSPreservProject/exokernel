
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

#include <errno.h>
#include <exos/kprintf.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "handler.h"
#include "interim_syscalls.h"

extern struct reg_storage r_s;

ssize_t interim_readv(int d, const struct iovec *iov, int iovcnt)
{
  ssize_t bcount = 0;
  int vcount, r;

  for (vcount = 0; vcount < iovcnt; vcount++, iov++)
    {
      if (!iov->iov_len) continue;
      r = read(d, iov->iov_base, iov->iov_len);
      if (r == -1) 
	{
	  if (bcount == 0) bcount = -1;
	  break;
	}
      bcount += r;
      if (r != iov->iov_len) break;
    }
  return bcount;
}

ssize_t interim_writev(int d, const struct iovec *iov, int iovcnt)
{
  ssize_t bcount = 0;
  int vcount, r;

  for (vcount = 0; vcount < iovcnt; vcount++, iov++)
    {
      if (!iov->iov_len) continue;
      r = write(d, iov->iov_base, iov->iov_len);
      if (r == -1)
	{
	  if (bcount == 0) bcount = -1;
	  break;
	}
      bcount += r;
      if (r != iov->iov_len) break;
    }
  return bcount;
}

int interim_setitimer(int which, const struct itimerval *value,
		      struct itimerval *ovalue)
{
  if (ovalue)
    {
      ovalue->it_interval.tv_sec = 0;
      ovalue->it_interval.tv_usec = 0;
      ovalue->it_value.tv_sec = 0;
      ovalue->it_value.tv_usec = 0;
    }
  return 0;
}

int interim_setsockopt(int s, int level, int optname, const void *optval,
		       int optlen)
{
  return 0;
}

int interim_lseek(int fildes, int pad, quad_t offset, int whence)
{
  off_t res = lseek(fildes,offset,whence);

  r_s.edx = ((int*)&res)[1];

  return ((int*)&res)[0];
}

#define	POLLIN		0x0001
#define	POLLPRI		0x0002
#define	POLLOUT		0x0004
#define	POLLERR		0x0008
#define	POLLHUP		0x0010
#define	POLLNVAL	0x0020
#define	POLLRDNORM	0x0040
#define POLLWRNORM      POLLOUT
#define	POLLRDBAND	0x0080
#define	POLLWRBAND	0x0100
int interim_poll(struct pollfd *fds, int nfds, int timeout) {
  fd_set r, w, e;
  struct timeval t, *tp;
  int i, snfds, ret, maxfd=0;

  if (timeout >= 0) {
    tp = &t;
    t.tv_sec = timeout/1000;
    t.tv_usec = (timeout*1000) % 1000000;
  } else
    tp = NULL;

  FD_ZERO(&r);
  FD_ZERO(&w);
  FD_ZERO(&e);
  if (fds)
    for (i=0; i < nfds; i++) {
      if (fds[i].events & (POLLIN | /* POLLNORM | */ POLLPRI | POLLRDNORM |
			   POLLRDBAND))
	FD_SET(fds[i].fd, &r);
      if (fds[i].events & (POLLOUT | POLLWRNORM | POLLWRBAND))
	FD_SET(fds[i].fd, &w);
      if (fds[i].fd > maxfd) maxfd = fds[i].fd;
    }
  else
    maxfd = 0;

  snfds = select(maxfd, &r, &w, &e, tp);

  if (snfds == -1 || snfds == 0) {
    if (errno == EBADF) {
      kprintf("Poll - bad file descriptor\n");
      exit(-1);
    }
    return nfds;
  }

  ret = 0;
  for (i=0; i < nfds; i++) {
    if (FD_ISSET(fds[i].fd, &r)) fds[i].revents |= POLLIN | /* POLLNORM | */
				   POLLPRI | POLLRDNORM | POLLRDBAND;
    if (FD_ISSET(fds[i].fd, &w)) fds[i].revents |= POLLOUT | POLLWRNORM |
				   POLLWRBAND;
    if (fds[i].revents) ret++;
  }

  return ret;
}
