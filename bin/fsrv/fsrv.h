
#ifndef __FSRV_H__
#define __FSRV_H__

#define IPC_FSRV_OPEN    128
#define IPC_FSRV_READ	 129
#define IPC_FSRV_WRITE	 130
#define IPC_FSRV_LSEEK	 131
#define IPC_FSRV_FSTAT   132
#define IPC_FSRV_FCNTL   133
#define IPC_FSRV_FLOCK   134
#define IPC_FSRV_CLOSE	 135



typedef struct ipc_fsrv
{
  union
  {
    struct {
      int fd;
      char name[256];
#if 0
      unsigned int size;
      int flags;
      mode_t mode;
      off_t offset;
      int whence;
#else
      int a0;
      int a1;
#define _f_size		a0 	/* read and write ops */

#define _f_offset 	a0	/* lseek */
#define _f_whence 	a1

#define _f_flags 	a0	/* open */
#define _f_mode		a1

#define _f_cmd		a0	/* fcntl and flock */
#define _f_arg		a1
#endif
    } _req;

    struct {
      int  retval;
      int  errno;
    } _rep;
  } _m;
#define req _m._req
#define rep _m._rep

  void *buf;
} ipc_fsrv_t;



#endif

