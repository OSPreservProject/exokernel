
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

/*
  Signal code

  Implements the signal oriented system calls:
  sigaction, sigreturn, sigprocmask, sigaltstack, sigsuspend, sigpending, and
  kill

  Uses the ipc mechanism provided by the kernel.

  Provides a default termination handler which prints info to stdout and
  the console.

  LibExOS signals are not complete in the following ways:
  - What's the proper implementation for SIGKILL and SIGSTOP?
    (current implementation is pretty close.  SIGSTOP/CONT sometimes
    does not work properly after a vfork)
  - no restartable system calls
    (it is implemented for read, write, wait)
    to implement this use tsleep or tsleep_pred and check return code.
    (look at wait4 in procd.c for example)
  - doesn't check capabilities/permissions
  - not sure whether the required functions/syscalls are reentrant
    (implemented for some calls, wait and proc calls is one of them,
    basically use signals_{on,off} as not to deliver signals at bad times.)
  - does not core dump - needs get/setrlimit to be implemented so it can
    determine the maximum size of the core file
  - siginfo and sigcontext structures are not properly filled in
  - dont seem to be able to call exit from a signal handler in a restartable
    signal on a read.
 */

#include <exos/osdecl.h>
#include <exos/ipcdemux.h>
#include <exos/uwk.h>
#include <exos/conf.h>
#include <exos/process.h>
#include <exos/cap.h>
#include <exos/ipc.h>
#include <exos/callcount.h>
#include <exos/critical.h>
#include <exos/signal.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include "procd.h"

/* for debugging */
/* dprintf: for kill, killpg and when executing a handler */
#define dprintf  if (0) kprintf
/* d1printf for signals_{on,off} */
#define d1printf if (0) kprintf
/* d2printf for sigsuspend, and sigprocmask */
#define d2printf if (0) kprintf
/* d3printf for printing time to DeliverPendingSignals */
#define d3printf if (0) kprintf

#undef signals_on
#undef signals_off

/* IPC level interface to signals in exos */
static int receive_signal(int code, int signal, int, int, u_int caller);
static int send_signal(u_int envid, int signal);

/* Built-in actions */
static void signal_terminate (int, siginfo_t *, struct sigcontext *);
static void signal_stop (int, siginfo_t *, struct sigcontext *);

/* NOTE: ALWAYS USE THE MASK AND FLAGS OF SIGNAL_VEC. */

/* table listing the default handlers for each signal */
#define DEFAULT_HANDLER {{(void*)signal_terminate}, 0xffff, SA_SIGINFO}
#define STOP_HANDLER {{(void*)signal_stop}, 0xffff, SA_SIGINFO}
static struct sigaction default_signal_vec[] = {
  [SIGHUP] = DEFAULT_HANDLER,
  [SIGINT] = DEFAULT_HANDLER,
  [SIGQUIT] = DEFAULT_HANDLER,
  [SIGILL] = DEFAULT_HANDLER,
  [SIGTRAP] = STOP_HANDLER,   /* XXX - uses STOP_HANDLER for ptrace */
  [SIGABRT] = DEFAULT_HANDLER,
  [SIGEMT] = DEFAULT_HANDLER,
  [SIGFPE] = DEFAULT_HANDLER,
  [SIGKILL] = DEFAULT_HANDLER,
  [SIGBUS] = DEFAULT_HANDLER,
  [SIGSEGV] = DEFAULT_HANDLER,
  [SIGSYS] = DEFAULT_HANDLER,
  [SIGPIPE] = DEFAULT_HANDLER,
  [SIGALRM] = DEFAULT_HANDLER,
  [SIGTERM] = DEFAULT_HANDLER,
  [SIGURG] = {{SIG_IGN}, 0, 0},
  [SIGSTOP] = STOP_HANDLER,
  [SIGTSTP] = STOP_HANDLER,
  [SIGCONT] = {{SIG_IGN}, 0, 0},
  [SIGCHLD] = {{SIG_IGN}, 0, 0},
  [SIGTTIN] = STOP_HANDLER,
  [SIGTTOU] = STOP_HANDLER,
  [SIGIO] = {{SIG_IGN}, 0, 0},
  [SIGXCPU] = DEFAULT_HANDLER,
  [SIGXFSZ] = DEFAULT_HANDLER,
  [SIGVTALRM] = DEFAULT_HANDLER,
  [SIGPROF] = DEFAULT_HANDLER,
  [SIGWINCH] = {{SIG_IGN}, 0, 0},
  [SIGINFO] = {{SIG_IGN}, 0, 0},
  [SIGUSR1] = DEFAULT_HANDLER,
  [SIGUSR2] = DEFAULT_HANDLER
};

