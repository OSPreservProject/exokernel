#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>


void print_stat(char *,int);

int main() {
  int p[2];
  print_stat("/dev/console",0);
  print_stat("/dev/tty",0);
  print_stat("/dev/null",0);
  pipe(p);
  print_stat("pipe 0",p[0]);
  print_stat("pipe 1",p[1]);
  
  
return 0;
}

void print_stat(char *f, int fd) {
  struct stat statbuf;

  if (fd == 0 && stat(f,&statbuf) < 0) {
      printf("could not stat file %s\n",f);
  } else if (fstat(fd,&statbuf) < 0) {
    printf("could not stat fd %d\n",fd);
  } else {
    printf("file: %s
st_dev:  %5d     st_ino: %5d      st_mode: %5x     st_nlink: %d
st_uid:  %5d     st_gid: %5d      st_rdev: %5d     st_atime: %d
st_size: %5d  st_blocks: %5d   st_blksize: %5d\n
",f,
statbuf.st_dev,
statbuf.st_ino,
statbuf.st_mode,
statbuf.st_nlink,
statbuf.st_uid,
statbuf.st_gid,
statbuf.st_rdev,
statbuf.st_atime,
statbuf.st_size,
statbuf.st_blocks,
statbuf.st_blksize);

  }
}
  
