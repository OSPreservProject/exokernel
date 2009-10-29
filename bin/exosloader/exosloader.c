
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

/* The loader expects the parent to pre-load all the text and data pages
   and zero its bss.

   The loader will demand load the child's text and data and demand zero
   the child's bss.  The child is in charge of all other faults, including
   stack, mmap, and malloc/brk faults. (The loader will handle COW faults
   for the data segment.)

   The parent also passes the envid of the sls and an fd of the child to be
   used for reading the text and data of the child.  These will be under the
   ps_strings structure just below USRSTACK.
*/

#include <exos/cap.h>
#include <exos/ipcdemux.h>
#include <exos/osdecl.h>
#include <exos/process.h>
#include <exos/sls.h>
#include <exos/ubc.h>
#include <exos/vm.h>
#include <machine/vmparam.h>
#include <string.h>
#include <sys/exec.h>
#include <xok/env.h>
#include <xok/kqueue.h>
// #include <xok/pmap.h>
#include <xok/sys_ucall.h>

static void loader(void);

/* two pages of private stack for the loader */
static char loader_stack[2*NBPG];

/* extra space */
static char loader_space[2*NBPG];
static u_int temp_page;

/* local copy of child'd exec header */
static struct exec hdr;

/* file descriptor used with sls */
static int sfd;

/* useful values rounded to pages */
static u_int text_start_va;
static u_int data_start_va;
/* useful values not rounded to pages */
static u_int bss_start_va;
static u_int end_start_va;

/* original esp */
u_int orig_esp;

static void inline EnterCritical () {
  asm ("cmpl $0, %1\n"		/* don't init if we're a nested EnterCritical */
       "\tjg 1f\n"
       "\tmovl $0, %0\n"	/* clear the interrupted flag */
       "\tcall 0f\n"	        /* save eip on first enter, using call to push eip on stack */
       "0:\n"
       "\tpopl %2\n"
       "1:\n"
       "\tincl %1\n"		/* inc count of nested of EnterCriticals */
       : "=m" (UAREA.u_interrupted), "=m" (UAREA.u_in_critical), "=m" (UAREA.u_critical_eip) :: "cc", "memory");
}

static void inline ExitCritical () {
  asm ("decl %0\n"		/* dec critical ref count */
       "\tcmpl $0, %0\n"	/* if > 0 mean we're still in a critical pair */
       "\tjg 0f\n"
       "\tcmpl $0, %1\n"	/* we're the last exit critical so yield if needed */
       "\tje 0f\n"		/* not interrupted--no need to yield */
       "\tpushl %2\n"		/* setup our stack to look like an upcall */
       "\tpushfl\n"
       "\tjmp _xue_yield\n"	/* and go to yield */
       "0:\n"
       : "=m" (UAREA.u_in_critical): "m" (UAREA.u_interrupted), "g" (&&next)
       : "cc", "memory");
next:
}

struct bc_entry *__bc_lookup64 (u32 d, u_quad_t b) {
  struct bc_entry *buf;

  EnterCritical ();
  buf = KLIST_UPTR (&__bc.bc_qs[__bc_hash64 (d, b)].lh_first, UBC);
  while (buf) {
    if (buf->buf_dev == d && buf->buf_blk64 == b) {
      ExitCritical ();
      return (buf);
    }
    buf = KLIST_UPTR (&buf->buf_link.le_next, UBC);
  }

  ExitCritical ();
  return (NULL);
}

/* sls vcopy destination */
static struct vc_info {
  struct {
    int fd;
    u_quad_t blk;
  } to;
  struct {
    int dev;
    u_quad_t blk;
  } from;
} vcopy_info;

/* XXX */
#define _exos_ipc_off()
#define _exos_ipc_on()
struct _exos_exec_args {
  u_int sls_envid;
  int sls_fd;
  struct ps_strings pss;
};
struct _exos_exec_args *_EXOS_EXEC_ARGS;

/* called from entry.S */
void loader_start(u_int esp) {
  UAREA.u_entipc1 = UAREA.u_entipc2 = 0;
  UAREA.u_xsp = 0;
  UAREA.u_entfault = (u_int) xue_fault;
  UAREA.u_entprologue = (u_int) xue_prologue;
  UAREA.u_entepilogue = (u_int) xue_epilogue;
  UAREA.u_fpu = 0;

  /* keep track of original stack */
  orig_esp = esp;

  /* use loader stack */
  asm("movl %0, %%esp" : : "g" (loader_stack+sizeof(loader_stack)));

  /* if quantum ran out since started, then yield now */
  if (UAREA.u_ppc) yield(-1);

  loader();
}

