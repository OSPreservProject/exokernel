
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

#if 0
#undef PRINTF_LEVEL
#define PRINTF_LEVEL 1
#endif

#include "fd/proc.h"
#include "fd/path.h"
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

/* unistd stuff */
#define SEEK_CUR 1
extern int close(int);

#include <dirent.h>
#include <assert.h>
#include <malloc.h>

#include "sys/shm.h"
#include "sys/ipc.h"
#include <exos/vm-layout.h>
#include <exos/process.h>
#include <xok/mmu.h>
#include <xok/sysinfo.h>

#include <fd/modules.h>
#include <exos/debug.h>

#include <exos/regions.h>

#if 0
#define PR fprintf(stderr,"%s: %d\n",__FILE__,__LINE__);
#else
#define PR
#endif

struct global_ftable *global_ftable;
static int fd_setup_dumb(void);

/* clear_ftable_lock - initializes the ftable lock, should only be used
 once, implemented naively for now */
void
clear_ftable_lock(void) { 
    DPRINTF(LOCK_LEVEL,("clear_ftable_lock\n"));
    exos_lock_init(&global_ftable->lock);
}

/* lock_ftable, unlock_ftable - lock the ftable so the no one else can
   allocate or deallocate filps but they could for example change
   reference counts, if we want more strict semantics we can change
   lock_ftable.  Implemented naively for now. */
void 
lock_ftable(void) { 
    DPRINTF(LOCK_LEVEL,("lock_ftable\n"));
    exos_lock_get_nb(&(global_ftable->lock));
}

void
unlock_ftable(void) { 
    DPRINTF(LOCK_LEVEL,("unlock_ftable\n"));
    exos_lock_release(&global_ftable->lock); 
}

/* ftable_init - initializes the global_ftable:
 - clears the lock
 - zeros filp freemap
 */

ssize_t 
write(int fd, const void *buf, size_t nbyte);

void
ftable_init(void)     
{
    int error;
    int status;
    int i;
    extern int dumb_terminal_init();
    extern int mount_init(void);
    extern int namei_init(void);
    extern int dma_init(void);


    START(ftable,init);
    DPRINTF(SYS_LEVEL,("ftable_init entering\n"));
    namei_init();
    dma_init();
    status = fd_shm_alloc(FILP_SHM_OFFSET,
			   (sizeof(struct global_ftable)),
			   (char *)FILP_SHARED_REGION);
    StaticAssert((sizeof(struct global_ftable)) <= FILP_SHARED_REGION_SZ);

    global_ftable = (struct global_ftable *) FILP_SHARED_REGION;
    if (status == -1) {
	demand(0, problems attaching shm);
    }

    /* we should have inherited the mount table from our parent */
    assert ((vpt[PGNO(MOUNT_SHARED_REGION)] & (PG_U|PG_P|PG_W)) == (PG_U|PG_P|PG_W));
    StaticAssert (sizeof (struct mounttable) <= MOUNT_SHARED_REGION_SZ);
    global_ftable->mt = (struct mounttable *)MOUNT_SHARED_REGION;

    for (i = 0; i < SUPPORTED_OPS; i++) {
      __current->fops[i] = (struct file_ops *)0;      
    }
    if (status) {
      /* printf("Initializing Filp Table shared data structture\n");*/

      /* Setup the dma regions, the first NR_FTABLE regions starting at
	 regid_filetable belong to file entries, the last region
	 belongs to the other data in the global ftable.
       */
      dma_ctrlblk_t c;
      dma_region_t r[NR_FTABLE + 1];
      int i;
#define FTABLE_KEY 20
      for (i = 0; i < NR_FTABLE; i++) {
	r[i].key = FTABLE_KEY; r[i].reg_addr = &global_ftable->fentries[i];
	r[i].reg_size = sizeof(struct file);
      }
      r[NR_FTABLE].reg_addr = &global_ftable->fentries[NR_FTABLE];
      r[NR_FTABLE].reg_size = 
	sizeof(struct global_ftable) - (sizeof(struct file)* NR_FTABLE);
      r[NR_FTABLE].key = FTABLE_KEY;
      c.nregions = NR_FTABLE + 1;
      c.dma_regions = &r[0];
      status = dma_setup_append(&c);
      assert(status >= 0);
      __current->regid_filetable = status;

      clear_ftable_lock();
      exos_lock_init (&global_ftable->cffs_lock); /* XXX temp temp temp */
      FD_ZERO(&global_ftable->inuse);
      error = dumb_terminal_init();
      demand(!error, dumb_terminal_init failed);
      fd_setup_dumb();
/*
      write(1,"fd 1 to the console\n",20);
*/
      error = mount_init();
      global_ftable->counter0 = 0;
      global_ftable->counter1 = 0;
      global_ftable->counter2 = 0;
      demand(!error, mount_init failed);
      /* initialize envid stuff */
      global_ftable->nsd_envid = 0;

      for (i = 0; i < MAX_DISKS; i++) {
	global_ftable->mounted_disks[i] = 0;
	global_ftable->inited_disks[i] = 0;
      }
    } else {
      /* printf("This is not first process, just attaching memory\n");*/ 
      error = dumb_terminal_init();
      demand(!error, dumb_terminal_init failed);
    }
    /* we must initialize pty first otherwise if we use
     * ptys before (say in a printf) we would seg fault 
     */
    for (i = 0; start_modules[i].func != 0 ;i++) {
      error = start_modules[i].func();
      if (error < 0) {
	printf("error initializing module %s\n",start_modules[i].name);
	assert(0);
      }
    }
#if 0
    fprintf(stderr,"PROGRAM: %s pid: %d\n",__progname,getpid());
#endif
    STOP(ftable,init);
}


