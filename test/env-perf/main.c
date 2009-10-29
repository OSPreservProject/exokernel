#include <xok/sys_ucall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <xok/mmu.h>
#include <exos/tick.h>
#include <exos/process.h>
#include <exos/vm-layout.h>
#include <exos/vm.h>

static unsigned b,e;

static void B() { b = ae_gettick (); }
static void E() { e = ae_gettick (); }

/******************************************************************************
 *
 * env ubenchmarks.
 */

void env_basic (int n) {
  int i;
  int envid;
  int r;

  B();
  for (i = 0; i < n; i++) {
    envid = sys_env_alloc (0, &r);
    assert (envid > 0);
    assert (sys_env_free (0, envid) >= 0);
  }
  E();
}

void env_clone (int n) {
#ifdef __SMP__
  int i;
  int envid;
  int r;

  B();
  for (i = 0; i < n; i++) {
    envid = sys_env_clone (0, &r);
    assert (envid > 0);
    assert (sys_env_free (0, envid) >= 0);
  }
  E();
#endif
}

void zero (int n) {
  int i;
  char page[4096];

  B();
  for (i = 0; i < n; i++) {
    bzero (page, 4096);
  }
  E();
}

#if 0
void alloc_pid (int n) {
  int i;
  int pid;

  B();
  for (i = 0; i < n; i++) {
    pid = AllocateFreePid (10);
    UnrefPid (pid);
  }
  E();
};
#endif

void vm_scan (int n) {
  int i;
  int pte;
  u_int va;

  B();
  for (i = 0; i < n; i++) {
    for (va = NBPG; va < UTOP; ) {
      if ((vpd[PDENO(va)] & (PG_P|PG_U)) != (PG_P|PG_U)) {
	va += NBPD;
	continue;
      }
      pte = vpt[va >> PGSHIFT];
      if ((pte >> PGSHIFT) > 0x10000) {
	continue;
      }
      va += NBPG;
    }
  }
  E();
}
      
void page_insert (int n) {
  int i;
  int envid;
  int r;
  u_int num_completed;
#define BATCH 128
  Pte ptes[BATCH];

  for (i = 0; i < BATCH; i++) {
    ptes[i] = PG_U|PG_P|PG_W;
  }

  B();
  for (i = 0; i < n; i++) {
    envid = sys_env_alloc (0, &r);
    assert (envid > 0);
    num_completed = 0;
    assert (_exos_insert_pte_range (0, ptes, BATCH, 0, &num_completed, 0,
				    envid, 0, NULL) >= 0);
    assert (sys_env_free (0, envid) >= 0);
  }
  E();
}
	
void wrt_uenv (int n) {
  int i;
  struct Uenv a;
  int envid;
  int r;

  assert ((envid = sys_env_alloc (0, &r)) >= 0);
  assert (sys_rdu (0, envid, &a) >= 0);

  B();
  for (i = 0; i < n; i++) {
    assert (sys_wru (0, envid, &a) >= 0);
  }
  E();
}

/******************************************************************************
 *
 * Driver.
 */

	/* test vector */
static struct { 
  char str[64];  
  int n; 
  void (*fptr)(int); 
} tv[] = 
  {
  {"create/destroy",           10000,        env_basic },
  {"clone/destroy",	       10000,	     env_clone },
  {"bzero",                    10000,         zero },
#if 0
  {"alloc/free pid",           1000,          alloc_pid },
#endif
  {"vm scan",                  1000,           vm_scan },
  {"insert 128 pages",         1000,          page_insert },
  {"write uenv",               10000,         wrt_uenv },
  };

int main() { 
	int 	i;
	printf ("rate = %d\n", ae_getrate ());

	printf ("\nTimes are in micro-seconds per operation\n");

	for(i = 0; i < sizeof tv / sizeof tv[0]; i++) {
	     tv[i].fptr(tv[i].n);
	     printf ("%s:%d\n", tv[i].str, ((e-b+1)*ae_getrate ()/tv[i].n));
	}

	printf ("\nFinished\n");

	return 0;
}
