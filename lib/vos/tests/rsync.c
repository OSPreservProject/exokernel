
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vos/sbuf.h>
#include <vos/locks_impl.h>

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/pctr.h>


#define START start = rdtsc()
#define STOP  stop  = rdtsc()


#define CALC(test_name,l) \
{ \
  double usec; \
  usec = (double) (stop-start) / __sysinfo.si_mhz; \
  printf("%s: %f us elapsed, per %f\n", test_name, usec, usec/(l)); \
  counter = 0; counter2 = 0; \
  fflush(stdout); \
}



/*
 * test rsync
 */
  
u_quad_t start, stop;
u_quad_t pctr_val;
#define CRITICAL_SECTION_DELAY	100

u_int *data;
u_int *version;
spinlock_t *sl1;
int counter  = 0;
int counter2 = 0;


static inline void 
critical_section_update(int x)
{ 
  int ii; 
  int v = *data; 
  
  *version = (*version)+1; 
  
  *data = 11; 
  for(ii=0; ii<x; ii++); 
  *data = 12; 
  for(ii=0; ii<x; ii++); 
  *data = 13; 
  for(ii=0; ii<x; ii++); 
  *data = 14; 
  for(ii=0; ii<x; ii++); 
  *data = 15; 
  for(ii=0; ii<x; ii++); 
  *data = 16; 
  for(ii=0; ii<x; ii++); 
  *data = v+1; 
  
  *version = (*version)+1; 
}


static inline void 
critical_section_read_nosync()
{ 
  int v; 
  v = *data; 
}


static inline void 
critical_section_read_noser()
{ 
  int v1, v2, v; 
restart: 
  v1 = *version; 
  v = *data; 
  v2 = *version; 
  if (v1 != v2 || (v1 & 1)) { counter++; goto restart; } 
  if (v < 100) 
  {
    // printf("%d %d %d\n", v1, v, v2);
    counter2++; 
  }
}


static inline void 
critical_section_read()
{ 
  int v1, v2, v; 
restart: 
  v1 = *version; 
  asm volatile("lock\n" "\tincl %0\n" : "=m" (v1) : "m" (v1));
  v = *data; 
  asm volatile("lock\n" "\tdecl %0\n" : "=m" (v1) : "m" (v1));
  v2 = *version; 

  if (v1 != v2 || (v1 & 1)) { counter++; goto restart; } 
  if (v < 100) 
  {
    counter2++; 
  }
}


static inline void 
critical_section_read_cpuid()
{ 
  int v1, v2, v; 
restart: 
  v1 = *version; 
  asm volatile("cpuid\n" ::: "eax", "ebx", "ecx", "edx");
  v = *data; 
  asm volatile("cpuid\n" ::: "eax", "ebx", "ecx", "edx");
  v2 = *version; 

  if (v1 != v2 || (v1 & 1)) { counter++; goto restart; } 
  if (v < 100) 
  {
    kprintf("%d %d %d\n",v1,v,v2);
    counter2++; 
  }
}


#define UPDATE_LOOPS 1000000
#define READ_LOOPS   20000000

int 
main(int argc, char **argv)
{
  int i;
  u_int *stage;

  stage = (u_int *) shared_malloc(0,4);
  data = (u_int *) shared_malloc(0, 4);
  version = (u_int *) shared_malloc(0, 4);
  sl1 = (spinlock_t*) shared_malloc(0,sizeof(spinlock_t));
  spinlock_reset(sl1);
  
  *version = 0;
  *data = 100;
  *stage = 0;
  
  START;
  for(i=0; i<1000000; i++)
  {
    spinlock_acquire(sl1);
    critical_section_read_nosync();
    spinlock_release(sl1);
  }
  STOP;
  CALC("cri_sec_read___lsync",1000000);

  START;
  for(i=0; i<READ_LOOPS; i++)
  {
    critical_section_read();
  }
  STOP;
  CALC("cri_sec_read__srsync",READ_LOOPS);
  printf("counter: %d, counter2: %d\n",counter,counter2);counter=counter2=0;
  
  START;
  for(i=0; i<READ_LOOPS; i++)
  {
    critical_section_read_noser();
  }
  STOP;
  CALC("cri_sec_read___rsync",READ_LOOPS);
  printf("counter: %d, counter2: %d\n",counter,counter2);counter=counter2=0;
  
  START;
  for(i=0; i<READ_LOOPS; i++)
  {
    critical_section_read_cpuid();
  }
  STOP;
  CALC("cri_sec_read___cpuid",READ_LOOPS);
  printf("counter: %d, counter2: %d\n",counter,counter2);counter=counter2=0;
  
  START;
  for(i=0; i<UPDATE_LOOPS; i++)
  {
    spinlock_acquire(sl1);
    critical_section_update(CRITICAL_SECTION_DELAY);
    spinlock_release(sl1);
  }
  STOP;
  CALC("cri_sec_updt_latency",UPDATE_LOOPS);
  
  *version = 0;
  *data = 100;

#ifdef __SMP__
  if (fork_to_cpu(1, -1) != 0) /* child, on cpu 1, update */
  {
    while(*stage == 0) asm volatile("":::"memory");
    *stage = 2;
    
    for(i=0; i<UPDATE_LOOPS; i++)
    {
      spinlock_acquire(sl1);
      critical_section_update(CRITICAL_SECTION_DELAY);
      spinlock_release(sl1);
    }

    while(*stage == 2) asm volatile("":::"memory");
    *stage = 4;

    for(i=0; i<UPDATE_LOOPS; i++)
    {
      spinlock_acquire(sl1);
      critical_section_update(CRITICAL_SECTION_DELAY);
      spinlock_release(sl1);
    }

    while(*stage == 4) asm volatile("":::"memory");
    *stage = 6;
    
    for(i=0; i<UPDATE_LOOPS; i++)
    {
      spinlock_acquire(sl1);
      critical_section_update(CRITICAL_SECTION_DELAY);
      spinlock_release(sl1);
    }
    
  }

  else
  {
    *stage = 1;
    while(*stage != 2) asm volatile("":::"memory");
    
    START;
    for(i=0; i<READ_LOOPS; i++)
    {
      critical_section_read_noser();
    }
    STOP;
    printf("counter: %d, counter2: %d\n",counter,counter2);counter=counter2=0;
    CALC("cri_sec_read____cpu0",READ_LOOPS);

    *stage = 3;
    while(*stage != 4) asm volatile("":::"memory");

    START;
    for(i=0; i<READ_LOOPS; i++)
    {
      spinlock_acquire(sl1);
      critical_section_read_nosync();
      spinlock_release(sl1);
    }
    STOP;
    CALC("cri_sec_lock____cpu0",READ_LOOPS);

    *stage = 5;
    while(*stage != 6) asm volatile("":::"memory");

    START;
    for(i=0; i<READ_LOOPS; i++)
    {
      critical_section_read();
    }
    STOP;
    printf("counter: %d, counter2: %d\n",counter,counter2);counter=counter2=0;
    CALC("cri_sec_sread___cpu0",READ_LOOPS);

    shared_free(sl1,0,sizeof(spinlock_t));
    shared_free(stage,0,4);
    shared_free(data,0,4);
    shared_free(version,0,4);
  }

#endif
  return 0;
}


