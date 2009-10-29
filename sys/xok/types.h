
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

/*	$OpenBSD: types.h,v 1.12 1997/11/30 18:50:18 millert Exp $	*/
/*	$NetBSD: types.h,v 1.29 1996/11/15 22:48:25 jtc Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)types.h	8.4 (Berkeley) 1/21/94
 */

#ifndef _SYS_TYPES_H_  /* for openbsd */
#define	_SYS_TYPES_H_  /* for linux */

#ifndef _SYS_TYPES_H
#define	_SYS_TYPES_H

/* Machine type dependent parameters. */
#include <machine/types.h>

#include <machine/ansi.h>
#include <machine/endian.h>

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

typedef unsigned char	unchar;		/* Sys V compatibility */
typedef	unsigned short	ushort;		/* Sys V compatibility */
typedef	unsigned int	uint;		/* Sys V compatibility */
typedef unsigned long	ulong;		/* Sys V compatibility */
#endif

typedef	u_int64_t	u_quad_t;	/* quads */
typedef	int64_t		quad_t;
typedef	quad_t *	qaddr_t;

typedef	char *		caddr_t;	/* core address */
typedef	int32_t		daddr_t;	/* disk address */
#if 0 /* EXOKERNEL - we should convert to int32_t someday... */
typedef	int32_t		dev_t;		/* device number */
#else
typedef	short		dev_t;		/* device number */
#endif
typedef	u_int32_t	fixpt_t;	/* fixed point number */
typedef	u_int32_t	gid_t;		/* group id */
typedef	u_int32_t	ino_t;		/* inode number */
typedef	long		key_t;		/* IPC key (for Sys V IPC) */
typedef	u_int16_t	mode_t;		/* permissions */
#if 0 /* EXOKERNEL - we're trying to be POSIX */
typedef	u_int16_t	nlink_t;	/* link count */
#else
typedef	int16_t	nlink_t;		/* POSIX link count */
#endif
typedef	quad_t		off_t;		/* file offset */
typedef	int32_t		pid_t;		/* process id */
typedef quad_t		rlim_t;		/* resource limit */
typedef	int32_t		segsz_t;	/* segment size */
typedef	int32_t		swblk_t;	/* swap offset */
typedef	u_int32_t	uid_t;		/* user id */
typedef	u_int32_t	useconds_t;	/* microseconds */
typedef	int32_t		suseconds_t;	/* microseconds (signed) */

/*
 * XPG4.2 states that inclusion of <netinet/in.h> must pull these
 * in and that inclusion of <sys/socket.h> must pull in sa_family_t.
 * We put there here because there are other headers that require
 * these types and <sys/socket.h> and <netinet/in.h> will indirectly
 * include <sys/types.h>.  Thus we are compliant without too many hoops.
 */
typedef u_int32_t	in_addr_t;	/* base type for internet address */
typedef u_int16_t	in_port_t;	/* IP port type */
typedef u_int8_t	sa_family_t;	/* sockaddr address family type */

/*
 * These belong in unistd.h, but are placed here too to ensure that
 * long arguments will be promoted to off_t if the program fails to
 * include that header or explicitly cast them to off_t.
 */
#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#ifndef _KERNEL
#include <xok/cdefs.h>
__BEGIN_DECLS
off_t	 lseek __P((int, off_t, int));
int	 ftruncate __P((int, off_t));
int	 truncate __P((const char *, off_t));
__END_DECLS
#endif /* !_KERNEL */
#endif /* !defined(_POSIX_SOURCE) ... */

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
/* Major, minor numbers, dev_t's. */
#define	major(x)	((int32_t)(((u_int32_t)(x) >> 8) & 0xff))
#define	minor(x)	((int32_t)((x) & 0xff))
#define	makedev(x,y)	((dev_t)(((x) << 8) | (y)))
#endif

#ifdef	_BSD_CLOCK_T_
typedef	_BSD_CLOCK_T_	clock_t;
#undef	_BSD_CLOCK_T_
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_SSIZE_T_
typedef	_BSD_SSIZE_T_	ssize_t;
#undef	_BSD_SSIZE_T_
#endif

#ifdef	_BSD_TIME_T_
typedef	_BSD_TIME_T_	time_t;
#undef	_BSD_TIME_T_
#endif

#ifdef	_BSD_CLOCKID_T_
typedef	_BSD_CLOCKID_T_	clockid_t;
#undef	_BSD_CLOCKID_T_
#endif

#ifdef	_BSD_TIMER_T_
typedef	_BSD_TIMER_T_	timer_t;
#undef	_BSD_TIMER_T_
#endif

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#define	NBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be enough for most uses.
 */
#ifndef	FD_SETSIZE
#if 0 /* EXOKERNEL - do we really need this many more? */
#define	FD_SETSIZE	256
#else
#define	FD_SETSIZE	4096
#endif
#endif

typedef int32_t	fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */

#ifndef howmany
#define	howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#ifdef _KERNEL
#define	FD_COPY(f, t)	bcopy(f, t, sizeof(*(f)))
#define	FD_ZERO(p)	bzero(p, sizeof(*(p)))
#else
#define	FD_COPY(f, t)	memcpy(t, f, sizeof(*(f)))
#define	FD_ZERO(p)	memset(p, 0, sizeof(*(p)))
#endif

#if defined(__STDC__) && defined(_KERNEL)
/*
 * Forward structure declarations for function prototypes.  We include the
 * common structures that cross subsystem boundaries here; others are mostly
 * used in the same place that the structure is defined.
 */
struct	proc;
struct	pgrp;
struct	ucred;
struct	rusage;
struct	file;
struct	buf;
struct	tty;
struct	uio;
#endif

#endif /* !defined(_POSIX_SOURCE) ... */
#endif /* !_SYS_TYPES_H */
#endif /* !_SYS_TYPES_H_ */
