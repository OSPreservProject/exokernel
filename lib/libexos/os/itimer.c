
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

#include <sys/errno.h>
#include <sys/time.h>
#include <xok/sysinfo.h>
#include <xok/env.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <exos/uwk.h>
#include <exos/osdecl.h>
#include <fd/proc.h>
#include <xok/sys_ucall.h>

/* XXX - only ITIMER_REAL, and now ITIMER_VIRTUAL is supported */
/* XXX - has race conditions */

#define NUM_ITIMERS 3
static struct itimerval itvs[NUM_ITIMERS];
static unsigned long long real_alarm = 0; /* in ticks */
static unsigned long long virtual_alarm = 0; /* also in ticks */


static void set_real_timer_value(unsigned long long t) {
  itvs[ITIMER_REAL].it_value.tv_sec = ((real_alarm - t) *
				       __sysinfo.si_rate) / 1000000;
  itvs[ITIMER_REAL].it_value.tv_usec = ((real_alarm - t) *
					__sysinfo.si_rate) % 1000000;
}

static void set_virtual_timer_value(unsigned long long t) {
  itvs[ITIMER_VIRTUAL].it_value.tv_sec = ((virtual_alarm - t) *
                                       __sysinfo.si_rate) / 1000000;
  itvs[ITIMER_VIRTUAL].it_value.tv_usec = ((virtual_alarm - t) *
                                        __sysinfo.si_rate) % 1000000;
}


static void update_real_timer() {
  if (real_alarm) {
    while (1) {
      unsigned long long t = __sysinfo.si_system_ticks;
      
      if (t < real_alarm) {
	set_real_timer_value(t);
	return;
      }
      else {
	unsigned long long old_real_alarm = real_alarm;

	wk_deregister_extra(wk_mksleep_pred, real_alarm); /* XXX - check 
							     for error */
	real_alarm = 0;
	if (timerisset(&itvs[ITIMER_REAL].it_interval)) {
	  real_alarm = old_real_alarm +
	    ((itvs[ITIMER_REAL].it_interval.tv_sec *
	      (unsigned long long)1000000) +
	     (itvs[ITIMER_REAL].it_interval.tv_usec % 1000000)) /
	    __sysinfo.si_rate;
	  wk_register_extra(wk_mksleep_pred, NULL, real_alarm,
			    UWK_MKSLEEP_PRED_SIZE); /* XXX - check for error */
	  set_real_timer_value(t);
	  kill(getpid(), SIGALRM);
	}
	else {
	  timerclear(&itvs[ITIMER_REAL].it_value);
	  kill(getpid(), SIGALRM);
	  return;
	}
      }
    }
  }
}

static void update_virtual_timer() {
  if(virtual_alarm) {
    while(1) {
      unsigned int t = __curenv->env_ticks;
      
      if(t < virtual_alarm) {        /* Update virtual_alarm */
	set_virtual_timer_value(t);
	return;
      }
      else {                         /* Timer has gone off */
	
	unsigned long long old_virtual_alarm = virtual_alarm;      
	
	virtual_alarm = 0;
	
	/* Check for reset */
	if(timerisset(&itvs[ITIMER_VIRTUAL].it_interval)) { 
	  
	  virtual_alarm = old_virtual_alarm + 
	    ((itvs[ITIMER_VIRTUAL].it_interval.tv_sec *
	      (unsigned long long)1000000) +
	     (itvs[ITIMER_VIRTUAL].it_interval.tv_usec % 1000000)) /
	    __sysinfo.si_rate;

	  set_virtual_timer_value(t);               /* Reset timer */
	  kill(getpid(), SIGVTALRM);                /* Send signal */
	}
	else {
	  timerclear(&itvs[ITIMER_VIRTUAL].it_value); /* Clear timer */
	  kill(getpid(), SIGVTALRM);                  /* Send signal */
	  return;
	}
      }
    }
  }
}

int getitimer(int which, struct itimerval *value) {
  if (which < 0 || which >= NUM_ITIMERS) {
    errno = EINVAL;
    return -1;
  }

  assert(which == ITIMER_REAL || which == ITIMER_VIRTUAL); /* XXX */

  switch(which) 
    {
    case ITIMER_REAL:
      update_real_timer();
      break;
    case ITIMER_VIRTUAL:
      update_virtual_timer();
      break;
    }

  *value = itvs[which];
  return 0;
}

int setitimer(int which, const struct itimerval *value,
	      struct itimerval *ovalue) {
  struct itimerval itv;

  if (which < 0 || which >= NUM_ITIMERS) {
    errno = EINVAL;
    return -1;
  }
  if (value == NULL) {
    errno = EFAULT;
    return -1;
  }
  assert(which == ITIMER_REAL || which == ITIMER_VIRTUAL); /* XXX */

  if (ovalue) getitimer(which, &itv);

  itvs[which] = *value;

  switch(which) 
    {
    case ITIMER_REAL:
      /* if timer was already set, then unset it */
      if (real_alarm)
	wk_deregister_extra(wk_mksleep_pred, real_alarm); /* XXX - check
							     for error */
      /* register new alarm is requested */
      if (timerisset(&itvs[which].it_value)) {
	real_alarm = __sysinfo.si_system_ticks +
	  ((itvs[ITIMER_REAL].it_value.tv_sec * (unsigned long long)1000000) +
	   (itvs[ITIMER_REAL].it_value.tv_usec % 1000000)) / __sysinfo.si_rate;
	wk_register_extra(wk_mksleep_pred, NULL, real_alarm,
			  UWK_MKSLEEP_PRED_SIZE); /* XXX - check for error */
      }
      else {
	real_alarm = 0;
      }
      break;

    case ITIMER_VIRTUAL:                    /* Virtual Timer */
      /* if timer was set unset it */
      if (timerisset(&itvs[which].it_value)) {
	virtual_alarm = __curenv->env_ticks +
	  ((itvs[ITIMER_VIRTUAL].it_value.tv_sec * 
	    (unsigned long long)1000000) + 
	   (itvs[ITIMER_VIRTUAL].it_value.tv_usec % 1000000)) / 
	  __sysinfo.si_rate;
      }
      else
	virtual_alarm = 0;
      break;
    } /* switch(which) */
  
  if (ovalue) *ovalue = itv;

  return 0;
}


/* called from prologue */
void __CheckITimers () {

#if 0
  sys_cputs("checking timers\n");
#endif

  /* if the alarm is set and it should go/have gone off, then send signal */
  if (real_alarm && real_alarm <= __sysinfo.si_system_ticks) 
    update_real_timer();
  if (virtual_alarm && virtual_alarm <= __curenv->env_ticks) 
    update_virtual_timer();
}
