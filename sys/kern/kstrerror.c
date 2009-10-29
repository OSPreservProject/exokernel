
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

#include <xok/kerrno.h>

static char *kerrlist[] = {
  /*  0 */  [0] = "No error",

  /*  1 */  [E_UNSPECIFIED] = "Unspecified or unknown problem with syscall",

  /*  2 */  [E_BAD_ENV] = "Environment doesn't exist or otherwise "
	    "cannot be used in requested action",
  /*  3 */  [E_INVAL] = "Invalid parameter",

  /*  4 */

  /*  5 */  [E_NO_MEM] = "Request failed due to memory shortage",

  /*  6 */  [E_SHARE] = "Share mode of object prevents action",

  /*  7 */  [E_NOT_FOUND] = "Name not found",

  /*  8 */  [E_EXISTS] = "Object already exists",

  /*  9 */

  /* 10 */  [E_FAULT] = "Virtual address would fault",

  /* 11 */  [E_RVMI] = "Remote virtual memory invalid",

  /* 12 */  [E_IPC_BLOCKED] = "Attempt to ipc to env blocking ipc's",

  /* 13 */

  /* 14 */  [E_CAP_INVALID] = "Capability doesn't exist or is marked "
	    "invalid",
  /* 15 */  [E_CAP_INSUFF] = "Capability has insufficient access rights "
	    "to requested object",
  /* 16 */  [E_NO_FREE_ENV] = "Attempt to create a new environment beyond "
	    "the maximum allowed",
  /* 17 */  [E_NO_XN] = "Attempt to use XN on non-XN configed disk",

  /* 18 */  [E_NO_NETTAPS] = "No nettaps left to complete operation",

  /* 19 */

  /* 20 */  [E_NO_PKTRINGS] = "No pktrings left to complete operation",

  /* 21 */  [E_NO_DPFS] = "No filters left to complete operation",
  /* 22 */  [E_ACL_SIZE] = "Attempt to make acl too big",
  /* 23 */  [E_ACL_INVALID] = "Acl is (or would be if created) "
	    "invalid",
};

int _knerr = { sizeof kerrlist/sizeof kerrlist[0] };

/* Return pointer to string description of error. Accepts the positive or
   negative version of the errcode. */
char *kstrerror(int r) {
  if (r < 0) r *= -1;
  if (r > _knerr || !kerrlist[r]) return "Uknown error";
  return kerrlist[r];
}
