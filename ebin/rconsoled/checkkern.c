#include <xok/sysinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define MIN(x, y) (((x) > (y)) ? (y) : (x))

void checkkern() {
  int fd;
  char osid[MAXOSID];
  off_t size;

  /* compare the osid in the file to the current osid and return if there's
     no difference */
  bzero(osid, MAXOSID);
  if (access("/boot/.noboot", F_OK) == 0 ||
      (fd = open("/boot/xok.osid", O_RDONLY, 0)) == -1 ||
      (size = lseek(fd, 0, SEEK_END)) == -1 ||
      lseek(fd, 0, SEEK_SET) == -1 ||
      size >= MAXOSID ||
      read(fd, osid, size) != size ||
      close(fd) == -1 ||
      !strcmp(osid, __sysinfo.si_osid))
    return;

  /* try to load the kernel */
  printf("Reloading kernel...\n");
  {
    void loadkern(char*);
    loadkern("/boot/xok");
  }
}
