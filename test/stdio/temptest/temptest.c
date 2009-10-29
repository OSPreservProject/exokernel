#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *files[500];

int main ()
{
  char *fn;
  FILE *fp;
  int i;

  for (i = 0; i < 500; i++) {
#ifdef AEGIS
    fn = __stdio_gen_tempname((const char *) NULL,
	"file", 0, (size_t *) NULL, (FILE **) NULL);
#else
    fn = mktemp(strdup ("file.XXXXXX"));
#endif
    if (fn == NULL) {
      printf ("__stdio_gen_tempname failed\n");
      perror("__stdio_gen_tempname failed");
      exit (1);
    }
    files[i] = strdup (fn);
    printf ("file: %s\n", fn);
    fp = fopen (fn, "w");
    fclose (fp);
  }

  printf ("Removing files\n");

  for (i = 0; i < 500; i++)
    remove (files[i]);

  printf ("Test succeeded\n");
  exit (0);
}
