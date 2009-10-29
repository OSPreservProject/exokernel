#include <string.h>
#include <signal.h>

#include <xok/ipc.h>
#include <xok/sysinfo.h>
#include <xok/env.h>
#include <xok/sys_ucall.h>
#include <xok/kerrno.h>

#include <vos/proc.h>
#include <vos/kprintf.h>
#include <vos/ipc.h>
#include <vos/ipcport.h>
#include <vos/fd.h>
#include <vos/signals.h>
#include <vos/cap.h>
#include <vos/errno.h>


/*
 * test ipcport operations
 */

int 
server(pid_t cltpid)
{
  int r, fd;
  char port[32], t[32];
  void spipe_def(u_int);

  ipcport_create(CAP_USER,cltpid);
  ipcport_name(cltpid, port);
  spipe_def(ipcports[envidx(cltpid)].va);

  fd = open(port,0,0);
  printf("env %d: server: opening ipcport %s: %d\n",getpid(), port, fd);

  sleep(3);
  
  do {
    r = read(fd, t, 6);
    if (r > 0)
    {
      t[r]='\0';
      printf("env %d: server: read %d bytes: %s\n",getpid(),r,t);
    }
    else
      printf("env %d: server: read: %d errno %d\n",getpid(),r,errno);
  } while (r > 0);
  
  fd = close(fd);
  printf("env %d: server: closing ipcport %s: %d\n",getpid(), port, fd);
  return 0;
}

int 
client(pid_t srvpid)
{
  int doneit = 0;
  int fd;
  char port[32], t[32];

  while(env_alive(srvpid))
  {
    Pte pte = ipcports[envidx(srvpid)].pte;

    if (pte != 0 && doneit == 0)
    {
      int cpid;
      int r;
      ipcport_name(srvpid, port);
  
      fd = open(port,0,0);
      printf("env %d: client: opening ipcport %s: %d\n",getpid(), port, fd);

      env_fastyield(srvpid);

      cpid = fork();

      strcpy(t, "hello world");
      r = write(fd, t, 11);
      printf("env %d: client: writing to ipcport %s: %d (%d)\n",
	  getpid(), port, r, errno);

      fd = close(fd);
      printf("env %d: client: closing ipcport %s: %d\n",getpid(), port, fd);

      if (cpid == 0) 
	exit(0);

      doneit = 1;
    }
    env_fastyield(-1);
  }
  return 0;
}


int main()
{
  int r;

#ifdef __SMP__
  if ((r = fork_to_cpu(1,-1)) == 0) /* child */
#else
  if ((r = fork()) == 0) /* child */
#endif
  {
    printf("client running on cpu %d\n",sys_get_cpu_id());
    client(getppid());
  }

  else
  {
    printf("server running on cpu %d\n",sys_get_cpu_id());
    server(r);
    kill(r, SIGKILL);
  }

  return 0;
}


