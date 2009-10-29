
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

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

#include <errno.h>
#include <exos/ipc.h>
#include <exos/ipcdemux.h>
#include <exos/mallocs.h>
#include <exos/osdecl.h>
#include <exos/process.h>
#include <exos/ptrace.h>
#include <exos/uidt.h>
#include <exos/vm.h>
#include <machine/reg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <unistd.h>
#include "procd.h"

/* used in entry.S */
u_int _exos_ptrace_eax_save;
u_int _exos_ptrace_ebx_save;

int is_being_ptraced() {
  return UAREA.u_ptrace_flags & EXOS_PT_BEING_PTRACED;
}

static inline void set_ptraced() {
  sipcout(get_procd_envid(), IPC_PROC_PTRACE, 1, 0, 0);
  UAREA.u_ptrace_flags |= EXOS_PT_BEING_PTRACED;
}

static inline void unset_ptraced() {
  sipcout(get_procd_envid(), IPC_PROC_PTRACE, 0, 0, 0);
  UAREA.u_ptrace_flags &= ~EXOS_PT_BEING_PTRACED;
}

static inline void set_pcontinue() {
  UAREA.u_ptrace_flags |= EXOS_PT_PCONTINUE;
}

static inline void unset_pcontinue() {
  UAREA.u_ptrace_flags &= ~EXOS_PT_PCONTINUE;
}

static inline void set_ptracer(u_int envid) {
  UAREA.u_ptrace_tracer = envid;
}

static inline u_int get_ptracer() {
  return UAREA.u_ptrace_tracer;
}

#define PT_BREAKPOINT asm("int $3")

struct ptrace_shared_data {
  u_int read_data;
  u_int errcode;
  struct reg r;
  struct fpreg fr;
};

struct ptrace_share_map {
  pid_t pid;
  u_int remote_va;
  struct ptrace_shared_data *local_va;
};
static struct ptrace_share_map *ptrace_process_map = 0;
static int mapsize = 0;

struct reg pt_r_s; /* location for asm stub to store regs */

void ptrace_breakpoint_handler_c() {
  sigset_t a;

  pt_r_s.r_esp += 4*3; /* adjust for eip, cs, and eflags */
  signal(SIGTRAP, SIG_DFL);
  sigemptyset(&a);
  sigaddset(&a, SIGTRAP);
  sigprocmask(SIG_UNBLOCK, &a, NULL);
  kill(getpid(), SIGTRAP);
  pt_r_s.r_esp -= 4*3;
}

/* setup breakpoint trap instruction handler */
static void register_ptrace_bp_handler() {
  extern u_int ptrace_breakpoint_handler_s;
  /* Int 1 = Debug exception for when single stepping */
  uidt_register_sfunc(1, (u_int)&ptrace_breakpoint_handler_s);
  /* Int 3 = Breakpoint handler for explicitly set INT3's */
  uidt_register_sfunc(3, (u_int)&ptrace_breakpoint_handler_s);
}

static void unregister_ptrace_bp_handler() {
  uidt_unregister(1);
  uidt_unregister(3);
}

#define	ISSET(t, f)	((t) & (f))
#define LEGAL_REQUEST						\
{								\
  proc_p t;							\
								\
  /* are we being traced? */					\
  if (!is_being_ptraced()) return EPERM;			\
  /* are we being traced by caller? */				\
  if (get_ptracer() != caller) return EBUSY;			\
  /* are we stopped? */						\
  t = efind(__envid);						\
  if (t->p_stat != SSTOP || !ISSET(t->p_flag, P_WAITED))	\
    return (EBUSY);						\
}

#define EXOS_PT_SET_ADDR  (PT_FIRSTMACH+100)
#define EXOS_PT_GET_SHARE (PT_FIRSTMACH+101)

#define ptisvawriteable(va) \
(((vpd[PDENO((va))] & (PG_P | PG_U | PG_W)) == (PG_P | PG_U | PG_W)) && \
 (((vpt[PGNO((va))] & (PG_P | PG_U | PG_W)) == (PG_P | PG_U | PG_W)) || \
  ((vpt[PGNO((va))] & (PG_P | PG_U | PG_COW | PG_RO)) == \
   (PG_P | PG_U | PG_COW))))

