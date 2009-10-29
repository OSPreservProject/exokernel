
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

/* For multiplexing the ipc entry point(s) */

#include <xok/env.h>
#include <xok/ipc.h>
#include <xok/sys_ucall.h>
#include <exos/ipcdemux.h>

#define IPCDEMUX_LAST 255

static struct {
  ipcdemux_handler handler;
  int numargs;
} ipcs[IPCDEMUX_LAST+1]; /* Statically init'd to NULLs */

/* Calls the handler for code, returns the result of the call, or
   -1 if there was no handler. */
static u_quad_t ipc1_demux(int, int, int, int, u_int)
     __attribute__ ((regparm (3)));
static u_quad_t ipc1_demux(int code, int a, int b, int numextraargs,
			   u_int caller) {
  u_quad_t r;

  if (code <= IPCDEMUX_LAST && ipcs[code].handler &&
      ipcs[code].numargs - 4 <= numextraargs) {
    ((u_int*)&r)[1] = 0;
    switch (ipcs[code].numargs) {
    case 2: ((u_int*)&r)[0] = ipcs[code].handler(code, caller); break;
    case 3: ((u_int*)&r)[0] = ipcs[code].handler(code, a, caller); break;
    case 4: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b, caller); break;
    case 5: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						 UAREA.u_ipc1_extra_args[0],
						 caller); break;
    case 6: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						 UAREA.u_ipc1_extra_args[0],
						 UAREA.u_ipc1_extra_args[1],
						 caller); break;
    case 7: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						 UAREA.u_ipc1_extra_args[0],
						 UAREA.u_ipc1_extra_args[1],
						 UAREA.u_ipc1_extra_args[2],
						 caller); break;
    case 8: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						 UAREA.u_ipc1_extra_args[0],
						 UAREA.u_ipc1_extra_args[1],
						 UAREA.u_ipc1_extra_args[2],
						 UAREA.u_ipc1_extra_args[3],
						 caller); break;
    case 9: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						 UAREA.u_ipc1_extra_args[0],
						 UAREA.u_ipc1_extra_args[1],
						 UAREA.u_ipc1_extra_args[2],
						 UAREA.u_ipc1_extra_args[3],
						 UAREA.u_ipc1_extra_args[4],
						 caller); break;
    case 10: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  caller); break;
    case 11: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  caller); break;
    case 12: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						 caller); break;
    case 13: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  caller); break;
    case 14: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  UAREA.u_ipc1_extra_args[9],
						  caller); break;
    case 15: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  UAREA.u_ipc1_extra_args[9],
						  UAREA.u_ipc1_extra_args[10],
						  caller); break;
    case 16: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  UAREA.u_ipc1_extra_args[9],
						  UAREA.u_ipc1_extra_args[10],
						  UAREA.u_ipc1_extra_args[11],
						  caller); break;
    case 17: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  UAREA.u_ipc1_extra_args[9],
						  UAREA.u_ipc1_extra_args[10],
						  UAREA.u_ipc1_extra_args[11],
						  UAREA.u_ipc1_extra_args[12],
						  caller); break;
    case 18: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  UAREA.u_ipc1_extra_args[9],
						  UAREA.u_ipc1_extra_args[10],
						  UAREA.u_ipc1_extra_args[11],
						  UAREA.u_ipc1_extra_args[12],
						  UAREA.u_ipc1_extra_args[13],
						  caller); break;
    case 19: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  UAREA.u_ipc1_extra_args[9],
						  UAREA.u_ipc1_extra_args[10],
						  UAREA.u_ipc1_extra_args[11],
						  UAREA.u_ipc1_extra_args[12],
						  UAREA.u_ipc1_extra_args[13],
						  UAREA.u_ipc1_extra_args[14],
						  caller); break;
    case 20: ((u_int*)&r)[0] = ipcs[code].handler(code, a, b,
						  UAREA.u_ipc1_extra_args[0],
						  UAREA.u_ipc1_extra_args[1],
						  UAREA.u_ipc1_extra_args[2],
						  UAREA.u_ipc1_extra_args[3],
						  UAREA.u_ipc1_extra_args[4],
						  UAREA.u_ipc1_extra_args[5],
						  UAREA.u_ipc1_extra_args[6],
						  UAREA.u_ipc1_extra_args[7],
						  UAREA.u_ipc1_extra_args[8],
						  UAREA.u_ipc1_extra_args[9],
						  UAREA.u_ipc1_extra_args[10],
						  UAREA.u_ipc1_extra_args[11],
						  UAREA.u_ipc1_extra_args[12],
						  UAREA.u_ipc1_extra_args[13],
						  UAREA.u_ipc1_extra_args[14],
						  UAREA.u_ipc1_extra_args[15],
						  caller); break;
    default:
      ((int*)&r)[1] = IPCDEMUX_RET_NO_HAND;
      break;
    }
  } else
    ((int*)&r)[1] = IPCDEMUX_RET_NO_HAND;
 return r;
}

/* Install the demuxer as the one ipc1 handler for LibOS programs */
static int ipcdemux_inited = 0;
static void ipcdemux_init() {
  ipc1_in = ipc1_demux;
  UAREA.u_entipc1 = (u_int)&ipc1_entry;
  sys_set_allowipc1(XOK_IPC_ALLOWED);
  ipcdemux_inited = 1;
}

/* Register NULL to remove a handler.
   Handler always gets code as first argument and caller as last argument,
   thus a minimum of 2 for numargs  */
int ipcdemux_register(int code, ipcdemux_handler handler, int numargs) { 
  if (!ipcdemux_inited) ipcdemux_init();
  if (code > IPCDEMUX_LAST || numargs < 2 || numargs > U_MAX_EXTRAIPCARGS+4)
    return -1;
  ipcs[code].handler = 0;
  ipcs[code].numargs = numargs;
  ipcs[code].handler = handler;
  return 0;
}

/* XXX - remove once everyone switched to new ipc code
   Register NULL to remove a handler. */
void
ipc_register(int code, ipcdemux_handler handler) {
  ipcdemux_register(code, handler, 5);
}
