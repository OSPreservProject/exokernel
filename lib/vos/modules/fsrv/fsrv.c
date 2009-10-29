
/*
 * Copyright MIT 1999
 */

#include <string.h>
#include <stdlib.h>

#include <xok/sys_ucall.h>

#include <vos/uinfo.h>
#include <vos/fd.h>
#include <vos/fdtypes.h>
#include <vos/ipc.h>
#include <vos/vm.h>
#include <vos/errno.h>

#include "../../../../bin/fsrv/fsrv.h"

#define dprintf if (0) kprintf


typedef struct
{
  int fd;
  sbuf_t sb;
} fsrv_fd_t;

#define FSRV_RWSIZE (4096-sizeof(ipc_fsrv_t))

static int
fsrv_read_once (S_T(fd_entry) *fd, void *buffer, int nbyte)
{
  int size = 0;
  fsrv_fd_t *s;
  ipc_fsrv_t *msg;

  if (nbyte > FSRV_RWSIZE)
    RETERR(V_INVALID);
  
  s = (fsrv_fd_t*)fd->state;
  msg = (ipc_fsrv_t*)(s->sb.va);
  msg->req.fd = s->fd;
  msg->req._f_size = nbyte;

  dprintf("pct to read from %d\n",s->fd);
  size = 
    ipc_sleepwait(IPC_FSRV_READ,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,1000);

  if (size < 0)
    return size;

  errno = msg->rep.errno;

  if (msg->rep.retval > 0)
  {
    msg->buf = (void*) (s->sb.va + sizeof(ipc_fsrv_t));
    memmove(buffer, msg->buf, msg->rep.retval);
  }
  
  return msg->rep.retval;
}


static int
fsrv_read (S_T(fd_entry) *fd, void *buffer, int nbyte)
{
  char buf[FSRV_RWSIZE+1];
  int nread = 0;

  dprintf("in fsrv_read, need %d\n", nbyte);

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  while(nread < nbyte)
  {
    int r = fsrv_read_once(fd, &buf[0], FSRV_RWSIZE);
    if (r > 0)
    {
      memmove(&(((char*)buffer)[nread]), &buf[0], r); 
      nread += r;
    }

    if (r < FSRV_RWSIZE) 
      break;
  }
  return nread;
}


static int
fsrv_write_once (S_T(fd_entry) *fd, const void *buffer, int nbyte)
{
  fsrv_fd_t *s;
  u_int size;
  ipc_fsrv_t *msg;

  if (nbyte > FSRV_RWSIZE)
    RETERR(V_INVALID);

  if (nbyte == 0)
    return 0;

  s = (fsrv_fd_t*)fd->state;
  msg = (ipc_fsrv_t*)s->sb.va;
  msg->req.fd = s->fd;
  msg->req._f_size = nbyte;
  msg->buf = (void*) (s->sb.va + sizeof(ipc_fsrv_t));
  memmove(msg->buf, buffer, nbyte);

  size = 
    ipc_sleepwait(IPC_FSRV_WRITE,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,1000);

  if (size < 0)
    return size;

  errno = msg->rep.errno;
  return msg->rep.retval;
}


static int
fsrv_write (S_T(fd_entry) *fd, const void *buffer, int nbyte)
{
  int nwrite = 0;

  dprintf("in fsrv_write, need to write %d\n", nbyte);

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  if (nbyte < 0)
    RETERR(V_INVALID);
  
  while(nwrite < nbyte)
  {
    int wsize = nbyte - nwrite;
    int r;
   
    if (wsize > FSRV_RWSIZE) 
      wsize = FSRV_RWSIZE;
    r = fsrv_write_once(fd, &(((char*)buffer)[nwrite]), wsize);

    if (r > 0)
      nwrite += r;

    if (r < wsize) 
      break;
  }
  return nwrite;
}


static int
fsrv_close_final (S_T(fd_entry) *fd)
{
  fsrv_fd_t *s;
  ipc_fsrv_t *msg;
  int ret;

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (fsrv_fd_t*)fd->state;
  msg = (ipc_fsrv_t*)s->sb.va;
  msg->req.fd = s->fd;
 
  ret = ipc_sleepwait(IPC_FSRV_CLOSE,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,0);
  
  sbuf_free(&(s->sb));
  free(s);
 
  if (ret < 0)
    return ret;

  errno = msg->rep.errno;
  return msg->rep.retval;
}


static int
fsrv_close (S_T(fd_entry) *fd)
{
  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  return 0;
}


