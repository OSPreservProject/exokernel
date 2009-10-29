
/*
 * Copyright MIT 1999
 */

#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <xok/sys_ucall.h>
#include <xok/scheduler.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/ipc.h>
#include <xok/pmap.h>

#include <vos/cap.h>
#include <vos/vm.h>
#include <vos/kprintf.h>
#include <vos/proc.h>
#include <vos/fd.h>
#include <vos/ipc.h>
#include <vos/errno.h>


char **environ = 0L;
int errno = 0;


#define INITIALIZE(m) \
  {\
    extern void m##_init(void); \
    m##_init(); \
  }


void
create_extproc()
{
  UAREA.ext_proc = (struct ExtProc*) malloc(sizeof(struct ExtProc));
  (UAREA.ext_proc)->curenv = &__envs[envidx(sys_geteid())];
  bzero((UAREA.ext_proc)->p_children, CHILDREN_MASK*sizeof(u_char));
  (UAREA.ext_proc)->p_num_children = 0;
}


void
__proc_child_start(void)
{
  create_extproc();
  INITIALIZE(ipc_child);
}


void
__proc_start(void)
{
  INITIALIZE(uinfo);
  INITIALIZE(malloc);

  create_extproc();

  INITIALIZE(sbuf);
  INITIALIZE(uidt);
  INITIALIZE(fpu);
  INITIALIZE(fd);
  INITIALIZE(raw_console);
  
  open("/dev/console",0,0); /* open stdin */
  open("/dev/console",0,0); /* open stdout */
  open("/dev/console",0,0); /* open stderr */

  INITIALIZE(ipc);
  INITIALIZE(signals);
  INITIALIZE(ipcport);
  INITIALIZE(iptable);
  INITIALIZE(arptable);

#ifdef VOS_DEBUG_LOCK
  INITIALIZE(dmlock);
#endif
  INITIALIZE(fsrv);
  INITIALIZE(ports);
  INITIALIZE(udp_socket);
  INITIALIZE(tcp_socket);
  INITIALIZE(spipe);
  INITIALIZE(prd);
  
  bzero(fault_handlers, sizeof(fault_handler_t)*NUM_FAULT_HANDLERS);
}


void 
__main () 
{
}


static int quanta_freed = 0;

/* frees quanta when a process dies */
static void 
free_quanta()
{
  int i_cpu, i_index, i_offset;
  int cpu_total = sys_get_num_cpus();
  struct Env* e = &__envs[envidx(getpid())];

  quanta_freed = 1;
  if (!e) 
    return; 

  for (i_cpu = 0; i_cpu < cpu_total; i_cpu++) 
  {
    for(i_index = 0; i_index < QMAP_SIZE; i_index++) 
    {
      unsigned char q = e->env_quanta[i_cpu][i_index];
      for(i_offset = 0; i_offset < 8; i_offset++) 
      {
        if ((q & (1<<i_offset)) != 0) 
	{
 	   sys_quantum_free(CAP_USER, i_index*8+i_offset, i_cpu);
	}
      }
    }
  }
}


  
static int __env_suspended = 0;
void
env_suspend()
{
  extern void wk_waitfor_value_neq (int *addr, int value, u_int cap);
  __env_suspended = 1;
 
  while(__env_suspended == 1) 
  {
    wk_waitfor_value_neq(&__env_suspended, 1, CAP_USER);
    printf("warning: woken up from env_suspend!\n");
  }
}



void 
_exit(int status) __attribute__ ((noreturn));

void
_exit(int status)
{
  if (quanta_freed != 1)
    free_quanta();
  sys_env_free (CAP_USER, sys_geteid());
  sys_cputs ("$$ I'm dead, but I don't feel dead.\n");
  for (;;);
}


