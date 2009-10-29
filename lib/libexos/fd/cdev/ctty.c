
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

/*	$OpenBSD: tty_tty.c,v 1.6 1997/11/06 05:58:24 csapuntz Exp $	*/
/*	$NetBSD: tty_tty.c,v 1.13 1996/03/30 22:24:46 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tty_tty.c	8.2 (Berkeley) 9/23/93
 */

/*
 * Indirect driver for controlling tty.
 */
#include <sys/param.h>
#include <sys/ioctl.h>
//#include <sys/proc.h>
#include "opty.h"
#include "os/procd.h"
#include "fd/proc.h"

#include "sw.h"
#define _KERNEL
#include <sys/tty.h>
#undef _KERNEL


#define cttyvp(p) ((p)->p_flag & P_CONTROLT ? (p)->p_session->s_ttyvp : NULL)

/*ARGSUSED*/
int
cttyopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct file *ttyvp = cttyvp(p);
	struct tty *tp;
	dev_t device;

	if (ttyvp == NULL)
		return (ENXIO);
	device = ttyvp->f_dev;
	
	tp = (*cdevsw[major(device)].d_tty)(device);
	if (tp->t_state&TS_XCLUDE && GET_CR_UID(curproc) != 0) {
	  return (EBUSY);
	}
	return 0;
}

/*ARGSUSED*/
int
cttyread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct file *ttyvp = cttyvp(curproc);
	int error;
	dev_t device;

	if (ttyvp == NULL)
		return (EIO);
	device = ttyvp->f_dev;

	error = (*cdevsw[major(device)].d_read)(device,uio, flag);
	return (error);
}

/*ARGSUSED*/
int
cttywrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct file *ttyvp = cttyvp(curproc);
	int error;
	dev_t device;

	if (ttyvp == NULL)
		return (EIO);
	device = ttyvp->f_dev;

	error = (*cdevsw[major(device)].d_write)(device,uio, flag);
	return (error);
}

/*ARGSUSED*/
int
cttyioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct file *ttyvp = cttyvp(p);
	int error;
	dev_t device;

	if (ttyvp == NULL)
		return (EIO);
	if (cmd == TIOCSCTTY)		/* XXX */
		return (EINVAL);
	if (cmd == TIOCNOTTY) {
		if (!SESS_LEADER(p)) {
#ifdef LIBEXOS_TTY
		  {int ret = proc_controlt_clearflag(); assert(ret == 0);}
#else
			p->p_flag &= ~P_CONTROLT;
#endif
			return (0);
		} else
			return (EINVAL);
	}
	device = ttyvp->f_dev;
	error = (*cdevsw[major(device)].d_ioctl)(device,cmd, addr, flag, p);
	return error;
}

/*ARGSUSED*/
int
cttyselect(dev, flag, p)
	dev_t dev;
	int flag;
	struct proc *p;
{
	struct file *ttyvp = cttyvp(p);
	int error;
	dev_t device;

	if (ttyvp == NULL)
		return (1);	/* try operation to get EOF/failure */

	device = ttyvp->f_dev;
	error = (*cdevsw[major(device)].d_select)(device, flag, p);
	return error;
}
