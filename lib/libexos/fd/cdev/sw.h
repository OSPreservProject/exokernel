
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

#include <sys/cdefs.h>
#include <sys/uio.h>
#include <exos/uio.h>
struct proc;
int     nullop __P((void *));
int     nullioctl __P((struct tty *tp,u_long cmd,char *data, 
	      int flags, struct proc *p));
int     enodev __P((void));
int     enosys __P((void));
int     enoioctl __P((void));
int     enxio __P((void));
int     eopnotsupp __P((void));


int   ptsopen   (dev_t, int, int, struct proc *);
int   ptsclose   (dev_t, int, int, struct proc *);
int   ptsread   (dev_t, struct uio *, int);
int   ptswrite   (dev_t, struct uio *, int);
int   ttselect (dev_t dev,int rw,struct proc *p);
int   ptyioctl   (dev_t, u_long, caddr_t, int, struct proc *);
int   ptsstop   (struct tty *, int);
struct tty *  ptytty    (dev_t);


int   ptcopen   (dev_t, int, int, struct proc *);
int   ptcclose   (dev_t,
		  int, int, struct proc *);
int   ptcread   (dev_t, struct uio *, int);
int   ptcwrite   (dev_t, struct uio *, int);
int   ptcselect(dev_t dev,int rw,struct proc *p);
int   ptyioctl   (dev_t, u_long, caddr_t, int, struct proc *);
int   ptcstop   (struct tty *, int);
struct tty *  ptytty    (dev_t);


struct cdevsw {
	int	(*d_open)	__P((dev_t dev, int oflags, int devtype,
				     struct proc *p));
	int	(*d_close)	__P((dev_t dev, int fflag, int devtype,
				     struct proc *));
	int	(*d_read)	__P((dev_t dev, struct uio *uio, int ioflag));
	int	(*d_write)	__P((dev_t dev, struct uio *uio, int ioflag));
	int	(*d_ioctl)	__P((dev_t dev, u_long cmd, caddr_t data,
				     int fflag, struct proc *p));
	int	(*d_stop)	__P((struct tty *tp, int rw));
	struct tty *
		(*d_tty)	__P((dev_t dev));
	int	(*d_select)	__P((dev_t dev, int which, struct proc *p));
	int	(*d_mmap)	__P((dev_t, int, int));
	int	d_type;
};

struct linesw {
	int	(*l_open)	__P((dev_t dev, struct tty *tp));
	int	(*l_close)	__P((struct tty *tp, int flags));
	int	(*l_read)	__P((struct tty *tp, struct uio *uio,
				     int flag));
	int	(*l_write)	__P((struct tty *tp, struct uio *uio,
				     int flag));
	int	(*l_ioctl)	__P((struct tty *tp, u_long cmd, caddr_t data,
				     int flag, struct proc *p));
	int	(*l_rint)	__P((int c, struct tty *tp));
	int	(*l_start)	__P((struct tty *tp));
	int	(*l_modem)	__P((struct tty *tp, int flag));
};
extern int nlinesw;
extern struct linesw linesw[];
extern struct cdevsw cdevsw[];
