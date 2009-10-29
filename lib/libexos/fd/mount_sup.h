
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

#ifndef __MOUNT_SUP_H__
#define __MOUNT_SUP_H__

#include <exos/debug.h>
#include <exos/locks.h>
#include <fd/proc.h>

int switch_to_private_mount_table ();
int mount_superblock (u_int dev, u_int sb, char *path);
int __unmount (const char *path, int weak);
int __unmount_all ();

static inline void 
clear_mt_lock(void) {
    DPRINTF(LOCK_LEVEL,("clear_mt_lock\n"));
    exos_lock_init(&global_ftable->mt->lock);
}

static inline void 
lock_mt(void) { 
  DPRINTF(LOCK_LEVEL,("lock_mt\n"));
  exos_lock_get_nb(&global_ftable->mt->lock);
}

static inline void
unlock_mt(void) { 
    DPRINTF(LOCK_LEVEL,("unlock_mt\n"));
    exos_lock_release(&global_ftable->mt->lock);
}


static inline void 
pr_mount(mount_entry_t * m) {
  printf("f_flags: %x, inuse: %d\n",m->f_flags,m->inuse);
  printf("f_mntonname: %s\n",m->f_mntonname);
  printf("f_mntfromname: %s\n",m->f_mntfromname);
  pr_filp(&m->from,"FROM FILP");
  pr_filp(&m->to,"TO FILP  ");
}
static inline int
pr_mounts(void) {
  int i;
  int count = 0;
  mount_entry_t *m;
  m = global_ftable->mt->mentries;
  //  printf("nr_entries: %d\n",global_ftable->mt->nr_mentries);
  for (i = 0; i < NR_MOUNT; i++) {
    //    printf("MENTRIE: %2d  Used: %d\n",i,m->inuse);
    if (m->inuse) {
      printf ("%s mounted on %s\n", m->f_mntfromname, m->f_mntonname);
    }
    m++;
  }
  return count;
}

/* support subroutines */

/* tentatively acquires the slot */
static inline int 
getslot(void) {
  mounttable_t *mt;
  mount_entry_t *mnt;
  int i;
  mt = global_ftable->mt;
  mnt = mt->mentries;
  
  for (i = 0; i < NR_MOUNT; i++) {
    if (mnt->inuse == 0) {
      if (i > mt->nr_mentries) {
	demand(0, i > mt->nr_mentries);
      }
      mnt->inuse = -1;
      if (i == global_ftable->mt->nr_mentries) global_ftable->mt->nr_mentries++;
      return i;
    }
    mnt++;
  }
  return -1;
}

/* commits the adquisition of the slot */
static inline void 
setslot(int slot) {
  mounttable_t *mt;
  demand(slot >= 0 && slot < NR_MOUNT,slot out of range);
  mt = global_ftable->mt;
  mt->mentries[slot].inuse = 1;
}

/* return true if slot is in use */
static inline int
isslotset (int slot) {
  mounttable_t *mt;
  int ret;

  demand(slot >= 0 && slot < NR_MOUNT,slot out of range);
  mt = global_ftable->mt;

  if (mt->mentries[slot].inuse == 1) {
    ret = 1;
  } else {
    ret = 0;
  }

  return ret;
}

/* return the max numbered slot in use */
static inline int
maxslot () {
  mounttable_t *mt;

  mt = global_ftable->mt;
  return mt->nr_mentries;
}

/* returns the slot */
static inline void
putslot(int slot) {
  mounttable_t *mt;

  demand(slot >= 0 && slot < NR_MOUNT,slot out of range);
  mt = global_ftable->mt;

  if (mt->mentries[slot].inuse == 0) {
      printf("Warning: putslot called to an unused slot\n");
      assert (0);
  }
  mt->mentries[slot].inuse = 0;
  if (slot == (mt->nr_mentries - 1)) mt->nr_mentries--;
}

#endif /* __MOUNT_SUP_H__ */
