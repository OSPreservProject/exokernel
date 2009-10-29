
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

#ifndef _PROC_H_

#define _PROC_H_

#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <exos/locks.h>
#include <xok/wk.h>
#include <xok/disk.h>
#include "fdstat.h"

/* debugging levels:
 1. system call level, open close
 2. cluster call level, socket_read, nfs_read etc..
 3. cluster call level one deep, ie helpers to socket read etc
 4. Unused for now
 5. Unused for now
 6. system call helpers like getfd etc...
 7. locks
 */


#define NR_OPEN 128		/* # of max fd's */
#define NR_FTABLE 256		/* # of files open among all processes */
#define NR_RSVRD_FTABLE 60      /* # of files in ftable reserved for root processes */

#undef FD_SETSIZE
#define FD_SETSIZE NR_FTABLE	/* for fd_set type */

#define NR_SOCKTYPES 4		/* # of socket types, just udp,udp,tcp,tcp */
#define GROUP_END -1
#define NO_OPS -1


#define OUTFD -2
#define NOFILPS -3

#define FD_SHM_OFFSET 789213	/* dont use 789212 because I am using that
				 * too */
#define NR_OPS 11		/* # of operations: tty, udp, nfs */
#define SUPPORTED_OPS NR_OPS
#define UDP_SOCKET_TYPE 0
#define PTY_TYPE        1
#define NFS_TYPE        2
#define TCP_SOCKET_TYPE 3
#define CONSOLE_TYPE    4
#define PIPE_TYPE       5
#define NULL_TYPE       6
#define NPTY_TYPE       7
#define DUMB_TYPE       8
#define CFFS_TYPE       9
#define CDEV_TYPE       10

#define DEV_DEVICE 63		/* st_dev of dev devices like null,dumb,console... */

/* types for the select filp operation */
#define SELECT_READ	1
#define SELECT_WRITE	2
#define SELECT_EXCEPT	3

#define BLOCKING 1
#define NONBLOCKING 0
/* for return value of file_ops.select */
#define FILE_DATA_SIZE 12

#define O_DONOTRELEASE 0x8000	/* used by namei to indicate that a
				 * filp should not be released */
#define O_USECLOSE 0xf0000      /* by namei to indicate that a
				 * filp is not to be released, instead
				 * close should be used */




/* if the file is unlocked, then the flock_envid field has no meaning,
 * otherwise it is the envid that holds _THE_ exclusive lock, or the
 * envid of _ONE OF_ the env's that hold a shared lock 
 */

#define IS_FILP_FLOCKED(filp) (((filp)->flock_state == FLOCK_STATE_EXCLUSIVE) || ((filp)->flock_state == FLOCK_STATE_SHARED))

#define FLOCK_STATE_UNLOCKED 0  
#define FLOCK_STATE_EXCLUSIVE 1
#define FLOCK_STATE_SHARED 2


struct file {
  mode_t f_mode;		/* file mode like FILE/DIR 0666 etc */
  off_t f_pos;			/* offset */
  dev_t  f_dev;              	/* set at allocation time */
  ino_t f_ino;			/* set at allocation time */
  unsigned short f_flags;	/* how the file was opened ie O_APPEND etc */
  unsigned short ff_count;	/* how many people have this file open */
  pid_t f_owner;                /* pid or -pgrp where SIGIO should be sent */
  int op_type;			/* index into file_ops vector */
  u_int flock_envid;
  u_int flock_state;
  exos_lock_t lock;
  unsigned char data[FILE_DATA_SIZE]; /* partial-private data for file */
};

static inline void
filp_refcount_init(struct file *filp)
{
  filp->ff_count = 0;
}

static inline void
filp_refcount_inc(struct file *filp)
{
  filp->ff_count++;
}

static inline void
filp_refcount_dec(struct file *filp)
{
  filp->ff_count--;
}

static inline u_int
filp_refcount_get(struct file *filp)
{
  return filp->ff_count;
}

struct file_ops {
    int (*open)         (struct file *dir , struct file *where, char *name,
			 int flags, mode_t mode);
    off_t (*lseek)        (struct file *from, off_t new, int whence);
    int (*read)         (struct file *from, char *buffer, int nbyte, 
			 int blocking);
    int (*write)        (struct file *from, char *buffer, int nbyte, 
			 int blocking);
    int (*select)       (struct file *from,int rw);
    int (*select_pred)  (struct file *from, int rw, struct wk_term *pred);
    int (*ioctl)        (struct file *from, unsigned int request,
			 char *argp); 
    int (*close0)        (struct file *from, int fd); /* called anytime there is
						a close; useful for gc.*/
    int (*close)        (struct file *from); /* called once the last reference to
						the object is closed*/
  /* note close0 is called anytime a close is done, is also called on
     the closing of the last reference before the calling of close */
    
