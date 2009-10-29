
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

/*	$OpenBSD: sysctl.h,v 1.27 1998/08/16 03:22:55 millert Exp $	*/
/*	$NetBSD: sysctl.h,v 1.16 1996/04/09 20:55:36 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
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
 *	@(#)sysctl.h	8.2 (Berkeley) 3/30/95
 */

#ifndef _EXOS_SYSCTL_H_
#define	_EXOS_SYSCTL_H_

struct proc;
struct eproc;

#ifdef	DEBUG
/*
 * CTL_DEBUG variables.
 *
 * These are declared as separate variables so that they can be
 * individually initialized at the location of their associated
 * variable. The loader prevents multiple use by issuing errors
 * if a variable is initialized in more than one place. They are
 * aggregated into an array in debug_sysctl(), so that it can
 * conveniently locate them when querried. If more debugging
 * variables are added, they must also be declared here and also
 * entered into the array.
 */
struct ctldebug {
	char	*debugname;	/* name of debugging variable */
	int	*debugvar;	/* pointer to debugging variable */
};
extern struct ctldebug debug0, debug1, debug2, debug3, debug4;
extern struct ctldebug debug5, debug6, debug7, debug8, debug9;
extern struct ctldebug debug10, debug11, debug12, debug13, debug14;
extern struct ctldebug debug15, debug16, debug17, debug18, debug19;
#endif	/* DEBUG */

/*
 * Internal sysctl function calling convention:
 *
 *	(*sysctlfn)(name, namelen, oldval, oldlenp, newval, newlen);
 *
 * The name parameter points at the next component of the name to be
 * interpreted.  The namelen parameter is the number of integers in
 * the name.
 */
typedef int (sysctlfn)
    __P((int *, u_int, void *, size_t *, void *, size_t, struct proc *));

int sysctl_int __P((void *, size_t *, void *, size_t, int *));
int sysctl_rdint __P((void *, size_t *, void *, int));
int sysctl_string __P((void *, size_t *, void *, size_t, char *, int));
int sysctl_tstring __P((void *, size_t *, void *, size_t, char *, int));
int sysctl__string __P((void *, size_t *, void *, size_t, char *, int, int));
int sysctl_rdstring __P((void *, size_t *, void *, char *));
int sysctl_rdstruct __P((void *, size_t *, void *, void *, int));
int sysctl_struct __P((void *, size_t *, void *, size_t, void *, int));
int sysctl_file __P((char *, size_t *));
int sysctl_doproc __P((int *, u_int, char *, size_t *));
struct radix_node;
struct walkarg;
int sysctl_dumpentry __P((struct radix_node *, void *));
int sysctl_iflist __P((int, struct walkarg *));
int sysctl_rtable __P((int *, u_int, void *, size_t *, void *, size_t));
int sysctl_clockrate __P((char *, size_t *));
int sysctl_rdstring __P((void *, size_t *, void *, char *));
int sysctl_rdstruct __P((void *, size_t *, void *, void *, int));
int sysctl_vnode __P((char *, size_t *, struct proc *));
int sysctl_ntptime __P((char *, size_t *));
#ifdef GPROF
int sysctl_doprof __P((int *, u_int, void *, size_t *, void *, size_t));
#endif

void fill_eproc __P((struct proc *, struct eproc *));

int kern_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		     struct proc *));
int hw_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		   struct proc *));
#ifdef DEBUG
int debug_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		      struct proc *));
#endif
int vm_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		   struct proc *));
int fs_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		   struct proc *));
int fs_posix_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
			 struct proc *));
int net_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *));
int cpu_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *));
int vfs_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *));

#endif	/* !_EXOS_SYSCTL_H_ */
