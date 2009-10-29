#include <sys/types.h>
#include <xok/mmu.h>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <exos/tick.h>

static unsigned b,e;

static void B() { b = ae_gettick (); }
static void E() { e = ae_gettick (); }

#define PAGESIZ 4096

/******************************************************************************
 *
 * virtual memory ubenchmarks.
 */

#define AP        (PROT_READ|PROT_WRITE)
#define PAGES 100
#define TRAPS 5000

static int              pagesize;
static char             *mem, *please_protect, *please_unprotect;
static int              perm[PAGES],  count = 0;

static void prot_init(void) {
	int i,j,k;
       	pagesize = getpagesize();
        /* n = (((PAGES + 1) * pagesize) + pagesize - 1) & ~(pagesize - 1); */
        mem = (char *) malloc((PAGES+2)*pagesize);
	mem = (char *)(((int)mem + pagesize ) & ~(pagesize-1));

	demand(((unsigned)mem%PAGESIZ) == 0, unaligned);
        for (i = 0; i < PAGES; i++)
                mem[i * pagesize] = 20;
        for (i = 1; i < PAGES; i++)
                perm[i] = i;
	for (i = 1; i < PAGES - 1; i++) {
                j = random() % (PAGES - i - 1);
                k = perm[j + i];
                perm[j + i] = perm[i];
                perm[i] = k;
        }
}

static void handler(int a) {
        count++;
         if (count >= 0) {
                if (please_protect)
                        if(mprotect(please_protect, pagesize, PROT_READ) < 0)
				assert(0);
                if(mprotect(please_unprotect, pagesize, PROT_WRITE) < 0)
			assert(0);
        }
}

#define PROT_GRAPH 1


/* prot1: time to protect a single, random, page.  */
static void prot1_test(int n) {
	int i;

	B();
        /* protect random pages */
	for(i=0; i < n; i++) {
                if(mprotect(&mem[perm[i%PAGES] * pagesize], pagesize, PROT_READ) < 0) {
printf ("errno = %d\n", errno);
                	demand(0,0);
		}
	   }
	E();
}

static void trap (int n) {

  if(signal(SIGSEGV, handler) < 0)
    assert(0);

  B();
  count = -n;
  if (mprotect (&mem[0], pagesize, PROT_READ) < 0) {
    printf ("errno = %d\n", errno);
  }
  please_protect = 0;
  please_unprotect = &mem[0];
  *(int volatile *)please_unprotect = 30;
  E();
}

#define UNPROT_GRAPH 2

/* uprot1: time to unprotect a single, random, page.  */
static void uprot1_test(int n) {
	int i;

	B();
        /* uprotect random pages */
	for(i=0; i < n; i++) {
                if(mprotect(&mem[perm[i%PAGES] * pagesize], pagesize, PROT_WRITE) < 0) {
		     printf ("mem = %p\n", &mem[perm[i%PAGES] * pagesize]);
		     printf ("pagesize = %d\n", pagesize);
		     printf ("failing on i = %d. errno = %d\n", i, errno);
                	demand(0,0);
		}
        }
	E();
}

/* uprot100: time to unprotect a single, random, page.  */
static void uprot100_test(int n) {
	int i;
	B();
        /* uprotect random pages */
	for(i=0; i < n; i++) {
                if(mprotect(mem, 100 * pagesize, PROT_WRITE) < 0)
                	demand(0,0);
        }
	E();

}

#define APPEL_GRAPH 3

/* appel1_test: protN+trap+unprot */
static void appel1_test(int n) {
	int i,j;
	if(signal(SIGSEGV, handler) < 0)
		assert(0);
	n /= PAGES;
	B();
        for (i = 0; i < n; i++) {
                if(mprotect(mem, PAGES * pagesize, PROT_READ) < 0)
			assert(0);
                for (j = 0; j < PAGES; j++) {
                        please_unprotect = mem + perm[j] * pagesize;
                        please_protect = 0;
                        *(volatile int *)please_unprotect = 10;
                }
        }
	E();
}

/* appel2_test: prot1+trap+unprot */
static void appel2_test(int n) {
	int i,j;

	if(signal(SIGSEGV, handler) < 0)
		assert(0);
	        if(mprotect(mem, PAGES * pagesize, PROT_READ) < 0)
		assert(0);
        if(mprotect(mem + perm[PAGES - 1] * pagesize, pagesize, PROT_WRITE) < 0)
		assert(0);
        please_protect = mem + perm[PAGES - 1] * pagesize;
        *please_protect = 0;
	n /= PAGES;

	B();
        for (i = 0; i < n; i++) {
                for (j = 0; j < PAGES; j++) {
                        please_unprotect = mem + perm[j] * pagesize;
                        *(volatile int *)please_unprotect = 10;
                        please_protect = please_unprotect;
                }
	}
	E();

}

/* prot100: time to protect a single, random, page.  */
static void prot100_test(int n) {
     int i;
     B();
     /* protect random pages */
     for(i=0; i < n; i++) {
#if 1
	  if(mprotect(mem, 100 * pagesize, PROT_READ) < 0)
	       demand(0,0);
#else
	  if (sys_self_mod_pte_range (0, 0, PG_W, (unsigned )mem, 100) < 0) {
	    printf ("FAILED\n");
	    return;
	  }
#endif
     }
     E();
}



/******************************************************************************
 *
 * Driver.
 */

	/* test vector */
static struct { char str[64];  int n; void (*fptr)(int); unsigned t[3], time; } tv[] = {
   { "trap",                    1000000,        trap }, 
   { "prot1", 		        1000000, 	prot1_test }, 	
   { "prot100", 		100000,		prot100_test }, 	
   { "uprot1", 		        1000000, 	uprot1_test }, 	
   { "uprot100", 		100000,		uprot100_test }, 	
   { "protN+trap+unprot", 	100000,		appel1_test }, 	
   { "prot1+trap+unprot", 	100000,		appel2_test }, 	
   };

int main() { 
	int 	i;

	prot_init();

	printf ("\nTimes are in micro-seconds per 1000 operations\n");

	for(i = 0; i < sizeof tv / sizeof tv[0]; i++) {
	     tv[i].fptr(tv[i].n);
	     printf ("%s:%d\n", tv[i].str, ((e-b+1)*ae_getrate ())/(tv[i].n/1000));
	}

	printf ("\nFinished\n");

	return 0;
}
