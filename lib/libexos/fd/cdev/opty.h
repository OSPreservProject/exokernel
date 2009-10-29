
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
#ifndef _OPTY_H_
#define _OPTY_H_
#include <exos/critical.h>
#include <exos/kprintf.h>

#include "fd/proc.h"

#define LIBEXOS_TTY
/* used by tty_subr.c  */

#define spltty() (EnterCritical(),  0)
#define splx(x) (ExitCritical())

/* get credentials for process p */
extern uid_t getuid(void);
extern pid_t getpid(void);
#define GET_CR_UID(p) (getuid())		/* ->p_ucred->cr_uid */

/* FNONBLOCK is another non blocking flag */
#define IO_NDELAY       0x10            /* FNDELAY flag set in file table */
static inline int GENFLAG(struct file *filp) {
  int flag = 0;
  if (CHECKNB(filp)) flag |= IO_NDELAY|FNONBLOCK;
  if (filp->f_flags & O_WRONLY) flag |= FWRITE;
  else if (filp->f_flags & O_RDWR) flag |= (FWRITE|FREAD);
  else flag |= FREAD;
  return flag;
}
/* use to wake up processes once a second */
extern int lbolt;

/* size of clist */
#define CLALLOCSZ 1024
#define NUMBER_PTY 8

/* we can't assume that function pointers will be the same in all programs  */
enum oproc_t { OPROC_NONE = 0, OPROC_PTSSTART };
struct tty;
extern void ptsstart (struct tty *);

#endif /* _OPTY_H_ */
