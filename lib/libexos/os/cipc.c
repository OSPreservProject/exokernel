
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

#include <exos/ipc.h>
#include <exos/uwk.h>
#include <xok/ipc.h>
#include <xok/kerrno.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <exos/critical.h>


/* information about each outstanding ipc call */
struct _ipc_outstanding {
  u_int envid;
  u_int done;
#define IPC_OUTSTANDING_DONE_RET  0x1 /* The return ipc2 has occurred */
#define IPC_OUTSTANDING_DONE_CONT 0x2 /* Continuation after ipc1 call point
					 has occurred */
  u_int ret;
  u_int ipcret;
  struct _ipc_func *func;
};

#define IPC_MAX_OUTSTANDING 100
struct _ipc_outstanding _ipcs_outstanding[IPC_MAX_OUTSTANDING];
int _ipc_next_outstanding = -1;
int _ipc_num_outstanding = 0;

static int ipc2_demux(int a, int b, int c, int d, int index, u_int caller)
     __attribute__ ((regparm (3)));
static int ipc2_demux(int a, int b, int c, int d, int index,
		      u_int caller) {
  UAREA.u_status--;
  if ((u_int)index >= IPC_MAX_OUTSTANDING ||
      _ipcs_outstanding[index].envid != caller)
    return 0; /* bad ipc2 call, ignore */

  _ipcs_outstanding[index].ret = a;
  _ipcs_outstanding[index].ipcret = -abs(d);
  _ipcs_outstanding[index].done |= IPC_OUTSTANDING_DONE_RET;
  if (_ipcs_outstanding[index].func) {
    _ipcs_outstanding[index].func->func(_ipcs_outstanding[index].func,
					_ipcs_outstanding[index].ipcret,
					a, b, c, d, caller);
    _ipcs_outstanding[index].func = 0;
  }
  return _ipcs_outstanding[index].done;
}

/* setup the return path (ipc2) ipc handler */
static int ipc_inited = 0;
static void ipc_init() {
  ipc2_in = ipc2_demux;
  UAREA.u_entipc2 = (u_int) &ipc2_entry;
  sys_set_allowipc2(XOK_IPC_ALLOWED);
  ipc_inited = 1;
}

