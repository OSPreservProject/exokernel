#include <xok/types.h>
#include <xok/sysinfo.h>
#include <xok/pctr.h>
#include <xok/sys_ucall.h>

#include <vos/fpu.h>
#include <vos/locks.h>
#include <vos/proc.h>

#define START start = rdtsc()
#define STOP  stop  = rdtsc()

#define CALC(test_name) \
{ \
  u_quad_t diff; \
  double usec; \
  double cycles; \
  diff = stop-start; \
  cycles = (double) diff; \
  usec = (double) cycles/__sysinfo.si_mhz; \
  printf("%s: %2.3f us, %2.3f s\n", \
         test_name, usec, usec/1000000); \
  fflush(stdout); \
}




/* pi	  2 * 2 * 4 * 4 * 6 * 6 * 8 * 8 * ...
   ---- = -----------------------------------
    2	  1 * 3 * 3 * 5 * 5 * 7 * 7 * 9 * ...
NOTE:
    So, pi = 2 * sqr(product of evens/product of odds) * truncation_odd
 */

#include <stdio.h>

#define N 15000000
#define B 10

int main(int ac, char **av) 
{
  u_quad_t start, stop;
  int i, j;
  double numer = 1, denom = 1, pi = 1, ratio;
	  
  START;

  for (j = 1; j < B; j++) 
  { 
    numer *= 2 * j; 
    denom *= 2 * j + 1;
  }
  
  ratio = numer / denom;
  pi *= ratio;
  pi *= ratio;

  for (i = 1; i < N; i++) 
  { 
    numer = 1;
    denom = 1;
    for (j = B * i; j < B * (i + 1); j++) 
    { 
      numer *= 2 * j;
      denom *= 2 * j + 1;
    }
    
    ratio = numer / denom;
    pi *= ratio;
    pi *= ratio;
  }
    
  STOP;
    
  fprintf(stderr, "partial product order = %g\n", (double)((long)N * (long)B));
  fprintf(stderr, "pi = %17.17g\n", 2 * (2*(j-1)+1) * pi);
  CALC("pi_usrspa");
  return 0;
}


