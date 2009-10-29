#include <unistd.h>
#include <sys/tty.h>
#include <stdio.h>
#include <xok/sysinfo.h>
#include <errno.h>
#include <signal.h>

#include <sys/param.h>		/* for PCATCH */
#include <exos/signal.h>

#include <sys/wait.h>

#define tsleep(a,b,c,d) ({int r; signals_off(); r = tsleep(a,b,c,d); signals_on(); r;})

static void waitchild(void) {
  int ret;
  int status;
  if ((ret = wait(&status)) < 0) {
    printf("waiting for child failed: %d\n",ret);
    exit (-1);
  }
}

static void assure0(int ret, int should, char *shoulds, int x) {
  if (ret != should) {
    printf("Failed %d != %s (%d) on normal case\n",
	   ret, shoulds, should);
    if (x) exit(-1);
  } else 
    printf("Ok [returned %s (%d)]\n",shoulds,should);
}
#define assure(r,s) assure0(r, s, #s,0)
#define assurex(r,s) assure0(r, s, #s,1)

static void sighandler(int signo) {
  printf("received signal: %d\n",signo);
}

extern int lbolt;
int
main(int argc, char **argv) {
  int ret;
  int ticks;
  struct sigaction sa;

  sa.sa_handler = sighandler;
  sa.sa_mask = 0;
  sa.sa_flags = SA_RESTART;

  ret = sigaction(SIGUSR1, &sa, (struct sigaction *)0);
  printf("sigaction USR1 (RESTART) -> %d\n",ret);

  sa.sa_handler = sighandler;
  sa.sa_mask = 0;
  sa.sa_flags = 0;
  ret = sigaction(SIGUSR2, &sa, (struct sigaction *)0);

  printf("sigaction USR2 (INTR) -> %d\n",ret);

  /* I will wait number of ticks per second */
  ticks = 1000000/__sysinfo.si_rate;

  printf("testing tsleep\n");
  fflush(stdout);
  printf("testing sleep timeout of 1 seconds\n");
  ret = tsleep((void *)0x2,0,"test 1 second",ticks);
  assure(ret,EWOULDBLOCK);

  printf("testing normal use, it should block for 2 seconds then wakeup\n");
  if (fork() == 0) {
    /* child */
    sleep (2);
    wakeup((void*)0x5);
    exit(0);
  } else {
    ret = tsleep((void *)0x5,0,"",0);
    printf("tsleep returns: %d\n",ret);
    assure(ret, 0);
  }
  waitchild();

  printf("testing normal use, it should block for 2 seconds then wakeup\n");
  if (fork() == 0) {
    /* child */
    ret = tsleep((void *)0x6,0,"",4*ticks);
    printf("tsleep returns: %d\n",ret);
    assure(ret, 0);
    exit(0);
  } else {
    sleep (2);
    wakeup((void*)0x6);
  }
  waitchild();

  printf("testing normal use, it should block for 2 seconds then EWOULDBLOCK (%d)\n",
	 EWOULDBLOCK);
  if (fork() == 0) {
    /* child */
    sleep (4);
    wakeup((void*)0x8);
    exit(0);
  } else {
    ret = tsleep((void *)0x8,0,"",2*ticks);
    printf("tsleep returns: %d\n",ret);
    assure(ret, EWOULDBLOCK);
  }
  waitchild();
  
  /* 
   * SIGNALS 
   *
   * SIGUSR1 restartable
   * SIGUSR2 not-restartable
   */
  

  printf("testing signal use, it should block for 4 seconds then EWOULDBLOCK (%d)\n",
	 EWOULDBLOCK);
  /* no PCATCH */
  if (fork() == 0) {
    /* child */
    sleep (2);
    kill(getppid(),SIGUSR1);
    exit(0);
  } else {
    /* no pcatch */
    ret = tsleep((void *)0x8,0,"",4*ticks);
    printf("tsleep returns: %d\n",ret);
    assure(ret,EWOULDBLOCK);
  }
  waitchild();



  printf("testing signal use, it should block for 4 seconds then ERESTART (%d)\n",
	 ERESTART);
  if (fork() == 0) {
    /* child */
    sleep (2);
    kill(getppid(),SIGUSR1);
    exit(0);
  } else {
    ret = tsleep((void *)0x8,PCATCH,"",4*ticks);
    printf("tsleep returns: %d\n",ret);
    assure(ret,ERESTART);
  }
  waitchild();

  printf("testing signal use, it should block for 4 seconds then EINTR (%d)\n",
	 EINTR);
  if (fork() == 0) {
    /* child */
    sleep (2);
    kill(getppid(),SIGUSR2);
    exit(0);
  } else {
    ret = tsleep((void *)0x8,PCATCH,"",4*ticks);
    printf("tsleep returns: %d\n",ret);
    assure(ret,EINTR);
  }
  waitchild();

  printf("Done\n");
  sleep(1);
  return 0;
}
