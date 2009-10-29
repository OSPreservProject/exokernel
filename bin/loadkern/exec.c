
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

/*	$OpenBSD: exec.c,v 1.18 1997/09/28 22:46:02 weingart Exp $	*/
/*	$NetBSD: exec.c,v 1.15 1996/10/13 02:29:01 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
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
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
extern off_t imagesize;

#include <sys/param.h>
#include <sys/exec.h>
#ifndef INSECURE
#include <sys/stat.h>
#endif

static char *ssym, *esym;

extern u_int opendev;

int
lkexec(path, loadaddr, howto)
	char *path;
	void *loadaddr;
	int howto;
{
	int io;
#ifndef INSECURE
	struct stat sb;
#endif
	struct exec x;
	int i;
	char *addr;
#ifdef EXEC_DEBUG
	char *daddr, *etxt;
#endif

	io = open(path, 0);
	if (io < 0)
		return -1;

	(void) fstat(io, &sb);
	if (sb.st_mode & 2)
		printf("non-secure file, check permissions!\n");

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) ||
	    N_BADMAG(x)) {
		errno = EFTYPE;
		return -1;
	}

#ifdef EXEC_DEBUG
	printf("\nstruct exec {%lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx}\n",
		x.a_midmag, x.a_text, x.a_data, x.a_bss, x.a_syms,
		x.a_entry, x.a_trsize, x.a_drsize);
#endif

        /* Text */
	printf("%d", x.a_text);
	addr = loadaddr;
	i = x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC) {
		bcopy((char *)&x, addr, sizeof x);
		addr += sizeof x;
		i -= sizeof x;
	}
	if (read(io, (char *)addr, i) != i)
		goto shread;
	addr += i;
#ifdef EXEC_DEBUG
	printf("\ntext {%x, %x, %x, %x}\n",
	    addr[0], addr[1], addr[2], addr[3]);
	etxt = addr;
#endif
	if (N_GETMAGIC(x) == NMAGIC)
		while ((long)addr & (N_PAGSIZ(x) - 1))
			*addr++ = 0;

        /* Data */
#ifdef EXEC_DEBUG
	daddr = addr;
#endif
	printf("+%d", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;

        /* Bss */
	printf("+%d", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;
	imagesize = (u_int)addr - (u_int)loadaddr;

        /* Symbols */
	if (x.a_syms) {
		ssym = addr;
		bcopy(&x.a_syms, addr, sizeof(x.a_syms));
		addr += sizeof(x.a_syms);
		printf("+[%d", x.a_syms);
		if (read(io, addr, x.a_syms) != x.a_syms)
			goto shread;
		addr += x.a_syms;

		if (read(io, &i, sizeof(int)) != sizeof(int))
			goto shread;

		bcopy(&i, addr, sizeof(int));
		if (i) {
			i -= sizeof(int);
			addr += sizeof(int);
			if (read(io, addr, i) != i)
                	goto shread;
			addr += i;
		}

		/* and that many bytes of string table */
		printf("+%d]", i);
		esym = addr;
	} else {
		ssym = 0;
		esym = 0;
	}

	close(io);

	/* and note the end address of all this	*/
	printf(" total=0x%lx", (u_long)addr);

/* XXX - Hack alert!
   This is no good, loadaddr is passed into
   machdep_start(), and it should do whatever
   is needed.

	x.a_entry += (long)loadaddr;
*/
	printf(" start=0x%x\n", x.a_entry);

#ifdef EXEC_DEBUG
        printf("loadaddr=%p etxt=%p daddr=%p ssym=%p esym=%p\n",
	    	loadaddr, etxt, daddr, ssym, esym);
        printf("\n\nReturn to boot...\n");
        getchar();
#endif

	return 0;

shread:
	close(io);
	errno = EIO;
	return -1;
}
