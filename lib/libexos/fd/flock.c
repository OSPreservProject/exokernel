
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

#include <exos/callcount.h>
#include <fcntl.h>
#include <errno.h>
#include <exos/debug.h>
#include <exos/uwk.h>
#include "proc.h"

#define IS_FLOCKED 1 
#define SUCCESS 0
#define FAILURE -1

#define NOT_IN_FTABLE 0
#define UNFLOCKED_AND_IN_FTABLE 1
#define FLOCKED_BY_ALIVE_ENV 2
#define FLOCKED_BY_DEAD_ENV 3


#define FLOCK_ERROR(arg) do { kprintf arg; } while (0)

#if 0
#define DEBUG(arg) do { kprintf arg; } while (0)

static char *state_to_string_tab[] = {"NOT_IN_FTABLE",
				      "UNFLOCKED_AND_IN_FTABLE",
				      "FLOCKED_BY_ALIVE_ENV",
				      "FLOCKED_BY_DEAD_ENV"};


#define DEBUG_BUF_SIZE 60
static char debug_buf[DEBUG_BUF_SIZE]; 

#else
#define DEBUG(arg) 
#endif


/* XXX get rid of this when performance matters */
#define CHECK_INVARIANTS check_invariants


#if 0				/* not used */
/* clear_ftable_flock - initializes the ftable flock lock, should only be used
 once, implemented naively for now -- copied from ftable.c */
static void
clear_ftable_flock(void) { 
    DPRINTF(LOCK_LEVEL,("clear_ftable_flock\n"));
    exos_lock_init(&global_ftable->flock_lock);
}
#endif

/* lock_ftable, unlock_ftable - lock the ftable so the no one else can
   change the "flock_envid" field of the struct file.
   -- copied from ftable.c */
static void 
lock_ftable_flock(void) { 
    DPRINTF(LOCK_LEVEL,("lock_ftable_flock\n"));
    exos_lock_get_nb(&(global_ftable->flock_lock));
}

static void
unlock_ftable_flock(void) { 
    DPRINTF(LOCK_LEVEL,("unlock_ftable_flock\n"));
    exos_lock_release(&global_ftable->flock_lock); 
}


static void flock_dump()
{ 
  dump_file_structures(NULL);
}

static void wait_until_terminates_or_lock_is_released(u_int envid, struct file *filp)
{
  struct wk_term t[ 3*UWK_MKCMP_NEQ_PRED_SIZE + 2 ];
  int wk_sz=0;

  DEBUG(("flock.wait_until_terminates_or_lock_is_released envid: %d waiting for envid: %d on filep %s.\n",
	 __envid, envid, filp_to_string(filp, debug_buf, DEBUG_BUF_SIZE)));

  /* This function assumes two things:
   *
   * 1. When a slot in the ftable gets allocated, its f_dev and f_ino
   * fields are initialized and are thereafter unchanged.  (ie. to be
   * change the slot must be deallocated then reallocated with different
   * values)
   *
   * 2. That a ftable slot will only be deallocated, if there are no
   * refs.
   *
   * Combining these two assumptions, we see that the f_dev and f_ino
   * fields could not have changed from the time-of-check (in acquire)
   * until the time-of-use (which will be when the wake-up predicates
   * are run).
   */


  /* we wait until the env changes to state to something other than ENV_OK */ 
  wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
			     &__envs[envidx(envid)].env_status,
			     ENV_OK, 0);
  wk_sz = wk_mkop (wk_sz, t, WK_OR);

  /* or the env slot gets reallocate...which means the orignal env died */
  wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
			     &__envs[envidx(envid)].env_id,
			     envid, 0);
  wk_sz = wk_mkop (wk_sz, t, WK_OR);
  /* or the lock state changes */
  wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
			    &filp->flock_state,
			    filp->flock_state, 0);

  unlock_ftable_flock();
  unlock_ftable();

  wk_waitfor_pred_directed(t, wk_sz, envid);
}


