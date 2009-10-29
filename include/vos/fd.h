
#ifndef __VOS_FD_H__
#define __VOS_FD_H__

#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <xok/types.h>

#include <vos/errno.h>
#include <vos/kprintf.h>
#include <vos/fdtypes.h>
#include <vos/wk.h>
#include <vos/sbuf.h>
#include <vos/sync.h>


#define MAX_FD_ENTRIES 		1024
#define MAX_FD_TYPES   		64

#define NR_OPEN 128
#define SELECT_READ   1
#define SELECT_WRITE  2
#define SELECT_EXCEPT 3


MK_SHARED_RECORD
  (fd_entry, 
   struct {
     u_char shared;
     u_char protected;
   },
   struct {
     u_short fd;
     u_short type;
     u_short refcnt;
     u_char  nb;
     void *  state;
   });

/* macros to check attributes */
#define FD_ISNB(fd) ((fd_entry_shr_t*)fd)->nb==1

/* per process file descriptor table, fds are recycled */
extern SR_T(fd_entry)* fds[MAX_FD_ENTRIES];


/* specifies operations for a type of fd */
typedef struct fd_op
{
  /* sharing related operations */

  /* verify state of descriptor */
  int (*verify) (S_T(fd_entry) *fd);
  /* increment the reference on the state of current descriptor */
  int (*incref) (S_T(fd_entry) *fd, u_int new_envid);

  /* generic file operations */

  /* open descriptor */
  int (*open) (S_T(fd_entry) *fd, const char *name, int flags, mode_t mode);
  
  /* reading from the descriptor */
  int (*read) (S_T(fd_entry) *fd, void *buffer, int nbyte);
  /* writing to the descriptor */
  int (*write) (S_T(fd_entry) *fd, const void *buffer, int nbyte);
  
  /* scatter read */
  int (*readv) (S_T(fd_entry) *fd, const struct iovec* vec, int count);
  /* scatter write */
  int (*writev) (S_T(fd_entry) *fd, const struct iovec* vec, int count);

  /* goto a particularly location */
  off_t (*lseek) (S_T(fd_entry) *fd, off_t offset, int whence);
 
  /* return status of descriptor */
  int (*select) (S_T(fd_entry) *fd, int rw);
  /* wait on descriptor */
  int (*select_pred) (S_T(fd_entry) *fd, int rw, struct wk_term *pred);
  
  /* manipulate options */
  int (*ioctl) (S_T(fd_entry) *fd, unsigned int request, char *argp); 
  int (*fcntl) (S_T(fd_entry) *fd, int cmd, long arg);
  int (*flock) (S_T(fd_entry) *fd, int op);
  
  /* called anytime there is a close, including the last close */
  int (*close) (S_T(fd_entry) *fd);
  /* called once the last reference to the object is closed*/
  int (*close_final) (S_T(fd_entry) *fd);
  
  /* duplicate file descriptor */  
  int (*dup2) (S_T(fd_entry) *fd, S_T(fd_entry) *newfd);
  
  /* obtain file status */  
  int (*fstat) (S_T(fd_entry) *fd, struct stat *buf);

  /* network related operations */

  int (*socket) (S_T(fd_entry) *fd, int domain, int type, int protocol);
  int (*bind) (S_T(fd_entry) *fd, const struct sockaddr *umyaddr, int len);
  int (*connect) (S_T(fd_entry) *fd, const struct sockaddr *uservaddr, int len);
  int (*accept) (S_T(fd_entry) *fd, S_T(fd_entry) *newfd,
                 struct sockaddr *newsockaddr, int *addr_len);
  int (*listen) (S_T(fd_entry) *fd, int len);
  int (*sendto) (S_T(fd_entry) *fd, const void *buff, 
                 size_t len, unsigned flags, 
                 const struct sockaddr *, int addr_len);
  int (*recvfrom) (S_T(fd_entry) *fd, void *buff, size_t len, unsigned flags, 
                   struct sockaddr *, int *addr_len);
} fd_op_t;
#define NUM_FD_OPS 18

/* table of operations for each type of fd */
extern fd_op_t fd_operations[MAX_FD_TYPES];



/* 
 * register_fd_ops: registers a new fd type and operation. The types are
 * symbolicly defined in fdtypes.h. Always returns 0.
 */
