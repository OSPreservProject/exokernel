#include <stdio.h>
#include <signal.h>

#include <xok/ipc.h>
#include <xok/sys_ucall.h>

#include <vos/ipc.h>
#include <vos/signals.h>
#include <vos/ipc-services.h>
#include <vos/proc.h>
#include <vos/errno.h>


static int 
my_handler(int,int,int,int,u_int,void*) __attribute__ ((regparm (3)));

static int
my_handler(int task, int arg1, int arg2, int ipctype, u_int envid, void* d)
{
  int i=0;
 
  if (arg1 == 16)
  {
    printf("server: 16 received\n");
    for(i=0; i<500000; i++)
      sys_null();
    printf("server: done, 16\n");
  }

  else if (arg1 == 15)
  {
    printf("server: 15 received\n");
    for(i=0; i<1000000; i++)
      sys_null();
    sys_set_allowipc1(XOK_IPC_BLOCKED);
    printf("server: done, 15\n");
  }

  else if (arg1 == 14)
  {
    printf("server: 14 received\n");
    for(i=0; i<500000; i++)
      sys_null();
    printf("server: done, 14\n");
  }

  else if (arg1 == 13)
  {
    printf("server: 13 received\n");
    
    for(i=0; i<500000; i++)
      sys_null();

    printf("server: done, 13\n");
  }

  else if (arg1 == 12)
  {
    printf("server: 12 received, program not available\n");
    RETERR(V_PROGUNAVAIL);
  }

  return arg1;
}


static int 
my_ret_handler(int,int,int,int,u_int) __attribute__ ((regparm (3)));

static int 
my_ret_handler(int r, int arg1, int reqid, int ipctype, u_int envid) 
{
  if (r < 0)
    errno = arg1;
  return r;
}

static int ret;

int 
client(pid_t srvpid)
{
  int r;

  /* test simple pct */
  errno = 0;
  r = pct(MIN_APP_IPC_NUMBER, 11, 12, 0, 0L, my_ret_handler, srvpid, &ret);
  printf("c: pct(%d,11,12,0,%d) -> r,errno,ret:%d,%d,%d, ",
         MIN_APP_IPC_NUMBER,srvpid,r,errno,ret);
  printf("should be: 1,0,11\n");
  ipc_clear_request(r);
  
  /* test simple pct that passes two args */
  r = pct(MIN_APP_IPC_NUMBER, 12, 12, 0, 0L, my_ret_handler, srvpid, &ret);
  printf("c: pct(%d,12,12,0,%d) -> r,errno,ret:%d,%d,%d, ",
         MIN_APP_IPC_NUMBER,srvpid,r,errno,ret);
  printf("should be: 257,%d,-1\n",V_PROGUNAVAIL);
  ipc_clear_request(r);

  ret = -1;
  errno = 0;
 
  /* test ipc_msg */
  r = ipc_msg(MIN_APP_IPC_NUMBER, 13, 12, 0, 0L, my_ret_handler, srvpid, &ret);
  printf("c: msg(%d,13,12,0,%d) -> r,errno,ret:%d,%d,%d, ",
         MIN_APP_IPC_NUMBER,srvpid,r,errno,ret);
  printf("should be: 513,0,-1\n");

  while(ret == -1) 
  {
    asm volatile ("" : : : "memory");
    env_fastyield(srvpid);
  }
  printf("client eventually returned %d (should be 13)\n",ret);
  ipc_clear_request(r);

  /* test ipc_sleepwait, using pct */
  ret = -1;
  errno = 0;
  r = ipc_sleepwait(MIN_APP_IPC_NUMBER,14,12,0,0L,my_ret_handler,srvpid,1000);
  printf("c: msg(%d,14,12,0,%d) -> r,errno,ret:%d,%d,%d, ",
         MIN_APP_IPC_NUMBER,srvpid,r,errno,ret);
  printf("should be: 14,??,-1\n");
  ipc_clear_request(r);


  /* test ipc_sleepwait, using msg, should timeout */
  ret = -1;
  errno = 0;
  r = ipc_sleepwait(MIN_APP_IPC_NUMBER,15,12,0,0L,my_ret_handler,srvpid,1);
  printf("c: msg(%d,15,12,0,%d) -> r,errno,ret:%d,%d,%d, ",
         MIN_APP_IPC_NUMBER,srvpid,r,errno,ret);
  printf("should be: -1,35,-1\n");
  ipc_clear_request(r);


  /* test ipc_sleepwait, using msg, should not timeout */
  ret = -1;
  errno = 0;
  r = ipc_sleepwait(MIN_APP_IPC_NUMBER,16,12,0,0L,my_ret_handler,srvpid,0);
  printf("c: msg(%d,16,12,0,%d) -> r,errno,ret:%d,%d,%d, ",
         MIN_APP_IPC_NUMBER,srvpid,r,errno,ret);
  printf("should be: 16,??,-1\n");
  ipc_clear_request(r);
  
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


