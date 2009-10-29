
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

#define TTYDEFCHARS
/* 
#define PTYDEBUG
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h> 
#include <unistd.h>
#include <stdlib.h>

#include "fd/proc.h"
#include "xok/wk.h"
#include "exos/uwk.h"

#include "errno.h"
#include <exos/debug.h>

#include "exos/process.h"
#include "exos/vm-layout.h"
#include "exos/locks.h"

#include "fd/checkdev.h"
#include <termios.h>

#define PTY_BUFFER_SIZE 512
#define NR_PTY 16


static inline dev_t makeptydev(master,pty_number) {
  if (master) return makedev(PTY_MAJOR_MASTER,pty_number);
  return makedev(PTY_MAJOR_SLAVE,pty_number);

}

static struct pty_shared_data {
    exos_lock_t lock;
    struct pty {
	exos_lock_t lock;
	int inuse[2];		/* inuse[1] = master, inuse[0] = slave */
	struct pipe {
	    char buffer[PTY_BUFFER_SIZE];
	    int start;
	    int length;
	    exos_lock_t lock;
	} pipe[2];
	int timer;
      pid_t pgrp;		/* pgrp of this pty. */
      struct termios tm;
    } pty[NR_PTY];
} *pty_shared_data;

struct pty_data {
    struct pty *ptyp;
};

#define PTY_BUSY(master,ptyp) \
(ptyp->inuse[(master)] == 1)

#define SET_PTY_INUSE(master,ptyp) ptyp->inuse[(master)] = 1

#define CLR_PTY_INUSE(master,ptyp) \
(ptyp->inuse[(master)] = 0)

#define GETPTYPP(filp) (struct pty_data *)(filp)->data


#define GETPTYP(filp)  ((struct pty_data *)((filp)->data))->ptyp


#define MAKEPTYP(ptynum) (&(pty_shared_data->pty[ptynum]))


int
pty_init(void);

static int
pty_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode);

static int
pty_read(struct file *filp, char *buffer, int nbyte, int blocking);

static int
pty_write(struct file *filp, char *buffer, int nbyte, int blocking);
  
static int 
pty_select(struct file *filp,int);

static int 
pty_select_pred(struct file *filp,int,struct wk_term *);

static int 
pty_ioctl(struct file *from, unsigned int request, char * argp); 

static int 
pty_lookup(struct file *from, const char *name,struct file *to);

static int 
pty_stat(struct file *from, struct stat *buf); 

static int 
pty_close(struct file * filp);


#define DEBUG printf("%s/%d\n",__FILE__,__LINE__)
#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

static struct file_ops const pty_file_ops = {
    pty_open,			/* open */
    NULL,			/* lseek */
    pty_read,			/* read */
    pty_write,			/* write */
    pty_select,			/* select */
    pty_select_pred,		/* select_pred */
    pty_ioctl,			/* ioclt */
    NULL,			/* close0 */
    pty_close,			/* close */
    pty_lookup,			/* lookup */
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
    pty_stat,			/* stat */
    NULL,			/* readdir */
    NULL,			/* utimes */
    NULL,			/* bmap */
    NULL,			/* fsync */
    NULL			/* exithandler */
};

