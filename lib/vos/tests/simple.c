
#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>
#include <xok/pctr.h>

#include <vos/proc.h>
#include <vos/sbuf.h>

int
main()
{
  sys_perf_timer();
  return 0;
}

