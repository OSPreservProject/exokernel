#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <xok/env.h>
#include <exos/cap.h>
#include <exos/ipcdemux.h>
#include <exos/vm-layout.h>
#include <exos/vm.h>
#include <exos/process.h>

#include "fsrv.h"

#define dprintf if (0) printf


#define FSRV_START \
  if (arg1 <= 0) \
    return -1; \
  _exos_self_insert_pte(0,(arg1<<PGSHIFT)|PG_U|PG_P|PG_W,USER_TEMP_PGS,0,0); \
  msg = (ipc_fsrv_t*) USER_TEMP_PGS; \
  msg->buf = (void*) (USER_TEMP_PGS+sizeof(ipc_fsrv_t)); 

#define FSRV_END \
  _exos_self_unmap_page(0, USER_TEMP_PGS);

int
fsrv_open(int arg0, int arg1, u_int caller) 
{
  int fd;
  ipc_fsrv_t *msg;

  FSRV_START;

  fd = open(msg->req.name, msg->req._f_flags, msg->req._f_mode);
  dprintf("fsrv: opening %s, %d, %d --> %d, %d\n", 
      msg->req.name, msg->req._f_flags, msg->req._f_mode, fd, errno);
  msg->rep.retval = fd;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}


int
fsrv_read(int arg0, int arg1, u_int caller) 
{
  ipc_fsrv_t *msg;
  int fd, size, retval;
  
  FSRV_START;

  fd = msg->req.fd;
  size = msg->req._f_size;
  
  dprintf("fsrv: reading %d\n", fd);
  retval = read(fd, msg->buf, size);

  msg->rep.retval = retval;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}


int
fsrv_write(int arg0, int arg1, u_int caller) 
{
  ipc_fsrv_t *msg;
  int fd, size, retval;
  
  FSRV_START;

  fd = msg->req.fd;
  size = msg->req._f_size;
  
  dprintf("fsrv: writing %d, size %d\n", fd, size);
  retval = write(fd, msg->buf, size);

  msg->rep.retval = retval;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}


int
fsrv_lseek(int arg0, int arg1, u_int caller) 
{
  ipc_fsrv_t *msg;
  int fd, whence, retval;
  off_t offset;
  
  FSRV_START;
  fd = msg->req.fd;
  offset = msg->req._f_offset;
  whence = msg->req._f_whence;

  dprintf("fsrv: lseek on %d\n", fd);
  retval = lseek(fd, offset, whence);
  
  msg->rep.retval = retval;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}


int
fsrv_fcntl(int arg0, int arg1, u_int caller) 
{
  ipc_fsrv_t *msg;
  int fd, cmd, retval;
  long arg;
  
  FSRV_START;
  fd = msg->req.fd;
  cmd = msg->req._f_cmd;
  arg = msg->req._f_arg;

  dprintf("fsrv: fcntl on %d\n", fd);
  retval = fcntl(fd, cmd, arg);
  
  msg->rep.retval = retval;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}


int
fsrv_flock(int arg0, int arg1, u_int caller) 
{
  ipc_fsrv_t *msg;
  int fd, op, retval;
  
  FSRV_START;
  fd = msg->req.fd;
  op = msg->req._f_cmd;

  dprintf("fsrv: flock on %d\n", fd);
  retval = flock(fd, op);
  
  msg->rep.retval = retval;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}


int
fsrv_fstat(int arg0, int arg1, u_int caller) 
{
  ipc_fsrv_t *msg;
  int fd, retval;
  
  FSRV_START;
  fd = msg->req.fd;

  dprintf("fsrv: fstating %d\n", fd);
  retval = fstat(fd, (struct stat *)msg->buf);
  dprintf("blksize is %d \n", ((struct stat*)(msg->buf))->st_blksize);

  msg->rep.retval = retval;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}


int
fsrv_close(int arg0, int arg1, u_int caller) 
{
  ipc_fsrv_t *msg;
  int fd, retval;
  
  FSRV_START;
  fd = msg->req.fd;

  dprintf("fsrv: closing %d\n", fd);
  retval = close(fd);

  msg->rep.retval = retval;
  msg->rep.errno = errno;

  FSRV_END;
  return 0;
}



int 
main(void) 
{
  extern int write_uinfo();

  if (write_uinfo() < 0)
    printf("fsrv: warning: cannot map uinfo\n");

  ipcdemux_register(IPC_FSRV_OPEN,  fsrv_open, 3);
  ipcdemux_register(IPC_FSRV_READ,  fsrv_read, 3);
  ipcdemux_register(IPC_FSRV_WRITE, fsrv_write, 3);
  ipcdemux_register(IPC_FSRV_LSEEK, fsrv_lseek, 3);
  ipcdemux_register(IPC_FSRV_FSTAT, fsrv_fstat, 3);
  ipcdemux_register(IPC_FSRV_FCNTL, fsrv_fcntl, 3);
  ipcdemux_register(IPC_FSRV_FLOCK, fsrv_flock, 3);
  ipcdemux_register(IPC_FSRV_CLOSE, fsrv_close, 3);

  while(1)
  {
    UAREA.u_status = U_SLEEP;
    yield(-1);
  }
}

