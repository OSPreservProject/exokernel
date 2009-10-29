
#include <xok/sys_ucall.h>
#include <pwafer/threads.h>
#include <pwafer/pwafer.h>
#include <pwafer/kprintf.h>

void 
test()
{
  int cpu = sys_get_cpu_id();
  kprintf("%d(%d,%d): hello world from cpu %d, %d, %x!\n", 
          __envid, sys_geteid(), __threadid, 
	  cpu, sys_quantum_get(cpu), UAREA.u_entprologue);
  pw_thread_exit();
}

#define NR_THREADS 20

int
main () 
{
  int r;
  kprintf("in hello program, will create %d threads\n", NR_THREADS);
  yield(-1);
  for(r = 0; r < NR_THREADS; r++) 
    pw_thread_create(test, -1);
  return 0;
}


