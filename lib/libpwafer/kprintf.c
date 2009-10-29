
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


/*-
 * Copyright (c) 1986, 1988, 1991, 1993
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
 *	@(#)subr_prf.c	8.3 (Berkeley) 1/21/94
 */

#include <xok/defs.h>
#include <stdarg.h>
#include <xok/sys_ucall.h>
#include <sys/types.h>

static char *ksprintn (u_long, int, int *);
int kvsprintf (char *, const char *, va_list);

int
kprintf (const char *fmt, ...)
{
  va_list ap;
  int len;
  // static char pbuf[256] = ""; /* XXX do we need this much */
  char pbuf[256] = ""; 

  va_start (ap, fmt);
  len = kvsprintf (pbuf, fmt, ap);
  sys_cputs (pbuf);
  va_end (ap);
  return len;
}

int
ksprintf (char *buf, const char *fmt, ...)
{
  va_list ap;
  int len;

  va_start (ap, fmt);
  len = kvsprintf (buf, fmt, ap);
  va_end (ap);
  return len;
}

/*
 * Scaled down version of vsprintf(3).
 */
int
kvsprintf (char *buf, const char *cfmt, va_list ap)
{
  register const char *fmt = cfmt;
  register char *p, *bp;
  register int ch, base;
  u_long ul;
  int lflag, tmp, width;
  char padc;

  for (bp = buf;;) {
    padc = ' ';
    width = 0;
    while ((ch = *(u_char *) fmt++) != '%')
      if ((*bp++ = ch) == '\0')
	return ((bp - buf) - 1);

    lflag = 0;
  reswitch:
    switch (ch = *(u_char *) fmt++) {
    case '0':
      padc = '0';
      goto reswitch;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      for (width = 0;; ++fmt) {
	width = width * 10 + ch - '0';
	ch = *fmt;
	if (ch < '0' || ch > '9')
	  break;
      }
      goto reswitch;
    case 'l':
      lflag = 1;
      goto reswitch;
      /* case 'b': ... break; XXX */
    case 'c':
      *bp++ = va_arg (ap, int);
      break;
      /* case 'r': ... break; XXX */
    case 's':
      p = va_arg (ap, char *);
      while (*bp++ = *p++)
	continue;
      --bp;
      break;
    case 'd':
      ul = lflag ? va_arg (ap, long) : va_arg (ap, int);
      if ((long) ul < 0) {
	*bp++ = '-';
	ul = -(long) ul;
      }
      base = 10;
      goto number;
      break;
    case 'o':
      ul = lflag ? va_arg (ap, u_long) : va_arg (ap, u_int);
      base = 8;
      goto number;
      break;
    case 'u':
      ul = lflag ? va_arg (ap, u_long) : va_arg (ap, u_int);
      base = 10;
      goto number;
      break;
    case 'p':
      *bp++ = '0';
      *bp++ = 'x';
      ul = (u_long) va_arg (ap, void *);
      base = 16;
      goto number;
    case 'x':
      ul = lflag ? va_arg (ap, u_long) : va_arg (ap, u_int);
      base = 16;
    number:
      p = ksprintn (ul, base, &tmp);
      if (width && (width -= tmp) > 0)
	while (width--)
	  *bp++ = padc;
      while (ch = *p--)
	*bp++ = ch;
      break;
    default:
      *bp++ = '%';
      if (lflag)
	*bp++ = 'l';
      /* FALLTHROUGH */
    case '%':
      *bp++ = ch;
    }
  }
}

/*
 * Put a number (base <= 16) in a buffer in reverse order; return an
 * optional length and a pointer to the NULL terminated (preceded?)
 * buffer.
 */
static char *
ksprintn (u_long ul, int base, int *lenp)
{				/* A long in base 8, plus NULL. */
  // static char buf[sizeof (long) * 8 / 3 + 2] = "";
  char buf[sizeof (long) * 8 / 3 + 2] = "";
  register char *p;

  p = buf;
  do {
    *++p = "0123456789abcdef"[ul % base];
  } while (ul /= base);
  if (lenp)
    *lenp = p - buf;
  return (p);
}
