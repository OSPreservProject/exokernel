
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

#define k0printf if (0) kprintf
#define k1printf if (0) kprintf
/* 
#define PTYDEBUG
 */
/* #define	TTYDEFCHARS  */
#include <fcntl.h>
#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <string.h> 
#include <unistd.h>
#include <stdlib.h>

#include "fd/proc.h"
#include "os/procd.h"
#include "npty.h"

#include "errno.h"
#include <exos/debug.h>

#include "exos/process.h"
#include "exos/vm-layout.h"
#include "exos/locks.h"
#undef	TTYDEFCHARS 
#undef PID_MAX
#include <sys/tty.h>
#include "fd/cdev/sw.h"

#include <xok/wk.h>
#include "npty_other.h"

#include <fd/checkdev.h>

#include <xuser.h>

#include <xok/sysinfo.h>

#define DEBUG printf("%s/%d\n",__FILE__,__LINE__)
#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

void pr_uio(struct uio *uio);
extern void pt_softc_init(int);
extern void npty_tty_init(void);
extern void npty_sched_init(int);
struct npty_shared_data *const npty_shared_data = (struct npty_shared_data *) NPTY_SHARED_REGION;;
static inline int check_name(const char *name,int *master, int *number);

#define GENFLAG(filp) ((CHECKNB(filp)) ? IO_NDELAY : 0)

int
npty_init(void) {

    int status,i;

    DPRINTF(CLUHELP_LEVEL,("pty_init"));

    status = fd_shm_alloc(FD_SHM_OFFSET + NPTY_TYPE + 990,
			  (sizeof(struct npty_shared_data)),
			  (char *)NPTY_SHARED_REGION);
    demand(npty_shared_data != 0, pty_shared_data getting bad pointer);
    
    StaticAssert((sizeof(struct npty_shared_data)) <= NPTY_SHARED_REGION_SZ);


    if (status == -1) {
	demand(0, problems attaching shm);
	return -1;
    }
    if (status) {
k1printf("npty_init first %d\n",NPTY_TYPE);
      bzero(npty_shared_data,(sizeof(struct npty_shared_data)));
      npty_tty_init();			/* initialize tty, clists */
      pt_softc_init(1);
      init_npty_shared_lock();
      for (i = 0; i < NPTY; i++) {
	init_lock_pty(i);
	npty_shared_data->refcount[0][i] = 0;
	npty_shared_data->refcount[1][i] = 0;

      }
      //npty_sched_init(1);
    } else {
k1printf("npty_init second+ %d\n",NPTY_TYPE);
      //npty_sched_init(0);
      pt_softc_init(0);
    }
    k1printf("pty_init: envid: %d\n",__envid);
    register_file_ops((struct file_ops *)&pty_file_ops, NPTY_TYPE);

    return 0;
}




static int
pty_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode) {

    int master;
    int pty_number;
    int status = 0;
    
    DPRINTF(CLU_LEVEL,("pty_open: dirp: %08x, name: %s, flags: %d, mode: %d.\n",
		       (int)dirp,name,flags,mode));

    demand(filp, bogus filp);
    CHECKDEVS(name,filp);


    if (check_name(name,&master,&pty_number) == -1) {
      return -1;
    }


#if 0
    if (check_lock_pty(pty_number)) {errno = EBUSY; return -1;}
#endif
    filp->f_dev = makedev(master,pty_number);
    filp->f_mode = S_IFCHR && 0644;
    filp->f_pos = master;
    filp->f_flags = flags;
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = __current->uid;
    filp->op_type = NPTY_TYPE;

    lock_pty(pty_number);
    if (master) {
      status = ptcopen(filp->f_dev,0,0,0);
      k0printf("ptcopen returned %d\n",status);
    } else {
      status = ptsopen(filp->f_dev,0,0,0);
      k0printf("ptsopen returned %d\n",status);
    }
    unlock_pty(pty_number);
    if (status == 0) {
      npty_shared_data->refcount[master][pty_number]++;
      return 0;
    } else {
      errno = status; return -1;
    }

    return 0;
}
static int 
pty_lookup(struct file *from, const char *name,struct file *to) {
  int master,pty_number;
  int flags = 0;
  demand(to,bogus filp);

  CHECKDEVS(name,to);

  if (!strcmp(name,".")) {
    *to = *from;
    filp_refcount_init(to);
    filp_refcount_inc(to);
    return 0;
  }

  if (check_name(name, &master,&pty_number) == -1) return -1;

  to->f_dev = makedev(master,pty_number);
  to->f_mode = S_IFCHR && 0644;
  to->f_pos = master;
  to->f_flags = 0;
  filp_refcount_init(to);
  filp_refcount_inc(to);
  to->f_owner = __current->uid;
  to->op_type = NPTY_TYPE;
  return 0;
}

