
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

#include "fd/proc.h"
#include <exos/callcount.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <assert.h>

#include <exos/debug.h>
#include <xok/sysinfo.h>
#include <exos/uwk.h>

#undef RATE
#define RATE (__sysinfo.si_rate)
#undef TICKS
#define TICKS (__sysinfo.si_system_ticks)

#define SELECT_EXCEPT_CONDITIONS 1

static inline void
copyfds(fd_set *a,fd_set *b, int width) {
  memcpy((void*)a,(void*)b,width);
}

int
select(int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	   struct timeval *timeout)
{
  unsigned int wait_ticks = 0;  
  int fd;
#define WK_SELECT_SZ 1024
  struct wk_term t[WK_SELECT_SZ];  
  int next;
  int total = 0;
  fd_set newreadfds, newwritefds, newexceptfds;
#define DID_FDREADY 1		
#define DID_TIMEOUT 2
#define DID_SIGNAL 3
  struct file *filp;
  int had_prev_term;

  OSCALLENTER(OSCALL_select);
  width = MIN (width+1, NR_OPEN);

  /* make sure that all fd's set to be polled are valid fd's */

  for (fd = 0; fd < width; fd++) {
    if ((readfds && FD_ISSET (fd, readfds)) || 
	(writefds && FD_ISSET (fd, writefds)) ||
	(exceptfds && FD_ISSET (fd, exceptfds))) {
      CHECKFD(fd, OSCALL_select);
      assert (CHECKOP (__current->fd[fd], select));
      assert (CHECKOP (__current->fd[fd], select_pred));
    }
  }

  FD_ZERO(&newreadfds);
  FD_ZERO(&newwritefds);
  FD_ZERO(&newexceptfds);

  /* Our basic algorithm is poll the fd's once. If any fd's are found
     ready return. Otherwise sleep until one of them might be ready
     and repeat the process. In practice we don't expect to go
     through this loop more than once, but theoretically we could
     wakeup for some reason other than haveing fd's ready. Am I just
     being paranoid? */

  /* Note: we can _definitely_ go thru this loop more than once, because
     in some cases (e.g., TCP), we can only sleep on an "indication" that
     select might pass (e.g., a packet arrived).  We have to then call
     select to find out if it does in fact make the socket ready, and rebuild
     the sleep predicate otherwise. */

  do {

    had_prev_term = 0;

    /* do a poll on the fd's. We want to make sure that we do this
       before sleeping so on the first time through the do loop we
       avoid descheduling and having to wait till we're re-scheduled
       before noticing that there're fd's ready. */

    for (fd = 0; fd < width; fd++) {
      if (readfds && FD_ISSET (fd, readfds))
	if (DOOP (__current->fd[fd], select, (__current->fd[fd], SELECT_READ))) {
	  total++;
	  FD_SET (fd, &newreadfds);
	}
      if (writefds && FD_ISSET (fd, writefds))
	if (DOOP (__current->fd[fd], select, (__current->fd[fd], SELECT_WRITE))) {
	  total++;
	  FD_SET (fd, &newwritefds);
	}	
      if (SELECT_EXCEPT_CONDITIONS && exceptfds && FD_ISSET (fd, exceptfds))
	if (DOOP (__current->fd[fd], select, (__current->fd[fd], SELECT_EXCEPT))) {
	  total++;
	  FD_SET (fd, &newexceptfds);
	}	
    }

    /* ok, we found some fd's that we need to report. Replace the
       fdsets the user passed in with fdsets containing which
       fd's are ready and return the total number of fd's ready. */

    if (total) {
      if (readfds)
	copyfds (readfds, &newreadfds, width);
      if (writefds)
	copyfds (writefds, &newwritefds, width);
      if (exceptfds)
	copyfds (exceptfds, &newexceptfds, width);
      /* XXX */
      OSCALLEXIT(OSCALL_select);
      return total;
    }

    /* if the user is just polling, handle that now before going through
       all the work to construct a predicate */

    if (timeout) {
      wait_ticks = ((1000000/RATE) * timeout->tv_sec) +
	(timeout->tv_usec + RATE - 1)/RATE;
      if (!wait_ticks)
	{
	  if (readfds) FD_ZERO(readfds);
	  if (writefds) FD_ZERO(writefds);
	  if (exceptfds) FD_ZERO(exceptfds);
	  OSCALLEXIT(OSCALL_select);
	  return 0;
	}
    }

    /* now construct a wakeup-predicate that will wake us when something
       interesting happens on these fd's. We call each fd's select_pred
       operation which returns a clause of the final predicate. All
       clauses are combined into one large predicate that we'll sleep on. */

    next = 0;
    had_prev_term = 0;
    next = wk_mktag (next, t, DID_FDREADY);
    for (fd = 0; fd < width; fd++) {
      filp = __current->fd[fd];
      if (readfds && FD_ISSET (fd, readfds)) {
	if (had_prev_term)
	  next = wk_mkop (next, t, WK_OR);	
	next += DOOP (filp,select_pred,(filp,SELECT_READ,&t[next]));
	had_prev_term = 1;
      }
	  
      if (writefds && FD_ISSET (fd, writefds)) {
	if (had_prev_term)
	  next = wk_mkop (next, t, WK_OR);	
	next += DOOP (filp,select_pred,(filp,SELECT_WRITE,&t[next]));	
	had_prev_term = 1;
      }

      if (SELECT_EXCEPT_CONDITIONS && exceptfds && FD_ISSET (fd, exceptfds)) {
	if (had_prev_term)
	  next = wk_mkop (next, t, WK_OR);	
	next += DOOP (filp,select_pred,(filp,SELECT_EXCEPT,&t[next]));	
	had_prev_term = 1;
      }
    }
    /* slap on a final term to wake us when the timeout occurrs, if there
       is one */

    if (timeout) {
      if (had_prev_term)
	next = wk_mkop (next, t, WK_OR);
      next = wk_mktag (next, t, DID_TIMEOUT);
      next += wk_mksleep_pred (&t[next], wait_ticks + __sysinfo.si_system_ticks);
      had_prev_term = 1;
    }

    /* we need to wakeup if a signal comes in */
    if (had_prev_term) {
       next = wk_mkop (next, t, WK_OR);
    }
    next = wk_mktag (next, t, DID_SIGNAL);
    next += wk_mksig_pred (&t[next]);
    had_prev_term = 1;
    
    /* wait for predicate to evaluate to true */
    wk_waitfor_pred (t, next);

    /* u_pred_tag is set to the piece of the predicate that caused
       us to wake up */

    if (UAREA.u_pred_tag == DID_TIMEOUT) {
      if (readfds) FD_ZERO(readfds);
      if (writefds) FD_ZERO(writefds);
      if (exceptfds) FD_ZERO(exceptfds);
      OSCALLEXIT(OSCALL_select);
      return 0;
    }
    if (UAREA.u_pred_tag == DID_SIGNAL) {
      //kprintf("%d select interrupted by signal\n",getpid());
      errno = EINTR;
      OSCALLEXIT(OSCALL_select);
      return -1;
    }
  
  } while (1);
}

