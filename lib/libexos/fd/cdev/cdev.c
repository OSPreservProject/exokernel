
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

#include <fcntl.h>
#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <string.h> 
#include <unistd.h>
#include <stdlib.h>

#include "fd/proc.h"
#include "os/procd.h"
#include "opty.h"

#include "errno.h"
#include <exos/debug.h>

#undef	TTYDEFCHARS 
#undef PID_MAX
#include <sys/tty.h>
#include "sw.h"
#include "opty.h"
#include <fd/checkdev.h>

#include <exos/signal.h>

/* print read and write */
#define k0printf if (0) kprintf
#define k1printf if (0) kprintf

/* select */
#define d5printf if (0) kprintf

#define kprintf  if (1) kprintf

#define DEBUG printf("%s/%d\n",__FILE__,__LINE__)
#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

void pr_uio(struct uio *uio);


static int
cdev_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode);

static int
cdev_read(struct file *filp, char *buffer, int nbyte, int blocking);

static int
cdev_write(struct file *filp, char *buffer, int nbyte, int blocking);
  
static int 
cdev_select(struct file *filp,int);

static int
cdev_select_pred (struct file *filp, int, struct wk_term *);

static int 
cdev_ioctl(struct file *from, unsigned int request, char * argp); 

static int 
cdev_lookup(struct file *from, const char *name,struct file *to); 

static int 
cdev_stat(struct file *from, struct stat *buf); 

static int 
cdev_close(struct file * filp);

static struct file_ops const cdev_file_ops = {
    cdev_open,			/* open */
    NULL,			/* lseek */
    cdev_read,			/* read */
    cdev_write,			/* write */
    cdev_select,		/* select */
    cdev_select_pred,		/* select_pred */
    cdev_ioctl,			/* ioclt */
    NULL,			/* close0 */
    cdev_close,			/* close */
    cdev_lookup,			/* lookup */
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
    cdev_stat,			/* stat */
    NULL,			/* readdir */
    NULL,			/* utimes */
    NULL,			/* bmap */
    NULL,			/* fsync */
    NULL			/* exithandler */
};

int
cdev_init(void) {
  k1printf("cdev_init: envid: %d\n",__envid);
  register_file_ops((struct file_ops *)&cdev_file_ops, CDEV_TYPE);
  return 0;
}
/* sets errno  */
static int name2dev(char *name) {
  /* direct device on 2 */
  int majornum, minornum;
  if (!strcmp(name,"tty")) {
    return makedev(2,0);    
  } 
  if ((!strncmp(name, "ptyp",4) || 
       !strncmp(name, "ttyp",4)) &&
      (name[4] >= '0' && name[4] < '0' + NUMBER_PTY)) {
    /* slave (tty) 0, master 1 (pty) */
    majornum = (name[0] == 't') ? 0 : 1;
    minornum = name[4] - '0';
    return makedev(majornum, minornum);
  }
  return -1;
}
static int
cdev_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode) {
  dev_t device;
  int status;
  /* generate dev number from name */
  
  device = name2dev(name);
  if (device == -1) {
    CHECKDEVS(name,filp);

    errno = ENOENT;
    return -1;
  }

  status = (*cdevsw[major(device)].d_open)(device, GENFLAG(filp), 0, curproc);

  if (status == 0) {
    filp->f_dev = device;
    filp->f_mode = S_IFCHR && 0644;
    filp->f_pos = 0;
    filp->f_flags = flags;
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = __current->uid;
    filp->op_type = CDEV_TYPE;
    return 0;
  } else {
    errno = status;
    return -1;
  }
}

static int 
cdev_lookup(struct file *from, const char *name,struct file *to) {
  dev_t device;
  int flags = 0;
  demand(to,bogus filp);

  if ((device = name2dev((char *)name)) != -1) {
    to->f_dev = device;
    to->f_mode = S_IFCHR && 0644;
    to->f_pos = 0;
    to->f_flags = flags;
    filp_refcount_init(to);
    filp_refcount_inc(to);
    to->f_owner = __current->uid;
    to->op_type = CDEV_TYPE;
    return 0;
  }
  if (!strcmp(name,".")) {
    *to = *from;
    filp_refcount_init(to);
    filp_refcount_inc(to);
    return 0;
  }
					
  CHECKDEVS(name,to);

  errno = ENOENT;
  return -1;
}

static int 
cdev_stat(struct file *filp, struct stat *buf) {

    DPRINTF(CLU_LEVEL,("cdev_stat:\n"));
    demand(filp, bogus filp);
    if (buf == (struct stat *) 0) {errno = EFAULT; return -1;}
    
    buf->st_dev     = filp->f_dev;
    buf->st_ino     = CDEV_TYPE;
    buf->st_mode    = 0x2190;
    buf->st_nlink   = 1;
    buf->st_uid     = getuid();
    buf->st_gid     = getgid();
    buf->st_rdev    = CDEV_TYPE;
    buf->st_size    = 0;
    buf->st_atime   = 0;
    buf->st_mtime   = 0;
    buf->st_ctime   = 0;
    buf->st_blksize = CLALLOCSZ;
    buf->st_blocks  = 0;
    return(0);
}

