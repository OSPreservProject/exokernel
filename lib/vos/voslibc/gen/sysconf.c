/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sean Eric Fagan of Cygnus Support.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: sysconf.c,v 1.4 1998/06/02 06:10:26 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <errno.h>
#include <unistd.h>


/*
 * sysconf --
 *	get configurable system variables.
 *
 * XXX
 * POSIX 1003.1 (ISO/IEC 9945-1, 4.8.1.3) states that the variable values
 * not change during the lifetime of the calling process.  This would seem
 * to require that any change to system limits kill all running processes.
 * A workaround might be to cache the values when they are first retrieved
 * and then simply return the cached value on subsequent calls.  This is
 * less useful than returning up-to-date values, however.
 */
long
sysconf(name)
	int name;
{
	size_t len;
	int mib[2], value;

	len = sizeof(value);

	switch (name) {
/* 1003.1 */
	case _SC_ARG_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_ARGMAX;
		break;
	case _SC_CHILD_MAX:
	  printf ("how come I can't have _SC_CHILD_MAX?\n");
          assert(0);
	case _SC_CLK_TCK:
	  printf ("how come I can't have _SC_CLK_TCK?\n");
          assert(0);
	case _SC_JOB_CONTROL:
	  printf ("how come I can't have _SC_JOB_CONTROL?\n");
          assert(0);
	  return -1;
	case _SC_NGROUPS_MAX:
	  printf ("how come I can't have _SC_NGROUPS_MAX?\n");
          assert(0);
	case _SC_OPEN_MAX:
	  printf ("how come I can't have _SC_OPEN_MAX?\n");
          assert(0);
	case _SC_STREAM_MAX:
	case _SC_TZNAME_MAX:
	case _SC_SAVED_IDS:
	case _SC_VERSION:
/* 1003.2 */
	case _SC_BC_BASE_MAX:
	case _SC_BC_DIM_MAX:
	case _SC_BC_SCALE_MAX:
	case _SC_BC_STRING_MAX:
	case _SC_COLL_WEIGHTS_MAX:
	case _SC_EXPR_NEST_MAX:
	case _SC_LINE_MAX:
	case _SC_RE_DUP_MAX:
	case _SC_2_VERSION:
	case _SC_2_C_BIND:
	case _SC_2_C_DEV:
	case _SC_2_CHAR_TERM:
	case _SC_2_FORT_DEV:
	case _SC_2_FORT_RUN:
	case _SC_2_LOCALEDEF:
	case _SC_2_SW_DEV:
	case _SC_2_UPE:
	  assert(0);
	default:
	  assert(0);
	}
	return (sysctl(mib, 2, &value, &len, NULL, 0) == -1 ? -1 : value); 
}
