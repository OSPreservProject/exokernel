
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

#include <xok/bc.h>
#include <exos/ubc.h>
#include <xok/sysinfo.h>
#include <exos/process.h>
#include <exos/critical.h>
#include <xok/env.h>
#include <stdio.h>
#include <exos/kprintf.h>

#include "../fd/proc.h"

/* misc user-level functions for dealing with the buffer cache */

struct bc_entry *__bc_lookup (u32 d, u32 b) {
  struct bc_entry *buf;

  dlockputs(__BC_LD,"bc_lookup get lock ");
  EXOS_LOCK(UBC_LOCK);
  dlockputs(__BC_LD,"... got lock\n");
  EnterCritical ();
  buf = KLIST_UPTR (&__bc.bc_qs[__bc_hash (d, b)].lh_first, UBC);
  while (buf) {
    if (buf->buf_dev == d && buf->buf_blk == b) {
      EXOS_UNLOCK(UBC_LOCK);
      dlockputs(__BC_LD,"bc_lookup release lock\n");
      ExitCritical ();
      return (buf);
    }
    buf = KLIST_UPTR (&buf->buf_link.le_next, UBC);
  }

  EXOS_UNLOCK(UBC_LOCK);
  dlockputs(__BC_LD,"bc_lookup release lock\n");
  ExitCritical ();
  return (NULL);
}

struct bc_entry *__bc_lookup64 (u32 d, u_quad_t b) {
  struct bc_entry *buf;

  dlockputs(__BC_LD,"bc_lookup64 get lock ");
  EXOS_LOCK(UBC_LOCK);
  dlockputs(__BC_LD,"... got lock\n");
  EnterCritical ();
  buf = KLIST_UPTR (&__bc.bc_qs[__bc_hash64 (d, b)].lh_first, UBC);
  while (buf) {
    if (buf->buf_dev == d && buf->buf_blk64 == b) {
      EXOS_UNLOCK(UBC_LOCK);
      dlockputs(__BC_LD,"bc_lookup64 release lock\n");
      ExitCritical ();
      return (buf);
    }
    buf = KLIST_UPTR (&buf->buf_link.le_next, UBC);
  }

  EXOS_UNLOCK(UBC_LOCK);
  dlockputs(__BC_LD,"bc_lookup64 release lock\n");
  ExitCritical ();
  return (NULL);
}

#if 0 

/* XXX not used anymore */
 
/* Ask the sync daemon to flush some blocks at some point in the future */

int __syncd_request_writeback (u32 envid, u32 dev, u32 start, u32 num, u32 when) {
  return (sipcout (envid, dev, start, num, when));
}

int __syncd_set_env(u_int envid) {
  global_ftable->syncd_envid = envid;
  return 0;
}

static inline unsigned int __syncd_get_env(void) {
  return global_ftable->syncd_envid;
}

unsigned int __syncd_lookup_env () {
  u_int envid;
  envid = __syncd_get_env();
  if (envid == 0)
    goto no_env;
  if (__envs[envidx(envid)].env_status != ENV_OK)
    goto no_env;
  return (envid);
no_env:
  __syncd_set_env(0);
  return 0;
}

#endif
