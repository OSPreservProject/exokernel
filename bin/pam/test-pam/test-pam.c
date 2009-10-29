#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <xok/sys_ucall.h>
#include <xok/capability.h>
#include <xok/pam.h>
#include <xok/pctr.h>

#include "test-pam.h"

#define SIZE 1 /* (1024*1024) */

void main (int argc, char **argv)
{
  char     buf[SIZE];
  arg_t    args;

  /* Initialize the abstraction and then perform the test sequence */

  S_INIT(0);

  {
    int i, t=0, rounds=atoi(argv[1]);

    for (i=0 ; i < rounds ; i++) {
      pctrval t0 = rdtsc();
      S_EMPTY(0);
      t += (rdtsc() - t0);
      bzero ((void *)buf, SIZE);
    }

    printf ("%d\t%d\n", rounds, (unsigned int) t/200);
  }
}