static int writeaddr(u_int addr, u_int data) {
  /* if it's plainly writeable, then write it */
  if (ptisvawriteable(addr) && ptisvawriteable(addr+3)) {
    *(u_int*)addr = data;
    return 0;
  }
  /* if it's mapped read-only then make it writeable and write */
  if (isvareadable(addr) && isvareadable(addr+3))
    if (sys_self_mod_pte_range(0, PG_COW, PG_RO, addr,
			       PGNO(addr+3)-PGNO(addr)+1) < 0)
      return EPERM;
    else {
      *(u_int*)addr = data;
      return 0;
    }

  /* test the location */
  __fault_test_va = addr;
  __fault_test_enable = 1;
  *(char*)addr = (char)data;
  if (!__fault_test_enable) {
    /* unmap courtesy page */
    _exos_self_insert_pte (0, 0, PGROUNDDOWN(addr), 0, NULL);
    return EPERM;
  }
  if (PGNO(addr) != PGNO(addr+3)) {
    __fault_test_va = addr+3;
    *(char*)(addr+3) = ((char*)&data)[3];
    if (!__fault_test_enable) {
      /* unmap courtesy page */
      /* XXX - need to replace data already written at addr */
      _exos_self_insert_pte (0, 0, PGROUNDDOWN(addr+3), 0, NULL);
      return EPERM;
    }
  }
  __fault_test_enable = 0;
  *(u_int*)addr = data;
  return 0;
}

static int readaddr(u_int addr, u_int *data) {
  /* if it's plainly readable, then read it */
  if (isvareadable(addr) && isvareadable(addr+3)) {
    *data = *(u_int*)addr;
    return 0;
  }
  /* test the location */
  __fault_test_va = addr;
  __fault_test_enable = 1;
  *(char*)data = *(char*)addr;
  if (!__fault_test_enable) {
    /* unmap courtesy page */
    _exos_self_insert_pte (0, 0, PGROUNDDOWN(addr), 0, NULL);
    return EPERM;
  }
  if (PGNO(addr) != PGNO(addr+3)) {
    __fault_test_va = addr+3;
    *(char*)data = *(char*)(addr+3);
    if (!__fault_test_enable) {
      /* unmap courtesy page */
      _exos_self_insert_pte (0, 0, PGROUNDDOWN(addr+3), 0, NULL);
      return EPERM;
    }
  }
  __fault_test_enable = 0;
  *data = *(u_int*)addr;
  return 0;
}

static int ptrace_ipc_handler(int code, int request, int addr, int data,
			      u_int caller) {
  static struct ptrace_shared_data *shared_data = 0;
  static u_int oldparent=0
;
  if (code != IPC_PTRACE) return -1;

  switch (request) {
  case PT_READ_I:
  case PT_READ_D:
    LEGAL_REQUEST;
    if (!shared_data) { /* allocate page on the ptracee side */
      shared_data = __malloc(NBPG);
      if (!shared_data) return -1;
    }
    {
      u_int r;
      shared_data->errcode = readaddr(addr, &r);
      if (shared_data->errcode == 0) shared_data->read_data = r;
      return (int)shared_data;
    }
  case PT_WRITE_I:
  case PT_WRITE_D:
    LEGAL_REQUEST;
    return writeaddr(addr, data);
  case PT_DETACH:
    LEGAL_REQUEST;
    unregister_ptrace_bp_handler();
    unset_ptraced();
    unset_pcontinue();
    set_ptracer(0);
    if (shared_data) {
      __free(shared_data);
      shared_data = 0;
    }
    sipcout(get_procd_envid(), IPC_PROC_REPARENT, oldparent, 0, 0);
    kill(getpid(), SIGCONT);
    return 0;
  case PT_STEP: 
  case PT_KILL:
  case PT_CONTINUE:
    LEGAL_REQUEST;
    set_pcontinue();
    /* set the single step debug flag if stepping*/
    if (request == PT_STEP)
      pt_r_s.r_eflags |= FL_TF;
    else
      pt_r_s.r_eflags &= ~FL_TF;
    kill(getpid(), SIGCONT);
    return 0;
  case EXOS_PT_SET_ADDR:
    LEGAL_REQUEST;
    pt_r_s.r_eip = addr;
    return -1;
  case EXOS_PT_GET_SHARE:
    LEGAL_REQUEST;
    if (!shared_data) { /* allocate page on the ptracee side */
      shared_data = __malloc(NBPG);
      if (!shared_data) return -1;
    }
    shared_data->errcode = 0; /* touch page and init */
    return (int)shared_data;
  case PT_ATTACH:
    if (is_being_ptraced()) return EBUSY;
    set_ptraced();
    set_ptracer(caller);
    unset_pcontinue();
    register_ptrace_bp_handler();
    oldparent = pid2envid(getppid());
    sipcout(get_procd_envid(), IPC_PROC_REPARENT, caller, 0, 0);
    /* XXX - check permissions */
    UAREA.u_status++;
    return 0;
  case PT_GETREGS:
    LEGAL_REQUEST;
    if (!shared_data) { /* allocate page on the ptracee side */
      shared_data = __malloc(NBPG);
      if (!shared_data) return -1;
    }
    shared_data->errcode = 0;
    shared_data->r = pt_r_s;
    return (int)shared_data;
  case PT_SETREGS:
    LEGAL_REQUEST;
    if (!shared_data) return -1;
    pt_r_s = shared_data->r;
    return 0;
  case PT_GETFPREGS:
    LEGAL_REQUEST;
    if (!shared_data) { /* allocate page on the ptracee side */
      shared_data = __malloc(NBPG);
      if (!shared_data) return -1;
    }
    shared_data->errcode = 0;
    /* shared_data->fr = *(u_int*)addr; XXX - implement */
    return (int)shared_data;
  case PT_SETFPREGS:
    LEGAL_REQUEST;
    if (!shared_data) return -1;
    /* shared_data->fr = *(u_int*)addr; XXX - implement */
    return 0;
  }

  return -1;
}

