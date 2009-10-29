#include <xok/sys_ucall.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <fcntl.h>
#include <exos/process.h>
#include "../../lib/libexos/os/procd.h"

void dumpimage(void) {
  printf("Dump option not supported\n");
}
void signaleverbody(void) {
  int pid,mypid;
  mypid = getpid();

  /* Ignore the SIGHUP we get when our parent shell dies. */
  (void)signal(SIGHUP, SIG_IGN);

  for ( pid = 2; pid < PID_MAX; pid++) {
    if (pid == mypid) continue;
    /* supposed to send SIGTERM to everybody, then after few seconds a SIGKILL
       if they are not dead. */
    kill(pid,SIGTERM);
  }
  sleep(3);
  for ( pid = 2; pid < PID_MAX; pid++) {
    if (pid == mypid) continue;
    /* supposed to send SIGTERM to everybody, then after few seconds a SIGKILL
       if they are not dead. */
    kill(pid,SIGKILL);
  }
  sleep(2);
}

void usage(void) {
  printf("reboot [-nqd] [-i] (-i for running via inetd)\n");
  exit(-1);
}
int main(int argc, char **argv) {
  int opterr,optind;
  int nflag,dflag, qflag,iflag;
  char ch;

  nflag = 0;
  dflag = 0;
  qflag = 0;
  iflag = 0;

  opterr = 0;
  while ((ch = getopt(argc, argv, "nqdi")) != -1)
    switch(ch) {
    case 'i':
      iflag = 1;
      break;
    case 'n':
      nflag = 1;
      break;
    case 'q':
      qflag = 1;
      break;
    case 'd':
      dflag = 1;
      break;
    case '?':
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  if (iflag) {
    int fd = open("/dev/dumb",0);
    dup2(fd,0);
    dup2(fd,1);
    dup2(fd,2);
    if (fork()) return 0;
    sleep(2);
  }

  if(qflag) {
    /* quick ungraceful shutdown */
  } else {
    /* send signals */
    signaleverbody();
  }

  if (dflag) {dumpimage();}
  if (nflag) {sync();}

  sys_reboot();
  return 0;
}
