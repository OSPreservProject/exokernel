#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define TRUNCATE1 5

char *filename = "__truncate_tmp";
void print_stat(char *f, int fd);
void failed(void);

int main(int argc, char **argv) {
  int fd;
  struct stat statbuf;
  
  unlink(filename);
  if ((fd = open(filename,O_RDWR|O_CREAT ,0644)) < 0) {
    printf("coult not create file: %s\n",filename);
    failed();
  }

  if (write(fd,"0123456789",10) != 10) {
    printf("write failed, errno: %d\n",errno);
    failed();
  }
  
  /* check initial size */
  if (fstat(fd,&statbuf) < 0) {
    printf("could not stat fd %d\n",fd);
    failed();
  }
  if (statbuf.st_size != 10) {
    printf("file size (%qd) differs from expected (%d)\n",statbuf.st_size,10);
    failed();
  }

  /* check 0 truncation */
  if (ftruncate(fd,0) < 0) {
    printf("ftruncate failed to trunicate to size %d errno: %d",0,errno);
    failed();
  }
  if (fstat(fd,&statbuf) < 0) {
    printf("could not stat fd %d\n",fd);
    failed();
  }
  if (statbuf.st_size != 0) {
    printf("truncate to 0 size failed file length: %qd expected %d\n",
	   statbuf.st_size,0);
    failed();
  }

  printf("current position after writing 10 and truncating: %qd\n",
	 lseek(fd,0,SEEK_CUR));
  /* bring pointer back */
  lseek(fd,0,0);
  /* put stuff back */
  if (write(fd,"0123456789",10) != 10) {
    printf("write failed, errno: %d\n",errno);
    failed();
  }
  if (fstat(fd,&statbuf) < 0) {
    printf("could not stat fd %d\n",fd);
    failed();
  }
  if (statbuf.st_size != 10) {
    printf("file size (%qd) differs from expected (%d)\n",statbuf.st_size,10);
    failed();
  }


  /* truncate in middle */
  if (ftruncate(fd,TRUNCATE1) < 0) {
    printf("ftruncate failed to truncate to size %d errno: %d",TRUNCATE1,errno);
    failed();
  }
  if (fstat(fd,&statbuf) < 0) {
    printf("could not stat fd %d\n",fd);
    failed();
  }
  if (statbuf.st_size != TRUNCATE1) {
    printf("truncate to half size failed file length: %qd expected %d\n",
	   statbuf.st_size,TRUNCATE1);
    failed();
  }


  /* truncate negative */
  errno = 0;
  if (ftruncate(fd,-10) >= 0) {
    printf("ftruncate succeded with a negative length\n");
    failed();
  }
  /* printf("errno of ftruncate with negative length: %d\n",errno);*/
  if (fstat(fd,&statbuf) < 0) {
    printf("could not stat fd %d\n",fd);
    failed();
  }
  if (statbuf.st_size != TRUNCATE1) {
    printf("truncate after negative size failed file length: %qd expected %d\n",
	   statbuf.st_size,TRUNCATE1);
    failed();
  }

  /* increase size */
  if (ftruncate(fd,10) < 0) {
    printf("ftruncate failed when increasing length, errno: %d\n",errno);
    failed();
  }

  if (fstat(fd,&statbuf) < 0) {
    printf("could not stat fd %d\n",fd);
    failed();
  }
  if (statbuf.st_size != 10) {
    printf("truncate after increasing size failed\n"
	   "file length: %qd expected %d\n\n",
	   statbuf.st_size,10);
    failed();
  }

  close(fd);
  if (unlink(filename) < 0) {
    printf("warning could not unlink file: %s\n",filename);
  }
  printf("Ok\n");
  return 0;
  
  
}

void failed(void) {  
  unlink(filename);
  printf("FAILED\n");
  exit( -1);
}
