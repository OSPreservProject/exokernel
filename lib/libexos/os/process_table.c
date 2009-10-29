
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

#include <unistd.h>
#include "fd/proc.h"
#include <exos/callcount.h>
#include <exos/conf.h>
#ifdef PROCESS_TABLE
pid_t 
getpid(void) { 
  OSCALLENTER(OSCALL_getpid);
  OSCALLEXIT(OSCALL_getpid);
  return (UAREA.pid);
}

pid_t 
getppid(void) { 
    OSCALLENTER(OSCALL_getppid);
    OSCALLEXIT(OSCALL_getppid);
    return (UAREA.parent);

}
pid_t 
getpgrp(void) { 
    OSCALLENTER(OSCALL_getpgrp);
    OSCALLEXIT(OSCALL_getpgrp);
    return(__current->pgrp); 
}

/* setsid defined in os/setsid.c */
pid_t
getpgid(pid_t pid) { 
    if (pid == 0) { 
	return getppid(); 
    } else {
	printf("warning, getpgid not fully implemented yet\n");
	return -1;	/* this is the wrong result */
    }
}

pid_t
setsid(void)
{
  pid_t r;

  OSCALLENTER(OSCALL_setsid);
  r = getpid();
  OSCALLEXIT(OSCALL_setsid);
  return r;
}

pid_t
getsid(pid_t pid)
{
  return (pid == 0) ? getpid() : pid;
}

int 
setpgid(pid_t pid, pid_t pgrp) {
  OSCALLENTER(OSCALL_setpgid);
  if (pid == 0 || pid == getpid()) {
    __current->pgrp = pgrp;
  }
  OSCALLEXIT(OSCALL_setpgid);
  return 0;
}

char *
_getlogin (void) {
  static char logname[MAXLOGNAME];

  OSCALLENTER(OSCALL_getlogin);
  if (__current->logname[0]) {
    strcpy(logname,__current->logname);
    OSCALLEXIT(OSCALL_getlogin);
    return (char *) logname;
  } else {
    OSCALLEXIT(OSCALL_getlogin);
    return (char *)0;
  }
}

int 
setlogin (const char *name) {
  int len;
  
  extern int __logname_valid;	/* used in libc */
  __logname_valid = 0;

  OSCALLENTER(OSCALL_setlogin);
  if (getuid() == 0 || geteuid() == 0) {
    len = strlen(name);
    if (len+1 > MAXLOGNAME) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_setlogin);
      return -1;
    }
    strcpy(__current->logname,name);
    OSCALLEXIT(OSCALL_setlogin);
    return 0;
  } else {
    errno = EPERM;
    OSCALLEXIT(OSCALL_setlogin);
    return -1;
  }
}
#endif
