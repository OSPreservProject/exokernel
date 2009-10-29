/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
#define FLOATING_POINT 1
*/

int main (int argc, char **argv)
{
  char buf[BUFSIZ];
  FILE *in = stdin, *out = stdout;

#if PIPESTREAMS_EXIST
  if (argc == 2 && !strcmp (argv[1], "-opipe"))
    {
      out = popen ("/bin/cat", "w");
      if (out == NULL)
	{
	  perror ("popen: /bin/cat");
	  exit (0);
	}
    }
  else if (argc == 3 && !strcmp (argv[1], "-ipipe"))
    {
      sprintf (buf, "/bin/cat %s", argv[2]);
      in = popen (buf, "r");
    }
#endif

  {
    char name[50];
    fprintf (out,
	     "sscanf (\"thompson\", \"%%s\", name) == %d, name == \"%s\"\n",
	     sscanf ("thompson", "%s", name),
	     name);
  }

  fputs ("Testing scanf (vfscanf)\n", out);

  fputs ("Test 1:\n", out);
  {
    int n, i;
    char name[50];
#if FLOATING_POINT
    float x;
    n = fscanf (in, "%d%f%s", &i, &x, name);
    fprintf (out, "n = %d, i = %d, x = %f, name = \"%.50s\"\n",
	     n, i, x, name);
#else
    char x[20];
    n = fscanf (in, "%d%s%s", &i, x, name);
    fprintf (out, "n = %d, i = %d, x = %s, name = \"%.50s\"\n",
	     n, i, x, name);
#endif
  }
  fprintf (out, "Residual: \"%s\"\n", fgets (buf, sizeof (buf), in));
  fputs ("Test 2:\n", out);
  {
    int i;
    char name[50];
#if FLOATING_POINT
    float x;
    (void) fscanf (in, "%2d%f%*d %[0123456789]", &i, &x, name);
    fprintf (out, "i = %d, x = %f, name = \"%.50s\"\n", i, x, name);
#else
    char x[20];
    (void) fscanf (in, "%2d%s%*d %[0123456789]", &i, x, name);
    fprintf (out, "i = %d, x = %s, name = \"%.50s\"\n", i, x, name);
#endif
  }
  fprintf (out, "Residual: \"%s\"\n", fgets (buf, sizeof (buf), in));
  fputs ("Test 3:\n", out);
#if FLOATING_POINT
  {
    float quant;
    char units[21], item[21];
    while (!feof (in) && !ferror (in))
      {
	int count;
	quant = 0.0;
	units[0] = item[0] = '\0';
	count = fscanf (in, "%f%20s of %20s", &quant, units, item);
	(void) fscanf (in, "%*[^\n]");
	fprintf (out, "count = %d, quant = %f, item = %.21s, units = %.21s\n",
		 count, quant, item, units);
      }
  }
#endif
  fprintf (out, "Residual: \"%s\"\n", fgets (buf, sizeof (buf), in));

#if PIPESTREAMS_EXIST
  if (out != stdout)
    pclose (out);
#endif

  exit(0);
}
