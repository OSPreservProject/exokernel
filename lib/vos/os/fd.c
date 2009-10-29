/*
 * Copyright MIT 1999
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <xok/sysinfo.h>

#include <vos/proc.h>
#include <vos/vm.h>
#include <vos/kprintf.h>
#include <vos/assert.h>

#include <vos/fd.h>

int sr_localize = 0;

/*
 * XXX - FIXME: right now, we place fd type specific routines obliviously in
 * the untrusted synchronization protocol. the protocol acquires a spinlock
 * before we enter the generic non-type-specific fd_xxx routines in this file.
 * therefore, if we in turn calls a fd type specific routine, say xio's accept
 * routine, that blocks, other processes waiting on the spinlock is also
 * blocked... not good. since we don't use the untrusted sync protocol to
 * guard xio's state anyways, really need to separate fd type specific
 * routines away from generic fd routines, at least not protect it with the
 * spinlock.
 */

fd_op_t fd_operations[MAX_FD_TYPES];
SR_T(fd_entry)* fds[MAX_FD_ENTRIES];


#define dprintf if (0) kprintf

#define CHECK_FD_OPS(type, op) \
  if ((fd_operations[(type)].op) == 0L) \
    RETERR(V_BADFD); 

#define CHECK_FD_OPS_FREE(type, op, d) \
  if ((fd_operations[(type)].op) == 0L) { \
    free(d); \
    RETERR(V_BADFD); \
  }


static inline int
find_free_fd()
{
  int i;
  for(i=0; i<MAX_FD_ENTRIES; i++)
  {
    dprintf("fds[i] = %p\n",fds[i]);
    if (fds[i] == NULL)
      return i;
  }
  return -1;
}


/* expects valid d */
static inline int
fd_incref(int d, u_int new_envid)
{
  S_T(fd_entry) *fd = fds[d]->self;
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, incref);

  fd->refcnt++;
  dprintf("env %d: fd: inc ref fd %d, refcnt incremented to %d\n",
      getpid(),d,fd->refcnt);
  return (*(fd_operations[type].incref))(fd, new_envid);
}


static inline int 
incref(int d, u_int new_envid)
{
  GEN_FD_SYSCALL_STUB(d, fd_incref(d, new_envid));
}


static void 
fd_before_fork(u_int new_envid)
{
  int i;

  for(i=0; i<MAX_FD_ENTRIES; i++)
  {
    if (fds[i] != NULL && fds[i]->pri.protected == 0)
    {
      incref(i, new_envid);
      SR_UPDATE_META(fds[i]);
      fds[i]->pri.shared = 1;
    }
  }
  return;
}


static void 
fd_child_start()
{
  int i;

  for(i=0; i<MAX_FD_ENTRIES; i++)
  {
    if (fds[i] != NULL && fds[i]->pri.protected == 1)
      fds[i] = NULL;
  }
  return;
}


static void
fd_on_exit()
{
  int i;

  flush_io();
  for(i=0; i<MAX_FD_ENTRIES; i++)
  {
    if (fds[i] != NULL) 
      close(i);
  }
  dprintf("fd_on_exit() done\n");
  return;
}


void 
fd_init()
{
  int i;
  
  for(i=0; i<MAX_FD_ENTRIES; i++)
  {
    fds[i] = NULL;
  }

  for(i=0; i<MAX_FD_TYPES; i++)
  {
    bzero(&fd_operations[i], sizeof(fd_op_t));
  }

  proc_before_fork(fd_before_fork);
  proc_child_start(fd_child_start);
  atexit(fd_on_exit);
}


void
fd_protect(int d) /* do not inherit descriptor d when forking */
{
  if (fds[d] != NULL) fds[d]->pri.protected = 1;
}


void
fd_unprotect(int d) /* do inherit descriptor d when forking */
{
  if (fds[d] != NULL) fds[d]->pri.protected = 0;
}


/* expects valid d */
int
fd_read(int d, void *buffer, size_t bytes)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);

  CHECK_FD_OPS(type, read);
  return (*(fd_operations[type].read))(fds[d]->self, buffer, bytes);
}


