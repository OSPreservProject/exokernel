#include <stdio.h>
#include <unistd.h>

#include "fd/proc.h"
#include "fd/path.h"
#include <string.h>
#include <stdlib.h>
#include <xok/sysinfo.h>

#include <sys/mount.h>

#include <fd/mount_sup.h>

extern char *__progname;
extern char *strncpy2 (char *dst, const char *src,size_t n);

int pr_mounts(void);
int isthere_cffs(void);

void 
usage(void) {
  printf("Usage: %s host:path directory\n",__progname);
  printf("Usage: %s DISK# directory\n",__progname);
  printf("Usage: %s -clean #\n",__progname);
  printf("Usage: %s -list\n",__progname);
  printf("Usage: %s -cffs\n",__progname);
  exit(-1);
}
int isnfs = 0;
int 
main(int argc, char **argv) {
  int status;
  int slot;

  if (argc != 3) {
    if (argc == 2 && !strcmp(argv[1],"-list")) {
      return pr_mounts();
    } else if (argc == 2 && !strcmp(argv[1],"-cffs")) {
      return isthere_cffs();
    } else usage();
  }
  if (argc == 3 && !strcmp(argv[1],"-clean")) {
    slot = atoi(argv[2]);
    printf("clearing slot: %d\n",slot);
    putslot(slot);
  }
  status = mount(argv[2],argv[1]);
  if (status != 0) perror(argv[2]);

  return status;
}

int
isthere_cffs(void) {
  int i;
  int count = 0;
  mount_entry_t *m;
  m = global_ftable->mt->mentries;
  for (i = 0; i < NR_MOUNT; i++) {
    if (m->inuse) {if (m->to.op_type == CFFS_TYPE) return 1;}
    m++;
  }
  return 0;
}