    /* lookup - will take a locked directory filp and 
       should return a locked file structure.  */
    int (*lookup)       (struct file *from, const char *name,struct file *to); 
    int (*link)         (struct file *to, struct file *dir, 
			 const char *name); 
    int (*symlink)      (struct file *dir, const char *name, const char *to);
    int (*unlink)       (struct file *dir, const char *name);
    int (*mkdir)        (struct file *dir, const char *dirname,mode_t);
    int (*rmdir)        (struct file *dir, const char *dirname);
    int (*mknod)        (struct file *dir, const char *name, int,int,int);
    int (*rename)       (struct file *dirfrom, const char *namefrom,
			 struct file *dirto,   const char *nameto); 
    int (*readlink)     (struct file *filp, char *buffer, int bufsize);
    int (*follow_link)  (struct file *,int,int,struct file **);
    int (*truncate)     (struct file *, off_t length);
    int (*dup)          (struct file *oldsock);
    
    /* release - releases a file pointer locked by lookup or acquire */
    int (*release)      (struct file *filp);
    int (*acquire)      (struct file *filp);
    int (*bind)         (struct file *sock, struct sockaddr *umyaddr,
			 int sockaddr_len);
    int (*connect)      (struct file *sock, struct sockaddr *uservaddr,
			 int sockaddr_len, int flags);
    int (*filepair)     (struct file *sock1, struct file *sock2);
    int (*accept)       (struct file *sock, struct file *newsock, 
			 struct sockaddr *newsockaddr, 
			 int *addr_len, int flags);
    int (*getname)      (struct file *sock, struct sockaddr *uaddr,
			 int *usockaddr_len, int peer);
    int (*listen)       (struct file *sock, int len);
    int (*sendto)       (struct file *sock, void *buff, int len, int nonblock,
                         unsigned flags, struct sockaddr *, int addr_len);
    int (*recvfrom)     (struct file *sock, void *buff, int len, int nonblock,
                         unsigned flags, struct sockaddr *, int *addr_len);
    int (*shutdown)     (struct file *sock, int flags);
    int (*setsockopt)   (struct file *sock, int level, int optname,
			 const void *optval, int optlen);
    int (*getsockopt)   (struct file *sock, int level, int optname,
			 void *optval, int *optlen);
    int (*fcntl)        (struct file *sock, int cmd, int arg);    
    int (*mount)        (struct file *from, struct file *to);
    int (*unmount)      (struct file *from);

    int (*chmod)        (struct file *filp, mode_t mode);
    int (*chown)        (struct file *filp, uid_t owner, gid_t group);
    int (*stat)         (struct file *filp, struct stat *buf);
    int (*getdirentries)      (struct file *from, char *buffer, int nbyte);
    /*     int (*permission) (struct file *, int); */
  /* can assume that *times is a valid pointer to a timeval. */
    int (*utimes)	(struct file *filp, const struct timeval *times);
    int (*bmap)		(struct file *filp, u_int *diskBlockHighP, 
			 u_int *diskBlockLowP, off_t offset, u_int *seqBlocksP);
    int (*fsync)	(struct file *filp);
    void (*exithandler)	();
};

struct proc_struct {
  pid_t pid,ppid,pgrp,session,leader;
  gid_t groups[NGROUPS + 1];
  char logname[MAXLOGNAME];
  unsigned short fsuid;	/* this is for NFS for now. */
  unsigned short uid,euid;
  unsigned short gid,egid;
  unsigned long timeout;
  int link_count;
  unsigned short umask;
  struct file * root;
  struct file * cwd;
  int cwd_isfd;
  int count;
  struct file * fd[NR_OPEN];
  char cloexec_flag[NR_OPEN];
  struct file_ops *fops[SUPPORTED_OPS];

  int regids[SUPPORTED_OPS];	/* contains the first region id of
				   each module */
  int regid_filetable;		/* filetable regid */
  int regid_mounttable;		/* mounttable regid */
  int regid_extra1;		/* just in case we need them later, */
  int regid_extra2;		/* so no need to recompile */
  int regid_extra3;
};
extern struct proc_struct *__current;

#define REMOTEDEVSTART -1
#define NR_MOUNT 8		/* maximum number of mounted filesystems */
#define	MNTNAMELEN	90	/* length of buffer for returned name */

/* HB - Used by fd/mount.c */
struct mounttable {
  exos_lock_t lock;
  short nr_mentries;		/* valid mentries */
  struct mount_entry {
    short f_flags;		/* copy of mount flags */
    char inuse;
    char f_mntonname[MNTNAMELEN];	  /* directory on which mounted */
    char f_mntfromname[MNTNAMELEN];  /* mounted file system */
    struct file from;
    struct file to;
    unsigned int sblk;		/* optional superblock param */
  } mentries[NR_MOUNT];
};