extern inline int
register_fd_ops(u_short type, const fd_op_t* op)
{
  if (type >= MAX_FD_TYPES) 
    RETERR(V_BADFD);
  fd_operations[type] = *op;
  return 0;
}


/* fd related system calls */

/*
 * fd_read: read nbytes from object referenced by descriptor d into buffer.
 * buffer should be pre-allocated. Expects a well-defined d (within range and
 * fds[d] non-null. Behavior otherwise unspecified. Returns 0 if successful.
 * Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_read(int d, void *buffer, size_t bytes);

/*
 * fd_write: write nbytes to object referenced by descriptor d from buffer.
 * buffer should be pre-allocated. Expects a well-defined d (within range and
 * fds[d] non-null. Behavior otherwise unspecified. Returns 0 if successful.
 * Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_write(int d, const void *buffer, size_t nbytes);
 
/* fd_readv, fd_writev: scattered read and write. Scattered buffer should be
 * pre-allocated. Expects a well-defined d (within range and fds[d] non-null.
 * Behavior otherwise unspecified. Returns 0 if successful.  Returns -1
 * otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_readv (int d, const struct iovec* vec, int count);
extern int fd_writev (int d, const struct iovec* vec, int count);

/*
 * fd_lseek: go to a particular location in file. Only works for file
 * implementations (not for sockets or pipes, that is). Expects a well-defined
 * d (within range and fds[d] non-null. Behavior otherwise unspecified.
 * Returns 0 if successful.  Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern off_t fd_lseek(int d, off_t offset, int whence);

/*
 * fd_fcntl: manipulate options on a file descriptor. Expects a
 * well-defined d (within range and fds[d] non-null. Behavior otherwise
 * unspecified. Returns 0 if successful.  Returns -1 otherwise. Errno is
 * set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_fcntl(int d, int cmd, long arg);

/*
 * fd_flock: put a lock on a file (not the descriptor, so dup won't
 * duplicate the lock, but will use the same one). Expects a well-defined d
 * (within range and fds[d] non-null. Behavior otherwise unspecified.
 * Returns 0 if successful.  Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_flock(int d, int op);

/*
 * fd_fstat: Obtains status on an open file descriptor. Expects a well-defined
 * d (within range and fds[d] non-null. Behavior otherwise unspecified.
 * Returns 0 if successful.  Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_fstat(int d, struct stat *buf);

/*
 * fd_close: close the object referenced by descriptor d.  Decrement fd
 * reference count. If reference count is 0, call close_final on the specific
 * object. Expects a well-defined d (within range and fds[d] non-null.
 * Behavior otherwise unspecified. Returns 0 if successful.  Returns -1
 * otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *             does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_close(int d);

/*
 * fd_open: open a file descriptor. Returns file descriptor number if
 * successful.  Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with the fd does not have the
 *             requested operation defined.  
 *   V_MFILE:  all fds used up
 *   V_NOPERM: no permission to open requested file descriptor
 *   errnos defined by specific objects related to the file descriptor type.
 */
extern int fd_open(const char *path, int flags, mode_t mode);

/*
 * fd_dup2: duplicate descriptor d, use newd as the new descriptor. If newd
 * already in use, close first. Returns file descriptor number if successful.
 * Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with the fd does not have the
 *                       requested operation defined.  
 *   V_MFILE:  all fds used up
 *   errnos defined by specific objects related to the file descriptor type.
 *
 * fd_dup: same as dup2, accept allocates a new fd number.
 */
extern int fd_dup2(int d, int newd);
extern int fd_dup(int d);

/*
 * fd_socket: create a socket. Returns file descriptor number if successful.
 * Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with the fd does not have the
 *                       requested operation defined.  
 *   V_MFILE:  all fds used up
 *   V_BADSOCKET: bad domain specified, bad type specified
 * 
 *   errnos defined by specific objects related to the file descriptor type.
 */
extern int fd_socket (int domain, int type, int protocol);

