
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
#include <exos/cap.h>
#include <exos/callcount.h>
#include <exos/ipc.h>
#include <exos/ipcprint.h>
#include <exos/process.h>
#include <exos/oscallstr.h>
#include <exos/vm.h>
#include <exos/mallocs.h>
#include <exos/vm-layout.h>
#include <stdio.h>
#include <string.h>
#include <xok/env.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <exos/conf.h>
#ifdef PROCD
#include "procd.h"
#endif

#if defined(YIELD_COUNT) || defined(KERN_CALL_COUNT) || \
    defined(SYS_CALL_COUNT) || defined(EPILOGUE_COUNT) || \
    defined(EPILOGUE_ABORT_COUNT)
static u_int find_ipcprint() {
#if defined(PROCESS_TABLE) || defined(PROCD)
  int i;
  static struct Uenv cu;
  /* look for ipc printer */
  for (i = 0; i < PID_MAX; i++) {
    if (pid2envid(i) == 0) {
      continue;
    }
    if (sys_rdu(CAP_ROOT, pid2envid(i), &cu) < 0) {
      continue;
    }
    if (!strstr(cu.name, "ipcprint")) continue;
    return pid2envid(i);
  }
#endif

  return 0;
}
#endif

static u_int COUNT_TEMP_REGION=0;

#ifdef YIELD_COUNT
static void yield_count_results() {
  u_int eid;

  eid = find_ipcprint();
  if (!eid) return;

  _exos_self_insert_pte(0, PG_P | PG_U | PG_W, COUNT_TEMP_REGION, ESIP_DONTPAGE);
  bcopy(&UAREA.u_yield_count, (char*)COUNT_TEMP_REGION, sizeof(UAREA.u_yield_count));
  while (sipcout(eid, IPC_PRINT, IPRINT_YIELD_CALLS,
		 PGNO(vpt[PGNO(COUNT_TEMP_REGION)]), 0) == -2)
    yield(eid);
  _exos_self_unmap_page(0, COUNT_TEMP_REGION);
}

static int yield_count_fork(u_int k, int envid, int NewPid) {
  struct Uenv cu;

  sys_rdu(0, envid, &cu);
  cu.u_yield_count = 0;
  sys_wru(0, envid, &cu);

  return 0;
}

static int yield_count_exec(u_int k, int envid, int execonly) {
  struct Uenv cu;

  if (execonly) yield_count_results();
  sys_rdu(0, envid, &cu);
  cu.u_yield_count = 0;
  sys_wru(0, envid, &cu);

  return 0;
}

static void yield_count_init() {
  OnFork(yield_count_fork);
  OnExec(yield_count_exec);
}
#endif

#ifdef KERN_CALL_COUNT
unsigned int *__kern_call_counts = 0;

static void kern_call_count_results() {
  u_int eid;

  eid = find_ipcprint();
  if (!eid) return;

  assert(_exos_self_insert_pte(0, PG_P | PG_U | PG_W, COUNT_TEMP_REGION,
			       ESIP_DONTPAGE) == 0);
  bcopy(__kern_call_counts, (char*)COUNT_TEMP_REGION,
	256*sizeof(unsigned int));
  while (sipcout(eid, IPC_PRINT, IPRINT_KERN_CALLS,
		 PGNO(vpt[PGNO(COUNT_TEMP_REGION)]), 0) == -2)
    yield(eid);
  assert(_exos_self_unmap_page(0, COUNT_TEMP_REGION) == 0);
}

static int kern_call_count_fork(u_int k, int envid, int NewPid) {
  /* zero the child's counts */
  assert(_exos_self_insert_pte(0, PG_P|PG_U|PG_W|PG_SHARED,
			       COUNT_TEMP_REGION, ESIP_DONTPAGE) == 0);
  bzero((void*)COUNT_TEMP_REGION, NBPG);
  assert(_exos_insert_pte(0, vpt[PGNO(COUNT_TEMP_REGION)],
			  (u_int)__kern_call_counts, 0, envid,
			  ESIP_DONTPAGE) == 0);
  assert(_exos_self_unmap_page(0, COUNT_TEMP_REGION) == 0);

  return 0;
}

static int kern_call_count_exec(u_int k, int envid, int execonly) {
  if (execonly) kern_call_count_results();

  return 0;
}

static void kern_call_count_init() {
  u_int va = (u_int)exos_pinned_malloc(NBPG);

  __vm_alloc_region(va, NBPG, 0, PG_P|PG_U|PG_W|PG_SHARED);
  bzero((void*)va, NBPG);
  __kern_call_counts = (u_int*)va;
  UAREA.u_start_kern_call_counting = 1;
  OnFork(kern_call_count_fork);
  OnExec(kern_call_count_exec);
}
#endif

#ifdef SYS_CALL_COUNT
static int __in_oscall = 0;
struct os_call_count_struct {
  u_int depth1;
  u_int total;
};
 /* number of each os call made at depth 1 and higher */
static struct os_call_count_struct sys_call_counts[256];

