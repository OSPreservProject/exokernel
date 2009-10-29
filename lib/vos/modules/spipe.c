
/*
 * Copyright MIT 1999
 */

/* a shared memory implementation of pipe */

#include <stdlib.h>

#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>

#include <vos/fdtypes.h>

#include <vos/kprintf.h>
#include <vos/ipc.h>
#include <vos/vm.h>
#include <vos/errno.h>
#include <vos/locks.h>
#include <vos/assert.h>

#include <vos/ipcport.h>
#include <vos/fd.h>


#define dprintf if (0) kprintf


/* spipe: a trusted pipe implemented in shared memory. conventioned yield lock
 * is used for synchronization! also, once a data is read by one, it is
 * removed.
 */
#define SPIPESZ		2048
#define SPIPE_CLOSED	0
#define SPIPE_OPEN	1
typedef struct
{
  yieldlock_t lock;	/* trusted yield lock */
  char    status;	/* status of pipe: closed or open */
  u_short refcnt;	/* reference count used when sharing... 
			 * ref cnt for fd is in the fd structure */
  u_short head_ptr;	/* ptr to first unread char */
  u_short free_ptr;	/* ptr to first free char */
  u_short bytes;	/* number of bytes in buffer */
  char    buf[SPIPESZ];	/* the buffer */
} spipe_state_t;


/*
 * define a spipe structure on the va given
 */
void
spipe_def(u_int va)
{
  spipe_state_t *s = (spipe_state_t*) va;
  yieldlock_reset(&s->lock);
  s->status = SPIPE_CLOSED;
  s->refcnt = 0;
  s->head_ptr = 0;
  s->free_ptr = 0;
  s->bytes = 0;
}


/*
 * read buffer from spipe
 */