/*
 * fd_connect: establish a network connection on a socket. Expects a
 * well-defined d (within range and fds[d] non-null. Behavior otherwise
 * unspecified. Returns 0 if successful.  Returns -1 otherwise. Errno is set
 * to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *                       does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_connect(int d, const struct sockaddr *name, int namelen);

/*
 * fd_bind: bind a socket to a specific address. Expects a well-defined d
 * (within range and fds[d] non-null. Behavior otherwise unspecified.  Returns
 * 0 if successful.  Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *                       does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_bind (int d, const struct sockaddr *name, int namelen);

/*
 * fd_listen: specify that the socket is willing to receive connections.
 * Expects a well-defined d (within range and fds[d] non-null. Behavior
 * otherwise unspecified. Returns 0 if successful. Returns -1 otherwise. Errno
 * is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *                       does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_listen (int d, int backlog);

/*
 * fd_accept: accepts an connection on the socket. Return only when connection
 * has been established. Expects a well-defined d (within range and fds[d]
 * non-null. Behavior otherwise unspecified. Returns the new descriptor if
 * successful. Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *                       does not have the requested operation defined.
 *   V_MFILE:  all fds used up
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_accept (int d, struct sockaddr* addr, int *addrlen);

/*
 * fd_sendto: sends a packet to a specified address. Expects a well-defined d
 * (within range and fds[d] non-null. Behavior otherwise unspecified. Returns
 * the amount sent if successful. Returns -1 otherwise. Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *                       does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_sendto (int d, const void *msg, size_t len, int flags, 
                      const struct sockaddr *to, int tolen);

/*
 * fd_recvfrom: reads a packet from a specified address. Expects a
 * well-defined d (within range and fds[d] non-null. Behavior otherwise
 * unspecified.  Returns the amount received if successful. Returns -1
 * otherwise.  Errno is set to:
 *
 *   V_BADFD:  type associated with fd is out of range or type
 *                       does not have the requested operation defined.
 *   errnos defined by specific objects related to the file descriptor.
 */
extern int fd_recvfrom(int d, void *buf, size_t len, int flags, 
                       struct sockaddr* from, int *fromlen);


/*
 * fd_protected/fd_unprotected: prevent/allow a descriptor from/tobe inherited
 * by a child process across fork.
 */
extern void fd_protect(int d);
extern void fd_unprotect(int d);


/* fd related system call stubs: these stubs implement the synchronization
 * algorithm around normal fd system calls. Return value of the stubs is the
 * same as that of the underlining fd system calls, except an additional
 * errno, V_BADFD, is passed back if the descriptor given is not found,
 * null, or corrupted */

/* most fd related system call stubs use the following macro! */
#define GEN_FD_SYSCALL_STUB(d, fdcall) \
{ \
  if (d >= MAX_FD_ENTRIES || d < 0 || fds[d] == NULL) \
    RETERR(V_BADFD); \
  { \
    int r; \
    if (fds[d]->pri.shared) \
    { \
      u_short type = (fds[d]->self)->type; \
      SR_LOCK(fds[d]); \
      SR_LOCALIZE(fds[d],(*(fd_operations[type].verify)),); \
      if (fds[d]->self->refcnt==0) \
	fds[d]->pri.shared = 0; \
    } \
    r = fdcall; \
    if (fds[d]->pri.shared) \
    { \
      SR_UPDATE_META(fds[d]); \
      SR_UNLOCK(fds[d]); \
    } \
    return r; \
  }  \
  RETERR(V_BADFD); \
}

#define GEN_FD_SYSCALL_STUB_X(d, fdcall) \
{ \
  if (d >= MAX_FD_ENTRIES || d < 0 || fds[d] == NULL) \
    RETERR(V_BADFD); \
  { \
    int r; \
    if (fds[d]->pri.shared) \
    { \
      SR_LOCK(fds[d]); \
      if (fds[d]->self->refcnt==0) \
	fds[d]->pri.shared = 0; \
    } \
    r = fdcall; \
    if (fds[d]->pri.shared) \
    { \
      SR_UNLOCK(fds[d]); \
    } \
    return r; \
  }  \
  RETERR(V_BADFD); \
}


extern inline int 
read(int d, void *buffer, size_t nbytes)
{
  GEN_FD_SYSCALL_STUB(d, fd_read(d, buffer, nbytes));
}

extern inline int 
write(int d, const void *buffer, size_t nbytes)
{
  GEN_FD_SYSCALL_STUB(d, fd_write(d, buffer, nbytes));
}

extern inline int 
readv (int d, const struct iovec* vec, int count)
{
  GEN_FD_SYSCALL_STUB(d, fd_readv(d, vec, count));
}