static int is_env_alive (u_int envid) 
{
  int ret =  ((__envs[envidx(envid)].env_status == ENV_OK)
	      && (__envs[envidx(envid)].env_id == envid));

  DEBUG(("env %d is %s\n", envid, ret ? "ALIVE" : "DEAD"));
  return ret;
}


/*
 * We check that in the whole ftable only one of the follow holds:
 *  a. no locks on <dev, ino>
 *  b. exactly one exclusive lock on <dev, ino>
 *  c. one or more shared locks on <dev, ino>  
 *
 */

static int check_invariant1_helper(int dev, ino_t ino) 
{
  int num_exclusive_locks = 0;
  int num_shared_locks = 0;
  int i;

  for (i=0; i < NR_FTABLE; i++) {
    if (FD_ISSET(i,&global_ftable->inuse)) {
      struct file *filp = &global_ftable->fentries[i];
      if ((filp->f_dev == dev) && (filp->f_ino == ino)) {
	if (filp->flock_state == FLOCK_STATE_SHARED)
	  num_shared_locks++;
	else if (filp->flock_state == FLOCK_STATE_EXCLUSIVE)
	  num_exclusive_locks++;
      }
    }
  }

  if ((num_exclusive_locks == 0 && num_shared_locks >= 0) /* success: covers case a and b, above */
      || (num_exclusive_locks == 1 && num_shared_locks == 0)) { /* success: case c */
    return SUCCESS;
  } else {
    FLOCK_ERROR(("Invariant corrupted: there are %d shared locks and %d exclusive locks help on <dev %d, ino %d>.\n",
		 num_shared_locks, num_exclusive_locks, dev, ino));
    flock_dump();
    return FAILURE;
  }
}

static int check_invariant1(void) 
{
  int i;
  int ret;

  /*
   * We really want to call check_invariant1_helper, on
   * each unique file (ie. <dev,ino>) in the file table.  
   *
   * It is tricky to do _just_ that, so we settle for iterating over
   * the whole file table.  This has the disadvantage of checking the
   * invariant possibly more than once for the same file (dev,ino
   * pair).  
   */

  for (i=0; i < NR_FTABLE; i++) {
    if (FD_ISSET(i,&global_ftable->inuse)) {
      struct file *filp = &global_ftable->fentries[i];	  
      ret = check_invariant1_helper(filp->f_dev, filp->f_ino);
      if (ret != SUCCESS)
	return FAILURE;
    }
  }
  return SUCCESS;
}

/* for correctness, the global_ftable and global_ftable_flock locks 
 * need to have been acquire before calling this function */ 
static int check_invariants(void) 
{
  int ret;
  ret = check_invariant1();
  if (ret != SUCCESS) {
    FLOCK_ERROR(("flock: INVARIANT1 CHECK FAILED"));
  }  

  return ret;
}


static int release(int fd) 
{
  struct file *filp;

  DEBUG(("flock.release prog: %s envid: %d fd: %d\n", __progname, __envid, fd)); 

  lock_ftable();
  lock_ftable_flock();

  CHECK_INVARIANTS();

  if (fd < 0 || fd > NR_OPEN || __current->fd[fd] == NULL) {
    DEBUG(("flock.release received a bogus fd %d", fd));
    errno = EBADF;
    unlock_ftable_flock();
    unlock_ftable();
    return FAILURE;
  }

  filp = __current->fd[fd];

  /* we check that we are actually holding the lock */
  if (IS_FILP_FLOCKED(filp) && (filp->flock_envid == __envid)) {
    DEBUG(("flock.release envid: %d prog: %s already holds to locks, so it can release it.\n", __envid, __progname));
    filp->flock_state = FLOCK_STATE_UNLOCKED;
    unlock_ftable_flock();
    unlock_ftable();
    return SUCCESS;
  } else {
    DEBUG(("flock.release envid %d prog %s is trying to release a lock which it does not hold. fd %d\n",
		 __envid, __progname, fd));
    flock_dump();
    unlock_ftable_flock();
    unlock_ftable();
    return FAILURE;
  }
}


