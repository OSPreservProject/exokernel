
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

#include "stdlib.h"
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <exos/debug.h>

#include <exos/conf.h>
#include <exos/process.h>
#include <exos/osdecl.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <exos/vm-layout.h>
#include <exos/net/ae_net.h>
#include <exos/vm.h>
#include <xok/sys_ucall.h>

#include <xok/ash.h>

#include <malloc.h>
#include "fd/proc.h"
#include "fd/path.h"
#include <fd/procaux.h>


struct proc_struct *__current = (struct proc_struct *)0;

/* Called at the end of every process,
 * This is assured by calling OnExec at the initialization of the first 
 * process, and every other process thereafter.
 * This procedures copies its current structure to its child to be.
 */
/* shares current into the child process as COW */
int 
ExecProcInit(u_int k, int newenvid, int execonly) {
  u_int num_completed = 0;
  /* kprintf("ExecProcInit\n"); */
  assert(__current);
  StaticAssert(sizeof(struct proc_struct) <= PROC_STRUCT_SZ);

  if (_exos_insert_pte_range(k, &vpt[PGNO(__current)],
			     PGNO(PGROUNDUP((u_int)__current +
					    sizeof(struct proc_struct))) -
			     PGNO(__current), PROC_STRUCT, &num_completed, k,
			     newenvid, 0, NULL) < 0 ||
      sys_mod_pte_range(k, PG_COW, PG_W | PG_RO, PGROUNDDOWN((u_int)__current),
			PGNO(PGROUNDUP((u_int)__current +
				       sizeof(struct proc_struct))) -
			PGNO((u_int)__current), k, newenvid) < 0 ||
      sys_self_mod_pte_range(k, PG_COW, PG_W | PG_RO,
			     PGROUNDDOWN((u_int)__current),
			     PGNO(PGROUNDUP((u_int)__current +
					    sizeof(struct proc_struct))) -
			     PGNO((u_int)__current)) < 0 ||
      _exos_insert_pte(k, vpt[PGNO(MOUNT_SHARED_REGION)], MOUNT_SHARED_REGION,
		       k, newenvid, 0, NULL) < 0) {
    kprintf ("ExecProcInit failed!\n");
    return (-1);
  }
  
  return 0;
}
    
/* Called once by the very first process.  No other time after that.
 * initializes the current structure, zeroes function vectors, and file
 * descriptors 
 * 
 * current is copied to the child process by ExecProcInit
 */
int
FDFreshProcInit(void) {
    int i;
    int NumPages = PGROUNDUP(sizeof (struct proc_struct))/NBPG;
    u_int page;
    u_int k = 0; /* key */
    /* we want to map into the child and parent where
       the proc_struct and the fd vector will go*/
    
    page = PROC_STRUCT;
    while (NumPages-- >= 0) {
      if (_exos_self_insert_pte (k, PG_U|PG_W|PG_P, page, 0, NULL) < 0) {
	kprintf("FDFreshProcInit: _exos_self_insert_pte failed\n");
	return -1;
      }
      page += NBPG;
    }

    /* allocate a page for the mount table...this will be inhereted
       across forks because of the PG_SHARED and mapped in before
       exec's by ExecProcInit above. */

    StaticAssert (MOUNT_SHARED_REGION_SZ == NBPG);
    if (_exos_self_insert_pte (k, PG_U|PG_W|PG_P|PG_SHARED, MOUNT_SHARED_REGION,
				 0, NULL) < 0) {
      assert (0);
    }
    
    __current = (struct proc_struct *) PROC_STRUCT;
    
    __current->pid = envidx(__envid);
    __current->pgrp = __current->pid;
    __current->session = 0;
    __current->leader = 0;
    __current->uid = 0;
    __current->euid = __current->uid;
    __current->fsuid = __current->uid;
    __current->gid = 100;
    __current->egid = __current->gid;
    __current->groups[0] = __current->gid;
    __current->groups[1] = GROUP_END;
    memset((char *)&__current->logname[0],0,MAXLOGNAME);
    __current->timeout = 0;
    __current->link_count = 0;
    __current->umask = 022;
    __current->root = (struct file *) 0;
    __current->cwd = (struct file *) 0;
    __current->cwd_isfd = 0;
    __current->count = 0;

    for(i = 0; i < SUPPORTED_OPS; i++)
	__current->fops[i] = (struct file_ops *) 0;

    for(i = 0; i < NR_OPEN; i++) {
	__current->fd[i] = (struct file *) 0;
	__current->cloexec_flag[i] = 0;
    }

    return 0;
    /* move stuff from proc_init */
}

/* Updates the ref count of the objects pointed by the fd vector of
 * the current process.  This is called by the parent before creating 
 * (via exec or fork) a new process, therefore there are no race conditions.
 */

