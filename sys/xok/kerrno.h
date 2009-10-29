
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

#ifndef _XOK_KERRNO_H_
#define _XOK_KERRNO_H_

/* Error values syscalls can return */

#define E_UNSPECIFIED	1	/* Unspecified or unknown problem */
#define E_BAD_ENV       2       /* Environment doesn't exist or otherwise
				   cannot be used in requested action */
#define E_INVAL		3	/* Invalid parameter */
#define E_VSPACE	4	/* Not enough virtual memory space reserved */
#define E_NO_MEM	5	/* Request failed due to memory shortage */

#define E_SHARE	 	6	/* Share mode of object prevents action */
#define E_NOT_FOUND	7	/* Name not found */
#define E_EXISTS	8	/* Object already exists */
#define E_FAULT		10	/* Virtual address would fault */
#define E_RVMI		11	/* Remote virtual memory invalid */

#define E_UNAVAIL       12	/* Syscall unavailable (e.g. to uniprocessor) */
#define E_CPU_INVALID   13	/* The CPU given does not exist in the system */

#define E_CAP_INVALID   14      /* Capability doesn't exist or is marked
				   invalid */
#define E_CAP_INSUFF    15      /* Capability has insufficient access rights
				   to requested object */
#define E_NO_FREE_ENV   16      /* Attempt to create a new environment beyond
				   the maximum allowed */
#define E_NO_XN         17      /* Attempt to use XN on non-XN configed
				   disk */
#define E_NO_NETTAPS    18      /* No nettaps left to complete operation */

#define E_NO_PKTRINGS   20      /* No pktrings left to complete operation */
#define E_NO_DPFS       21      /* No filters left to complete operation */
#define E_ACL_SIZE      22      /* Attempt to make acl too big */
#define E_ACL_INVALID   23      /* Acl is (or would be if created) invalid */
#define E_XTNT_SIZE     24      /* Not enough bptr's in pxn for new extent */
#define E_XTNT_RDONLY   25	/* trying to add a read-only extent to a pxn */
#define E_BUFFER	26	/* page has a buffer when it sholdn't or
				   doesn't when it should */
#define E_QUOTA         27	/* Allocation would exceed quota */
#define E_TOO_BIG       28      /* Request requires too much work for one
				   kernel call, or too many resources */

#define E_IPC_BLOCKED   35      /* Attempt to ipc to env blocking ipc's */
#define E_IPI_BLOCKED   36	/* Attempt to ipi blocked by target CPU */
#define E_ENV_RUNNING   37	/* environment running on another CPU */

#define E_NO_MSGRING    40      /* no msg rings left to complete operation */
#define E_MSGRING_FULL 	41	/* msg ring full */

#define E_INTR		100	/* Finished prematurely */



#endif