static int acquire (int fd, int blocking)
{
  struct file *offending_filp = NULL;
  struct file *filp = NULL;
  u_int offending_envid = 0;
  int i;
  int state = NOT_IN_FTABLE;
  
  DEBUG(("flock.acquire prog: %s envid: %d fd: %d blocking: %s\n", __progname, __envid, fd, (blocking?"yes":"no")));

  lock_ftable();
  lock_ftable_flock();
 
  CHECK_INVARIANTS();

  if (fd < 0 || fd > NR_OPEN || __current->fd[fd] == NULL) {
    DEBUG(("flock.acquire received a bogus fd %d", fd));
    errno = EBADF;
    unlock_ftable_flock();
    unlock_ftable();
    return FAILURE;
  }
  filp = __current->fd[fd];

  DEBUG(("flock.acquire prog: %s envid: %d fd: %d --> dev: %d ino: %d\n", __progname, __envid, fd, filp->f_dev, filp->f_ino)); 

  for (i=0; i < NR_FTABLE; i++) {
    if (FD_ISSET(i,&global_ftable->inuse)) {
	struct file *tmpfilp = &global_ftable->fentries[i];
	if ((tmpfilp->f_dev == filp->f_dev) && (tmpfilp->f_ino == filp->f_ino)) {
	    offending_filp = tmpfilp;
	    state = UNFLOCKED_AND_IN_FTABLE;
	    if (IS_FILP_FLOCKED(tmpfilp))
	      {
		offending_envid = tmpfilp->flock_envid;
		offending_filp = tmpfilp;
		state = is_env_alive(tmpfilp->flock_envid) ?  FLOCKED_BY_ALIVE_ENV : FLOCKED_BY_DEAD_ENV ; 
		break;
	      }
	  }
      }
  }

  DEBUG(("flock.acquire prog: %s envid: %d state %s offending_envid %d offending_filp %s\n",
	 __progname, __envid, state_to_string_tab[state], offending_envid,
	 (offending_filp ? filp_to_string(offending_filp, debug_buf, DEBUG_BUF_SIZE) : "NULL")));


  switch (state) {
  case FLOCKED_BY_ALIVE_ENV:
    if (offending_filp == filp) {
      DEBUG(("flock.acquire env %d is trying to acquire a lock it already holds for fd %d.", __envid, fd));
      unlock_ftable_flock();
      unlock_ftable();
      return SUCCESS;
    } else if (blocking) {
      DEBUG(("flock.acquire env: %d prog: %s is trying to acquire lock held by envid: %d in blocking mode.\n" 
	     "Therefore, it will wait. offending_filp: %s\n",
	     __envid, __progname, offending_envid, filp_to_string(offending_filp, debug_buf, DEBUG_BUF_SIZE)));
      /* wait_until_... requires the global_ftable and the global_flock_lock to be held.  It releases them. */
      wait_until_terminates_or_lock_is_released(offending_envid, offending_filp);
      return IS_FLOCKED;      
    } else {
      DEBUG(("flock.acquire env: %d prog: %s is trying to acquire lock held by envid: %d in non-blocking mode.\n"
	     "Therefore, it will return immediately. offending_filp: %s\n",
	     __envid, __progname, offending_envid, filp_to_string(offending_filp, debug_buf, DEBUG_BUF_SIZE)));
      unlock_ftable_flock();
      unlock_ftable();
      return IS_FLOCKED;      
    }

  case UNFLOCKED_AND_IN_FTABLE:
    DEBUG(("flock.acquire env: %d prog: %s is acquire the available lock offending_filp: %s\n",
	   __envid, __progname, filp_to_string(offending_filp, debug_buf, DEBUG_BUF_SIZE)));
    filp->flock_state = FLOCK_STATE_EXCLUSIVE;
    filp->flock_envid = __envid;
    unlock_ftable_flock();
    unlock_ftable();
    return SUCCESS;

  case FLOCKED_BY_DEAD_ENV:
    DEBUG(("flock.acquire env: %d prog: %s is trying to acquire lock held a dead envid: %d\n" 
	   "Therefore, it will reap the lock and return immediately. offending_filp: %s\n",
	   __envid, __progname, offending_envid, filp_to_string(offending_filp, debug_buf, DEBUG_BUF_SIZE)));

    /* we change which file ptr the lock is on b/c flock are associated with fd's 
     * (and a filp is associated with fd)
     */
    offending_filp->flock_state = FLOCK_STATE_UNLOCKED;
    filp->flock_state = FLOCK_STATE_EXCLUSIVE;
    filp->flock_envid = __envid; 
    unlock_ftable_flock();
    unlock_ftable();
    return SUCCESS;


  case NOT_IN_FTABLE:
  default:
    DEBUG(("flock.acquire internal error\n"));
    flock_dump();
    unlock_ftable_flock();
    unlock_ftable();
    demand (0, this is impossible);
  }
}