#define signal_bits(x) (1 << ((x) - 1))

/* our table of current signal handlers for each signal */
#define special_mask(signo) (0xffffffff & \
			     ~(signal_bits(SIGKILL)| \
			       signal_bits(SIGSTOP)| \
			       signal_bits(signo))) 
static struct sigaction signal_vec[] = {
  [SIGHUP]    = {{SIG_DFL}, 0, SA_RESTART},
  [SIGINT]    = {{SIG_DFL}, 0, SA_RESTART},
  [SIGQUIT]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGILL]    = {{SIG_DFL}, special_mask(SIGILL), 0},
  [SIGTRAP]   = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGABRT]   = {{SIG_DFL}, special_mask(SIGABRT), 0},
  [SIGEMT]    = {{SIG_DFL}, special_mask(SIGEMT), 0},
  [SIGFPE]    = {{SIG_DFL}, special_mask(SIGFPE), 0},
  [SIGKILL]   = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGBUS]    = {{SIG_DFL}, special_mask(SIGBUS), 0},
  [SIGSEGV]   = {{SIG_DFL}, special_mask(SIGSEGV), 0},
  [SIGSYS]    = {{SIG_DFL}, special_mask(SIGSYS), 0},
  [SIGPIPE]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGALRM]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGTERM]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGURG]    = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGSTOP]   = {{SIG_DFL}, 0, SA_RESTART}, /* HBXX it had SA_NODEFER */
  [SIGTSTP]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGCONT]   = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGCHLD]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGTTIN]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGTTOU]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGIO]     = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGXCPU]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGXFSZ]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGVTALRM] = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGPROF]   = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGWINCH]  = {{SIG_DFL}, 0, SA_RESTART},
  [SIGINFO]   = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART},
  [SIGUSR1]   = {{SIG_DFL}, 0, SA_RESTART},
  [SIGUSR2]   = {{SIG_DFL}, 0, SA_NODEFER|SA_RESTART}

};

/* we repesent queued signals using a bitmask. This table is indexed by
   signal number and gives the bitmask representation in return. */
static const unsigned int signal_bitmasks[] = {
  [SIGHUP] = signal_bits(SIGHUP),
  [SIGINT] = signal_bits(SIGINT),
  [SIGQUIT] = signal_bits(SIGQUIT),
  [SIGILL] = signal_bits(SIGILL),
  [SIGTRAP] = signal_bits(SIGTRAP),
  [SIGABRT] = signal_bits(SIGABRT),
  [SIGEMT] = signal_bits(SIGEMT),
  [SIGFPE] = signal_bits(SIGFPE),
  [SIGKILL] = signal_bits(SIGKILL),
  [SIGBUS] = signal_bits(SIGBUS),
  [SIGSEGV] = signal_bits(SIGSEGV),
  [SIGSYS] = signal_bits(SIGSYS),
  [SIGPIPE] = signal_bits(SIGPIPE),
  [SIGALRM] = signal_bits(SIGALRM),
  [SIGTERM] = signal_bits(SIGTERM),
  [SIGURG] = signal_bits(SIGURG),
  [SIGSTOP] = signal_bits(SIGSTOP),
  [SIGTSTP] = signal_bits(SIGTSTP),
  [SIGCONT] = signal_bits(SIGCONT),
  [SIGCHLD] = signal_bits(SIGCHLD),
  [SIGTTIN] = signal_bits(SIGTTIN),
  [SIGTTOU] = signal_bits(SIGTTOU),
  [SIGIO] = signal_bits(SIGIO),
  [SIGXCPU] = signal_bits(SIGXCPU),
  [SIGXFSZ] = signal_bits(SIGXFSZ),
  [SIGVTALRM] = signal_bits(SIGVTALRM),
  [SIGPROF] = signal_bits(SIGPROF),
  [SIGWINCH] = signal_bits(SIGWINCH),
  [SIGINFO] = signal_bits(SIGINFO),
  [SIGUSR1] = signal_bits(SIGUSR1),
  [SIGUSR2] = signal_bits(SIGUSR2)
};

static const u_int delivery_order[] = {
  SIGKILL,
  SIGSTOP,
  SIGSEGV,
  SIGBUS,
  SIGILL,
  SIGFPE,
  SIGHUP,
  SIGINT,
  SIGQUIT,
  SIGTRAP,
  SIGABRT,
  SIGEMT,
  SIGSYS,
  SIGPIPE,
  SIGALRM,
  SIGTERM,
  SIGURG,
  SIGTSTP,
  SIGCONT,
  SIGCHLD,
  SIGTTIN,
  SIGTTOU,
  SIGIO,
  SIGXCPU,
  SIGXFSZ,
  SIGVTALRM,
  SIGPROF,
  SIGWINCH,
  SIGINFO,
  SIGUSR1,
  SIGUSR2,
  0
};

