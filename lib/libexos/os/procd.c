#include "procd.h"
#include <string.h>
#include <assert.h>
#include <sys/wait.h>

#include <xok/sys_ucall.h>

#include <exos/ipc.h>
#include <exos/conf.h>
#include <exos/critical.h>
#include <exos/locks.h>
#include <exos/uwk.h>
#include <exos/kprintf.h>
#include <exos/signal.h>
#include <exos/ipcdemux.h>

#include <unistd.h>		/* for getppid */
#include <signal.h>

#include "../fd/proc.h"

#define LIBEXOS_TTY
#define _KERNEL
#include <sys/param.h>
#include <sys/tty.h>
#undef _KERNEL
#undef LIBEXOS_TTY


/* kprintf for very bad
 * fprintf for bad but normally possible
 * dprintf for debuggin
 */

//#define fprintf(x,fmt, args...) 
//#define kprintf(fmt, args...) 
#define dprintf if (0) kprintf


int 
proc_table_ipc_setsid(int code, int nop0, int nop1, int nop2, u_int caller) {
  proc_p p;
  pid_t ret;
  dprintf("proc_table_ipc_setsid by envid: %d\n",caller);
  dlockputs(__PROCD_LD,"proc_table_ipc_setsid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(caller);
  if (p == NULL) {
    ret = -ESRCH;
  } else if (p->p_pgid == p->p_pid || pgfind(p->p_pid)) {
    /* if we are the session leader or if there is a process group with our
       pid */
    ret = -EPERM;
  } else {
    (void)enterpgrp(p, p->p_pid, 1);
    assert(p->p_pid >= 0);
    ret = p->p_pid;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_setsid release lock\n");
  ExitCritical();
  return ret;
}

int 
proc_table_ipc_ptrace(int code, int value, int nop1, int nop2, u_int caller) {
  proc_p p;
  pid_t ret;
  dprintf("proc_table_ipc_ptrace by envid: %d\n",caller);
  dlockputs(__PROCD_LD,"proc_table_ipc_ptrace get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(caller);
  if (p == NULL) {
    ret = -ESRCH;
  } else {
    if (value)
      p->p_flag |= P_TRACED;
    else
      p->p_flag &= ~P_TRACED;
    assert(p->p_pid >= 0);
    ret = p->p_pid;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_ptrace release lock\n");
  ExitCritical();
  return ret;
}

int 
proc_table_ipc_reparent(int code, int new_parent_envid, int nop1, int nop2,
			u_int caller) {
  proc_p p, p2;
  pid_t ret;
  dprintf("proc_table_ipc_reparent by envid: %d\n",caller);
  dlockputs(__PROCD_LD,"proc_table_ipc_reparent get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(caller);
  p2 = efind((u_int)new_parent_envid);
  if (p == NULL || p2 == NULL) {
    ret = -ESRCH;
  } else {
    proc_reparent(p, p2);
    ret = 0;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_reparent release lock\n");
  ExitCritical();
  return ret;
}

/*
 * set process group (setpgid/old setpgrp)
 *
 * caller does setpgid(targpid, targpgid)
 *
 * pid must be caller or child of caller (ESRCH)
 * if a child
 *	pid must be in same session (EPERM)
 *	pid can't have done an exec (EACCES)
 * if pgid != pid
 * 	there must exist some pid in same session having pgid (EPERM)
 * pid must not be session leader (EPERM)
 */
/* ARGSUSED */
int 
proc_table_ipc_setpgid(int code, int pid , int pgid, int nop2, u_int caller) {
  proc_p targp, curp;   /* target and current (calling) process */
  pgrp_p pgrp;		/* target pgrp */
  pid_t ret = 0;
  
  if (pgid < 0) 
    return -EINVAL;

  dlockputs(__PROCD_LD,"proc_table_ipc_setpgid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  targp = curp = efind(caller);
  if (curp == 0) {ret = -ESRCH; goto done;}

  if (pid != 0 && pid != curp->p_pid) {
    /* it is not ourselves */
    if ((targp = __pd_pfind(pid)) == 0 || !inferior(targp,curp))
      ret = -ESRCH;
    else if (targp->p_session != curp->p_session) {
      fprintf(stderr,"setpgid fails, different session: caller pid: %d, "
	      "sess: %d, targ: %d\n", curp->p_pid,
	      curp->p_session->s_leader->p_pid,
	      targp->p_session->s_leader->p_pid);
      ret = -EPERM;
    } else if (HASEXECED(targp))
      ret = -EACCES;
    if (ret != 0) goto done;
  } else 
    targp = curp;

  if (SESS_LEADER(targp)) {
    ret = -EPERM; 
    fprintf(stderr,"setpgid fails because is session leader: pid: %d\n",
	    targp->p_pid);
    goto done;
  }

  if (pgid == 0)
    pgid = targp->p_pid;
  else if (pgid != targp->p_pid)
    if ((pgrp = pgfind(pgid)) == 0 || 
	pgrp->pg_session != curp->p_session) {
      fprintf(stderr,"setpgid fails, pgrp does not exists %d we are in "
	      "different session %d\n", pgfind(pgid) == 0,
	      pgrp->pg_session != curp->p_session);
      ret = -EPERM ; goto done;
    }
  ret = -1 * enterpgrp(targp, pgid, 0);	/* enterpgrp returns positive errno's */
done:
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_setpgid release lock\n");
  ExitCritical();
#if 0
  printf("setpgid(%d,%d) ret: %d ",pid,pgid,ret);
  printf("curp (%p): %d %d %d,",
	 curp,curp->p_pid, curp->p_pgid, curp->p_session->s_leader->p_pid);
  printf(" target (%p): %d %d %d\n",
	 targp,targp->p_pid, targp->p_pgid, targp->p_session->s_leader->p_pid);
  if (ret < 0) {procd_ps();}
  printf("setpgid(%d,%d) ret: %d errno: %d ",pid,pgid,ret,errno);
#endif
  return ret;
}

int 
proc_table_ipc_setlogin(int code, int l1234, int l5678, int l9012,
			u_int caller) {
  char login[MAXLOGNAME];
  session_p s;
  proc_p p;
  StaticAssert(MAXLOGNAME == 12);
  bcopy((char *)&l1234,&login[0],4);
  bcopy((char *)&l5678,&login[4],4);
  bcopy((char *)&l9012,&login[8],4);
  login[MAXLOGNAME - 1] = 0;
  
  p = efind(caller);
  if (p == NULL) {
    return -ESRCH;
  }
  /* NEED TO test that user is super user */
  s = p->p_session;
  bcopy(&login[0], &s->s_login,MAXLOGNAME);
  return 0;
}

/* 
 How exit works:
 process cleans up it's state.  
 If is it a session leader, closes terminal device, sends SIGHUP to foreground
 process group.  Drains the terminal.  Revoke access to terminal
 Then call IPC_PROC_XSTAT(envid,SZOMB,xstat).
     XSTAT sets the exit status
     if it is session leader it decouples it from session
     fixes job control on exiting process' process group
     reparent process' children
     increment parents 
 signal parent SIGCHLD
 Then do a sys_env_free(self)
 */
int 
proc_exit(int exit_status) {
  proc_p p;
  int ret,status;
  pid_t ppid;
  u_int procd_envid;

  dlockputs(__PROCD_LD,"proc_exit get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  /* we can't do this because shared memory segments are unmapped */
  p = efind(__envid);
  if (p && SESS_LEADER(p)) {
    session_p sp = p->p_session;
    dprintf("proc_exit, we are session leader\n");
    /* we are the session leader */
    /* validate the controlling terminal for the session  */
    /* signal SIGHUP to all processes in the process group */
    /* drain terminal */
    /* revoke terminal (by procd) */

    if (sp->s_ttyvp) {
      /*
       * Controlling process.
       * Signal foreground pgrp,
       * drain controlling terminal
       * and revoke access to controlling terminal.
       */
      if (sp->s_ttyp->t_session == sp) {
  	EXOS_UNLOCK(PROCD_LOCK);
        dlockputs(__PROCD_LD,"proc_exit release lock\n");
	ExitCritical();
	if (sp->s_ttyp->t_pgrp) 
	  proc_pgsignal(sp->s_ttyp->t_pgrp, SIGHUP, 1);
#if 0
	(void) ttywait(sp->s_ttyp);
#endif
        dlockputs(__PROCD_LD,"proc_table_exit get lock ");
  	EXOS_LOCK(PROCD_LOCK);
        dlockputs(__PROCD_LD,"... got lock\n");
	EnterCritical();
	/*
	 * The tty could have been revoked
	 * if we blocked.
	 */
#if 0				/* we don't have revoke */
	if (sp->s_ttyvp)
	  VOP_REVOKE(sp->s_ttyvp, REVOKEALL);
#endif
      }
    }
    /* sp->s_leader will be cleared by procd */
  }
  procd_envid = get_procd_envid();

  if (procd_envid == -1) {
    /* we don't have a terminal anymore */
    kprintf("Warning: proc_exit, procd not running\n");
    ret = -1;
  } else {
    if (efind(__envid)) {
      int signalinit = 0;

      if (p->p_children.lh_first) signalinit = 1; /* we'll orphan some
						     children */
      EXOS_UNLOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"proc_exit release lock\n");
      ExitCritical();
      status = sipcout(procd_envid,IPC_PROC_SETSTAT,__envid,SZOMB,exit_status);
      dlockputs(__PROCD_LD,"proc_table_exit get lock ");
      EXOS_LOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"... got lock\n");
      EnterCritical();

      if (status != 0) kprintf("Warning proc_exit ipc_proc_setstat "
			       "failed: %d\n",status);
      ppid = getppid();
      dprintf("sending SIGCHLD to parent from %d to %d\n",getpid(),ppid);
      if (ppid != 0) kill(ppid,SIGCHLD);
      if (signalinit) kill(INITPROC,SIGCHLD);
      ret = 0;
    } else {
      kprintf("proc_exit: envid: %d we are not registered\n",__envid);
      ret = -1;
    }
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_exit release lock\n");
  ExitCritical();
  return ret;
}

int 
proc_pid_exit(pid_t pid, int exit_status) {
  proc_p p;
  int ret,status;
  pid_t ppid;
  u_int procd_envid;

  dlockputs(__PROCD_LD,"proc_pid_exit get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  /* we can't do this because shared memory segments are unmapped */
  p = __pd_pfind(pid);
  procd_envid = get_procd_envid();
  if (procd_envid == -1) {
    /* we don't have a terminal anymore */
    kprintf("Warning: proc_exit, procd not running\n");
    ret = -1;
  } else {
    int signalinit = 0;
    if (p->p_children.lh_first) signalinit = 1; /* we'll orphan some children */


    EXOS_UNLOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"proc_pid_exit release lock\n");
    ExitCritical();
    status = sipcout(procd_envid,IPC_PROC_SETSTAT,p->envid,SZOMB,exit_status);
    dlockputs(__PROCD_LD,"proc_pid_exit get lock ");
    EXOS_LOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"... got lock\n");
    EnterCritical();

    if (status != 0) kprintf("Warning proc_exit ipc_proc_setstat failed: %d\n",
			     status);
    ppid = p->p_pptr->p_pid;
    kprintf("sending SIGCHLD to parent from %d to %d\n",getpid(),ppid);
    sleep(1);
    if (ppid != 0) {
      printf("signaling %d\n",ppid);
      kill(ppid,SIGCHLD);
    }
    if (signalinit) {
      printf("signaling %d\n",INITPROC);
      kill(INITPROC,SIGCHLD);
    }
    ret = 0;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_pid_exit release lock\n");
  ExitCritical();
  return ret;
}

int
proc_reap(proc_p p) {
  int procd_envid;
  int ipcstatus;
  int ret;
  if (p == NULL) return -1;
  dlockputs(__PROCD_LD,"proc_reap get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  procd_envid = get_procd_envid();
  if (procd_envid == -1) {
    /* we don't have a terminal anymore */
    kprintf("Warning: proc_reap, procd not running\n");
    ret = -1;
  } else {
    if (p->p_stat != SZOMB) 
      kprintf("Warning: proc_reap on a non-zombied process: pid %d\n",p->p_pid);

    EXOS_UNLOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"proc_reap release lock\n");
    ExitCritical();
    ipcstatus = sipcout(procd_envid,IPC_PROC_REAP,p->envid,REAPOP_REAP,0);
    dlockputs(__PROCD_LD,"proc_reap get lock ");
    EXOS_LOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"... got lock\n");
    EnterCritical();

    if (ipcstatus != 0) kprintf("Warning proc_reap ipc_proc_setstat failed: %d\n",
				ipcstatus);
    ret = ipcstatus;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_reap release lock\n");
  ExitCritical();
  return ret;
}

int 
proc_pid_reap(pid_t pid) {
  proc_p p;
  if ((p = __pd_pfind(pid))) 
    return proc_pid_reap(p->envid);
  else {
    kprintf("proc_pid_reap: Warning non registered pid: %d\n",pid);
    return -1;
  }
}


int 
proc_stop(int signum) {
  int ret;
  u_int procd_envid;
  dprintf("PROC_STOP\n");
  dlockputs(__PROCD_LD,"proc_stop get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  /* we can't do this because shared memory segments are unmapped */
  procd_envid = get_procd_envid();

  if (procd_envid == -1) {
    /* we don't have a terminal anymore */
    kprintf("Warning: proc_stop, procd not running\n");
    ret = -1;
  } else {
    if (efind(__envid)) {
      pid_t ppid;

      EXOS_UNLOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"proc_stop release lock\n");
      ExitCritical();
      ret = sipcout(procd_envid,IPC_PROC_SETSTAT,__envid,SSTOP,signum);
      dlockputs(__PROCD_LD,"proc_stop get lock ");
      EXOS_LOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"... got lock\n");
      EnterCritical();

      dprintf("%d proc_stop (%d,%d) -> %d\n",__envid,__envid,signum,ret);
      if (ret != 0) kprintf("Warning proc_exit ipc_proc_setstat failed: %d\n",
			    ret);
      ppid = getppid();
      dprintf("sending SIGCHLD to parent from %d to %d\n",getpid(),ppid);
      if (ppid != 0) kill(ppid,SIGCHLD);
    } else {
      kprintf("proc_stop: envid: %d we are not registered\n",__envid);
      ret = -1;
    }
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_stop release lock\n");
  ExitCritical();
  return ret;
}

int 
proc_continue(void) {
  int ret;
  u_int procd_envid;

  dlockputs(__PROCD_LD,"proc_continue get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  /* we can't do this because shared memory segments are unmapped */
  procd_envid = get_procd_envid();

  if (procd_envid == -1) {
    /* we don't have a terminal anymore */
    kprintf("Warning: proc_continue, procd not running\n");
    assert(0);
    ret = -1;
  } else {
    if (efind(__envid)) {

      EXOS_UNLOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"proc_continue release lock\n");
      ExitCritical();
      ret = sipcout(procd_envid,IPC_PROC_SETSTAT,__envid,SRUN,-1);
      dlockputs(__PROCD_LD,"proc_continue get lock ");
      EXOS_LOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"... got lock\n");
      EnterCritical();

      dprintf("%d proc_continue (%d) -> %d\n",__envid,__envid,ret);
      if (ret != 0) {
	kprintf("Warning proc_continue ipc_proc_setstat failed: %d\n",ret);
	assert(0);
      }
    } else {
      kprintf("proc_continue: envid: %d we are not registered\n",__envid);
      assert(0);
      ret = -1;
    }
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_continue release lock\n");
  ExitCritical();
  return ret;
}

int 
proc_table_ipc_setstat(int code, int signed_envid, int stat, int xstat,
		       u_int caller) {
  u_int envid = (unsigned)signed_envid;
  proc_p p, curproc;
  int ret;

  dprintf("proc_table_ipc_setstat envid: %d caller: %d stat: %d xstat: %d\n",
	  envid,caller,stat, xstat);
  
  dlockputs(__PROCD_LD,"proc_table_ipc_setstat get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();

  curproc = efind(caller);
  p = efind(envid);

  if (p == NULL) {
    kprintf("Warning: ipc_xstat no process bound with target envid\n");
    ret = -ESRCH; goto done;
  }
  if (curproc == NULL) {
    kprintf("Warning: ipc_xstat no process bound with source envid\n");
    ret = -ESRCH; goto done;
  }
  /* HBXX need to do some permission checking, although is not clear what */

  if (stat != -1) {
    if (p->p_stat == SZOMB) {
      kprintf("Warning: trying to set flag of zombie (pid %d) by envid %d\n",
	      p->p_pid,caller);
      ret = -EINVAL;
      goto done;
    }

    switch(stat) {
    case SSTOP:
      p->p_flag &= ~P_WAITED;
      /* fall through */
    case SRUN:
      p->p_stat = stat;
      break;
    case SZOMB: {
      proc_p q,nq,initproc;

      if (p->p_flag & P_PPWAIT) {
	p->p_flag &= ~P_PPWAIT;
	/* HBXX - OpenBSD wakes up parent here to avoid deadlock */
      }

      if (p && SESS_LEADER(p)) {
	register struct session *sp = p->p_session;
	if (sp->s_ttyvp) {
	  struct file *filp = (struct file *)sp->s_ttyvp;
	  if(CHECKOP(filp,close0)) DOOP(filp,close0,(filp,-1));
	  filp_refcount_dec(filp);
	  if (filp_refcount_get(filp) == 0) {
	    int error = 0;
	    if(CHECKOP(filp,close)) error = DOOP(filp,close,(filp));
	    if (error) kprintf("WARNING error closing controlling "
			       "terminal: %d %d\n", error, errno);
	  }
	  sp->s_ttyvp = NULL;
	}
	sp->s_leader = NULL;
      }

      fixjobc(p, p->p_pgrp, 0);
      
      p->p_stat = stat;

      initproc = __pd_pfind(INITPROC);
      if (initproc) 
	for (q = p->p_children.lh_first ; q != 0; q = nq) {
	  nq = q->p_sibling.le_next;
	  proc_reparent(q, initproc);
	  initproc->nxchildren++;
	}
      else kprintf("WARNING: ipc_setstat no initproc\n");

      break;
    }
    default:
      kprintf("Warning: ipc_xstat given bad stat (%d), by %d\n",stat,caller);
      ret = -EINVAL;
      goto done;
    }
  }
  if (xstat != -1) {
    p->p_xstat = xstat;
  }
  p->p_pptr->nxchildren++;
  ret = 0;
  done:
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_setstat release lock\n");
  ExitCritical();
  return ret;
}

/* 
 How wait works:
 check process' nxchildren.  Traverse children's list looking for
 p_stat == SZOMB
 if found copy xstat and call IPC_PROC_REAP(pid) to remove children.
 
 xstat is the return status indicating reason for stoping or exiting. 
 NOTE: we don't handle any of the cases of PTRACE 
 */
#ifdef PROCD
int 
wait4 (int pid, int *status, int options, struct rusage *rusage) {
  int nfound;
  proc_p q, p;
  int ipcstatus;
  int retval;
  int nxchildren;
  u_int procd_envid;
  int ret;

  OSCALLENTER(OSCALL_wait4);

  if (options & ~(WUNTRACED|WNOHANG)) {
    errno = EINVAL; 
    OSCALLEXIT(OSCALL_wait4);
    return -1;
  }

  signals_off();
  dlockputs(__PROCD_LD,"wait4 get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  q = efind(__envid);

  if (q == 0) {
    kprintf("Warning, wait4: we are not registered with procd: envid: %d, "
	    "getpid says: %d\n", __envid, getpid());
    errno = EINVAL; retval = -1; goto done;
  }

  if (pid == 0) 
    pid = -(q->p_pgid);

  procd_envid = get_procd_envid();
  if (procd_envid == -1) {
    kprintf("Error, wait4: procd not running, will not be able to reap "
	    "children\n");
    errno = EINVAL; retval = -1; goto done;
  }

loop:
  nxchildren = q->nxchildren;
  nfound = 0;
  for (p = q->p_children.lh_first; p != 0; p = p->p_sibling.le_next) {
    if (pid != WAIT_ANY && p->p_pid != pid && p->p_pgid != -pid)
      continue;
    nfound++;
    if (p->p_stat == SZOMB) {
      retval = p->p_pid;
      if (status) *status = p->p_xstat;

      /* XXX - rusage not implementetd */
      if (rusage != (struct rusage *)0)
	memset((void *)rusage, 0, sizeof(struct rusage));
      
      /* HBXX we don't handle all ptrace cases */
      /* dprintf("REAP sipcout(%d,%d,%d,%d,%d)\n",procd_envid,IPC_PROC_REAP,
	 p->envid,REAPOP_REAP,0); */
      
      EXOS_UNLOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"wait4 release lock\n");
      ExitCritical();
      ipcstatus = sipcout(procd_envid,IPC_PROC_REAP,p->envid,REAPOP_REAP,0);
      dlockputs(__PROCD_LD,"wait4 get lock ");
      EXOS_LOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"... got lock\n");
      EnterCritical();

      if (ipcstatus != 0) kprintf("Warning ipc_proc_reap (reap) failed: %d\n",
				  ipcstatus);
      goto done;
    }
    /* if process is stopped, has not been waited upon, and options is
       WUNTRACED */
    if (p->p_stat == SSTOP && (p->p_flag & P_WAITED) == 0 &&
	(p->p_flag & P_TRACED || options & WUNTRACED)) {
      retval = p->p_pid;

      /* dprintf("WAITED sipcout(%d,%d,%d,%d,%d)\n",procd_envid,IPC_PROC_REAP,
	 p->envid,REAPOP_WAITED,0); */
      /* set the waited flag */
      EXOS_UNLOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"wait4 release lock\n");
      ExitCritical();
      ipcstatus = sipcout(procd_envid,IPC_PROC_REAP,p->envid,REAPOP_WAITED,0);
      dlockputs(__PROCD_LD,"wait4 get lock ");
      EXOS_LOCK(PROCD_LOCK);
      dlockputs(__PROCD_LD,"... got lock\n");
      EnterCritical();
      if (ipcstatus != 0) {
	kprintf("Warning ipc_proc_reap (waited) failed: %d\n",ipcstatus);
	assert(0);
      }

      if (status) *status = W_STOPCODE(p->p_xstat);
      goto done;
    }
  }
  if (nfound == 0) {errno = ECHILD; retval = -1; goto done;}
  if (options & WNOHANG) {retval = 0; goto done;}

  /* wk_waitfor_value_neq(&q->nxchildren, nxchildren, CAP_PROCD); 
   * but with proper signal handling */
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"wait4 release lock\n");
  ExitCritical();
  {
    struct wk_term t[3];
    int sz = 0;
    sz = wk_mkvar (sz, t, wk_phys (&q->nxchildren), CAP_PROCD);
    sz = wk_mkimm (sz, t, nxchildren);
    sz = wk_mkop (sz, t, WK_NEQ);
    ret = tsleep_pred(t,sz, PCATCH, "wait4", 0);
    if (ret == EINTR) {errno = EINTR; retval = -1; goto donec;}
    if (ret == ERESTART) {
      /* Will deliver the signal */
      signals_on();
      signals_off();
    }
    /* for ERESTART we'll try again */
  }
  /* check to see if we were interrupted by a signal */
  dlockputs(__PROCD_LD,"wait4 get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  goto loop;
  
done:
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"wait4 release lock\n");
  ExitCritical();
donec:
  OSCALLEXIT(OSCALL_wait4);
  signals_on();
  return retval;
}
#endif

int 
proc_table_ipc_reap(int code, int signed_envid, int reap_op, int op,
		    u_int caller) {
  u_int envid = (unsigned)signed_envid;
  proc_p p,curproc;
  int ret;
  dprintf("proc_table_ipc_reap %d op: %s\n",
	  envid, (reap_op == REAPOP_WAITED) ? "WAITED" : (
	  (reap_op == REAPOP_REAP) ? "REAP" : "BADOP"));
  dlockputs(__PROCD_LD,"proc_table_ipc_reap get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();

  curproc = efind(caller);
  p = efind(envid);

  if (p == NULL) {
    kprintf("Warning: ipc_reap no process bound with target envid\n");
    ret = -ESRCH; goto done;
  }
  if (curproc == NULL) {
    kprintf("Warning: ipc_reap no process bound with source envid\n");
    ret = -ESRCH; goto done;
  }
  switch(reap_op) {
  case REAPOP_WAITED:
    /* you can only change the P_WAITED of yourself */
    if (caller != p->p_pptr->envid) {ret = -EPERM; goto done;}
    p->p_flag |= P_WAITED;
    ret = 0;
    break;
  case REAPOP_REAP:
    /* HBXX need to do some permission checking, although is not clear what */
    dprintf("proc_table_ipc_reap REAPING pid: %d\n",p->p_pid);
    /* remove it from the siblings */
    leavepgrp(p);
    LIST_REMOVE(p, p_sibling);

    proc_dealloc(p);
    ret = 0;
    break;
  default:
    ret = -EINVAL;
    kprintf("Warning: proc_table_ipc_reap bad op: %d\n",reap_op);
      
  }
  done:
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_reap release lock\n");
  ExitCritical();
  return ret;
}


/*
 * make process 'parent' the new parent of process 'child'.
 */
void
proc_reparent(proc_p child, proc_p parent) {
  if (child->p_pptr == parent)
    return;
  
  LIST_REMOVE(child, p_sibling);
  LIST_INSERT_HEAD(&parent->p_children, child, p_sibling);
  child->p_pptr = parent;
}

pid_t 
proc_fork1(u_int newenvid, int forktype) {
  u_int procd_envid;
  int ret;
  dlockputs(__PROCD_LD,"proc_fork1 get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  procd_envid = get_procd_envid();
  if (procd_envid == -1) {
    ret = newenvid;
  } else {
    int code;
    assert(forktype == PROCD_FORK || forktype == PROCD_VFORK);
    code = (forktype == PROCD_FORK) ? IPC_PROC_FORK : IPC_PROC_VFORK;
    EXOS_UNLOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"proc_fork1 release lock\n");
    ExitCritical();
    ret = sipcout(procd_envid,code, __envid, newenvid,0);
    dlockputs(__PROCD_LD,"proc_fork1 get lock ");
    EXOS_LOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"... got lock\n");
    EnterCritical();
    //kprintf("%d proc_fork (%d) -> %d\n",__envid,newenvid,ret);
    if (ret < 0) {
      kprintf("WARNING PROC_TABLE_%sFORK fails: %d\n",
	      forktype == PROCD_FORK ? "" : "V",ret);
      ret = newenvid;
    } 
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_fork1 release lock\n");
  ExitCritical();
  return ret;
}


int 
proc_table_ipc_fork(int code, int signed_envid_parent, int signed_envid_child, 
		    int nop3, u_int caller) {
  u_int envidp = (unsigned)signed_envid_parent;
  u_int envidc = (unsigned)signed_envid_child;
  proc_p p1,p2;
  int ret;
  
  dprintf("proc_table_ipc_%sfork p: %d c: %d caller: %d\n",
	  (code == IPC_PROC_VFORK) ? "v" : "",
	  envidp, envidc,caller);
  if (caller != envidp) {
    kprintf("ERROR: proc_table_ipc_fork not from parent\n");
    return -EPERM;
  }
  dlockputs(__PROCD_LD,"proc_table_ipc_fork get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p1 = efind(envidp);
  if (p1 == NULL) {ret = -ESRCH; goto done;}
  p2 = proc_alloc();
  if (p2 == NULL) {ret = -EAGAIN; goto done;}

  if (p1->p_session->s_ttyp != NULL && p1->p_flag & P_CONTROLT)
    p2->p_flag |= P_CONTROLT;
  if (code == IPC_PROC_VFORK) 
    p2->p_flag |= P_PPWAIT;    /* parent is waiting for child to exec or exit */

  dprintf(" proc_table_ipc_%sfork p: %d c: %d caller: %d (p2->p_pid: %d) "
	  "succeeds\n", (code == IPC_PROC_VFORK) ? "v" : "", envidp,
	  envidc,caller,p2->p_pid);

  /* XXX the pid of p2 is set by proc_alloc */
  p2->p_pgrp = p1->p_pgrp;
  p2->p_pptr = p1;


  /* insert new process as child of old process */
  LIST_INSERT_HEAD(&p1->p_children, p2, p_sibling);
  /* initiliaze p2 */
  LIST_INIT(&p2->p_children);
  LIST_INSERT_AFTER(p1, p2, p_pglist);
  p2->p_xstat = 0;
  p2->nxchildren = 0;
  p2->p_stat = SRUN;
  p2->envid = envidc;
  ret = p2->p_pid;
		    done:
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_fork release lock\n");
  ExitCritical();
  return ret;
}

pid_t 
proc_exec(u_int newenvid) {
  u_int procd_envid;
  int ret;
  
  dlockputs(__PROCD_LD,"proc_exec get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  procd_envid = get_procd_envid();
  if (procd_envid == -1) {
    ret = newenvid;
  } else {
    EXOS_UNLOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"proc_exec release lock\n");
    ExitCritical();
    ret = sipcout(procd_envid,IPC_PROC_EXEC, __envid, newenvid,0);
    dlockputs(__PROCD_LD,"proc_exec get lock ");
    EXOS_LOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"... got lock\n");
    EnterCritical();
    dprintf("proc_exec (%d) -> %d (from envid: %d)\n",newenvid,ret,__envid);
    if (ret < 0) {
      kprintf("WARNING PROC_TABLE_EXEC fails: %d\n",ret);
      ret = newenvid;
    } 
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_exec release lock\n");
  ExitCritical();
  return ret;
}

int 
proc_table_ipc_exec(int code, int signed_envid_parent, int signed_envid_child, 
		    int nop3, u_int caller) {
  u_int envidp = (unsigned)signed_envid_parent;
  u_int envidc = (unsigned)signed_envid_child;
  proc_p p;
  int ret;

  if (caller != envidp) {
    kprintf("ERROR: proc_table_ipc_exec not from parent\n");
    return -EPERM;
  }

  dprintf("proc_table_ipc_exec p: %d c: %d caller: %d\n",envidp, envidc,caller);

  if (envidc == 0) 
    kprintf("WARNING: proc_table_ipc_exec child envid == 0\n");

  dlockputs(__PROCD_LD,"proc_table_ipc_exec get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(envidp);
  if (p == NULL) {
    ret = -ESRCH; 
  } else {
    p->envid = envidc;
    p->p_flag |= P_EXEC;
    if (p->p_flag & P_PPWAIT) {
      p->p_flag &= ~P_PPWAIT;
      /* HBXX - OpenBSD wakes up parent here to avoid deadlock */
    }
    ret = 0;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_exec release lock\n");
  ExitCritical();
  return ret;
}
#ifdef PROCD
u_int 
pid2envid(pid_t pid) {
  proc_p p;
  u_int envid;
  dlockputs(__PROCD_LD,"pid2envid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = __pd_pfind(pid);
  envid = (p) ? p->envid : 0;
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"pid2envid release lock\n");
  ExitCritical();
  return envid;
}

pid_t
envid2pid(u_int envid) {
  proc_p p;
  pid_t pid;
  dlockputs(__PROCD_LD,"envid2pid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(envid);
  pid = (p) ? p->p_pid : 0;
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"envid2pid release lock\n");
  ExitCritical();
  return pid;
}
#endif




int
proc_controlt(int controlt, int ttyp, int ttyvp) {
  u_int procd_envid;
  int ret;
  
  dlockputs(__PROCD_LD,"proc_controlt get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  procd_envid = get_procd_envid();
  if (procd_envid == -1) {
    kprintf("Error, proc_controlt: procd not running\n");
    errno = EINVAL; 
    ret = -1; 
  } else {
    EXOS_UNLOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"proc_controlt release lock\n");
    ExitCritical();
    ret = sipcout(procd_envid,IPC_PROC_CONTROLT, controlt, ttyp, ttyvp);
    dlockputs(__PROCD_LD,"proc_controlt get lock ");
    EXOS_LOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"... got lock\n");
    EnterCritical();
    dprintf("envid: %d proc_controlt (%d,0x%x,0x%x) -> %d\n",__envid,
	    controlt, ttyp, ttyvp,ret);
    if (ret < 0) kprintf("WARNING PROC_TABLE_CONTORLT fails: %d\n",ret);
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_controlt release lock\n");
  ExitCritical();
  return ret;
}


int 
proc_table_ipc_controlt(int code, int controlt, int ttyp, int ttyvp,
			u_int caller) {
  proc_p p;
  int ret;

  dlockputs(__PROCD_LD,"proc_table_ipc_controlt get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();

  p = efind(caller);

  if (p == NULL) {
    kprintf("Warning: ipc_controlt no process bound with target envid\n");
    ret = -ESRCH; goto done;
  }

  switch(controlt) {
  case 0:
    p->p_flag &= ~P_CONTROLT;
    break;
  case 1:
    p->p_flag |= P_CONTROLT;
    break;
  default:
  }
  if (ttyvp == -1 && ttyp == -1) {
    ret = 0; goto done;
  }

  if (!SESS_LEADER(p)) {
    ret = -EPERM; goto done;
  }
  if (ttyvp != -1) {
    if (p->p_session->s_ttyvp == NULL) {
      struct file *filp;
      p->p_session->s_ttyvp = (void *)ttyvp;
      /* increase refcount */
      filp = (struct file *)ttyvp;
      filp_refcount_inc(filp);
    } else {
      assert(0);
    }
  }
  if (ttyp != -1) {
    p->p_session->s_ttyp = (struct tty *)ttyp;
  }
  ret = 0;
  done:
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"proc_table_ipc_controlt release lock\n");
  ExitCritical();
  return ret;
}

