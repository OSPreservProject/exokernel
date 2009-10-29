#include <stdio.h>
#include <unistd.h>

#include "fd/proc.h"
#include "fd/path.h"
#include <string.h>

#include <xok/sysinfo.h>

#include <sys/mount.h>

#include <fd/mount_sup.h>

extern char *__progname;

void 
usage(void) {
  printf("Usage: %s directory\n",__progname);
  exit(-1);
}

int 
main(int argc, char **argv) {
  int status;
  if (argc != 2) usage();
  else {
    {
      status = unmount(argv[1]);
      if (status != 0) perror(argv[1]);
    }
  }
  return 0;
}

