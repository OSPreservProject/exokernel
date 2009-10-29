#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>

#include <signal.h>

/* tests the behavior of lseek . 
 * on a local file systems it should be ok
 * on a remote file system it should fail with EPERM
 */

int main(int argc, char **argv) {
  int fd, ret, pid, status=0, i;
  char c;
  char *file,*cp;
  struct stat sb;

  printf("* Tests the behavior of reading a directory \n");
  printf("* on a local file systems it should be ok\n");
  printf("* on a remote file system it should fail with EPERM (1)\n");
  printf("* Usage: %s [directory] (directory \".\" is the default)\n",argv[0]);

  argv++;
  if (*argv) {file = *argv;} else { file = "foo";}
  
  fd = open(file,O_RDWR|O_CREAT,0666);
  
  if (fd == -1) {
    printf("open \"%s\" failed, errno: %d\n",file,errno);
    exit(-1);
  }
  printf("open \"%s\" successful\n",file);
  unlink(file);

  signal(SIGCHLD,SIG_IGN);
  signal(SIGUSR1,SIG_IGN);
  if ((pid= fork())) {
    /* parent */
#define SIZE 1024*1024*10

    cp = (char *)malloc(SIZE);
    if (cp == 0) {
      printf("could not malloc memory\n");
      status = -1;
      goto done;
    }
    kill(pid,SIGUSR1);
    printf("Writing: %d\n",SIZE);
    ret = write(fd,cp,SIZE);
    printf("wrote: %d\n",ret);
    status = 0;
  } else {
    /* child */
    
    for(i = 0; i < 100; i++) {
      printf("fd: offset: %qd\n",lseek(fd,4096,SEEK_CUR));
      usleep(100000);
    }
  }
  ret = read(fd,&c,1);
  printf("read 1, returned: %d, errno: %d\n",ret,errno);

done:
  sleep(2);
  kill(pid,1);
  fstat(fd,&sb);
  printf("final file size: %qd\n",sb.st_size);
  close(fd);
  return status;
}
