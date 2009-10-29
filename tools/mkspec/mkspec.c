
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <netinet/in.h>
#include "./exec.h"
#define	_AOUT_INCLUDE_
#include "./nlist.h"
#else
#include <a.out.h>		/* pulls in nlist.h */
#endif /* __linux */
static int inline  my_min (const int a, const int b) {
  if (a < b) {
    return (a);
  } else {
    return (b);
  }
}

#include <err.h>


char *exclude[] = {"___gnu_compiled_c",
		   "_main",
		   "gcc2_compiled.",
		   "_edata",
		   "_etext",
		   "_end",
		   NULL};

int main (int argc, char *argv[]) {
  register struct nlist *s;
  register caddr_t strtab;
  register off_t stroff, symoff;
  register u_long symsize;
  register int cc;
  size_t strsize;
  struct nlist nbuf[1024];
  struct exec exec;
  struct stat st;
  int fd;
  int pageOffset;

  if ((fd = open (argv[1], O_RDONLY)) < 0) {
    fprintf (stderr, "could not open %s\n", argv[1]);
    return -1;
  }
  if (lseek(fd, (off_t)0, SEEK_SET) == -1 ||
      read(fd, &exec, sizeof(exec)) != sizeof(exec) ||
      fstat(fd, &st) < 0) {
    fprintf (stderr, "Invalid binary file\n");
    return -1;
  }

  if (N_BADMAG (exec) || N_GETMID (exec) != MID_I386) {
    fprintf (stderr, "invalid executable file\n");
    exit (1);
  }
  if (N_GETFLAG (exec) & EX_DYNAMIC) {
    fprintf (stderr, "are you giving me a dynamically linked executable??\n");
    return -1;
  }

  symoff = N_SYMOFF(exec);
  symsize = exec.a_syms;
  stroff = symoff + symsize;

#ifndef __linux__
  /* Check for files too large to mmap. */
  if (st.st_size - stroff > SIZE_T_MAX) {
    fprintf (stderr, "file too large\n");
    return -1;
  }
#endif

  /*
   * Map string table into our address space.  This gives us
   * an easy way to randomly access all the strings, without
   * making the memory allocation permanent as with malloc/free
   * (i.e., munmap will return it to the system).
   */
  strsize = st.st_size - stroff;
  pageOffset = stroff % 4096;
  stroff -= pageOffset;
  strsize += pageOffset; 

  strtab = mmap(NULL, (size_t)strsize, PROT_READ, MAP_SHARED, fd, stroff);
  if (strtab == (char *)-1) {
    warn ("could not mmap string table");
    return -1;
  }
  strtab += pageOffset;
  strsize -= pageOffset; 

  if (lseek(fd, symoff, SEEK_SET) == -1) {
    fprintf (stderr, "could not lseek to symbol table\n");
    return -1;
  }

  while (symsize > 0) {
    int i;
    cc = my_min(symsize, sizeof(nbuf));
    if (read(fd, nbuf, cc) != cc)
      break;
    symsize -= cc;
    for (s = nbuf; cc > 0; ++s, cc -= sizeof(*s)) {
      register int soff = s->n_un.n_strx;

      if (soff == 0 || (s->n_type & N_STAB) != 0)
	continue;
      for (i = 0; exclude[i]; i++) {
	if (!strcmp (&strtab[soff], exclude[i]))
	  goto skip;
      }
      /* hack to avoid symbol with name equal to tmp filename used
	 to build us */
      if (strchr (&strtab[soff], '.') || strchr (&strtab[soff], '/'))
	goto skip;
      if (s->n_type & N_EXT) {
	printf ("\t.globl\t%s\n\t.set\t%s,0x%lx\n\t.weak\t%s\n\n", 
		&strtab[soff], &strtab[soff], s->n_value, &strtab[soff]);
      }
    skip:
    }
  }
  munmap(strtab, strsize);
  return 0;
}
