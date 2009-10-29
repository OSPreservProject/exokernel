
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

/*	$OpenBSD: tty_pty.c,v 1.5 1997/02/24 14:20:00 niklas Exp $	*/
/*	$NetBSD: tty_pty.c,v 1.33.4.1 1996/06/02 09:08:11 mrg Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)tty_pty.c	8.2 (Berkeley) 9/23/93
 */

/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */


#include "opty.h"
#include "os/procd.h"
#include "fd/proc.h"
#include "exos/vm-layout.h"
#include <xok/mmu.h>
#include "sw.h"
#include <exos/signal.h>
#include <exos/osdecl.h>

#include <sys/param.h>
#include <sys/ioctl.h>
#define _KERNEL
#include <sys/tty.h>
#undef _KERNEL
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#undef	TTYDEFCHARS 
#include <sys/time.h>
#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/systm.h>


#ifdef LIBEXOS_TTY
#define psignal proc_psignal
#define pgsignal proc_pgsignal
#endif



#undef BUFSIZ			/* from stdio.h */
#define BUFSIZ 100		/* Chunk size iomoved to/from user */

/*
 * pts == /dev/tty[pqrs]?
 * ptc == /dev/pty[pqrs]?
 */
#ifdef LIBEXOS_TTY
/*
 * At compile time, choose:
 * There are two ways the TTY_QUOTE bit can be stored. If QBITS is
 * defined we allocate an array of bits -- 1/8th as much memory but
 * setbit(), clrbit(), and isset() take more cpu. If QBITS is
 * undefined, we just use an array of bytes.
 * 
 * If TTY_QUOTE functionality isn't required by a line discipline,
 * it can free c_cq and set it to NULL. This speeds things up,
 * and also does not use any extra memory. This is useful for (say)
 * a SLIP line discipline that wants a 32K ring buffer for data
 * but doesn't need quoting.
 */
#define QBITS

#ifdef QBITS
#define QMEM(n)		((((n)-1)/NBBY)+1)
#else
#define QMEM(n)		(n)
#endif


/* HBXX - for protection make sure this structure is page aligned */
struct	pt_softc {
  struct	tty *pt_tty;
  int	pt_flags;
  struct	selinfo pt_selr, pt_selw;
  u_char	pt_send;
  u_char	pt_ucntl;

  struct tty    tty0;
  int refcount[2];		/* 0 slave, 1 master */
  char rawq[CLALLOCSZ];
  char canq[CLALLOCSZ];
  char outq[CLALLOCSZ];
  char rawquot[QMEM(CLALLOCSZ)];
  char canquot[QMEM(CLALLOCSZ)];
  char padding[0];
} *pt_softc;

static inline void INCREF(dev_t device) {
  assert(major(device) == 0 || major(device) == 1);
  assert(minor(device) >= 0 || minor(device) < NUMBER_PTY);
  pt_softc[major(device)].refcount[minor(device)]++;
}
static inline int DECREF(dev_t device) {
  assert(major(device) == 0 || major(device) == 1);
  assert(minor(device) >= 0 || minor(device) < NUMBER_PTY);
  return --(pt_softc[major(device)].refcount[minor(device)]);
}

