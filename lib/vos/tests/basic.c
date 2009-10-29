
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include <vos/proc.h>
#include <vos/sbuf.h>

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/pctr.h>


/*
 * basic benchmarks
 */

#define LOOPSIZE 1000000
unsigned int loops = LOOPSIZE;

#define START start = rdtsc()
#define STOP  stop  = rdtsc()

#define CALC(test_name) \
{ \
  double usec; \
  usec = ((double) (stop-start) / (loops * __sysinfo.si_mhz)); \
  printf("%14s: %2.3f us, %4.3f cycles\n", \
         test_name, usec, usec * __sysinfo.si_mhz); \
  fflush(stdout); \
}


/*****************  procedure calling tests *********************/

void null0() {} 
void null1(int a) {} 
void null2(int a, int b) {} 
void null3(int a, int b, int c) {} 
void null4(int a, int b, int c, int d) {} 
void null5(int a, int b, int c, int d, int e) {} 

void
proc_call_tests()
{
  extern void __asm_fasttrap ();
  u_quad_t start, stop;
  int cnt = 0;
    
  START;
  for (cnt=0; cnt<loops; cnt++) {}
  STOP;
  CALC("empty_for_loop");
 
  START;
  for (cnt=0; cnt<loops; cnt++) { null0(); }
  STOP;
  CALC("null_proc_call");
  
  START;
  for (cnt=0; cnt<loops; cnt++) { null1(1); }
  STOP;
  CALC("1arg_proc_call");

  START;
  for (cnt=0; cnt<loops; cnt++) { null2(1,2); }
  STOP;
  CALC("2arg_proc_call");

  START;
  for (cnt=0; cnt<loops; cnt++) { null3(1,2,3); }
  STOP;
  CALC("3arg_proc_call");

  START;
  for (cnt=0; cnt<loops; cnt++) { null4(1,2,3,4); }
  STOP;
  CALC("4arg_proc_call");

  START;
  for (cnt=0; cnt<loops; cnt++) { null5(1,2,3,4,5); }
  STOP;
  CALC("5arg_proc_call");
  
  START;
  for (cnt=0; cnt<loops; cnt++) { sys_null(); }
  STOP;
  CALC("null__sys_call");

  START;
  for (cnt=0; cnt<1; cnt++) { sys_slock(0); }
  STOP;
  CALC("null_sys_slock");

  START;
  for (cnt=0; cnt<1; cnt++) { sys_qlock(0); }
  STOP;
  CALC("null_sys_qlock");

  START;
  for (cnt=0; cnt<1; cnt++) { sys_rlock(0); }
  STOP;
  CALC("null_sys_rlock");

  START;
  for (cnt=0; cnt<1; cnt++) { sys_wlock(0); }
  STOP;
  CALC("null_sys_wlock");

  START;
  for (cnt=0; cnt<loops; cnt++) __asm_fasttrap();
  STOP;
  CALC("krn_trap_n_bck");
}


/*****************  memory tests *********************/
  

char mem_s1[4096];
char mem_s2[4096];

void
memory_tests()
{
  u_quad_t start, stop;
  int cnt = 0;
  
  START;
  for (cnt=0; cnt<loops; cnt++) bzero(mem_s1,4096); 
  STOP;
  CALC("bzero_____4096");
  
  START;
  for (cnt=0; cnt<loops; cnt++) bcopy(mem_s1,mem_s2,4096); 
  STOP;
  CALC("bcopy_____4096");
}



/*****************  locks tests *********************/

#include <vos/locks_impl.h>

spinlock_t sl1;
yieldlock_t yl1;
volatile int val;

void 
locks_tests()
{
  u_quad_t start, stop;
  int cnt = 0;

  spinlock_reset(&sl1);
  yieldlock_reset(&yl1);

  START;
  for (cnt=0; cnt<loops; cnt++) 
  { 
    spinlock_acquire(&sl1);
    spinlock_release(&sl1);
  }
  STOP;
  CALC("spinlk_latency");

  START;
  for (cnt=0; cnt<loops; cnt++) 
  { 
    spinlock_acquire(&sl1);
    spinlock_acquire(&sl1);
    spinlock_release(&sl1);
    spinlock_release(&sl1);
  }
  STOP;
  CALC("spinlk_doublel");

  val = 0;
  START;
  for (cnt=0; cnt<loops; cnt++)
  {
    spinlock_acquire(&sl1);
    val++;
    spinlock_release(&sl1);
  }
  STOP;
  CALC("spinlk_incrmnt");

  START;
  for (cnt=0; cnt<loops; cnt++)
  {
    asm volatile("lock\n\tincl %0\n" : "=m" (val) : "0" (val));
  }
  STOP;
  CALC("atomic_incrmnt");

  START;
  for (cnt=0; cnt<loops; cnt++)
  {
    int a = 1;
    asm volatile("lock\n\txaddl %0,%1\n"
	         : "=r" (a), "=m" (val)
		 : "0" (a), "m" (val));
  }
  STOP;
  CALC("atomic____xadd");
  
  START;
  for (cnt=0; cnt<loops; cnt++)
  {
    int new, equal;
    equal = val;
    new = equal++;

    asm volatile("lock\n\tcmpxchgl %2,%0\n"
                 : "=m" (val), "=a" (equal) 
		 : "r" (new), "m" (val), "a" (equal) 
		 : "eax", "cc", "memory");
  }
  STOP;
  CALC("atomic_cmpxchg");
  
  START;
  for (cnt=0; cnt<loops; cnt++)
  {
    val++;
  }
  STOP;
  CALC("plain_incrment");

  START;
  for (cnt=0; cnt<loops; cnt++) 
  { 
    yieldlock_acquire(&yl1);
    yieldlock_release(&yl1);
  }
  STOP;
  CALC("yildlk_latency");
}