/* remove a share mapping */
static void share_detach(pid_t pid) {
  int i;
  void *res;

  /* does the requested mapping exist? */
  for (i=0; i < mapsize; i++)
    if (ptrace_process_map[i].pid == pid) {
      __free(ptrace_process_map[i].local_va);
      ptrace_process_map[i] = ptrace_process_map[mapsize-1];
      mapsize--;
      res = __realloc(ptrace_process_map,
		      mapsize*sizeof(struct ptrace_share_map));
      if (res) ptrace_process_map = res;
    }
}

/* XXX - inefficient */
/* set va (the remote va) to 1 if you don't know it */
static struct ptrace_shared_data *share_if_not_shared(u_int va, pid_t pid) {
  int i;
  void *tempp;
  struct ptrace_shared_data *local_va;
  int r;

  /* does the requested mapping already exist? */
  for (i=0; i < mapsize; i++)
    if (ptrace_process_map[i].pid == pid) {
      /* unless we don't know the va then require it be the same */
      if ((ptrace_process_map[i].remote_va == va || va == 1) &&
	  PGNO(sys_read_pte(va, 0, pid2envid(pid), &r)) ==
	  PGNO(vpt[PGNO((u_int)ptrace_process_map[i].local_va)]))
	return ptrace_process_map[i].local_va;
      /* The ptracee has exec'd or otherwise relocated its share page */
      share_detach(pid);
      va = 1;
      break;
    }

  /* mapping doesn't exist, so add it */
  if (va == 1) { /* must ask ptracee for remote_addr */
    va = sipcout(pid2envid(pid), IPC_PTRACE, EXOS_PT_GET_SHARE, 0, 0);
    if ((va % NBPG) != 0) return (struct ptrace_shared_data *)-1;
  }
  local_va = (struct ptrace_shared_data*)__malloc(NBPG);
  if (!local_va) return 0;
  mapsize++;
  tempp = __realloc(ptrace_process_map,
		    mapsize*sizeof(struct ptrace_share_map));
  if (!tempp) {
    mapsize--;
    __free(local_va);
    return 0;
  }
  ptrace_process_map = (struct ptrace_share_map*)tempp;
  ptrace_process_map[mapsize-1].pid = pid;
  ptrace_process_map[mapsize-1].remote_va = va;
  ptrace_process_map[mapsize-1].local_va = local_va;
  /* now map in the page from the other process */
  if (_exos_self_insert_pte(0, sys_read_pte(va, 0, pid2envid(pid), &r),
			    (u_int)local_va, 0, NULL) == -1) {
    void *res;

    __free(local_va);
    mapsize--;
    res = __realloc(ptrace_process_map,
		    mapsize*sizeof(struct ptrace_share_map));
    if (res) ptrace_process_map = res;
    return 0;
  }
  return ptrace_process_map[mapsize-1].local_va;
}

