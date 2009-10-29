
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

/* who should lock the file, the system call or the user?, probably
   the system call because if you are specializing or are the only
   user, you may not need to lock it.  For now no locking since we
   have to do through testing in single user non-share state.  */

/* Caveat: I have strived to maintain the constrained that lookup
 takes a locked directory and returns another a locked filp, and that
 namei takes a path an returns a locked filp.  All of these should be
 released properly.  This is not a problem since NFS is stateless, but
 if this code is ever used with a statefull File System, this property
 has to be checked.  (released is used to tell the system that it
 could deallocate "inodes" for example) */

/* INVARIANT: foreach filp returned via lookup there will be a release
 *	      foreach filp returned via open there will be a close */
#if 0
#undef PRINTF_LEVEL
#define PRINTF_LEVEL 1
#endif

#include "fd/proc.h"
#include "fd/path.h"
#include <string.h>
#include <exos/callcount.h>
#include <exos/mallocs.h>
#include <fcntl.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/uio.h>
#include <exos/signal.h>
#include <xok/ae_recv.h>

//#define FAKE_PROT
#ifdef FAKE_PROT
#include <sys/sys_ucall.h>
#define EXTRA_SYSCALL ( sys_geteid (), sys_geteid(), sys_geteid() )
#else
#define EXTRA_SYSCALL
#endif

/* unistd stuff */
#define SEEK_CUR 1
extern int close(int);

#include <assert.h>
#include <malloc.h>

#include <exos/debug.h>
#include "fd/fdstat.h"

#include <stdarg.h>

#undef NULL
#define NULL	0

#if 0
#define PR fprintf(stderr,"%s: %d\n",__FILE__,__LINE__);
#else
#define PR
#endif

//#define CACHEDLOOKUP /* if doing cheap/dirty cached lookup see namei_cache.c */
extern int __sdidinit;
#define kprintf if (0) printf
//#define printf if (1) printf
extern char *__progname;


#define assert2(x)						\
if(!(x)) {							\
  char *i;							\
  int tot = 0;							\
  printf("\nx: %p  ",(void *)(x));				\
  for (i = (char *)((u_int)(&x) & ~PGMASK);			\
       i < (char *)(((u_int)(&x) & ~PGMASK) + NBPG); i++)	\
    if (!*i) tot++;						\
  printf("  %d: total zeros in page: %08x, %d\n",		\
	 __LINE__,((u_int)(&x) & ~PGMASK),tot);			\
  printf("t1: %d  cur: %d",t1,TICKS);				\
  assert(0);							\
}
void *current_file_op = NULL;

/* this structure is used for socket() and 
 * register_family_type_protocol() 
 */
typedef struct family_type_protocol_matrix {
  int busy;
  int family;
  int type;
  int protocol;
  int (*socket)(struct file *);
  struct family_type_protocol_matrix *next;
} ftpm_t, *ftpm_p;

static ftpm_p ftpm_base = 0;


/* clear_filp_lock, lock_filp, unlock_filp - similar to ftable
   operations, just lock an specific filp so no one can change it
   while you are using it. Implemented naively for now. */
void
clear_filp_lock(struct file * filp) { 
    DPRINTF(LOCK_LEVEL,("clear_filp_lock\n"));
    demand(filp, null file pointer);
    exos_lock_init(&filp->lock);
}
inline void
lock_filp(struct file * filp) {
    DPRINTF(LOCK_LEVEL,("lock_filp\n"));
    demand(filp, null file pointer);
    /* potential race condition, when acquiring the lock and disabling signals */
    exos_lock_get_nb(&(filp->lock));
    signals_off();
    EXTRA_SYSCALL;
}

#define lock_filp(filp)				\
    DPRINTF(LOCK_LEVEL,("lock_filp\n"));	\
    EXTRA_SYSCALL;				\
    demand(filp, null file pointer);		\
    exos_lock_get_nb(&(filp->lock));\
    signals_off();

inline void
unlock_filp(struct file * filp) { 
    DPRINTF(LOCK_LEVEL,("unlock_filp\n"));
    demand(filp, null file pointer);
    exos_lock_release(&filp->lock);
    signals_on();
    EXTRA_SYSCALL;
}

#define unlock_filp(filp)			\
    DPRINTF(LOCK_LEVEL,("unlock_filp\n"));	\
    EXTRA_SYSCALL;				\
    demand(filp, null file pointer);		\
    exos_lock_release(&filp->lock);\
    signals_on();

int 
register_file_ops(struct file_ops *fops, int type) {
  if (type < 0 || type >= SUPPORTED_OPS) { 
    assert(0);return -1;
  } else {
    if (__current->fops[type]) {
      printf("replacing operations of type: %d\n",type);
    }
    __current->fops[type] = fops;
    return 0;
  }
}


void pr_fd2(int fd,char *label) {
  pr_filp(__current->fd[fd],label);
  fprintf(stderr,"closeon exec: %d\n",__current->cloexec_flag[fd]);
}

int
pr_fds(void) {
    int i, d = 0;

    fprintf(stderr,"PID: %d fds in use:\n",getpid());
    for (i = 0; i < NR_OPEN; i++) {
	if (__current->fd[i]) {
	  char buffer[16];
	  fprintf(stderr,"FD: %2d filp %p cloexec: %d\n",i,__current->fd[i],
		  __current->cloexec_flag[i]);
	  sprintf(buffer,"FD: %2d",i);
	  pr_filp(__current->fd[i],buffer);
	  d++;
	}
    }
    return d;
}

/* ------------------------------------------------------------------------------------------ */ 
/* extra printing functions */

char *filp_to_string (struct file *filp, char *buf, int maxlen)
{
  int len;
  if (IS_FILP_FLOCKED(filp)) {
    len = snprintf (buf, maxlen - 1, "<dev %3d ino %8d refs %3d %s env: %d>", filp->f_dev, filp->f_ino, 
		    filp_refcount_get(filp), 
		    ((filp->flock_state == FLOCK_STATE_SHARED) ? "shared" : "exclusive"),
		    filp->flock_envid);

  } else {
    len = snprintf (buf, maxlen - 1, "<dev %3d ino %8d refs %3d unlocked>", filp->f_dev, filp->f_ino, 
		    filp_refcount_get(filp));
  }
  *(buf + len) = NULL; 
  return buf;
}



static void
print_fds(FILE *out) {
    int i;
    int BUFSIZE=60;
    char buf[BUFSIZE];

    if (out)
      fprintf(out, "PID %d ENVID %d PROG %s. fds in use:\n",getpid(), __envid, __progname);
    else
      kprintf("PID %d ENVID %d PROG %s. fds in use:\n",getpid(), __envid, __progname);

    for (i = 0; i < NR_OPEN; i++) {
	if (__current->fd[i]) {
	  if (out)
	    fprintf(out, "FD: %2d filp %s cloexec: %d\n",i,
		   filp_to_string(__current->fd[i],buf,BUFSIZE),
		   __current->cloexec_flag[i]);
	  else
	    kprintf("FD: %2d filp %s cloexec: %d\n",i,
		    filp_to_string(__current->fd[i],buf,BUFSIZE),
		    __current->cloexec_flag[i]);
	}
    }
}





static void
print_ftable(FILE *out) {
  int i;
  int BUFSIZE=60;
  char buf[BUFSIZE];

  if (out)
    fprintf(out, "--FTABLE--\n"); 
  else
    kprintf("--FTABLE--\n");

  for (i=0; i < NR_FTABLE; i++) {
    if (FD_ISSET(i,&global_ftable->inuse))
      {
	struct file *filp = &global_ftable->fentries[i];
	if (out)
	  fprintf (out, "%s\n", filp_to_string(filp, buf, BUFSIZE));
	else
	  kprintf ("%s\n", filp_to_string(filp, buf, BUFSIZE));
      }
  }
  if (out)
    fprintf(out, "----------\n");
  else
    kprintf("----------\n");
}


void dump_file_structures(FILE *out) {
  print_fds(out);
  print_ftable(out);
}


/* ------------------------------------------------------------------------------------------ */ 


/* getfd - gets you a fd with an associated file pointer. */
int
getfd(void)
{
    int fd;
    START(ftable,getfd);
    DPRINTF(SYSHELP_LEVEL,("getfd entering\n"));
    for(fd = 0; fd < NR_OPEN ; fd++)
	if (__current->fd[fd] == NULL) 
	    {
		__current->fd[fd] = getfilp();
		DPRINTF(SYSHELP_LEVEL,("filp = %08x\n",(int)__current->fd[fd]));

		if (__current->fd[fd] == (struct file *)0)
		{
		    DPRINTF(SYSHELP_LEVEL,("getfd out of filps!\n"));
		    fprintf(stderr,"getfd out of filps!\n");
		    STOP(ftable,getfd);
		    return NOFILPS;
		} else {
#if 0
		    DPRINTF(SYS_LEVEL,("found: %d, %08x\n",fd,(int)__current->fd[fd]));
#endif
		    filp_refcount_init(__current->fd[fd]);
		    __current->cloexec_flag[fd] = 0;
		    STOP(ftable,getfd);
		    return fd;
		}
	    }
    DPRINTF(SYSHELP_LEVEL,("getfd out of fds!\n"));
    fprintf(stderr,"getfd out of fds!\n");
    STOP(ftable,getfd);
    return OUTFD;
}