/* getfilp - allocates a global file pointer from global_ftable */
struct file *
getfilp(void)
{
    int i;
    int upper;
    START(ftable,getfilp);
#if 0
    DPRINTF(SYSHELP_LEVEL,("getfilp:\n"));
	    DPRINTF(SYS_LEVEL,("before pr_ftable: %d\n",pr_ftable()));
#endif


    StaticAssert(NR_FTABLE >= NR_RSVRD_FTABLE);
    upper = NR_FTABLE;

    if (getuid() != 0) 
      /* The upper NR_RSVRD_FTABLE entries are reserved for root processes.
       * Note, that by reserving the upper entries of the ftable
       * we ensure that they will be used only if the ftable is otherwise 
       * full.
       */
      upper -= NR_RSVRD_FTABLE;

    lock_ftable();
    for (i = 0; i < upper; i++)
	if (!(FD_ISSET(i,&global_ftable->inuse)))
	{
	    DPRINTF(SYSHELP_LEVEL,
		    ("found a filp, fentry[%d]\n",i));
	    FD_SET(i,&global_ftable->inuse);
	    clear_filp_lock(&global_ftable->fentries[i]);
	    global_ftable->fentries[i].ff_count = 11711;  /* XXX what does this number mean? --josh */
	    global_ftable->fentries[i].flock_state = FLOCK_STATE_UNLOCKED;
	    unlock_ftable();
#if 0
	    DPRINTF(SYS_LEVEL,("after pr_ftable: %d\n",pr_ftable()));
	    DPRINTF(SYS_LEVEL,("getfilp returning: %08x\n",(int)&global_ftable->fentries[i]));
#endif
	    STOP(ftable,getfilp);
	    return(&global_ftable->fentries[i]);
	}
    unlock_ftable();
    DPRINTF(SYSHELP_LEVEL,("getfilp out of global file pointer!\n"));
    fprintf(stderr,"warning getfilp out of global file pointer pid: %d\n",getpid());
    STOP(ftable,getfilp);
    return (struct file *) 0;
}



/* putfilp - deallocates a global file pointer previously allocated
   from getfilp */
