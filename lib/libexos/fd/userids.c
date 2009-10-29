
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
#include <sys/param.h>
#include "fd/proc.h"
#include <sys/ioctl.h>
#include <errno.h>
#include <xok/sys_ucall.h>
#include <exos/conf.h>
#include <exos/cap.h>
#include <exos/callcount.h>
#include <xok/env.h>
#include <xok/sysinfo.h>
#include <exos/osdecl.h>		/* __curenv */

int 
getgroups(int gidsetlen, gid_t gidset[])
{
  int i;
  
  OSCALLENTER(OSCALL_getgroups);
  if (gidsetlen == 0) {
    /* just wants to know how many */
    for (i = 0; __current->groups[i] != GROUP_END ; i++);
  } else {
    for (i = 0; __current->groups[i] != GROUP_END ; i++)
      gidset[i] = __current->groups[i];
  }
  OSCALLEXIT(OSCALL_getgroups);
  return(i);
}

/* HBXX I am not sure if other process should see the changes.  For simplicity
 * they will not, only the future children of this process will */
/* JJJ Shouldn't the check just be that egid/euid does not match
   gid/uid, since euif/egid might be set, but not to root? -jj */
int 
setgroups(int gidsetlen, const gid_t *gidset)
{
  int i;
  int j;

  OSCALLENTER(OSCALL_setgroups);
  if (getuid() == 0 || geteuid() == 0) {
    if (gidsetlen > NGROUPS || gidsetlen < 0) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_setgroups);
      return -1;
    }
    for (i = 0; i < gidsetlen; i++) {
      /* make sure we can touch all the gidset 
       * if we failed we could return errno EFAULT */
      j |= gidset[i];
    }

    for (i = 0; i < gidsetlen; i++) {
      __current->groups[i] = gidset[i];
    }
    __current->groups[i] = GROUP_END;
    OSCALLEXIT(OSCALL_setgroups);
    return 0;
  } else {
    errno = EPERM;
    OSCALLEXIT(OSCALL_setgroups);
    return -1;
  }
}

uid_t 
getuid(void) {
    OSCALLENTER(OSCALL_getuid);
    OSCALLEXIT(OSCALL_getuid);
    return (__current->uid);
}

uid_t 
geteuid(void) {
    OSCALLENTER(OSCALL_geteuid);
    OSCALLEXIT(OSCALL_geteuid);
    return (__current->euid);
}

gid_t 
getgid(void) {
    OSCALLENTER(OSCALL_getgid);
    OSCALLEXIT(OSCALL_getgid);
    return (__current->gid);
}

gid_t 
getegid(void) {
    OSCALLENTER(OSCALL_getegid);
    OSCALLEXIT(OSCALL_getegid);
    return (__current->egid);
}

/* HBXX not 100% of correctness, excerpt from man page:
   The issetugid() function returns 1 if the process was made setuid or set-
   gid as the result of the last execve() system call.  Otherwise it returns
   0.
   */
int 
issetugid(void) {
  int r;

  OSCALLENTER(OSCALL_issetugid);
  r = ((__current->egid == 0 && __current->gid != 0) || 
       (__current->euid == 0 && __current->uid != 0));
  OSCALLEXIT(OSCALL_issetugid);
  return r;
}


int 
setuid(uid_t uid) {
  struct Capability cap;

  OSCALLENTER(OSCALL_setuid);

  /* Layout the desired capability */
  cap.c_perm = CL_ALL;
  cap.c_valid = 1;
  cap.c_isptr = 0;
  cap.c_len = uid ? 3 : 1;
  cap.c_name[0] = 1;
  cap.c_name[1] = uid >> 8;
  cap.c_name[2] = uid & 0xFF;

  /* Try to forge the new uid capability with CAP_EUID */
  if (sys_self_forge(CAP_EUID, CAP_SCRATCH, &cap) < 0) {
    /* Couldn't forge, are we just blowing away the euid? */
    if (uid == __current->uid) {
      sys_self_forge(CAP_UID, CAP_EUID, &__curenv->env_clist[CAP_UID]);
      __current->euid = uid;
      __current->fsuid = uid;
      OSCALLEXIT(OSCALL_setuid);
      return 0;
    } else {
      errno = EPERM;
      OSCALLEXIT(OSCALL_setuid);
      return -1;
    }
  }

  /* CAP_SCRATCH is now a token for `cap', move this into euid and uid */
  /* These can't fail since we just forged `cap' into CAP_SCRATCH */
  sys_self_forge(CAP_SCRATCH, CAP_UID, &cap);
  sys_self_forge(CAP_SCRATCH, CAP_EUID, &cap);
  __current->uid = uid;
  __current->euid = uid;
  __current->fsuid = uid;
  OSCALLEXIT(OSCALL_setuid);
  return 0;
}
  
