
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

#include <fd/proc.h>
#include <stdlib.h>
#include <string.h>

#include <xok/sysinfo.h>


#ifdef FDSTAT
/* if you add something here add it to fdstat.h */
struct fdstatistics 
  fd_fd,
  fd_fd_op[NR_OPS],
  fd_ftable,
  fd_namei,
  fd_nfs_lookup,
  fd_nfsc,fd_misc;


extern void (*pr_fd_statp)(void);
extern void pr_fd_stat(void);

static int ok_printfdstat(void);

void fd_stat_init(void) {
#ifdef PRINTFDSTAT
  int i;
  APPLYM(fd,FDSTATINIT);
  APPLYM(ftable,FDSTATINIT);
  APPLYM(namei,FDSTATINIT);
  APPLYM(nfs_lookup,FDSTATINIT);
  APPLYM(nfsc,FDSTATINIT);
  APPLYM(misc,FDSTATINIT);
  for (i = 0; i< NR_OPS; i++) {
    APPLYM(fd_op[i],FDSTATINIT);
  }
  if (ok_printfdstat()) {
    kprintf("setting pr_fd_statp\n");
    pr_fd_statp = pr_fd_stat;
  }
#endif
}



/* looks at environment variable PRINTFDSTAT which is a 
 colon separated list of programs (ls:csh:...) that should 
 dump statistics at end of run. */
static int ok_printfdstat(void) {
  extern char *__progname;
  char *progname;
  char *progs;
  int len;

  if (!(progs = getenv("PRINTFDSTAT"))) return 0;

  if (!(progname = rindex(__progname,'/'))) {
    progname = __progname;
  } else {
    progname++;
  }
  len = strlen(progname);

  while(*progs) {
    if (!strncmp(progname,progs,len)) return 1;
    progs = index(progs,':');
    if (!progs) break;
    progs++;
  } 
  return 0;
}

#else 

void fd_stat_init(void) {}

#endif /* FDSTAT */
