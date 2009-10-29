
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

#ifndef _PTY_H_
#define _PTY_H_

#include "fd/proc.h"
#undef PID_MAX
#include <exos/debug.h>
#include <signal.h>
#include <exos/locks.h>
#include <termios.h>


#include <sys/ioctl.h>
#include <sys/cdefs.h>
#define _KERNEL
#define LIBEXOS_TTY
#include <sys/tty.h>
#undef _KERNEL
#define	NPTY	32		/* crude XXX */

#define CLALLOCSZ 1024
#define NCLISTS 2*NPTY
#define NCLISTSC ((((NCLISTS)-1)/NBBY)+1) /* chars for nclists */

#define STATIC_ALLOCATION 
/* Used by tty_subr.c */
/*
 * At compile time, choose:
 * There are two ways the TTY_QUOTE bit can be stored. If QBITS is
 * defined we allocate an array of bits -- 1/8th as much memory but
 * setbit(), clrbit(), and isset() take more cpu. If QBITS is
 * undefined, we just use an array of bytes.
 * 
 */
#define QBITS
#ifdef QBITS
#define QMEM(n)		((((n)-1)/NBBY)+1)
#else
#define QMEM(n)		(n)
#endif

struct	pt_softc {
	struct	tty *pt_tty;
	int	pt_flags;
	struct	selinfo pt_selr, pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
};

struct npty_shared_data {
  exos_lock_t masterlock;
  exos_lock_t lock[NPTY];
  char refcount[2][NPTY];
  struct pt_softc pt_softc[NPTY];
  struct tty tp[NPTY];
#define CHANSZ NPTY*2
  struct chan {
    exos_lock_t c_lock;
    void *c_chan[CHANSZ];
    char c_used[CHANSZ];
  } chan;
  u_char c_cs[NCLISTS][CLALLOCSZ];
  u_char c_cq[NCLISTS][QMEM(CLALLOCSZ)];
  u_char used[NCLISTSC];
  
};
extern struct npty_shared_data * const npty_shared_data;


/* Definitions and macros */

#define pgsignal(where,what,x) kprintf("PID %d SIG %d\n",(int)where,(int)what); \
kill((int)where, what);

#define psignal(where,what) pgsignal(where,what,0)
/* Signals that I am ignoring */
#define IGNORED_SIGS 0xffffffff	/* p->p_sigignore */
/* The mask of my signals see ttioctl */
#define MASK_SIGS 0xffffffff	/* p->p_sigmask */
/* Find out if superuser */
#define suser(x,y) 0
/* If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 */
#undef PCATCH		/* GROK -- why is this here?? see <sys/param.h> */
#define PCATCH 0

#ifdef STATIC_ALLOCATIONNO
#define MALLOC(a,cast,sz,x,y) assert(0)
#else
#define MALLOC(a,cast,sz,x,y) a = (cast) __malloc(sz);
#endif

#define FREE(a,x) __free(a);
#define spltty() 0
#define splx(x)

#ifndef min
#define min(x,y) (x < y) ? (x) : (y)
#endif

#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))


#if 0
#define CDEVSW_STOP(tp,fl) (*cdevsw[major(tp->t_dev)].d_stop)(tp,fl)
#else
#define CDEVSW_STOP(tp,fl)
#endif

#if 0
#define ISDEVMASTER(dev) (cdevsw[major(dev)].d_open == ptcopen)
#else
#define ISDEVMASTER(dev) (major(dev))
#endif

#if 0
#define ISNOTROOT(p) (p->p_ucred->cr_uid != 0)
#else 
#define ISNOTROOT(p) 1
#endif

#define ptcwakeup0(a,b) kprintf("%s:%d\n",__FILE__,__LINE__);ptcwakeup0(a,b)

#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_NOSTOP	0x40
  
#endif

#undef isctty
#undef isbackground
/* from sys/tty.h */
#define isctty(p, tp) 1
#define isbackground(p, tp) 0

#define IO_NDELAY       0x10            /* FNDELAY flag set in file table */
extern int lbolt;