/* putfd - deallocates a fd */
void
putfd(int fd)
{
  START(ftable,putfd);
    DPRINTF(SYSHELP_LEVEL,("putfd entering fd: %d\n",fd));
    if (!((fd >= 0 && fd < NR_OPEN))) {
	printf("BAD FD: %d\n",fd);
    }
    demand (fd >= 0 && fd < NR_OPEN, putfd: fd out of bounds);
    if (__current->fd[fd] == NULL) {
      fprintf(stderr,"warning, putfd was given a non-inuse fd\n");
      return;
    } else {
	if (filp_refcount_get(__current->fd[fd]) == 0) putfilp(__current->fd[fd]);
	__current->fd[fd] = (struct file *) NULL;
	__current->cloexec_flag[fd] = 0;
    }
    STOP(ftable,putfd);
}

/* make sure you check right value for followlinks flag for namei */


/* dup does not fully comply with UNIX, when you dup a file, the new
 * fd should have the CLOSE_ON_EXEC flags cleared, but then 
 * again we dont yet have a real exec function. */

int
dup2(int fd1, int fd2) {
    struct file * filp;

    OSCALLENTER(OSCALL_dup2);
    DPRINTF(SYS_LEVEL,("dup2 %d %d: entering\n",fd1,fd2));
    CHECKFD(fd1, OSCALL_dup2);
    
    if (fd2 < 0 || fd2 > NR_OPEN) {
      errno = EBADF;
      OSCALLEXIT(OSCALL_dup2);
      return -1;
    }

    /* handle the case they are the same */
    if (fd1 == fd2) {
      OSCALLEXIT(OSCALL_dup2);
      return fd2;
    }

    if (__current->fd[fd2]) {
	close(fd2);
    }
    
    filp = __current->fd[fd1];
    
    __current->fd[fd2] = __current->fd[fd1];
    filp_refcount_inc(filp);
    
    OSCALLEXIT(OSCALL_dup2);
    return fd2;
}

static int
dup3(int fd1, int fd2) {
  /* will find a file descriptor greater or equal fd2 */
    struct file * filp;

    DPRINTF(SYS_LEVEL,("dup3 %d %d: entering\n",fd1,fd2));
    CHECKFD(fd1, -1);
    
    if (fd2 < 0 || fd2 > NR_OPEN) { errno = EINVAL;return -1;}

    filp = __current->fd[fd1];
    for ( ; fd2 < NR_OPEN ; fd2++) {
      if (__current->fd[fd2] == NULL) {
	filp_refcount_inc(filp);
	__current->fd[fd2] = filp;
	return fd2;
      }
    }
    errno = EMFILE;
    return -1;
}

int
dup(int fd) {
  int r;

  OSCALLENTER(OSCALL_dup);
  DPRINTF(SYS_LEVEL,("dup %d: entering\n",fd));
  r = dup3(fd,0);
  OSCALLEXIT(OSCALL_dup);
  return r;
}

int 
close(int fd)
{
    int error = 0;
    struct file * filp;
    
    OSCALLENTER(OSCALL_close);
    START(fd,close);

    DPRINTF(SYS_LEVEL,("close %d: entering\n",fd));
    CHECKFD(fd, OSCALL_close);
    filp = __current->fd[fd];

    /* CLOSE0 is called on any close */
    lock_filp(filp);
    filp_refcount_dec(filp);

    if(CHECKOP(filp,close0)) {
      kprintf("%18s real close0 ino: %d\n","",filp->f_ino);
      error = DOOP(filp,close0,(filp,fd));
    }

    if (filp_refcount_get(filp) == 0) {
      /* 
       * if refcount == 0, then we must be the last ones referenceing filp
       * so it should be unnecessary to lock the following line with the ftable_flock (right?)
       */
      filp->flock_state = FLOCK_STATE_UNLOCKED;
    }

    unlock_filp(filp);

    if (filp_refcount_get(filp) == 0) {
      /* special case for character devices which might be
	 opened via many independent processes 
	 */
      lock_filp(filp);
      if(CHECKOP(filp,close)) {
	kprintf("%18s real close ino: %d\n","",filp->f_ino);
	error = DOOP(filp,close,(filp));
      }
      unlock_filp(filp);
    }

    putfd(fd);
    STOP(fd,close);
    OSCALLEXIT(OSCALL_close);
    return(error);
}


int 
sendto(int fd, const void *msg, size_t len, int flags, 
       const struct sockaddr *to, int tolen) {
    int error;
    struct file * filp;

    OSCALLENTER(OSCALL_sendto);
    START(fd,sendto);
#if 0
    DPRINTF(SYS_LEVEL,("sendto %d: entering\n",fd));
#endif
    CHECKFD(fd, OSCALL_sendto);
    filp = __current->fd[fd];
    error = CHECKOP(filp,sendto);
    if (error == NULL) {
      errno = ENOTSOCK;
      OSCALLEXIT(OSCALL_sendto);
      return -1;
    }
    /* should be able to handle errors: 
       EINVAL, EMSGSIZE, ENOBUFS, EWOULDBLOCK, EINTR, EFAULT */
    lock_filp(filp);
    error = DOOP(filp,sendto,(filp, (char *)msg, len, BLOCKING, flags, (struct sockaddr *)to, tolen)); 
    unlock_filp(filp);
    STOP(fd,sendto);
    OSCALLEXIT(OSCALL_sendto);
    return error;
}

int 
recvfrom(int fd, void *msg, size_t len, int flags, 
	   struct sockaddr *from, int *fromlen) {
    int error;
    struct file * filp;

    OSCALLENTER(OSCALL_recvfrom);
    START(fd,recvfrom);
#if 0
    DPRINTF(SYS_LEVEL,("recvfrom %d %p %d 0x%x: entering\n",fd,msg,len,flags));
#endif
    CHECKFD(fd, OSCALL_recvfrom);
    filp = __current->fd[fd];
    error = CHECKOP(filp,recvfrom);
    if (error == NULL) {
      errno = ENOTSOCK;
      OSCALLEXIT(OSCALL_recvfrom);
      return -1;
    }
    /* should be able to handle errors: 
       EWOULDBLOCK, EINTR,  EFAULT */
    lock_filp(filp);
    error = DOOP(filp,recvfrom,(filp, (char *)msg, len, BLOCKING, 
			       flags, from, fromlen)); 
    unlock_filp(filp);

    STOP(fd,recvfrom);
    OSCALLEXIT(OSCALL_recvfrom);
    return error;
}

int 
bind(int fd, const struct sockaddr *name, int namelen) {
    int error, r;
    struct file * filp;

    OSCALLENTER(OSCALL_bind);
    DPRINTF(SYS_LEVEL,("bind %d %p %d: entering\n",fd,name,namelen));

    CHECKFD(fd, OSCALL_bind);

    filp = __current->fd[fd];
    error = CHECKOP(filp,bind);
    if (error == NULL) { 
	DPRINTF(SYS_LEVEL,("bind: fd not a socket (no bind operation)\n"));

	errno = ENOTSOCK; 
	OSCALLEXIT(OSCALL_bind);
	return -1;
    }
    /* should be able to handle errors: 
       EACCES, EADDRINUSE, EADDRNOTAVAIL, EFAULT, EINVAL, 
       EACCES, EIO, EISDIR, ELOOP, ENAMETOOLONG, ENOENT, ENOTDIR, EROFS  */
    r = DOOP(filp,bind,(filp, (struct sockaddr *)name, namelen));
    OSCALLEXIT(OSCALL_bind);
    return r;
}


int 
connect(int fd, const struct sockaddr *name, int namelen) {
    int error, r;
    struct file * filp;
    OSCALLENTER(OSCALL_connect);
    DPRINTF(SYS_LEVEL,("connect %d %p %d: entering\n",fd,name,namelen));

    CHECKFD(fd, OSCALL_connect);

    filp = __current->fd[fd];
    error = CHECKOP(filp,connect);
    if (error == NULL) { 
	DPRINTF(SYS_LEVEL,("connect: fd not a socket (no connect operation)\n"));

	errno = ENOTSOCK; 
	OSCALLEXIT(OSCALL_connect);
	return -1;
    }
    /* should be able to handle errors: 
       EACCES, EADDRINUSE, EADDRNOTAVAIL, EFAULT, EINVAL, 
       EACCES, EIO, EISDIR, ELOOP, ENAMETOOLONG, ENOENT, ENOTDIR, EROFS  */
    r = DOOP(filp,connect,(filp, (struct sockaddr *)name, namelen,0));
    OSCALLEXIT(OSCALL_connect);
    return r;
}

int 
accept(int fd, struct sockaddr *name, int *namelen) {
    int error, newfd;
    struct file * filp;
    struct file * newfilp;

    OSCALLENTER(OSCALL_accept);
    DPRINTF(SYS_LEVEL,("accept %d %p %p: entering\n",fd,name,namelen));

    CHECKFD(fd, OSCALL_accept);

    filp = __current->fd[fd];

    error = CHECKOP(filp,bind);
    if (error == NULL) { 
	DPRINTF(SYS_LEVEL,("accept: fd not a socket (no bind operation)\n"));
	errno = ENOTSOCK; 
	OSCALLEXIT(OSCALL_accept);
	return -1;
    }

    error = CHECKOP(filp,accept);
    if (error == NULL) { 
	DPRINTF(SYS_LEVEL,("accept: fd no accept operation\n"));
	errno = EOPNOTSUPP; 
	OSCALLEXIT(OSCALL_accept);
	return -1;
    }

    newfd = getfd();
    if (newfd == OUTFD) {
	errno = EMFILE;
	OSCALLEXIT(OSCALL_accept);
	return -1;
    }
    if (newfd == NOFILPS) {
	errno = ENFILE;
	OSCALLEXIT(OSCALL_accept);
	return -1;
    }

    newfilp = __current->fd[newfd];

    /* should be able to handle errors: 
       EWOULDBLOCK */
    error =  DOOP(filp,accept,(filp, newfilp, name, namelen,0));
    if (error == -1) {
      putfd(newfd);
      OSCALLEXIT(OSCALL_accept);
      return -1;
    } else {
      filp_refcount_init(newfilp);
      filp_refcount_inc(newfilp);
      OSCALLEXIT(OSCALL_accept);
      return newfd;
    }
}