static u_int slseid = 0;
static int
S_READINSERT(u_long vcopyinfo) {
  int ret, ipcret;

  ret = _ipcout_default(slseid, 5, &ipcret, IPC_SLS, SLS_READINSERT, vcopyinfo,
			0, 0);
  if (ret != 0) {
    sys_argtest(ret, slseid, sfd, (int)_EXOS_EXEC_ARGS, sizeof(*_EXOS_EXEC_ARGS), 5);
    while(1);
  }
  return ipcret;
}

static int
S_NEWPAGE(u_int vpte) {
  int ret, ipcret;

  ret = _ipcout_default(slseid, 5, &ipcret, IPC_SLS, SLS_NEWPAGE, vpte, 0, 0);

  if (ret != 0) {
    sys_argtest(ret, slseid, sfd, (int)_EXOS_EXEC_ARGS, sizeof(*_EXOS_EXEC_ARGS), 5);
    while(1);
  }
  return ipcret;
}

/* "main" procedure of the loader */
static void loader() {
  struct bc_entry *bce = NULL;
  int ret;

  DEF_SYM (UAREA, UADDR);
  DEF_SYM (__u_status, &UAREA.u_status);
  DEF_SYM (__u_entipc2, &UAREA.u_entipc2);
  DEF_SYM (__u_entfault2, &UAREA.u_entfault2);
  DEF_SYM (__u_xesp, &UAREA.u_xsp);
  DEF_SYM (__u_ppc, &UAREA.u_ppc);
  DEF_SYM (__u_donate, &UAREA.u_donate);
  DEF_SYM (__u_yield_count, &UAREA.u_yield_count);
  DEF_SYM (__u_epilogue_count, &UAREA.u_epilogue_count);
  DEF_SYM (__u_epilogue_abort_count, &UAREA.u_epilogue_abort_count);
  DEF_SYM (__u_ptrace_flags, &UAREA.u_ptrace_flags);
  DEF_SYM (__envs, UENVS);
  DEF_SYM (__ppages, UPPAGES);
  DEF_SYM (__sysinfo, SYSINFO);
  DEF_SYM (__u_in_critical, &UAREA.u_in_critical);
  DEF_SYM (__u_interrupted, &UAREA.u_interrupted);
  DEF_SYM (__u_fpu, &UAREA.u_fpu);
  DEF_SYM (__u_next_timeout, &UAREA.u_next_timeout);
  DEF_SYM (__si_system_ticks, &__sysinfo.si_system_ticks);
  DEF_SYM (__bc, UBC);
  DEF_SYM (__xr, UXN);
  DEF_SYM (__xn_free_map, UXNMAP);
  DEF_SYM (__u_revoked_pages, &UAREA.u_revoked_pages);

  DEF_SYM (vpt, UVPT);
#define SRL(val, shamt) (((val) >> (shamt)) & ~(-1 << (32 - (shamt))))
  DEF_SYM (vpd, UVPT + SRL(UVPT, 10));

  /* init some vars */
  temp_page = PGROUNDUP((u_int)loader_space);
  _EXOS_EXEC_ARGS =
    (struct _exos_exec_args*)(USRSTACK - sizeof(*_EXOS_EXEC_ARGS));

  /* record the sls envid and fd locally */
  slseid = _EXOS_EXEC_ARGS->sls_envid;
  sfd = _EXOS_EXEC_ARGS->sls_fd;

  /* get exec header info */
  /* have first page mapped by sls */
  vcopy_info.to.fd = sfd;
  vcopy_info.to.blk = 0;

  do {
    /* get dev/blkno from sls */
    if ((ret = S_READINSERT((u_long)&vcopy_info)) != 0) while(1);

    EnterCritical();
    if ((bce = __bc_lookup64(vcopy_info.from.dev, vcopy_info.from.blk))) {
      struct Xn_name *xn;
      struct Xn_name xn_nfs;
      if (bce->buf_dev > MAX_DISKS) {
	xn_nfs.xa_dev = bce->buf_dev;
	xn_nfs.xa_name = 0;
	xn = &xn_nfs;
      } else {
	xn = &__sysinfo.si_pxn[bce->buf_dev];
      }

      /* map the page */
      if (sys_self_bc_buffer_map(xn, CAP_ROOT,
				 ppnf2pte(bce->buf_pp, PG_P|PG_U),
				 temp_page) < 0) {
	while(1);
	bce = NULL; /* XXX */
      }
    } else while (1);
    ExitCritical();
  } while (ret != 0 || bce == NULL);

  /* copy the header */
  hdr = *(struct exec*)temp_page;
  /* unmap temp location */
  if (sys_self_insert_pte(CAP_ROOT, 0, temp_page) == -1) {
    /* XXX */
sys_cputs("[7]");
  }
  sys_argtest(hdr.a_text, hdr.a_data, hdr.a_bss, hdr.a_midmag, 77, 88);

  /* init variables */
  text_start_va = UTEXT;
  data_start_va = PGROUNDDOWN(text_start_va + sizeof(hdr) + hdr.a_text);
  bss_start_va = text_start_va + sizeof(hdr) + hdr.a_text + hdr.a_data;
  end_start_va = bss_start_va + hdr.a_bss;

sys_cputs("<F>");
  /* resume original stack and start running the child */
 asm("movl %0, %%esp\n"
     "movl %1, %%eax\n"
     "jmpl %%eax" : : "g" (orig_esp), "i" (UTEXT+0x20));
}