static int 
pty_stat(struct file *filp, struct stat *buf) {

    DPRINTF(CLU_LEVEL,("pty_stat:\n"));
    demand(filp, bogus filp);
    if (buf == (struct stat *) 0) {errno = EFAULT; return -1;}
    
    buf->st_dev     = filp->f_dev;
    buf->st_ino     = NPTY_TYPE;
    buf->st_mode    = 0x2190;
    buf->st_nlink   = 1;
    buf->st_uid     = getuid();
    buf->st_gid     = getgid();
    buf->st_rdev    = NPTY_TYPE;
    buf->st_size    = 0;
    buf->st_atime   = 0;
    buf->st_mtime   = 0;
    buf->st_ctime   = 0;
    buf->st_blksize = CLALLOCSZ;
    buf->st_blocks  = 0;
    return(0);
}

static int
pty_read(struct file *filp, char *buffer, int nbyte, int blocking) {
  int status;
  int master;
  struct uio uio;
  struct iovec iov[4];

  demand(filp, bogus filp);
  DPRINTF(CLU_LEVEL,
	  ("pty_read: filp: %08x offset: %qd nbyte: %d\n",
	   (int)filp, filp->f_pos, nbyte));
  master = filp->f_pos;
  if (nbyte > CLALLOCSZ) {fprintf(stderr,"npty_read, warn large nbyte\n");}
  iov[0].iov_base = buffer;
  iov[0].iov_len = nbyte;
  uio.uio_iov = iov;
  uio.uio_iovcnt = 1;
  uio.uio_offset = 0;
  uio.uio_resid = nbyte;
  uio.uio_rw = UIO_READ;

  if (master) {

    k0printf("ptcread %d...\n",nbyte);
    pr_uio(&uio);

    k0printf("read: locking master...\n");
    lock_pty(minor(filp->f_dev));
    k0printf("locked\n");
    status = ptcread(filp->f_dev,&uio,GENFLAG(filp));
    unlock_pty(minor(filp->f_dev));
    k0printf("ptcread returned %d\n",status);
    pr_uio(&uio);

  } else {

    k0printf("ptsread %d...\n",nbyte);
    pr_uio(&uio);
    k0printf("read: locking slave...\n");
    lock_pty(minor(filp->f_dev));
    k0printf("locked\n");
    status = ptsread(filp->f_dev,&uio,GENFLAG(filp));
    unlock_pty(minor(filp->f_dev));
    k0printf("ptsread returned %d\n",status);
    pr_uio(&uio);

  }

  k0printf("Read: %d: ",nbyte - uio.uio_resid);
  if (status == 0) {
    return nbyte - uio.uio_resid;
  } else {
    errno = status; return -1;
  }
}

static int
pty_write(struct file *filp, char *buffer, int nbyte, int blocking) {
  int master,status;
  struct uio uio;
  struct iovec iov[4];
  char buf[CLALLOCSZ*8];
  int i;

  demand(filp, bogus filp);
  DPRINTF(CLU_LEVEL,
	  ("pty_write: filp: %08x offset: %qd nbyte: %d\n",
	   (int)filp, filp->f_pos, nbyte));
  master = filp->f_pos;

  demand (master == 1 || master == 0,master flag set improperly);
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
    k0printf("-%d (%c)",(unsigned int)buf[i],buf[i]);

  if (master) {
    k0printf("write: locking %d...\n",minor(filp->f_dev));
    lock_pty(minor(filp->f_dev));
    k0printf("write: locking done\n");
    k0printf("ptcwrite :%d...\n",nbyte);
    pr_uio(&uio);
    status = ptcwrite(filp->f_dev,&uio,GENFLAG(filp));
    k0printf("ptcwrite returned %d, wrote: %d\n",status,nbyte - uio.uio_resid);
    pr_uio(&uio);
  } else {
    k0printf("write: locking %d...\n",minor(filp->f_dev));
    lock_pty(minor(filp->f_dev));
    k0printf("write: locking done\n");
    k0printf("ptswrite %d...\n",nbyte);
    pr_uio(&uio);
    status = ptswrite(filp->f_dev,&uio,GENFLAG(filp));
    k0printf("ptswrite returned %d wrote: %d\n",status,nbyte - uio.uio_resid);
    pr_uio(&uio);
  }
  k0printf("write: unlocking %d...\n",minor(filp->f_dev));
  unlock_pty(minor(filp->f_dev));
  k0printf("write: unlocking done\n");

  if (status == 0) {
    return nbyte - uio.uio_resid;
  } else {
    errno = status; return -1;
  }
}

