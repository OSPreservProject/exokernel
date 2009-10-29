#include <stdio.h>

#include <xok/network.h>

#include <vos/proc.h>
#include <vos/fd.h>
#include <vos/errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <xok/sysinfo.h>
#include <xok/pctr.h>

#define START start[cpu] = rdtsc()
#define STOP  stop[cpu]  = rdtsc()
#define LOOPS 100000

#define CALC(test_name) \
{ \
  double usec; \
  usec = ((double) (stop[cpu]-start[cpu]) / (LOOPS * __sysinfo.si_mhz)); \
  printf("%14s: %2.3f us, %4.3f cycles\n", \
         test_name, usec, usec * __sysinfo.si_mhz); \
  fflush(stdout); \
}

u_quad_t start[2], stop[2];

int 
main(int argc, char **argv)
{
  int fd, r;
  // int pid;
  int cpu = 0;
  extern int sr_localize;
 
  struct sockaddr_in to, self;
  char addrto[4] = {18,26,4,102};
  char addrself[4] = {18,26,4,82};

  bzero(&self, sizeof(self));
  self.sin_port = htons(12345);
  self.sin_family = AF_INET;
  memcpy(&(self.sin_addr.s_addr),addrself,4);

  bzero(&to, sizeof(to));
  to.sin_port = htons(14567);
  to.sin_family = AF_INET;
  memcpy(&(to.sin_addr.s_addr),addrto,4);

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  printf("socket: fd = %d, errno = %d\n",fd,errno);
  
  r = bind(fd, (struct sockaddr *)&self, sizeof(self));
  printf("bind: r = %d, errno = %d\n",r,errno);

#if 0
  if ((pid = fork_to_cpu(1,-1)) == 0)
  // if ((pid = fork()) == 0)
  {
    int i=0;
    cpu = 1;
    START;
    for(i=0; i<LOOPS; i++) 
    {
      r = sendto(fd, "hello world c", 13, 0, (struct sockaddr*)&to, sizeof(to));
      // env_fastyield(getppid());
      env_fastyield(getpid());
    }
    STOP;
    CALC("sendto_child");
    printf("child: sr_localize %d\n", sr_localize);
    r = close(fd);
  }
  else
#endif
  {
    int i=0;
    cpu = 0;
    START;
    for(i=0; i<LOOPS; i++)
    {
      r = sendto(fd, "hello world p", 13, 0, (struct sockaddr*)&to, sizeof(to));
      // env_fastyield(pid);
      // env_fastyield(getpid());
    }
    STOP;
    CALC("sendto_parent");
    printf("parent: sr_localize %d\n", sr_localize);
    r = close(fd);
  }
  return 0;
}