/* NOTE: negative ipc value comes from callee */
int _ipcout(u_int env, int a1, int a2, int a3, int numextraargs,
	    struct _ipc_func *callback, u_int usec_out, u_int usec_ret,
	    int *ret, u_int flags, va_list ap) {
  int aret;
  int orig_ipc_next_outstanding;
  int new_outstanding;
  struct wk_term t[UWK_MKCMP_EQ_PRED_SIZE + 1 +
		  2 * (UWK_MKCMP_NEQ_PRED_SIZE + 1) +
		  UWK_MKSLEEP_PRED_SIZE + 1];
  int wk_sz=0;
  u_quad_t ticks=0;

  if (!ipc_inited) ipc_init();

  /* check for bad args */
  if ((flags | (IPC_FL_OUT_NONBLOCK|IPC_FL_RET_NONBLOCK)) !=
      (IPC_FL_OUT_NONBLOCK|IPC_FL_RET_NONBLOCK)) {
    return IPC_RET_BADFLAGS;
  }

  /* if first ipc then init outstanding structure */
  /* XXX - synchronize/make reentrant/etx */
  if (_ipc_next_outstanding == -1) {
    _ipc_next_outstanding = 0;
    bzero(_ipcs_outstanding,
	  IPC_MAX_OUTSTANDING*sizeof(struct _ipc_outstanding));
  }

  /* if already too many outstanding then return error */
  if (_ipc_num_outstanding == IPC_MAX_OUTSTANDING)
    return IPC_RET_TOOMANYOUT;
  orig_ipc_next_outstanding = _ipc_next_outstanding;
  while (_ipcs_outstanding[_ipc_next_outstanding].envid != 0) {
    _ipc_next_outstanding =
      (_ipc_next_outstanding + 1) % IPC_MAX_OUTSTANDING;
    if (_ipc_next_outstanding == orig_ipc_next_outstanding)
      return IPC_RET_TOOMANYOUT;
  }
  _ipc_num_outstanding++;
  new_outstanding = _ipc_next_outstanding;
  _ipcs_outstanding[new_outstanding].envid = env;
  _ipcs_outstanding[new_outstanding].done = 0;
  _ipcs_outstanding[new_outstanding].func = callback;

  do {
    aret = __asm_ipcout(a1, a2, a3, numextraargs, env, new_outstanding, ap);
    switch (aret) {
    case 0: break; /* success */
    case -E_BAD_ENV:
      _ipcs_outstanding[new_outstanding].envid = 0;
      _ipc_num_outstanding--;
      return IPC_RET_BADENV;
    case -E_IPC_BLOCKED:
      if (flags & IPC_FL_OUT_NONBLOCK) {
	_ipcs_outstanding[new_outstanding].envid = 0;
	_ipc_num_outstanding--;
	return IPC_RET_OUTWOULDBLOCK;
      }
      /* setup timeout and try ipc again */
      if (!wk_sz) {
	wk_sz = wk_mkcmp_eq_pred(&t[0],
				 &__envs[envidx(env)].env_allowipc1,
				 XOK_IPC_ALLOWED, 0);
	wk_sz = wk_mkop (wk_sz, t, WK_OR);
	wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
				   &__envs[envidx(env)].env_status,
				   ENV_OK, 0);
	wk_sz = wk_mkop (wk_sz, t, WK_OR);
	wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
				   &__envs[envidx(env)].env_id, env, 0);
	if (usec_out) {
	  ticks = ((usec_out + __sysinfo.si_rate - 1)
		   / __sysinfo.si_rate) + __sysinfo.si_system_ticks;
	  wk_sz += wk_mkusleep_pred(&t[wk_sz], usec_out);
	}
      }
      wk_waitfor_pred_directed(t, wk_sz, env);
      if (usec_out && ticks < __sysinfo.si_system_ticks) {
	_ipcs_outstanding[new_outstanding].envid = 0;
	_ipc_num_outstanding--;
	return IPC_RET_OUTTIMEOUT;
      }
      break;
    default:
      _ipcs_outstanding[new_outstanding].envid = 0;
      _ipc_num_outstanding--;
      if (aret > 0) return -aret;
      return IPC_RET_UNKNOWN_ERR;
    }
  } while (aret != 0);

  dlockputs(__IPC_LD,"ipcout get lock ");
  EXOS_LOCK(IPC_LOCK);
  dlockputs(__IPC_LD,"... got lock\n");
  EnterCritical();
  /* ipc already returned */
  if (_ipcs_outstanding[new_outstanding].done) {
    EXOS_UNLOCK(IPC_LOCK);
    dlockputs(__IPC_LD,"ipcout release lock\n");
    ExitCritical();
    if (ret) *ret = _ipcs_outstanding[new_outstanding].ret;
    if (!_ipcs_outstanding[new_outstanding].func) {
      _ipcs_outstanding[new_outstanding].envid = 0;
      _ipc_num_outstanding--;
    }
    return IPC_RET_OK;
  }
  _ipcs_outstanding[new_outstanding].done = IPC_OUTSTANDING_DONE_CONT;
  EXOS_UNLOCK(IPC_LOCK);
  dlockputs(__IPC_LD,"ipcout release lock\n");
  ExitCritical();

  if (flags & IPC_FL_RET_NONBLOCK) return IPC_RET_OK;

  /* wait for return */
  wk_sz = wk_mkcmp_eq_pred(&t[0],
			   &_ipcs_outstanding[new_outstanding].done,
			   IPC_OUTSTANDING_DONE_CONT|IPC_OUTSTANDING_DONE_RET,
			   0);
  wk_sz = wk_mkop (wk_sz, t, WK_OR);
  wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
			     &__envs[envidx(env)].env_status,
			     ENV_OK, 0);
  wk_sz = wk_mkop (wk_sz, t, WK_OR);
  wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
			     &__envs[envidx(env)].env_id, env, 0);
  if (usec_ret) {
    ticks = ((usec_ret + __sysinfo.si_rate - 1)
	     / __sysinfo.si_rate) + __sysinfo.si_system_ticks;
    wk_sz = wk_mkop (wk_sz, t, WK_OR);
    wk_sz += wk_mkusleep_pred(&t[wk_sz], usec_ret);
  }

  wk_waitfor_pred_directed(t, wk_sz, env);
  if (_ipcs_outstanding[new_outstanding].done & IPC_OUTSTANDING_DONE_RET) {
    if (ret) *ret = _ipcs_outstanding[new_outstanding].ret;
    aret = _ipcs_outstanding[new_outstanding].ipcret;
    if (!_ipcs_outstanding[new_outstanding].func) {
      _ipcs_outstanding[new_outstanding].envid = 0;
      _ipc_num_outstanding--;
    }
    return aret;
  }

  _ipcs_outstanding[new_outstanding].envid = 0;
  _ipc_num_outstanding--;

  if (usec_ret && ticks < __sysinfo.si_system_ticks)
    return IPC_RET_RETTIMEOUT;

  if (__envs[envidx(env)].env_status != ENV_OK)
    return IPC_RET_BADENV;

  return IPC_RET_UNKNOWN_ERR;
}

int _ipcout_default(u_int env, int numargs, int *ret, ...) {
  int a1=0, a2=0, a3=0;
  va_list ap;

  va_start(ap, ret);
  if (numargs) {
    a1 = va_arg(ap, int);
    numargs--;
    if (numargs) {
      a2 = va_arg(ap, int);
      numargs--;
      if (numargs) {
	a3 = va_arg(ap, int);
	numargs--;
      }
    }
  }
  
  a1 = _ipcout(env, a1, a2, a3, numargs, 0, IPC_DEFAULT_OUTTIMEOUT,
	       IPC_DEFAULT_RETTIMEOUT, ret, IPC_DEFAULT_FLAGS, ap);
  va_end(ap);
  return a1;
}
