
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

/* #define PRINTF_LEVEL 9 */
#include <fcntl.h>
#include <string.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "fd/proc.h"

#include "errno.h"
#include <exos/debug.h>

#include "exos/vm-layout.h"
#include "exos/process.h"
#include "exos/locks.h"

#include <exos/uwk.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <exos/critical.h>
#include <exos/regions.h>
#include <exos/callcount.h>
#include <unistd.h>

#include <exos/signal.h>
#if 0
#include <stdio.h>
#define PR fprintf(stderr,"%s: %d\n",__FILE__,__LINE__);
#else
#define PR
#endif

#define USEPREDICATES	   /* If you want to use tom's wk predicates */
#define HANDLESIGNALS		/* if USEPREDICATES is true, it also handles signals */
#define WPROTECT
/* #define RPROTECT                /* read protect */
#define DIRECT            /* Copy direct from writer to reader */

#define PIPE_KEY PIPE_TYPE
#define PIPE_BUFFER_SIZE (2*4096)
#define NR_PIPE 200

#ifdef HANDLESIGNALS
#include <exos/synch.h>
#endif

static struct pipe_shared_data {
    exos_lock_t lock;
    struct pipe {
	exos_lock_t lock;
	volatile int inuse[2];  /* inuse[1] = writer, inuse[0] = reader */
	volatile int head;	/* writers write here */
	volatile int tail;	/* readers read from here */
        volatile int full;      /* if head == tail, is it full or empty? */
	char buffer[PIPE_BUFFER_SIZE]; /* data in pipe */
	volatile int done;
	volatile int reid;      /* eids for reader and writer */
	volatile int weid;
#ifdef DIRECT
	volatile int n;
        volatile int written;		/* number of bytes directly written */
	volatile char *buf;
#endif
    } pipe[NR_PIPE];
} *pipe_shared_data;

static int piperegid = -99;

#define GETREGION(p) (((p) - &pipe_shared_data->pipe[0]) + piperegid)

#define PIPE_INC(i)     ((i + 1) % PIPE_BUFFER_SIZE)
#define PIPE_INCN(i, n) ((i + (n)) % PIPE_BUFFER_SIZE)
#define PIPE_FULL(p)    (((p)->head == (p)->tail) && ((p)->full))
#define PIPE_EMPTY(p)   (((p)->head == (p)->tail) && ((p)->full == 0))
#define PIPE_NBYTES(p)  (PIPE_FULL(p) ? PIPE_BUFFER_SIZE :		   \
			 ((p)->head >= (p)->tail ? (p)->head - (p)->tail : \
			  (p)->head - (p)->tail + PIPE_BUFFER_SIZE))

