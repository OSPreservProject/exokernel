#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vos/sbuf.h>

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/pctr.h>

#define PCTR_USR (1<<16)
#define PCTR_OS  (1<<17)
#define PCTR_ON  (1<<22)
#define PCTR_MEM 		0x43
#define PCTR_DCU_MISS		0x48
#define PCTR_BUS_LOCK		0x63
#define PCTR_BUS_WB		0x67
#define PCTR_BUS_RFO		0x66
#define PCTR_BUS_INVL		0x69
#define PCTR_BUS_HIT		0x7a
#define PCTR_BUS_HITM		0x7b
#define PCTR_BUS_SNOOPSTALL	0x7e

#define LOOPSIZE 1000000

#ifdef __PCTR__
#define PCTR_START(v) \
{ \
  u_int a = v | PCTR_USR | PCTR_ON; \
  sys_pctr(PCIOCS0, 0, (void*)&a); \
}

#define PCTR_STOP \
{ \
  struct pctrst st; \
  u_int a = 0; \
  sys_pctr(PCIOCRD, 0, (void*)&st); \
  pctr_val = st.pctr_hwc[0]; \
  sys_pctr(PCIOCS0, 0, (void*)&a); \
}

#define PCTR_CALC(test_name, pctr, s) \
{ \
  printf("%s %s %qd %f %s", \
         test_name, pctr, pctr_val, (double) pctr_val/LOOPSIZE, s); \
  fflush(stdout); \
}
#else
#define PCTR_START(v)
#define PCTR_STOP
#define PCTR_CALC(test_name, pctr, s) { printf("%s",s); fflush(stdout); }
#endif

#define START start[cpu] = rdtsc()
#define STOP  stop[cpu]  = rdtsc()

#define CALC(test_name) \
{ \
  double usec; \
  usec = ((double) (stop[cpu]-start[cpu]) / (LOOPSIZE * __sysinfo.si_mhz)); \
  printf("%14s (%d): %2.3f us, %4.3f cycles\n", \
         test_name, cpu, usec, usec * __sysinfo.si_mhz); \
  fflush(stdout); \
}


/*
 * test spinlock performances
 */
  
u_quad_t start[4], stop[4];
u_quad_t pctr_val;

#define PCTR_SEL PCTR_BUS_HIT;
#define PCTR_STR "#hit"
#define DELAY_BASE 	0
#define DELAY_TOP	15
#define DELAY_STEP	3

int 
main(int argc, char **argv)
{
#ifdef __SMP__
  int iters[] = {20,50,85,90,92,95,97,100,110,125,150,200,300,500,0};
  int i;

  for (i=0; iters[i] != 0; i++)
  { 
    char *stage; 
    int cpu = 0;
    stage = (char *) shared_malloc(0, 32);
  
    *stage = 0;

    START;
    sys_qlock(iters[i]);
    STOP; 
    CALC("single_sys_qlock");
  
    if (fork_to_cpu(1, -1) == 0) /* on cpu 1 */
    {
      cpu = 1;
      *stage = 1;
      while (*stage != 5) asm volatile("" ::: "memory");
      
      START;
      sys_qlock(iters[i]);
      STOP; 
      CALC("quad_sys_qlock");
      exit(0);
    }
  
    else if (fork_to_cpu(2, -1) == 0) /* on cpu 2 */
    {
      cpu = 2;
      while (*stage != 1) asm volatile("" ::: "memory");
      *stage = 2;
      while (*stage != 5) asm volatile("" ::: "memory");
      
      START;
      sys_qlock(iters[i]);
      STOP; 
      CALC("quad_sys_qlock");
      exit(0);
    }
  
    else if (fork_to_cpu(3, -1) == 0) /* on cpu 3 */
    {
      cpu = 3;
      while (*stage != 2) asm volatile("" ::: "memory");
      *stage = 3;
      while (*stage != 5) asm volatile("" ::: "memory");
      
      START;
      sys_qlock(iters[i]);
      STOP; 
      CALC("quad_sys_qlock");
      exit(0);
    }
  
    else /* on cpu 0 */
    {
      cpu = 0;
      while (*stage != 3) asm volatile("" ::: "memory");
      *stage = 5;
  
      START;
      sys_qlock(iters[i]);
      STOP; 
      CALC("quad_sys_qlock");

      for(cpu=0; cpu<1000000; cpu++)
      {
	int j;
	for(j=0; j<1000; j++);
      }
    }
  }
  
  #endif
  return 0;
}


