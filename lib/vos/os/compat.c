
/*
 * this file is created for compatibility with other standard library
 * interfaces such as those offered by libc...
 */

#define inline
#include <vos/malloc.h>
#include <vos/fd.h>
#include <vos/proc.h>
#include <vos/ipc-services.h>


/* should be removed */
void
abort()
{
  exit(-1);
}


int 
__sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, 
         size_t newlen) 
{
  RETERR(V_UNAVAIL);
}


int 
kill(pid_t pid, int signo)
{
  extern int 
    pct(int, int, int, u_int, u_int, void*, pid_t, int *);
  pct(IPC_UNIX_SIGNALS,signo,0,0,0L,0,pid,0L);
  return 0;
}


