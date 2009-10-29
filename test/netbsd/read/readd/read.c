#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* tests the behavior of reading . 
 * on a local file systems it should be ok
 * on a remote file system it should fail with EPERM
 */

int main(int argc, char **argv) {
  int fd, ret;
  char c;
  char *file;

  printf("* Tests the behavior of reading a directory \n");
  printf("* on a local file systems it should be ok\n");
  printf("* on a remote file system it should fail with EPERM (1)\n");
  printf("* Usage: %s [directory] (directory \".\" is the default)\n",argv[0]);

  argv++;
  if (*argv) {file = *argv;} else { file = ".";}
  
  fd = open(file,O_RDONLY,0);
  
  if (fd == -1) {
    printf("open \"%s\" failed, errno: %d\n",file,errno);
    exit(-1);
  }
  printf("open \"%s\" successful\n",file);

  ret = read(fd,&c,1);


  if (ret == -1) {
    printf("read 1 failed, errno: %d\n",errno);
    printf("DONE\n");
    exit(-1);
  }
  printf("read 1 successful\n");
  close(fd);
  printf("DONE\n");
  return 0;
}