/* expects valid d */
int
fd_write(int d, const void *buffer, size_t bytes)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, write);
  return (*(fd_operations[type].write))(fds[d]->self, buffer, bytes);
}


/* expects valid d */
int
fd_readv(int d, const struct iovec *vec, int count)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, readv);
  return (*(fd_operations[type].readv))(fds[d]->self, vec, count);
}


/* expects valid d */
int
fd_writev(int d, const struct iovec *vec, int count)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, writev);
  return (*(fd_operations[type].writev))(fds[d]->self, vec, count);
}


/* expects valid d */
off_t
fd_lseek(int d, off_t offset, int whence)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, lseek);
  return (*(fd_operations[type].lseek))(fds[d]->self, offset, whence);
}


/* expects valid d */
int
fd_fcntl(int d, int cmd, long arg)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, fcntl);
  return (*(fd_operations[type].fcntl))(fds[d]->self, cmd, arg);
}


/* expects valid d */
int
fd_flock(int d, int op)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, flock);
  return (*(fd_operations[type].flock))(fds[d]->self, op);
}


/* expects valid d */
int
fd_fstat(int d, struct stat *b)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, fstat);
  return (*(fd_operations[type].fstat))(fds[d]->self, b);
}


/* expects valid d and newd */
int
fd_dup2(int d, int newd)
{
  u_short type = (fds[d]->self)->type;
  S_T(fd_entry) *newfd;
  S_T(fd_entry) *fd;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, dup2);

  if (fds[newd] != NULL)
  {
    if (close(newd) < 0) 
      return -1;
  }

  fds[newd] = (SR_T(fd_entry)*) malloc(sizeof(SR_T(fd_entry)));
  INIT_SHARED_RECORD(fd_entry, *fds[newd], 0, 0);
  fds[newd]->pri.shared = 0;
  fds[newd]->pri.protected = 0;

  newfd = fds[newd]->self;
  newfd->state = 0L;
  newfd->fd = newd;
  newfd->refcnt = 1;
  
  fd = fds[d]->self;
  newfd->nb = fd->nb;
  newfd->type = fd->type;

  if ((*(fd_operations[type].dup2))(fd, newfd) < 0)
  {
    free(fds[newd]);
    fds[newd] = NULL;
    return -1;
  }

  return newd;
}


/* expects valid d */
int
fd_dup(int d)
{
  int newd = find_free_fd();
  return fd_dup2(d, newd);
}


/* expects valid d */
int
fd_close(int d)
{
  S_T(fd_entry) *fd = fds[d]->self;
  u_short type = fd->type;
  int r;

  dprintf("env %d: fd_close %d\n", getpid(),d);

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, close);

  r = (*(fd_operations[type].close))(fd);
  if (r < 0)
    return r;

  dprintf("env %d: fd: refcnt %d\n", getpid(),fd->refcnt);

  fd->refcnt--;
  dprintf("env %d: fd: closing fd %d, refcnt decremented to %d\n", 
      getpid(),d,fd->refcnt);

  if (fd->refcnt == 0)
  {
    dprintf("env %d: fd: closing fd %d, calling close_final\n",getpid(),d);
    r = (*(fd_operations[type].close_final))(fd);
    dprintf("env %d: fd: closing fd %d, called close_final\n",getpid(),d);
    if (r < 0)
      return r;

    fd->type = 0;
    fd->state = 0;
  }

  return 0;
}


