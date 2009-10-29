#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

char *dir = ".";
#define GDSZ 2048

#if 0
#ifdef EXOPC
extern int getdirentries();
#endif /* EXOPC */
#endif
void print_entry(struct dirent *d) {
  static int total=0;
  printf("total: %4d ino: %6d type: %2d rlen: %3u nlen: %2u name: %s\n",
	 total,d->d_fileno, d->d_type, d->d_reclen, d->d_namlen, d->d_name);
  total += d->d_reclen;
}

int main(int argc, char **argv) {
  int fd,length,l;
  char buf0[GDSZ];
  char *buf = buf0;
  long basep;
  int limit = 7;
  int internallimit;

  if (argc > 1) { dir = argv[1];}
  printf("using directory: %s\n",dir);
  if ((fd = open(dir,O_RDONLY,0)) < 0) {
    printf("could not open %s\n",dir);
    return -1;
  }
  while((length = getdirentries(fd, buf, GDSZ, &basep)) > 0 && limit-- > 0) {
    printf("LENGTH: %d BASEP: %d\n",length,(int)basep);
    internallimit = 40;
    while(length > 0 && internallimit-- > 0) {
      print_entry((struct dirent *)buf);
      l = ((struct dirent *)buf)->d_reclen;
      buf += l;
      length -= l;
    }
    buf = buf0;
  }
  if (length == -1) {
    printf("length == -1 , errno: %d\n",errno);
    return -1;
  }
  return 0;
}
