
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

/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <string.h>
#include <exos/kprintf.h>

extern char *__progname;
/*
 * This is used mainly in the kernel for different levels of debug which
 * can be set at compile time.  Add a flag -DPRINTF_LEVEL=n (n >= 0) to the
 * compiler call to get the desired level of debugging.
 * Example call to DPRINTF is
 *	DPRINTF(2, ("this is the %d error message\n", mesg_num));
 */


/* I have defined different levels of debugging so that it will be
 uniform throughout modules etc. mostly for debugging 
 user-level  OSes */

#define SYS_LEVEL 1		/* system calls */
#define CLU_LEVEL 2		/* cluster, say nfs, udp, etc */
#define CLUHELP_LEVEL 3		/* cluster helpers */
#define CLUHELP2_LEVEL 4	/* cluster helpers one level deeper */
#define SYSHELP_LEVEL 6		/* system call helpers */
#define LOCK_LEVEL 7		/* locking procedures */
#define DISABLED_LEVEL 99	/* in case you want to temporarily
				 * disabled a debugging feature*/

#ifdef PRINTF_LEVEL

#define	DPRINTF(x, y)	do { 		\
	if ((x) <= PRINTF_LEVEL)	\
	    {char *__debug_tmp = (char *) strrchr (__FILE__,'/');\
	     if (__debug_tmp == NULL) {__debug_tmp = __FILE__;}\
	     kprintf("%14s %4d: ", __debug_tmp, __LINE__);\
	     kprintf y;}			\
} while(0)

#else

#define DPRINTF(x, y)

#endif /* PRINTF_LEVEL */

#endif /* __DEBUG_H__ */
