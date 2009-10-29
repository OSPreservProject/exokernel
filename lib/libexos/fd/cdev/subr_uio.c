
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

/*      $OpenBSD: kern_subr.c,v 1.4 1997/02/24 14:19:56 niklas Exp $    */
/*      $NetBSD: kern_subr.c,v 1.15 1996/04/09 17:21:56 ragge Exp $     */

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      @(#)kern_subr.c 8.3 (Berkeley) 1/21/94
 */


#include <sys/types.h>
#include <sys/uio.h>
#include <exos/uio.h>
#include <memory.h>
#include <assert.h>

int
uiomove(cp, n, uio)
        register caddr_t cp;
        register int n;
        register struct uio *uio;
{
        register struct iovec *iov;
        u_int cnt;
        int error = 0;

#ifdef DIAGNOSTIC
        if (uio->uio_rw != UIO_READ && uio->uio_rw != UIO_WRITE)
                panic("uiomove: mode");
        if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
                panic("uiomove proc");
#endif
        while (n > 0 && uio->uio_resid) {
                iov = uio->uio_iov;
                cnt = iov->iov_len;
                if (cnt == 0) {
                        uio->uio_iov++;
                        uio->uio_iovcnt--;
                        continue;
                }
                if (cnt > n)
		  cnt = n;
		if (uio->uio_rw == UIO_READ)
		  bcopy((caddr_t)cp, iov->iov_base, cnt);
		else
		  bcopy(iov->iov_base, (caddr_t)cp, cnt);
		
                iov->iov_base += cnt;
                iov->iov_len -= cnt;
                uio->uio_resid -= cnt;
                uio->uio_offset += cnt;
                cp += cnt;
                n -= cnt;
        }
        return (error);
}

int 
ureadc(c, uio)
        register int c;
        register struct uio *uio;
{
        register struct iovec *iov;
	assert(uio->uio_resid > 0);
again:
        assert(uio->uio_iovcnt > 0);
	iov = uio->uio_iov;
        if (iov->iov_len <= 0) {
                uio->uio_iovcnt--;
                uio->uio_iov++;
                goto again;
        }

	*(char*)iov->iov_base = c;
        iov->iov_base++;
        iov->iov_len--;
        uio->uio_resid--;
        uio->uio_offset++;
        return (0);
}


int
scanc(size, cp, table, mask)
        u_int size;
        register u_char *cp, table[];
        register u_char mask;
{
        register u_char *end = &cp[size];

        while (cp < end && (table[*cp] & mask) == 0)
                cp++;
        return (end - cp);
}