/* similar to tty_subr.c:clalloc but does not use malloc */
static void clprealloc(struct clist *clp, int size, int quot, char *cs, char *cq) {
  clp->c_cs = cs;
  bzero(clp->c_cs, size);
  if (quot) {
    clp->c_cq = cq; 
    bzero(clp->c_cq, QMEM(size));
  } else {
    clp->c_cq = (u_char *)0; 
  }
  clp->c_cf = clp->c_cl = (u_char *)0;
  clp->c_ce = clp->c_cs + size;
  clp->c_cn = size;
  clp->c_cc = 0;
}
void pt_softc_prealloc1(struct pt_softc *pti) {
  struct tty *tp;
  bzero(pti,sizeof(struct pt_softc));
  /* preallocate tty */
  tp = pti->pt_tty = &pti->tty0;
  /* preallocate rawq (with quoting) */
  clprealloc(&tp->t_rawq, CLALLOCSZ, 1, &pti->rawq[0], &pti->rawquot[0]);
  /* preallocate canq (with quoting) */
  clprealloc(&tp->t_canq, CLALLOCSZ, 1, &pti->canq[0], &pti->canquot[0]);
  /* preallocate outq (without quoting) */
  clprealloc(&tp->t_outq, CLALLOCSZ, 0, &pti->outq[0], NULL);
  /* extra since the bzero already takes care of this */
  pti->refcount[0] = pti->refcount[1] = 0;
}
int
newpty_init(void) {
  int status,i;
  status = fd_shm_alloc(NEWPTY_SHM_OFFSET,
			(sizeof(struct pt_softc)*NUMBER_PTY),
			(char *)NEWPTY_SHARED_REGION);
      
  StaticAssert((sizeof(struct pt_softc)*NUMBER_PTY) <= NEWPTY_SHARED_REGION_SZ);
  if (status == -1) {
    demand(0, problems attaching shm);
    return -1;
  }
  pt_softc = (struct pt_softc *)NEWPTY_SHARED_REGION;
  if (status) {
    for (i = 0; i < NUMBER_PTY; i++) {
      pt_softc_prealloc1(&pt_softc[i]);
    }
  }
  return 0;
}

int npty = NUMBER_PTY;
#else 
#if NPTY == 1
#undef NPTY
#define	NPTY	32		/* crude XXX */
#endif

struct	pt_softc {
	struct	tty *pt_tty;
	int	pt_flags;
	struct	selinfo pt_selr, pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
} pt_softc[NPTY];		/* XXX */

int	npty = NPTY;		/* for pstat -t */

#endif /* LIBEXOS_TTY */


#define	PF_PKT		0x08		/* packet mode */
#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_REMOTE	0x20		/* remote and flow controlled input */
#define	PF_NOSTOP	0x40
#define PF_UCNTL	0x80		/* user control mode */

void	ptyattach __P((int));
void	ptcwakeup __P((struct tty *, int));
struct tty *ptytty __P((dev_t));
void	ptsstart __P((struct tty *));

/*
 * Establish n (or default if n is 1) ptys in the system.
 */
void
ptyattach(n)
	int n;
{
#ifdef notyet
#define	DEFAULT_NPTY	32
#error "This should not be compiled"
	/* maybe should allow 0 => none? */
	if (n <= 1)
		n = DEFAULT_NPTY;
	pt_softc = __malloc(n * sizeof(struct pt_softc), M_DEVBUF, M_WAITOK);
	npty = n;
#endif
}

/* 
 ************************
 *   SLAVE PROCEDURES   *
 ************************
 */

/*ARGSUSED*/
/* Slave PTY cdevsw */
int
ptsopen(dev, flag, devtype, p)
	dev_t dev;
	int flag, devtype;
	struct proc *p;
{
	struct pt_softc *pti;
	register struct tty *tp;
	int error;

	if (minor(dev) >= npty)
		return (ENXIO);
	pti = &pt_softc[minor(dev)];
#ifdef LIBEXOS_TTY
	assert(pti->pt_tty);	/* all terminal structures are preallocated */
#else
	if (!pti->pt_tty) {
		tp = pti->pt_tty = ttymalloc();
		tty_attach(tp);
	}
	else
#endif /* LIBEXOS_TTY */
		tp = pti->pt_tty;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);		/* Set up default chars */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);		/* would be done in xxparam() */
	
#ifdef LIBEXOS_TTY
	} else if (tp->t_state&TS_XCLUDE && GET_CR_UID(p) != 0) {
#else
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0) {
#endif /* LIBEXOS_TTY */
	  return (EBUSY);}
	if (tp->t_oproc)			/* Ctrlr still around. */
		tp->t_state |= TS_CARR_ON;
	while ((tp->t_state & TS_CARR_ON) == 0) {
		tp->t_state |= TS_WOPEN;
		if (flag&FNONBLOCK)
			break;
		error = ttysleep(tp, (caddr_t)&tp->t_rawq, TTIPRI | PCATCH,
				 ttopen, 0);
		if (error)
			return (error);
	}
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	ptcwakeup(tp, FREAD|FWRITE);
#ifdef LIBEXOS_TTY
	if (error == 0) INCREF(dev);
