
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
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 */

#include <assert.h>
#include <errno.h>
#include <exos/callcount.h>
#include <exos/kprintf.h>
#include <exos/net/if.h>
#include <exos/sysctl.h>
#include <machine/vmparam.h>
#include <string.h>
#include <sys/exec.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/syslimits.h>
#include <vm/vm_param.h>
#include <xok/sysinfo.h>

/* NOTE: SOME CODE FROM OpenBSD */
/*
 * Validate parameters and get old parameters
 * for a structure oriented sysctl function.
 */
int
sysctl_rdstruct(oldp, oldlenp, newp, sp, len)
	void *oldp;
	size_t *oldlenp;
	void *newp, *sp;
	int len;
{
	int error = 0;

	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = len;
	if (oldp)
	        memcpy(oldp, sp, len);
	return (error);
}

/*
 * As above, but read-only.
 */
int
sysctl_rdint(oldp, oldlenp, newp, val)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	int val;
{
	int error = 0;

	if (oldp && *oldlenp < sizeof(int))
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = sizeof(int);
	if (oldp)
	        memcpy(oldp, &val, sizeof(int));
	return (error);
}

/*
 * Validate parameters and get old / set new parameters
 * for a string-valued sysctl function.
 */
int
sysctl_string(oldp, oldlenp, newp, newlen, str, maxlen)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	char *str;
	int maxlen;
{
	int len, error = 0;

	len = strlen(str) + 1;
	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp && newlen >= maxlen)
		return (EINVAL);
	if (oldp) {
		*oldlenp = len;
		memcpy(oldp, str, len);
	}
	if (newp) {
		memcpy(str, newp, newlen);
		str[newlen] = 0;
	}
	return (error);
}
/*
 * As above, but read-only.
 */
int
sysctl_rdstring(oldp, oldlenp, newp, str)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	char *str;
{
	int len, error = 0;

	len = strlen(str) + 1;
	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = len;
	if (oldp) memcpy(oldp, str, len);
	return (error);
}

/* XXX - init these somewhere else */
static char ostype[] = "ExOS_BSD_Xok";
static char osrelease[] = "Internal";
static char *osversion = __sysinfo.si_osid;
static int sysctl_kern(int *name, u_int namelen, void *oldp, size_t *oldlenp,
		       void *newp, size_t newlen) {
  switch (name[1]) {
  case KERN_OSTYPE:
    return (sysctl_rdstring(oldp, oldlenp, newp, ostype));
  case KERN_OSRELEASE:
    return (sysctl_rdstring(oldp, oldlenp, newp, osrelease));
  /* XXX The following both can't be the same thing! */
  case KERN_OSVERSION:
    return (sysctl_rdstring(oldp, oldlenp, newp, osversion));
  case KERN_VERSION:
    return (sysctl_rdstring(oldp, oldlenp, newp, osversion));
  case KERN_HOSTNAME:
    if (oldp && oldlenp) 
      if (if_gethostname(oldp,*oldlenp) != 0) return EINVAL;
    if (newp) 
      if (if_sethostname(newp,newlen) != 0) return EINVAL;
    return 0;
  case KERN_DOMAINNAME:
    if (oldp && oldlenp) 
      if (if_getdomainname(oldp,*oldlenp) != 0) return EINVAL;
    if (newp) 
      if (if_setdomainname(newp,newlen) != 0) return EINVAL;
    return 0;
  case KERN_CLOCKRATE:
    return (sysctl_clockrate(oldp, oldlenp));
  case KERN_ARGMAX:
    return (sysctl_rdint(oldp, oldlenp, newp, ARG_MAX));
  default:
    return EOPNOTSUPP;
  }
}

/* XXX - init this somewhere else */
static char machine[] = "i386";
static int sysctl_hw(int *name, u_int namelen, void *oldp, size_t *oldlenp,
		     void *newp, size_t newlen) {
  switch (name[1]) {
  case HW_MACHINE:
    return(sysctl_rdstring(oldp, oldlenp, newp, machine));
  case HW_PAGESIZE:
    return(sysctl_rdint(oldp, oldlenp, newp, NBPG));
  default:
    return EOPNOTSUPP;
  }
}

/* XXX - implement load avgs for real */
struct loadavg averunnable = {{FSCALE, FSCALE, FSCALE}, FSCALE};
static int sysctl_vm(int *name, u_int namelen, void *oldp, size_t *oldlenp,
		     void *newp, size_t newlen) {
  struct _ps_strings _ps = { PS_STRINGS };

  switch (name[1]) {
  case VM_LOADAVG:
    averunnable.fscale = FSCALE;
    return (sysctl_rdstruct(oldp, oldlenp, newp, &averunnable,
			    sizeof(averunnable)));
  case VM_PSSTRINGS:
    return (sysctl_rdstruct(oldp, oldlenp, newp, &_ps, sizeof _ps));
  default:
    return EOPNOTSUPP;
  }
}

int __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
	     size_t newlen) {
  int error;

  OSCALLENTER(OSCALL___sysctl);
  if (namelen < 2 || namelen > CTL_MAXNAME) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL___sysctl);
    return -1;
  }

  switch (name[0]) {
  case CTL_KERN:
    error = sysctl_kern(name, namelen, oldp, oldlenp, newp, newlen);
    break;
  case CTL_HW:
    error = sysctl_hw(name, namelen, oldp, oldlenp, newp, newlen);    
    break;
  case CTL_VM:
    error = sysctl_vm(name, namelen, oldp, oldlenp, newp, newlen);    
    break;
  default:
    error = EOPNOTSUPP;
    break;
  }

  if (error == EOPNOTSUPP) {
    kprintf("\nUnsupported call to sysctl: %d:%d\n\n", name[0], name[1]);
    assert(0);
  }

  if (error) {
    errno = error;
    error = -1;
  }
  OSCALLEXIT(OSCALL___sysctl);
  return (error);
}