int 
listen(int fd, int backlog) {
    int error, r;
    struct file * filp;
    
    OSCALLENTER(OSCALL_listen);
    DPRINTF(SYS_LEVEL,("listen %d %d: entering\n",fd,backlog));

    CHECKFD(fd, OSCALL_listen);

    filp = __current->fd[fd];
    error = CHECKOP(filp,bind);
    if (error == NULL) { 
	DPRINTF(SYS_LEVEL,("listen: fd not a socket (no bind operation)\n"));
	errno = ENOTSOCK; 
	OSCALLEXIT(OSCALL_listen);
	return -1;
    }
    error = CHECKOP(filp,listen);
    if (error == NULL) { 
	DPRINTF(SYS_LEVEL,("listen: fd no listen operation\n"));
	errno = EOPNOTSUPP; 
	OSCALLEXIT(OSCALL_listen);
	return -1;
    }
    r = DOOP(filp,listen,(filp, backlog));
    OSCALLEXIT(OSCALL_listen);
    return r;
}

int 
ioctl (int fd, unsigned long request, void *argp) {
  struct file * filp;
  int error, r;

  OSCALLENTER(OSCALL_ioctl);
  CHECKFD(fd, OSCALL_ioctl);
  filp = __current->fd[fd];
  START(fd,ioctl);
  switch(request) {
  case FIOCLEX:
    r = fcntl(fd,F_SETFD,FD_CLOEXEC);
    OSCALLEXIT(OSCALL_ioctl);
    return r;
  case FIONCLEX:
    r = fcntl(fd,F_SETFD,0);
    OSCALLEXIT(OSCALL_ioctl);
    return r;
  case FIONBIO:
    if (*(int*)argp) {
      r = fcntl(fd,F_SETFL,filp->f_flags | O_NONBLOCK);
      OSCALLEXIT(OSCALL_ioctl);
      return r;
    } else {
      r = fcntl(fd,F_SETFL,filp->f_flags & ~O_NONBLOCK);
      OSCALLEXIT(OSCALL_ioctl);
      return r;
    }
  case OSIOCGIFBRDADDR:
  case SIOCGIFBRDADDR:
  case SIOCGIFFLAGS:
  case OSIOCGIFADDR:
  case SIOCGIFADDR:
  case OSIOCGIFCONF:
  case SIOCGIFCONF:
    {
      extern int if_ioctl(int d, unsigned long request, char *argp);
      r = if_ioctl(fd,request,argp);
      OSCALLEXIT(OSCALL_ioctl);
      return r;
    }
  default:
  }
  error = CHECKOP(filp,ioctl);
  if (error == NULL) { 
    switch(request) {
    case TIOCGETA:
      /* this is ok for non terminals not to have them */
      OSCALLEXIT(OSCALL_ioctl);
      return -1;
    default:
      
      kprintf("ioctl unknown: fd %d type: %d request: %08x\n",
	      fd,filp->op_type,(int)request);
      printf("ioctl unknown: fd %d type: %d request: %08x\n",
	     fd,filp->op_type,(int)request);
      OSCALLEXIT(OSCALL_ioctl);
      return -1;
    }
  }
  /* should be able to handle errors: 
     EACCES, EDEADLK, EFAULT, EINTR, EINVAL, EMFILE, ENOLCK
     */
  lock_filp(filp) ; 
  error = DOOP(filp,ioctl,(filp, request, argp)); 
  if (error == -1) {
    kprintf("ioctl fd: %d, request: %d, argp: %p failed\n",
	    fd,(int)request,argp);
  }
  unlock_filp(filp) ; 
  STOP(fd,ioctl);
  OSCALLEXIT(OSCALL_ioctl);
  return error;
}

int 
fcntl (int fd, int request, ...) {
    int error;
    va_list ap;
    int arg;
    struct file * filp;

    OSCALLENTER(OSCALL_fcntl);

    va_start(ap,request);
    arg = va_arg(ap,int);
    va_end(ap);

    START(fd,fcntl);
    //DPRINTF(SYS_LEVEL,("fcntl %d %d %d: entering\n",fd,request,arg));

    CHECKFD(fd, OSCALL_fcntl);
    filp = __current->fd[fd];
    switch(request) {
    case F_SETFD:
      arg &= FD_CLOEXEC;
      __current->cloexec_flag[fd] = arg;
      error = 0;
      goto good;
    case F_GETFD:
      error = (__current->cloexec_flag[fd] & FD_CLOEXEC);
      goto good;
    case F_DUPFD:
      error = dup3(fd,arg);
      goto good;
    case F_SETFL:
      demand(!(arg & O_ASYNC),O_ASYNC not implemented yet);
      /* set flag only allows these 3 flags */
      arg &= (O_NONBLOCK | O_APPEND | O_ASYNC);
      /* clear those flags */
      filp->f_flags &= ~(O_NONBLOCK | O_APPEND | O_ASYNC);
      /* set them again */
      filp->f_flags |= arg;
      error = 0;
      goto good;
    case F_GETFL:
      /*  & (O_NONBLOCK | O_APPEND | O_ASYNC); 
      * HBXX this is not what it sez on the man pages, but netbsd assumes it
      * gives all flags back*/
      /* it seems they dont want FD_CLOEXEC because you can get it
       via GETFD and it interferes with O_RDWR */
      error = filp->f_flags;
      goto good;
    case F_GETOWN:
      error = filp->f_owner;
      goto good;
    case F_SETOWN:
      filp->f_owner = arg;
      error = 0;
      goto good;
    default:
      fprintf(stderr,"fctnl fd: %d, request: %d, argp: 0x%08x\n",
	   fd,request,arg);
      demand(0, unknown flag);
    }
    error = CHECKOP(filp,fcntl);
    if (error == NULL) { demand(0,fcntl undefined);}
    /* should be able to handle errors: 
       EACCES, EDEADLK, EFAULT, EINTR, EINVAL, EMFILE, ENOLCK
       */
    lock_filp(filp) ; 
    error = DOOP(filp,fcntl,(filp, request, arg)); 
    unlock_filp(filp) ;
good:
    STOP(fd,fcntl);

    OSCALLEXIT(OSCALL_fcntl);
    return error;
}

int
register_family_type_protocol(int family, int type, int protocol,     
			      int (*socket)(struct file *)) {
  ftpm_p ftpm = ftpm_base;
#if 0
  DPRINTF(SYS_LEVEL,("register_ftpm: entering\n"));
#endif
  if (!ftpm) {
    ftpm = ftpm_base = (ftpm_p)__malloc(sizeof(ftpm_t));
    assert(ftpm_base);
  } else {
    for (  ; ftpm->next != 0 ; ftpm = ftpm->next);

    ftpm->next = (ftpm_p)__malloc(sizeof(ftpm_t));
    assert(ftpm->next);
    ftpm = ftpm->next;
  }
  ftpm->next = 0;
  ftpm->busy = 1;
  ftpm->family = family;
  ftpm->type = type;
  ftpm->protocol = protocol;
  ftpm->socket = socket;
  return 0;
}

int
socketpair(int d, int type, int protocol, int *sv) {
  demand(0, socketpair not implemented);
}

int 
socketf(struct file * filp, int family, int type, int protocol) {
  ftpm_p ftpm = ftpm_base;

  assert(ftpm_base);
  for (ftpm = ftpm_base ; ftpm != 0 ; ftpm = ftpm->next) {
    if (ftpm->family == family && 
	ftpm->type == type && 
	ftpm->protocol == protocol) {
      return ftpm->socket(filp);
    }
  }
  errno = EPROTONOSUPPORT;
  /* note: not reporting EPROTOTYPE */
  return -1;
}

int 
socket(int family, int type, int protocol) {
  int fd;
  int error;
  struct file * filp;

  OSCALLENTER(OSCALL_socket);
  DPRINTF(SYS_LEVEL,("socket %d %d %d: entering\n",family,type,protocol));

  fd = getfd();
  if (fd == OUTFD) {
    errno = EMFILE;
    OSCALLEXIT(OSCALL_socket);
    return -1;
  }
  if (fd == NOFILPS) {
    errno = ENFILE;
    OSCALLEXIT(OSCALL_socket);
    return -1;
  }
  filp = __current->fd[fd];
  
  error = socketf(filp,family,type,protocol);
  if (error == -1) {
    /* assumes	socketf(filp) set the correct errno */
    putfd(fd);
    OSCALLEXIT(OSCALL_socket);
    return error;
  }
  filp_refcount_init(filp);
  filp_refcount_inc(filp);
  filp->f_flags |= O_RDWR;
  OSCALLEXIT(OSCALL_socket);
  return fd;
}

int 
shutdown(int fd, int flags) {
    int error;
    struct file * filp;

    OSCALLENTER(OSCALL_shutdown);
    DPRINTF(SYS_LEVEL,("shutdown %d 0x%x: entering\n",fd,flags));

    CHECKFD(fd, OSCALL_shutdown);
    filp = __current->fd[fd];
    error = CHECKOP(filp,shutdown);
    if (error == NULL) {
      errno = ENOTSOCK;
      OSCALLEXIT(OSCALL_shutdown);
      return -1;
    }
    lock_filp(filp);
    error = DOOP(filp,shutdown,(filp, flags));
    unlock_filp(filp);
    OSCALLEXIT(OSCALL_shutdown);
    return error;
}

