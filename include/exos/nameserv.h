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

/*  Nameserver Interface     dkw 11/96

  To register a service with the nameserver:
    nameserver_register(service_id, server_eid);
 
  To lookup a service:
    nameserver_lookup(service_id);

  To remove a service:
    nameserver_remove(service_id);

  To verify that all services registered have valid eid's:
   nameserver_verify();
   */

#ifndef __NAMESERV_H__
#define __NAMESERV_H__

#include <sys/types.h>
#include <fd/proc.h>		/* for __current */
#include <exos/ipc.h>
#include <exos/ipcdemux.h>
#include <exos/process.h>

#include <assert.h>

/* requests */
#define NS_ADD 1
#define NS_LOOKUP 2
#define NS_REMOVE 3
#define NS_VERIFY 4

/* return codes */
#define NS_OK 0
#define NS_NO_SPACE -2
#define NS_INVAL -3
#define NS_DUP -4
#define NS_BOUNDS -5
#define NS_IPC_FAIL -6

/* nameserver's eid */
#define NS_EID 2311		/* hardcoded based on it being the 4th process */

static inline int nameserver_eid () {
  static int eid = 0;

  if (!eid) {
    assert (global_ftable->nsd_envid);
    eid = global_ftable->nsd_envid;
  }
  return eid;
}

/* registered id's */

#define NS_NM_CFFSD 10
#define NS_NM_SLS   11

#define MAX_RETRIES 100

static inline int nameserver_register (u_int a, u_int b) {
  int ret;
  int ipc_ret;
  int cnt = 0;

 retry:
  ipc_ret = _ipcout_default (nameserver_eid (), 4, &ret, IPC_NS, NS_ADD, a, b);
  if (ipc_ret == IPC_RET_BADENV && cnt++ < MAX_RETRIES) {
    yield (NS_EID);
    goto retry;
  }
  assert (ipc_ret == IPC_RET_OK);
  return ret;;
}

static inline int nameserver_lookup (u_int a) {
  int ret;
  int ipc_ret;
  int cnt = 0;
  
 retry:
  ipc_ret = _ipcout_default (nameserver_eid (), 4, &ret, IPC_NS, NS_LOOKUP, a, 0);
  if (ipc_ret == IPC_RET_BADENV && cnt++ < MAX_RETRIES) {
    yield (NS_EID);
    goto retry;
  }
  assert (ipc_ret == IPC_RET_OK);
  return ret;
}

static inline int nameserver_remove (u_int a) {
  int ret;
  int ipc_ret;
  int cnt = 0;

 retry:
  ipc_ret = _ipcout_default (nameserver_eid (), 4, &ret, IPC_NS, NS_REMOVE, a, 0);
  if (ipc_ret == IPC_RET_BADENV && cnt++ < MAX_RETRIES) {
    yield (NS_EID);
    goto retry;
  }
  assert (ipc_ret == IPC_RET_OK);
  return ret;
}

static inline int nameserver_verify () {
  int ret;
  int ipc_ret;
  int cnt = 0;

 retry:
  ipc_ret = _ipcout_default (nameserver_eid (), 4, &ret, IPC_NS, NS_VERIFY, 0, 0);
  if (ipc_ret == IPC_RET_BADENV && cnt++ < MAX_RETRIES) {
    yield (NS_EID);
    goto retry;
  }
  assert (ipc_ret == IPC_RET_OK);
  return ret;
}

#endif /* __NAMESERV_H__ */


