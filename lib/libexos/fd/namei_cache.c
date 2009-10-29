
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

#include "namei.h"
#include "fd/cffs/name_cache.h"
#include <stdlib.h>
#include <exos/mallocs.h>

static inline int 
namei_cache_add(struct file *old_filp,char *name, struct file *new_filp);
static inline int 
namei_cache_lookup(struct file *old_filp,char *name, struct file **new_filp);


static nc_t *nc;
static int namei_lookup_count = 0;

int 
namei_lookup(struct file *old_filp, char *name, struct file *new_filp) {
  struct file *filp = NULL;
  static int malloc_count = 0;
  static int mesg = 1;
  int error;

  START(namei,lookup);
  namei_lookup_count++;

  if (namei_cache_lookup(old_filp,name,&filp)) {
    *new_filp = *filp;
  } else {
    /* miss */
    error = DOOP(old_filp,lookup,(old_filp,name,new_filp));
    if (error) {
//      NAMEI_RELEASE(old_filp,&release);
#ifdef NAMEIDEBUG
      printf("LOOKUP old_filp %s failed errno: %s\n",next,strerror(errno));
#endif
      STOP(namei,lookup);
      return -1;
    }
    if (mesg) filp = (struct file *)__malloc(sizeof (struct file));
    if (filp == 0 && mesg) {
      kprintf("malloc failed (on try: %d\n",malloc_count);
      mesg = 0;
    } else {
      malloc_count++;
      *filp = *new_filp;
      namei_cache_add(old_filp,name,filp);
    }
  }
  STOP(namei,lookup);
  return 0;
}



static inline int 
namei_cache_add(struct file *old_filp,char *name, struct file *new_filp) {
  START(namei,cache_add);
  if (old_filp->f_dev > 0 && !( EQDOTDOT(name))) { 
    name_cache_addEntry(nc,(int)old_filp->f_ino,name,strlen(name),(int)new_filp);
  }
  STOP(namei,cache_add);
  return 0;
}

static inline int 
namei_cache_lookup(struct file *old_filp,char *name, struct file **new_filp) {
  int ret = 0;
  START(namei,cache_lookup);
  if (old_filp->f_dev > 0) {
    ret = name_cache_findEntry(nc,(int)old_filp->f_ino,name, strlen(name),
			       (int *)new_filp);
  }
  STOP(namei,cache_lookup);
  return ret;
}

void
namei_cache_remove(struct file *old_filp,char *name, struct file *newfilp) {
#ifdef CACHEDLOOKUP
  START(namei,cache_remove);
  name_cache_removeEntry(nc,old_filp->f_ino,name,strlen(name));
  STOP(namei,cache_remove);
#endif
}

#if 0
static void 
pr_namei_stat(void) {
#ifdef CACHEDLOOKUP
  printf("Namei with cached lookups\n");
#endif
  if (nc && namei_lookup_count) {
    name_cache_printStats(nc);
    name_cache_printContents(nc);
  }
}
#endif

static void 
namei_cache_remove_func(int id, char *name, int value) {
  START(namei,remove);
  __free((void *)value);
  STOP(namei,remove);
}

void 
namei_init(void) {
  START(namei,init);
  nc = name_cache_init(4096,64);
  name_cache_removefunc(namei_cache_remove_func);
#ifdef CACHEDLOOKUP
//  kprintf("Namei with cached lookups\n");
#endif
  assert(nc);

  //atexit(pr_namei_stat);
  STOP(namei,init);
}