int 
fd_open(const char *path, int flags, mode_t mode)
{
  int d = find_free_fd();
  int r;
  S_T(fd_entry) *fd;

  if (d == -1)
    RETERR(V_MFILE);
  
  fds[d] = (SR_T(fd_entry)*) malloc(sizeof(SR_T(fd_entry)));
  INIT_SHARED_RECORD(fd_entry, *fds[d], 0, 0);
  fds[d]->pri.shared = 0;
  fds[d]->pri.protected = 0;

  fd = fds[d]->self;
  fd->fd = d;
  fd->state = 0L;
  fd->refcnt = 1;
  fd->nb = 0;

  dprintf("env %d: fd: opening fd %d, refcnt 1\n",getpid(),d);

  if (!strcmp(path,"/dev/console"))
    fd->type = FD_TYPE_CONSOLE;

  else if (!strncmp(path,"/dev/ipcport",12))
    fd->type = FD_TYPE_SPIPE;

  else if (!strncmp(path,"/dev/pktring",12))
    fd->type = FD_TYPE_PRD;

  else
    fd->type = FD_TYPE_FSRV_FILE;

  CHECK_FD_OPS_FREE(fd->type, open, fds[d]);

  r = (*(fd_operations[fd->type].open))(fd, path, flags, mode);
  if (r < 0) 
  {
    free(fds[d]);
    fds[d] = NULL;
    return r;
  }
  return d;
}

  
int
fd_socket(int domain, int type, int protocol)
{
  int d = find_free_fd();
  int r;
  S_T(fd_entry) *fd;

  if (d == -1)
    RETERR(V_MFILE);

  switch(domain)
  {
    case AF_INET:
      {
	switch(type)
	{
	  case SOCK_DGRAM:
	    CHECK_FD_OPS(FD_TYPE_UDPSOCKET, socket);

            fds[d] = (SR_T(fd_entry)*) malloc(sizeof(SR_T(fd_entry)));
  	    INIT_SHARED_RECORD(fd_entry, *fds[d], 0, 0);
  	    fds[d]->pri.shared = 0;
  	    fds[d]->pri.protected = 0;

  	    fd = fds[d]->self;
  	    fd->fd = d;
  	    fd->state = 0L;
  	    fd->refcnt = 1;
  	    fd->nb = 0;
    	    fd->type = FD_TYPE_UDPSOCKET;

	    r = (*(fd_operations[fd->type].socket))(fd, domain, type, protocol);
	    if (r < 0)
	      return r;
	    return d;
	  
	  case SOCK_STREAM:
	    CHECK_FD_OPS(FD_TYPE_TCPSOCKET, socket);

            fds[d] = (SR_T(fd_entry)*) malloc(sizeof(SR_T(fd_entry)));
  	    INIT_SHARED_RECORD(fd_entry, *fds[d], 0, 0);
  	    fds[d]->pri.shared = 0;
  	    fds[d]->pri.protected = 0;

  	    fd = fds[d]->self;
  	    fd->state = 0L;
  	    fd->fd = d;
  	    fd->refcnt = 1;
  	    fd->nb = 0;
    	    fd->type = FD_TYPE_TCPSOCKET;

  	    r = (*(fd_operations[fd->type].socket))(fd, domain, type, protocol);
	    if (r < 0)
	      return r;
	    return d;

	  default:
	    RETERR(V_BADSOCKET);
	}
      }
      break;
    default:
      RETERR(V_BADSOCKET);
  }
}


/* expects valid d */
int 
fd_connect(int d, const struct sockaddr *name, int namelen)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, connect);
  return (*(fd_operations[type].connect))(fds[d]->self, name, namelen);
}


/* expects valid d */
int 
fd_bind(int d, const struct sockaddr *name, int namelen)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, bind);
  return (*(fd_operations[type].bind))(fds[d]->self, name, namelen);
}


/* expects valid d */
int 
fd_listen (int d, int backlog)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, listen);
  return (*(fd_operations[type].listen))(fds[d]->self, backlog);
}


/* expects valid d */
int 
fd_accept (int d, struct sockaddr* addr, int *addrlen)
{
  u_short type = (fds[d]->self)->type;
  S_T(fd_entry) *fd;
  S_T(fd_entry) *newfd;
  int newd = find_free_fd();

  if (newd == -1)
    RETERR(V_MFILE);

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, accept);
  
  fds[newd] = (SR_T(fd_entry)*) malloc(sizeof(SR_T(fd_entry)));
  INIT_SHARED_RECORD(fd_entry, *fds[newd], 0, 0);
  fds[newd]->pri.shared = 0;
  fds[newd]->pri.protected = 0;

  newfd = fds[newd]->self;
  newfd->fd = newd;
  newfd->refcnt = 1;
  
  fd = fds[d]->self;
  newfd->nb = fd->nb;
  newfd->state = 0L;
  newfd->type = fd->type;

  if ((*(fd_operations[type].accept))(fd, newfd, addr, addrlen) < 0)
  {
    free(fds[newd]);
    fds[newd] = NULL;
    return -1;
  }
  return newd;
}


