#include <fcntl.h>
#include <stdio.h>
#include <sys/exec_aout.h>
#include <unistd.h>

#define NBPG 0x1000

void usage(char *name) {
  fprintf(stderr, "Usage: %s <filename>\n", name);
  exit(-1);
}

int main(int argc, char **argv) {
  int fd;
  struct exec e;

  if (argc != 2) usage(argv[0]);

  fd = open(argv[1], O_RDONLY, 0);
  if (fd == -1) {
    perror("Error opening file");
    exit(-1);
  }

  if (read(fd, &e, sizeof(e)) != sizeof(e)) {
    perror("Error reading file");
    exit(-1);
  }

  printf("\t.text\n"
	 "\t.fill %ld, 1, 0\n", NBPG-e.a_text-0x20);
  close(fd);
  return 0;
}