#ifdef KERN_CALL_COUNT
/* 2 megs of data! */
/* kern call vs os call matrix */
static struct os_call_count_struct sys_call_count_matrix[256][256];
static u_int sys_call_kern_temp[256]; /* record kern_call_count at beginning */
                                      /* of os call */
static u_int sys_call_kern_temp2[256]; /* one for depth 1, one for total */
#endif

static void touch_pages(u_int start_va, u_int end_va, int write) {
  u_int i;
  char c;

  if (write)
    for (i = start_va; i < PGROUNDUP(end_va); i += NBPG)
      *(char*)i = *(char*)i;
  else
    for (i = start_va; i < end_va; i += NBPG)
      c = *(char*)i;
}

static void child_zero(u_int start_va, u_int end_va, u_int envid) {
  u_int i;

  for (i = start_va; i < end_va; ) {
    Pte pte;

    pte = sys_read_pte(i, 0, envid);
    pte |= PG_W;
    pte &= ~PG_COW;
    pte &= ~PG_RO;
    assert(_exos_self_insert_pte(0, pte, COUNT_TEMP_REGION,
				 ESIP_DONTPAGE) == 0);
    bzero((void*)(COUNT_TEMP_REGION + (i%NBPG)), NBPG - (i%NBPG) -
	  ((PGNO(i) != PGNO(end_va)) ? 0 : (NBPG - (end_va%NBPG))));
    i += NBPG;
    i = PGROUNDDOWN(i);
  }
  assert(_exos_self_unmap_page(0, COUNT_TEMP_REGION) == 0);
}

static int sys_call_count_fork(u_int k, int envid, int NewPid) {
  /* zero the child's counts */
  touch_pages((u_int)sys_call_counts,
	      (u_int)sys_call_counts + 256 * sizeof(sys_call_counts[0]), 1);
  child_zero((u_int)sys_call_counts,
	     (u_int)sys_call_counts + 256 * sizeof(sys_call_counts[0]), envid);
#ifdef KERN_CALL_COUNT
  touch_pages((u_int)sys_call_count_matrix,
	      (u_int)sys_call_count_matrix +
	      256 * 256 * sizeof(sys_call_count_matrix[0][0]), 1);
  child_zero((u_int)sys_call_count_matrix,
	     (u_int)sys_call_count_matrix +
	     256 * 256 * sizeof(sys_call_count_matrix[0][0]), envid);
#endif

  return 0;
}

static void sys_call_count_results();

static int sys_call_count_exec(u_int k, int envid, int execonly) {
  if (execonly) {
    OSCALLEXIT(OSCALL_execve);
    sys_call_count_results();
  }

  return 0;
}

static void sys_call_count_init() {
  OnFork(sys_call_count_fork);
  OnExec(sys_call_count_exec);
}

#define MAXOSCALLDEPTH 20
static int prevcalls[MAXOSCALLDEPTH];
void __oscallenter(int c) {
  if (__in_oscall < MAXOSCALLDEPTH)
    prevcalls[__in_oscall] = c;
  else
    assert(__in_oscall < MAXOSCALLDEPTH); /* Increase MAXOSCALLDEPTH */
  sys_call_counts[c].total++;
  if (!__in_oscall) {
    sys_call_counts[c].depth1++;
#ifdef KERN_CALL_COUNT
    if (UAREA.u_start_kern_call_counting) {
      bcopy(__kern_call_counts, sys_call_kern_temp, 256*sizeof(u_int));
      bcopy(__kern_call_counts, sys_call_kern_temp2, 256*sizeof(u_int));
    } else {
      bzero(sys_call_kern_temp, 256*sizeof(u_int));
      bzero(sys_call_kern_temp2, 256*sizeof(u_int));
    }
#endif
  }
#ifdef KERN_CALL_COUNT
  else if (UAREA.u_start_kern_call_counting) {
    int i;
    
    for (i=0; i < 256; i++)
      sys_call_count_matrix[i][prevcalls[__in_oscall-1]].total +=
	__kern_call_counts[i] - sys_call_kern_temp[i];
    bcopy(__kern_call_counts, sys_call_kern_temp, 256*sizeof(u_int));
  }
#endif
  __in_oscall++;
}

void __oscallexit(int c) {
  __in_oscall--;
  if (__in_oscall < 0) {
    kprintf("__oscallexit called too many times. Last argument: %s\n",
	    oscallstr(c));
    assert(0);
  }
  if (__in_oscall < MAXOSCALLDEPTH && prevcalls[__in_oscall] != c) {
    int i;

    for (i=0; i <= __in_oscall; i++)
      kprintf("[%s]", oscallstr(prevcalls[i]));
    assert(0);
  }
#ifdef KERN_CALL_COUNT
  if (UAREA.u_start_kern_call_counting) {
    int i;

    for (i=0; i < 256; i++)
      sys_call_count_matrix[i][c].total += __kern_call_counts[i] -
	sys_call_kern_temp[i];
    bcopy(__kern_call_counts, sys_call_kern_temp, 256*sizeof(u_int));
    if (!__in_oscall) {
      for (i=0; i < 256; i++)
	sys_call_count_matrix[i][c].depth1 += __kern_call_counts[i] -
	  sys_call_kern_temp2[i];
    }
  }
#endif
}