int ptrace(int request, pid_t pid, caddr_t addr, int data) {
  u_int res;
  int ret=0;

  OSCALLENTER(OSCALL_ptrace);
  switch (request) {
  case PT_TRACE_ME:
    set_pcontinue();
    set_ptraced();
    set_ptracer(pid2envid(getppid()));
    register_ptrace_bp_handler();
    errno = 0;
    ret = 0;
    goto ptrace_return;
  case PT_READ_I:
  case PT_READ_D:
    res = sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data);
    if ((int)res == -1) {
      errno = EPERM; /* XXX set errno's correctly */
      ret = -1;
      goto ptrace_return;
    } else {
      struct ptrace_shared_data *psd = share_if_not_shared(res, pid);
      /* psd must be on page boundary */
      if (!psd || (PGMASK & (u_int)psd) || psd->errcode != 0) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
	goto ptrace_return;
      }
      errno = 0;
      ret = psd->read_data;
      goto ptrace_return;
    }
  case PT_WRITE_I:
  case PT_WRITE_D:
    if (sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data) != 0) {
      errno = EPERM; /* XXX set errno's correctly */
      ret = -1;
    } else {
      errno = 0;
      ret = 0;
    }
    goto ptrace_return;
  case PT_KILL:
    data = SIGKILL;
    addr = (caddr_t)1;
  case PT_STEP:
  case PT_DETACH:
  case PT_CONTINUE:
    /* sanity check data */
    if (data < 0 || data > NSIG) {
      errno = EINVAL;
      ret = -1;
      goto ptrace_return;
    }
    /* first, tell process to reset it's pc, if necessary */
    if (addr != (caddr_t)1 && request != PT_DETACH)
      if (sipcout(pid2envid(pid), IPC_PTRACE, EXOS_PT_SET_ADDR, (u_int)addr,
		  data) != 0) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
	goto ptrace_return;
      }
    /* next, send signal if requested */    
    if (data)
      if (kill(pid, data)) {
	/* errno set by kill */
	ret = -1;
	goto ptrace_return;
      }
    /* now tell process to continue */
    if (sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data) != 0) {
      errno = EPERM; /* XXX set errno's correctly */
      ret = -1;
    } else {
      if (request == PT_DETACH) share_detach(pid);
      errno = 0;
      ret = 0;
    }    
    goto ptrace_return;
  case PT_ATTACH:
    if (pid == getpid()) { /* can't ptrace yourself */
      errno = EINVAL;
      ret = -1;
      goto ptrace_return;
    }
    if (sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data) != 0) {
      errno = EPERM; /* XXX set errno's correctly */
      ret = -1;
    } else {
      errno = 0;
      ret = 0;
    }
    /* XXX - wakeup a program SIGSTOP'd in assert code */
    kill(pid, SIGCONT);
    goto ptrace_return;
  case PT_GETREGS:
    res = sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data);
    if ((int)res == -1) {
      errno = EPERM; /* XXX set errno's correctly */
      ret = -1;
      goto ptrace_return;
    } else {
      struct ptrace_shared_data *psd = share_if_not_shared(res, pid);
      /* psd must be on page boundary */
      if (!psd || (PGMASK & (u_int)psd) || psd->errcode != 0) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
	goto ptrace_return;
      }
      errno = 0;
      *(struct reg*)addr = psd->r; /* XXX - check for fault */
      ret = 0;
      goto ptrace_return;
    }
  case PT_SETREGS:
    {
      struct ptrace_shared_data *psd = share_if_not_shared(1, pid);
      /* psd must be on page boundary */
      if (!psd || (PGMASK & (u_int)psd) || psd->errcode != 0) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
	goto ptrace_return;
      }
      psd->r = *(struct reg*)addr; /* XXX - check for fault */
      res = sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data);
      if ((int)res == -1) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
      } else
	ret = 0;
      goto ptrace_return;
    }
  case PT_GETFPREGS:
    res = sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data);
    if ((int)res == -1) {
      errno = EPERM; /* XXX set errno's correctly */
      ret = -1;
      goto ptrace_return;
    } else {
      struct ptrace_shared_data *psd = share_if_not_shared(res, pid);
      /* psd must be on page boundary */
      if (!psd || (PGMASK & (u_int)psd) || psd->errcode != 0) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
	goto ptrace_return;
      }
      errno = 0;
      *(struct fpreg*)addr = psd->fr; /* XXX - check for fault */
      ret = 0;
      goto ptrace_return;
    }
  case PT_SETFPREGS:
    {
      struct ptrace_shared_data *psd = share_if_not_shared(1, pid);
      /* psd must be on page boundary */
      if (!psd || (PGMASK & (u_int)psd) || psd->errcode != 0) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
	goto ptrace_return;
      }
      psd->fr = *(struct fpreg*)addr; /* XXX - check for fault */
      res = sipcout(pid2envid(pid), IPC_PTRACE, request, (u_int)addr, data);
      if ((int)res == -1) {
	errno = EPERM; /* XXX set errno's correctly */
	ret = -1;
      } else
	ret = 0;
      goto ptrace_return;
    }
  }

  errno = EINVAL;
  ret = -1;
 ptrace_return:
  OSCALLEXIT(OSCALL_ptrace);
  return ret;
}

void _exos_ptrace_setup() {
  /* setup ipc handler for communicating with tracer */
  ipc_register(IPC_PTRACE, ptrace_ipc_handler);

  /* was our exec'er being ptrace'd */
  if (is_being_ptraced()) {
    register_ptrace_bp_handler();
    /* signal ptracer */
    PT_BREAKPOINT;
  }
}
