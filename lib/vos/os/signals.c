
#include <stdio.h>
#include <signal.h>

#include <xok/env.h>
#include <vos/ipc-services.h>
#include <vos/proc.h>
#include <vos/kprintf.h>
#include <vos/ipc.h>

#define dprintf if (0) printf


static int
unix_signals_h(int task, int action, int a2, int ipc_type, u_int envid, void *) 
  __attribute__ ((regparm (3)));

static int
unix_signals_h(int task, int action, int a2, int ipc_type, u_int envid, void *d)
{
  dprintf("unix_signals_h: received %d, %d, %d\n",task, action, a2);
  
  switch (action)
  {
    case SIGKILL:
      kprintf("%d: sigkill caught\n", getpid());
      exit(0);
      return 0;
    case SIGINT:
      kprintf("%d: sigint caught\n", getpid());
      exit(0);
      return 0;
    case SIGQUIT:
      kprintf("%d: sigquit caught\n", getpid());
      exit(0);
      return 0;
    case SIGTERM:
      kprintf("%d: sigterm caught\n", getpid());
      exit(0);
      return 0;
    case SIGABRT:
      kprintf("%d: sigabrt caught\n", getpid());
      exit(0);
      return 0;
    case SIGCONT:
      UAREA.u_status = U_RUN;
      return 0;
    case SIGSTOP:
      UAREA.u_status = U_SLEEP;
      return 0;
  }
  return 0;
}

void
signals_init(void)
{
  int r = ipc_register_handler(IPC_UNIX_SIGNALS, unix_signals_h);
  if (r < 0)
  {
    printf("signals_init: cannot register handler for UNIX signals. "
	   "Signals will not work\n");
  }
}


