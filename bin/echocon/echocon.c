#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <memory.h>
#include <xok/sysinfo.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

extern int fork_execv();
int do_console(int master, int console);
int openpty(int *amaster, int *aslave, char *name, 
	struct termios *termp, struct winsize *winp);

void pchild(int notused) {
  int status;
  kprintf("waiting for child...");
  wait(&status);
  kprintf("done %d",status);
  exit(status);
}

#define SZ 1024
#define MASTERFD 14
#define CONSOLEFD 15

char buf[SZ];
int status;
int main(int argc, char **argv) {
  int console,master,slave;
  if (argc < 1) {
    printf("Usage: echocon program args...\n");
    exit(-1);
  }
  if ((console = open("/dev/console",O_RDWR,0)) < 0) {
    printf("could not open /dev/console\n");
    exit(-1);
  }
  if (openpty(&master,&slave,0,0,0) < 0) {
    printf("out of ptys\n");
    return -1;
  }
  dup2(master,MASTERFD);
  dup2(console,CONSOLEFD);
  close(master);
  close(console);
  fcntl(MASTERFD,F_SETFD,1);
  fcntl(CONSOLEFD,F_SETFD,1);
  dup2(slave,0);
  dup2(slave,1);
  dup2(slave,2);
  signal(SIGCHLD,pchild);
  printf("execing slave... %s\n",argv[1]);
  if (fork_execv(argv[1],&argv[1]) == -1) {
    printf("fork_exec failed\n");
    exit(-1);
  }
  do_console(MASTERFD,CONSOLEFD);
  kprintf("SHOULD NEVER SEE THIS\n");
  assert(0);
  return 0;
}

int do_console(int master, int console) {
  fd_set fdsetw;
  fd_set fdsetr;
  int status;
  FD_ZERO(&fdsetr);
  FD_ZERO(&fdsetw);
  for (;;) {
    FD_SET(master,&fdsetr);
    FD_SET(console,&fdsetr);
    FD_SET(master,&fdsetw);
    FD_SET(console,&fdsetw);
    status = select(32,&fdsetr,&fdsetw,0,0);
    if (status <= 1) continue;
    if (FD_ISSET(console,&fdsetr) && FD_ISSET(master,&fdsetw)) {
      status = read(console,buf,SZ);
      if (status <= 0) {
	printf("1status: %d, errno: %d\n",status,errno);
	break;
      }
      write(master,buf,status);
    } 
    if (FD_ISSET(master,&fdsetr) && (FD_ISSET(console,&fdsetw))) {
      status = read(master,buf,SZ);
      if (status <= 0) {
	printf("2status: %d, errno: %d\n",status,errno);
	break;
      }
      write(console,buf,status);
    } 
  }
  close(console);
  close(master);
  pchild(0);
  assert(0);
  return -1;
}
