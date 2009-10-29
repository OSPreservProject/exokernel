#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <fcntl.h>

int open_it(const char *name);
void die(char *);

#define MAXLINE 200

static void err_doit(int errnoflag, const char *fmt, va_list ap)
{
  int errno_save;
  char buf[MAXLINE];
  
  errno_save = errno;
  vsprintf(buf, fmt, ap);
  if (errnoflag)
    sprintf(buf+strlen(buf), ": %s", strerror(errno_save));
  strcat(buf, "\n");
  fflush(stdout);
  fputs(buf, stderr);
  fflush(NULL);
  return;
}

void err_sys(const char *fmt, ...)
{
  va_list ap;
  va_start(ap,fmt);
  err_doit(1, fmt, ap);
  va_end(ap);
  exit(1);
}




void die(char *msg) 
{
  fprintf(stderr, "ERROR %s\n", msg);
  fflush(stderr);
  exit(-1);
}

int open_it(const char *name)
{
#define DOES_NOT_MATTER 0
  int fd;
  fd = open(name, O_RDONLY, DOES_NOT_MATTER);
  if (fd < 0) {
    err_sys("XXX couldnt open the file");
  }

  return fd;
}

void stage(unsigned int i) {
  printf("stage %d\n", i);
  fflush(stdout);
}

void send(int fd, char c) {
  if (write (fd, &c, 1) < 0)
    err_sys("XXX write error");
}

void get(int fd, char expected) {
  char tmp;
  if (read (fd, &tmp, 1) < 0)
    err_sys("XXX read error");

  if (tmp != expected)
    err_sys("XXX send did not get expected data"); 
}


void parent(int write_fd, int read_fd) 
{
  int fd;
  int ret;

  fd = open_it("test.data");

  if ( (ret = flock(fd, LOCK_EX)) < 0)
    err_sys("XXX flock failed");

  stage(1);
  send(write_fd, 'P'); /* end stage 1 */

  sleep(5); /* give the child time to attempt to flock */

  stage(3);
  if ( (ret = flock(fd, LOCK_UN)) < 0) /* end stage 2 */
    err_sys("XXX flock failed");


  /* this prevents "the parent exiting" as the reason the child
     is able to obtain the lock.  And therefore gives us some 
     assurance that it was b/c the parent unlocked it.
  */
  get(read_fd, 'C'); 
}

void child(int write_fd, int read_fd) 
{
  int ret;
  int fd;


  fd = open_it("test.data"); 

  get(read_fd, 'P');
  stage(2);

  /* now we should get stuck */
  if ( (ret = flock(fd, LOCK_EX)) < 0) /* end stage 2 */
    err_sys("XXX flock failed");

  stage(4);
  send(write_fd, 'C');
}


int
test1(argc, argv)
	int argc;
	char *argv[];
{
  int child_to_parent_pipe[2], parent_to_child_pipe[2];
  pid_t pid;

  if (pipe(parent_to_child_pipe) < 0)
    err_sys("XXX pipe error");

  if (pipe(child_to_parent_pipe) < 0)
    err_sys("XXX pipe error");

  if ( (pid = fork()) < 0)
    err_sys("XXX fork error");

  else if (pid > 0) { /* parent */
    if (close(parent_to_child_pipe[0]) < 0)
      err_sys("XXX close error");
    if (close(child_to_parent_pipe[1]) < 0)
      err_sys("XXX close error");
    parent (parent_to_child_pipe[1], child_to_parent_pipe[0]);
  } else { /* child */
    if (close(parent_to_child_pipe[1]) < 0)
      err_sys("XXX close error");
    if (close(child_to_parent_pipe[0]) < 0)
      err_sys("XXX close error");
    child (child_to_parent_pipe[1], parent_to_child_pipe[0]);
  }
  return 1;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
  printf("The output should be:\n"
	 "stage 1\n"
	 "stage 2\n"
	 "stage 3\n"
	 "stage 4\n"
	 "(IN THIS ORDER). starting the test now...\n");


  fflush(stdout);
  return test1(argc, argv);

}
