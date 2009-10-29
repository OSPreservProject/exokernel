#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

int main (int argc, char *argv[]) {
  int i;
  char count[10];

#define ITERS 1000

#if 0
  for (i = 0; i < argc; i++) {
    printf ("argv[%d] = %s\n", i, argv[i]);
  }
#endif

  assert (argc == 2);
  i = atoi (argv[1]);
  assert (i >= 0 && i < ITERS);

  i++;
  if (i == ITERS)
    return 0;

  sprintf (count, "%d", i);
  if (execl (argv[0], argv[0], count, NULL) < 0) {
    printf ("exec failed with errno = %d\n", errno);
    return 0;
  }

  return 0;
}