/* getsockname and getpeername use the getname file op 
   (see struct file_ops).  They just differ in the last argument
   to getname:
   int (*getname)      (struct file *sock, struct sockaddr *uaddr,
			int *usockaddr_len, int peer);
   if peer == 0 then we want the getsockname behavior, if peer == 1,
   we want the getpeername behavior
*/
int 
getsockname(int fd, struct sockaddr *bound, int *boundlen) {
    int error;
    struct file * filp;

    OSCALLENTER(OSCALL_getsockname);
    DPRINTF(SYS_LEVEL,("getsockname %d %p %p: entering\n",fd,bound, boundlen));

    CHECKFD(fd, OSCALL_getsockname);
    filp = __current->fd[fd];
    error = CHECKOP(filp,getname);
    if (error == NULL) {
      errno = ENOTSOCK;
      OSCALLEXIT(OSCALL_getsockname);
      return -1;
    }
    lock_filp(filp);
    error = DOOP(filp,getname,(filp, bound, boundlen, 0));
    unlock_filp(filp);
    OSCALLEXIT(OSCALL_getsockname);
    return error;
}


int 
getpeername(int fd, struct sockaddr *bound, int *boundlen) {
    int error;
    struct file * filp;

    OSCALLENTER(OSCALL_getpeername);
    DPRINTF(SYS_LEVEL,("getpeername %d %p %p: entering\n",fd,bound,boundlen));

    CHECKFD(fd, OSCALL_getpeername);
    filp = __current->fd[fd];
    error = CHECKOP(filp,getname);
    if (error == NULL) {
      errno = ENOTSOCK;
      OSCALLEXIT(OSCALL_getpeername);
      return -1;
    }
    lock_filp(filp);
    error = DOOP(filp,getname,(filp, bound, boundlen, 1));
    unlock_filp(filp);
    OSCALLEXIT(OSCALL_getpeername);
    return error;
}

int
getsockopt(int fd, int level, int optname, void *optval, int *optlen) {
    int error;
    struct file * filp;

    OSCALLENTER(OSCALL_getsockopt);
    DPRINTF(SYS_LEVEL,("getsockopt %d %d %d %p %p: entering\n",
		       fd,level,optname, optval,optlen));

    CHECKFD(fd, OSCALL_getsockopt);
    filp = __current->fd[fd];
    error = CHECKOP(filp,getsockopt);
    if (error == NULL) {
      errno = ENOTSOCK;
      OSCALLEXIT(OSCALL_getsockopt);
      return -1;
    }
    lock_filp(filp);
    error = DOOP(filp,getsockopt,(filp,level,optname,optval,optlen));
    unlock_filp(filp);
    OSCALLEXIT(OSCALL_getsockopt);
    return error;
}

int
setsockopt(int fd, int level, int optname,const void *optval, int optlen) {
    int error;
    struct file * filp;

    OSCALLENTER(OSCALL_setsockopt);
    DPRINTF(SYS_LEVEL,("setsockopt %d %d %d %p %d: entering\n",
		       fd,level,optname,optval,optlen));

    CHECKFD(fd, OSCALL_setsockopt);
    filp = __current->fd[fd];
    error = CHECKOP(filp,setsockopt);
    if (error == NULL) {
      errno = ENOTSOCK;
      OSCALLEXIT(OSCALL_setsockopt);
      return -1;
    }
    lock_filp(filp);
    error = DOOP(filp,setsockopt,(filp, level, optname, optval,  optlen));
    unlock_filp(filp);
    OSCALLEXIT(OSCALL_setsockopt);
    return error;
}


ssize_t
readv(int d, const struct iovec *iov, int iovcnt) {
  int i;
  int total,partial;

  OSCALLENTER(OSCALL_readv);
  total = 0;
  for(i = 0; i < iovcnt ; iov++) {
    if (iov->iov_len == 0) continue;

    partial = read(d,iov->iov_base, iov->iov_len);
    if (partial > 0) {
      total += partial;
      if (partial != iov->iov_len) {
	OSCALLEXIT(OSCALL_readv);
	return total;
      }
    } else if (total > 0) {
    /* partial <= 0, we return total because we read some bytes */
      OSCALLEXIT(OSCALL_readv);
      return total;
    } else {
      /* last read failed */
      OSCALLEXIT(OSCALL_readv);
      return -1;
    }
  }
  OSCALLEXIT(OSCALL_readv);
  return total;
}

ssize_t
read(int fd, void *buf, size_t nbyte)
{
    int error;
    struct file * filp;
    retry:
    OSCALLENTER(OSCALL_read);
    START(fd,read);

#if 0
    DPRINTF(SYS_LEVEL,("read %d %p %d: entering\n",fd,buf,nbyte));
#endif
    CHECKFD(fd, OSCALL_read);
    filp = __current->fd[fd];
    error = CHECKOP(filp,read);
    if (error) {
      /* should handle errors:
     EAGAIN, EBADMSG, EFAULT, EINTR, EINVAL, EIO, EISDIR, 
     EWOULDBLOCK, EFAULT, EINVAL.
     */
	if (nbyte < 0) {
	  errno = EINVAL;
	  OSCALLEXIT(OSCALL_read);
	  return -1;
	}
	lock_filp(filp);
	error = DOOP(filp,read,(filp,(char *)buf,nbyte,NONBLOCKING));
	unlock_filp(filp);
	STOP(fd,read);
	OSCALLEXIT(OSCALL_read);
	if (error == -1 && errno == ERESTART) goto retry;
	return(error);
    } else {
      errno = EBADF;
      OSCALLEXIT(OSCALL_read);
      return -1;
    }
}

ssize_t
writev(int d, const struct iovec *iov, int iovcnt) {
  int total,partial;
  total = 0;

  OSCALLENTER(OSCALL_writev);
  for( ; iovcnt > 0; iov++,iovcnt--) {
    if (iov->iov_len == 0) continue;

    partial = write(d,iov->iov_base, iov->iov_len);
    if (partial > 0) {
      total += partial;
      if (partial != iov->iov_len) {
	OSCALLEXIT(OSCALL_writev);
	return total;
      }
    } else if (total > 0) {
    /* partial <= 0 */
      OSCALLEXIT(OSCALL_writev);
      return total;
    } else {
      OSCALLEXIT(OSCALL_writev);
      return partial;
    }
  }
  OSCALLEXIT(OSCALL_writev);
  return total;
}

ssize_t 
write(int fd, const void *buf, size_t nbyte)
{
    int error;
    struct file * filp;

retry:
    OSCALLENTER(OSCALL_write);
    START(fd,write);
#if 0
    DPRINTF(SYS_LEVEL,("write %d %p %d: entering\n",
		       fd,buf,nbyte));
#endif
    CHECKFD(fd, OSCALL_write);
    filp = __current->fd[fd];
    error = CHECKOP(filp,write);
    if (error) {
      /* should handle errors:
	 EBADF, EDQUOT, EFAULT, EFBIG, EINTR, EINVAL, EIO, ENOSPC,
	 ENXIO, EPIPE, ERANGE.
	 */
	if (nbyte < 0) {
	  errno = EINVAL;
	  OSCALLEXIT(OSCALL_write);
	  return -1;
	}
      lock_filp(filp) ; 
      error = DOOP(filp,write,(filp,(char *)buf,nbyte,NONBLOCKING));
      unlock_filp(filp) ; 
      STOP(fd,write);
      OSCALLEXIT(OSCALL_write);
      if (error == -1 && errno == ERESTART) goto retry;
      return(error);
    } else {
      errno = EBADF;
      OSCALLEXIT(OSCALL_write);
      return -1;
    }
}

off_t 
lseek(int fd, off_t offset, int whence) {
  off_t errorq;
  int error;
  struct file * filp;

  OSCALLENTER(OSCALL_lseek);
  START(fd,lseek);
  DPRINTF(SYS_LEVEL,("lseek %d %d %d: entering\n",fd,(int)offset,whence));

  CHECKFD(fd, OSCALL_lseek);
  filp = __current->fd[fd];
  error = CHECKOP(filp,lseek);
  if (error) {
    /* should handle errors:
       EBADF, EINVAL, ESPIPE
       */
    
    errorq = DOOP(filp,lseek,(filp,offset,whence));
    STOP(fd,lseek);
    if (errorq < 0) {
      fprintf(stderr,"lseek %d, %qd, %d returned -1 errno: %d\n",fd,offset,whence,errno);
      pr_filp(filp,"failed lseek");
    }
	
    
    OSCALLEXIT(OSCALL_lseek);
    return (int)errorq;
  } else {
//    fprintf(stderr,"warning: lseek op missing: lseek op_type: %d\n",
//	    filp->op_type);
    errno = EBADF;
    OSCALLEXIT(OSCALL_lseek);
    return -1;
  }
}

int
tell(int fd) {
  return lseek(fd, 0L, SEEK_CUR);
}


int 
bmap(int fd, u_quad_t *diskBlockP, off_t offset, u_int *seqBlocksP) {
    struct file *filp;
    int status;
    u_int diskBlockHighP, diskBlockLowP;
    DPRINTF(SYS_LEVEL,("bmap %d %p %qd %p: entering\n",fd,diskBlockP,offset,seqBlocksP));
    CHECKFD(fd, -1);
    filp = __current->fd[fd];

    if (diskBlockP == NULL) {
       errno = EFAULT;
       return (-1);
    }

    if (CHECKOP(filp,bmap) == 0) {
	errno = EBADF;		/* this is the closest I can find */
	return -1;
    }
    status = DOOP(filp,bmap,(filp,&diskBlockHighP,&diskBlockLowP,offset,seqBlocksP));
    *diskBlockP = INT2QUAD(diskBlockHighP, diskBlockLowP);
    return status;
}