/* hold info for undelivered signals */
static siginfo_t sis[NSIG];
static struct sigcontext scs[NSIG];

/* set of signals we're ignoring for now. - start by ignoring all until 
   signals_init is called.
*/
static volatile unsigned blocked_signals = 0xffff;

/* set of signals to be processed. */
static volatile unsigned pending_signals = 0;

/* whether there's a pending signal that's not blocked */
volatile unsigned pending_nonblocked_signal = 0;

/* info on the alternate signal stack */
static struct sigaltstack __sigaltstack = {NULL, 0, SS_DISABLE};

/* are we stopped? */
static volatile int stopped=0;

/* HBXX - somewhere here we need to check if we are an orphaned process,
   the process will not stop in response to SIGTSTP, SIGTTIN, or SIGTTOU */
/* stop the process temporarily */
static void signal_stop (int sig, siginfo_t *sip, struct sigcontext *scp) {
  EnterCritical();
  stopped = 1;
  /* clear any pending SIGCONT's */
  pending_signals &= ~signal_bitmasks[SIGCONT];
  pending_nonblocked_signal = 0;
  ExitCritical();
#ifdef PROCESS_TABLE
  /* tell parent that we're stopping */
  EnterCritical();
  if (UAREA.parent_slot != -1)
    kill(getppid(), SIGCHLD);
  ExitCritical();
#endif
#ifdef PROCD
  proc_stop(sig);		/* proc_stop sends sigchld to parent */
#endif
  /* wait for a SIGCONT to set stopped back to 0 */
  //kprintf("%d STOPPING\n",getpid());
  wk_waitfor_value((int *)&stopped, 0, CAP_ROOT);
  //kprintf("%d CONTINUING\n",getpid());

  pending_signals &= ~signal_bitmasks[sig];
  pending_nonblocked_signal = ((pending_signals & ~blocked_signals)) ? 1 : 0;
}

/* Stop process and reset pending_nonblocked_signal variable */
void __signal_stop_process(int sig) { 
  signal_stop(sig,NULL,NULL);
  pending_signals &= ~signal_bitmasks[sig];
  pending_nonblocked_signal = ((pending_signals & ~blocked_signals)) ? 1 : 0;
}

/* uncaught signal that terminates us */
static void signal_terminate (int sig, siginfo_t *sip,
			      struct sigcontext *scp) {
  static int onlyonce = 1;
  extern void __stacktrace();
  extern char *__progname;

  UAREA.u_in_critical = 0; /* take ourselves out of any critical region */
  if (onlyonce) {
    onlyonce = 0;
    kprintf("[%s]Pid %d. Envid %d. Terminated by signal %d[%s].\n",
	    __progname, getpid (), __envid, sig, strsignal(sig));
    if (scp && sip && sip->si_signo == SIGSEGV)
      kprintf("eip=0x%x, esp=0x%x, eflags=0x%x, reason: %s\n"
	      "faulting address: %p, action: %s, mode: %s.\n", scp->sc_eip,
	      scp->sc_esp, scp->sc_eflags,
	      sip->si_code == SEGV_ACCERR ? "permissions violation" :
	      "page not present", sip->si_addr,
	      scp->sc_err & FEC_WR ? "write" : "read",
	      scp->sc_err & FEC_U ? "user" : "supervisor");
    if (sip) switch(sip->si_signo) {
    case SIGSEGV:
    case SIGKILL:
    case SIGBUS:
    case SIGFPE:
    case SIGILL:
      __debug_print_argv(0);
      if (UAREA.u_in_pfault <= 2) __stacktrace(0);
    }
#if 0 /* printf will cause infinite loop in pty code in some cases */
    printf("[%s]Pid %d. Envid %d. Terminated by signal %d[%s].\n",
	   __progname, getpid (), __envid, sig, strsignal(sig));
    if (scp && sip && sip->si_signo == SIGSEGV)
      printf("eip=0x%x, esp=0x%x, eflags=0x%x, reason: %s\n"
	     "faulting address: %p, action: %s, mode: %s.\n", scp->sc_eip,
	     scp->sc_esp, scp->sc_eflags,
	     sip->si_code == SEGV_ACCERR ? "permissions violation" :
	     "page not present", sip->si_addr,
	     scp->sc_err & FEC_WR ? "write" : "read",
	     scp->sc_err & FEC_U ? "user" : "supervisor");
    __debug_print_argv(1);
    if (UAREA.u_in_pfault <= 2) __stacktrace(1);
#endif
  }

  ProcessEnd (W_EXITCODE (0, sig), sip ? (u_int)sip->si_addr : 7);
}