void
putfilp(struct file * filp)
{
    int i;
    START(ftable,putfilp);
    DPRINTF(SYSHELP_LEVEL,("putfilp: %08x\n",(int)filp));
    demand(filp, bogus filp);
    if (filp < &global_ftable->fentries[0]) return;
    lock_ftable();
    for (i = 0; i < NR_FTABLE ; i++)
	if (FD_ISSET(i,&global_ftable->inuse))
	{
	    if (&global_ftable->fentries[i] == filp)
	    {
		FD_CLR(i,&global_ftable->inuse);
		unlock_ftable();
		STOP(ftable,putfilp);
		return;
	    }
	}
    unlock_ftable();
    DPRINTF(SYSHELP_LEVEL,("warning, putfilp was given a non-inuse filp\n"));
    STOP(ftable,putfilp);
}


static int
fd_setup_dumb(void) {
  int fd, error;
  struct file *filp;

    fd = getfd();
    if (fd == OUTFD) {
	errno = EMFILE;
	return -1;
    }
    if (fd == NOFILPS) {
	errno = ENFILE;
	return -1;
    }
    filp = __current->fd[fd];

    filp->f_pos = 0;
    filp->f_owner = __current->uid;
    filp->op_type = DUMB_TYPE;
    filp->f_mode = S_IFDIR | 0755;
    filp->f_flags = 0;
    
    if (CHECKOP(filp,open) == 0) {
	demand(filp_refcount_get(filp) == 0, hmm error opening a file and ref count not 0);
	putfd(fd);
	demand(0,This is impossible since open operation is checked at mount time);
	return -1;
    }
    error = DOOP(filp,open,((struct file *)0,filp,"dumb",0,0));
    if (error) {
	DPRINTF(SYSHELP_LEVEL,
		("open: error status = %d, while opening console, ",error));
	demand(filp_refcount_get(filp) == 0, hmm error opening a file and ref count not 0);
	putfd(fd);
	return -1;
    }
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    dup2(fd,0);
    dup2(fd,1);
    dup2(fd,2);
    if (fd > 2) close(fd);
    return 0;
}



/* it allocates in shm, and id segment, a region of size size, and
 * in location location.  it returns 0, on ok, and if it is not the
 * first time that someone allocates this region.  1 if it is the
 * first time, and -1 on an error.
 */
int
fd_shm_alloc(key_t seg, int size, char *location) {
    int SegHandle;
    int first_flag = 0;
    char *where;

    START(misc,shm_alloc);
    //demand(size < FD_SRSZ,possible overlap if larger see vm-layout.h);

    DPRINTF(SYS_LEVEL,("fd_shm_alloc: allocating shm, loc: %08x size: %08x loc+size: %08x\n",(int)location,size, (int)location+size));
	   
    errno = 0;
    SegHandle = shmget(seg,size,0);

    if (SegHandle == -1) {
	if (errno == ENOENT) {
	    SegHandle = shmget(seg,size,IPC_CREAT);
	    assert (SegHandle != -1);
	    first_flag = 1;
	} else {
	    printf("Warning: fd_shm_alloc, could not get shared memory,"
		   " errno: %d\n",errno);
	    demand(0,could not shmget);
	    STOP(misc,shm_alloc);
	    return -1;
	}
    }
	
    START(misc,attach);
    where = shmat(SegHandle,location,0); 
    STOP(misc,attach);
    if (where != location) {
	printf("Error: fd_shm_alloc,shmat returned: %08x, "
	       "extected: %08x, errno: %d\n",(int)where,(int)location,errno);
	demand(0,problems with shmat);
	STOP(misc,shm_alloc);
	return -1;
    }
    STOP(misc,shm_alloc);
    return first_flag;

}

/* Get a device number for remote mounted volumes */
dev_t 
getnewdevicenum(void) {
  dev_t ret = global_ftable->remotedev;
  global_ftable->remotedev--;
  return ret;
}

/*
 * some debugging code 
 */

