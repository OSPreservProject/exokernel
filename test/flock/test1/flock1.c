#include <stdio.h>
#include <fcntl.h>

int open_it(const char *name);
void die(char *);


void die(char *msg) 
{
  fprintf(stderr, "ERROR: %s\n", msg);
  fflush(stderr);
  exit(-1);
}

int open_it(const char *name)
{
#define DOES_NOT_MATTER 0
  int fd;
  fd = open(name, O_RDONLY, DOES_NOT_MATTER);
  if (fd < 0) {
    die("couldnt open the file");
  }

  return fd;
}

int
test1(argc, argv)
	int argc;
	char *argv[];
{
  int fd1, fd2;
  int ret;

  printf("This test requires a file named test.data to be in the current directory.\n");
  fd1 = open_it("./test.data");
  fd2 = open_it("./test.data");

  printf("Obtaining exclusive lock on test.data.\n");
  ret = flock(fd1, LOCK_EX);
  printf("First flock ret = %d (0 on success).\n", ret);
  getchar();
  if (ret != 0) {
    die("The first lock could not be obtained."); 
  }

  printf("Obtaining exclusive lock on same file, but through a different descriptor.\n"
	 "This should block forever.\n"
	 "So, hit Ctrl-C when you are satisfied or bored.\n");

  ret = flock(fd2, LOCK_EX);
  die("Flock returned, but it should have blocked forever!");

  return 1;
}


int
main(argc, argv)
	int argc;
	char *argv[];
{
  return test1(argc, argv);

}