static int
spipe_read (S_T(fd_entry) *fd, void *buffer, int nbyte)
{
  int size = 0;
  spipe_state_t *s;

  if (fd->type != FD_TYPE_SPIPE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (spipe_state_t*)fd->state;
 
  yieldlock_acquire(&s->lock);

  /* we ar the only reader, abort */
  if (s->head_ptr == s->free_ptr && s->refcnt <= 1) 
  {
    yieldlock_release(&s->lock);
    RETERR(V_NOTCONN);
  }

  else if (FD_ISNB(fd) && (s->head_ptr == s->free_ptr))
  {
    yieldlock_release(&s->lock);
    RETERR(V_WOULDBLOCK);
  }

wait:
  if (s->head_ptr == s->free_ptr) /* blocking */
  {
    dprintf("env %d: spipe: read waiting, we have %d %d\n",
	getpid(),s->free_ptr,s->head_ptr);
    if (s->refcnt <= 1)
    {
      yieldlock_release(&s->lock);
      RETERR(V_NOTCONN);
    }
    yieldlock_release(&s->lock);
    if (fds[fd->fd]->pri.shared)
      SR_UNLOCK(fds[fd->fd]);
    env_yield(-1);
    if (fds[fd->fd]->pri.shared)
      SR_LOCK(fds[fd->fd]);
    yieldlock_acquire(&s->lock);
    goto wait;
  }

  /* more to read */
  if (s->head_ptr < s->free_ptr)
    size = s->free_ptr - s->head_ptr;
  else
    size = SPIPESZ - s->head_ptr;

  /* requestd size is less than available memory */
  if (size > nbyte)
  {
    size = nbyte;
    memmove(buffer, &s->buf[s->head_ptr], size);
    s->head_ptr += size;
  }

  /* no need to wrap around */
  else if (s->head_ptr < s->free_ptr)
  {
    memmove(buffer, &s->buf[s->head_ptr], size);
    s->head_ptr += size;
  }
 
  /* need to wrap around, so two reads */
  else
  {
    u_int tsz;
    memmove(buffer, &s->buf[s->head_ptr], size);
    tsz = s->free_ptr;
    if (tsz > nbyte-size)
      tsz = nbyte-size;
    memmove(&((char*)buffer)[size],&s->buf[0], tsz);
    s->head_ptr = tsz;
    size += tsz;
  }

  s->bytes -= size;
  yieldlock_release(&s->lock);
  return size;
}


/*
 * write buffer to spipe
 */
static int
spipe_write (S_T(fd_entry) *fd, const void *buffer, int nbyte)
{
  spipe_state_t *s;
  u_int size;

  if (fd->type != FD_TYPE_SPIPE)
    RETERR(V_BADFD);

  if (fd->state == 0L)
    RETERR(V_BADFD);

  if (nbyte < 0)
    RETERR(V_INVALID);

  if (nbyte == 0)
    return 0;

  s = (spipe_state_t*)fd->state;
  yieldlock_acquire(&s->lock);

  /* already closed */
  if (s->status == SPIPE_CLOSED)
  {
    yieldlock_release(&s->lock);
    RETERR(V_NOTCONN);
  }

  /* if not enough space, return overflow */
  if (s->bytes + nbyte > SPIPESZ)
  {
    yieldlock_release(&s->lock);
    RETERR(V_PIPE);
  }

  if (s->head_ptr > s->free_ptr)
    size = s->head_ptr - s->free_ptr;
  else
    size = SPIPESZ - s->free_ptr;

  /* available space w/o wrapping is greater than bytes needed */
  if (size >= nbyte) 
  {
    memmove(&s->buf[s->free_ptr], buffer, nbyte);
    s->free_ptr += nbyte;
    if (s->free_ptr == SPIPESZ)
      s->free_ptr = 0;
  }

  /* need to wrap */
  else
  {
    memmove(&s->buf[s->free_ptr], buffer, size);
    memmove(&s->buf[0], &((char*)buffer)[size], nbyte-size);
    s->free_ptr = nbyte-size;
  }

  s->bytes += nbyte;
  dprintf("env %d: spipe: after write on %d, free: %d head: %d, ref %d\n",
      getpid(),va2ppn((u_int)s),s->free_ptr,s->head_ptr,s->refcnt);
  yieldlock_release(&s->lock);
  return nbyte;
}



/*
 * close state for the final time (ref cnt expired)
 */
static int
spipe_close_final (S_T(fd_entry) *fd)
{
  spipe_state_t *s;

  if (fd->type != FD_TYPE_SPIPE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (spipe_state_t*)fd->state;
  yieldlock_acquire(&s->lock);
  
  if (s->status == SPIPE_CLOSED)
  {
    yieldlock_release(&s->lock);
    return 0;
  }

  if (s->refcnt == 0)
    s->status = SPIPE_CLOSED;

  dprintf("env %d: spipe: close_final called, pipe status %d\n", 
      getpid(),s->status);
  
  yieldlock_release(&s->lock);

  return 0;
}


/*
 * close fd 
 */
static int
spipe_close (S_T(fd_entry) *fd)
{
  spipe_state_t *s;
  
  if (fd->type != FD_TYPE_SPIPE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (spipe_state_t*)fd->state;
  
  yieldlock_acquire(&s->lock);
  
  if (s->status == SPIPE_OPEN)
    s->refcnt--;
  
  dprintf("env %d: spipe: close called, refcnt decremented to %d\n", 
      getpid(), s->refcnt);

  yieldlock_release(&s->lock);

  return 0;
}


/*
 * open fd
 */
static int
spipe_open (S_T(fd_entry) *fd, const char *name, int flags, mode_t mode)
{
  spipe_state_t *s;

  if (strncmp(name,"/dev/ipcport",12)==0)
  {
    int pid = atoi(&name[12]);
    
    if (pid == -1)
      RETERR(V_BADFD);

    if (ipcports[envidx(pid)].pte == 0)
      RETERR(V_BADFD);

    fd->state = (void*) ipcports[envidx(pid)].va;
    
    s = (spipe_state_t*)fd->state;

    yieldlock_acquire(&s->lock);

    if (s->status == SPIPE_CLOSED)
      s->status = SPIPE_OPEN;

    asm volatile("" ::: "memory");
    s->refcnt++;
    dprintf("env %d: spipe: open: spipe on %d, refcnt incremented to %d\n",
	getpid(), va2ppn((u_int)s), s->refcnt);
    dprintf("env %d: spipe: open: spipe is at 0x%x, %d\n", 
	getpid(), (u_int)s, va2ppn((u_int)s));
  
    yieldlock_release(&s->lock);
  }

  else
    RETERR(V_BADFD);
  
  return fd->fd;
}


/*
 * verify state
 */
static int
spipe_verify(S_T(fd_entry) *fd)
{
  if (fd->type != FD_TYPE_SPIPE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  return 0;
}


/*
 * duplicate fd and state
 */
static int
spipe_incref(S_T(fd_entry) *fd, u_int new_envid)
{
  spipe_state_t *s;

  if (fd->type != FD_TYPE_SPIPE)
    RETERR(V_BADFD);

  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (spipe_state_t*)fd->state;
  
  yieldlock_acquire(&s->lock);
  
  if (s->status == SPIPE_CLOSED)
  {
    yieldlock_release(&s->lock);
    RETERR(V_NOTCONN);
  }
 
  s->refcnt++;
  dprintf("env %d: spipe: dup called, spipe on %d, refcnt incremented to %d\n", 
      getpid(), va2ppn((u_int)s), s->refcnt);
  yieldlock_release(&s->lock);

  return 0;
}



static fd_op_t const spipe_ops = {
  spipe_verify,
  spipe_incref,

  spipe_open,
  spipe_read,
  spipe_write,
  NULL, /* readv */
  NULL, /* writev */
  NULL, /* lseek */
  NULL, /* select */
  NULL, /* select_pred */
  NULL, /* ioctl */
  NULL, /* fcntl */
  NULL, /* flock */
  spipe_close,
  spipe_close_final,
  NULL, /* dup */
  NULL, /* fstat */
  NULL, /* socket */
  NULL, /* bind */
  NULL, /* connect */
  NULL, /* accept */
  NULL, /* listen */
  NULL, /* sendto */
  NULL  /* recvfrom */
};


/* call this to initialize spipe fd stuff */
void
spipe_init()
{
  register_fd_ops(FD_TYPE_SPIPE, &spipe_ops);
}