/* function dropped in 4.4 */
/* I dropped all the setr*id functions b/c it's not as obvious how
   they should map into a capabilities based world. I could emulate
   UNIX exactly, but in sete?[gu]id, I didn't, because I wanted to
   allow the change if you had the capabilities for it, not *just* if
   you were root.  For these it's slightly less obvious, and they're
   cruft anyway -jj */

#if 0				/* Let's drop it too -jj */
int 
setreuid(int uid, int euid) {
  if (uid != -1 ) __current->uid = uid;
  if (euid != -1 ) __current->euid = euid;
  return 0;
}
#endif

int 
seteuid(uid_t euid) {
  struct Capability cap;
  
  OSCALLENTER(OSCALL_seteuid);

  if (__current->euid == euid) {
    OSCALLEXIT(OSCALL_seteuid);
    return 0;
  }

  /* Layout the desired capability */
  cap.c_perm = CL_ALL;
  cap.c_valid = 1;
  cap.c_isptr = 0;
  cap.c_len = euid ? 3 : 1;
  cap.c_name[0] = 1;
  cap.c_name[1] = euid >> 8;
  cap.c_name[2] = euid & 0xFF;

  /* Try to forge the new uid capability with CAP_UID */
  if (sys_self_forge(CAP_UID, CAP_EUID, &cap) < 0) {
    errno = EPERM;
    OSCALLEXIT(OSCALL_seteuid);
    return -1;
  }

  __current->euid = euid;
  OSCALLEXIT(OSCALL_seteuid);
  return 0;
}

#if 0				/* Drop this gunk.  -jj */
int 
setruid(uid_t ruid) {
    __current->uid = ruid;
    return 0;
}
#endif

int 
setgid(gid_t gid) {
  struct Capability cap;

  OSCALLENTER(OSCALL_setgid);

  /* Layout the desired capability */
  cap.c_perm = CL_DUP | CL_W;
  cap.c_valid = 1;
  cap.c_isptr = 0;
  cap.c_len = gid ? 3 : 1;
  cap.c_name[0] = 2;
  cap.c_name[1] = gid >> 8;
  cap.c_name[2] = gid & 0xFF;

  /* Try to forge the new gid capability with CAP_EGID and CAP_EUID */
  if (sys_self_forge(CAP_EGID, CAP_SCRATCH, &cap) < 0  &&
      sys_self_forge(CAP_EUID, CAP_SCRATCH, &cap) < 0) {
    /* Couldn't forge, Use CAP_GID only if we are setting egid back to gid */
    if (gid == __current->gid) {
      sys_self_forge(CAP_GID, CAP_EGID, &__curenv->env_clist[CAP_GID]);
      __current->egid = gid;
      OSCALLEXIT(OSCALL_setgid);
      return 0;
    } else {
      errno = EPERM;
      OSCALLEXIT(OSCALL_setgid);
      return -1;
    }
  }

  /* CAP_SCRATCH is now a token for `cap', move this into euid and uid */
  /* These can't fail since we know CAP_SCRATCH dominates/equals `cap' */
  sys_self_forge(CAP_SCRATCH, CAP_GID, &cap);
  sys_self_forge(CAP_SCRATCH, CAP_EGID, &cap);
  __current->gid = gid;
  __current->egid = gid;
  OSCALLEXIT(OSCALL_setgid);
  return 0;
}

/* function dropped in 4.4 */
#if 0				/* Drop it -jj */
int 
setregid(int gid, int egid) {
  if (gid != -1) __current->gid = gid;
  if (egid != -1) __current->egid = egid;
  return 0;
}
#endif

int 
setegid(gid_t egid) {
  struct Capability cap;

  OSCALLENTER(OSCALL_setegid);

  if (__current->egid == egid)
    return 0;

  /* Layout the desired capability */
  cap.c_perm = CL_DUP | CL_W;
  cap.c_valid = 1;
  cap.c_isptr = 0;
  cap.c_len = egid ? 3 : 1;
  cap.c_name[0] = 2;
  cap.c_name[1] = egid >> 8;
  cap.c_name[2] = egid & 0xFF;

  /* Try to forge the new gid capability with CAP_GID */
  if (sys_self_forge(CAP_GID, CAP_EGID, &cap) < 0) {
    errno = EPERM;
    OSCALLEXIT(OSCALL_setegid);
    return -1;
  }

  __current->egid = egid;
  OSCALLEXIT(OSCALL_setegid);
  return 0;
}

#if 0				/* Drop it. -jj */
int 
setrgid(gid_t rgid) {
    __current->gid = rgid;
    return 0;
}
#endif


mode_t 
umask(mode_t numask) {
  mode_t ret;

  OSCALLENTER(OSCALL_umask);
  ret = __current->umask;
  __current->umask = numask;
  OSCALLEXIT(OSCALL_umask);
  return ret;
}