/* called by restore_context when it detects that a signal has been
   deleverd while we were descheduled, and by kill when sending a
   signal to oneself */
int deliversignals = 1;
void DeliverPendingSignal ();
#define SIGNAL_BAND 8
void signals_on(void) {
  d1printf("%d signals_on (at %d)\n",__envid,deliversignals);
  deliversignals++;
  assert(deliversignals > -1*SIGNAL_BAND && deliversignals < SIGNAL_BAND);
  if (deliversignals == 1) {
    pending_nonblocked_signal = (pending_signals & ~blocked_signals) ? 1:0;
    if (pending_nonblocked_signal) {
      d3printf("Delivering signals on signals_on\n");
      DeliverPendingSignal();
    }
  }
}
void signals_off(void) {
  d1printf("%d signals_off (at %d)\n",__envid,deliversignals);
  deliversignals--;
  assert(deliversignals > -1*SIGNAL_BAND && deliversignals < SIGNAL_BAND);
}
void DeliverPendingSignal () {
  int sig, i, oldesp=0;
  siginfo_t si, *sip;
  struct sigcontext sc, *scp;

  d3printf("pending signals: %x, bad signals: %x, ds: %d (pnbs %d)\n",
	  pending_signals,
	  (pending_signals & (signal_bits(SIGKILL)|signal_bits(SIGSEGV)|
			      signal_bits(SIGBUS)|signal_bits(SIGILL)|
			      signal_bits(SIGFPE))),
	  deliversignals,
	  pending_nonblocked_signal);
  /* SIGKILL cannot be delayed, use with caution */
  if (deliversignals < 1) pending_nonblocked_signal = 0;
  /* XXX - shouldn't have SIGTRAP - only a hack for ptrace */
  if (!((pending_signals & (signal_bits(SIGKILL)|signal_bits(SIGSEGV)|
			    signal_bits(SIGBUS)|signal_bits(SIGILL)|
			    signal_bits(SIGFPE)|signal_bits(SIGTRAP))) ||
      deliversignals >= 1)) return;

again:
  i = 0;
  while (sig = delivery_order[i++]) {
    EnterCritical();
    if (pending_signals & ~blocked_signals & signal_bitmasks[sig]) {
      sigset_t oblocked = blocked_signals;
      struct sigaction sa;

      /* always restart from the beginning */
      i = 0;
      
      sa.sa_flags = signal_vec[sig].sa_flags;
      sa.sa_mask = signal_vec[sig].sa_mask;
      sa.sa_handler = ((void*)signal_vec[sig].sa_sigaction == SIG_DFL ?
		    default_signal_vec[sig].sa_handler :
		    signal_vec[sig].sa_handler);

      /* Setup the signal mask */
      blocked_signals |= sa.sa_mask;
      if (!(sa.sa_flags & SA_NODEFER)) blocked_signals |= sig;

      /* Mark signal as no longer pending */
      pending_signals &= ~signal_bitmasks[sig];

      /* Revert to default handler if requested */
      if (sa.sa_flags & SA_RESETHAND) {
	(void*)signal_vec[sig].sa_sigaction = SIG_DFL;
	/* HBXX should not reset mask signal_vec[sig].sa_mask = 0; */
	/* HBXX should not reset flags signal_vec[sig].sa_flags = 0; */
      }

      /* If siginfo requested then fill in siginfo struct, etc... */
      /* XXX - can't hurt to always send */
      if (1 || sa.sa_flags & SA_SIGINFO) {
	sip = &si;
	scp = &sc;

       	/* Fill in siginfo */
	si = sis[sig];
	
	/* Fill in sigcontext */
	sc = scs[sig];
	sc.sc_onstack = ((__sigaltstack.ss_flags & SA_ONSTACK) ? 01 : 0);
	sc.sc_mask = oblocked;
      } else {
	sip = NULL;
	scp = NULL;
      }

      /* set the pending nonblocked signal bit */
      if (pending_signals & ~blocked_signals)
	pending_nonblocked_signal = 1;
      else
	pending_nonblocked_signal = 0;

      /* Switch to alt stack if requested */
      if ((sa.sa_flags & SA_ONSTACK) &&
	  !(__sigaltstack.ss_flags & SS_ONSTACK) &&
	  !(__sigaltstack.ss_flags & SS_DISABLE)) {
	__sigaltstack.ss_flags |= SS_ONSTACK;
	/* copy stack frame to new stack and transfer to it */
	asm("movl %%esp, %0\n"
	    "\tmovl %%ebp, %%esi\n"
	    "\tmovl %1, %%edi\n"
	    "\tmovl %%ebp, %%ecx\n"
	    "\tsubl %%esp, %%ecx\n"
	    "\trep\n"
	    "\t movsb\n"
	    "\tmovl %%edi, %%esp" : "=r" (oldesp)
	    : "r" (__sigaltstack.ss_sp));
      }

      ExitCritical();

      /* call the handler */
      dprintf("%d Calling signal handler %p sig: %d\n",
	      getpid(),sa.sa_sigaction,sig);
      sa.sa_sigaction(sig, sip, scp);

      EnterCritical();

      /* Switch to original stack if necessary */
      if (oldesp) {
	asm("movl %0, %%esp" :: "r" (oldesp));
	oldesp = 0;
      }

      blocked_signals = oblocked;
    }
    ExitCritical();
  }
  /* restart at beginning, if still any left */
  if (pending_nonblocked_signal) goto again;

}