int 
fstat(int fd, struct stat *buf) {
    struct file *filp;
    int status;

    OSCALLENTER(OSCALL_fstat);
    START(fd,fstat);
    //DPRINTF(SYS_LEVEL,("fstat %d %p: entering\n",fd,buf));
    CHECKFD(fd, OSCALL_fstat);
    filp = __current->fd[fd];

    if (CHECKOP(filp,stat) == 0) {
	errno = EBADF;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_fstat);
	return -1;
    }
    status = DOOP(filp,stat,(filp,buf));
    STOP(fd,fstat);
    OSCALLEXIT(OSCALL_fstat);
    return status;
}

int 
stat(const char *path, struct stat *buf) {
    int error;
    struct file filp;

    OSCALLENTER(OSCALL_stat);
    START(fd,stat);
    
    //DPRINTF(SYS_LEVEL,("stat %s %p: entering\n",path,buf));
    if (path == (char *)0 || buf == (struct stat *)0) {
      errno = EFAULT; 
      OSCALLEXIT(OSCALL_stat);
      return -1;
    }
    if (*path == (char)0) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_stat);
      return -1;
    }
    
    error = namei(NULL,path,&filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_stat);
      return -1;
    }
    
    if (CHECKOP(&filp,stat) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_stat);
	return -1;
    }
    
    error = DOOP(&filp,stat,(&filp,buf));

    namei_release(&filp,0);
    STOP(fd,stat);
    OSCALLEXIT(OSCALL_stat);
    return error;
}

int 
lstat(const char *path, struct stat *buf) {
    int error;
    struct file filp;

    OSCALLENTER(OSCALL_lstat);
    START(fd,lstat);
    DPRINTF(SYS_LEVEL,("lstat %s %p: entering\n",path,buf));
//    printf("* lstat %s %p: entering\n",path,buf);

    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_lstat);
      return -1;
    }
    
    error = namei(NULL,path,&filp,0); 
    if (error) {
      OSCALLEXIT(OSCALL_lstat);
      return -1;
    }
    
    if (CHECKOP(&filp,stat) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_lstat);
	return -1;
    }
    
    error = DOOP(&filp,stat,(&filp,buf));

    namei_release(&filp,0);
    STOP(fd,lstat);
    OSCALLEXIT(OSCALL_lstat);
    return error;
}

/* Other system calls */

int 
mkdir(const char *path, mode_t mode) {
    int error;
    struct file dir_filp;
    char suffix[NAME_MAX + 1];
    char prefix[PATH_MAX - NAME_MAX + 1];

    OSCALLENTER(OSCALL_mkdir);
    START(fd,mkdir);

    DPRINTF(SYS_LEVEL,("mkdir %s 0x%x: entering\n",path,mode));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_mkdir);
      return -1;
    }
    

    SplitPath(path,suffix,prefix);

    DPRINTF(SYS_LEVEL,("symlink: path: \"%s\"\n"
		       "suffix: \"%s\", prefix: \"%s\"\n",
		       path,suffix,prefix));

    error = namei(NULL,prefix,&dir_filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_mkdir);
      return -1;
    }

    if (CHECKOP(&dir_filp,mkdir) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_mkdir);
	return -1;
    }
    
    error = DOOP(&dir_filp,mkdir,(&dir_filp,suffix,mode));

    namei_release(&dir_filp,0);

    STOP(fd,release);
    OSCALLEXIT(OSCALL_mkdir);
    return(error);
}

int 
rmdir(const char *path) {
    int error;
    struct file dir_filp;
    char suffix[NAME_MAX + 1];
    char prefix[PATH_MAX - NAME_MAX + 1];

    OSCALLENTER(OSCALL_rmdir);
    START(fd,rmdir);

    DPRINTF(SYS_LEVEL,("rmdir %s: entering\n",path));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_rmdir);
      return -1;
    }

    SplitPath(path,suffix,prefix);

    DPRINTF(SYS_LEVEL,("symlink: path: \"%s\"\n"
		       "suffix: \"%s\", prefix: \"%s\"\n",
		       path,suffix,prefix));

    error = namei(NULL,prefix,&dir_filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_rmdir);
      return -1;
    }

    /* check for mount point */

    if (CHECKOP(&dir_filp,rmdir) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_rmdir);
	return -1;
    }
    
    error = DOOP(&dir_filp,rmdir,(&dir_filp,suffix));
#ifdef CACHEDLOOKUP
#error Thisshouldnotbe
    if (!error) namei_cache_remove(&dir_filp,suffix,NULL);
#endif

    namei_release(&dir_filp,0);
    STOP(fd,rmdir);
    OSCALLEXIT(OSCALL_rmdir);
    return(error);
}

int 
truncate(const char *path, off_t length) {
    int error;
    struct file filp;
    START(fd,truncate);
    
    OSCALLENTER(OSCALL_truncate);
    DPRINTF(SYS_LEVEL,("truncate %s %qd: entering\n",path,length));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_truncate);
      return -1;
    }
    if (length < 0) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_truncate);
      return -1;
    }
    
    error = namei(NULL,path,&filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_truncate);
      return -1;
    }
    
    if (CHECKOP(&filp,truncate) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	error = -1;
    } else {
      error = DOOP(&filp,truncate,(&filp,length));
    }

    namei_release(&filp,0);
    STOP(fd,truncate);
    OSCALLEXIT(OSCALL_truncate);
    return error;
}

int 
ftruncate(int fd, off_t length) {
    struct file *filp;
    int error;
    START(fd,ftruncate);

    OSCALLENTER(OSCALL_ftruncate);
    DPRINTF(SYS_LEVEL,("ftruncate %d %qd: entering\n",fd,length));
    CHECKFD(fd, OSCALL_ftruncate);
    filp = __current->fd[fd];

    if (length < 0) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_ftruncate);
      return -1;
    }

    if (CHECKOP(filp,truncate) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_ftruncate);
	return -1;
    }
    
    error = DOOP(filp,truncate,(filp,length));
    STOP(fd,ftruncate);
    OSCALLEXIT(OSCALL_ftruncate);
    return error;
}

int 
chmod(const char *path, mode_t mode) {
    int error;
    struct file filp;

    OSCALLENTER(OSCALL_chmod);
    START(fd,chmod);
    
    DPRINTF(SYS_LEVEL,("chmod %s 0x%x: entering\n",path,mode));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_chmod);
      return -1;
    }
    
    error = namei(NULL,(char *)path,&filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_chmod);
      return -1;
    }
    
    if (CHECKOP(&filp,chmod) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	error = -1;
    } else {
      error = DOOP(&filp,chmod,(&filp,mode));
    }

    namei_release(&filp,0);
    STOP(fd,chmod);
    OSCALLEXIT(OSCALL_chmod);
    return error;
}


int 
fchmod(int fd, mode_t mode) {
    struct file *filp;
    int error;

    OSCALLENTER(OSCALL_fchmod);
    START(fd,chmod);
    DPRINTF(SYS_LEVEL,("fchmod %d 0x%x: entering\n",fd,mode));
    CHECKFD(fd, OSCALL_fchmod);
    filp = __current->fd[fd];

    if (CHECKOP(filp,chmod) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_fchmod);
	return -1;
    }
    
    error = DOOP(filp,chmod,(filp,mode));
    STOP(fd,chmod);
    OSCALLEXIT(OSCALL_fchmod);
    return error;
}

int 
chown(const char *path, uid_t owner, gid_t group) {
    int error;
    struct file filp;

    OSCALLENTER(OSCALL_chown);	  
    DPRINTF(SYS_LEVEL,("chown %s %d %d: entering\n",path,owner,group));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_chown);    
      return -1;
    }
    
    error = namei(NULL,path,&filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_chown);    
      return -1;
    }
    
    if (CHECKOP(&filp,chown) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	error = -1;
    } else {
      error = DOOP(&filp,chown,(&filp,owner,group));
    }

    namei_release(&filp,0);

    OSCALLEXIT(OSCALL_chown);	 
    return error;
}

/* same as chown, except don't follow last link */
int
lchown(const char *path, uid_t owner, gid_t group) {
    int error;
    struct file filp;

    OSCALLENTER(OSCALL_chown);	  
    DPRINTF(SYS_LEVEL,("chown %s %d %d: entering\n",path,owner,group));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_chown);    
      return -1;
    }
    
    error = namei(NULL,path,&filp,0);
    if (error) {
      OSCALLEXIT(OSCALL_chown);    
      return -1;
    }
    
    if (CHECKOP(&filp,chown) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	error = -1;
    } else {
      error = DOOP(&filp,chown,(&filp,owner,group));
    }

    namei_release(&filp,0);

    OSCALLEXIT(OSCALL_chown);	 
    return error;
}

int 
fchown(int fd, uid_t owner, gid_t group) {
    struct file *filp;
    int error;

    OSCALLENTER(OSCALL_fchown);    
    DPRINTF(SYS_LEVEL,("fchown %d %d %d: entering\n",fd,owner,group));
    CHECKFD(fd, OSCALL_fchown);
    filp = __current->fd[fd];

    if (CHECKOP(filp,chown) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_fchown);    
	return -1;
    }
    
    error = DOOP(filp,chown,(filp,owner,group));
    
    OSCALLEXIT(OSCALL_fchown);	  
    return error;
}