void 
pr_current() {
  assert(__current);
  printf("current
pid: %d uid: %d gid: %d euid: %d egid: %d
pgrp: %d session: %d leader: %d umask: %d
root: %p cwd: %p\n",
__current->pid,
__current->uid,
__current->gid,
__current->euid,
__current->egid,
__current->pgrp,
__current->session,
__current->leader,
__current->umask,
__current->root,
__current->cwd);
}


int
pr_ftable(void) {
    int i, c = 0,d = 0;
    int using;
    
    printf("FTABLE: lock %d\n",global_ftable->lock.lock);
    printf("counter0: %d\n",global_ftable->counter0);
    printf("counter1: %d\n",global_ftable->counter1);
    printf("counter2: %d\n",global_ftable->counter2);
    printf("remotedev: %d\n",global_ftable->remotedev);

#if 0
   for (i = 0; i < NR_FTABLE ; i++) {
	using = ((int)FD_ISSET(i,&global_ftable->inuse)) ? 1 : 0;
	printf("[%02d]:%d pt %08x ",i,
	       using,(int)&global_ftable->fentries[i]);
	    if (i % 4 == 3) printf("\n");
	    if (FD_ISSET(i,&global_ftable->inuse)) c++;
	}
#endif
    for (i = 0; i < NR_FTABLE ; i++) {
	using = ((int)FD_ISSET(i,&global_ftable->inuse)) ? 1 : 0;
	if (using) {
	  if (global_ftable->fentries[i].op_type == NFS_TYPE) {
//	     || global_ftable->fentries[i].op_type == UDP_SOCKET_TYPE) {
	    printf("\n#ftable entry: %2d\n",i);
	    pr_filp(&global_ftable->fentries[i]," ");
	  }
	}
    }
    printf("root: %p (%d)\ncwd: %p (%d)\n",
	   __current->root,
	   __current->root - global_ftable->fentries ,
	   __current->cwd,
	   __current->cwd - global_ftable->fentries);
    if (__current->root) pr_filp(__current->root,"ROOT");
    if (__current->cwd) pr_filp(__current->cwd,"CWD");
    printf("fds in use:\n");
    for (i = 0; i < NR_OPEN; i++) {
	if (__current->fd[i]) {
	    printf("%02d (%4d):%d ",i,
		   ((int)__current->fd[i] - (int)&global_ftable->fentries[0])/
		   sizeof(struct file),
		   (int)__current->cloexec_flag[i]);
	    if (d % 4 == 3) printf("\n");
	    d++;
	}
    }
    printf("\n");
    return c;
}


int
isbusy_fs(dev_t dev, int fs) {
  int count = 0,i,using;
  //  printf("FTABLE: lock %d\n",global_ftable->lock);
  //  printf("remotedev: %d\n",global_ftable->remotedev);
  
  for (i = 0; i < NR_FTABLE ; i++) {
    using = ((int)FD_ISSET(i,&global_ftable->inuse)) ? 1 : 0;
    if (using && 
	global_ftable->fentries[i].f_dev == dev && 
	global_ftable->fentries[i].op_type == fs) {
      // pr_filp(&global_ftable->fentries[i],"using dev ");
      count++;
    }
  }
  return count;
}

char *filp_op_type2name(int t) {
  char *n;
  switch(t) {
  case UDP_SOCKET_TYPE: n = "UDP"; break;
  case PTY_TYPE: n = "PTY"; break;
  case NFS_TYPE: n = "NFS"; break;
  case TCP_SOCKET_TYPE: n = "TCP"; break;
  case CONSOLE_TYPE: n = "CONSOLE"; break;
  case PIPE_TYPE: n = "PIPE"; break;
  case NPTY_TYPE: n = "NPTY"; break;
  case DUMB_TYPE: n = "DUMB"; break;
  case CFFS_TYPE: n = "CFFS"; break;
    
  default: n = "UNKNOWN"; break;
  }
  return n;
}