/* create a clause that will wake us when there's a pending signal */
int wk_mksig_pred (struct wk_term *t) {
  int s = 0;

  s = wk_mkvar (s, t, wk_phys (&pending_nonblocked_signal), 0);
  s = wk_mkimm (s, t, 0);
  s = wk_mkop (s, t, WK_NEQ);
  return (s);
}

/* More complex interface to setting signal handlers. Can specify signals
   to block when signal is delieverd and other flags fine-tuning things. */
int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) {

  OSCALLENTER(OSCALL_sigaction);
  /* ensure a proper signal */
  if (sig < 1 || sig >= NSIG) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_sigaction);
    return (-1);
  }

  /* record old signal handler info */
  if (oact) {
    *oact = signal_vec[sig];
  }

  /* don't provide new handler info */
  if (!act) {
    OSCALLEXIT(OSCALL_sigaction);
    return (0);
  }

  /* can't catch or ignore SIGKILL or SIGSTOP */
  if (sig == SIGKILL || sig == SIGSTOP) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_sigaction);
    return (-1);
  }

  /* alright, install the new handler */
  EnterCritical();
  /* maintain the set of ignored signals in case of exec */
  if (signal_vec[sig].sa_sigaction == (void*)SIG_IGN &&
      act->sa_sigaction != (void*)SIG_IGN)
    UAREA.u_sig_exec_ign &= ~signal_bitmasks[sig];
  if (signal_vec[sig].sa_sigaction != (void*)SIG_IGN &&
      act->sa_sigaction == (void*)SIG_IGN)
    UAREA.u_sig_exec_ign |= signal_bitmasks[sig];
  /* copy the new action */  
  signal_vec[sig] = *act;
  /* if setting action to ignore, then cancel any pending signals and reset
     the pending_nonblocked_signal flag if necessary */
  if ((act->sa_sigaction == (void*)SIG_IGN ||
       (act->sa_sigaction == (void*)SIG_DFL &&
	default_signal_vec[sig].sa_sigaction == (void*)SIG_IGN)) &&
      (pending_signals & signal_bitmasks[sig])) {
    pending_signals &= ~signal_bitmasks[sig];
    if (pending_nonblocked_signal && !(pending_signals & ~blocked_signals))
      pending_nonblocked_signal = 0;
  }
  ExitCritical();

  OSCALLEXIT(OSCALL_sigaction);
  return (0);
}

/* change the mask of blocked signals to sigmask and block waiting
   for a signal. Take the signal and then restore the mask of blocked
   signals */
static int sigsuspend_wakeup;
int sigsuspend (const sigset_t *sigmask) {
  sigset_t orig_mask = blocked_signals;

  d2printf("%d sigsuspend: blocked_signals %x, newset: %x pend: %x pnbs %d\n",__envid,
	  blocked_signals, *sigmask, pending_signals, pending_nonblocked_signal);

  OSCALLENTER(OSCALL_sigsuspend);
  EnterCritical();
  blocked_signals =
    *sigmask & ~signal_bitmasks[SIGKILL] & ~signal_bitmasks[SIGSTOP];
  sigsuspend_wakeup = 0;
  ExitCritical();

  wk_waitfor_value_neq(&sigsuspend_wakeup, 0, 0);

  blocked_signals = orig_mask;
  errno = EINTR;
  OSCALLEXIT(OSCALL_sigsuspend);
  return (-1);
}

