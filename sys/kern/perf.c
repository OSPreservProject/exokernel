
#include <xok/sysinfo.h>
#include <xok/env.h>
#include <xok/sys_proto.h>
#include <xok/kerrno.h>
#include <xok/console.h>
#include <xok/printf.h>
#include <xok/pctr.h>
#include <xok/mplock.h>

#include <machine/cpufunc.h>
#include <dev/isa/isareg.h>
#include <i386/isa/nvram.h>
#include <i386/isa/timerreg.h>
#include <xok/kclock.h>


/* these are for performance debugging */
u_quad_t rdtsc_accm_0 = 0;
u_quad_t rdtsc_accm_1 = 0;
u_quad_t rdtsc_accm_2 = 0;
u_quad_t rdtsc_accm_3 = 0;

u_quad_t kern_accm_0 = 0;
u_quad_t kern_accm_1 = 0;
u_quad_t kern_accm_2 = 0;
u_quad_t kern_accm_3 = 0;


void
sys_perf_dump (u_int sn)
{
  printf("%qd, %qd, %qd, %qd; ", 
         rdtsc_accm_0, rdtsc_accm_1, rdtsc_accm_2, rdtsc_accm_3);
  printf("%qd %qd %qd %qd\n",
         kern_accm_0, kern_accm_1, kern_accm_2, kern_accm_3);
  
  rdtsc_accm_0 = 0;
  rdtsc_accm_1 = 0;
  rdtsc_accm_2 = 0;
  rdtsc_accm_3 = 0;
  kern_accm_0 = 0;
  kern_accm_1 = 0;
  kern_accm_2 = 0;
  kern_accm_3 = 0;
}


void
sys_perf_timer (u_int sn)
{
#if 0
  u_quad_t last = 0;
  int i;
#endif

#if 0  /* speed of apic timer reset - roughly 57 cycles on PPro 200Mhz */
 rdtsc_accm_0 = 0;
 last = rdtsc();
 for(i=0; i<1000000; i++)
 {
   apic_read(APIC_TMICT);
   apic_write(APIC_TMICT, 40);
 }
 rdtsc_accm_0 = rdtsc() - last;
 printf("%qd\n", rdtsc_accm_0);
#endif

#if 0  /* speed of rtc timer reset - roughly 600 cycles on PPro 200Mhz */
 rdtsc_accm_1 = 0;
 last = rdtsc();
 for(i=0; i<1000000; i++)
 {
   mc146818_write(NULL, MC_REGA, rtc_current_rega);
 }
 rdtsc_accm_1 = rdtsc() - last;
 printf("%qd\n", rdtsc_accm_1);
#endif

#if 0  /* speed of 8253 timer reset - roughly 410 cycles on PPro 200Mhz */
 rdtsc_accm_2 = 0;
 last = rdtsc();
 for(i=0; i<1000000; i++)
 {
   outb(IO_TIMER1, TIMER_DIV(SI_HZ)%256);
   outb(IO_TIMER1, TIMER_DIV(SI_HZ)/256);
 }
 rdtsc_accm_2 = rdtsc() - last;
 printf("%qd\n", rdtsc_accm_2);
#endif
}


u_quad_t lock_pp_counter = 0;
u_quad_t lock_env_counter = 0;

void
sys_lock_usage (u_int sn)
{
#ifdef __SMP__
  int i, j;

  printf("Exokernel Lock Usage: \n");

  printf("global slocks\n");
  j=0;
  for(i=0; i<NR_GLOBAL_SLOCKS; i++)
  {
    if (global_slocks[i].uses != 0)
    {
      printf("%d:%qd  ", i, global_slocks[i].uses);
      j++;
      if (j == 6) { printf("\n"); j=0; }
    }
  }
  if (j != 0) printf("\n");
  printf("\n");

  printf("global qlocks\n");
  j=0;
  for(i=0; i<NR_GLOBAL_QLOCKS; i++)
  {
    if (global_qlocks[i].uses != 0)
    {
      printf("%d:%qd  ", i, global_qlocks[i].uses);
      j++;
      if (j == 6) { printf("\n"); j=0; }
    }
  }
  if (j != 0) printf("\n");
  printf("\n");

  printf("env locks: %qd\n", lock_env_counter);

  j = 0;
  for(i=0; i<NENV; i++)
  {
    if (__envs[i].env_spinlock.uses != 0)
    {
      printf("%d:%qd  ", i, __envs[i].env_spinlock.uses);
      j++;
      if (j == 6) { printf("\n"); j=0; }
    }
  }
  if (j != 0) printf("\n");
  printf("\n");
  
  printf("pp locks: %qd\n", lock_pp_counter);
#endif
}


struct kspinlock ps_lock;
struct kqueuelock pq_lock;
struct krwlock prw_lock;

  
void 
syscall_perf_init()
{
#ifdef __SMP__
  krwlock_init(&prw_lock);
  kspinlock_init(&ps_lock);
  kqueuelock_init(&pq_lock);
#endif
}


void
sys_slock (u_int sn, u_int delay)
{
#ifdef __SMP__
  int i;
  int j;
  for(i=0; i<1000000; i++)
  {
    kspinlock_get(&ps_lock);
    for(j=0; j<delay; j++);
    kspinlock_release(&ps_lock);
    for(j=0; j<(delay/2); j++);
  }
#endif
}


void
sys_qlock (u_int sn, u_int delay)
{
#ifdef __SMP__
  int i;
  int j;
  for(i=0; i<1000000; i++)
  {
    kqueuelock_get(&pq_lock);
    for(j=0; j<delay; j++);
    kqueuelock_release(&pq_lock);
    for(j=0; j<(delay/2); j++);
  }
#endif
}


void
sys_rlock (u_int sn, u_int delay)
{
#ifdef __SMP__
  int i;
  int j;
  for(i=0; i<1000000; i++)
  {
    krwlock_read_get(&prw_lock);
    for(j=0; j<delay; j++);
    krwlock_read_release(&prw_lock);
    for(j=0; j<(delay/2); j++);
  }
#endif
}


void
sys_wlock (u_int sn, u_int delay)
{
#ifdef __SMP__
  int i;
  int j;
  for(i=0; i<1000000; i++)
  {
    krwlock_write_get(&prw_lock);
    for(j=0; j<delay; j++);
    krwlock_write_release(&prw_lock);
    for(j=0; j<(delay/2); j++);
  }
#endif
}



