
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

#include <exos/callcount.h>
#include <sys/time.h>
#include <xok/sysinfo.h>
#include <exos/cap.h>

#include <xok/sys_ucall.h>

#include <errno.h>
#include <assert.h>

#include <xok/pctr.h>

/* if you comment this out, make sure you also comment out in time.c */
#define USE_PCTR

#define printf(format, args...)	/* disabled printfs */


/* timezone in kernel not supported, and not needed there either */
int 
gettimeofday(struct timeval *tp, struct timezone *tzp) {

  OSCALLENTER(OSCALL_gettimeofday);
  if (tp) {
#ifdef USE_PCTR
    long usecs;
    long secs;
    long long u = rdtsc() / (quad_t)__sysinfo.si_mhz;

    secs = u / 1000000;
    usecs = u % 1000000;
    tp->tv_sec = __sysinfo.si_startup_time + secs;
    tp->tv_usec = usecs;
#else
    long usecs = (__sysinfo.si_system_ticks * __sysinfo.si_rate);
    tp->tv_sec = __sysinfo.si_startup_time + (usecs / 1000000);
    tp->tv_usec = usecs % 1000000;
#endif
    printf("gettimeof day tv_sec: %ld tv_usec: %ld secs: %ld usecs %ld\n",
	   tp->tv_sec, tp->tv_usec, secs, usecs);
  }
  if (tzp) bzero(tzp, sizeof(*tzp)); /* timezone not kept in libos */

  OSCALLEXIT(OSCALL_gettimeofday);
  return 0;
}

/* timezone in kernel not supported, and not needed there either */
int
settimeofday(const struct timeval *tp, const struct timezone *tzp) {
  long t;

  OSCALLENTER(OSCALL_settimeofday);
  if (tp) {
    long secs;
    long long u;
#ifdef USE_PCTR
    u = rdtsc() / (quad_t)__sysinfo.si_mhz;
#else
    u = __sysinfo.si_system_ticks * __sysinfo.si_rate;
#endif
    secs = u / 1000000;
    t = tp->tv_sec; 
    if (t > secs) {
      printf("settimeofday tv_sec: %ld tv_usec: %ld secs: %ld usecs %ld\n",
	     tp->tv_sec, tp->tv_usec, secs, usecs);
      t = t - secs;
    } else {
      demand(0, negative time not possible);
    }
  } else {
    t = __sysinfo.si_startup_time;
  }

  printf("setting startuptime: %ld\n", t);

  if (sys_startup_time(CAP_ROOT, t) == 0) {
    printf("values: %ld\n", 
	   __sysinfo.si_startup_time);
    OSCALLEXIT(OSCALL_settimeofday);
    return 0;
  } else {
    errno = EPERM;
    OSCALLEXIT(OSCALL_settimeofday);
    return -1;
  }
}