#endif
	return (error);
}

int
ptsclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;
	int error;

#ifdef LIBEXOS_TTY
	/* difference in calls to open/close of LibEXOS and OpenBSD */
	if (DECREF(dev) > 0) return 0;
#endif

	error = (*linesw[tp->t_line].l_close)(tp, flag);
	error |= ttyclose(tp);
	/* HBXX - I had something here to check if otherside was close
	 check here if memory leak
	 */
	ptcwakeup(tp, FREAD|FWRITE);
	return (error);
}

int
ptsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct proc *p = curproc;
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;
	int error = 0;

again:
	if (pti->pt_flags & PF_REMOTE) {
		while (isbackground(p, tp)) {
		        if (
#ifdef LIBEXOS_TTY
			    __issigignored(SIGTTIN) || __issigmasked(SIGTTIN) ||
#else
			    (p->p_sigignore & sigmask(SIGTTIN)) ||
			    (p->p_sigmask & sigmask(SIGTTIN)) ||
#endif
			    p->p_pgrp->pg_jobc == 0 || 
			    p->p_flag & P_PPWAIT)

				return (EIO);
			pgsignal(p->p_pgrp, SIGTTIN, 1);
			error = ttysleep(tp, (caddr_t)&lbolt, 
					 TTIPRI | PCATCH, ttybg, 0);
			if (error)
				return (error);
		}
		if (tp->t_canq.c_cc == 0) {
			if (flag & IO_NDELAY)
				return (EWOULDBLOCK);
			error = ttysleep(tp, (caddr_t)&tp->t_canq,
					 TTIPRI | PCATCH, ttyin, 0);
			if (error)
				return (error);
			goto again;
		}
		while (tp->t_canq.c_cc > 1 && uio->uio_resid > 0)
			if (ureadc(cl_getc(&tp->t_canq), uio) < 0) {
				error = EFAULT;
				break;
			}
		if (tp->t_canq.c_cc == 1)
			(void) cl_getc(&tp->t_canq);
		if (tp->t_canq.c_cc)
			return (error);
	} else
		if (tp->t_oproc)
			error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
	ptcwakeup(tp, FWRITE);
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
int
ptswrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;

	if (tp->t_oproc == 0)
		return (EIO);
	/* HBXX check here if not calling proper write */
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
void
ptsstart(tp)
	struct tty *tp;
{
	register struct pt_softc *pti = &pt_softc[minor(tp->t_dev)];

	if (tp->t_state & TS_TTSTOP)
		return;
	if (pti->pt_flags & PF_STOPPED) {
		pti->pt_flags &= ~PF_STOPPED;
		pti->pt_send = TIOCPKT_START;
	}
	ptcwakeup(tp, FREAD);
}

int
ptsstop(tp, flush)
	register struct tty *tp;
	int flush;
{
	struct pt_softc *pti = &pt_softc[minor(tp->t_dev)];
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pti->pt_flags |= PF_STOPPED;
	} else
		pti->pt_flags &= ~PF_STOPPED;
	pti->pt_send |= flush;
	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	ptcwakeup(tp, flag);
	return 0;
}

void
ptcwakeup(tp, flag)
	struct tty *tp;
	int flag;
{
	struct pt_softc *pti = &pt_softc[minor(tp->t_dev)];

	if (flag & FREAD) {
		selwakeup(&pti->pt_selr);
		wakeup((caddr_t)&tp->t_outq.c_cf);
	}
	if (flag & FWRITE) {
		selwakeup(&pti->pt_selw);
		wakeup((caddr_t)&tp->t_rawq.c_cf);
	}
}

/* 
 ************************
 *  MASTER PROCEDURES   *
 ************************
 */

