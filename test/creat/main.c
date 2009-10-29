#include <stdio.h>

int main (int argc, char *argv[]) {
  FILE *f;

  if (argc != 2) {
    printf ("usage: %s file\n", argv[0]);
    return 0;
  }

  f = fopen ("hi", "wb");
  if (!f) {
    perror (NULL);
  } else {
    printf ("ok\n");
  }

  return 0;
}
