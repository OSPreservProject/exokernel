#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <memory.h>
#include <sys/stat.h>

#define CLRBUFFER memset(buffer,0,BUFSIZE)
#define BUFSIZE 80

int main(int argc, char **argv) {
  int fd;
  char templatel[] = "ab.XXX"; /* local template */
  char templater[] = "/home/zw0/hb/un.XXXX"; /* remote template */
  char *string = "abcdefgh1234567890\n";
  char buffer[BUFSIZE];
  int len;
  struct stat statbuf;

  len = strlen(string) + 1;	/* we write strings */
  assert(BUFSIZE >= len);

#ifndef EXOPC
  printf("run this on a machine different than zwolle\n");
#endif
  printf("Doing local tests\n");
  printf("1) Simple unlink test\n");
  /* make sure unlink works */
  fd = mkstemp(templatel);
  if (fd < 0) {
    printf("could not create temporary\n");
    goto err;
  }
  printf("created file: %s\n",templatel);
  if (write(fd,string,len) != len) {
    printf("could not write temporary %s\n",templatel);
    goto err;
  }
  /* verify */
  lseek(fd,0,0);
  CLRBUFFER;
  if (read(fd,buffer,len) != len) {
    printf("could not read temporary %s\n",templater);
    goto err;
  }
  if (strncmp(buffer,string,len)) { /* returns non 0 if different */
    printf("data written and read differs\n");
    printf("string: %s\n",string);
    printf("buffer: %s\n",buffer);
    goto err;
  }
  close(fd);
  unlink(templatel);
  if (stat(templatel,&statbuf) == 0) {
    printf("stat template %s succeded although file was deleted\n",templatel);
    goto err;
  }

  printf("2) Create unlink then reference test\n");
  /* testing unlink file, but still referenced via fd */
  fd = mkstemp(templatel);
  if (fd < 0) {
    printf("could not create temporary\n");
    goto err;
  }
  printf("created file: %s\n",templatel);
  if (write(fd,string,len) != len) {
    printf("could not write temporary %s before unlink\n",templatel);
    goto err;
  }
  if (unlink(templatel) != 0) {
    printf("warning problems unlinking: %d\n",errno);
  }
  if (stat(templatel,&statbuf) == 0) {
    printf("stat template %s succeded although file was deleted\n",templatel);
    goto err;
  }
  if (write(fd,string,len) != len) {
    printf("could not write temporary %s after unlink\n",templatel);
    goto err;
  }
  lseek(fd,0,0);
  CLRBUFFER;
  if (read(fd,buffer,len) != len) {
    printf("could not read temporary %s\n",templater);
    goto err;
  }
  if (strncmp(buffer,string,len)) { /* returns non 0 if different */
    printf("data written and read differs\n");
    printf("string: %s\n",string);
    printf("buffer: %s\n",buffer);
    goto err;
  }
  CLRBUFFER;
  if (read(fd,buffer,len) != len) {
    printf("could not read temporary %s\n",templater);
    goto err;
  }
  if (strncmp(buffer,string,len)) { /* returns non 0 if different */
    printf("data written and read differs\n");
    printf("string: %s\n",string);
    printf("buffer: %s\n",buffer);
    goto err;
  }

  if (fstat(fd,&statbuf) != 0) {
    printf("stat template %s failed (%d) although file is referenced\n",templatel,errno);
    goto err;
  }
  close(fd);
  printf("Passed local tests\n");
#ifndef EXOPC
  printf("Doing remote tests (tmp file on remote volume)\n");
  printf("3) Create unlink then reference remote test\n");
  /* do the remote test on machines that are not xok */
  /* right now 10/3/96 all FS on xok machines are remote anyways */
  fd = mkstemp(templater);
  if (fd < 0) {
    printf("could not create temporary\n");
    goto err;
  }
  printf("created file: %s\n",templater);
  if (write(fd,string,len) != len) {
    printf("could not write temporary %s before unlink\n",templater);
    goto err;
  }
  lseek(fd,0,0);
  CLRBUFFER;
  if (read(fd,buffer,len) != len) {
    printf("1 could not read temporary %s\n",templater);
    goto err;
  }
  if (strncmp(buffer,string,len)) { /* returns non 0 if different */
    printf("data written and read differs\n");
    printf("string: %s\n",string);
    printf("buffer: %s\n",buffer);
    goto err;
  }

  if (unlink(templater) != 0) {
    printf("warning problems unlinking: %d\n",errno);
  }
  if (stat(templater,&statbuf) == 0) {
    printf("stat template %s succeded although file was deleted\n",templatel);
    goto err;
  }

  if (write(fd,string,len) != len) {
    printf("could not write temporary %s after unlink\n",templater);
    goto err;
  }

  if (fstat(fd,&statbuf) != 0) {
    printf("stat template %s failed (%d) although file is referenced\n",
	   templater,errno);
    goto err;
  }
  lseek(fd,len,0);
  if (read(fd,buffer,len) != len) {
    printf("could not read temporary %s\n",templater);
    goto err;
  }
  if (strncmp(buffer,string,len)) { /* returns non 0 if different */
    printf("data written and read differs\n");
    printf("string: %s\n",string);
    printf("buffer: %s\n",buffer);
    goto err;
  }

  close(fd);

#endif

  printf("Ok\n");
  exit(0);
err:
  printf("FAILED\n");
  exit(-1);
}