/* read select for a single filp with timeout.  More efficient than
   using general select */
int
__select_single_filp(struct file *filp, struct timeval *timeout)
{
  unsigned long long wait_ticks = 0;  
  unsigned long long wait_until = 0;  
  unsigned int begin_ticks = 0;  

#define WK_SELECT_SZ 1024
  struct wk_term t[WK_SELECT_SZ];  
  int next;
#define DID_FDREADY 1		
#define DID_TIMEOUT 2
#define DID_SIGNAL 3
  assert(RATE);
  assert(filp->op_type == UDP_SOCKET_TYPE);
  /* Our basic algorithm is poll the fd's once. If any fd's are found
     ready return. Otherwise sleep until one of them might be ready
     and repeat the process. In practice we don't expect to go
     through this loop more than once, but theoretically we could
     wakeup for some reason other than haveing fd's ready. Am I just
     being paranoid? */

  /* Note: we can _definitely_ go thru this loop more than once, because
     in some cases (e.g., TCP), we can only sleep on an "indication" that
     select might pass (e.g., a packet arrived).  We have to then call
     select to find out if it does in fact make the socket ready, and rebuild
     the sleep predicate otherwise. */

    /* do a poll on the fd's. We want to make sure that we do this
       before sleeping so on the first time through the do loop we
       avoid descheduling and having to wait till we're re-scheduled
       before noticing that there're fd's ready. */

  for(;;) {
    if (DOOP (filp, select, (filp, SELECT_READ))) {
      //kprintf("& %d",(int)__sysinfo.si_system_ticks);
      return 1;
    }
    
    /* if the user is just polling, handle that now before going through
     all the work to construct a predicate */
    
    if (timeout) {
      wait_ticks = ((1000000/RATE) * timeout->tv_sec) + timeout->tv_usec/RATE;
      if (!wait_ticks) {
//	kprintf("%");

	return 0;
      }
    }
    
    /* now construct a wakeup-predicate that will wake us when something
       interesting happens on these fd's. We call each fd's select_pred
       operation which returns a clause of the final predicate. All
       clauses are combined into one large predicate that we'll sleep on. */
    
    next = 0;
    next = wk_mktag (next, t, DID_FDREADY);
    
    next += DOOP (filp,select_pred,(filp,SELECT_READ,&t[next]));
    
    /* slap on a final term to wake us when the timeout occurrs, if there
       is one */

    begin_ticks = (int)__sysinfo.si_system_ticks;
    wait_until = wait_ticks + __sysinfo.si_system_ticks;
    if (timeout) {
      next = wk_mkop (next, t, WK_OR);
      next = wk_mktag (next, t, DID_TIMEOUT);
      next += wk_mksleep_pred (&t[next], wait_until);
    }

	/* we need to wakup if a signal comes in */
    next = wk_mkop (next, t, WK_OR);
    next = wk_mktag (next, t, DID_SIGNAL);
    next += wk_mksig_pred (&t[next]);

	/* wait for predicate to evaluate to true */
    wk_waitfor_pred (t, next);
    
    /* u_pred_tag is set to the piece of the predicate that caused
       us to wake up */
    
    if (UAREA.u_pred_tag == DID_TIMEOUT) {
#if 0
      kprintf("timeout wt %d rate %d\n",
	      (int)wait_ticks,(unsigned int)(RATE));
      kprintf("Timeout sec: %ld usec: %ld\n",timeout->tv_sec,timeout->tv_usec);
      kprintf("begin ticks: %d sleep until ticks: %d si_system_ticks: %d\n",
	      begin_ticks,(int)wait_until,(int)__sysinfo.si_system_ticks);
#endif
      return 0;
    }

  } 
}
#if 0
int 
old_select(int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
       struct timeval *timeout)
{
  int fd;
  fd_set newreadfds, newwritefds, newexceptfds;
  unsigned int wait_ticks = 0;
  unsigned int t,s;
  int total = 0;
  struct file *filp;
  int error;
  int traversed = 0;
  DPRINTF(SYS_LEVEL,("select: entering, timeout %d\n",
		     (timeout != NULL) ? (int) timeout->tv_sec : 0));

