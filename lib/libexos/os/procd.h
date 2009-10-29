
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

/*	$OpenBSD: proc.h,v 1.16 1997/09/15 05:46:14 millert Exp $	*/
/*	$NetBSD: proc.h,v 1.44 1996/04/22 01:23:21 christos Exp $	*/

/*-
 * Copyright (c) 1986, 1989, 1991, 1993
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
 *	@(#)proc.h	8.8 (Berkeley) 1/21/94
 */

#ifndef _OS_PROCD_H_
#define _OS_PROCD_H_

#include <bitstring.h>
#include <sys/types.h>
#include <sys/param.h>
#include <xok/env.h>
#include <xok/queue.h>

/* DESIGN: similar to arpd
   all processes go through procd for:
   - setsid, 
   - setpgid, 
   - getnewpid()
   - wait4() ; which sets waiting variable, you poll on waiting variable
   - setlogin();
   you read from shm for:
   - getpid()
   - getpgrp()
   - getppid()
   - getlogin()
   - iterate_pgrp_pids
   */
typedef struct proc    proc_t, *proc_p;
typedef struct pgrp    pgrp_t,*pgrp_p;
typedef struct session session_t, *session_p;

#define p_session       p_pgrp->pg_session
#define p_pgid          p_pgrp->pg_id


#define P_CONTROLT      0x00002 /* Has a controlling terminal. */
#define P_PPWAIT        0x00010 /* Parent is waiting for child to exit. 
				 * (also for exec, but not this is not used) */
#define	P_TRACED	0x00800	/* Debugged process being traced. */
#define	P_WAITED	0x01000	/* Debugging process has waited for child, 
				   needed for wait4 and WUNTRACED option. */
#define	P_EXEC		0x04000	/* Process called exec. */

#define HASEXECED(p) (p->p_flag & P_EXEC) /* HBXX NEED TO FIX THIS to use P_EXEC? */

#define INITPROC 1
#define CAP_PROCD 0
struct proc {
  pid_t   p_pid;		/* its pid */
  pgrp_p  p_pgrp;		/* pgrp of this process */
  LIST_HEAD(, proc) p_children; /* Pointer to list of children. */
  proc_p p_pptr;                /* Pointer to parent process. */
  LIST_ENTRY(proc) p_sibling;   /* List of sibling processes. */
  LIST_ENTRY(proc) p_pglist;    /* List of processes in pgrp. */
  u_short     p_xstat;		/* exit status */
  u_short     p_stat;		/* status of process RUN,SLEEP,STOP,ZOMB */
  int	      p_flag;		/* P_* flags. */
  u_int       nxchildren;	/* # of changes of xstat of children */
  u_int       envid;		/* envid, when it is 0 means entry is free */
};

struct pgrp {
  session_p pg_session;		/* session where pgrp belongs */
  LIST_HEAD(, proc) pg_members;   /* Pointer to pgrp members. */
  pid_t     pg_id;			/* pgrp id */
  int       pg_jobc;		/* # procs qualifying for jobc */
};

struct tty;
struct file;
struct session {
  proc_p    s_leader;		/* session leader */
  int       s_count;		/* number of pgrps in session */
  char      s_login[MAXLOGNAME];
  struct tty *s_ttyp;		/* controlling terminal (opaque) */
  struct file *s_ttyvp;		/* file table entry */
};

extern proc_p curproc;


/* Status values. */
#define SIDL    1               /* Process being created by fork. */
#define SRUN    2               /* Currently runnable. */
#define SSLEEP  3               /* Sleeping on an address. */
#define SSTOP   4               /* Process suspended. */
#define SZOMB   5               /* Awaiting collection by parent. */


#define GETPGRP(p) ((p)->p_pgrp->pg_id)
#define SESS_LEADER(p) ((p)->p_session->s_leader == (p))

/* could be any value */
#define MAXSESSION 32
#define MAXPGRP NENV
#define MAXPROC NENV

#define PID_MAX MAXPROC
#define NO_PID (PID_MAX + 1)

typedef struct proc_table {
  unsigned int procd_envid;
  bitstr_t bit_decl(sessions_used,MAXSESSION);
  bitstr_t bit_decl(pgrps_used,MAXPGRP);
  bitstr_t bit_decl(procs_used,MAXPROC);
  session_t sessions[MAXSESSION];
  pgrp_t pgrps[MAXPGRP];
  proc_t procs[MAXPROC];
} proc_table_t, *proc_table_p;