/* change the mask of blocked signals and/or query it */
int sigprocmask (int how, const sigset_t *set, sigset_t *oset) {
  sigset_t s, o = blocked_signals;

  OSCALLENTER(OSCALL_sigprocmask);

  if (set != NULL) {
    s = *set;
    if (sigismember (&s, SIGKILL)) {
      sigdelset (&s, SIGKILL);
    }
    if (sigismember (&s, SIGSTOP)) {
      sigdelset (&s, SIGSTOP);
    }

    switch (how) {
    case SIG_BLOCK: blocked_signals |= s; break;
    case SIG_UNBLOCK: blocked_signals &= ~s; break;
    case SIG_SETMASK: blocked_signals = s; break;
    default:
      errno = EINVAL;
      OSCALLEXIT(OSCALL_sigprocmask);
      return (-1);
    }
  }

  if (oset != NULL) {
    *oset = o;
  }

  OSCALLEXIT(OSCALL_sigprocmask);
  d2printf("%d sigprocmask blocked_signals: %x\n",__envid,blocked_signals);
  /* if a signal is now unblocked and pending, then go to it */
  EnterCritical();
  if (pending_signals & ~blocked_signals) pending_nonblocked_signal = 1;
  ExitCritical();
  if (pending_nonblocked_signal) {
    d3printf("Delivering signals on sigprocmask\n");
    DeliverPendingSignal();
  }

  return (0);
}

/* what signals are pending */
int sigpending(sigset_t *set) {
  OSCALLENTER(OSCALL_sigpending);
  if (set != NULL) *set = pending_signals;
  OSCALLEXIT(OSCALL_sigpending);
  return 0;
}

/* provide for separate signal stack */
int sigaltstack(const struct sigaltstack *ss, struct sigaltstack *oss) {
  struct sigaltstack o = __sigaltstack;

  OSCALLENTER(OSCALL_sigaltstack);
  if (ss != NULL) {
    if ((ss->ss_flags & SS_DISABLE) && (__sigaltstack.ss_flags & SS_ONSTACK)) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_sigaltstack);
      return -1;
    }
    if (!(ss->ss_flags & SS_DISABLE) && (ss->ss_size <= MINSIGSTKSZ)) {
      errno = ENOMEM;
      OSCALLEXIT(OSCALL_sigaltstack);
      return -1;
    }
    __sigaltstack = *ss;
    __sigaltstack.ss_flags &= ~SS_ONSTACK;
  }

  if (oss != NULL) {
    *oss = o;
  }

  OSCALLEXIT(OSCALL_sigaltstack);
  return 0;
}

/* return from a signal - XXX race condition? */
int sigreturn(struct sigcontext *scp) {
  char dcode[100];
  u_int dcode_size;

  OSCALLENTER(OSCALL_sigreturn);

  EnterCritical();
  /* restore signal mask */
  blocked_signals = scp->sc_mask & ~SIGKILL & ~SIGSTOP;

  /* stack status */
  if (scp->sc_onstack & 01)
    __sigaltstack.ss_flags |= SS_ONSTACK;
  else
    __sigaltstack.ss_flags &= ~SS_ONSTACK;
  ExitCritical();

  /* set up dynamic code: eip and cs jmp */
  dcode_size = &&dcode_stop - &&dcode_start;
  memcpy(dcode, &&dcode_start, dcode_size);
  dcode[dcode_size++] = 0xea; /* jmp opcode */
  *(u32*)(&dcode[dcode_size]) = scp->sc_cs;
  dcode_size += 2;
  *(u32*)(&dcode[dcode_size]) = scp->sc_eip;
  dcode_size += 4;

  OSCALLEXIT(OSCALL_sigreturn);

  /* restore most registers */
  asm("movl %0, %%eax\n" /* put address of dynamic code in eax */
      "\tpushl %1\n"     /* use the context as stack */
      "\tpopl %%esp\n"
      "\tpopl %%gs\n"
      "\tpopl %%fs\n"
      "\tpopl %%es\n"
      "\tpopl %%ds\n"
      "\tpopl %%edi\n"
      "\tpopl %%esi\n"
      "\tpopl %%ebp\n"
      "\tpopl %%ebx\n"
      "\tpopl %%edx\n"
      "\tpopl %%ecx\n"
      "\tjmpl %%eax" :: "r" (dcode), "r" (scp));

 dcode_start:
  /* dynamic code */
  asm("popl %eax\n"
      "\taddl $8, %esp\n"
      "\tpopfl\n"
      "\tlss 0(%esp), %esp"); /* set ss and esp, then the jmp... */
 dcode_stop:

  /* never reached */
  errno = EINVAL;
  return -1;
}