static inline int check_name(const char *name,int *master, int *pty_number) {
     if (strlen(name) != 4 || name[1] != 't' || name[2] != 'y' ||
	(name[0] != 't' && name[0] != 'p')) {
	printf("*did not match name pty[a-?] or tty[a-?] "
	       "%d %d %d %d %s\n",strlen(name),name[0],name[1],name[2],name);
	errno = ENOENT;
	return -1;
    }
  *master = (name[0] == 'p') ? 1 : 0;

    if (name[3] >= 'a' && name[3] < 'a' + NR_PTY) {
      *pty_number = name[3] - 'a';
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

static inline void 
init_pty_shared_lock(void) { 
    DPRINTF(LOCK_LEVEL,("init_pty_shared_lock\n"));
    exos_lock_init(&(pty_shared_data->lock));
}
static inline void 
lock_pty_shared_data(void) { 
    DPRINTF(LOCK_LEVEL,("lock_pty_shared_data\n"));
    exos_lock_get_nb(&(pty_shared_data->lock));
}
static inline void
unlock_pty_shared_data(void) { 
    DPRINTF(LOCK_LEVEL,("unlock_pty_shared_data\n"));
    exos_lock_release(&(pty_shared_data->lock)); 
}



static inline void 
init_lock_pty(struct pty *pty) { 
    DPRINTF(LOCK_LEVEL,("init_lock_pty\n"));
    exos_lock_init(&(pty->lock));
}
static inline void 
lock_pty(struct pty *pty) { 
    DPRINTF(LOCK_LEVEL,("lock_pty\n"));
    exos_lock_get_nb(&(pty->lock));
}
static inline void
unlock_pty(struct pty *pty) { 
    DPRINTF(LOCK_LEVEL,("unlock_pty\n"));
    exos_lock_release(&(pty->lock));
}



static inline void 
init_lock_pipepty(struct pipe *pipeb) { 
    DPRINTF(LOCK_LEVEL,("init_lock_pipepty\n"));
    exos_lock_init(&(pipeb->lock));
}
static void 
lock_pipepty(struct pipe *pipeb) { 
    DPRINTF(LOCK_LEVEL,("lock_pipepty\n"));
    exos_lock_get_nb(&(pipeb->lock));
}
static inline void
unlock_pipepty(struct pipe *pipeb) { 
    DPRINTF(LOCK_LEVEL,("unlock_pipepty\n"));
    exos_lock_release(&(pipeb->lock));
}





int
pty_init(void) {
    int i;
    int status;
    struct pty *ptyp;
#ifdef AEGIS
    tmp_putchr = putchr;
    putchr = ae_putc;
#endif /* AEGIS */

    DPRINTF(CLUHELP_LEVEL,("pty_init"));

    status = fd_shm_alloc(FD_SHM_OFFSET + PTY_TYPE,
			   (sizeof(struct pty_shared_data)),
			   (char *)PTY_SHARED_REGION);

    StaticAssert((sizeof(struct pty_shared_data)) <= PTY_SHARED_REGION_SZ);
    pty_shared_data = (struct pty_shared_data *) PTY_SHARED_REGION;


    demand(pty_shared_data, pty_shared_data getting bad pointer);

    if (status == -1) {
	demand(0, problems attaching shm);
	return -1;
    }

    if (status) {
#ifdef AEGIS
	ae_putc('P');
	ae_putc('I');
#endif /* AEGIS */
	init_pty_shared_lock();
	for (i = 0; i < NR_PTY; i++) {
	    ptyp = MAKEPTYP(i);
	    init_lock_pty(ptyp);
	    CLR_PTY_INUSE(0,ptyp);
	    CLR_PTY_INUSE(1,ptyp);
	}
    } else {
	/*printf("This is not first process, just attaching memory\n");  */
    }
    register_file_ops((struct file_ops *)&pty_file_ops, PTY_TYPE);
#ifdef AEGIS
    putchr = tmp_putchr;
#endif /* AEGIS */


    
    return 0;
}




static int 
pipewrite(struct pipe *pipeb, char *buffer,int nbyte) {
    int to_write,to_write1;
    int total;
    int initial_length = pipeb->length;
    int initial_start = pipeb->start;
    /*     printf("pipewrite: wrinting %s, %d bytes\n",buffer,nbyte); */
    to_write = total = MIN(nbyte, pipeb->length);
    if ((pipeb->start + to_write) > PTY_BUFFER_SIZE) {
	/* copy in two steps */
	to_write1 = PTY_BUFFER_SIZE - pipeb->start;
	to_write -= to_write1;
	memcpy(&pipeb->buffer[pipeb->start],buffer,to_write1);
	buffer += to_write1;
	pipeb->start = (pipeb->start + to_write1) % PTY_BUFFER_SIZE;
	if (pipeb->start != 0) 
	    printf("pipewrite (pid %d): hmm, start should be 0\n"
		   "initial st: %d len: %d\n"
		   "current st: %d len: %d\n"
		   "nbyte: %d towrite: %d towrite1: %d",getpid(),
		   initial_start,initial_length,
		   pipeb->start, pipeb->length,
		   nbyte,to_write,to_write1);
	pipeb->length -= to_write1;
    }


    /* copy in one step */
    memcpy(&pipeb->buffer[pipeb->start],buffer,to_write);
    pipeb->start = (pipeb->start + to_write) % PTY_BUFFER_SIZE;
    pipeb->length -= to_write;
    return total;
}

#define GRL(start,length) ((start + length) % PTY_BUFFER_SIZE)

static int 
piperead(struct pipe *pipeb, char *buffer,int nbyte,int linedis,int *foundnl) {
    int to_read = 0,to_read1 = 0;
    int total;
    int c;
    int newline_flag = 0;
    int initial_length = pipeb->length;
    int initial_start = pipeb->start;
    
				/*     printf("piperead_line: reading %d bytes\n",nbyte); */
    to_read = total = MIN(nbyte, PTY_BUFFER_SIZE - pipeb->length);

    if ((GRL(pipeb->start,pipeb->length) + to_read) > PTY_BUFFER_SIZE) {
	/* copy in two steps because it wraps around */
	to_read1 = PTY_BUFFER_SIZE - GRL(pipeb->start,pipeb->length);
	to_read -= to_read1;

	if (linedis) {
	    for (c = 0; c < to_read1; c++) {
		if (pipeb->buffer[GRL(pipeb->start + c,pipeb->length)] == '\n')
		{
		    to_read1 = c + 1;
		    newline_flag = 1;
		    break;
		}
	    }
	}

	memcpy(buffer,&pipeb->buffer[GRL(pipeb->start,pipeb->length)],
	       to_read1);
	buffer += to_read1;
	       
	pipeb->length = (pipeb->length + to_read1);
	if (GRL(pipeb->length,pipeb->start) != 0)
	    printf("piperead (pid %d): hmm, start should be 0\n"
		   "initial st: %d len: %d\n"
		   "current st: %d len: %d\n"
		   "nbyte: %d toread: %d toread1: %d",getpid(),
		   initial_start,initial_length,
		   pipeb->start, pipeb->length,
		   nbyte,to_read,to_read1);
	if (newline_flag) {
	  *foundnl = newline_flag;
	  return to_read1;
	}
    }


    /* copy in one step */
    if (linedis) {
	for (c = 0; c < to_read; c++) {
	    if (pipeb->buffer[GRL(pipeb->start + c,pipeb->length)] == '\n') {
		to_read = c + 1;
		newline_flag = 1;
		break;
	    }
	}
    }
    
    memcpy(buffer,&pipeb->buffer[GRL(pipeb->start,pipeb->length)],to_read);
    pipeb->length = (pipeb->length + to_read);

    *foundnl = newline_flag;
    return (to_read + to_read1);
}



#ifdef PTYDEBUG
static void
pr_ptyp(struct pty *ptyp) {
    struct pipe *pipeb;

    printf("lck:%d ",ptyp->lock);
    printf("inuse m:%d s:%d ",ptyp->inuse[1],
	   ptyp->inuse[0]);
    pipeb = &(ptyp->pipe[0]);
    printf("pipe[s]-%d  %3d:%3d ",pipeb->lock,pipeb->start, pipeb->length);
    pipeb = &(ptyp->pipe[1]);
    printf("pipe[m]-%d  %3d:%3d\n",pipeb->lock,pipeb->start, pipeb->length);
}
#endif

static int
pty_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode) {

    int master;
    int pty_number;
    struct pty *ptyp,*newptyp;
    struct pipe *pipeb;
    
    DPRINTF(CLU_LEVEL,("pty_open: dirp: %08x, name: %s, flags: %d, mode: %d.\n",
		       (int)dirp,name,flags,mode));

    demand(filp, bogus filp);

    CHECKDEVS(name,filp);


    if (strlen(name) != 4 || name[1] != 't' || name[2] != 'y' ||
	(name[0] != 't' && name[0] != 'p')) {
	printf("*did not match name pty[a-?] or tty[a-?] "
	       "%d %d %d %d %s\n",strlen(name),name[0],name[1],name[2],name);
	errno = ENOENT;
	return -1;
    }
    master = (name[0] == 'p') ? 1 : 0;
#ifdef PTYDEBUG
    printf("this is a %s pty\n",master ? "master" : "slave");
#endif

    if (name[3] >= 'a' && name[3] < 'a' + NR_PTY) {
	pty_number = name[3] - 'a';
#ifdef PTYDEBUG
    printf("pty number: %d\n",pty_number);
#endif
    } else {
	printf("did not match range");
	errno = ENOENT;
	return -1;
    }

    newptyp = MAKEPTYP(pty_number);
    lock_pty(newptyp);
#ifdef PTYDEBUG
    printf("lock finished, newptyp: 0x%08x\n",(int)newptyp);
    pr_ptyp(newptyp);
#endif
    if (PTY_BUSY(master,newptyp)) {
#ifdef PTYDEBUG
	printf("pty is busy\n");
#endif
	unlock_pty(newptyp);
	errno = EACCES;
	return -1;
    }

    filp->f_dev = makeptydev(master,pty_number);
    filp->f_ino = PTY_TYPE;
    filp->f_mode = S_IFREG && 0644;
    filp->f_pos = master;
    filp->f_flags = flags;
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = __current->uid;
    filp->op_type = PTY_TYPE;

    GETPTYP(filp) = newptyp;
    /* first one to open a pty initializes both streams */
    if (!(PTY_BUSY(0,newptyp)) && !(PTY_BUSY(1,newptyp))) {
#ifdef PTYDEBUG
	printf("Initializing pty %s\n",name);
#endif
	ptyp = newptyp;
	pipeb = &(ptyp->pipe[master]);
	pipeb->start = 0;
	pipeb->length = PTY_BUFFER_SIZE;
	init_lock_pipepty(pipeb);
#ifdef PTYDEBUG
	printf("%d pipeb %08x\n",master,(int)pipeb);
#endif
	pipeb = &(ptyp->pipe[1 - master]);
	pipeb->start = 0;
	pipeb->length = PTY_BUFFER_SIZE;
	init_lock_pipepty(pipeb);
#ifdef PTYDEBUG
	printf("%d pipeb %08x\n",1 - master,(int)pipeb);
	pr_ptyp(newptyp);
#endif
    }
    SET_PTY_INUSE(master,newptyp);
    newptyp->pgrp = 0;

    newptyp->tm.c_iflag = TTYDEF_IFLAG;        /* input flags */
    newptyp->tm.c_oflag = TTYDEF_OFLAG;        /* output flags */
    newptyp->tm.c_cflag = TTYDEF_CFLAG;        /* control flags */
    newptyp->tm.c_lflag = TTYDEF_LFLAG;        /* local flags */
    memcpy((char *)&newptyp->tm.c_cc,
	   (char *)ttydefchars,
	   sizeof ttydefchars);     /* control chars */
    newptyp->tm.c_ispeed = TTYDEF_SPEED;       /* input speed */
    newptyp->tm.c_ospeed = TTYDEF_SPEED;       /* output speed */
#ifdef PTYDEBUG
    pr_ptyp(newptyp);
#endif
    unlock_pty(newptyp);
#ifdef PTYDEBUG
    pr_ptyp(newptyp);
    printf("opening pty: %s, RETURNED OK\n",name);
#endif
    return 0;
}

static int
pty_read(struct file *filp, char *buffer, int nbyte, int blocking) {
  struct pty *pty;
  struct pipe *pipeb;
#if 0
  int  final = 0,len;
#else
  int total;
#endif
  int foundnl,master;

  demand(filp, bogus filp);
  DPRINTF(CLU_LEVEL,
	  ("pty_read: filp: %08x offset: %qd nbyte: %d\n",
	   (int)filp, filp->f_pos, nbyte));
  pty = GETPTYP(filp);
				/*   lock_pty(pty); */
  master = filp->f_pos;
  demand (master == 1 || master == 0,master flag set improperly);

  pipeb = &(pty->pipe[master]);

  if (!(PTY_BUSY(1 - master,pty)) && pipeb->length == PTY_BUFFER_SIZE) {
      /* someone closed the other end, and nothing to read */
      return 0;
  }

  if (pipeb->length == PTY_BUFFER_SIZE && CHECKNB(filp)) {
    errno = EWOULDBLOCK;
    return -1;
  }
    

  while(pipeb->length == PTY_BUFFER_SIZE) {
      /* we block if cant read anything */
      unlock_filp(filp);
      wk_waitfor_value_lt (&pipeb->length, PTY_BUFFER_SIZE, 0);
#if 0
      yield(-1);
#endif
      lock_filp(filp);
  }

  if (!(PTY_BUSY(1 - master,pty)) && pipeb->length == PTY_BUFFER_SIZE) {
      /* someone closed the other end, and nothing to read */
      return 0;
  }
  
  lock_pipepty(pipeb); 

  if (master) {
      /* master pty does not have line discipline */
      total = piperead(pipeb,buffer,nbyte,0,&foundnl);
  } else {
      /* slave pty have line discipline */
      total = piperead(pipeb,buffer,nbyte,1,&foundnl);
  }
  unlock_pipepty(pipeb); 
				/*   unlock_pty(pty); */
				/*   if (master) pr_ptyp(pty);   */
  return total;
}
static int
pty_write(struct file *filp, char *buffer, int nbyte, int blocking) {
  struct pty *pty;
  struct pipe *pipeb;
  int master,total = 0, final = 0;
  demand(filp, bogus filp);
  DPRINTF(CLU_LEVEL,
	  ("pty_write: filp: %08x offset: %qd nbyte: %d\n",
	   (int)filp, filp->f_pos, nbyte));
  pty = GETPTYP(filp);
  master = filp->f_pos;

  demand (master == 1 || master == 0,master flag set improperly);

  pipeb = &(pty->pipe[1 - master]);

  if (pipeb->length == 0 && CHECKNB(filp)) {
    errno = EWOULDBLOCK;
    return -1;
  }

  while(nbyte > 0) {
    if (!(PTY_BUSY(1 - master,pty))) {
      /* signal(getpid(),SIGPIPE); */
      return -1;
    }
    if (pipeb->length != 0) {
      int len;
      len = MIN(nbyte,pipeb->length);

      lock_pipepty(pipeb); 
      total = pipewrite(pipeb,buffer,len);
      final += total;
      buffer += total;
      unlock_pipepty(pipeb); 
      nbyte -= total;
    } else {
      unlock_filp(filp);
      wk_waitfor_value_gt (&pipeb->length, 0, 0);
#if 0
      yield(-1);
#endif
      lock_filp(filp);
    }
  }
  return final;

}

static int 
pty_close(struct file * filp) {
  struct pty *ptyp;
  int master;
  DPRINTF(CLU_LEVEL,("pty_close\n"));
  demand(filp, bogus filp);
  ptyp = GETPTYP(filp);
  master = filp->f_pos;
  lock_pty(ptyp);  

  CLR_PTY_INUSE(master,ptyp);
  unlock_pty(ptyp);

  return 0;
}

static int
pty_select_pred (struct file *filp, int rw, struct wk_term *t) {
    int master;
    struct pty *pty;
    struct pipe *pipeb;
    int next = 0;

    demand(filp, bogus filp);
    pty = GETPTYP(filp);
    master = filp->f_pos;
    demand (master == 1 || master == 0,master flag set improperly);

    switch (rw) {
    case SELECT_READ:
      pipeb = &(pty->pipe[master]);      
      next = wk_mkvar (next, t, wk_phys (&pipeb->length), 0);
      next = wk_mkimm (next, t, PTY_BUFFER_SIZE);
      next = wk_mkop (next, t, WK_LT);
      return (next);
    case SELECT_WRITE:
      pipeb = &(pty->pipe[1 - master]);      
      next = wk_mkvar (next, t, wk_phys (&pipeb->length), 0);
      next = wk_mkimm (next, t, 0);
      next = wk_mkop (next, t, WK_GT);
      return (next);
    default:
      demand (0, invalid operation);
    }
}
		
static int 
pty_select(struct file *filp, int rw) {
    int output = 0;
    struct pty *pty;
    struct pipe *pipeb;
    int master;
    demand(filp, bogus filp);
    DPRINTF(CLU_LEVEL,("pty_select\n"));
    pty = GETPTYP(filp);
				/*     lock_pty(pty); */
    master = filp->f_pos;
    demand (master == 1 || master == 0,master flag set improperly);
    switch (rw) {
    case SELECT_READ:
      pipeb = &(pty->pipe[master]);
      if (pipeb->length < PTY_BUFFER_SIZE) {
	output = 1;
      }
      break;
    /*     printf("pty_select: read %d\n",output); */
    case SELECT_WRITE:
      pipeb = &(pty->pipe[1 - master]);
      if (pipeb->length > 0) {
	output = 1;
      }
      break;
    default:
      assert(0);
    }
    /*     printf("pty_select: write %d\n",output); */
				/*     unlock_pty(pty); */
    return output;
}
static int 
pty_ioctl(struct file *filp, unsigned int request, char *argp) {
  static struct winsize ws = {24,80,0,0};
  struct pty *pty;
  struct pipe *pipeb;
    
  switch(request) {
  case TIOCGWINSZ:
    /* get window size */
    *((struct winsize *) argp) = ws;
    return 0;
  case TIOCSWINSZ:
    /* set window size */
    ws = *((struct winsize *) argp);
    return 0;
  case TIOCSTI:
    pty = GETPTYP(filp);
    pipeb = &(pty->pipe[filp->f_pos]);
    while(pipeb->length == 0) {
      unlock_filp(filp);
      wk_waitfor_value_gt (&pipeb->length, 0, 0);
#if 0
      yield(-1);
#endif
      lock_filp(filp);
    }
    lock_pipepty(pipeb); 
    if (pipewrite(pipeb,argp,1) != 1) {
      assert(0);
    }
    unlock_pipepty(pipeb); 
    return 0;
  case TIOCGPGRP:
    pty = GETPTYP(filp);
    return pty->pgrp;		/* check this */
  case TIOCSPGRP:
    pty = GETPTYP(filp);
    pty->pgrp = *(int *)argp;
    return 0;
  case TIOCGETA:
    pty = GETPTYP(filp);
    *((struct termios *)argp) = pty->tm;
    return 0;
  case TIOCSETA:
  case TIOCSETAW:
  case TIOCSETAF:
    /* we ignore request type */
    pty = GETPTYP(filp);
    pty->tm = *((struct termios *)argp);
    return 0;
  default:
    printf("ioctl filp: %p, request: %d, argp: %p\n",
	   filp,request,argp);
    assert(0);
  }
  assert(0);
  return -1;
}

static int 
pty_lookup(struct file *from, const char *name,struct file *to) {
  int master,pty_number;
  int flags = 0;
  demand(to,bogus filp);

  if (EQDOT(name)) {
    *to = *from;
    return 0;
  }
  CHECKDEVS(name,to);

  if (check_name(name, &master,&pty_number) == -1) return -1;

  to->f_dev = makeptydev(master,pty_number);
  to->f_mode = S_IFCHR && 0644;
  to->f_pos = master;
  to->f_flags = 0;
  filp_refcount_init(to);
  filp_refcount_inc(to);
  to->f_owner = __current->uid;
  to->op_type = PTY_TYPE;
  return 0;
}

static int 
pty_stat(struct file *filp, struct stat *buf) {

    DPRINTF(CLU_LEVEL,("pty_stat:\n"));
    demand(filp, bogus filp);
    if (buf == (struct stat *) 0) {errno = EFAULT; return -1;}
    
    buf->st_dev     = DEV_DEVICE;
    buf->st_ino     = PTY_TYPE;
    buf->st_mode    = 0x2190;
    buf->st_nlink   = 1;
    buf->st_uid     = getuid();
    buf->st_gid     = getgid();
    buf->st_rdev    = 0;
    buf->st_size    = 0;
    buf->st_atime   = 0;
    buf->st_mtime   = 0;
    buf->st_ctime   = 0;
    buf->st_blksize = PTY_BUFFER_SIZE;
    buf->st_blocks  = 0;
    return(0);
}



