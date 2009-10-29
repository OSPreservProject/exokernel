
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
 *
 * EXOKERNEL
 *
 * This is a simplified buf structure, used as a means for communication
 * within the SCSI device driver "pa". It can be expanded to provide
 * linked lists and a buffer cache again. However, unnecessary overhead
 * (such as the vnode crap) has been killed.
 *
 */

/*	$NetBSD: buf.h,v 1.19 1995/03/29 20:57:38 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)buf.h	8.7 (Berkeley) 1/21/94
 */

#ifndef _XOK_BUF_H_
#define	_XOK_BUF_H_

#include <xok/types.h>

/*
 * The buffer header describes an I/O operation in the kernel.
 */
struct buf {
	struct	buf *b_next;	        /* Device driver queue when active. */
	struct	buf *b_sgnext;	        /* Next in scat/gath queue. */
	volatile u_int b_flags;		/* B_* flags. */
	u_int	b_dev;			/* Device associated with buffer. */
	u_quad_t b_blkno;		/* Underlying physical block number. */
	u_int	b_bcount;		/* Valid bytes in buffer. */
	u_int	b_sgtot;		/* Total bytes, if first in scat/gath
					   queue. (0 if not scatgath) */
	void	*b_memaddr;		/* Corresponding main memory
					   location. */
	int	*b_resptr;		/* If set, decremented upon
					   completion. */
	u_int	b_envid;		/* ID of initiating environment. */
	int	b_resid;		/* Remaining I/O. */
  /* Note: The total for a SCATGATH request is only held in the first struct
     and the b_resptr is only set in the last struct. */
};

/*
 * These flags are kept in b_flags.
 */
#define B_WRITE		0x00000000	/* Write buffer (partner to B_READ) */
#define	B_READ		0x00000001	/* Read buffer. */
#define	B_BUSY		0x00000002	/* I/O in progress. */
#define	B_DONE		0x00000004	/* I/O completed. */
#define B_KERNEL	0x00000008	/* synchronous, internal kernel I/O */
#define B_SCATGATH	0x00000010	/* non-final entry in scatter/gather list */
#define B_SCSICMD	0x00000020	/* direct SCSI command */
#define B_ABSOLUTE      0x00000040      /* no partition table translation request */
#define B_BC_REQ	0x00001000      /* bc (buffer cache) request */
#define	B_ERROR		0xFFFF0000	/* I/O error occurred. */

/* Error codes for the disk system -- they should fit into B_ERROR area */

#define B_NOERR		0x00000000	/* No Error */
#define B_EIO		0xFFFF0000	/* General Input/Output Error */
#define B_ENXIO		0xFFFE0000	/* Device not configured or present */
#define B_EACCES	0xFFFD0000	/* Permission denied */
#define B_EPERM		0xFFFC0000	/* Operation not permitted */
#define B_EINVAL	0xFFFB0000	/* Invalid argument */
#define B_EROFS		0xFFFA0000	/* Read-only disk */
#define B_EBUSY		0xFFF90000	/* Device busy */


/* The arguments that describe a SCSI command for the low-level driver.   */
/* In fact, the contents of this structure exactly matches the parameters */
/* for scsi_scsi_cmd (...) in scsi_base.c.                                */
/* It is used by B_SCSICMD requests to talk with the high-level driver,   */
/* which will replace "sc_link" and augment "flags" appropriately.        */

#define B_SCSICMD_MAXLEN	80	/* in bytes -- put bound for malloc */

struct scsicmd {
   struct scsi_link *sc_link;
   struct scsi_generic *scsi_cmd;
   int cmdlen;
   u_char *data_addr;
   int datalen;
   int retries;
   int timeout;
   struct buf *bp;
   int flags;
};

#endif /* !_XOK_BUF_H_ */