static int kill1(pid_t pid,int signal) {
  u_int envid;
  if (!(envid = pid2envid(pid))) {
    errno = ESRCH;
    return (-1);
  }
  send_signal(envid, signal);
  return 0;
}
static int killpg1(pid_t pgrp, int signal) {
#ifdef PROCD
  pgrp_p pg;
  proc_p p;
  proc_p iter;
  dprintf("killpg1 pgrp %d signo %d  ",pgrp, signal);
  pg = pgfind(pgrp);
  dprintf("pgfind: %p\n",pg);
  if (pg == 0) {
    errno = ESRCH;
    return (-1);
  } else {
    /* do some permission checking here */
    /* The sending process and members of the process group must have the same */
    /* effective user ID, or the sender must be the super-user.  As a single */
    /* special case the continue signal SIGCONT may be sent to any process that */
    /* is a descendant of the current process. */
    iter = init_iter_pg_p(pg);
    EnterCritical();
#if 0
    {
      char state[] = "XIRSHZ";
      kprintf("killpg1: ");
      while((p = iter_pg_p(&iter))) kprintf("%d (%c) ",p->p_pid,state[p->p_stat]);
      kprintf("\n");
    }
      iter = init_iter_pg_p(pg);
#endif
    while ( (p = iter_pg_p (&iter)) ) {
      dprintf("killpg1 pid %p kill to: %d envid: %d\n",p,p->p_pid,p->envid);
      send_signal(p->envid, signal);
    }

    ExitCritical();
    //procd_ps();
    return 0;
  }
#endif
#ifdef PROCESS_TABLE
    //kprintf("cannot send signals to process groups\n");
    return 0;
#endif
}

int kill (int pid, int signal) {
  int ret;
  dprintf("pid %d kill to %d signo %d (%s)\n",getpid(), pid,signal,strsignal(signal));
  OSCALLENTER(OSCALL_kill);
  /* make sure sig might be reasonable */
  if (signal < 0 || signal >= NSIG) {
    kprintf("kill invalid argument from %d, pid %d signal %d\n",getpid(),pid,signal);
    errno = EINVAL;
    ret = -1;
  } else if (pid != getpid ()) {
    /* see if this is a signal sent to ourselves or to another process */
    /* make sure they exist */
    if (pid > PID_MAX) {
      errno = ESRCH;
      ret = -1;
    } else if (pid == 0) {
      /* to our process group */
      ret = killpg1(getpgrp(),signal);
    } else if (pid < -1) {
      /* to process group -pid */
      ret = killpg1(-pid,signal);
    } else if (pid == -1) {
      /* to all processes */
      fprintf(stderr,"kill(-1,%d) not supported\n",signal);
      errno = EPERM;
      ret = -1;
    } else {
      /* to process pid */
      ret = kill1(pid,signal);
    }
  } else {
    /* call the handler */
    receive_signal(IPC_SIGNAL, signal, 0, 0, __envid);
    if (pending_nonblocked_signal) {
      d3printf("Delivering signals on kill (to myself)\n");
      DeliverPendingSignal();
    }
    ret = 0;
  }

  OSCALLEXIT(OSCALL_kill);
  return (ret);
}



/* internal way of self-signalling (eg, for faults) which gives si and sc */
int _kill(siginfo_t *si, struct sigcontext *sc) {
  int sig = si->si_signo;

  /* make sure sig might be reasonable, if 0, then just check permission */
  if (sig < 1 || sig >= NSIG) {
    errno = EINVAL;
    return (-1);
  }

  EnterCritical();
  sis[sig] = *si;
  scs[sig] = *sc;
  receive_signal(IPC_SIGNAL, sig, 0, 0, __envid);
  ExitCritical();

  if (pending_nonblocked_signal) {
    d3printf("Delivering signals on _kill (to myself)\n");
    DeliverPendingSignal();
  }

  return 0;
}

int signals_init() {
  int i;

  /* check through the set of signals passed down from the parent which are
     to be ignored and set them to be ignored */
  for (i=0; i < NSIG; i++)
    if (UAREA.u_sig_exec_ign & (1 < i)) {
      signal_vec[i].sa_sigaction = (void*)SIG_IGN;
      /* HBXX should not reset mask signal_vec[sig].sa_mask = 0; */
      /* HBXX should not reset flags signal_vec[sig].sa_flags = 0; */
    }
  UAREA.u_sig_exec_ign = 0;
  /* allow signals */
  blocked_signals = 0;
  pending_signals = 0;
  pending_nonblocked_signal = 0;
  /* register the signal handler */
  ipc_register(IPC_SIGNAL, receive_signal);

  return 1;
}

