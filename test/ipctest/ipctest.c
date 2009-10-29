#include <stdio.h>
#include <xok/pctr.h>
#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>
#include <exos/ipcdemux.h>
#include <exos/ipc.h>
#include <xok/env.h>
#include <unistd.h>
#include <exos/process.h>
#include <signal.h>

int pingpong(int code, int arg1,int arg2,int arg3, u_int caller) {
  return 2;
}

static int count = 0;
void callback(struct _ipc_func *c, int _ipcret, int a1,
	      int a2, int a3, int a4, u_int caller) {
  count++;
}

int main()
{
  unsigned long long start, stop, i, loopsize = 1000000;
  pctrval v;
  int pid, ret = 0;
  u_int eid;
  struct _ipc_func c;

  pid = fork();
  if (pid == 0) { /* child */
    sigset_t s;

    sigemptyset(&s);
    /* setup ping pong ipc servers */
    ipc_register(IPC_PING, pingpong);
    ipcdemux_register(IPC_USER, pingpong, 2);
    while (1) { /* sleep forever */
      sigsuspend(&s);
    };
    printf("Hey! The ipc server started running! exiting...\n");
    exit(-1);
  }
  eid = pid2envid(pid);

  /* wait until child is initialized */
  while (1) {
    if (_ipcout_default(eid, 4, &ret, IPC_PING, 0, 0, 0) != IPC_RET_OK ||
	ret != 2) {
      yield(eid);
    }
    else
      break;
  }

  printf("Default ipc with 4 arguments...\n");

  start = __sysinfo.si_system_ticks;

  v = rdtsc();

  for (i=0; i < loopsize; i++)
    _ipcout_default(eid, 4, &ret, IPC_PING, 0, 0, 0);

  v = rdtsc() - v;

  stop = __sysinfo.si_system_ticks;

  printf("%qd cycles per loop\n", v / loopsize);
  printf("%qu total ticks\n", stop - start);
  printf("%f usecs per loop\n",
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("%f cycles per tick\n", v / ((stop - start) * 1.0));
  printf("\n\n");

  printf("Loops: %qu\n", loopsize);
  printf("Start ticks: %qu\n", start);
  printf("Stop ticks:  %qu\n", stop);
  printf("Difference:  %qu\n", stop - start);
  printf("Machine MHz: %u\n", __sysinfo.si_mhz);
  printf("Machine usec/tick: %u\n", __sysinfo.si_rate);
  printf("Ticks per loop: %f\n", (stop - start) / (loopsize * 1.0));
  printf("Usecs per loop: %f\n", 
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("Cycles per loop: %qu\n", 
	 ((stop - start) * __sysinfo.si_rate * __sysinfo.si_mhz) / loopsize);

  /* now test with callbacks */
  printf("\nBasic ipc (no timeouts, etc) with callback and 4 arguments...\n\n");
  c.func = callback;

  start = __sysinfo.si_system_ticks;

  v = rdtsc();

  for (i=0; i < loopsize; i++)
    _ipcout(eid, IPC_PING, 0, 0, 1, &c, 0, 0, &ret, 0, (char*)&i);

  v = rdtsc() - v;

  stop = __sysinfo.si_system_ticks;

  printf("count: %d\n", count);

  printf("%qd cycles per loop\n", v / loopsize);
  printf("%qu total ticks\n", stop - start);
  printf("%f usecs per loop\n",
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("%f cycles per tick\n", v / ((stop - start) * 1.0));
  printf("\n\n");

  printf("Loops: %qu\n", loopsize);
  printf("Start ticks: %qu\n", start);
  printf("Stop ticks:  %qu\n", stop);
  printf("Difference:  %qu\n", stop - start);
  printf("Machine MHz: %u\n", __sysinfo.si_mhz);
  printf("Machine usec/tick: %u\n", __sysinfo.si_rate);
  printf("Ticks per loop: %f\n", (stop - start) / (loopsize * 1.0));
  printf("Usecs per loop: %f\n", 
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("Cycles per loop: %qu\n", 
	 ((stop - start) * __sysinfo.si_rate * __sysinfo.si_mhz) / loopsize);

  /* now test without arguments */
  printf("\nBasic ipc - no callbacks, etc - with 1 argument...\n\n");
  c.func = callback;

  start = __sysinfo.si_system_ticks;

  v = rdtsc();

  for (i=0; i < loopsize; i++)
    _ipcout(eid, IPC_USER, 0, 0, 0, &c, 0, 0, &ret, 0, 0);

  v = rdtsc() - v;

  stop = __sysinfo.si_system_ticks;

  /* cleanup child */
  kill(pid, SIGTERM);
  
  printf("%qd cycles per loop\n", v / loopsize);
  printf("%qu total ticks\n", stop - start);
  printf("%f usecs per loop\n",
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("%f cycles per tick\n", v / ((stop - start) * 1.0));
  printf("\n\n");

  printf("Loops: %qu\n", loopsize);
  printf("Start ticks: %qu\n", start);
  printf("Stop ticks:  %qu\n", stop);
  printf("Difference:  %qu\n", stop - start);
  printf("Machine MHz: %u\n", __sysinfo.si_mhz);
  printf("Machine usec/tick: %u\n", __sysinfo.si_rate);
  printf("Ticks per loop: %f\n", (stop - start) / (loopsize * 1.0));
  printf("Usecs per loop: %f\n", 
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("Cycles per loop: %qu\n", 
	 ((stop - start) * __sysinfo.si_rate * __sysinfo.si_mhz) / loopsize);

  return 0;
}