int flock(int fd, int operation) {
  int ret;

  DEBUG(("flock %s %s envid %d fd %d operation %s %s\n", __progname, 
	       (operation & LOCK_UN) ? "release" : "acquire", __envid, fd,
	       (operation & LOCK_SH) ? "sh" : "ex",
	       (operation & LOCK_NB) ? "nb":"bl"));

  OSCALLENTER(OSCALL_flock);

  CHECKFD(fd, EBADF);
  if (__current->fd[fd]->op_type != CFFS_TYPE) {
      DEBUG(("flock op_type != CFFS_TYPE\n"));
      errno = EOPNOTSUPP;
      return FAILURE;
    }


  /*
   * XXX remember to do permission checking
   * According to some man pages the rules are as follows:
   *    to obtain a shared lock you must have read permission on the file
   *    to obtain an excl. lock you must have write permission on the file  
   */
#define NON_BLOCKING_MODE 0
#define BLOCKING_MODE (!NON_BLOCKING_MODE)

  if (operation & LOCK_SH) {
    /* we dont implement shared locks, yet */
    errno = EOPNOTSUPP;
    OSCALLEXIT(OSCALL_flock);
    return FAILURE;

  } else if (operation & LOCK_EX) {

    if (operation & LOCK_NB) {                    /* EXCLUSIVE and NON-BLOCKING */
      ret = acquire(fd, NON_BLOCKING_MODE);
      switch(ret) {
      case IS_FLOCKED:  
	errno = EWOULDBLOCK;
	OSCALLEXIT(OSCALL_flock);
	return FAILURE;
      case SUCCESS:   
	OSCALLEXIT(OSCALL_flock);
	return SUCCESS;
      default: 
	flock_dump();
	FLOCK_ERROR(("flock internal error\n"));
	demand (0, this is impossible);
      }
    } else {                                      /* EXCLUSIVE and BLOCKING */
      int i = 0;
      for (;;) {
	if (i++ > 20) {
	  FLOCK_ERROR(("flock: envid %d prog: %s fd: %d are we losing some kind of race here? something looks suspicious.\n", 
		       __envid, __progname, fd));
	  i = 0;
	}
	ret = acquire(fd, BLOCKING_MODE);
	if (ret == SUCCESS) {
	  OSCALLEXIT(OSCALL_flock);
	  return SUCCESS;
	} 
	/* otherwise we try again */
      }
    }

  } else if (operation & LOCK_UN) {
    ret = release(fd);
    switch (ret) {
    case FAILURE:  
      OSCALLEXIT(OSCALL_flock);
      return FAILURE;
    case SUCCESS:   
      OSCALLEXIT(OSCALL_flock);
      return SUCCESS;
    default: 
      flock_dump();
      FLOCK_ERROR(("flock internal error\n"));
      demand (0, this is impossible);
    }
  } else {
    /* bad operation */
    errno = EINVAL;
    OSCALLEXIT(OSCALL_flock);
    return FAILURE;
  }
}