extern inline int 
writev (int d, const struct iovec* vec, int count)
{
  GEN_FD_SYSCALL_STUB(d, fd_writev(d, vec, count));
}

extern inline off_t 
lseek(int d, off_t offset, int whence)
{
  GEN_FD_SYSCALL_STUB(d, fd_lseek(d, offset, whence));
}

extern inline int 
fcntl(int d, int cmd, long arg)
{
  GEN_FD_SYSCALL_STUB(d, fd_fcntl(d, cmd, arg));
}

extern inline int 
flock(int d, int op)
{
  GEN_FD_SYSCALL_STUB(d, fd_flock(d, op));
}

extern inline int
fstat(int d, struct stat *buf)
{
  GEN_FD_SYSCALL_STUB(d, fd_fstat(d, buf));
}

extern inline int 
dup2(int d, int newd)
{
  if (newd >= MAX_FD_ENTRIES || newd < 0)
    RETERR(V_BADFD); 

  GEN_FD_SYSCALL_STUB(d, fd_dup2(d, newd));
}

extern inline int 
dup(int d)
{
  GEN_FD_SYSCALL_STUB(d, fd_dup(d));
}

extern inline int 
open(const char *path, int flags, mode_t mode)
{
  return fd_open(path,flags,mode);
}

extern inline int 
socket(int domain, int type, int protocol)
{
  return fd_socket(domain, type, protocol);
}

extern inline int 
recvfrom(int d, void *buf, size_t len, int flags, 
         struct sockaddr* from, int *fromlen)
{
  GEN_FD_SYSCALL_STUB(d, fd_recvfrom(d, buf, len, flags, from, fromlen));
}

extern inline int 
sendto(int d, const void *msg, size_t len, int flags, 
       const struct sockaddr *to, int tolen)
{
  GEN_FD_SYSCALL_STUB(d, fd_sendto(d, msg, len, flags, to, tolen));
}

extern inline int
accept (int d, struct sockaddr* addr, int *addrlen)
{
  GEN_FD_SYSCALL_STUB(d, fd_accept(d, addr, addrlen));
}

extern inline int 
listen (int d, int backlog)
{
  GEN_FD_SYSCALL_STUB(d, fd_listen(d, backlog));
}

extern inline int 
bind (int d, const struct sockaddr *name, int namelen)
{
  GEN_FD_SYSCALL_STUB(d, fd_bind(d, name, namelen));
}

extern inline int 
connect(int d, const struct sockaddr *name, int namelen)
{
  GEN_FD_SYSCALL_STUB(d, fd_connect(d, name, namelen));
}

/*
 * close: wrapper around fd_close. At the end, frees buffer used for sharing
 * and free file descriptor. Returns 0 if successful. -1 otherwise. Errno is
 * set to:
 *
 *   V_BADFD: fd is corrupted.
 *   errno defined by fd_close.
 */
extern inline int 
close(int d)
{
  if (d >= MAX_FD_ENTRIES || d < 0 || fds[d] == NULL) 
    RETERR(V_BADFD);

  {
    int r;
    if (fds[d]->pri.shared)
    {
      u_short type = (fds[d]->self)->type;
      SR_LOCK(fds[d]);
      SR_LOCALIZE(fds[d],(*(fd_operations[type].verify)),);
      if (fds[d]->self->refcnt==0)
	fds[d]->pri.shared = 0;
    }
    r = fd_close(d);
    if (fds[d]->pri.shared)
    {
      SR_MOVETO_MASTER(fds[d]);
      SR_UNLOCK(fds[d]);
    }

    sbuf_free(&(fds[d]->__self));
    free(fds[d]);
    fds[d] = NULL;
    return r;
  }
  RETERR(V_BADFD);
}

/*
 * select: go through all fd specified and construct a big wkup pred 
 */
extern int
select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, 
       struct timeval *timeout);
 

/* 
 * ioctl: control io operation properties.
 */
extern inline int
ioctl (int d, unsigned long request, void *argp)
{
  RETERR(V_BADFD);
}


/*
 * flush pending io
 */
extern inline void
flush_io ()
{
  fflush(stdin);
  fflush(stdout);
  fflush(stderr);

  /* XXX - should flush all io descriptors */
}


#endif

