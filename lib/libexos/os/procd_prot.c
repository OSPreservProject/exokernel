#include <unistd.h>
#include <exos/osdecl.h>
#include "procd.h"

#include <string.h>
#include <stdio.h>

#include <exos/critical.h>
#include <exos/ipcdemux.h>
#include <exos/ipc.h>

#include <exos/conf.h>
#include <errno.h>

#define dprintf(fmt, args...) if (0) kprintf(fmt, ## args)

#ifdef PROCD
pid_t 
getpid(void) {
  proc_p p;
  pid_t pid;
  OSCALLENTER(OSCALL_getpid);
  
  dlockputs(__PROCD_LD,"getpid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(__envid);
  if (p) {
    pid =  p->p_pid;
  } else {
    fprintf(stderr,"Warning: getpid process (%d) not registered with procd\n",__envid);
    pid = PID_MAX;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"getpid release lock\n");
  ExitCritical();
  OSCALLEXIT(OSCALL_getpid);
  return pid;
}

pid_t 
getppid(void) {
  proc_p p;
  pid_t ppid;
  OSCALLENTER(OSCALL_getppid);
  dlockputs(__PROCD_LD,"getppid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(__envid);
  if (p) {
    ppid =  p->p_pptr->p_pid;
  } else {
    fprintf(stderr,"Warning: getppid process (%d) not registered with procd\n",__envid);
    ppid = PID_MAX;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"getppid release lock\n");
  ExitCritical();
  OSCALLEXIT(OSCALL_getppid);
  return ppid;
}
/* Get process group ID; note that POSIX getpgrp takes no parameter */
pid_t 
getpgrp(void) {
  proc_p p;
  pid_t pgrp;

  dprintf("%d getpgrp()\n",getpid());
  OSCALLENTER(OSCALL_getpgrp);
  dlockputs(__PROCD_LD,"getpgrp get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(__envid);
  if (p) {
    pgrp =  p->p_pgrp->pg_id;
  } else {
    fprintf(stderr,"Warning: getpgrp process (%d) not registered with procd\n",__envid);
    assert(0);
    pgrp = PID_MAX;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"getpgrp release lock\n");
  ExitCritical();
  OSCALLEXIT(OSCALL_getpgrp);
  return pgrp;
}

/*
 * SysVR.4 compatible getpgid()
 */
/* no OSCALL number */
pid_t
getpgid(pid_t pid) {
  proc_p p;
  dprintf("%d getpgid(%d)\n",getpid(),pid);
  dlockputs(__PROCD_LD,"getpgid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  if (pid == 0) 
    p = efind(__envid);
  else 
    p = __pd_pfind(pid);

  if (p) {
    pid = p->p_pgid;
  } else {
    if (pid == 0)		/* only for the case of our pid */
      fprintf(stderr,"Warning: getpgid process not registered with procd\n");
    errno = ESRCH;
    pid = 0;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"getpgid release lock\n");
  ExitCritical();
  return pid;
}

/*
 * getsid() defined in later POSIX 93? 
 */
/* no OSCALL number */
pid_t
getsid(pid_t pid) {
  proc_p p;
  dprintf("%d getsid(%d)\n",getpid(),pid);
  dlockputs(__PROCD_LD,"getsid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  if (pid == 0) {
    p = efind(__envid);
  } else {
    p = __pd_pfind(pid);
  }

  if (p) {
    pid =  p->p_session->s_leader->p_pid;
  } else {
    if (pid == 0)
      fprintf(stderr,"Warning: getsid, process not registered with procd\n");
    errno = ESRCH;
    pid = -1;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"getsid release lock\n");
  ExitCritical();
  return pid;

}

/*
 * setsid()
 */
pid_t 
setsid(void) {
  u_int procd_envid;
  int status;
  dprintf("%d setsid()\n",getpid());

  OSCALLENTER(OSCALL_setsid);
  dlockputs(__PROCD_LD,"setsid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  procd_envid = get_procd_envid();
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"setsid release lock\n");
  ExitCritical();
  if (procd_envid == -1) {
    fprintf(stderr,"Warning: setsid, procd not running\n");
    errno = EINVAL;
    return -1;
  }

  status = sipcout(procd_envid, IPC_PROC_SETSID, 0,0,0);

  if (status < 0) {
    /* could not find ourselves */
    if (status == -ESRCH)
      fprintf(stderr,"Warning: setsid, we (env %d) are not registered with procd\n",__envid);

    errno = -status;
    status = -1;
  }
  OSCALLEXIT(OSCALL_setsid);
  dprintf("%d setsid() returns %d  (%d)\n",getpid(),status,errno);
  return status;
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

int 
setpgid(pid_t pid, pid_t pgid) {
  u_int procd_envid;
  int status;
  dprintf("%d setpgid(%d,%d)\n",getpid(),pid,pgid);

  errno = 50;

  OSCALLENTER(OSCALL_setpgid);
  if (pgid < 0) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_setpgid);
    return -1;
  }
  dlockputs(__PROCD_LD,"setpgid get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  procd_envid = get_procd_envid();
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"setpgid release lock\n");
  ExitCritical();
  if (procd_envid == -1) {
    fprintf(stderr,"Warning: setpgid, procd not running\n");
    errno = EINVAL;
    OSCALLEXIT(OSCALL_setpgid);
    return -1;
  }
  errno = 49;

  status = sipcout(procd_envid, IPC_PROC_SETPGID, pid,pgid,0);

  if (status < 0) {
    /* could not find ourselves */
    if (status == -ESRCH && efind(__envid) == 0)
      fprintf(stderr,"Warning: setpgid, we (env %d) are not registered with procd\n",__envid);

    errno = -status;
    status = -1;
  }
  OSCALLEXIT(OSCALL_setpgid);
  dprintf("%d setpgid(%d,%d) returns %d (%d)\n",getpid(),pid,pgid,status,errno);
  
  return status;
}

/*
 * Get login name, if available.
 */
char * 
_getlogin(void) {
  static char login[MAXLOGNAME];
  proc_p p;

  OSCALLENTER(OSCALL_getlogin);
  dlockputs(__PROCD_LD,"_getlogin get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  p = efind(__envid);
  if (p) {
    bcopy(p->p_session->s_login,login,MAXLOGNAME);
  } else {
    fprintf(stderr,"Warning: getlogin process not registered with procd\n");
    login[0] = 0;
  }
  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"_getlogin release lock\n");
  ExitCritical();
  OSCALLEXIT(OSCALL_getlogin);
  return &login[0];

}

/*
 * Set login name.
 */
int 
setlogin(const char *name) {
  int namelen;
  int l1234,l5678,l9012;
  int status;
  u_int procd_envid;
  /* this algorithm and the protocol between us and procd only works
   if LOGNAME is <= 12 */

  extern int __logname_valid;	/* used in libc */

  OSCALLENTER(OSCALL_setlogin);
  __logname_valid = 0;		/* always reset the cached value (aggressive) */

  StaticAssert(MAXLOGNAME == 12);

  if (name == 0) {
    errno = EFAULT; 
    OSCALLEXIT(OSCALL_setlogin);
    return -1;
  }
  namelen = strlen(name);
  if (namelen + 1 >= MAXLOGNAME) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_setlogin);
    return -1;
  }
  /* we could check that we are super user here */
  if (__envid != 0 && getuid() != 0) {
    errno = EPERM;
    OSCALLEXIT(OSCALL_setlogin);
    return -1;
  }

  bcopy(&name[0],(char*)&l1234,4);
  bcopy(&name[4],(char*)&l5678,4);
  bcopy(&name[8],(char*)&l9012,4);

  dlockputs(__PROCD_LD,"setlogin get lock ");
  EXOS_LOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"... got lock\n");
  EnterCritical();
  procd_envid = get_procd_envid();
  if (procd_envid == -1) {
    EXOS_UNLOCK(PROCD_LOCK);
    dlockputs(__PROCD_LD,"setlogin release lock\n");
    ExitCritical();
    fprintf(stderr,"Warning: setlogin, procd not running\n");
    errno = EINVAL;
    OSCALLEXIT(OSCALL_setlogin);
    return -1;
  }

  EXOS_UNLOCK(PROCD_LOCK);
  dlockputs(__PROCD_LD,"setlogin release lock\n");
  ExitCritical();
  status = sipcout(procd_envid, IPC_PROC_SETLOGIN, l1234,l5678,l9012);

  if (status < 0) {
    /* could not find ourselves */
    if (status == -ESRCH)
      fprintf(stderr,"Warning: setlogin, we (env %d) are not registered with procd\n",__envid);

    errno = -status;
    status = -1;
  }
  OSCALLEXIT(OSCALL_setlogin);
  return status;
}
#endif