static void sys_call_count_results() {
  u_int eid;

  eid = find_ipcprint();
  if (!eid) return;

#ifdef KERN_CALL_COUNT
  touch_pages((u_int)sys_call_count_matrix,
	      (u_int)sys_call_count_matrix +
	      256 * 256 * sizeof(sys_call_count_matrix[0][0]), 1);
  while (sipcout(eid, IPC_PRINT, IPRINT_SYS_CALLS, (u_int)sys_call_counts, 
		 (u_int)sys_call_count_matrix) == -2)
    yield(eid);
#else
  touch_pages((u_int)sys_call_counts,
	      (u_int)sys_call_counts + 256 * sizeof(sys_call_counts[0]), 1);
  while (sipcout(eid, IPC_PRINT, IPRINT_SYS_CALLS, (u_int)sys_call_counts, 0)
	 == -2)
    yield(eid);
#endif
}
#endif

#ifdef EPILOGUE_COUNT
static void epilogue_count_results() {
  u_int eid;

  eid = find_ipcprint();
  if (!eid) return;

  _exos_self_insert_pte(0, PG_P | PG_U | PG_W, COUNT_TEMP_REGION,
			ESIP_DONTPAGE);
  bcopy(&UAREA.u_epilogue_count, (char*)COUNT_TEMP_REGION,
	sizeof(UAREA.u_epilogue_count));
  while (sipcout(eid, IPC_PRINT, IPRINT_EPILOGUE_CALLS,
		 PGNO(vpt[PGNO(COUNT_TEMP_REGION)]), 0) == -2)
    yield(eid);
  _exos_self_unmap_page(0, COUNT_TEMP_REGION);
}

static int epilogue_count_fork(u_int k, int envid, int NewPid) {
  struct Uenv cu;

  sys_rdu(0, envid, &cu);
  cu.u_epilogue_count = 0;
  sys_wru(0, envid, &cu);

  return 0;
}

static int epilogue_count_exec(u_int k, int envid, int execonly) {
  struct Uenv cu;

  if (execonly) epilogue_count_results();
  sys_rdu(0, envid, &cu);
  cu.u_epilogue_count = 0;
  sys_wru(0, envid, &cu);

  return 0;
}

static void epilogue_count_init() {
  OnFork(epilogue_count_fork);
  OnExec(epilogue_count_exec);
}
#endif

#ifdef EPILOGUE_ABORT_COUNT
static void epilogue_abort_count_results() {
  u_int eid;

  eid = find_ipcprint();
  if (!eid) return;

  _exos_self_insert_pte(0, PG_P | PG_U | PG_W, COUNT_TEMP_REGION,
			ESIP_DONTPAGE);
  bcopy(&UAREA.u_epilogue_abort_count, (char*)COUNT_TEMP_REGION,
	sizeof(UAREA.u_epilogue_abort_count));
  while (sipcout(eid, IPC_PRINT, IPRINT_EPILOGUE_ABORT_CALLS,
		 PGNO(vpt[PGNO(COUNT_TEMP_REGION)]), 0) == -2)
    yield(eid);
  _exos_self_unmap_page(0, COUNT_TEMP_REGION);
}

static int epilogue_abort_count_fork(u_int k, int envid, int NewPid) {
  struct Uenv cu;

  sys_rdu(0, envid, &cu);
  cu.u_epilogue_abort_count = 0;
  sys_wru(0, envid, &cu);

  return 0;
}

static int epilogue_abort_count_exec(u_int k, int envid, int execonly) {
  struct Uenv cu;

  if (execonly) epilogue_abort_count_results();
  sys_rdu(0, envid, &cu);
  cu.u_epilogue_abort_count = 0;
  sys_wru(0, envid, &cu);

  return 0;
}

static void epilogue_abort_count_init() {
  OnFork(epilogue_abort_count_fork);
  OnExec(epilogue_abort_count_exec);
}
#endif

void __call_count_results() {
#ifdef KERN_CALL_COUNT
  kern_call_count_results();
#endif
#ifdef SYS_CALL_COUNT
  sys_call_count_results();
#endif
#ifdef EPILOGUE_COUNT
  epilogue_count_results();
#endif
#ifdef EPILOGUE_ABORT_COUNT
  epilogue_abort_count_results();
#endif
#ifdef YIELD_COUNT
  yield_count_results();
#endif
}

void __call_count_init() {
  COUNT_TEMP_REGION = (u_int)exos_pinned_malloc(NBPG);
#ifdef EPILOGUE_COUNT
  epilogue_count_init();
#endif
#ifdef EPILOGUE_ABORT_COUNT
  epilogue_abort_count_init();
#endif
#ifdef YIELD_COUNT
  yield_count_init();
#endif
#ifdef KERN_CALL_COUNT
  kern_call_count_init();
#endif
#ifdef SYS_CALL_COUNT
  sys_call_count_init();
#endif
}
