#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <pwd.h>
#include <errno.h>
#include <sys/env.h>
extern char **environ;
extern pipe_init();

char aarray[4096];
char buffer[1024];
int
main (int argc, char **argv)
{
  int status;
  char line[80];
  status = read(0,line,80);
  if (status <= 0) {
    kprintf("status: %d, errno: %d\n",status,errno);
    exit (status);
  }
  line[status] = 0;
  kprintf("GOING TO WRITE\n");
  printf("LINE: %s\n",line);
  printf ("PID %d\n",getpid());
  printf("DONE\n");
  sleep(2);
  return 0;
}
