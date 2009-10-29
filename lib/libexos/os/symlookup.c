
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

#define _AOUT_INCLUDE_
#include <exos/conf.h>
#include <exos/process.h>
#include <fcntl.h>
#include <nlist.h>
#include <stab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xok/defs.h>
#include <xok/env.h>
#include <sys/exec_aout.h>
#include <xok/mmu.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <exos/mallocs.h>
#include <machine/endian.h>
#define __LDPGSZ        4096

/* bsearch modified to return entry preceding where key would be if it's not
   already there */
const void *
sym_bsearch(key, base0, nmemb, size, compar)
     register const void *key;
     const void *base0;
     size_t nmemb;
     register size_t size;
     register int (*compar) (const void *, const void *);
{
  register const char *base = base0;
  register int lim, cmp;
  register const void *p, *preceding=NULL;
  
  for (lim = nmemb; lim != 0; lim >>= 1) {
    p = base + (lim >> 1) * size;
    cmp = (*compar)(key, p);
    if (cmp == 0)
      return ((void *)p);
    if (cmp > 0) {	/* key > p: move right */
      preceding = p;
      base = (char *)p + size;
      lim--;
    } /* else move left */
  }
  return (preceding);
}

/* compare to nlist structures for qsort and sym_bsearch */
int symcomp(const void* s1, const void *s2)
{
  const struct nlist *nl1 = s1, *nl2 = s2;

  if (nl1->n_type != N_FUN && nl1->n_type != N_SLINE &&
      nl1->n_type != N_TEXT && nl1->n_type != (N_TEXT | N_EXT))
    return -1;
  if (nl2->n_type != N_FUN && nl2->n_type != N_SLINE &&
      nl2->n_type != N_TEXT && nl2->n_type != (N_TEXT | N_EXT))
    return 1;

  if (nl1->n_value == nl2->n_value)
    {
      if (nl1->n_type == N_FUN &&
	  (nl2->n_type == N_TEXT || nl2->n_type == (N_TEXT | N_EXT)))
	return 1;
      if (nl2->n_type == N_FUN &&
	  (nl1->n_type == N_TEXT || nl1->n_type == (N_TEXT | N_EXT)))
	return -1;
      return 0;
    }
  if (nl1->n_value > nl2->n_value) return 1;
  return -1;
}

#define SYMLOOKUP_SYM_LEN 60
char* symlookup(char *fname, u_int val)
{
  int fd;
  struct exec exec;
  struct stat st;
  off_t stroff, symoff;
  size_t strsize;
  struct nlist nl_key;
  const struct nlist *nl_res;
  static int state = 0; /* 0 = not attempted, 1 = failed, 2 = succeded */
  static u_long symsize = 0;
  static caddr_t strtab = 0;
  static struct nlist *symtab = NULL;
  static char sym[SYMLOOKUP_SYM_LEN];
  static char prev_fname[100];

  if (state != 0 && strcmp(prev_fname, fname)) state = 0;

  strcpy(prev_fname, fname);

  if (state == 1) return (NULL);

  if (state == 0)
    {
      state = 1;
      fd = open(fname, O_RDONLY, 0);
      if (fd < 0)  return NULL;
  
      if (lseek(fd, (off_t)0, SEEK_SET) == -1 ||
	  read(fd, &exec, sizeof(exec)) != sizeof(exec) ||
	  N_BADMAG(exec) || fstat(fd, &st) < 0)
	{
	  close(fd);
	  return NULL;
	}

      symoff = N_SYMOFF(exec);
      symsize = exec.a_syms;
      stroff = N_STROFF(exec);
      strsize = st.st_size - stroff;

      /* sanity check values */
      if (symoff > st.st_size || 
	  symoff+symsize > st.st_size || 
	  stroff > st.st_size || 
	  stroff+symsize > st.st_size)
	{
	  close(fd);
	  return NULL;
	}

      /* map in string table */
      if (strtab) __free(strtab);
      strtab = __malloc(strsize);
      if (!strtab)
	{
	  close(fd);
	  return NULL;
	}
      if (lseek(fd, stroff, SEEK_SET) == -1)
	{
	  close(fd);
	  __free(strtab);
	  strtab = NULL;
	  return NULL;
	}

      if (read(fd, strtab, strsize) != strsize)
	{
	  close(fd);
	  __free(strtab);
	  strtab = NULL;
	  return NULL;
	}

      /* read in sym table */
      if (symtab) __free(symtab);
      symtab = __malloc(symsize);
      if (!symtab)
	{
	  close(fd);
	  __free(strtab);
	  strtab = NULL;
	  return NULL;
	}
      if (lseek(fd, symoff, SEEK_SET) == -1 ||
	  read(fd, symtab, symsize) != symsize)
	{
	  close(fd);
	  __free(symtab);
	  __free(strtab);
	  symtab = NULL;
	  strtab = NULL;
	  return NULL;
	}
      close(fd);

      /* sort sym table */
      qsort(symtab, symsize / sizeof(struct nlist), sizeof(struct nlist),
	    symcomp);

      state = 2;
    }

  /* search the table for 'val' */
  nl_key.n_value = val;
  nl_key.n_un.n_strx = 1;
  nl_key.n_type = N_SLINE;
  nl_res = sym_bsearch(&nl_key, symtab, symsize / sizeof(struct nlist),
		       sizeof(struct nlist), symcomp);
  if (nl_res)
    {
      int line = 0;

      if (nl_res->n_type == N_SLINE) line = nl_res->n_desc;
      while (nl_res->n_type != N_FUN && nl_res->n_type != N_TEXT &&
	     nl_res->n_type != (N_TEXT | N_EXT) && nl_res >= symtab)
	nl_res--;
      if (nl_res >= symtab && nl_res->n_un.n_strx)
	{
	  char *colon;

	  strncpy(sym, &strtab[nl_res->n_un.n_strx], SYMLOOKUP_SYM_LEN-1);
	  sym[SYMLOOKUP_SYM_LEN-1] = 0;

	  /* remove symbol type info */
	  colon = strrchr(sym, ':');
	  if (colon) *colon = 0;
	}
      else
	sym[0] = 0;
      if (line)
	{
	  char sline[20];

	  sprintf(sline, ": line #%d", line);

	  if (strlen(sline) + strlen(sym) < SYMLOOKUP_SYM_LEN)
	    strcat(sym, sline);
	}
      if (sym[0]) return sym;
    }
  return NULL;
}
