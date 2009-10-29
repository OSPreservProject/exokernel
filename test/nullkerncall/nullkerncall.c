#include <stdio.h>
#include <xok/pctr.h>
#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>

int main()
{
  unsigned long long start, stop, i, loopsize = 1000000;
  pctrval v;
  
  sys_null();

  start = __sysinfo.si_system_ticks;

  v = rdtsc();

  for (i=0; i < loopsize; i++)
    sys_null();

  v = rdtsc() - v;

  stop = __sysinfo.si_system_ticks;
  
  printf("%qd cycles per loop\n", v / loopsize);
  printf("%qu total ticks\n", stop - start);
  printf("%f usecs per loop\n",
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("%f cycles per tick\n", v / ((stop - start) * 1.0));
  printf("\n\n");

  printf("Loops: %qu\n", loopsize);
  printf("Start ticks: %qu\n", start);
  printf("Stop ticks:  %qu\n", stop);
  printf("Difference:  %qu\n", stop - start);
  printf("Machine MHz: %u\n", __sysinfo.si_mhz);
  printf("Machine usec/tick: %u\n", __sysinfo.si_rate);
  printf("Ticks per loop: %f\n", (stop - start) / (loopsize * 1.0));
  printf("Usecs per loop: %f\n", 
	 ((stop - start) * __sysinfo.si_rate) / (loopsize * 1.0));
  printf("Cycles per loop: %qu\n", 
	 ((stop - start) * __sysinfo.si_rate * __sysinfo.si_mhz) / loopsize);

  return 0;
}
