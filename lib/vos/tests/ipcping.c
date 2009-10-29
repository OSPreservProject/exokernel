#include <signal.h>
#include <stdio.h>

#include <xok/ipc.h>
#include <xok/pctr.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>

#include <vos/ipc.h>
#include <vos/signals.h>
#include <vos/ipc-services.h>
#include <vos/proc.h>
#include <vos/errno.h>


/*
 * ipc ping performance: ping back and forth...
 */

//#define __DEBUG__
#ifndef __DEBUG__
#define LOOPSIZE 10000
#else
#define LOOPSIZE 10
#endif
#define TIMEOUT  10000
unsigned int loops = LOOPSIZE;

// #define USE_TICKS
#ifdef USE_TICKS

#define START start = __sysinfo.si_system_ticks
#define STOP  stop  = __sysinfo.si_system_ticks

#define CALC(test_name) \
{ \
  u_quad_t diff; \
  double usec; \
  diff = stop-start; \
  usec = (double) (diff*__sysinfo.si_rate) / loops; \
  printf("%s: %04qd ticks, per: %2.3f us, %4.3f cycles\n", \
         test_name, diff, usec, __sysinfo.si_mhz*usec); \
  fflush(stdout); \
}

#else

#define START start = rdtsc()
#define STOP  stop  = rdtsc()

#define CALC(test_name) \
{ \
  u_quad_t diff; \
  double usec; \
  double cycles; \
  diff = stop-start; \
  cycles = (double) diff / loops; \
  usec = (double) cycles/__sysinfo.si_mhz; \
  printf("%s: per: %2.3f us, %4.3f cycles\n", \
         test_name, usec, cycles); \
  fflush(stdout); \
}

#endif



static int 
my_handler(int,int,int,int,u_int,void*) __attribute__ ((regparm (3)));

static int
my_handler(int task, int arg1, int arg2, int ipctype, u_int envid, void *d)
{
  return arg1+1;
}


int 
client(pid_t srvpid)
{
  int i,r,n=110,ret,async=0;
  u_quad_t start, stop;

  START;
  for(i=0; i<loops; i++)
  { 
    r = pct(MIN_APP_IPC_NUMBER, n, 0, 0, 0L, 0L, srvpid, &ret);
   
    if (r < 1)
    {
      printf("ipc failed %d %d\n",r,errno);
      break;
    }
    

    if (errno == V_WOULDBLOCK)
    {
      async++;
      while (ret != n+1)
        asm volatile("" ::: "memory");
    }
    
    ipc_clear_request(r);

#ifdef __DEBUG__
    printf("got %d, n was %d\n", ret, n);
#endif
    n = ret;
  }
  
  STOP;
  CALC("pct_pings");
  printf("there were %d async handles\n", async);
  return 0;
}

int 
server()
{
  env_suspend();
  return 0;
}


int main()
{
  ipc_register_handler(MIN_APP_IPC_NUMBER, my_handler);

  if (fork() == 0) /* child */
  {
    client(getppid());
    kill(getppid(), SIGKILL);
  }

  else
  {
    server();
  }

  return 0;
}


