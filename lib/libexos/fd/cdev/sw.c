
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

/* this file has something of tty_conf.c or conf.c from OpenBSD */

#include "os/procd.h"

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <exos/uio.h>
#define _KERNEL
#define LIBEXOS_TTY
#include <sys/tty.h>
#undef _KERNEL

#include "sw.h"


/*
 * Do nothing specific version of line
 * discipline specific ioctl command.
 */
/*ARGSUSED*/
int
nullioctl(tp, cmd, data, flags, p)
        struct tty *tp;
        u_long cmd;
        char *data;
        int flags;
        struct proc *p;
{
#ifdef lint
        tp = tp; data = data; flags = flags; p = p;
#endif
        return (-1);
}

struct linesw linesw[] = {
  {ttyopen, ttylclose, ttread, ttwrite, 
   nullioctl, ttyinput, ttstart, nullmodem }
};
int nlinesw = sizeof(linesw) / sizeof(struct linesw);

#if 0
#define cdev_ctty_init(c,n) { \
        dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
        dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
        0, dev_init(c,n,select), (dev_type_mmap((*))) enodev, D_TTY }
#endif
#ifdef NEWPTY
int   cttyopen   (dev_t, int, int, struct proc *);
int   cttyread   (dev_t, struct uio *, int);
int   cttywrite   (dev_t, struct uio *, int);
int   cttyselect (dev_t dev,int rw,struct proc *p);
int   cttyioctl   (dev_t, u_long, caddr_t, int, struct proc *);
#endif

struct cdevsw cdevsw[] = {
  /* slave is 0 master is 1 */
  {   ptsopen  ,   ptsclose  ,   ptsread  ,   ptswrite  ,   
      ptyioctl   ,
      ptsstop  ,   
      ptytty   , 
      ttselect,
      (int  (*)   (dev_t, int, int)   ) enodev, 
      0
  } ,         
  {   ptcopen  ,   ptcclose  ,   ptcread  ,   ptcwrite  ,   
      ptyioctl   ,
      (int  (*)   (struct tty *, int)   ) nullop,   
      ptytty   ,   
      ptcselect,
      (int  (*)   (dev_t, int, int)   ) enodev, 
      0  } ,         
#ifdef NEWPTY
  {   cttyopen,   (int (*)(dev_t, int, int, struct proc *))nullop, cttyread, cttywrite,
      cttyioctl, 
      (int (*) (struct tty *, int)) nullop, 
      0,
      cttyselect, 
      (int  (*)   (dev_t, int, int)   ) enodev, 
      0 } ,
#endif
};

#if 0
...
#define ptstty          ptytty
#define ptsioctl        ptyioctl
cdev_decl(pts);

#define ptctty          ptytty
#define ptcioctl        ptyioctl
cdev_decl(ptc);
#endif