/* called from entry.S - no ipcing into process while handling this */
int
page_fault_handler (u_int va, u_int errcode, u_int eflags, u_int eip,
		    u_int esp) {
  int r = -1, ret;
  u_int pteflags = 0;
  struct bc_entry *bce;
static lastva = 0;

sys_cputs(".");
  _exos_ipc_off();
  /* out of range */
  if (va < text_start_va || va >= end_start_va) {
    /* XXX */
sys_argtest(va, eip, esp, 1, 1, 1);
if (va == lastva) while (1);
    r = -2;
    goto done;
  }

  /* the following faults are handled:
     read from text
     cow/read/write to data
     read/write to bss
  */
  if (va < data_start_va) {
    /* if reading from a missing text page */
    if ((errcode & (FEC_WR|FEC_PR|FEC_U)) == FEC_U)
      pteflags = PG_U|PG_P;
  }
  else if (va < PGROUNDUP(bss_start_va)) {
    /* if data page cow */
    if ((errcode & (FEC_WR|FEC_PR|FEC_U)) == (FEC_WR|FEC_PR|FEC_U) &&
	(vpt[PGNO(va)] & PG_COW) == PG_COW) {
DOCOW:
      pteflags = PG_U|PG_P|PG_W;
      /* have a page mapped by sls */
      if (S_NEWPAGE(PGROUNDDOWN(temp_page) | pteflags) != 0) {
	/* XXX */
	r = -1;
	goto done;
      }
      /* copy the page */
      bcopy((void*)PGROUNDDOWN(va), (void*)temp_page, NBPG);
      /* remap the page and unmap temp location */
      if (sys_self_insert_pte(0, vpt[PGNO(temp_page)],
			      PGROUNDDOWN(va)) == -1 ||
	  sys_self_insert_pte(0, 0, temp_page) == -1) {
	/* XXX */
	r = -1;
	goto done;
      }
      /* zero part of page if data/bss boudnary page */
      if (PGNO(va) == PGNO(bss_start_va))
	bzero((void*)bss_start_va, PGROUNDUP(bss_start_va) - bss_start_va);

      r = 0;
      goto done;
    }
    /* if data page write */
    else if ((errcode & (FEC_WR|FEC_PR|FEC_U)) == (FEC_U|FEC_WR))
      pteflags = PG_U|PG_P;
    /* if data page read */
    else if ((errcode & (FEC_WR|FEC_PR|FEC_U)) == FEC_U)
      pteflags = PG_U|PG_P|PG_COW;
  }
  else {
    /* bss read or write */
    if ((errcode & (FEC_PR|FEC_U)) == FEC_U) {
      pteflags = PG_U|PG_P|PG_W;
      /* have a page mapped by sls */
      if (S_NEWPAGE(PGROUNDDOWN(va) | pteflags) != 0) {
	/* XXX */
	r = -1;
	goto done;
      }
      bzero((void*)PGROUNDDOWN(va), NBPG);
      r = 0;
      goto done;
    }
  }

  vcopy_info.to.fd = sfd;
  vcopy_info.to.blk = PGROUNDDOWN(va) - PGROUNDDOWN(text_start_va);

  do {
    /* get dev/blkno from sls */
    if ((ret = S_READINSERT((u_long)&vcopy_info)) != 0) while(1);

    EnterCritical();
    if ((bce = __bc_lookup64(vcopy_info.from.dev, vcopy_info.from.blk))) {
      struct Xn_name *xn;
      struct Xn_name xn_nfs;

      if (bce->buf_dev > MAX_DISKS) {
	xn_nfs.xa_dev = bce->buf_dev;
	xn_nfs.xa_name = 0;
	xn = &xn_nfs;
      } else {
	xn = &__sysinfo.si_pxn[bce->buf_dev];
      }

      /* map the page */
      if (sys_self_bc_buffer_map(xn, CAP_ROOT,
				 ppnf2pte(bce->buf_pp, pteflags),
				 PGROUNDDOWN(va)) < 0) {
	bce = NULL; /* XXX */
	while(1);
      }
    }
    ExitCritical();
  } while (ret != 0 || bce == NULL);

  /* if writing to data page or read the bss/data boundary, then copy it */
  if ((errcode & (FEC_WR|FEC_PR|FEC_U)) == (FEC_U|FEC_WR) ||
      PGNO(va) == PGNO(bss_start_va))
    goto DOCOW;

  r = 0;

done:
  _exos_ipc_on();
lastva = va;
  return r;
}