static int receive_signal(int code, int signal, int b, int c, u_int caller) {
  assert(code == IPC_SIGNAL);
  if (signal > 0 && signal < NSIG) {
    /* a stopped process must continue on SIGCONT */
    if ((signal == SIGCONT || signal == SIGKILL) && stopped) {
      EnterCritical();
#ifdef PROCD
      proc_continue();
#endif
      //kprintf("%d receive signal (CONT) stopped 0\n",getpid());
      stopped = 0;
      /* and all stopping signals must be cleared */
      pending_signals &= ~(signal_bits(SIGSTOP) | 
			   signal_bits(SIGTSTP) | 
			   signal_bits(SIGTTIN) | 
			   signal_bits(SIGTTOU));
      ExitCritical();
      /* "restore" any pending nonblocked signals */
      pending_nonblocked_signal = (pending_signals & ~blocked_signals) ? 1:0;
      /* don't post SIGCONT */
      if (signal == SIGCONT) {
	sigsuspend_wakeup = 1;
	return 0;
      }
    }



    if (signal == SIGCHLD) {
      struct sigaction sa;
      sa.sa_flags = signal_vec[signal].sa_flags;
      sa.sa_mask = signal_vec[signal].sa_mask;
#ifdef PROCD
      /* HBXX it is not clear if SA_NOCLDWAIT implies SA_NOCLDSTOP (guessing yes) */
      if (sa.sa_flags & SA_NOCLDWAIT) {
	proc_p p;
	
	p = efind(caller);
	assert(p);
	//kprintf("SA_NOCLDWAIT: %d\n",p->p_stat);
	if (p->p_stat == SZOMB) {
	  /* reap process */
	  proc_reap(p);
	}
	sigsuspend_wakeup = 1;
	return 0;
      }
      if (sa.sa_flags & SA_NOCLDSTOP) {
	/* must determine whether a stop or an exit */
	proc_p p;
	p = efind(caller);
	if (p == NULL) {
	  kprintf("receive_signal: Warning: SIGCHLD from children that has been deallocated already!?\n");
	  return 0;
	}
	sigsuspend_wakeup = 1;
	if (p->p_stat == SSTOP) return 0;
      }
#endif
    }



    /* "record" generation of signal */
    if (signal_vec[signal].sa_handler != SIG_IGN &&
	(signal_vec[signal].sa_handler != SIG_DFL ||
	 default_signal_vec[signal].sa_handler != SIG_IGN)) {
      /* add signal to pending_signals */
      pending_signals |= signal_bitmasks[signal];
      if ((pending_signals & ~blocked_signals) && !stopped) {
	sigsuspend_wakeup = 1;
	pending_nonblocked_signal = 1;
      }
    }
  }
  return 0;
}

static int send_signal(u_int envid, int signal) {
  int ret;
  ret = sipcout(envid, IPC_SIGNAL, signal, 0, 0);
  if (ret != 0) 
    kprintf("%d send_signal %d, %d --> %d\n",getpid(),envid,signal,ret);
  return ret;
   
}

/* Used internally by the LibOS */

/* __issigignore returns 1 on signals that are set to SIG_IGN or
 * who are set to SIG_DFL where the default is to ignore
 * excludes SIGCONT
 * (comes from OpenBSD proc->p_sigignore)
 */
int __issigignored(int sig) {
  if (sig == SIGCONT) return 0;
  if ((void *)signal_vec[sig].sa_sigaction == SIG_IGN ||
      ((void *)signal_vec[sig].sa_sigaction == SIG_DFL &&
       default_signal_vec[sig].sa_handler == SIG_IGN)) 
    return 1;
  return 0;

}
/* __issigmasked returns non-zero if the signal is blocked, 
 * (equivalent to playing around with sigprocmask, but faster)
 */
int __issigmasked(int sig) {
  sigset_t o = blocked_signals;
  return (o & sigmask(sig));
}
/* returns non-zero if we should restart system call on this signal */
int __issigrestart(int sig) {
  return (signal_vec[sig].sa_flags & SA_RESTART);
}
/* returns 0 if there is not signal, otherwise it returns the signal ready to be 
 * delivered */
int __issigready(void) {
  int sig, i = 0;
  EnterCritical();
  if (pending_nonblocked_signal)
    while (sig = delivery_order[i++]) 
      if (pending_signals & ~blocked_signals & signal_bitmasks[sig]) {
	ExitCritical();
	return sig;
      }
  ExitCritical();
  return 0;
}

/* returns 1 if the action of the signal is to stop the process */
int __issigstopping(int sig) {
  return ((sig == SIGSTOP || sig == SIGTSTP ||
	   sig == SIGTTIN || sig == SIGTTOU) &&
	  signal_vec[sig].sa_handler == SIG_DFL) ? 1 : 0;
}
