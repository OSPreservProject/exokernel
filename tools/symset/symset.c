
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __linux__
   #include <elf.h>
#else
   #include <a.out.h>
#endif

#include <nlist.h>
#include <sys/param.h>
#include <sys/types.h>

int
main (int argc, char **argv)
{
  struct nlist nl[argc-1];
  char *nm[argc-1];
#ifdef __linux__
  Elf32_Ehdr ehd
#else
  struct exec ehd;
#endif
  int i;
  int fd;
  int error = 0;

  setvbuf(stdout, NULL, _IOFBF,0); 

  if (argc < 3) {
    fprintf (stderr, "usage: %s exec-file symbol1 [symbol2 ...]\n",
	     argv[0]);
    exit (1);
  }

  /* First, sanity check the file. */
  fd = open (argv[1], O_RDONLY);
  if (fd < 0) {
    perror (argv[1]);
    exit (1);
  }
  if (read (fd, &ehd, sizeof (ehd)) != sizeof (ehd)) {
    perror (argv[1]);
    exit (1);
  }
#ifdef __linux__
  if (!__elf_is_okay__ (&ehd)) {
#else
  if (N_BADMAG (ehd) || N_GETMID (ehd) != MID_I386) {
#endif
    fprintf (stderr, "invalid executable file\n");
    exit (1);
  }
#ifndef __linux__
  if (N_GETFLAG (ehd) & EX_DYNAMIC) {
    fprintf (stderr, "are you giving me a dynamically linked executable??\n");
    exit (1);
  }
#endif
  close (fd);

  for (i = 0; i < argc - 2; i++) {
    char *p = strdup (argv[i+2]);
    nm[i] = nl[i].n_un.n_name = p;
    if (p = strchr (p, '/')) {
      *p++ = '\0';
      nm[i] = p;
    }
  }
  nl[i].n_un.n_name = NULL;
  if (nlist (argv[1], nl) < 0) {
    fprintf (stderr, "nlist failed\n");
    exit (1);
  }
  for (i = 0; i < argc - 2; i++)
    if (nl[i].n_un.n_name && nl[i].n_type)
      printf ("\t.globl\t%s\n\t.set\t%s,0x%lx\n\n", nm[i],
	      nm[i], nl[i].n_value);
    else {
      fprintf (stderr, "%s: undefined\n", nl[i].n_un.n_name ?: "(undef)");
      error = 1;
    }
  exit (error);
}
