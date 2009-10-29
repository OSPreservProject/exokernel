#include <xok/env.h>
#include <xok/apic.h>
#include <xok/ipc.h>
#include <xok/ipi.h>
#include <xok/cpu.h>
#include <xok/kerrno.h>
#include <xok/mplock.h>
#include <xok/sys_proto.h>
#include <xok/syscall.h>


/* %eax register contain CPU */
void
plainipi()
{
#ifdef __SMP__
  u_short cpu = utf->tf_eax;
  lapic_ipi_cpu(cpu, T_IPI_IPITEST);
#endif
}


void
ipitest_ipi() 
{
}


#ifdef __SMP__

int
ipi_send_message(u_short cpu, u_short ctrl, u_int payload)
{
  struct CPUContext *cxt;

  assert(cpu != cpu_id);
  assert(cpu < smp_cpu_count);

  cxt = __cpucxts[cpu];

  MP_SPINLOCK_GET(&cxt->_ipi_spinlock);
 
  assert(cxt->_ipi_pending <= IPIQ_SIZE);
  
  if (cxt->_ipi_pending == IPIQ_SIZE)
  {
    printf("ipi_send_message: cpu %d has a full ipi queue\n", cpu);
    MP_SPINLOCK_RELEASE(&cxt->_ipi_spinlock);
    return -1;
  }

  cxt->_ipiq[cxt->_ipi_pending].ctrl = ctrl;
  cxt->_ipiq[cxt->_ipi_pending].payload = payload;
  cxt->_ipi_pending++;

  MP_SPINLOCK_RELEASE(&__cpucxts[cpu]->_ipi_spinlock);
  
  lapic_ipi_cpu(cpu, T_IPI);
  return 0;
}

#endif /* __SMP__ */


/* 
 * the real IPI handler. this should be real fast. we may do this once per
 * scheduler loop so we won't miss any ipi's
 */
void
ipi_handler() 
{
#ifdef __SMP__
  if (ipi_pending == 0) 
    return;
  
  in_ipih = 1;
  MP_SPINLOCK_GET(&ipi_spinlock);

  while(ipi_pending > 0)
  {
    u_int ctrl = ipiq[ipi_pending-1].ctrl;

    switch(ctrl)
    {
      case IPI_CTRL_HALT:
        localapic_disable();
	asm volatile("hlt");
	break;
      
      case IPI_CTRL_TLBFLUSH:
	{
	  u_int va = *(u_int*)(ipiq[ipi_pending-1].payload);
          invlpg(va);
          *(u_int*)(ipiq[ipi_pending-1].payload) = 0;
	}
	break;
    }
    ipi_pending--;
  }

  MP_SPINLOCK_RELEASE(&ipi_spinlock);
  in_ipih = 0;
#endif /* __SMP__ */
}



