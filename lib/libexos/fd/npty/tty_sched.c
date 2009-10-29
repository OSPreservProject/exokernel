
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

#error "obsoleted by os/synch.c"

#include "os/procd.h"
#include "npty.h"
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/select.h>
#include <assert.h>
#include <exos/process.h>
#include <xok/defs.h>

#include <exos/uwk.h>
#include <xok/sysinfo.h>
#include <exos/critical.h>
extern int lbolt; /* once a second variable to wait on */

#define kprintf if (0) kprintf
int __ntty_hz; /* frequency (ticks/sec) used by other tty files */

/* we can make them null, since we are still using polling model for select. */

static int 
wk_mktty_pred (struct wk_term *t, void *p, int ticks) {
  int s = 0;
#undef WK_TTY_PRED_SZ
#define WK_TTY_PRED_SZ 7

  s = wk_mkvar (s, t, wk_phys (p), 0);
  s = wk_mkimm (s, t, 0);
  s = wk_mkop (s, t, WK_EQ);
  if (ticks) {
    s = wk_mkop (s, t, WK_OR);
    s = wk_mkvar (s, t, wk_phys (&__sysinfo.si_system_ticks), 0);
    s = wk_mkimm (s, t, ticks);
    s = wk_mkop (s, t, WK_GT);
  }
  return (s);
}

void npty_sched_init(int first) {
  struct chan *c;
  int i;

  __ntty_hz = 1000000 / __sysinfo.si_rate;
  if (first) {
    c = &npty_shared_data->chan;
    DEF_SYM(lbolt,3);
    
    exos_lock_init(&c->c_lock);
    for (i = 0 ; i < CHANSZ ; i++) {
      c->c_chan[i] = (void *)0;
      c->c_used[i] = 0;
    }
  }
}

/* HBXX - openbsd comment:
 * General sleep call.  Suspends the current process until a wakeup is
 * performed on the specified identifier.  The process will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds
 * (0 means no timeout).  If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 */
/* time is in ticks */
int tsleep (void *chan, int pri, char *wmesg, int ticks) {
  struct chan *c;
  int i,spot;

  struct wk_term t[WK_TTY_PRED_SZ];
  int sz;

  assert(0);			/* use synch.c */
  c = &npty_shared_data->chan;
  kprintf("tsleep: chan %p ticks: %d\n",chan,ticks);

retry:
  exos_lock_get_nb(&c->c_lock);
  for (i = 0 ; i < CHANSZ ; i++) {
    if (c->c_used[i] == 0) {
      c->c_used[i] = 1;
      spot = i;
      goto done_chan;
    }
  }
  exos_lock_release(&c->c_lock);
  demand(0, I dont expect to run out of wait slots);
  yield(-1);
  goto retry;
done_chan:
  exos_lock_release(&c->c_lock);
  c->c_chan[spot] = chan;

  if (ticks) { ticks += __sysinfo.si_system_ticks; }
    /* sleep until *p goes to 0 or ticks */
  sz = wk_mktty_pred (t,&c->c_chan[spot],ticks);
  kprintf("predicate sz: %d\n",sz);

  wk_waitfor_pred (t, sz);

  assert(c->c_used[spot] == 1);

  if (ticks && __sysinfo.si_system_ticks > ticks) {
    c->c_used[spot] = 0;
    return EWOULDBLOCK;
  }

  assert(c->c_chan[spot] == (void *)0); 
  c->c_used[spot] = 0;

  return 0;
}
void wakeup (void *chan) {
  struct chan *c;
  int i;

  assert(0);			/* use synch.c */

  kprintf("wakeup: %p  ",chan);
  c = &npty_shared_data->chan;
  
  EnterCritical();
  for (i = 0; i < CHANSZ; i++) {
    if (c->c_used[i] && c->c_chan[i] == chan) {
      c->c_chan[i] = (void *)0;
    }
  }
  ExitCritical();
}
