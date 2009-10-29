#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <pwd.h>

#include <xok/env.h>

extern char **environ;

char aarray[4096];
char buffer[1024];
int
main (int argc, char **argv)
{
  unsigned int i;
  char **envptr;
  unsigned int *p,*p0;
  int fd;
  int pfd[2];
  int status;
  char line[80];
  printf ("HELLO WORLD pid %d\n",getpid());

#define MALLOCSIZE 8 * 1024 * 1024
  p = p0 = (int *)malloc(MALLOCSIZE);
  printf("malloced %d bytes\n",MALLOCSIZE);
  if (!p) {
    printf("could not malloc %d\n",MALLOCSIZE);
    return -1;
  }
  printf("starting pointer: %p\n",p0);
  for (i = 0; i < MALLOCSIZE/4 ; i++) {
    *p = i;
    /*printf("%p *p: %d\n",p,*p);*/
    p++;
  }
  for (i = 0; i < MALLOCSIZE/4 ; i++) {
    /*printf("%p *p0: %d\n",p,*p0);*/
    if (*p0++ != i) return -2;
  }
  printf("ending pointer: %p\n",p0);

  
  return 0;

  printf("argc: %d\n",argc);
  for (i = 0 ; i < argc; i++) {
    printf("argv[%d] & %08x:",i,(int)&argv[i]);
    printf("\"%s\"\n",argv[i]);
  }
  printf("argv[argc] is%s NULL\n",((argv[argc]) == (char *)0) ? "" : " not");
  printf("dumping environment\n");
  envptr = environ;
  while(*envptr != (char *)0) {
    printf("ENV: \"%s\"\n",*envptr++);
  }
  printf("DONE0\n");
  printf("gets\n");
  gets(line);
  printf("line: %s\n",(char *)line);
  return 0;
    do {
      unsigned char *cp;
      int i;
      printf("&cu.u_argv_lengths;\n");
      cp = (char *)&UAREA.u_argv_lengths;
      for (i = 0; i <  UNIX_NR_ARGV*sizeof(int); i++) {
	printf("%02x ",*cp++);
      }
      printf("\n&cu.u_env_lengths;\n");
      cp = (char *)&UAREA.u_env_lengths;
      for (i = 0; i <  UNIX_NR_ENV*sizeof(int); i++) {
	printf("%02x ",*cp++);
      }
      printf("\n&cu.u_argv_space;\n");
      cp = (char *)&UAREA.u_argv_space;
      for (i = 0; i <  UNIX_ARGV_SIZE; i++) {
	printf("%02x ",*cp++);
      }
      printf("\n&cu.u_env_space;\n");
      cp = (char *)&UAREA.u_env_space;
      for (i = 0; i <  UNIX_ENV_SIZE; i++) {
	if ((int)*cp == 4) continue;

	printf("%3d:%02x ",i,*cp++);
      }
      printf("\n----\n");

    } while(0);

  printf("testing system\n");
  system("ls");

  if ((fd  = open("/dev/console", O_RDONLY, 0)) < 0){
    printf("could not open console errno: %d\n",errno);
  } else {
    write(fd,"console only",12);
    close(fd);
  }
  write(1,"DONE1",6);
  printf("DONE2\n");
  printf("TESTING PIPE\n");

  if (pipe(pfd) < 0) {
    printf("pipe problems, call the plumber\n");
    return (0);
  } else {
    printf("pipe ok, fd0: %d fd1: %d\n",pfd[0],pfd[1]);
  }

  printf("good test\n");
  status = write(pfd[1],"test",5);
  printf("write status %d, errno: %d\n",status,errno);
  status = read(pfd[0],buffer,5);
  printf("read status %d, errno: %d\n",status,errno);
  printf("buffer: %s\n",buffer);

  printf("permissions test\n");
  status = write(pfd[0],"test",5);
  printf("write status %d, errno: %d\n",status,errno);
  status = read(pfd[1],buffer,5);
  printf("read status %d, errno: %d\n",status,errno);
  printf("buffer: %s\n",buffer);
  
  printf("TESTING HUMONGOUS WRITE TO PTY\n");
  for (i = 0; i < 4096; i++) {
    aarray[i] = 'a';
  }
  aarray[4095] = 0;
  write(1,aarray,4096);
  printf("DONE\n");
  return (32);
}