static inline void
SET_HEAD(struct pipe *pipep, int val) {
#ifdef WPROTECT
    int t;

    if (pipep->head != val &&
	(t = dma_to(0, &val, sizeof(val), __envid, (void *) &pipep->head, 
	       PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("set_head: dma_to: failed %d\n", t);
	assert(0);
    }
    if (pipep->head == pipep->tail) {
      val = 1;
      if (pipep->full != val &&
	  (t = dma_to(0, &val, sizeof(val), __envid, (void *) &pipep->full, 
		      PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("set_head: dma_to: failed %d\n", t);
	assert(0);
      }
    }
#else
    pipep->head = val;
    if (pipep->head == pipep->tail) pipep->full = 1;
#endif
}

static inline int
GET_HEAD(struct pipe *pipe) {
#ifdef RPROTECT
    int t;
    volatile int tmp;

    if ((t = dma_from(__envid, (void *) &pipe->head, 4,
	     0, (void *) &tmp, PIPE_KEY, GETREGION(pipe))) != 0) {
	printf("pipe_busy: dma_from: failed %d\n", t);
	assert(0);
    }
    return tmp;
#else
    return pipe->head;
#endif
}


static inline void
SET_TAIL(struct pipe *pipep, int val) {
#ifdef WPROTECT
    int t;

    if (pipep->tail !=  val &&
	(t = dma_to(0, &val, sizeof(val), __envid, (void *) &pipep->tail, 
	       PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("set_tail: dma_to: failed %d\n", t);
	assert(0);
    }
    if (pipep->head == pipep->tail) {
      val = 0;
      if (pipep->full != val &&
	  (t = dma_to(0, &val, sizeof(val), __envid, (void *) &pipep->full, 
		      PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("set_tail: dma_to: failed %d\n", t);
	assert(0);
      }
    }
#else
    pipep->tail = val;
    if (pipep->head == pipep->tail) pipep->full = 0;
#endif
}


static inline int
GET_TAIL(struct pipe *pipe) {
#ifdef RPROTECT
    int t;
    volatile int tmp;

    if ((t = dma_from(__envid, (void *) &pipe->tail, 4,
	     0, (void *) &tmp, PIPE_KEY, GETREGION(pipe))) != 0) {
	printf("pipe_busy: dma_from: failed %d\n", t);
	assert(0);
    }
    return tmp;
#else
    return pipe->tail;
#endif
}


static inline void
SET_WEID(struct pipe *pipep, int val) {
#ifdef WPROTECT
  int t;
  
  if (pipep->weid != val &&
      (t = dma_to(0, &val, sizeof(val), __envid, (void *) &pipep->weid, 
		  PIPE_KEY, GETREGION(pipep))) != 0) {
    printf("set_wied: dma_to: failed %d\n", t);
    assert(0);
  }
#else
  pipep->weid = val;
#endif
}


static inline void
SET_REID(struct pipe *pipep, int val) {
#ifdef WPROTECT
  int t;
  
  if (pipep->reid != val &&
      (t = dma_to(0, &val, sizeof(val), __envid, (void *) &pipep->reid, 
		  PIPE_KEY, GETREGION(pipep))) != 0) {
    printf("set_ried: dma_to: failed %d\n", t);
    assert(0);
  }
#else
  pipep->reid = val;
#endif
}


static inline void
WRITE_BUFFER(struct pipe *pipep, int i, char *buf, int n) {
#ifdef WPROTECT
    int t;  

    if (n > 0 &&
	(t = dma_to(0, buf, n, __envid, (void *) &(pipep->buffer[i]), 
		    PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("write_buffer: dma_to: failed %d\n", t);
	assert(0);
    }
#else
    memcpy(&pipep->buffer[i], buf, n);
#endif
}


static inline void
READ_BUFFER(struct pipe *pipep, int i, char *buf, int n) {
#ifdef RPROTECT
    int t;

    if ((t = dma_from(__envid, (void *) (&pipep->buffer[i]), n,
	     0, (void *) buf, PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("read_buffer: dma_from: failed %d\n", t);
	assert(0);
    }
#else
    memcpy(buf, &pipep->buffer[i], n);
#endif
}


static inline int 
PIPE_BUSY(int writer, struct pipe *pipep) {
#ifdef RPROTECT
    int t;
    volatile int tmp;

    if ((t = dma_from(__envid, (void *) (&pipep->inuse[(writer)]), 4,
	     0, (void *) &tmp, PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("pipe_busy: dma_from: failed %d\n", t);
	assert(0);
    }
    return (tmp == 1);
#else
    return pipep->inuse[writer];
#endif
}

/* need to find offset of pipep */
static inline void  
SET_PIPE_INUSE(int writer,struct pipe *pipep) {
#ifdef WPROTECT
    int t;
    static int one = 1;

    if (pipep->inuse[(writer)] != one &&
	(t = dma_to(0, &one, 4, __envid, (void *)&(pipep->inuse[(writer)]), 
	       PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("set_pipe_inuse: dma_to: failed %d\n", t);
	assert(0);
    }
#else
    pipep->inuse[writer] = 1;
#endif
}

static inline void 
CLR_PIPE_INUSE(int writer, struct pipe *pipep) {
#ifdef WPROTECT
    int t;
    static int zero = 0;

    if (pipep->inuse[(writer)] != zero &&
	(t = dma_to(0, &zero, 4, __envid, (void *)&(pipep->inuse[(writer)]), 
	       PIPE_KEY, GETREGION(pipep))) != 0) {
	printf("clr_pipe_inuse: dma_to failed %d\n", t);
	assert(0);
    }
#else
   pipep->inuse[writer] = 0;
#endif
}


static inline void
PIPE_INIT(struct pipe *pipe) {
#ifdef WPROTECT
    int t;
    static struct pinit {
	int inuse[2];
	int h;
	int t;
	int f;
    } pi = {{1, 1}, 0, 0, 0};
    void *s = (void *) &pipe->inuse[0];

    if ((t = dma_to(0, &pi, sizeof(pi), __envid, s, PIPE_KEY, 
	       GETREGION(pipe))) != 0) {
	printf("PIPE_INIT: dma_to failed %d\n", t);
	assert(0);
    }
#ifdef DIRECT
    {
      int n = 0;
      s = (void *)&pipe->n;
      if ((t = dma_to(0, &n, sizeof(n), __envid, s, PIPE_KEY, 
		      GETREGION(pipe))) != 0) {
	printf("PIPE_INIT: dma_to failed %d\n", t);
	assert(0);
      }
    }
#endif
#else
    pipe->inuse[0] = 1;
    pipe->inuse[1] = 1;
    pipe->head = 0;
    pipe->tail = 0;
    pipe->full = 0;
#ifdef DIRECT
    pipe->n = 0;
#endif
#endif
}

#define GETPIPEP(filp, p)  memcpy(&p, (filp)->data, sizeof(p))
#define SETPIPEP(filp, p)  memcpy((filp)->data, &p, sizeof(p))

#define MAKEPIPEP(pipenum) (&(pipe_shared_data->pipe[pipenum]))

int pipe_init(void);
int pipe(int fd[2]);
static int pipe_read(struct file *filp, char *buffer, int nbyte, int blocking);
static int pipe_write(struct file *filp, char *buffer, int nbyte, 
		      int blocking);
static int pipe_select(struct file *filp, int);
static int pipe_select_pred (struct file *filp, int, struct wk_term *);
static int pipe_stat(struct file *filp,struct stat *buf);
static int pipe_close(struct file * filp);
  
struct file_ops const pipe_file_ops = {
    NULL,			/* open */
    NULL,			/* lseek */
    pipe_read,			/* read */
    pipe_write,			/* write */
    pipe_select,		/* select */
    pipe_select_pred,           /* select_pred */
    NULL,			/* ioclt */
    NULL,			/* close0 */
    pipe_close,			/* close */
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
    pipe_stat,			/* stat */
    NULL,			/* readdir */
    NULL,			/* utimes */
    NULL,			/* bmap */
    NULL,			/* fsync */
    NULL			/* exithandler */
};

#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

static inline void 
init_pipe_shared_lock(void) { 
    DPRINTF(LOCK_LEVEL,("init_pipe_shared_lock\n"));
    exos_lock_init(&(pipe_shared_data->lock));
}
static inline void 
lock_pipe_shared_data(void) { 
    DPRINTF(LOCK_LEVEL,("lock_pipe_shared_data\n"));
    exos_lock_get_nb(&(pipe_shared_data->lock));
}
static inline void
unlock_pipe_shared_data(void) { 
    DPRINTF(LOCK_LEVEL,("unlock_pipe_shared_data\n"));
    exos_lock_release(&(pipe_shared_data->lock)); 
}

/* BUG use EnterCriticalRegion */
static inline void 
init_lock_pipe(struct pipe *pipe) { 
    DPRINTF(LOCK_LEVEL,("init_lock_pipe\n"));
    exos_lock_init(&(pipe->lock));
}
static inline void 
lock_pipe(struct pipe *pipe) { 
    DPRINTF(LOCK_LEVEL,("lock_pipe\n"));
    exos_lock_get_nb(&(pipe->lock));
}
static inline void
unlock_pipe(struct pipe *pipe) { 
    DPRINTF(LOCK_LEVEL,("unlock_pipe\n"));
    exos_lock_release(&(pipe->lock));
}


void
pr_pipep(struct pipe *pipep) {
    fprintf(stderr,"%p: lck:%d ", pipep, pipep->lock.lock);
    fprintf(stderr,"inuse reader: %d writer: %d ",
	    pipep->inuse[0],pipep->inuse[1]);
    fprintf(stderr,"pipe[s]-%d  %3d:%-3d(full=%d) ",
	    pipep->lock.lock,pipep->head, pipep->tail, pipep->full);
#ifdef DIRECT
    fprintf(stderr,"Dbuf: %p Dn: %d, Dd: %d",
	    pipep->buf, pipep->n, pipep->done);
#endif
    fprintf(stderr,"\n");
}


static struct pipe *
get_pipep(int *num) {
    struct pipe *pipep;
    int i;
    for (i = 0; i < NR_PIPE; i++) {
	pipep = MAKEPIPEP(i);
	if (PIPE_BUSY(0, pipep) || PIPE_BUSY(1, pipep)) continue;
	PIPE_INIT(pipep);
	*num = i;
	return pipep;
    }
    return 0;
}

int
pipe_init(void) {
    int i;
    int status;
    struct pipe *pipep;

    START(fd_op[PIPE_TYPE],init);
    DPRINTF(CLUHELP_LEVEL,("pipe_init\n"));

    status = fd_shm_alloc(FD_SHM_OFFSET + PIPE_TYPE,
			  (sizeof(struct pipe_shared_data)),
			  (char *)PIPE_SHARED_REGION);

    pipe_shared_data = (struct pipe_shared_data *) PIPE_SHARED_REGION;

    if (status == -1) {
	demand(0, problems attaching shm);
	STOP(fd_op[PIPE_TYPE],init);
	return -1;
    }

    if (status) {

       /* Setup the dma regions, the first NR_FTABLE regions starting at
         regid_filetable belong to file entries, the last region belongs to
         the other data in the global ftable.  */

        {
          dma_ctrlblk_t c;
          dma_region_t r[NR_PIPE];
          int i;
          for (i = 0; i < NR_PIPE; i++) {
            r[i].key = PIPE_KEY; 
            r[i].reg_addr = &pipe_shared_data->pipe[i];
            r[i].reg_size = sizeof(struct pipe);
          }
          c.nregions = NR_PIPE;
          c.dma_regions = &r[0];
          status = dma_setup_append(&c);
          assert(status >= 0);
          __current->regids[PIPE_TYPE] = status;
          piperegid = __current->regids[PIPE_TYPE];
          //kprintf("pipe regid begins at: %d\n",status);
        }

	init_pipe_shared_lock();
	for (i = 0; i < NR_PIPE; i++) {
	    pipep = MAKEPIPEP(i);
	    init_lock_pipe(pipep);
	    CLR_PIPE_INUSE(0,pipep);
	    CLR_PIPE_INUSE(1,pipep);
	}
	//kprintf("Maximum number of pipes: %d\n", NR_PIPE);
    }

    register_file_ops((struct file_ops *)&pipe_file_ops, PIPE_TYPE);
    piperegid = __current->regids[PIPE_TYPE];
    assert(__current->fops[PIPE_TYPE]->write);
    STOP(fd_op[PIPE_TYPE],init);
    return 0;
}

static int 
pipewrite(struct pipe *pipep, char *buffer, int nbyte) {
    int to_write, to_write1;
    int total;
    int n = PIPE_NBYTES(pipep);
    int h = GET_HEAD(pipep);
    int m = 0;

#ifdef DIRECT
    /* Copy as much as possible directly into the reader's buffer.  Copy
     * what is left in the shared buffer. */
    if (pipep->n > 0) {
        int r;
	m = MIN(nbyte, pipep->n);
	if ((r = sys_vcopyout(buffer, pipep->reid, (u_long) pipep->buf, 
			      m)) != 0) {
	  ExitCritical();
	  sys_cputs("vcopyout failed: ");
	  kprintf("%d\n", r);
	  printf("vcopyout failed: %d\n", r);
	  assert(0);
	}
	pipep->n = 0;
	pipep->done = 1;
	pipep->written = m;
	pipep->buf = 0;
	
	if (m == nbyte)
	  return nbyte;
	nbyte -= m;
	buffer += m;
    }
#endif
    to_write = total = MIN(nbyte, PIPE_BUFFER_SIZE - n);
    if ((h + to_write) > PIPE_BUFFER_SIZE) {
	/* copy in two steps */
	to_write1 = PIPE_BUFFER_SIZE - h;
	to_write -= to_write1;
	WRITE_BUFFER(pipep, h, buffer, to_write1);
	buffer += to_write1;
	h = 0;
    }
    /* copy in one step */
    WRITE_BUFFER(pipep, h, buffer, to_write);
    SET_HEAD(pipep, PIPE_INCN(h, to_write));
    return total + m;
}


static int 
piperead(struct pipe *pipep, char *buffer, int nbyte) {
    int to_read = 0,to_read1 = 0;
    int total;
    int n = PIPE_NBYTES(pipep);
    int t = GET_TAIL(pipep);
    
    to_read = total = MIN(nbyte, n);
    if ((t + to_read) > PIPE_BUFFER_SIZE) {
	/* copy in two steps because it wraps around */
	to_read1 = PIPE_BUFFER_SIZE - t;
	to_read -= to_read1;

	READ_BUFFER(pipep, t, buffer, to_read1);
	buffer += to_read1;
	t = 0;
    }
     
    READ_BUFFER(pipep, t, buffer, to_read);
    SET_TAIL(pipep, PIPE_INCN(t, to_read));
    return (to_read + to_read1);
}

int
pipe(int fd[2]) {
    struct pipe *newpipep;
    int fd0,fd1;
    struct file * filp;
    int num;

    OSCALLENTER(OSCALL_pipe);
    fd0 = getfd();
    if (fd0 == OUTFD) {
	errno = EMFILE;
	OSCALLEXIT(OSCALL_pipe);
	return -1;
    }
    if (fd0 == NOFILPS) {
	errno = ENFILE;
	OSCALLEXIT(OSCALL_pipe);
	return -1;
    }
    fd1 = getfd();
    if (fd1 == OUTFD) {
	putfd(fd0);
	errno = EMFILE;
	OSCALLEXIT(OSCALL_pipe);
	return -1;
    }
    if (fd1 == NOFILPS) {
	putfd(fd0);
	errno = ENFILE;
	OSCALLEXIT(OSCALL_pipe);
	return -1;
    }

    newpipep = get_pipep(&num);
    if (!newpipep) {
	putfd(fd0);
	putfd(fd1);
	kprintf("out of pipes\n");
	errno = ENFILE;
	OSCALLEXIT(OSCALL_pipe);
	return -1;
    }
    
    filp = __current->fd[fd0];
    filp->f_dev = makedev(2,num);
    filp->f_ino = PIPE_TYPE;
    filp->f_mode = S_IFREG & 0644;
    filp->f_pos = 0;
    filp->f_flags = O_RDWR;	/* popen expects this */
#if 0
    fprintf(stderr,"pipe flags: %d\n",filp->f_flags);
#endif
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = __current->uid;
    filp->op_type = PIPE_TYPE;

    SETPIPEP(filp, newpipep);

    *__current->fd[fd1] = *filp;
    __current->fd[fd1]->f_pos = 1;
    __current->fd[fd1]->f_dev = makedev(3,num);

    fd[0] = fd0; 
    fd[1] = fd1;
    OSCALLEXIT(OSCALL_pipe);
    return 0;
}


#ifdef USEPREDICATES
static int 
wk_mkread_pred (struct wk_term *t, volatile int *busy, volatile int *head,
		volatile int *tail, volatile int *done, volatile int *full) {
    int s = 0;
#undef WK_READ_PRED_SZ
#define WK_READ_PRED_SZ 16
    s = wk_mkvar (s, t, wk_phys (busy), 0);
    s = wk_mkimm (s, t, 1);
    s = wk_mkop (s, t, WK_NEQ);
    s = wk_mkop (s, t, WK_OR);
    s = wk_mkvar (s, t, wk_phys (head), 0);
    s = wk_mkvar (s, t, wk_phys (tail), 0);
    s = wk_mkop (s, t, WK_NEQ);
    s = wk_mkop (s, t, WK_OR);
    s = wk_mkvar (s, t, wk_phys (full), 0);
    s = wk_mkimm (s, t, 1);
    s = wk_mkop (s, t, WK_EQ);
    s = wk_mkop (s, t, WK_OR);
    s = wk_mkvar (s, t, wk_phys (done), 0);
    s = wk_mkimm (s, t, 0);
    s = wk_mkop (s, t, WK_NEQ);
    return (s);
}

static int 
wk_mkwrite_pred (struct wk_term *t, volatile int *busy, volatile int *head,
		 volatile int *tail, volatile int *full) {
    int s = 0;
#undef WK_WRITE_PRED_SZ
#define WK_WRITE_PRED_SZ 12
    s = wk_mkvar (s, t, wk_phys (busy), 0);
    s = wk_mkimm (s, t, 1);
    s = wk_mkop (s, t, WK_NEQ);
    s = wk_mkop (s, t, WK_OR);
    s = wk_mkvar (s, t, wk_phys (head), 0);
    s = wk_mkvar (s, t, wk_phys (tail), 0);
    s = wk_mkop (s, t, WK_NEQ);
    s = wk_mkop (s, t, WK_OR);
    s = wk_mkvar (s, t, wk_phys (full), 0);
    s = wk_mkimm (s, t, 1);
    s = wk_mkop (s, t, WK_NEQ);
    return (s);
}
#endif

/* signal semantics:
 if have writen something, and are blocking to write more, and get signal (restartable
 or not), return what has been written.
 if have written nothing, and get restartable signal, return -1 and errno = ERESTART
 and call will be restarted, otherwise return errno = EINTR */

static int
pipe_read(struct file *filp, char *buffer, int nbyte, int blocking) {
    struct pipe *pipep;
    int final = 0;
    int len;
#ifdef USEPREDICATES
    struct wk_term t[WK_READ_PRED_SZ];
    int sz;
#endif

    demand(filp, bogus filp);

    if (filp->f_pos == 1) {
	/* this fd is open for reading */
	return -1;
    }
    
    GETPIPEP(filp, pipep);
#ifdef DIRECT
    /* touch each page of buffer to make sure each is writable */
    {
      u_int va, temp=0;
      for (va = PGROUNDDOWN((u_int)buffer);
	   va < (u_int)buffer + nbyte;
	   va += NBPG)
	asm volatile ("movl %2, %0\n"
		      "\tmovl %3, %1" :
		      "=r" (temp), "=m" (*(u_int*)va) :
		      "m" (*(u_int*)va), "r" (temp) :
		      "memory");
    }
#endif
    signals_off();
    EnterCritical ();

    SET_REID(pipep, __envid);

    while (nbyte > 0) {
	if (!(PIPE_BUSY(1,pipep)) && PIPE_EMPTY(pipep)) {
	    /* someone closed the other end, and nothing to read */
	    break;
	}

	if (PIPE_EMPTY(pipep)) {
	    if (final > 0) break;
	    if (final == 0 && CHECKNB(filp)) {
		errno = EAGAIN;
		final = -1;
		break;
	    }
	}

	if (PIPE_EMPTY(pipep)) {
 	    /* we block if cant read anything */
#ifdef DIRECT
	    /* Signal writer that a direct copy is possible */
	    pipep->buf = buffer;
	    pipep->n = nbyte;
	    pipep->done = 0;
#endif
	    unlock_filp (filp);
#ifdef USEPREDICATES
	    {
	      int i;

	      /* busy wait a few times before using predicates... */
	      for (i=0; i < 3 && pipep->inuse[1] == 1 &&
		     pipep->head == pipep->tail && pipep->done == 0 &&
		     pipep->full == 0; i++)
		yield(pipep->weid);
	      if (i == 3) {
		sz = wk_mkread_pred (t, &pipep->inuse[1], &pipep->head,
				&pipep->tail, &pipep->done, &pipep->full);
#ifdef HANDLESIGNALS
		{
		  int ret;
		  ret = tsleep_pred_directed(t, sz, PCATCH, "piperead", 0,pipep->weid);
		  if (ret != 0 && (final == 0 && !pipep->done)) {
		    if (ret == EINTR) {final = -1; errno = EINTR;}
		    if (ret == ERESTART) {final = -1; errno = ERESTART;}
		    /* return EINTR or ERESTART or what we have read */
		    lock_filp(filp); 
		    goto done;
		  }
		}
#else
                wk_waitfor_pred_directed (t, sz, pipep->weid);
#endif
	      }
	    }
#else
	    yield (-1);
#endif
	    lock_filp (filp);
#ifdef DIRECT
	    /* HBXX - this assert will fail if multiple readers on the pipe */
	    assert (pipep->done || PIPE_EMPTY(pipep));
	    if (pipep->done) {	/* did the writer do a direct copy? */
	      final += pipep->written;
	      goto done;
	    }
#endif
	} else {
	    len = piperead(pipep, buffer, nbyte);
	    final += len;
	    buffer += len;
	    nbyte -= len; 
	}
    }
done:
    signals_on();
#ifdef DIRECT
    pipep->n = 0;		/* no need to copy anymore */
#endif
    ExitCritical ();
    return final;
}


/* signal semantics:
 if have writen something, and are blocking to write more, and get signal (restartable
 or not), return what has been written.
 if have written nothing, and get restartable signal, return -1 and errno = ERESTART
 and call will be restarted, otherwise return errno = EINTR */

static int
pipe_write(struct file *filp, char *buffer, int nbyte, int blocking) {
    struct pipe *pipep;
    int total = 0;
    int final = 0;
#ifdef USEPREDICATES
    struct wk_term t[WK_WRITE_PRED_SZ];
    int sz;
#endif

    demand(filp, bogus filp);

    if (filp->f_pos == 0) {
	/* this fd is openened for writing */
	return -1;
    }

    GETPIPEP(filp, pipep);

    if (PIPE_FULL(pipep) && CHECKNB(filp)) {
	errno = EAGAIN;
	return -1;
    }

#ifdef DIRECT
    /* touch each page of buffer to make sure each is readable */
    {
      u_int va;
      volatile char c;
      for (va = PGROUNDDOWN((u_int)buffer);
	   va < (u_int)buffer + nbyte;
	   va += NBPG)
	c = *((char*)va);
    }
#endif

    /* after this enter critical we make sure we either do an
       ExitCritical soon or do lots of yields to keep the kernel
       happy */
    signals_off();
    EnterCritical ();

    SET_WEID(pipep, __envid);

    while(nbyte > 0) {
	if (!(PIPE_BUSY(0, pipep))) {
	    if (final == 0) {
		final = -1;
		errno = EINTR;
		kill(getpid(),SIGPIPE);	/* it wont be delivered, until we leave */
		/* signal(getpid(),SIGPIPE); */
	    }
	    break;
	}

	if (PIPE_FULL(pipep)) {
	    unlock_filp (filp);
#ifdef USEPREDICATES
	    {
	      int i;

	      /* busy wait a few times before using predicates... */
	      for (i=0; i < 2 && pipep->inuse[0] == 1 &&
		     pipep->head == pipep->tail && pipep->full == 1; i++)
		yield(pipep->reid);
	      if (i == 2) {
		sz = wk_mkwrite_pred (t,&pipep->inuse[0], &pipep->head,
				      &pipep->tail, &pipep->full);
#ifdef HANDLESIGNALS
		{
		  int ret;
		  ret = tsleep_pred_directed(t, sz, PCATCH, "pipewrite", 0,pipep->reid);
		  if (ret != 0) {
		    if (ret == EINTR && final == 0) {final = -1; errno = EINTR;}
		    if (ret == ERESTART && final == 0) {final = -1; errno = ERESTART;}
		    /* return EINTR or ERESTART or what we have read */
		    
		    lock_filp(filp); 
		    goto done;
		  }
		}
#else
                wk_waitfor_pred_directed (t, sz, pipep->reid);
#endif
	      }
	    }
#else
	    yield(-1);
#endif
	    lock_filp (filp);  
	} else {
	    total = pipewrite(pipep, buffer, nbyte);
	    final += total;
	    buffer += total;
	    nbyte -= total;
	}
    }
done:
    signals_on();
    ExitCritical ();
    return final;
}

static int 
pipe_close(struct file * filp) {
    struct pipe *pipep;
    int writer;

    demand(filp, bogus filp);
    GETPIPEP(filp, pipep);
    writer = filp->f_pos;
    assert(writer == 0 || writer == 1);
    CLR_PIPE_INUSE(writer,pipep);

    return 0;
}

static int
pipe_select_pred (struct file *filp, int rw, struct wk_term *t) {
    struct pipe *pipe;
    int writer;
    int next = 0;

    demand (rw == SELECT_READ || rw == SELECT_WRITE, invalid op-type passed by select);
    demand (filp, bogus filp);
    GETPIPEP (filp, pipe);

    writer = filp->f_pos;
    demand (writer == 1 || writer == 0,writer flag set improperly);
    if (writer) {
	if (rw == SELECT_READ) {
	    demand (0, fd is always ready);
	} else { /* rw == SELECT_WRITE */
	    next += wk_mkwrite_pred (t, &pipe->inuse[0], &pipe->head,
				     &pipe->tail, &pipe->full);
	    return (next);
	}
    } else {
	if (rw == SELECT_READ) {
	    next += wk_mkread_pred(t, &pipe->inuse[1], &pipe->head,
				   &pipe->tail, &pipe->done, &pipe->full);
	    return (next);
	} else { /* rw == SELECT_WRITE */
	    demand (0, fd is always ready);
	}
    }
}

static int 
pipe_select(struct file *filp,int rw) {
    struct pipe *pipe;
    int writer;

    demand(filp, bogus filp);
    DPRINTF(CLU_LEVEL,("pipe_select\n"));
    GETPIPEP(filp, pipe);

    writer = filp->f_pos;
    demand (writer == 1 || writer == 0,writer flag set improperly);
  
    if (writer) {
	switch(rw) {
	case SELECT_READ:
	    return 1;
	case SELECT_WRITE:
	    return PIPE_FULL(pipe) ? 0 : 1;
	default:
	    return 0;
	}
	    
    } else {
	switch(rw) {
	case SELECT_READ:
	    return  PIPE_EMPTY(pipe) ? 0 : 1;
	case SELECT_WRITE:
	    return 1;
	default:
	    return 0;
	}
    }
    return 0;
}

static int 
pipe_stat(struct file *filp, struct stat *buf) {

    DPRINTF(CLU_LEVEL,("pty_stat:\n"));
    demand(filp, bogus filp);
    if (buf == (struct stat *) 0) {errno = EFAULT; return -1;}
    
    buf->st_dev     = filp->f_dev;
    buf->st_ino     = filp->f_ino;
    buf->st_mode    = 0x1000;
    buf->st_nlink   = 0;
    buf->st_uid     = getuid();
    buf->st_gid     = getgid();
    buf->st_rdev    = 0;
    buf->st_size    = 0;
    buf->st_atime   = 0;
    buf->st_mtime   = 0;
    buf->st_ctime   = 0;
    buf->st_blksize = PIPE_BUFFER_SIZE;
    buf->st_blocks  = 0;
    return(0);
}