int 
getdirentries(int fd, char *buffer, int count, long *basep) {
    struct file *filp;
    int error;

    OSCALLENTER(OSCALL_getdirentries);	  
    START(fd,getdirentries);
    DPRINTF(SYS_LEVEL,("getdirentries %d %p %d %p: entering\n",
		       fd,buffer,count,basep));
    CHECKFD(fd, OSCALL_getdirentries);
    filp = __current->fd[fd];

    if ( ! S_ISDIR(filp->f_mode)) { /* make sure is a directory */
      errno = ENOTDIR;
      OSCALLEXIT(OSCALL_getdirentries);    
      return -1;
    }
    if (CHECKOP(filp,getdirentries) == 0) {
	errno = EIO;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_getdirentries);    
	return -1;
    }
    if (basep != (long *)0) {
      *basep = filp->f_pos;
    }
    error = DOOP(filp,getdirentries,(filp,buffer,count));
    
    STOP(fd,getdirentries);
    OSCALLEXIT(OSCALL_getdirentries);	 
    return error;
}

int 
readlink(const char *path, char *buf, size_t bufsize) {
    int error;
    struct file filp;
    char suffix[NAME_MAX + 1];
    char prefix[PATH_MAX - NAME_MAX + 1];

    OSCALLENTER(OSCALL_readlink);    
    START(fd,readlink);

    DPRINTF(SYS_LEVEL,("readlink %s %p %d: entering\n",
		       path,buf,bufsize));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_readlink);    
      return -1;
    }
    
//    printf("readlink %s %p %d: entering\n",path,buf,bufsize);

    SplitPath(path,suffix,prefix);

    DPRINTF(SYS_LEVEL,("readlink: path: \"%s\"\n"
		       "suffix: \"%s\", prefix: \"%s\"\n",
		       path, suffix,prefix));
    error = namei(NULL,path,&filp,0);

    if (error) {
      OSCALLEXIT(OSCALL_readlink);    
      return -1;
    }

    if (! S_ISLNK(filp.f_mode)) {
      errno = EINVAL;
      error = -1;
    } else if (CHECKOP(&filp,readlink) == 0) {
      fprintf(stderr,"warning: readling op missing: readlink %s\n",
	     path);
      errno = ENOENT;		/* this is the closest I can find */
      error = -1;
    } else {
      error = DOOP(&filp,readlink,(&filp,buf,bufsize));
      /* the contents of buf are not null terminated */
    }

    namei_release(&filp,0);
    STOP(fd,readlink);
    OSCALLEXIT(OSCALL_readlink);    
    return error;
}

int 
rename(const char *path1, const char *path2) {
    int error;
    struct file dir_filp1, dir_filp2;
    char suffix1[NAME_MAX + 1];
    char prefix1[PATH_MAX - NAME_MAX + 1];
    char suffix2[NAME_MAX + 1];
    char prefix2[PATH_MAX - NAME_MAX + 1];
    
    OSCALLENTER(OSCALL_rename);    
    START(fd,rename);

    DPRINTF(SYS_LEVEL,("rename %s %s: entering\n",path1,path2));
    if (path1 == (char *)NULL || path2 == (char *)NULL) {
	errno = EFAULT;
	OSCALLEXIT(OSCALL_rename);    
	return -1;
    } else if (*path1 == NULL || *path2 == NULL) {
	errno = ENOENT;
	OSCALLEXIT(OSCALL_rename);    
	return -1;
    }

    SplitPath(path1,suffix1,prefix1);
    SplitPath(path2,suffix2,prefix2);

    DPRINTF(SYS_LEVEL,("rename: path1: \"%s\"\n"
		       "suffix1: \"%s\", prefix1: \"%s\"\n",
		       path1, suffix1,prefix1));
    DPRINTF(SYS_LEVEL,("rename: path2: \"%s\"\n"
		       "suffix2: \"%s\", prefix2: \"%s\"\n",
		       path2, suffix2,prefix2));

    error = namei(NULL,prefix1,&dir_filp1,1);
    if (error) {
      OSCALLEXIT(OSCALL_rename);    
      return -1;
    }

    error = namei(NULL,prefix2,&dir_filp2,1);
    if (error) {
      OSCALLEXIT(OSCALL_rename);    
      return -1;
    }

    if (CHECKOP(&dir_filp1,rename) == 0) {
      fprintf(stderr,"warning: rename op missing: rename %s,%s\n",
	     path1,path2);
	errno = EINVAL;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_rename);    
	return -1;
    }

    if (dir_filp1.op_type != dir_filp2.op_type) {
	errno = EXDEV;
	OSCALLEXIT(OSCALL_rename);    
	return -1;
    }
	
    error = DOOP(&dir_filp1,rename,(&dir_filp1,suffix1, &dir_filp2,suffix2));
    
    namei_release(&dir_filp1,0);
    namei_release(&dir_filp2,0);
    STOP(fd,rename);
    OSCALLEXIT(OSCALL_rename);	  
    return error;
}

int 
unlink(const char *path) {
    int error;
    struct file dir_filp;
    char suffix[NAME_MAX + 1];
    char prefix[PATH_MAX - NAME_MAX + 1];

    OSCALLENTER(OSCALL_unlink);    
    START(fd,unlink);

    DPRINTF(SYS_LEVEL,("unlink %s: entering\n",path));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_unlink);    
      return -1;
    }
    
    SplitPath(path,suffix,prefix);

    DPRINTF(SYS_LEVEL,("unlink: path: \"%s\"\n"
		       ", suffix: \"%s\", prefix: \"%s\"\n",
		       path, suffix,prefix));
    error = namei(NULL,prefix,&dir_filp,1);

    if (error) {
      OSCALLEXIT(OSCALL_unlink);    
      return -1;
    }
    
    if (CHECKOP(&dir_filp,unlink) == 0) {
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_unlink);    
	return -1;
    }
    
    error = DOOP(&dir_filp,unlink,(&dir_filp,suffix));
    
#ifdef CACHEDLOOKUP
    if (!error) namei_cache_remove(&dir_filp,suffix,NULL);
#endif
    namei_release(&dir_filp,0);
    STOP(fd,unlink);
    OSCALLEXIT(OSCALL_unlink);	  
    return(error);
}

int 
link(const char *path1, const char *path2) {
    int error;
    struct file filp,dir_filp;
    char suffix[NAME_MAX + 1];
    char prefix[PATH_MAX - NAME_MAX + 1];

    OSCALLENTER(OSCALL_link);	 
    START(fd,link);

    DPRINTF(SYS_LEVEL,("link %s %s: entering\n",path1,path2));
    if (*path1 == NULL) {
      errno = ENOENT;
      error = -1;
      goto error;
    }
    if (*path2 == NULL) {
      errno = ENOENT;
      error = -1;
      goto error;
    }
    
    SplitPath(path2,suffix,prefix);

    DPRINTF(SYS_LEVEL,("link: path1: \"%s\"\npath2: \"%s\"\n"
		       ", suffix: \"%s\", prefix: \"%s\"\n",
		       path1,path2, suffix,prefix));
    error = namei(NULL,prefix,&dir_filp,1);
    if (error) {
      errno = ENOENT;
      error = -1;
      goto error;
    }

    error = namei(NULL,path1,&filp,1);
    if (error) {
      errno = ENOENT;
      error = -1;
      goto error1;
    }

    if (CHECKOP(&dir_filp,link) == 0) {
      fprintf(stderr,"warning: link op missing: link %s,%s\n",
	      path1,path2);
      errno = EACCES;		/* this is the closest I can find */
      error = -1;
      goto error2;
    }

    if (dir_filp.f_dev != filp.f_dev) {
      errno = EXDEV;
      error = -1;
      goto error2;
    }
    
    error = DOOP(&dir_filp,link,(&filp,&dir_filp,suffix));
    
 error2:
    namei_release(&filp,0);
 error1:
    namei_release(&dir_filp,0);
 error:
    STOP(fd,link);
    OSCALLEXIT(OSCALL_link);	
    return error;
}

int 
symlink(const char *path1, const char *path2) {
    int error;
    struct file dir_filp;
    char suffix[NAME_MAX + 1];
    char prefix[PATH_MAX - NAME_MAX + 1];
    
    OSCALLENTER(OSCALL_symlink);    
    START(fd,symlink);
    DPRINTF(SYS_LEVEL,("symlink %s %s: entering\n",path1,path2));
    if (*path2 == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_symlink);
      return -1;
    }
    
    SplitPath(path2,suffix,prefix);

    DPRINTF(SYS_LEVEL,("symlink: path1: \"%s\"\npath2: \"%s\"\n"
		       ", suffix: \"%s\", prefix: \"%s\"\n",
		       path1,path2, suffix,prefix));
    error = namei(NULL,prefix,&dir_filp,1);
    if (error) {
      OSCALLEXIT(OSCALL_symlink);
      return -1;
    }

    if (CHECKOP(&dir_filp,symlink) == 0) {
      fprintf(stderr,"warning: symlink op missing: symlink %s,%s\n",
	      path1,path2);
	errno = ENOENT;		/* this is the closest I can find */
	OSCALLEXIT(OSCALL_symlink);
	return -1;
    }
    
    error = DOOP(&dir_filp,symlink,(&dir_filp,suffix,path1));
    
    namei_release(&dir_filp,0);
    STOP(fd,symlink);
    OSCALLEXIT(OSCALL_symlink);
    return error;
}

int 
mkfifo(const char *a, mode_t b) {
   OSCALLENTER(OSCALL_mkfifo);
   errno = EOPNOTSUPP;
   OSCALLEXIT(OSCALL_mkfifo);
   return (-1);
}

int 
chflags(const char *a, unsigned int b) {
   OSCALLENTER(OSCALL_chflags);
   errno = EOPNOTSUPP;
   OSCALLEXIT(OSCALL_chflags);
   return (-1);
}

int 
fchflags(int a, unsigned int b) {
   OSCALLENTER(OSCALL_fchflags);
   errno = EOPNOTSUPP;
   OSCALLEXIT(OSCALL_fchflags);
   return (-1);
}




