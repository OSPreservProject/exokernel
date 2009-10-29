
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


/*
 *
 * Copyright (c) 1995 David Mazieres (maziere1@das.harvard.edu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 *  This file is just a wrapper around fgets() to make it usable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/*  Stress-test.  Increase this later. */
#define LINEBUF_SIZE 16

typedef struct {
  char *buf;
  unsigned int size;
  int lineno;
  const char *filename;
  FILE *stream;
  void (*errfun) (const char *, ...);
} Linebuf;

static inline Linebuf *
Linebuf_alloc (const char *filename, void (*errfun) (const char *, ...))
{
  Linebuf *lb;

  if (! (lb = malloc (sizeof (*lb)))) {
    if (errfun)
      (*errfun) ("linebuf (%s): malloc failed\n", lb->filename);
    return (NULL);
  }
  lb->filename = filename;
  if (! (lb->stream = fopen (filename, "r"))) {
    free (lb);
    if (errfun)
      (*errfun) ("%s: %s\n", filename, strerror (errno));
    return (NULL);
  }
  if (! (lb->buf = malloc (lb->size = LINEBUF_SIZE))) {
    if (errfun)
      (*errfun) ("linebuf (%s): malloc failed\n", lb->filename);
    free (lb);
    return (NULL);
  }
  lb->errfun = errfun;
  lb->lineno = 0;
  return (lb);
}

static inline void
Linebuf_free (Linebuf *lb)
{
  fclose (lb->stream);
  free (lb->buf);
  free (lb);
}

static inline void
Linebuf_restart (Linebuf *lb)
{
  clearerr (lb->stream);
  rewind (lb->stream);
  lb->lineno = 0;
}

static inline int
Linebuf_lineno (Linebuf *lb)
{
  return (lb->lineno);
}

static inline char *
getline (Linebuf *lb)
{
  int n = 0;

  lb->lineno++;
  for (;;) {
    /* Read a line */
    if (! fgets (&lb->buf[n], lb->size - n, lb->stream)) {
      if (ferror (lb->stream) && lb->errfun)
	(*lb->errfun) ("%s: %s\n", lb->filename, strerror (errno));
      return (NULL);
    }
    n = strlen (lb->buf);

    /* Return it or an error if it fits */
    if (n > 0 && lb->buf[n-1] == '\n') {
      lb->buf[n-1] = '\0';
      return (lb->buf);
    }
    if (n != lb->size - 1) {
      if (lb->errfun)
	(*lb->errfun) ("%s: skipping incomplete last line\n", lb->filename);
      return (NULL);
    }

    /* Double the buffer if we need more space */
    if (! (lb->buf = realloc (lb->buf, (lb->size *= 2)))) {
      if (lb->errfun)
	(*lb->errfun) ("linebuf (%s): realloc failed\n", lb->filename);
      return (NULL);
    }
  }
}
