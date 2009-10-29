#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <xok/sys_ucall.h>
#include <xok/scheduler.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/pmapP.h>

#include <wafer/kprintf.h>
#include <wafer/wafer.h>

static int quanta_freed = 0;

void 
__main () 
{
}

void 
free_quanta()
{
  int i_cpu, i_index, i_offset;
  int cpu_total = sys_get_num_cpus();
  struct Env* e = &__envs[envidx(__envid)];

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
 	   sys_quantum_free(0, i_index*8+i_offset, i_cpu);
	}
      }
    }
  }
}

void 
__die (void) 
{
  if (quanta_freed != 1)
    free_quanta();
  sys_env_free (0, sys_geteid());
  sys_cputs ("$$ I'm dead, but I don't feel dead.\n");
  for (;;);
}

