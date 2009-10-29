
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

#ifndef EXOS_IPC_H
#define EXOS_IPC_H

#include <sys/types.h>
#include <stdarg.h>

struct _ipc_func;
typedef void (*ipc_rethandler)(struct _ipc_func *c, int _ipcret, int a1,
			       int a2, int a3, int a4, u_int caller);
struct _ipc_func {
  ipc_rethandler func;
};

/* flags for _ipcout */
#define IPC_FL_OUT_NONBLOCK 0x1 /* don't block waiting to get to dest env */
#define IPC_FL_RET_NONBLOCK 0x2 /* don't block waiting for return call */

/* return codes for _ipcout */
#define IPC_RET_OK            0 /* no problem */
#define IPC_RET_BADENV        1 /* problem with destination env */
#define IPC_RET_BADFLAGS      2 /* invalid flags argument */
#define IPC_RET_OUTWOULDBLOCK 3 /* if IPC_FL_OUT_NONBLOCK requested and
				   env is not accepting ipc's */
#define IPC_RET_OUTTIMEOUT    4 /* unable to get to env for too long */
#define IPC_RET_RETTIMEOUT    5 /* no response from env for too long */
#define IPC_RET_TOOMANYOUT    6 /* too many unreturned ipc's */
#define IPC_RET_UNKNOWN_ERR   7

/* NOTE: negative ipc return value comes from callee */
int _ipcout(u_int env, int a1, int a2, int a3, int numextraargs,
	    struct _ipc_func *callback, u_int usec_out, u_int usec_ret,
	    int *ret, u_int flags, va_list ap);

/* XXX - backwards compatibility - remove when not used */
static inline int sipcout(u_int env, int a1, int a2, int a3, int a4) {
  int r1, r2;

  r1 = _ipcout(env, a1, a2, a3, 1, 0, 0, 0, &r2, 0, (char*)&a4);

  if (r1) return -1;
  return r2;
}

#define IPC_DEFAULT_OUTTIMEOUT 7000000 /* seven seconds */
#define IPC_DEFAULT_RETTIMEOUT 7000000 /* seven seconds */
#define IPC_DEFAULT_FLAGS 0
int _ipcout_default(u_int env, int numargs, int *ret, ...);

extern void ipc1_entry (void) __attribute__ ((regparm (3)));
extern u_quad_t (*ipc1_in) (int, int, int, int, u_int);
extern void ipc2_entry (void) __attribute__ ((regparm (3)));
extern int (*ipc2_in) (int, int, int, int, int, u_int);
extern int __asm_ipcout (int a1, int a2, int a3, int numextraargs, u_int env,
			 int tableindex, char *extraargs)
     __attribute__ ((regparm (3)));


#endif
