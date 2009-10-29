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

#include <stdio.h>
#include <xok/sys_ucall.h>
#include <exos/process.h>

#include <exos/shm.h>
#include <exos/locks.h>
#include <exos/vm-layout.h>

#include <sys/shm.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#define dprintf if (0) kprintf


/* giant exos lock: init to 0 so we know if we got a real lock or not */
exos_lock_t *global_lock = (exos_lock_t*) 0;

#ifdef __LOCKS_DEBUG__
u_int* locks_debug_table;
#endif

void 
ExosLocksInit()
{
  int Segment;
  int NeedsInit = 0;

  StaticAssert(EXOS_LOCKS_SHM_SZ < EXOS_LOCKS_SHARED_REGION_SZ);

  /* see if the shared segment already created */
  Segment = shmget(EXOS_LOCKS_SHM_OFFSET, EXOS_LOCKS_SHM_SZ, 0);

  /* if not, create it */
  if (Segment == -1) {
    if (errno == ENOENT) {
      /* create shared segment */
      Segment = shmget(EXOS_LOCKS_SHM_OFFSET, EXOS_LOCKS_SHM_SZ, IPC_CREAT);
      NeedsInit = 1;
    }

    if (Segment == -1) {
      panic ("ExosLocksInit: Could not create shared segment for MP locks");
    }
  }

  if ((global_lock = (exos_lock_t*) 
      shmat(Segment, (char*)EXOS_LOCKS_SHARED_REGION, 0)) == (exos_lock_t*)-1) 
  {
    global_lock = (exos_lock_t*) 0;
    panic("ExosLocksInit: Could not attach global_lock");
  }

  if (NeedsInit)
    /* if we just created the lock, initialize it */
  {
    exos_lock_init(global_lock);
  }
#ifdef __LOCKS_DEBUG__
  exos_locks_debug_init();
#endif
  
  dprintf("ExosLocksInit: SHM obtained\n");
}