int ptcopen __P((dev_t, int, int, struct proc *));

/*ARGSUSED*/
int
ptcopen(dev, flag, devtype, p)
	dev_t dev;
	int flag, devtype;
	struct proc *p;
{
	struct pt_softc *pti;
	register struct tty *tp;

	if (minor(dev) >= npty)
		return (ENXIO);
	pti = &pt_softc[minor(dev)];
#ifdef LIBEXOS_TTY
	assert(pti->pt_tty);	/* all terminal structures are preallocated */
#else
	if (!pti->pt_tty) {
		tp = pti->pt_tty = ttymalloc();
		tty_attach(tp);
	}
	else
#endif /* LIBEXOS_TTY */
		tp = pti->pt_tty;
	if (tp->t_oproc)
		return (EIO);
	/* HBXX - be careful when assigning and comparing procedure addresses
	* this is a libos, so address for the same procedure could be in different
	* places on two processes */
	tp->t_oproc = (void *)OPROC_PTSSTART;
	(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	tp->t_lflag &= ~EXTPROC;
	pti->pt_flags = 0;
	pti->pt_send = 0;
	pti->pt_ucntl = 0;
#ifdef LIBEXOS_TTY
	INCREF(dev);
#endif
	return (0);
}

/*ARGSUSED*/
int
ptcclose(dev, flag, devtype, p)
	dev_t dev;
	int flag, devtype;
	struct proc *p;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;
#ifdef LIBEXOS_TTY
	/* difference in calls to open/close of LibEXOS and OpenBSD */
	if (DECREF(dev) > 0) return 0;
#endif

	(void)(*linesw[tp->t_line].l_modem)(tp, 0);
	tp->t_state &= ~TS_CARR_ON;
	tp->t_oproc = 0;		/* mark closed */
	/* HBXX - maybe should check oproc or t_state to deallocate tty now */
	return (0);
}

int
ptcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;
	char buf[BUFSIZ];
	int error = 0, cc;

	/*
	 * We want to block until the slave
	 * is open, and there's something to read;
	 * but if we lost the slave or we're NBIO,
	 * then return the appropriate error instead.
	 */
	for (;;) {
		if (tp->t_state&TS_ISOPEN) {
			if (pti->pt_flags&PF_PKT && pti->pt_send) {
				error = ureadc((int)pti->pt_send, uio);
				if (error)
					return (error);
				if (pti->pt_send & TIOCPKT_IOCTL) {
					cc = min(uio->uio_resid,
						sizeof(tp->t_termios));
					uiomove((caddr_t) &tp->t_termios,
						cc, uio);
				}
				pti->pt_send = 0;
				return (0);
			}
			if (pti->pt_flags&PF_UCNTL && pti->pt_ucntl) {
				error = ureadc((int)pti->pt_ucntl, uio);
				if (error)
					return (error);
				pti->pt_ucntl = 0;
				return (0);
			}
			if (tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0)
				break;
		}
		if ((tp->t_state&TS_CARR_ON) == 0)
			return (0);	/* EOF */
		if (flag & IO_NDELAY)
			return (EWOULDBLOCK);
		error = tsleep((caddr_t)&tp->t_outq.c_cf, TTIPRI | PCATCH,
			       ttyin, 0);
		if (error)
			return (error);
	}
	if (pti->pt_flags & (PF_PKT|PF_UCNTL))
		error = ureadc(0, uio);
	while (uio->uio_resid > 0 && error == 0) {
		cc = q_to_b(&tp->t_outq, buf, min(uio->uio_resid, BUFSIZ));
		if (cc <= 0)
			break;
		error = uiomove(buf, cc, uio);
	}
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	return (error);
}


int
ptcwrite(dev, uio, flag)
	dev_t dev;
	register struct uio *uio;
	int flag;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;
	register u_char *cp = NULL;
	register int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	int error = 0;