/* expects valid d */
int 
fd_sendto(int d, const void *msg, size_t len, int flags, 
          const struct sockaddr *to, int tolen)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, sendto);
  return (*(fd_operations[type].sendto))(fds[d]->self,msg,len,flags,to,tolen);
}


/* expects valid d */
int 
fd_recvfrom(int d, void *buf, size_t len, int flags, 
            struct sockaddr* from, int *flen)
{
  u_short type = (fds[d]->self)->type;

  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  
  CHECK_FD_OPS(type, recvfrom);
  return 
    (*(fd_operations[type].recvfrom))(fds[d]->self,buf,len,flags,from,flen);
}



#define NR_OPEN	128
#define SELECT_READ   1
#define SELECT_WRITE  2
#define SELECT_EXCEPT 3

#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#undef RATE
#define RATE (__sysinfo.si_rate)
#undef TICKS
#define TICKS (__sysinfo.si_system_ticks)
#define SELECT_EXCEPT_CONDITIONS 1


static inline void
copyfds(fd_set *a,fd_set *b, int width) 
{
  memcpy((void*)a,(void*)b,width);
}

/* expects valid d */
int
select(int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	   struct timeval *timeout)
{
#define WK_SELECT_SZ 1024
#define DID_FDREADY 1		
#define DID_TIMEOUT 2

  unsigned int wait_ticks = 0;  
  int fd;
  struct wk_term t[WK_SELECT_SZ];  
  int next;
  int total = 0;
  fd_set newreadfds, newwritefds, newexceptfds;
  int had_prev_term;

  width = MIN (width+1, NR_OPEN);

  /* make sure that all fd's set to be polled are valid fd's */
  for (fd = 0; fd < width; fd++) {
    if ((readfds && FD_ISSET (fd, readfds)) || 
	(writefds && FD_ISSET (fd, writefds)) ||
	(exceptfds && FD_ISSET (fd, exceptfds))) 
    {
      u_short type;

      if (fd >= MAX_FD_ENTRIES || fd < 0 || fds[fd] == NULL) 
	RETERR(V_BADFD);
      
      type = (fds[fd]->self)->type;

      if (type >= MAX_FD_TYPES) 
        RETERR(V_BADFD);
  
      CHECK_FD_OPS(type, select);
      CHECK_FD_OPS(type, select_pred);
    }
  }

  FD_ZERO(&newreadfds);
  FD_ZERO(&newwritefds);
  FD_ZERO(&newexceptfds);

  /* Our basic algorithm is poll the fd's once. If any fd's are found ready
   * return. Otherwise sleep until one of them might be ready and repeat the
   * process. In practice we don't expect to go through this loop more than
   * once, but theoretically we could wakeup for some reason other than
   * haveing fd's ready. Am I just being paranoid? */

  /* Note: we can _definitely_ go thru this loop more than once, because in
   * some cases (e.g., TCP), we can only sleep on an "indication" that select
   * might pass (e.g., a packet arrived).  We have to then call select to find
   * out if it does in fact make the socket ready, and rebuild the sleep
   * predicate otherwise. */

  do {
    had_prev_term = 0;

    /* do a poll on the fd's. We want to make sure that we do this before
     * sleeping so on the first time through the do loop we avoid descheduling
     * and having to wait till we're re-scheduled before noticing that
     * there're fd's ready. */

    for (fd = 0; fd < width; fd++) 
    {
      if (readfds && FD_ISSET (fd, readfds)) {
        u_short type = (fds[fd]->self)->type;
        if ((*(fd_operations[type].select))(fds[fd]->self,SELECT_READ)) {
	  total++;
	  FD_SET (fd, &newreadfds);
	}
      }
      if (writefds && FD_ISSET (fd, writefds)) {
        u_short type = (fds[fd]->self)->type;
        if ((*(fd_operations[type].select))(fds[fd]->self,SELECT_WRITE)) {
	  total++;
	  FD_SET (fd, &newwritefds);
	}
      }
      if (SELECT_EXCEPT_CONDITIONS && exceptfds && FD_ISSET (fd, exceptfds)) {
        u_short type = (fds[fd]->self)->type;
        if ((*(fd_operations[type].select))(fds[fd]->self,SELECT_EXCEPT)) {
	  total++;
	  FD_SET (fd, &newexceptfds);
	}
      }
    }

    /* ok, we found some fd's that we need to report. Replace the fdsets the
     * user passed in with fdsets containing which fd's are ready and return
     * the total number of fd's ready. */

    if (total) {
      if (readfds)
	copyfds (readfds, &newreadfds, width);
      if (writefds)
	copyfds (writefds, &newwritefds, width);
      if (exceptfds)
	copyfds (exceptfds, &newexceptfds, width);
      return total;
    }

    /* if the user is just polling, handle that now before going through all
     * the work to construct a predicate */

    if (timeout) {
      wait_ticks = ((1000000/RATE) * timeout->tv_sec) + 
	           (timeout->tv_usec + RATE - 1)/RATE;
      if (!wait_ticks) {
	if (readfds) FD_ZERO(readfds);
	if (writefds) FD_ZERO(writefds);
	if (exceptfds) FD_ZERO(exceptfds);
	return 0;
      }
    }

    /* now construct a wakeup-predicate that will wake us when something
     * interesting happens on these fd's. We call each fd's select_pred
     * operation which returns a clause of the final predicate. All clauses
     * are combined into one large predicate that we'll sleep on. */

    next = 0;
    had_prev_term = 0;
    next = wk_mktag (next, t, DID_FDREADY);
    
    for (fd = 0; fd < width; fd++) 
    {
      int r;

      if (readfds && FD_ISSET (fd, readfds)) {
        u_short type = (fds[fd]->self)->type;
	if (had_prev_term)
	  next = wk_mkop (next, t, WK_OR);	
        r = (*(fd_operations[type].select_pred))
	    (fds[fd]->self,SELECT_READ,&t[next]);
	assert(r>0);
	next += r;
	had_prev_term = 1;
      }
	  
      if (writefds && FD_ISSET (fd, writefds)) {
        u_short type = (fds[fd]->self)->type;
	if (had_prev_term)
	  next = wk_mkop (next, t, WK_OR);	
        r = (*(fd_operations[type].select_pred)) 
	    (fds[fd]->self,SELECT_WRITE,&t[next]);
	assert(r>0);
	next += r;
	had_prev_term = 1;
      }

      if (SELECT_EXCEPT_CONDITIONS && exceptfds && FD_ISSET (fd, exceptfds)) {
        u_short type = (fds[fd]->self)->type;
	if (had_prev_term)
	  next = wk_mkop (next, t, WK_OR);	
        r = (*(fd_operations[type].select_pred)) 
	    (fds[fd]->self,SELECT_EXCEPT,&t[next]); 
	assert(r>0);
	next += r;
	had_prev_term = 1;
      }
    }

    /* slap on a final term to wake us when the timeout occurrs, if there is
     * one */

    if (timeout) {
      if (had_prev_term)
	next = wk_mkop (next, t, WK_OR);
      next = wk_mktag (next, t, DID_TIMEOUT);
      next += wk_mksleep_pred(&t[next], wait_ticks+__sysinfo.si_system_ticks);
      had_prev_term = 1;
    }

    /* wait for predicate to evaluate to true */
    wk_waitfor_pred (t, next);

    /* u_pred_tag is set to the piece of the predicate that caused
       us to wake up */

    if (UAREA.u_pred_tag == DID_TIMEOUT) {
      if (readfds) FD_ZERO(readfds);
      if (writefds) FD_ZERO(writefds);
      if (exceptfds) FD_ZERO(exceptfds);
      return 0;
    }
  } while (1);
}


