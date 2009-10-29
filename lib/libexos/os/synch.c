
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

#include <assert.h>
#include <string.h>
#include <xok/sysinfo.h>
#include <exos/synch.h>
#include <exos/uwk.h>
#include <exos/critical.h>
#include "fd/proc.h"		/* for fd_shm_alloc */
#include <exos/vm-layout.h>
#include <exos/signal.h>
#include <exos/cap.h>


#include <unistd.h>

/* d0printf for tsleep and wakeup */
#define d0printf if (0) kprintf
/* d1printf for sel{record/wakeup} */
#define d1printf if (0) kprintf
/* d2printf for tsleep_pred */
#define d2printf if (0) kprintf

/* lbolt - OpenBSD once a second variable to wait on (it does a wakeup on this variable
 * once a second.  If we tsleep on this variable, we'll simply sleep 0.5 seconds, this
 * does not affect the external behavior (since it is not used for synchronization */
int lbolt; 

int hz; /* frequency (ticks/sec) used by other tty files */

static struct synch_table {
  void *wchan[NENV];		
  /* should in theory be MAXPROC, but MAXPROC = NENV.  At most one process is 
     sleeping on a variable */
} *synch_table = (struct synch_table *) 0;

int synch_init(void) {
  int status;
  int i;
  hz = 1000000 / __sysinfo.si_rate;

  status = fd_shm_alloc(SYNCH_SHM_OFFSET,
			sizeof(struct synch_table),
			(char *)SYNCH_SHARED_REGION);
  StaticAssert((sizeof(struct synch_table)) <= SYNCH_SHARED_REGION_SZ);
  if (status == -1) demand(0, problems attaching shm);

  synch_table = (struct synch_table *)SYNCH_SHARED_REGION;
  if (status) {
    /* first process */
    for (i = 0; i < NENV; i++) 
      synch_table->wchan[i] = NULL;
  }
  return 0;
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

/* could implement tsleep in terms of following tsleep_pred */
int tsleep (void *chan, int pri, char *wmesg, int ticks) {
#define PREDSZ 32
  struct wk_term t[PREDSZ];
  int sz = 0;
  int sig = 0;
  int catch = pri & PCATCH;
  void **wchan;
  int ret, i, CriticalLevels;
  unsigned long long dest_ticks = ticks;
retry:
  sz = 0;
  d0printf("%d tsleep chan: %p, catch: %d, ticks: %d (lbolt? %d)\n",
	  getpid(),chan, catch ?1:0, ticks, chan == &lbolt);

  global_ftable->counter0++;
  if (chan == &lbolt)   global_ftable->counter2++;

  wchan = &synch_table->wchan[envidx(__envid)];
  *wchan = chan;

  /* for timeout */
  if (dest_ticks) {
    dest_ticks += __sysinfo.si_system_ticks; 
    sz += wk_mksleep_pred(t, dest_ticks); /* t is fine since it is first */
    sz = wk_mkop(sz, t, WK_OR);
  }
  /* for channel */
  if (chan != &lbolt) {
    sz = wk_mkvar (sz, t, wk_phys (wchan), 0);
    sz = wk_mkimm (sz, t, 0);
    sz = wk_mkop (sz, t, WK_EQ);
  } else {
    /* sleep for 0.5 seconds */
    sz += wk_mksleep_pred(&t[sz],(500000/__sysinfo.si_rate) +
			  __sysinfo.si_system_ticks);
  }
  /* for signals */
  if (catch) {
    //kprintf("%d tsleep interruptible (sigready? %d)\n",getpid(),__issigready());

    if ((sig = __issigready())) goto resume;
    sz = wk_mkop(sz, t, WK_OR);
    sz += wk_mksig_pred(&t[sz]);
  }

  assert(sz <= PREDSZ);
  /* make sure when sleeping on lbolt => no ticks */
  assert(!(chan == &lbolt && dest_ticks != 0)); 

  /* Leave and then Enter Critical Region */
  i = CriticalLevels = CriticalNesting();  assert(i >= 0);
  while (i-- > 0) ExitCritical(); assert(CriticalNesting() == 0);
  
  wk_waitfor_pred (t, sz);

  i = 0; while (i++ < CriticalLevels) EnterCritical();
  assert(CriticalLevels == CriticalNesting());

resume:
  ret = 0;
  if (catch && (sig != 0 || (sig = __issigready()))) {
    /* there is a signal ready */
    //kprintf("%d tsleep interrupted by a signal: %d (%s)\n",getpid(),sig,sys_signame[sig]);
    if (__issigstopping(sig)) {
      __signal_stop_process(sig);
      goto retry;
    }
    if (__issigrestart(sig)) ret = ERESTART;
    else ret = EINTR;
    goto done;
  }
  if (chan != &lbolt && dest_ticks && __sysinfo.si_system_ticks > dest_ticks) {
    ret = EWOULDBLOCK;
  }
done:
  d0printf("%d tsleep ret: %d chan: %p, catch: %d, dest_ticks: %qu (lbolt? %d) -> %d\n",
	  getpid(),ret,chan, catch ?1:0, dest_ticks, chan == &lbolt, ret);
  /* clear waiting channel */
  *wchan = 0;
  return ret;
}

void wakeup (void *chan) {
  int i;

  d0printf("%d wakeup chan: %p\n", getpid(),chan);

  global_ftable->counter1++;
  EnterCritical();
  for (i = 0; i < NENV; i++) 
    if (synch_table->wchan[i] == chan) 
      synch_table->wchan[i] = (void *)0;
  ExitCritical();
}


#define DID_TIMEOUT 1
#define DID_SIGNAL 2
#define DID_PRED 3
int tsleep_pred_directed (struct wk_term *u, int sz0, int pri, char *wmesg, int ticks,
			  int envid) {
#define PREDSZ 32
  struct wk_term t[PREDSZ];
  int sig;
  int catch;
  int sz;
  int ret, i, CriticalLevels;
  unsigned long long dest_ticks = ticks;
retry:
  sig = 0;
  sz = sz0;
  catch = pri & PCATCH;

  assert(sz < PREDSZ);
  assert(sz);
  d2printf("%d tsleep_pred pred: %p sz: %d, catch: %d, ticks: %d\n",
	  getpid(),u, sz, catch ?1:0, ticks);

  i = wk_mktag(0, t, DID_PRED);
  memcpy(t + i, u, (sz * sizeof(struct wk_term)));
  /* for timeout */
  sz += i;
  if (dest_ticks) { 
    sz = wk_mkop(sz, t, WK_OR);
    sz = wk_mktag(sz, t, DID_TIMEOUT);
    dest_ticks += __sysinfo.si_system_ticks; 
    sz += wk_mksleep_pred(&t[sz], dest_ticks); 
  }
  /* for signals */
  if (catch) {
    d2printf("%d tsleep_pred interruptible (sigready? %d)\n",getpid(),__issigready());
    
    if ((sig = __issigready())) {
      d2printf("signal %d ready before going to sleep\n",sig);
      goto resume;
    }
    sz = wk_mkop(sz, t, WK_OR);
    sz = wk_mktag(sz, t, DID_SIGNAL);
    sz += wk_mksig_pred(&t[sz]);
  }

  assert(sz <= PREDSZ);

  /* Leave and then Enter Critical Region */
  i = CriticalLevels = CriticalNesting();  assert(i >= 0);
  while (i-- > 0) ExitCritical(); assert(CriticalNesting() == 0);
  wk_waitfor_pred_directed (t, sz, envid);
  d2printf("(%d) u_pred_tag: %d issigready: %d\n",getpid(),UAREA.u_pred_tag,__issigready());
  i = 0; while (i++ < CriticalLevels) EnterCritical();
  assert(CriticalLevels == CriticalNesting());

  ret = 0;
  switch(UAREA.u_pred_tag) {
  case DID_SIGNAL:
    if (catch && (sig != 0 || (sig = __issigready()))) {
resume:
      /* there is a signal ready */
      d2printf("%d tsleep interrupted by a signal: %d (%s)\n",getpid(),sig,
	       sys_signame[sig]);
      if (__issigstopping(sig)) {
	d2printf("%d stopping process\n",getpid());
	__signal_stop_process(sig);
	goto retry;
      }
      if (__issigrestart(sig)) ret = ERESTART;
      else ret = EINTR;
    }
    break;
  case DID_TIMEOUT:
    ret = EWOULDBLOCK;
    break;
  case DID_PRED:
    ret = 0;
    break;
  default:
    kprintf("bad pred tag: %d\n",UAREA.u_pred_tag);
    assert(0);
  }
  d2printf("tsleep_pred: %s, returns: %d\n",wmesg,ret);
  return ret;
}




/* OpenBSD's select primitives: selrecord and selwakeup.  We change the syntax 
 * a little, and add selselect_pred to tie in with our predicate scheduling 
 */

/* see comment on exos/synch.h about caveats 
 * selrecord - maintains a predicate of all counters of selinfo structure
 * selwakup - increase the counters therefore firing the predicates maintained
 *            by selrecord
 * selselect_pred - returns the predicate maintained by selrecord.  any subsequent 
 *            calls of selselect_pred will return 0
 */

#define WK_SEL_PRED_SZ 64
static struct wk_term rpred[WK_SEL_PRED_SZ];
static struct wk_term wpred[WK_SEL_PRED_SZ];
static struct wk_term xpred[WK_SEL_PRED_SZ];
static int rsz = 0;
static int wsz = 0;
static int xsz = 0;

static inline void _selrecord_pred(int *sz, struct wk_term *t, struct selinfo *sel) {
    if (*sz) *sz = wk_mkop(*sz, t, WK_OR); /* not first prepend OR */
    EnterCritical();
    *sz = wk_mkvar(*sz, t, wk_phys(&sel->counter), CAP_ROOT);
    *sz = wk_mkimm(*sz, t, sel->counter);
    ExitCritical();
    *sz = wk_mkop(*sz, t, WK_NEQ);
    if (*sz >= WK_SEL_PRED_SZ) {kprintf("pred size: %d\n",*sz);assert(0);}
}
/* only keep track of one level */
void    
selrecord (int rw, struct selinfo *sel) {
  d1printf("%d selrecord rw %d &selinfo %p counter: %d r:%d x:%d w:%d\n",getpid(),
	  rw,sel,sel->counter,rsz, xsz, wsz);
  switch(rw) {
  case FREAD:     rsz = 0;_selrecord_pred(&rsz, rpred, sel); break;
  case 0:         xsz = 0;_selrecord_pred(&xsz, xpred, sel); break;
  case FWRITE:    wsz = 0;_selrecord_pred(&wsz, wpred, sel); break;
  default:        assert(0);
  }
}
void    
selwakeup (struct selinfo *sel) {
  d1printf("%d selwakeup &selinfo %p counter: %d\n",getpid(),sel,sel->counter);
  sel->counter++;
}
static inline int _selselect_pred(int *sz, struct wk_term *pred, struct wk_term *t) {
  int osz;
  EnterCritical();
  osz = *sz;  
  bcopy(pred, t, osz * sizeof (struct wk_term));  
  *sz = 0;
  ExitCritical();
  return osz;
}
int
selselect_pred(int rw, struct wk_term *pred) {
  d1printf("%d selselect_pred rw %d r:%d x:%d w:%d\n",getpid(),
	  rw,rsz, xsz, wsz);
  switch(rw) {
  case FREAD:     return _selselect_pred(&rsz, rpred, pred); 
  case 0:         return _selselect_pred(&xsz, xpred, pred); 
  case FWRITE:    return _selselect_pred(&wsz, wpred, pred); 
  default:        assert(0);
  }
}