void
prcwd(void) {
  pr_filp(__current->cwd,"cwd");
}
int
chdir(const char *path) {
    int error;
    struct file *newfilp;
    START(fd,chdir);

    OSCALLENTER(OSCALL_chdir);
    DPRINTF(SYS_LEVEL,("chdir %s: entering\n",path));
    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_chdir);
      return -1;
    }
    
    newfilp = getfilp();
    if (newfilp == (struct file *)0) {
      errno = -100;		/* ? */
      goto err;
    }

    error = namei(NULL,path,newfilp,1);
    if (error) {
      goto err;
    }
    
    if ( ! S_ISDIR(newfilp->f_mode)) { /* make sure is a directory */
      errno = ENOTDIR;
      namei_release(newfilp,0);
      goto err;
    }

    clear_filp_lock(newfilp);
    lock_filp(__current->cwd);
    filp_refcount_dec(__current->cwd);

    if (filp_refcount_get(__current->cwd) == 0) {
      unlock_filp(__current->cwd);
      //printf("namei_release __current->cwd\n");
      namei_release(__current->cwd,1);
      putfilp(__current->cwd);
    } else {
      unlock_filp(__current->cwd);
    }
    
    filp_refcount_init(newfilp);
    filp_refcount_inc(newfilp);
//    newfilp->f_flags |= O_DONOTRELEASE;
    __current->cwd = newfilp;
    __current->cwd_isfd = 0;
    kprintf("%18s chdir %s: entering new ino %d\n","",path,__current->cwd->f_ino);
    STOP(fd,chdir);
    OSCALLEXIT(OSCALL_chdir);
    return error;
err:
    STOP(fd,chdir);
    putfilp(newfilp);
    OSCALLEXIT(OSCALL_chdir);
    return -1;
}

int
fchdir(int fd) {
    int error;
    struct file *newfilp, *filp;
    
    OSCALLENTER(OSCALL_fchdir);
    START(fd,chdir);

    DPRINTF(SYS_LEVEL,("fchdir %d: entering\n",fd));

    filp = __current->fd[fd];
    
    if ( ! S_ISDIR(filp->f_mode)) {
      errno = ENOTDIR;
      OSCALLEXIT(OSCALL_fchdir);
      return -1;
    }
    
    newfilp = getfilp();
    if (newfilp == (struct file *)0) {
      errno = -100;		/* ? */
      goto err;
    }


    error = namei(filp,".",newfilp,1);
    if (error) {
      goto err;
    }
    
    if ( ! S_ISDIR(newfilp->f_mode)) { /* make sure is a directory */
      errno = ENOTDIR;
      namei_release(newfilp,0);
      goto err;
    }

    clear_filp_lock(newfilp);
    lock_filp(__current->cwd);
    filp_refcount_dec(__current->cwd);

    if (filp_refcount_get(__current->cwd) == 0) {
      unlock_filp(__current->cwd);
      namei_release(__current->cwd,1);
      putfilp(__current->cwd);
    } else {
      unlock_filp(__current->cwd);
    }
    
    filp_refcount_init(newfilp);
    filp_refcount_inc(newfilp);
//    newfilp->f_flags |= O_DONOTRELEASE;
    __current->cwd = newfilp;
    __current->cwd_isfd = 0;
    kprintf("%18s fchdir: entering new ino %d\n","",__current->cwd->f_ino);
    STOP(fd,chdir);
    OSCALLEXIT(OSCALL_fchdir);
    return error;
err:
    STOP(fd,chdir);
    putfilp(newfilp);
    OSCALLEXIT(OSCALL_fchdir);
    return -1;
}

#if 0
int
fchdir(int fd) {
    struct file *filp;
    int error;
    START(fd,fchdir);
    DPRINTF(SYS_LEVEL,("fchdir %d: entering\n",fd));
    CHECKFD(fd);
    filp = __current->fd[fd];
    
    if ( ! S_ISDIR(filp->f_mode)) {
      errno = ENOTDIR;
      return -1;
    }

    if (filp == __current->cwd) {return 0;}
    
    lock_filp(__current->cwd);
    filp_refcount_dec(__current->cwd);
    
    if (filp_refcount_get(__current->cwd) == 0) {
      unlock_filp(__current->cwd);
      /* cwd */
      if (__current->cwd_isfd) {
	if (CHECKOP(__current->cwd,close)) {
	    kprintf("%18s real close ino: %d\n","",__current->cwd->f_ino);
	    error = DOOP(__current->cwd,close,(__current->cwd)); /* DOOPNT */
	}
      } else {
	namei_release(__current->cwd,1);
      }
      putfilp(__current->cwd);
    } else {
      unlock_filp(__current->cwd);
    }
    lock_filp(filp);
    filp_refcount_inc(filp);
    filp->f_flags |= O_DONOTRELEASE;
    __current->cwd = filp;
    __current->cwd_isfd = 1;
    unlock_filp(filp);
    STOP(fd,fchdir);
    return 0;
}
#endif
int
fsync(int fd) {
  struct file *filp;

  OSCALLENTER(OSCALL_fsync);
  CHECKFD(fd, OSCALL_fsync);
  filp = __current->fd[fd];
  if (CHECKOP(filp,bind)) {
    errno = EINVAL;  /* Fd refers to a socket, not to a file */
    OSCALLEXIT(OSCALL_fsync);
    return -1;
  } 
  if (CHECKOP(filp,fsync)) {
     int error = DOOP(filp,fsync,(filp));
     OSCALLEXIT(OSCALL_fsync);
     return (error);
  } else {
     /* HBXX we can do this because NFS is synchronous */
     /* & the other devices are currently assuming that this op is irrelevant */
     OSCALLEXIT(OSCALL_fsync);
     return 0;
  }
}


int
chroot(const char *path) {
    int error;
    struct file *newfilp;
    
    OSCALLENTER(OSCALL_chroot);
    DPRINTF(SYS_LEVEL,("chroot %s: entering\n",path));

    if (*path == NULL) {
      errno = ENOENT;
      OSCALLEXIT(OSCALL_chroot);
      return -1;
    }
    
    newfilp = getfilp();
    error = namei(NULL,path,newfilp,1);
    if (error) {goto err;}
    
    if (newfilp == (struct file *)0) {
	errno = -100;		/* ? */
	goto err;
    }

    if ( ! S_ISDIR(newfilp->f_mode)) { /* make sure is not a directory */
      errno = ENOTDIR;
      namei_release(newfilp,0);
      goto err;
    }

    clear_filp_lock(newfilp);

    lock_filp(__current->root);
    filp_refcount_dec(__current->root);

    if (filp_refcount_get(__current->root) == 0) {
      unlock_filp(__current->root);
      namei_release(__current->root,1);
      putfilp(__current->root);
    } else {
      unlock_filp(__current->root);
    }
    
    filp_refcount_init(newfilp);
    filp_refcount_inc(newfilp);
    newfilp->f_flags |= O_DONOTRELEASE;
    __current->root = newfilp;;
  
    OSCALLEXIT(OSCALL_chroot);
    return error;
err:
    putfilp(newfilp);
    OSCALLEXIT(OSCALL_chroot);
    return -1;
}


int
mknod(const char *path, mode_t mode, dev_t dev) {
   OSCALLENTER(OSCALL_mknod);
   printf ("Trying to use mknod, which doesn't yet exist\n");
   errno = EOPNOTSUPP;
   OSCALLEXIT(OSCALL_mknod);
   return (-1);
}

static int 
scheck_read_permission(struct stat *statbuf) {
  DPRINTF(CLUHELP_LEVEL,("checking read perm: file mode: %o uid: %d gid: %d\n",
	   statbuf->st_mode,statbuf->st_uid, statbuf->st_gid));

  if (statbuf->st_mode & S_IROTH) return(1);
  if (statbuf->st_mode & S_IRUSR && statbuf->st_uid == __current->uid) return(1);
  if (statbuf->st_mode & S_IRGRP && statbuf->st_gid == __current->egid) return(1);

  if (statbuf->st_mode & S_IRGRP)
    if (__is_in_group (statbuf->st_gid)) {
      return 1;
    } else {
      return 0;
    }
  return(0);
}

static int 
scheck_write_permission(struct stat *statbuf) {
  
  DPRINTF(CLUHELP_LEVEL,("checking write perm: file mode: %o uid: %d gid: %d\n",
	   statbuf->st_mode,statbuf->st_uid, statbuf->st_gid));
  
  if (statbuf->st_mode & S_IWOTH) return(1);
  if (statbuf->st_mode & S_IWUSR && statbuf->st_uid == __current->uid) return(1);
  if (statbuf->st_mode & S_IWGRP && statbuf->st_gid == __current->egid) return(1);

  if (statbuf->st_mode & S_IWGRP)
    if (__is_in_group (statbuf->st_gid)) {
      return 1;
    } else {
      return 0;
    }
  return 0;
}

static int 
scheck_execute_permission(struct stat *statbuf) {
  DPRINTF(CLUHELP_LEVEL,("checking execute perm: file mode: %o uid: %d gid: %d\n
",
	   statbuf->st_mode,statbuf->st_uid, statbuf->st_gid));

  if (statbuf->st_mode & S_IXOTH) return(1);
  if (statbuf->st_mode & S_IXUSR && statbuf->st_uid == __current->uid) return(1);
  if (statbuf->st_mode & S_IXGRP && statbuf->st_gid == __current->egid) return(1);

  if (statbuf->st_mode & S_IXGRP) 
    if (__is_in_group (statbuf->st_gid)) {
      return 1;
    } else {
      return 0;
    }
  return(0);
}