static int 
UpdateRefCount(void) {
    int fd;

    /* kprintf("Update Ref Count PID: %d\n",getpid()); */
    for (fd = 0 ; fd < NR_OPEN ; fd++) {
	if (__current->fd[fd] != NULL) {
/*	  kprintf("fd: %d is good, filp: %08x\n",fd,(int)__current->fd[fd]);*/
	  lock_filp(__current->fd[fd]);
	  filp_refcount_inc(__current->fd[fd]);
	  unlock_filp(__current->fd[fd]);
	}
    }
    if (__current->cwd) {
				/* when I get back will implement a lock
				 * for ref counts and another for the other
				 * structures, because otherwise you get
				 * into deadlocks */
				/*       lock_filp(__current->cwd); */
      filp_refcount_inc(__current->cwd);
				/*       unlock_filp(__current->cwd); */
    } else {
      /* kprintf("WARNING NO CWD\n"); */
    }

    if (__current->root) {
				/* when I get back will implement a lock
				 * for ref counts and another for the other
				 * structures, because otherwise you get
				 * into deadlocks */
				/*       lock_filp(__current->root); */
      filp_refcount_inc(__current->root);
				/*       unlock_filp(__current->root); */
    } else {
      /* kprintf("WARNING NO ROOT\n"); */
    }
    return 0;
}
/* To be used with OnExec */
static int 
UpdateRefCountE(u_int k, int envid, int execonly) {
  if (execonly) return 0;
  return UpdateRefCount();
}

int
CloseOnExecFD(void) {
    int fd;

    for (fd = 0 ; fd < NR_OPEN ; fd++) {
	if (__current->fd[fd] != NULL && 
	    __current->cloexec_flag[fd] == 1) {
	    /* 	    (__current->fd[fd]->f_flags & FD_CLOEXEC)) { */
	    /* I theory is not necessary to clear this flag, since whoever
	     * set it is not around anymore. */
	    close(fd);
	}
    }
    return 0;
}

/* Called at the startup of each process including the first one.  if
 * it is the first one is called after FDFreshProcInit.  the 'current'
 * structure is already mapped in.
 */ 
int
FDExecInit(void) {
  extern int ShareAshRegion(u_int k, int envid, int execonly);
  __current = (struct proc_struct *) PROC_STRUCT;
  OnExec(ExecProcInit);
#ifdef ASH_NET
  OnExec(ShareAshRegion);
#endif
  OnExec((OnExecHandler)UpdateRefCountE);
  OnFork((OnForkHandler)UpdateRefCount);

  ftable_init();
  CloseOnExecFD();
  return 0;
}

void (*pr_fd_statp)(void) = NULL;


/* called at the end of a process for clean up, it decrements ref 
 * counts for fds, cwd, and root directory and closes the appropiate
 * ones 
 */
int
FDEndProcess(void) {
  int fd;
  int i;
    int error;
    ISTART(misc,step6);
    if (__current->cwd) {
      lock_filp(__current->cwd);
      filp_refcount_dec(__current->cwd);
      

      if (filp_refcount_get(__current->cwd) == 0) {
	unlock_filp(__current->cwd);
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
    }
    STOPP(misc,step6);
    ISTART(misc,step7);
    if (__current->root) {
      lock_filp(__current->root);
      filp_refcount_dec(__current->root);
      
      if (filp_refcount_get(__current->root) == 0) {
	unlock_filp(__current->root);
	namei_release(__current->root,1);
	putfilp(__current->root);
      } else {
	unlock_filp(__current->root);
      }
    }

    STOPP(misc,step7);
    ISTART(misc,step8);
    for (fd = 3 ; fd < NR_OPEN ; fd++) {
	if (__current->fd[fd] != NULL) {
/*	    kprintf("closing fd: %d (%08x)...",fd,(int)__current->fd[fd]);*/
	    close(fd);
/*	    kprintf("done\n");*/
	}
    }
   
    STOPP(misc,step8);

#ifdef PRINTFDSTAT
    if (pr_fd_statp) pr_fd_statp();
#endif
    
    /*     kprintf("CloseAllFD done\n"); */
    if (__current->fd[0] != NULL) {close(0);}
    if (__current->fd[1] != NULL) {close(1);}
    if (__current->fd[2] != NULL) {close(2);}

    ISTART(misc,step9);
	/* shutdown the thing *after* closing any file descriptors */
    for (i=0; i<SUPPORTED_OPS; i++) {
       if ((__current->fops[i]) && (__current->fops[i]->exithandler)) {
          __current->fops[i]->exithandler();
       }
    }
    STOPP(misc,step9);

    return 0;
}