static int
fsrv_open (S_T(fd_entry) *fd, const char *name, int flags, mode_t mode)
{
  fsrv_fd_t *s;
  ipc_fsrv_t *msg;
  int ret;
  char *cwd = getenv("PWD");

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (name == 0)
    RETERR(V_INVALID);
  
  s = (fsrv_fd_t*) malloc(sizeof(fsrv_fd_t));
  if (sbuf_alloc(CAP_USER, NBPG, &(s->sb), PG_W) < 0)
  {
    free(s);
    RETERR(V_NOMEM);
  }

  (fsrv_fd_t*)fd->state = s;
  msg = (ipc_fsrv_t*)s->sb.va;
  msg->req._f_flags = flags;
  msg->req._f_mode = mode;

  if (name[0] != '/')
  {
    strcpy(msg->req.name, cwd);
    strcat(msg->req.name, "/");
    strcat(msg->req.name, name);
  }
  else
    strcpy(msg->req.name, name);

  dprintf("pct to open %s\n", msg->req.name);
  ret = ipc_sleepwait(IPC_FSRV_OPEN,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,1000);
  dprintf("pct to open: returned from kernel %d\n", ret);

  if (ret < 0)
    return ret;

  errno = msg->rep.errno;
  
  if (msg->rep.retval > 0)
    s->fd = msg->rep.retval;
 
  dprintf("pct to open: returning %d\n", msg->rep.retval);
  return msg->rep.retval;
}


static int
fsrv_verify(S_T(fd_entry) *fd)
{
  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  return 0;
}


static int
fsrv_incref(S_T(fd_entry) *fd, u_int new_envid)
{
  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);

  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  return 0;
}


static off_t
fsrv_lseek(S_T(fd_entry) *fd, off_t offset, int whence)
{
  int size = 0;
  fsrv_fd_t *s;
  ipc_fsrv_t *msg;

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (fsrv_fd_t*)fd->state;
  msg = (ipc_fsrv_t*)(s->sb.va);
  msg->req.fd = s->fd;
  msg->req._f_offset = offset;
  msg->req._f_whence = whence;

  dprintf("pct to lseek\n");
  size = 
    ipc_sleepwait(IPC_FSRV_LSEEK,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,1000);
  dprintf("pct to lseek, returned %d %d\n", msg->rep.retval, msg->rep.errno);

  if (size < 0)
    return size;
  
  errno = msg->rep.errno;
  return msg->rep.retval;
}


static int
fsrv_fcntl(S_T(fd_entry) *fd, int cmd, long arg)
{
  int size = 0;
  fsrv_fd_t *s;
  ipc_fsrv_t *msg;

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (fsrv_fd_t*)fd->state;
  msg = (ipc_fsrv_t*)(s->sb.va);
  msg->req.fd = s->fd;
  msg->req._f_cmd = cmd;
  msg->req._f_arg = arg;

  dprintf("pct to fcntl\n");
  size = 
    ipc_sleepwait(IPC_FSRV_FCNTL,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,1000);
  dprintf("pct to fcntl, returned %d %d\n", msg->rep.retval, msg->rep.errno);

  if (size < 0)
    return size;
  
  errno = msg->rep.errno;
  return msg->rep.retval;
}


static int
fsrv_flock(S_T(fd_entry) *fd, int op)
{
  int size = 0;
  fsrv_fd_t *s;
  ipc_fsrv_t *msg;

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (fsrv_fd_t*)fd->state;
  msg = (ipc_fsrv_t*)(s->sb.va);
  msg->req.fd = s->fd;
  msg->req._f_cmd = op;

  dprintf("pct to flock\n");
  size = 
    ipc_sleepwait(IPC_FSRV_FLOCK,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,1000);
  dprintf("pct to flock, returned %d %d\n", msg->rep.retval, msg->rep.errno);

  if (size < 0)
    return size;
  
  errno = msg->rep.errno;
  return msg->rep.retval;
}


static int
fsrv_fstat (S_T(fd_entry) *fd, struct stat *buf)
{
  int size = 0;
  fsrv_fd_t *s;
  ipc_fsrv_t *msg;

  if (fd->type != FD_TYPE_FSRV_FILE)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (fsrv_fd_t*)fd->state;
  msg = (ipc_fsrv_t*)(s->sb.va);
  msg->req.fd = s->fd;

  dprintf("pct to stat\n");
  size = 
    ipc_sleepwait(IPC_FSRV_FSTAT,s->sb.ppn,0,0,0,0L,uinfo->fsrv_envid,1000);
  dprintf("pct to stat returned: %d %d\n",msg->rep.retval, msg->rep.errno);

  if (size < 0)
    return size;

  errno = msg->rep.errno;

  if (msg->rep.retval == 0)
  {
    msg->buf = (void*) (s->sb.va + sizeof(ipc_fsrv_t));
    memmove(buf, msg->buf, sizeof(struct stat));
  }
  
  return msg->rep.retval;
}


static fd_op_t const fsrv_ops = 
{
  fsrv_verify,
  fsrv_incref,

  fsrv_open,
  fsrv_read,
  fsrv_write,
  NULL, /* readv */
  NULL, /* writev */
  fsrv_lseek,
  NULL, /* select */
  NULL, /* select_pred */
  NULL, /* ioctl */
  fsrv_fcntl,
  fsrv_flock, 
  fsrv_close,
  fsrv_close_final,
  NULL, /* dup */
  fsrv_fstat,
  NULL, /* socket */
  NULL, /* bind */
  NULL, /* connect */
  NULL, /* accept */
  NULL, /* listen */
  NULL, /* sendto */
  NULL  /* recvfrom */
};


/* call this to initialize file fd stuff */
void
fsrv_init()
{
  register_fd_ops(FD_TYPE_FSRV_FILE, &fsrv_ops);
}