int 
access(const char *path, int mode) {
    struct stat statbuf;
    int status;

    OSCALLENTER(OSCALL_access);
    START(fd,access);
    DPRINTF(SYS_LEVEL,("access %s 0x%x: entering\n",path,mode));

    status = stat(path, &statbuf);
    if (status != 0) {
      OSCALLEXIT(OSCALL_access);
      return status;
    }
    if (mode == F_OK) {
      OSCALLEXIT(OSCALL_access);
      return 0;
    }
    if (!((R_OK | W_OK | X_OK) & mode)) {
      errno = EINVAL; 
      OSCALLEXIT(OSCALL_access);
      return -1;
    } else if (((mode & R_OK) ? scheck_read_permission(&statbuf) : 1) &&
	       ((mode & W_OK) ? scheck_write_permission(&statbuf) : 1) &&
	       ((mode & X_OK) ? scheck_execute_permission(&statbuf) : 1)) {
      STOP(fd,access);
      OSCALLEXIT(OSCALL_access);
      return 0;
    } else {
      STOP(fd,access);
      errno = EACCES;
      OSCALLEXIT(OSCALL_access);
      return -1;
    }
}

int 
pr_fd(int fd) {
    char line[20];
    struct file *filp;
    sprintf(line,"pr_fd %d:",fd);
    CHECKFD(fd, -1);
    filp = __current->fd[fd];
    pr_filp(filp,line);
    return 0;
}

/* recvmsg and sendmsg routines for PAN on exopc. */

#if 0
#include <stdio.h>
#include <errno.h>
#include <iovec.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

/* Receive a message as described by MESSAGE from socket FD.
   Returns the number of bytes read or -1 for errors. */

int recvmsg(int fd, struct msghdr *msg, int flags)
{
#if IP_FRAG_WORKS
  int bytes_read = 0;
#endif
  int i = 0;
  int count, error;
  char *buf;
  struct msgspec {
    int pkts;
    int length;
  } incoming;
  char *pbuf;


#if IP_FRAG_WORKS
/**
*** DM 4/1/99: This variant is not fully debugged. Once IP frag is fixed this
*** should be checked.
**/

  for(i = 0 ; i < msg->msg_iovlen ; i++)
    bytes_read += msg->msg_iov[i].iov_len;

  if(msg->msg_iovlen > 1) {
    buf = (char *)malloc(bytes_read);
    if(!buf)
      return -1;
  }
  else
    buf = msg->msg_iov[0].iov_base;

  count = error = recvfrom(fd, 
		   buf, 
		   bytes_read,
		   flags,
		   (struct sockaddr *)msg->msg_name, 
		   &msg->msg_namelen);

  if(msg->msg_iovlen > 1) {
    i = 0;
    while(count > 0) {
      memcpy(msg->msg_iov[i].iov_base, buf, msg->msg_iov[i].iov_len);
      count -= msg->msg_iov[i].iov_len;
      buf += msg->msg_iov[i].iov_len;
      i++;
    }
  }
#else
  count = error = recvfrom(fd, 
		   (char *)&incoming, 
		   sizeof(struct msgspec),
		   flags,
		   (struct sockaddr *)msg->msg_name, 
		   &msg->msg_namelen);

  incoming.pkts = ntohs(incoming.pkts);
  incoming.length = ntohs(incoming.length);

  if(incoming.length > msg->msg_iov[0].iov_len) {
    buf = (char *)malloc(incoming.length);
    if(!buf)
      return -1;
  }
  else
    buf = msg->msg_iov[0].iov_base;

  pbuf = buf;
  error = 0;

  do {
    count = recvfrom(fd, 
		     pbuf, 
		     incoming.length,
		     flags,
		     (struct sockaddr *)msg->msg_name, 
		     &msg->msg_namelen);
    error += count;
    pbuf += count;
  } while(--incoming.pkts);

  count = error;

  if(incoming.length > msg->msg_iov[0].iov_len) {
    i = 0;
    while(count > 0) {
      memcpy(msg->msg_iov[i].iov_base, buf, msg->msg_iov[i].iov_len);
      count -= msg->msg_iov[i].iov_len;
      buf += msg->msg_iov[i].iov_len;
      i++;
    }
  }
#endif
  return error;
}

/**
*** Added by DM 10/9/98
**/

extern int fd_udp_sendtov(struct file *, struct ae_recv *, int, 
	      unsigned, struct sockaddr *, int);
extern int fd_udp_sendto(struct file *, void *, int, int, unsigned, 
			 struct sockaddr *, int);

/* Send a message described MESSAGE on socket FD.
   Returns the number of bytes sent, or -1 for errors.	*/
#if IP_FRAG_WORKS
/**
*** DM 4/1/99: This variant is not fully debugged. Once IP frag is fixed this
*** should be checked.
**/
ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
  volatile int i;
  struct ae_recv msg_recv;
  int error;

  struct file * filp;

  OSCALLENTER(OSCALL_sendmsg);
  START(fd,sendmsg);

  CHECKFD(fd, OSCALL_sendmsg);

  filp = __current->fd[fd];

  error = CHECKOP(filp,sendto);
  if (error == NULL) {
    errno = ENOTSOCK;
    OSCALLEXIT(OSCALL_sendmsg);
    return -1;
  }
  /* should be able to handle errors: 
     EINVAL, EMSGSIZE, ENOBUFS, EWOULDBLOCK, EINTR, EFAULT */
  lock_filp(filp);

  /**
  *** DM 10/7/98
  ***
  *** This routine can be written in terms of sendto, it's just a matter of 
  *** unpacking some of the arguments and passing them on.
  **/
  error = fd_udp_sendtov(filp, 
			      &msg_recv,
			      1,
			      flags,
			      (struct sockaddr *)msg->msg_name, 
			      msg->msg_namelen);



  if(msg->msg_iovlen >= AE_RECV_MAXSCATTER) {
    errno = EMSGSIZE;
    return -1;
  }

  msg_recv.n = msg->msg_iovlen;
  for(i = 0 ; i < msg->msg_iovlen; i++) {
    msg_recv.r[i].data = (msg->msg_iov[i]).iov_base;
    msg_recv.r[i].sz = (msg->msg_iov[i]).iov_len;
  }

  error = fd_udp_sendtov(filp, 
			      &msg_recv,
			      1,
			      flags,
			      (struct sockaddr *)msg->msg_name, 
			      msg->msg_namelen);
  
  unlock_filp(filp);

  STOP(fd,sendmsg);
  OSCALLEXIT(OSCALL_sendmsg);

  return error;
}
#else

ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
  volatile int i;
  struct ae_recv msg_recv;
  int error;
  struct msgspec {
    int pkts;
    int length;
  } outgoing;
  volatile int total = 0, packets = 0;
  volatile int index = 0, offset = 0;

  struct file * filp;

  OSCALLENTER(OSCALL_sendmsg);
  START(fd,sendmsg);

  CHECKFD(fd, OSCALL_sendmsg);

  filp = __current->fd[fd];

  error = CHECKOP(filp,sendto);
  if (error == NULL) {
    errno = ENOTSOCK;
    OSCALLEXIT(OSCALL_sendmsg);
    return -1;
  }
  /* should be able to handle errors: 
     EINVAL, EMSGSIZE, ENOBUFS, EWOULDBLOCK, EINTR, EFAULT */
  lock_filp(filp);

  /**
  *** DM 10/7/98
  ***
  *** This routine can be written in terms of sendto, it's just a matter of 
  *** unpacking some of the arguments and passing them on.
  **/
  if(msg->msg_iovlen >= AE_RECV_MAXSCATTER) {
    errno = EMSGSIZE;
    return -1;
  }

  for(i = 0 ; i < msg->msg_iovlen; i++) {
    total += (msg->msg_iov[i]).iov_len;
  }

  outgoing.pkts = total / 1200;
  if(total % 1200 > 0) outgoing.pkts++;
  
  packets = outgoing.pkts;
  outgoing.pkts = htons(outgoing.pkts);
  outgoing.length = htons(total);
    
  error = fd_udp_sendto(filp, 
			(char *)&outgoing, 
			sizeof(struct msgspec),
			1,
			flags,
			(struct sockaddr *)msg->msg_name, 
			msg->msg_namelen);

  error = errno = 0;
  do {
    total = 0;
    msg_recv.n = 0;
    
    for(i = index ; i < msg->msg_iovlen; i++) {
      msg_recv.r[msg_recv.n].data = (msg->msg_iov[i]).iov_base + offset;

      if(total + (msg->msg_iov[i]).iov_len - offset > 1200) {
	msg_recv.r[msg_recv.n].sz = 1200 - total;
	offset += msg_recv.r[msg_recv.n++].sz;
	index = i;
	break;
      }
      else
	msg_recv.r[msg_recv.n].sz = (msg->msg_iov[i]).iov_len - offset;
      total += msg_recv.r[msg_recv.n].sz;
      offset = 0;
      msg_recv.n++;
    }

    error += fd_udp_sendtov(filp, 
			   &msg_recv,
			   1,
			   flags,
			   (struct sockaddr *)msg->msg_name, 
			   msg->msg_namelen);
  } while (--packets);

  unlock_filp(filp);

  STOP(fd,sendmsg);
  OSCALLEXIT(OSCALL_sendmsg);

  return error;
}
#endif
    
#if 0

FDs		FS	Socket	Implemented
socket			x	x
bind			x	x
listen			x
accept			x
connect			x
read		x	x
write		x	x
recv			x
send			x
recvfrom		x	x
sento			x	x
recvmsg			x	x
sendmsg			x	x
close			x	x
shutdown		x
select		x	x	x
fcntl		x	x	x
open		x
creat		x
lseek		x
stat		x
dup		x	x
pipe			x
link		x
unlink		x
mount
unmount
chdir		x
chown		x
chmod		x
chroot		x
mknod		x 
#endif
