
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

#ifndef EXOS_SIGNAL_H
#define EXOS_SIGNAL_H

#include <signal.h>

/* Prototypes non-unix signal stuff. */
int signals_init();
int _kill(siginfo_t *si, struct sigcontext *sc);

/* signals are posted but not delivered, can be called in a nested manner. */
void signals_off(void);
void signals_on(void);		


/* __issigignore returns 1 on signals that are set to SIG_IGN or
 * who are set to SIG_DFL where the default is to ignore
 * excludes SIGCONT
 * (comes from OpenBSD proc->p_sigignore)
 */
int __issigignored(int sig);

/* __issigmasked returns 1 if the signal is blocked, 
 * (equivalent to playing around with sigprocmask, but faster)
 */
int __issigmasked(int sig);

/* returns non-zero if we should restart system call on this signal */
int __issigrestart(int sig);

/* returns 0 if there is not signal, otherwise it returns the signal ready to be 
 * delivered */
int __issigready(void);

/* returns 1 if the action of the signal is to stop the process */
int __issigstopping(int sig);

/* Stop process and reset pending_nonblocked_signal variable */
void __signal_stop_process(int sig);

#endif
