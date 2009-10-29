
#include <stdio.h>
#include <xok/sys_ucall.h>
#include <vos/proc.h>

/*
 * simple fork test 
 */

int 
main()
{
  pid_t cpid;
  double a;

  a = 3.2 * 1.8 * 5.2 * 6.4 * 9.6;

#ifdef __SMP__
  cpid = fork_to_cpu(1,-1);
#else
  cpid = fork();
#endif

  printf("env %d (on cpu %d): %f (should be 1840.25088)\n", 
         getpid(), sys_get_cpu_id(), a);
  printf("env %d (on cpu %d): %f (should be 1840.25088)\n", 
         getpid(), sys_get_cpu_id(), a);

  if (cpid == 0) // child
  {
    a = 4.1 * 1.8;
    printf("env %d: %f (should be 7.38)\n", getpid(), a);
  }
  return 0;
}