extern proc_table_p proc_table;
int proc_table_init(void);


void protect_proc_table(void);
void unprotect_proc_table(void);


unsigned int get_procd_envid(void);
void set_procd_envid(unsigned int envid);


/* IPC HANDLERS */
int proc_table_ipc_fork(int code, int envidp, int envidc, int nop, u_int caller);
int proc_table_ipc_exec(int code, int envidp, int envidc, int nop, u_int caller);
int proc_table_ipc_setlogin(int code, int l1234, int l5678, int l9012, u_int caller);
int proc_table_ipc_setsid(int code, int nop0, int nop1, int nop2, u_int caller);
int proc_table_ipc_setpgid(int code, int pid , int pgrp, int nop2, u_int caller);
#define REAPOP_REAP   1		/* reap the process */
#define REAPOP_WAITED 2		/* mark it as waited up */
#define REAPOP_PPWAIT 3		/* process is waiting */
int proc_table_ipc_reap(int code, int envid, int nop2, int nop3, u_int caller);
int proc_table_ipc_setstat(int code, int envid, int stat, int xstat, u_int caller);
int proc_table_ipc_controlt(int code, int ttyp, int ttyvp, int controlt, u_int caller);
int proc_table_ipc_ptrace(int code, int value, int nop1, int nop2, u_int caller);
int proc_table_ipc_reparent(int code, int new_parent_envid, int nop1, int nop2, u_int caller);


/* locators */
proc_p efind(u_int envid);
proc_p __pd_pfind(pid_t pid);
pgrp_p pgfind(pid_t pgid);

/* iterators */
int init_iter_proc_p(void);
proc_p iter_proc_p(int *);

void init_iter_pgrp_p(pid_t session);
pgrp_p iter_pgrp_p(void);

proc_p init_iter_pg_p(pgrp_p pg);
proc_p iter_pg_p(proc_p *);



/* allocators */
/* HBXX - proc_alloc, return p with a p->p_pid being a unique pid */
proc_p proc_alloc(void);
void proc_dealloc(proc_p p);

pgrp_p pgrp_alloc(void);
void pgrp_dealloc(pgrp_p pgrp);

session_p session_alloc(void);
void session_dealloc(session_p session);


int inferior(proc_p p1, proc_p p2);
void fixjobc(proc_p p, pgrp_p pgrp, int entering);
void pgdelete(pgrp_p pgrp);
int leavepgrp(proc_p p);
int enterpgrp(proc_p p, pid_t pgid, int mksess);

void proc_reparent(proc_p child, proc_p parent);

pid_t proc_fork1(u_int newenvid, int forktype);
#define PROCD_FORK  0
#define PROCD_VFORK 1
static inline pid_t proc_fork(u_int newenvid) {return proc_fork1(newenvid,PROCD_FORK);}
static inline pid_t proc_vfork(u_int newenvid) {return proc_fork1(newenvid,PROCD_VFORK);}

pid_t proc_exec(u_int newenvid);
int proc_exit(int exit_status);
int proc_reap(proc_p p);
int proc_pid_reap(pid_t pid);
/* used for cleaning up processes or by exokill */
int proc_pid_exit(pid_t pid, int exit_status);

/* used when a process is stopped */
int proc_stop(int signum);
int proc_continue(void);
/* is parent waiting for our exec or exit? (as in vfork),
 * this should be used by signal handler to make sure we don't deadlock
 * that is stop the process while this is true */
static inline int proc_isppwait(void) {return (curproc->p_flag & P_PPWAIT);}

int proc_controlt(int controlt, int ttyp, int ttyvp);
#define proc_controlt_setflag() proc_controlt(1,-1,-1)
#define proc_controlt_clearflag() proc_controlt(0,-1,-1)


void print_proc(proc_p p);
void print_pgrp(pgrp_p pg);
void print_session(session_p s);



/* there are two definitions of psignal, one to print the signal (user level
 * openbsd), and one to post signal in the kernel.  We want kernel one.
 */
int proc_psignal(proc_p p, int signo);
/*
 * Send a signal to a process group.  If checktty is 1,
 * limit to members which have a controlling terminal.
 */
void proc_pgsignal(pgrp_p pg, int signum, int checkctty);


/* for PS */
void procd_ps(void);
#endif /* _OS_PROCD_H_ */
