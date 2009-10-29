
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
#include <xok/env.h>
#include <xok/sys_ucall.h>	/* for sys_geteid */
#include <exos/ipcdemux.h>
#include <stdio.h>
#include <exos/process.h>
#include <exos/critical.h>
#include <assert.h>
#include <exos/nameserv.h>
#include <fd/proc.h>		/* for __current and global_ftable */

#define MAXSZ 1024  
int names[MAXSZ];
#define NS_FREE 0

int ns_request (int demux_id, int req, int name, int env, uint caller) {
  int ret,i,j;\

  if (req == NS_ADD) {
    if (name < 0 || name >= MAXSZ) {
      kprintf("ns: request to add out-of-bounds name\n");
      ret = NS_BOUNDS;
      goto out;
    }
    if (names[name] != NS_FREE) {
      kprintf ("ns: request for name already in use\n");
      ret = NS_DUP;
      goto out;
    }
    if (env < 0 || env < NENV) {
      kprintf ("ns: request to add bogus env %d by env %d\n", env, caller);
      ret = NS_INVAL;
      goto out;
    }

    if (__envs[envidx(env)].env_status != ENV_OK) {
      kprintf ("ns: request to add non-existant env %d by env %d\n", env, caller);
      ret = NS_INVAL;
      goto out;
    }
    kprintf("ns: service %d added at eid %d\n",name, env);
    names[name] = env;
    ret = NS_OK;
    goto out;
  } else if (req == NS_LOOKUP) {
    if (names[name] == NS_FREE) {
      kprintf ("ns: request to lookup non-existant env\n");
      ret = NS_INVAL;
      goto out;
    }
    ret = names[name];
    goto out;
  } else if (req == NS_REMOVE) {
    if (names[name] == NS_FREE) {
      kprintf("ns: request to remove non-existant env\n");
      ret = NS_INVAL;
      goto out;
    }
    names[name] = NS_FREE;
    ret = NS_OK;
    goto out;
  } else if (req == NS_VERIFY) {
    j = 0;
    for (i = 0; i < MAXSZ; i++) {
      if (__envs[envidx(names[i])].env_status != ENV_OK) {
	kprintf("ns: removed binding to env %d on verify\n",names[i]);
	names[i] = NS_FREE;
	j++;
      }
    }
    ret = j;
    goto out;
  } else {
    kprintf ("ns: received bogus request from env %d\n", caller);
    ret = NS_INVAL;
    goto out;
  }
out: 
  return (ret);
}

#ifdef STANDALONE
int main () {
#else
int ns_main () {
#endif
  int i;

  for (i = 0; i < MAXSZ; i++) {
    names[i] = NS_FREE;
  }

  ipcdemux_register(IPC_NS, ns_request, 5);

  /* advertise our eid */

  global_ftable->nsd_envid = sys_geteid ();

  kprintf ("environment name server started at pid %d\n", getpid ());

  EnterCritical ();

  UAREA.u_status = U_SLEEP;
  yield(-1);

  assert (0);
}



