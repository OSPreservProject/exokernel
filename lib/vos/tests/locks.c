
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vos/sbuf.h>

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/pctr.h>

#include "locks.h"


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
unsigned int loops = LOOPSIZE;

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
         test_name, pctr, pctr_val, (double) pctr_val/1000000, s); \
  fflush(stdout); \
}
#else
#define PCTR_START(v)
#define PCTR_STOP
#define PCTR_CALC(test_name, pctr, s) { printf("%s",s); fflush(stdout); }
#endif


#define START start = rdtsc()
#define STOP  stop  = rdtsc()

#define CALC(test_name) \
{ \
  double usec; \
  usec = (double) (stop-start) / __sysinfo.si_mhz; \
  elapsed = usec  / 1000000; \
  printf("%s: %f s elapsed\n", test_name, elapsed); \
  counter = 0; \
  fflush(stdout); \
}


#define NOCONT_CALC(test_name,s) \
{ \
  double sec; \
  sec = (double) (stop-start) / (__sysinfo.si_mhz * 1000000); \
  printf("%s %f %f %s",test_name,sec,sec-elapsed,s); \
  *per_delay = sec-elapsed; \
  counter = 0; \
  fflush(stdout); \
}


#define CONT_CALC(test_name,s) \
{ \
  double sec; \
  double nodelay; \
  sec = (double) (stop-start) / (__sysinfo.si_mhz * 1000000); \
  nodelay = sec - *per_delay; \
  printf("%s %f %f %d %s", \
         test_name, sec, nodelay/elapsed, counter,s); \
  counter = 0; \
  fflush(stdout); \
}


#define critical_section(x) \
{ \
  int v = *data; \
  if (v == my_cpu_id) counter++; \
  *data = my_cpu_id; \
  for(v=0; v<x; v++); \
}

#define delay(x) \
{ int __i; for(__i=0; __i<x; __i++); }



/*
 * test spinlock performances
 */
  
u_quad_t start, stop;
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
  int i;
  int counter = 0;
  double elapsed;
  int my_cpu_id = sys_get_cpu_id();
  int delay_factor;

#define CRITICAL_SECTION_DELAY	1000

  char *stage;
  double *per_delay;
  u_int *data;
  spinlock_t *sl1;

  stage = (char *) shared_malloc(0, 32);
  data = (u_int *) shared_malloc(0, 32);
  per_delay = (double *) shared_malloc(0, 32);
  sl1 = (spinlock_t*) shared_malloc(0,sizeof(spinlock_t));
  spinlock_reset(sl1);
  
  PCTR_START(PCTR_SEL); 
  START;
  for(i=0; i<loops; i++)
  {
    spinlock_acquire(sl1);
    critical_section(CRITICAL_SECTION_DELAY);
    spinlock_release(sl1);
    delay(0);
  }
  STOP;
  PCTR_STOP;
  CALC("critical_sec_latency");

  *stage = 0;

  if (fork_to_cpu(1, -1) == 0) /* child, on cpu 1 */
  {
    my_cpu_id = sys_get_cpu_id();
    srand(13);

    for(delay_factor=DELAY_BASE;delay_factor<DELAY_TOP;delay_factor+=DELAY_STEP)
    {
      while (*stage != 1) asm volatile("" ::: "memory");
      delay(1000000);

      /* small critical section */
      PCTR_START(PCTR_SEL); 
      START;
      for(i = 0; i < loops; i++)
      {
        spinlock_acquire(sl1);
        critical_section(CRITICAL_SECTION_DELAY);
        spinlock_release(sl1);
        delay(delay_factor);
      }
      STOP;
      PCTR_STOP;
      CONT_CALC("cpu1:", "");
      PCTR_CALC("", PCTR_STR, "\n");
      *stage = 2;
    }
      
    while (*stage != 3) asm volatile("" ::: "memory");
  }

  else
  {
    int my_cpu_id = sys_get_cpu_id();

    for(delay_factor=DELAY_BASE;delay_factor<DELAY_TOP;delay_factor+=DELAY_STEP)
    {
      PCTR_START(PCTR_SEL); 
      START;
      for(i = 0; i < loops; i++)
      {
        spinlock_acquire(sl1);
        critical_section(CRITICAL_SECTION_DELAY);
        spinlock_release(sl1);
        delay(delay_factor);
      }
      STOP;
      PCTR_STOP;
      NOCONT_CALC("latency:","");
      PCTR_CALC("", PCTR_STR, "\n");
      spinlock_reset(sl1);
    
      *stage = 1;
      delay(1000000);
    
      /* small critical section */ 
      PCTR_START(PCTR_SEL); 
      START;
      for(i = 0; i < loops; i++)
      {
        spinlock_acquire(sl1);
        critical_section(CRITICAL_SECTION_DELAY);
        spinlock_release(sl1);
        delay(delay_factor);
      }
      STOP;
      PCTR_STOP;
      while (*stage != 2) asm volatile("" ::: "memory");
      CONT_CALC("cpu0:", "");
      PCTR_CALC("", PCTR_STR,"\n");
      spinlock_reset(sl1);
      delay(1000000);
    }

    *stage = 3;
    shared_free(sl1,0,sizeof(spinlock_t));
    shared_free(data,0,32);
    shared_free(per_delay,0,32);
    shared_free(stage,0,32);
  }
#endif
  return 0;
}


