#include <sys/types.h>
#include <sys/time.h>
#include <memory.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/wait.h>
#include <exos/process.h>
int open_slave(char *ptyname);
int open_master(char **outptyname);

#define NR_PTY 8

#define BSIZE 100
unsigned char buffer[BSIZE];

int fork_execvp(const char *path, char *const argv[]);

int
main (int argc, char **argv)
{
  int confd,ptyfd,ttyfd;
  int status,len;
  char *ptyname;
  argc--;
  argv++;
  if (! *argv) {
    printf("usage: kbdsh program arguments\n");
    exit(-1);
  }

  if ((confd = open("/dev/console",O_RDWR,0644)) < 0) {
    printf("could not open /dev/console\n");
    exit (-1);
  }
    
  if ((ptyfd = open_master(&ptyname)) == -1) {
    printf("could not open master pty\n");
    return -1;
  }
  if ((ttyfd = open_slave(ptyname)) == -1) {
    printf("could not open slave of %s\n",ptyname);
    close(ptyfd);
    return -1;
  }


  if (!fork()) {
    /* child */
    close(ptyfd);
    close(confd);
    dup2(ttyfd,0);
    dup2(ttyfd,1);
    dup2(ttyfd,2);
    kprintf("starting: %s\n",*argv);
    status = execvp(*argv, argv);
    printf("execvp %s failed %d\n",*argv, status);
    exit(-1);
    
  } else {
    /* parent */
    struct timeval t = { 1 , 0};
    close(ttyfd);
    kprintf("parent\n");
    for(;;) {
      fd_set fdset;
      FD_ZERO(&fdset);
      FD_SET(ptyfd,&fdset);

      if ((len = read(confd,buffer,BSIZE))) {
	int i;
	/* remap \n to \r */
	for (i = 0; i < len ; i++) {
	  if (buffer[i] == 13) {
	    buffer[i] = 10;
	  }
	}
	write(confd,buffer,len);
	write(ptyfd,buffer,len);
      }
      if (select(32,&fdset,0,0,&t)) {
	len = read(ptyfd,buffer,BSIZE);
	buffer[len] = 0;
	write(confd,buffer,len);
      }
      if (wait4(0,&status,WNOHANG,0)) {
	kprintf("child returned: %d\n",status);
	exit(0);
	kprintf("BAD\n");
      }
      yield(-1);
    }
  }
  return 0;
}

/* opens a master pty, sets fd to the opened fd, and returns
   a pointer to the ptyname */
int
open_master(char **outptyname) {
  int fd;
  static char ptyname[] = "/dev/ttya";

  while((fd = open(ptyname,O_RDWR,0700)) < 0) {
      ptyname[8]++;
      if(ptyname[8] > ('a' + NR_PTY - 1)) {
	  /* we are wrapping around because we are out of terminals 
	   * trying again 
	   */
	  ptyname[8] = 'a';
	  sleep(10);
      }
  }
  if (fd) {
    *outptyname = ptyname;
    return fd;
  } else {
    return -1;
  }
}


/*  open the slave pty */
int 
open_slave(char *ptyname) {
    int fd;
    /* open pty's here */
    ptyname[5] = 'p';
#ifdef PTYDEBUG
    printf("start: initialize_input, opening %s\n",ptyname);
#endif
    fd = open(ptyname,O_RDWR,0644);
    if (fd > 0) {
#ifdef PTYDEBUG
	printf("Ok initialize_input\n\n");
#endif
    } else {
	printf("error opening pty: %s, open returned %d, errno: %d\n",ptyname,fd,errno);
	kprintf("error opening pty: %s, open returned %d, errno: %d\n",ptyname,fd,errno);
	
	return -1;
    }
       
    return fd;
}