typedef struct mount_entry mount_entry_t;
typedef struct mounttable mounttable_t;

#define FILPEQ(x,y) ((x)->f_ino == (y)->f_ino && (x)->f_dev == (y)->f_dev)
struct global_ftable {
  struct file fentries[NR_FTABLE]; /* file entries */
  exos_lock_t lock;
  exos_lock_t flock_lock;
  exos_lock_t cffs_lock;	/* global lock for all of cffs */
  dev_t remotedev;		/* remote devices for nfs dev */
  fd_set inuse;			/* if file entry is inuse */

  u_int inited_disks[MAX_DISKS];
  u_int mounted_disks[MAX_DISKS];
  struct mounttable *mt;		
  u_int nsd_envid;		/* env id of the system wide environment name server */
  u_int counter0;
  u_int counter1;
  u_int counter2;
};
extern struct global_ftable *global_ftable;

#define EQDOT(next) \
     ((next)[0] == '.' && (next)[1] == (char)NULL)
#define EQDOTDOT(next) \
     ((next)[0] == '.' && (next)[1] == '.' && (next)[2] == (char)NULL)


int 
register_family_type_protocol(int family, int type, int protocol,     
			      int (*socket)(struct file *));
int 
register_file_ops(struct file_ops *fops, int type);

void
clear_ftable_lock(void);
void 
lock_ftable(void);
void
unlock_ftable(void);

void
clear_filp_lock(struct file * filp);
void 
lock_filp(struct file * filp);
void
unlock_filp(struct file * filp);

/* the bmap function */
int 
bmap(int fd, u_quad_t *diskBlockp, off_t offset, u_int *seqBlocksP);

/* in namei.c */
int 
namei(struct file *relative,
      const char *path, struct file *result_filp, int flags);
/* for open */
int 
nameio(struct file *relative,const char *path, 
       struct file *result_filp, char *result_path, int followlinks);
/* for releasing filps obtained via namei */
int 
namei_release(struct file *filp, int force);

void
namei_cache_remove(struct file *old_filp,char *name, struct file *newfilp);

int
isbusy_fs(dev_t dev, int fs);

/* in groups.c */
int __is_in_group (gid_t gid);
void __copy_groups (gid_t *out, int maxlen);

/* in proc.c */
char *filp_to_string(struct file *, char *, int);
void dump_file_structures(FILE *); 


#define NI_FOLLOWLINKS 0x0001
#define NI_OPEN        0x0002

extern void putfd(int fd);
extern int getfd(void);

extern struct file *
getfilp(void);

extern void
putfilp(struct file * filp);

extern int
fd_shm_alloc(key_t seg, int size, char *location);

/* CHECKFD - verifies the validity of a FD, if bad sets the right error */
#define CHECKFD(x, y) {						\
  if ((x) < 0 || (x) > NR_OPEN || __current->fd[(x)] == NULL) {	\
    errno = EBADF;						\
    if (y >= 0) OSCALLEXIT(y);					\
    return -1;							\
  }								\
}


/* CHECKOP - makes sures the optype for the filp is valid and
 tests the existance of operation within that optype. */

#define CHECKOP(filp,operation)						 \
  ((filp) && 								 \
   ((filp)->op_type >= 0 && (filp)->op_type < NR_OPS) && 		 \
   __current->fops && 							 \
   __current->fops[(filp)->op_type] && 					 \
   __current->fops[(filp)->op_type]->operation)				

#define CHECKNB(filp) \
    ((filp->f_flags & O_NDELAY) || (filp->f_flags & O_NONBLOCK))

/* executes operation of filp.  Assumes existance of operation 
   ie. CHECKOP has been done before */
extern void *current_file_op;

#define DOOP(filp,operation,args) 			\
({int x; START(fd_op[(filp)->op_type],operation);	\
 x = DOOPNT(filp,operation,args);			\
 STOP(fd_op[(filp)->op_type],operation);		\
 x;})

#define DOOPNT(filp,operation,args)					 \
  (current_file_op = (void *)&__current->fops[(filp)->op_type]->operation, \
   (__current->fops[(filp)->op_type]->operation)args)



#define SWAP_FILP(filpa,filpb) {struct file *tmp; \
				    tmp = filpa; filpa = filpb; filpb = tmp;}

extern void
pr_filp(struct file *filp,char *label);
extern char *
filp_op_type2name(int t);

#if 0
#define PRSHORTFILP(prelabel,filp,postlabel)				\
  printf("%s dev: %d ino: %d cn:%d d:%d %s",				\
	 prelabel,(filp)->f_dev,(filp)->f_ino,filp_refcount_get(filp),	\
	 ((filp)->f_flags & O_DONOTRELEASE)?1:0,postlabel)

#else
#define PRSHORTFILP(a,b,c)
#endif

#endif /* _PROC_H_ */

			

