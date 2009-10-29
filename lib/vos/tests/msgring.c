
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xok/pctr.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/msgring.h>
#include <vos/proc.h>


/*
 * test message buffer interface: should print out 
 *    read a message: hello
 *    read a message: world
 *    read a message: bingo
 */

struct msgring_buf
{
  struct msgring_buf *next;
  u_int *pollptr;		// &flag
  u_int flag;
  int n;			// 1
  int sz;			// IPC_MAX_MSG_SIZE
  char *data;			// &space
  char space[IPC_MAX_MSG_SIZE];
};

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
  int r, pid;
  struct msgring_buf mrb[2];
  char msg1[6] = "hello\n";
  char msg2[6] = "world\n";

  mrb[0].next = &mrb[1];
  mrb[0].pollptr = &(mrb[0].flag);
  mrb[0].flag = 0;
  mrb[0].n = 1;
  mrb[0].sz = IPC_MAX_MSG_SIZE;
  mrb[0].data = &(mrb[0].space[0]);

  mrb[1].next = &mrb[0];
  mrb[1].pollptr = &(mrb[1].flag);
  mrb[1].flag = 0;
  mrb[1].n = 1;
  mrb[1].sz = IPC_MAX_MSG_SIZE;
  mrb[1].data = &(mrb[1].space[0]);

  // pid = fork_to_cpu(1,-1);
  pid = fork();

  if (pid == 0) // child, on cpu 1
  {
    int i=0,j=0;
  
    r = sys_msgring_setring ((struct msgringent *) &mrb[0]);
    env_yield(-1);
    env_yield(-1);
    env_yield(-1);

    for(j=0; j<LOOPS; j++)
    { 
      r = sys_ipc_sendmsg(13, getppid(), 6, (unsigned long) &msg1[0]);

      while(mrb[i].flag == 0) 
      {
	asm volatile ("" ::: "memory"); 
	env_fastyield(getppid());
      }
      // printf("%s",mrb[i].data);
      mrb[i].flag = 0;
      i++; if (i == 2) i = 0;
    }
    printf("child done\n");
  }

  else // parent, on cpu 0
  {
    int i=0,j=0;
    
    r = sys_msgring_setring ((struct msgringent *) &mrb[0]);
    env_yield(-1);
    env_yield(-1);
    env_yield(-1);

    START;
    for(j=0; j<LOOPS; j++)
    {
      while(mrb[i].flag == 0) 
      {
	asm volatile ("" ::: "memory");
	env_fastyield(pid);
      }
      // printf("%s",mrb[i].data);
      mrb[i].flag = 0;
      i++; if (i == 2) i = 0;
      r = sys_ipc_sendmsg(13, pid, 6, (unsigned long) &msg2[0]);
    }
    STOP;
    CALC("ipcmsg");
  }
  
  return 0;
}