  if (timeout != NULL) {
	DPRINTF(SYS_LEVEL,
		("setting waiting time: sec: %d, usec: %d\n",
		 (int) timeout->tv_sec, (int)timeout->tv_usec));
	wait_ticks = ((1000000/RATE) * timeout->tv_sec)
	  + timeout->tv_usec/RATE;
  }
  
  FD_ZERO(&newreadfds);
  FD_ZERO(&newwritefds);
  FD_ZERO(&newexceptfds);

  t = TICKS;
  width = MIN(width,NR_OPEN);
  do {
    DPRINTF(SYS_LEVEL,("polling: %d ticks\n",wait_ticks));
    for (fd = 0; fd <= width; fd++) {
      /* maybe should be using min(width,fdtable_max) */
      /*	DPRINTF(SYS_LEVEL,("testing fd.%d\n",fd));*/
      
      if ((readfds  && FD_ISSET(fd,readfds))) {
	if (!traversed) {
	  CHECKFD(fd);
	}
	filp = __current->fd[fd];
	error = CHECKOP(filp,select);
	assert(error);
	error = DOOP(filp,select,(filp,SELECT_READ));
	if (error) {FD_SET(fd,&newreadfds); total++;}
      }
      if ((writefds  && FD_ISSET(fd,writefds))) {
	if (!traversed) {
	  CHECKFD(fd);
	}
	filp = __current->fd[fd];
	error = CHECKOP(filp,select);
	assert(error);
	error = DOOP(filp,select,(filp,SELECT_WRITE));
	if (error) {FD_SET(fd,&newwritefds); total++;}
      }
    }
    traversed++;

    s = TICKS;
    
    if (total > 0) {
      if (readfds) copyfds(readfds,&newreadfds,width);
      if (writefds) copyfds(writefds,&newwritefds,width);
      return(total);
    }

    usleep(100000);
    
  } while(s < t + wait_ticks || timeout == NULL);
  return(total);
}
#endif
