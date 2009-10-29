
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

#include <exos/cap.h>
#include <xok/env.h>
#include <exos/osdecl.h>
#include <xok/sys_ucall.h>
#include <xok/mmu.h>

#include <stdio.h>
#include <limits.h>
#include <fd/proc.h>
#include <string.h>

void
InitCaps() {
  struct Capability cap;
  int r;

  /* clear cname */
  bzero (cap.c_name, sizeof (cap.c_name));

  /* JJJ Keep root capability here until we handle caps better. */
  sys_self_forge(CAP_ENV, CAP_ROOT, &__curenv->env_clist[CAP_ENV]);

  cap.c_valid = 1;
  cap.c_isptr = 0;


  cap.c_len = 1;
  
  cap.c_perm = CL_ALL;
  cap.c_name[0] = CAP_NM_UID;
  sys_self_forge(CAP_ROOT, CAP_UID, &cap);
  sys_self_forge(CAP_ROOT, CAP_EUID, &cap);
  
  cap.c_perm = CL_DUP | CL_W;
  cap.c_name[0] = CAP_NM_GID;
  sys_self_forge(CAP_ROOT, CAP_GID, &cap);
  sys_self_forge(CAP_ROOT, CAP_EGID, &cap);


  cap.c_len = 3;
  cap.c_name[1] = 0;
  cap.c_name[2] = 0;
  sys_self_forge(CAP_ROOT, CAP_WORLD, &cap);


  sys_pgacl_setsize(CAP_ROOT, sys_read_pte(UADDR, CAP_ROOT, __envid, &r),
		    2);
  sys_pgacl_mod(CAP_ROOT, sys_read_pte(UADDR, CAP_ROOT, __envid, &r),
		2, &cap);
}

int CopyCaps(u_int k, int envid, int ignore) {
  int i;
  /* JJJ CAP_ROOT should be CAP_LASTGID later. */
  for (i = 1; (i < __curenv->env_clen) && (i <= CAP_ROOT); i++) {
    /* We don't care when this fails, the point is  */
    /* only to forge duplicates when possible. -jj */
    sys_forge(i, i, &__curenv->env_clist[i], k, envid);
  }
  return 0;
}