static int
cdev_read(struct file *filp, char *buffer, int nbyte, int blocking) {
  int status;
  dev_t dev = filp->f_dev;
  struct uio uio;
  struct iovec iov[4];

  demand(filp, bogus filp);
  DPRINTF(CLU_LEVEL,
	  ("cdev_read: filp: %08x offset: %qd nbyte: %d\n",
	   (int)filp, filp->f_pos, nbyte));
  /* if (nbyte > CLALLOCSZ) {fprintf(stderr,"ncdev_read, warn large nbyte\n");} */
  iov[0].iov_base = buffer;
  iov[0].iov_len = nbyte;
  uio.uio_iov = iov;
  uio.uio_iovcnt = 1;
  uio.uio_offset = 0;
  uio.uio_resid = nbyte;
  uio.uio_rw = UIO_READ;

  signals_off();
  unlock_filp(filp);
  EnterCritical();
  status = (*cdevsw[major(dev)].d_read)(dev, &uio, GENFLAG(filp));
  ExitCritical();
  signals_on();

  k0printf("Read: %d: ",nbyte - uio.uio_resid);
  if (status == 0) {
    status = nbyte - uio.uio_resid;
  } else {
    errno = status; 
    status = -1;
  }
  /* HBXX - Race condition, can receive a signal during the return and next
     unlock_filp() */
  lock_filp(filp);
#if 0
  if (status >= 0) {
    extern void pr_uio();
    kprintf("read(%d,%d) ",major(dev),minor(dev));
    pr_uio(&uio);
  }
#endif
  return status;
}

static int
cdev_write(struct file *filp, char *buffer, int nbyte, int blocking) {
  int status;
  dev_t dev = filp->f_dev;
  struct uio uio;
  struct iovec iov[4];
  char buf[CLALLOCSZ*8];
  int i;

  demand(filp, bogus filp);

  unlock_filp(filp);
  signals_off();

  DPRINTF(CLU_LEVEL,
	  ("cdev_write: filp: %08x offset: %qd nbyte: %d\n",
	   (int)filp, filp->f_pos, nbyte));

  assert(nbyte <= CLALLOCSZ*8);
  memcpy(buf,buffer,nbyte);
  iov[0].iov_base = buf;
  iov[0].iov_len = nbyte;
  uio.uio_iov = iov;
  uio.uio_iovcnt = 1;
  uio.uio_offset = 0;
  uio.uio_resid = nbyte;
  uio.uio_rw = UIO_WRITE;
  k0printf("Write: %d: ",uio.uio_resid);
  for (i = 0; i < uio.uio_resid; i++) 
    k0printf(">%d (%c)",(unsigned int)buf[i],buf[i]);

  EnterCritical();
  status = (*cdevsw[major(dev)].d_write)(dev, &uio, GENFLAG(filp));
  ExitCritical();

  k0printf("Read: %d: ",nbyte - uio.uio_resid);
  if (status == 0) {
    status = nbyte - uio.uio_resid;
  } else {
    errno = status; status = -1;
  }

  signals_on();
  /* HBXX - Race condition, can receive a signal during the return and next
     unlock_filp() */
  lock_filp(filp);
  return status;
}

static int 
cdev_close(struct file * filp) {
  int status;
  dev_t device = filp->f_dev;
  DPRINTF(CLU_LEVEL,("cdev_close\n"));
  demand(filp, bogus filp);

  status = (*cdevsw[major(device)].d_close)(device, GENFLAG(filp), 0, curproc);

  if (status == 0) {
    return 0;
  } else {
    errno = status; return -1;
  }
}

static int
cdev_select_pred (struct file *filp, int rw, struct wk_term *t) {
  dev_t device = filp->f_dev;
  int error;
  d5printf("%d cdev_select_pred (%d,%d), rw: %d\n",
	   getpid(),major(device),minor(device), rw);
  error = selselect_pred(rw,t);
  d5printf("returns: %d\n",error);
  return error;
}

static int 
cdev_select(struct file *filp,int rw) {
  int which = 0;
  int error;
  dev_t device = filp->f_dev;
  
  d5printf("%d cdev_select (%d,%d), rw: %d\n",
	   getpid(),major(device),minor(device), rw);

  switch(rw) {
  case SELECT_READ: which = FREAD; break;
  case SELECT_WRITE: which = FWRITE; break;
  case SELECT_EXCEPT: which = 0; break;
  default: assert(0);
  }
  signals_off();
  EnterCritical();
  error = (*cdevsw[major(device)].d_select)(device, which, curproc);
  ExitCritical();
  signals_on();
  return error;
}

static int 
cdev_ioctl(struct file *filp, unsigned int request, char *argp) {
  int status;
  dev_t device = filp->f_dev;

  unlock_filp(filp);
  signals_off();

  status = (*cdevsw[major(device)].d_ioctl)(device, request, argp, GENFLAG(filp), curproc);

  if (status == 0) {
#if 1
    if (request == TIOCSCTTY) {
      {int ret = proc_controlt(-1,-1,(int)filp); assert(ret == 0);}
    }
#endif
  } else {
    errno = status; status = -1;
  }
  signals_on();
  /* HBXX - Race condition, can receive a signal during the return and next
     unlock_filp() */
  lock_filp(filp);
  return status;
}


void pr_uio(struct uio *uio) {
  int i;
  struct iovec *iov;
  return;
  k0printf("uio: %p  iov_cnt: %d offset: %d resid: %d rw: %s\n",
	  uio,
	  uio->uio_iovcnt, 
	  (int)uio->uio_offset, 
	  uio->uio_resid, 
	  (uio->uio_rw == UIO_WRITE) ? "uio->k W" :
	  (uio->uio_rw == UIO_READ) ? "k->uio R" : "UNKNOWN");
  iov = &uio->uio_iov[0];
  for (i = 0 ; i < uio->uio_iovcnt ; i++) {
    k0printf("[%d] base: %p len: %d\n",i,
	    iov->iov_base,iov->iov_len);
    iov++;
  }
}
