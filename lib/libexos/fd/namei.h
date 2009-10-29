
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

#include "fd/proc.h"
#include <string.h>
#include <assert.h>

extern unsigned sleep (unsigned);


#undef ASSERTOP
#define ASSERTOP(filp,op,msg) do {		\
  if (CHECKOP(filp,op) == 0) {			\
    printf("%s", #msg );			\
    sleep(15);                                   \
    assert(filp);				\
    pr_filp(filp,"bad filp");			\
    demand(0, filp ## _has no op );		\
    errno = EBADF;				\
    return -1;					\
  }						\
  } while(0)

const char *
getNextComponent (const char *path, char *next);

void 
SplitPath(const char *path, char *suffix, char *prefix);

/* you can do EQFILP on struct file's and struct shortfile's */
struct shortfile {
  dev_t f_dev;
  ino_t f_ino;
};

#define CPSHORTFILP(dest,src) \
  do { (dest)->f_dev = (src)->f_dev; (dest)->f_ino = (src)->f_ino; } while(0);

#undef NAMEI_RELEASE
#define NAMEI_RELEASE(filp,rel)						\
  if ((rel) && FILPEQ((filp),(rel)) || !(rel))	{			\
    PRSHORTFILP("RELEASING: ",filp,"\n");				\
    if (CHECKOP(filp,release)) DOOP(filp,release,(filp));	\
  }