/*****************  yield, cxtswitch related tests *********************/

void
yield_tests()
{
  int pid1;
  u_quad_t start, stop;
  int cnt = 0;
  
  if ((pid1 = fork()) == 0) // child
  {
    u_int eid = sys_geteid();
    pid1 = getppid();

    START;
    for (cnt=0; cnt<2*loops; cnt++) env_fastyield(eid);
    STOP;
    CALC("fastyld_2_self");

    START;
    for (cnt=0; cnt<loops; cnt++) env_fastyield(pid1);
    STOP;
    CALC("fastyld_w_cxts");
  }

  else // parent
  {
    UAREA.u_status = U_SLEEP;
    while (1)
      env_fastyield(pid1);
  }

  kill(pid1, SIGKILL);
}

void
sched_test()
{
  u_quad_t start, stop;
  int cnt = 0;

  START;
  for(cnt=0; cnt<loops; cnt++) env_yield(-1);
  STOP;
  CALC("schd_loop_arnd");
}



/***************** ipi tests  *********************/

void
ipi_tests()
{
#ifdef __SMP__
  extern void __asm_ipitest_plain (u_short cpu) __attribute__ ((regparm (1)));
  int pid;

  if ((pid = fork_to_cpu(1,-1)) == 0) /* child */
  { 
    u_quad_t start, stop;
    int cnt = 0;

    START;
    for (cnt=0; cnt<loops; cnt++) __asm_ipitest_plain(0);
    STOP;
    CALC("plain_ipi_kern");

    exit(0);
  }
  
  else if (pid > 0)
  {
    while(env_alive(pid)) asm("" ::: "memory");
  }

#else
  printf("sorry, on uniprocessor, no ipi tests\n");
#endif
  return;
}



/*****************  page fault benchmark *********************/

#include <vos/vm.h>

int num_faults = 0;
int fault_addr = 0x13;
u_quad_t start, stop;

int my_fault_handler(u_int va, u_int errcode, u_int eflags, u_int eip)
{
  if (--num_faults == 0)
  {
    STOP;
    CALC("usr_page_fault");
    exit(0);
  }
  return 0;
}

void
pfault_test()
{
  int pid;
  if ((pid = fork()) == 0) // child
  {
    fault_handler_add(my_fault_handler, 0x13, 0x100);
    num_faults = loops;
  
    START;
    *((unsigned int*)fault_addr) = 0;
  }
  
  else
  {
    while(env_alive(pid)) asm("" ::: "memory");
  }
}



/************ driver ****************/

inline void
usage(char *s)
{
  printf("%s [-l loops] [locks,yield,call,ipi,pfault,mem,sched]\n", s);
}

int run_all = 0;
int run_sched_test = 0;
int run_locks_test = 0;
int run_yield_test = 0;
int run_call_test = 0;
int run_ipi_test = 0;
int run_mem_test = 0;
int run_pfault_test = 0;

int
parse_args(int argc, char *argv[])
{
  int c = 1;

  if (argc == 1)
  {
    run_all = 1;
    return 0;
  }
  
  if (strcmp(argv[1],"-l")==0)
  {
    int n;
    if (argc < 3)
    {
      usage(argv[0]);
      return -1;
    }
    
    n = atoi(argv[2]);
    if (n < 1000)
    {
      printf("number of iterations must be at least 1000\n");
      return -1;
    }
    loops = n;
    c += 2;
  }

  if (c == argc)
  {
    run_all = 1;
    return 0;
  }

  while (c < argc)
  {
    if (strcmp(argv[c], "locks")==0)
      run_locks_test = 1;
    
    else if (strcmp(argv[c], "sched")==0)
      run_sched_test = 1;

    else if (strcmp(argv[c], "yield")==0)
      run_yield_test = 1;
    
    else if (strcmp(argv[c], "call")==0)
      run_call_test = 1;
    
    else if (strcmp(argv[c], "pfault")==0)
      run_pfault_test = 1;

    else if (strcmp(argv[c], "mem")==0)
      run_mem_test = 1;

    else if (strcmp(argv[c], "ipi")==0)
      run_ipi_test = 1;

    else
    {
      usage(argv[0]);
      return -1;
    }
    c++;
  }
  return 0;
}


int 
main(int argc, char *argv[])
{

  if (parse_args(argc, argv) < 0)
    return -1;

  sys_perf_dump();
  printf("%d iterations on a %u MHz machine\n", loops, __sysinfo.si_mhz);

  if (run_call_test || run_all) 
    proc_call_tests();
  
  if (run_locks_test || run_all) 
    locks_tests();
  
  if (run_pfault_test || run_all) 
    pfault_test(); 

  if (run_mem_test || run_all) 
    memory_tests(); 
  
  if (run_ipi_test || run_all) 
    ipi_tests(); 
  
  if (run_sched_test)
    sched_test(); 
  
  if (run_yield_test || run_all) 
    yield_tests(); 
  
  return 0;
}


