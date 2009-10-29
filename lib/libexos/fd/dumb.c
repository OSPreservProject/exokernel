
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

#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>
#define BUFSZ

#include "fd/proc.h"
#include "errno.h"
#include <exos/debug.h>
#include <fcntl.h>
#include <exos/process.h>	/* for yield */
#include <xok/console.h>
#include <exos/vm-layout.h>
#include <exos/vm.h>

#include <unistd.h>
#include <xok/wk.h>

/* this is used for the ioctl TIOCGETA as to fool the stdio that we are a 
 terminal device */
#include <sys/ioctl.h>
#include <termios.h>
#define TTYDEFCHARS
#include <sys/ttydefaults.h>

#define kprintf if (0) kprintf
int
dumb_terminal_init(void);

static int
dumb_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode);

static int
dumb_read(struct file *filp, char *buffer, int nbyte, int blocking);

static int
dumb_write(struct file *filp, char *buffer, int nbyte, int blocking);
  
static int
dumb_select_pred (struct file *, int, struct wk_term *);

static int 
dumb_select(struct file *filp,int sw);

static int 
dumb_close(struct file * filp);

static int
dumb_ioctl(struct file *from, unsigned int request, char *argp);

static int 
dumb_stat(struct file * filp,struct stat *buf);

static struct file_ops const dumb_file_ops = {
    dumb_open,		/* open */
    NULL,			/* lseek */
    dumb_read,		/* read */
    dumb_write,		/* write */
    dumb_select,		/* select */
    dumb_select_pred,	/* select_pred */
    dumb_ioctl,			/* ioclt */
    NULL,			/* close0 */
    dumb_close,		/* close */
    NULL,			/* lookup */
    NULL,			/* link */
    NULL,		        /* symlink */
    NULL,			/* unlink */
    NULL,			/* mkdir */
    NULL,			/* rmdir */
    NULL,			/* mknod */
    NULL,			/* rename */
    NULL,		        /* readlink */
    NULL,			/* follow_link */
    NULL,			/* truncate */
    NULL,			/* dup */
    NULL,		        /* release */
    NULL,		        /* acquire */
    NULL,			/* bind */
    NULL,			/* connect */
    NULL,			/* filepair */
    NULL,			/* accept */
    NULL,			/* getname */
    NULL,			/* listen */
    NULL,			/* sendto */
    NULL, 		        /* recvfrom */
    NULL,			/* shutdown */
    NULL,			/* setsockopt */
    NULL,			/* getsockopt */
    NULL,			/* fcntl */
    NULL,			/* mount */
    NULL,			/* unmount */
    NULL,			/* chmod */
    NULL,			/* chown */
    dumb_stat,		/* stat */
    NULL,			/* readdir */
    NULL,			/* utimes */
    NULL,			/* bmap */
    NULL,			/* fsync */
    NULL			/* exithandler */
};

  
int
dumb_terminal_init(void) {
    DPRINTF(CLUHELP_LEVEL,("dumb_terminal_init"));
    register_file_ops((struct file_ops *)&dumb_file_ops, DUMB_TYPE);
    return 0;
}

static int
dumb_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode) {

  DPRINTF(CLU_LEVEL,("dumb_open: dirp: %08x, name: %s, flags: %d, mode: %d.\n",
		     (int)dirp,name,flags,mode));

  demand(filp, bogus filp);

  filp->f_mode = S_IFREG && 0644;
  filp->f_pos = 0;
  filp->f_dev = DEV_DEVICE;
  filp->f_ino = DUMB_TYPE;
  filp->f_flags = flags;
  filp_refcount_init(filp);
  filp_refcount_inc(filp);
  filp->f_owner = __current->uid;
  filp->op_type = DUMB_TYPE;
  return 0;
}

static int
dumb_read(struct file *filp, char *buffer, int nbyte, int blocking) {
  volatile int *v;
  v = &__sysinfo.si_kbd_nchars;
  if (CHECKNB(filp) && (__sysinfo.si_kbd_nchars == 0)) {
    /* non-blocking */
    errno = EAGAIN;
    return -1;
  } else {
    /* blocking, spin until there are chars */
    while(v == 0) yield(-1);
  }
  return sys_cgets(buffer,nbyte);
}

static int
dumb_write(struct file *filp, char *buffer, int nbyte, int blocking) {
  int len=nbyte;
  int i;
  char tmp_buffer[1024];
  char *tmp_bufferp = (char *) tmp_buffer;

  demand(filp, bogus filp);
  if (nbyte < 0) {return -1;}
  if (nbyte == 0) {return 0;}

  for(i = 0; i < len; i++) {
    if (*buffer) {		/* copy non null chars */
      *tmp_bufferp++ = *buffer;
    }
    buffer++;
    if (tmp_bufferp == &tmp_buffer[1024 - 1]) {
      *tmp_bufferp = (char) 0;
      tmp_bufferp = (char *) &tmp_buffer[0];
      sys_cputs(tmp_bufferp);
    }
  }
  *tmp_bufferp = (char) 0;
  tmp_bufferp = (char *) &tmp_buffer[0];
  sys_cputs(tmp_bufferp);
  return len;
}

static int 
dumb_close(struct file * filp) {
  return 0;
}

static int
dumb_select_pred (struct file *filp, int rw, struct wk_term *t) {
  assert (0);
  return (0);
}
		
static int 
dumb_select(struct file *filp, int sw) {
  /* when we have read we fix this. */
  switch (sw) {
  case SELECT_READ:
    if (__sysinfo.si_kbd_nchars > 0) return 1; else return 0;
  case SELECT_WRITE:
    return 1;
  default:
    return 0;
  }
}
static int 
dumb_ioctl(struct file *filp, unsigned int request, char *argp) {
  static struct termios tm = {
    TTYDEF_IFLAG,
    TTYDEF_OFLAG,
    TTYDEF_LFLAG,
    TTYDEF_CFLAG,
    {0},
    TTYDEF_SPEED,
    TTYDEF_SPEED
  };
  switch(request) {
  case TIOCGETA:
    memcpy(&tm.c_cc,ttydefchars,sizeof(ttydefchars));
    *((struct termios *)argp) = tm;
    return 0;
  default:
    errno = EINVAL;
    return -1;
  }

}

static int 
dumb_stat(struct file *filp, struct stat *buf) {

    DPRINTF(CLU_LEVEL,("pty_stat:\n"));
    demand(filp, bogus filp);
    if (buf == (struct stat *) 0) {errno = EFAULT; return -1;}
    
    buf->st_dev     = DEV_DEVICE;
    buf->st_ino     = DUMB_TYPE;
    buf->st_mode    = 0x2190;
    buf->st_nlink   = 1;
    buf->st_uid     = getuid();
    buf->st_gid     = getgid();
    buf->st_rdev    = 0;
    buf->st_size    = 0;
    buf->st_atime   = 0;
    buf->st_mtime   = 0;
    buf->st_ctime   = 0;
    buf->st_blksize = 0;
    buf->st_blocks  = 0;
    return(0);
}