again:
	if ((tp->t_state&TS_ISOPEN) == 0)
		goto block;
	if (pti->pt_flags & PF_REMOTE) {
		if (tp->t_canq.c_cc)
			goto block;
		while (uio->uio_resid > 0 && tp->t_canq.c_cc < TTYHOG - 1) {
			if (cc == 0) {
				cc = min(uio->uio_resid, BUFSIZ);
				cc = min(cc, TTYHOG - 1 - tp->t_canq.c_cc);
				cp = locbuf;
				error = uiomove((caddr_t)cp, cc, uio);
				if (error)
					return (error);
				/* check again for safety */
				if ((tp->t_state&TS_ISOPEN) == 0)
					return (EIO);
			}
			if (cc)
				(void) b_to_q((char *)cp, cc, &tp->t_canq);
			cc = 0;
		}
		(void) cl_putc(0, &tp->t_canq);
		ttwakeup(tp);
		wakeup((caddr_t)&tp->t_canq);
		return (0);
	}
	while (uio->uio_resid > 0) {
		if (cc == 0) {
			cc = min(uio->uio_resid, BUFSIZ);
			cp = locbuf;
			error = uiomove((caddr_t)cp, cc, uio);
			if (error)
				return (error);
#if 0				/* HBXX DEBUGGING */
			do {
			  int i;
			  kprintf("TO WRITE: ");
			  for (i = 0; i < cc; i++) {
			    kprintf("-%d (%c)",(unsigned int)locbuf[i],locbuf[i]);
			  }
			  kprintf("\n");
			} while(0);
#endif
			/* check again for safety */
			if ((tp->t_state&TS_ISOPEN) == 0)
				return (EIO);
		}
		while (cc > 0) {
			if ((tp->t_rawq.c_cc + tp->t_canq.c_cc) >= TTYHOG - 2 &&
			   (tp->t_canq.c_cc > 0 || !(tp->t_iflag&ICANON))) {
				wakeup((caddr_t)&tp->t_rawq);
				goto block;
			}
			(*linesw[tp->t_line].l_rint)(*cp++, tp);
			cnt++;
			cc--;
		}
		cc = 0;
	}
	return (0);
block:
	/*
	 * Come here to wait for slave to open, for space
	 * in outq, or space in rawq.
	 */
	if ((tp->t_state&TS_CARR_ON) == 0)
		return (EIO);
	if (flag & IO_NDELAY) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		if (cnt == 0)
			return (EWOULDBLOCK);
		return (0);
	}
	error = tsleep((caddr_t)&tp->t_rawq.c_cf, TTOPRI | PCATCH,
		       ttyout, 0);
	if (error) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (error);
	}
	goto again;
}

int
ptcselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;
	int s;

	if ((tp->t_state&TS_CARR_ON) == 0)
		return (1);
	switch (rw) {
	case FREAD:
		/*
		 * Need to block timeouts (ttrstart).
		 */
		s = spltty();
		if ((tp->t_state&TS_ISOPEN) &&
		     tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0) {
			splx(s);
			return (1);
		}
		splx(s);
		/* FALLTHROUGH */
	case 0:					/* exceptional */
		if ((tp->t_state&TS_ISOPEN) &&
		    (((pti->pt_flags & PF_PKT) && pti->pt_send) ||
		     ((pti->pt_flags & PF_UCNTL) && pti->pt_ucntl)))
			return (1);
#ifdef LIBEXOS_TTY
		selrecord(FREAD, &pti->pt_selr);
		selrecord(0, &pti->pt_selr);
#else
		selrecord(p, &pti->pt_selr);
#endif
		break;

	case FWRITE:
		if (tp->t_state&TS_ISOPEN) {
			if (pti->pt_flags & PF_REMOTE) {
			    if (tp->t_canq.c_cc == 0)
				return (1);
			} else {
			    if (tp->t_rawq.c_cc + tp->t_canq.c_cc < TTYHOG-2)
				    return (1);
			    if (tp->t_canq.c_cc == 0 && (tp->t_iflag&ICANON))
				    return (1);
			}
		}
#ifdef LIBEXOS_TTY
		selrecord(FWRITE, &pti->pt_selw);
#else
		selrecord(p, &pti->pt_selw);
#endif
		break;

	}
	return (0);
}