static int 
pty_close(struct file * filp) {
  int master;
  int status;
  int ptynumber;
  DPRINTF(CLU_LEVEL,("pty_close\n"));
  demand(filp, bogus filp);

  master = filp->f_pos;
  ptynumber = minor(filp->f_dev);
  lock_pty(ptynumber);

  if (master) {
    if (--npty_shared_data->refcount[1][ptynumber] == 0) {
      status = ptcclose(filp->f_dev,GENFLAG(filp),0,0);
    } else {status = 0;}
    k0printf("ptcclose returned %d\n",status);
  } else {
    if (--npty_shared_data->refcount[0][ptynumber] == 0) {
      status = ptsclose(filp->f_dev,GENFLAG(filp),0,0);
    } else {status = 0;}
    k0printf("ptsclose returned %d\n",status);
  }
  unlock_pty(minor(filp->f_dev));

  if (status == 0) {
    return 0;
  } else {
    errno = status; return -1;
  }
}

/* poll the npty code every 50 msec */
static int
pty_select_pred (struct file *filp, int rw, struct wk_term *t) {
  if (rw == SELECT_EXCEPT) return 0;
  return wk_mksleep_pred (t, (50000/__sysinfo.si_rate)
			  + __sysinfo.si_system_ticks);
}

static int 
pty_select(struct file *filp,int rw) {
  int output = 0;
  if (rw == SELECT_EXCEPT) return 0;
  lock_pty(minor(filp->f_dev));
  k1printf("pty_select: %d\n",__envid);
  if (filp->f_pos) {
    switch(rw) {
    case SELECT_READ: 
      output = (ptcselect(filp->f_dev,SELECT_READ,0));
      break;
    case SELECT_WRITE: 
      output = (ptcselect(filp->f_dev,SELECT_WRITE,0));
      break;
    default: 
      output = (ptcselect(filp->f_dev,SELECT_EXCEPT,0));
      break;
    }
  } else {
    switch(rw) {
    case SELECT_READ: 
      output = (ttselect(filp->f_dev,SELECT_READ,0));
      break;
    case SELECT_WRITE: 
      output = (ttselect(filp->f_dev,SELECT_WRITE,0));
      break;
    default: 
      output = (ttselect(filp->f_dev,SELECT_EXCEPT,0));
      break;
    }
  }
  unlock_pty(minor(filp->f_dev));
  return output;

}

static int 
pty_ioctl(struct file *filp, unsigned int request, char *argp) {
  int status;
  lock_pty(minor(filp->f_dev));
  status = ptyioctl(filp->f_dev,(u_long)request,argp,GENFLAG(filp),0);
  unlock_pty(minor(filp->f_dev));
  k0printf("ptyioctl returned: %d\n",status);

  if (status == 0) {
    return 0;
  } else {
    errno = status; return -1;
  }
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



static inline int check_name(const char *name,int *master, int *pty_number) {

  if (strlen(name) != 5 || (strncmp(name,"ptyp",4) != 0 && 
			    strncmp(name,"ttyp",4) != 0)) {
    k0printf("*did not match name [pt]ty[a-?] "
	     "%d %d %d %d %s\n",strlen(name),name[0],name[1],name[2],name);
    errno = ENOENT;
    return -1;
  }
  *master = (name[0] == 'p') ? 1 : 0;

  if (name[4] >= '0' && name[4] < '0' + NPTY) {
    *pty_number = name[4] - '0';
#ifdef PTYDEBUG
    printf("pty number: %d\n",pty_number);
#endif
  } else {
    kprintf("did not match range");
    errno = ENOENT;
    return -1;
  }
  return 0;
}
