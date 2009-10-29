#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>

#ifndef EXOPC
#define kprintf printf
#endif /* !EXOPC */

void print_stat(char *,int);
void print_statbuf(char *f, struct stat *statbuf);

int main(int argc, char **argv) {
  int p[2];
  argv++;
  if (*argv) {
    while(*argv) {
#if 1
      print_stat(*argv,-1);
	argv++;
	continue;
#else
  int fd;
  kprintf("VIA OPEN\n");
      if ((fd = open(*argv,0,0)) < 0) kprintf("could not open: %s errno: %d\n",*argv,errno);
      else {
	struct stat statbuf;
	kprintf("OPENED\n");
	fstat(fd,&statbuf);
	kprintf("PRINT_STATBUF\n");
	print_statbuf(*argv,&statbuf);
	kprintf("closing\n");
	close(fd);
	kprintf("done\n");
      }
      argv++;
#endif
    }
    return 0;
  }

  print_stat("/dev/console",-1);
  print_stat("/dev/tty",-1);
  print_stat("/dev/null",-1);
  pipe(p);
  print_stat("pipe 0",p[0]);
  print_stat("pipe 1",p[1]);
  print_stat("stdout",1);
  print_stat("stdin",0); 
  
return 0;
}

void print_stat(char *f, int fd) {
  struct stat statbuf;
  memset((char *)&statbuf,0xff,sizeof statbuf);

  printf("sizeof struct stat: %d\n",sizeof(struct stat));

  if (fd == -1) {
    if (stat(f,&statbuf) < 0) {
      printf("could not stat file %s\n",f);
      return;
    }
  } else {

    if (fstat(fd,&statbuf) < 0) {
      printf("could not stat fd %d\n",fd);
      return;
    }
  }
  print_statbuf(f,&statbuf);
}

#if 0
void print_statbuf(char *f, struct stat *statbuf) {
    printf("file: %s
st_dev:   0x%-8x st_ino:    %-6d     st_mode: 0x%4x st_nlink: %d
st_rdev:  0x%-8x st_uid:    %-6d     st_gid:      %-6d    
st_atime: 0x%-8lx st_mtime:  0x%-8lx st_ctime:    0x%-8lx
st_size:  %-8qd   st_blocks: %-8qd   st_blksize: %5d\n
",
f,
statbuf->st_dev,
statbuf->st_ino,
statbuf->st_mode,
statbuf->st_nlink,
statbuf->st_rdev,
statbuf->st_uid,
statbuf->st_gid,
statbuf->st_atime,
statbuf->st_mtime,
statbuf->st_ctime,
statbuf->st_size,
statbuf->st_blocks,
(int)statbuf->st_blksize);

}  
#endif