struct tty *
ptytty(dev)
	dev_t dev;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;

	return (tp);
}

/*ARGSUSED*/
int
ptyioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct pt_softc *pti = &pt_softc[minor(dev)];
	register struct tty *tp = pti->pt_tty;
	register u_char *cc = tp->t_cc;
	int stop, error;

	/*
	 * IF CONTROLLER STTY THEN MUST FLUSH TO PREVENT A HANG.
	 * ttywflush(tp) will hang if there are characters in the outq.
	 */
	if (cmd == TIOCEXT) {
		/*
		 * When the EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pti->pt_flags & PF_PKT) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag |= EXTPROC;
		} else {
			if ((tp->t_state & EXTPROC) &&
			    (pti->pt_flags & PF_PKT)) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag &= ~EXTPROC;
		}
		return(0);
	} else
	  /* HBXX - CAVEAT this test might not work */
	if (cdevsw[major(dev)].d_open == ptcopen)
		switch (cmd) {

		case TIOCGPGRP:
			/*
			 * We aviod calling ttioctl on the controller since,
			 * in that case, tp must be the controlling terminal.
			 */
			*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : 0;
			return (0);

		case TIOCPKT:
			if (*(int *)data) {
				if (pti->pt_flags & PF_UCNTL)
					return (EINVAL);
				pti->pt_flags |= PF_PKT;
			} else
				pti->pt_flags &= ~PF_PKT;
			return (0);

		case TIOCUCNTL:
			if (*(int *)data) {
				if (pti->pt_flags & PF_PKT)
					return (EINVAL);
				pti->pt_flags |= PF_UCNTL;
			} else
				pti->pt_flags &= ~PF_UCNTL;
			return (0);

		case TIOCREMOTE:
			if (*(int *)data)
				pti->pt_flags |= PF_REMOTE;
			else
				pti->pt_flags &= ~PF_REMOTE;
			__ttyflush(tp, FREAD|FWRITE);
			return (0);

#ifdef COMPAT_OLDTTY
		case TIOCSETP:		
		case TIOCSETN:
#endif
		case TIOCSETD:
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
			ndflush(&tp->t_outq, tp->t_outq.c_cc);
			break;

		case TIOCSIG:
			if (*(unsigned int *)data >= NSIG)
				return(EINVAL);
			if ((tp->t_lflag&NOFLSH) == 0)
				__ttyflush(tp, FREAD|FWRITE);
			pgsignal(tp->t_pgrp, *(unsigned int *)data, 1);
#ifndef LIBEXOS_TTY
			if ((*(unsigned int *)data == SIGINFO) &&
			    ((tp->t_lflag&NOKERNINFO) == 0))
				ttyinfo(tp);
#endif
			return(0);
		}
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error < 0)
		 error = ttioctl(tp, cmd, data, flag, p);
	if (error < 0) {
		if (pti->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pti->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return (0);
		}
		error = ENOTTY;
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if ((tp->t_lflag&EXTPROC) && (pti->pt_flags & PF_PKT)) {
		switch(cmd) {
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
#ifdef COMPAT_OLDTTY
		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
#endif
			pti->pt_send |= TIOCPKT_IOCTL;
			ptcwakeup(tp, FREAD);
		default:
			break;
		}
	}
	stop = (tp->t_iflag & IXON) && CCEQ(cc[VSTOP], CTRL('s')) 
		&& CCEQ(cc[VSTART], CTRL('q'));
	if (pti->pt_flags & PF_NOSTOP) {
		if (stop) {
			pti->pt_send &= ~TIOCPKT_NOSTOP;
			pti->pt_send |= TIOCPKT_DOSTOP;
			pti->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pti->pt_send &= ~TIOCPKT_DOSTOP;
			pti->pt_send |= TIOCPKT_NOSTOP;
			pti->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}
	return (error);
}
