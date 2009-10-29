
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

/*	$NetBSD: common.c,v 1.4 1995/09/23 22:34:20 pk Exp $	*/
/*
 * Copyright (c) 1993,1995 Paul Kranenburg
 * All rights reserved.
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef dprintf
  #define dprintf slsprintf
#endif
#ifdef DYNAMIC

int rtld (int, struct crt_ldso *, struct _dynamic *);

typedef int (*rtld_entry_fn) __P((int, struct crt_ldso *));
static struct ld_entry	*ld_entry;

static void
__load_rtld(dp)
	struct _dynamic *dp;
{
	static struct crt_ldso	crt;
	crt.crt_dp = dp;
	crt.crt_ep = dyn_environ;
	/*	crt.crt_bp = (caddr_t)_callmain; */
	crt.crt_prog = __dyn_progname;

	slskprintf("About to call rtld\n");
	if (rtld(CRT_VERSION_BSD_4, &crt, dp) == -1) {
	  slskprintf("UnSuccessful call to rtld\n");
	  /* Feeble attempt to deal with out-dated ld.so */
#		define str "crt0: update /usr/libexec/ld.so\n"
	  (void)S_WRITE(2, str, sizeof(str));
#		undef str
	  slskprintf("Feeble attempt call\n");
	  if (rtld(CRT_VERSION_BSD_3, &crt,dp) == -1) {
	    _FATAL("ld.so failed\n");
	  }
	  slskprintf("Successful call to rtld\n");
	  ld_entry = dp->d_entry;
	  return;
	}
	ld_entry = crt.crt_ldentry;
	atexit(ld_entry->dlexit);

	return;
}

/*
 * DL stubs
 */

void *
dlopen(name, mode)
	char	*name;
	int	mode;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlopen)(name, mode);
}

int
dlclose(fd)
	void	*fd;
{
	if (ld_entry == NULL)
		return -1;

	return (ld_entry->dlclose)(fd);
}

void *
dlsym(fd, name)
	void	*fd;
	char	*name;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlsym)(fd, name);
}

int
dlctl(fd, cmd, arg)
void	*fd, *arg;
int	cmd;
{
	if (ld_entry == NULL)
		return -1;

	return (ld_entry->dlctl)(fd, cmd, arg);
}

char *
dlerror()
{
	int     error;

	if (ld_entry == NULL ||
	    (*ld_entry->dlctl)(NULL, DL_GETERRNO, &error) == -1)
		return "Service unavailable";

	return (char *)strerror(error);
}

/*
 * Support routines
 */

#ifdef DEBUG
static int
_strncmp(s1, s2, n)
	register char *s1, *s2;
	register int n;
{

	if (n == 0)
	  return (0);
	do {
	  if (*s1 != *s2++)
	    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
	  if (*s1++ == 0)
	    break;
	} while (--n != 0);
	return (0);
}

static char *
_getenv(name)
     register char *name;
{
  extern char **dyn_environ;
  register int len;
  register char **P, *C;
  
  for (C = name, len = 0; *C && *C != '='; ++C, ++len);
  for (P = dyn_environ; *P; ++P)
    if (!_strncmp(*P, name, len))
      if (*(C = *P + len) == '=') {
	return(++C);
      }
  return (char *)0;
}
#endif

static char *
_strrchr(p, ch)
register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}

void _FATAL (char *msg) {
  perror(msg);
  exit(0);
}


#endif /* DYNAMIC */
