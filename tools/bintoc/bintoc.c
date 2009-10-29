
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>


extern char *optarg;
extern int optind;

long nskip = 0x20;
int lenmask = 0xfff;

static void
usage (void)
{
  fprintf (stderr, "usage: bintoc [-S] [-s skipbytes] [-l log2_len_allign] "
	   "file.bin array_name\n");
  exit (1);
}

int
main (int argc, char **argv)
{
  unsigned char c;
  int asmout = 0;
  int len = 0;
  int opt;
  FILE *fp;

  setvbuf(stdout, NULL, _IOFBF,0); 

  while ((opt = getopt (argc, argv, "Ss:l:")) != -1) {
    switch (opt) {
    case 'S':
      asmout = 1;
      break;
    case 'l':
      lenmask = atoi (optarg);
      if (lenmask < 0 || lenmask > 16)
	usage ();
      lenmask = (1 << lenmask) - 1;
      break;
    case 's':
      nskip = atoi (optarg);
      break;
    case '?':
    default:
      usage ();
      break;
    }
  }
  argc -= optind;
  argv += optind;

  if (argc != 2)
      usage ();

  if (! (fp = fopen (argv[0], "r"))
      || fseek (fp, nskip, 0)) {
    perror (argv[0]);
    exit (1);
  }

  {				/* fix argv[1] to a plain name */
      char *argv1;
      if (!(argv1 = rindex(argv[1],'/'))) argv1 = argv[1];
      else argv1++;
      argv[1] = argv1;
  }

  if (asmout) {
    printf ("\t.text\n\n\t.global _%s\n_%s:", argv[1], argv[1]);
  }
  else {
    printf ("unsigned char %s[] = {\n", argv[1]);
  }

  while (fread (&c, 1, 1, fp) == 1) {
    if (!(len++ & 0x7)) {
      if (asmout)
	printf ("\n\t.byte");
      else if (len > 1)
	printf (",\n");
    }
    else
      printf (",");
    printf (" 0x%02x", c);
  }
  while (len & lenmask) {
    if (!(len++ & 0x7)) {
      if (asmout)
	printf ("\n\t.byte");
      else if (len > 1)
	printf (",\n");
    }
    else
      printf (",");
    printf (" 0x00");
  }
  if (asmout) {
    printf ("\n\n\t.globl _%s_size\n_%s_size:\n"
	    "\t.long (_%s_size - _%s)\t\t# 0x%x\n",
	    argv[1], argv[1], argv[1], argv[1], len);
  }
  else {
    printf ("\n};\n");
    printf ("const unsigned int %s_size = sizeof (%s);  /* 0x%x */\n",
	    argv[1], argv[1], len);
  }
  return (0);
}
