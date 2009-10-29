#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xok/pctr.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/msgring.h>
#include <vos/proc.h>
#include <vos/sbuf.h>


#define START start = rdtsc()
#define STOP  stop  = rdtsc()
#define LOOPS 10000

#define CALC(test_name) \
{ \
  double usec; \
  usec = ((double) (stop-start) / (LOOPS * __sysinfo.si_mhz)); \
  printf("%14s: %2.3f us, %4.3f cycles\n", \
         test_name, usec, usec * __sysinfo.si_mhz); \
  fflush(stdout); \
}

u_quad_t start, stop;

int 
main()
{
  int pid;
  char *shm = shared_malloc(0,64); 
  bzero(shm, 64);
  *((int*)shm) = 0;

  // pid = fork_to_cpu(1,-1);
  pid = fork();

  if (pid == 0) // child, on cpu 1
  {
    int j=0;
  
    for(j=0; j<LOOPS; j++)
    { 
      while (*((int*)shm) == 0)
      {
	asm volatile("" ::: "memory");
	env_fastyield(getppid());
      }
      *((int*)shm) = 0;
    }
    printf("child done\n");
  }

  else // parent, on cpu 0
  {
    int j=0;
    
    START;
    for(j=0; j<LOOPS; j++)
    {
      *((int*)shm) = 1;
      while (*((int*)shm) == 1)
      {
	asm volatile("" ::: "memory");
	env_fastyield(pid);
      }
    }
    STOP;
    CALC("shmmsg");
  }
  
  return 0;
}


