
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

#include <exos/vm.h>
#include <assert.h>
#include <exos/critical.h>
#include <exos/ipcdemux.h>
#include <exos/ipcprint.h>
#include <exos/mallocs.h>
#include <exos/oscallstr.h>
#include <exos/process.h>
#include <exos/vm-layout.h>
#include <signal.h>
#include <stdio.h>
#include <xok/env.h>
#include <xok/kerrno.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <xok/kerncallstr.h>

u_int temp_pages;

int handler(int code, int code2, int arg1, int arg2, u_int caller) {
  static int in_handler = 0;
  int ret;
  struct Uenv cu;
  u_int ppn = arg1;

  EnterCritical();
  if (in_handler) {
    ExitCritical();
    return -2;
  }
  in_handler = 1;
  ExitCritical();
  /* print caller's name */
  sys_rdu(0, caller, &cu);
  printf("\nProgram: %s, PID: %d\n", cu.name, envid2pid(caller));
  switch (code2) {
  case IPRINT_KERN_CALLS:
    if ((ret = _exos_self_insert_pte(0, (ppn << PGSHIFT) | PG_P | PG_U,
				     temp_pages, 0, NULL)) < 0) {
      printf("error handling kern call print: %d\n", ret);
      break;
    }
    {
      u_int i, sum=0, *calls;
      calls = (u_int*)temp_pages;

      for (i=0; i < 256; i++)
        sum += calls[i];
      printf("Total kern calls: %d\n", sum);
      for (i=0; i < 256; i++)
        if (calls[i]) {
	  char *s = kerncallstr(i);
          printf("%30s: %8d\n", s ? s : "UNKNOWN",
                 calls[i]);
	}
    }
    if ((ret = _exos_self_unmap_page(0, temp_pages)) < 0) {
      printf("error handling kern call print: %d\n", ret);
      break;
    }
    break;

  case IPRINT_SYS_CALLS:
    /* first print out totals */
    {
      struct os_call_count_struct {
	u_int depth1;
	u_int total;
      };
      struct os_call_count_struct *scc;
      struct os_call_count_struct (*kcs)[256][256] = NULL;
      u_int va = arg1, kc = arg2;
      Pte pte1, pte2;
      u_int i;
      int r;

      /* map in oscall data */
      pte1 = (sys_read_pte(va, 0, caller, &r) & ~(PG_COW | PG_RO)) | PG_W;
      pte2 = (sys_read_pte(va+NBPG, 0, caller, &r) & ~(PG_COW | PG_RO)) | PG_W;
      assert(_exos_self_insert_pte(0, pte1, temp_pages, 0, NULL) == 0);
      assert(_exos_self_insert_pte(0, pte2, temp_pages+NBPG, 0, NULL) == 0);
      scc = (void*)((((u_int)va) % NBPG) + temp_pages);
      if (kc) {
	/* map in kern vs oscall data */
	for (i=0; i < 256*256*sizeof(struct os_call_count_struct) + NBPG;
             i+=NBPG) {
	  pte1 = (sys_read_pte(kc + i, 0, caller, &r) & ~(PG_COW | PG_RO)) | PG_W;
	  assert(_exos_self_insert_pte(0, pte1, temp_pages + 2*NBPG + i, 0, NULL)
		 == 0);
	}
	kcs = (void*)((((u_int)kc) % NBPG) + temp_pages + 2*NBPG);
      }
      printf("\tFirst number = # of times called while not in another oscall\n"
	     "\tIn parentheses = Total # of times called\n");
      if (kc)
	printf("\tBefore slash = total kern calls during os call & sub os "
	       "calls from\n\t\tprogram only\n"
	       "\tAfter slash = kern calls not including sub os calls whether "
	       "called from\n\t\tprogram or other os call\n");
      for (i=0; i < 256; i++) {
        if (scc[i].total) {
	  char *s = oscallstr(i);
	  printf("%15s: %8d(%d)\n", s ? s : "UNKNOWN", scc[i].depth1,
		 scc[i].total);
	  if (kc) {
	    int j;

	    for (j=0; j < 256; j++)
	      if ((*kcs)[j][i].total) {
		char *s2 = kerncallstr(j);
		printf("%32d/%-8d: %s\n", (*kcs)[j][i].depth1,
		       (*kcs)[j][i].total, s2 ? s2 : "UNKNOWN");
	      }
	  }
	}
      }
      /* unmap oscall data */
      assert(_exos_self_unmap_page(0, temp_pages) == 0);
      assert(_exos_self_unmap_page(0, temp_pages+NBPG) == 0);
      if (kc)
	/* unmap kern vs oscall data */
	for (i=0; i < 256*256*sizeof(struct os_call_count_struct); i+=NBPG)
	  assert(_exos_self_unmap_page(0, temp_pages + 2*NBPG + i) == 0);
    }
  break;

  case IPRINT_YIELD_CALLS:
    if ((ret = _exos_self_insert_pte(0, (ppn << PGSHIFT) | PG_P | PG_U,
				     temp_pages, 0, NULL)) < 0) {
      printf("error handling yield call print: %d\n", ret);
      break;
    }
    printf("Total yield calls: %d\n", *(u_int*)temp_pages);
    if ((ret = _exos_self_unmap_page(0, temp_pages)) < 0) {
      printf("error handling yield call print: %d\n", ret);
      break;
    }
    break;

  case IPRINT_EPILOGUE_CALLS:
    if ((ret = _exos_self_insert_pte(0, (ppn << PGSHIFT) | PG_P | PG_U,
				     temp_pages, 0, NULL)) < 0) {
      printf("error handling epilogue call print: %d\n", ret);
      break;
    }
    printf("Total epilogues: %d\n", *(u_int*)temp_pages);
    if ((ret = _exos_self_unmap_page(0, temp_pages)) < 0) {
      printf("error handling epilogue call print: %d\n", ret);
      break;
    }
    break;

  case IPRINT_EPILOGUE_ABORT_CALLS:
    if ((ret = _exos_self_insert_pte(0, (ppn << PGSHIFT) | PG_P | PG_U,
				     temp_pages, 0, NULL)) < 0) {
      printf("error handling epilogue abort call print: %d\n", ret);
      break;
    }
    printf("Total epilogue aborts/delays: %d\n", *(u_int*)temp_pages);
    if ((ret = _exos_self_unmap_page(0, temp_pages)) < 0) {
      printf("error handling epilogue abort call print: %d\n", ret);
      break;
    }
    break;
  }

  in_handler = 0;
  return 0;
}

int main() {
  temp_pages = (u_int)__malloc(200*NBPG);
  if (temp_pages == 0) return -1;

  ipc_register(IPC_PRINT, handler);

  UAREA.u_status = U_SLEEP;
  while (1)
    yield(-1);

  __free((void*)temp_pages);
  return 0;
}
