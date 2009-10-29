
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

#ifdef EXOPC
#include "kernel.h"
#include <xok_include/assert.h>
#else
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#endif
#include "xok_include/assert.h"
#include "arena.h"
#define T Arena_T
#define THRESHOLD 10
struct T {
	T       prev;
	char   *avail;
	char   *limit;
};
union align {
	int     i;
	long    l;
	long   *lp;
	void   *p;
	void    (*fp) (void);
	float   f;
	double  d;
	long double ld;
};
union header {
	struct T b;
	union align a;
};
static T freechunks;
static int nfree;
T 
Arena_new(void)
{
	T       arena = malloc(sizeof(*arena));
	if (arena == NULL)
		return 0;
	arena->prev = NULL;
	arena->limit = arena->avail = NULL;
	return arena;
}
void 
Arena_dispose(T * ap)
{
	assert(ap && *ap);
	Arena_free(*ap);
	free(*ap);
	*ap = NULL;
}

void   *
Arena_alloc(T arena, long nbytes)
{
	assert(arena);
	assert(nbytes > 0);
	nbytes = ((nbytes + sizeof(union align) - 1) /
	            (sizeof(union align))) * (sizeof(union align));
	while (arena->avail + nbytes > arena->limit) {
		T       ptr;
		char   *limit;
		if ((ptr = freechunks) != NULL) {
			freechunks = freechunks->prev;
			nfree--;
			limit = ptr->limit;
		} else {
			long    m = sizeof(union header) + nbytes + 512;
			ptr = malloc(m);
			if (ptr == NULL)
				return 0;
			limit = (char *) ptr + m;
		}
		*ptr = *arena;
		arena->avail = (char *) ((union header *) ptr + 1);
		arena->limit = limit;
		arena->prev = ptr;
	}
	arena->avail += nbytes;
	return arena->avail - nbytes;
}
void   *
Arena_calloc(T arena, long count, long nbytes)
{
	void   *ptr;
	assert(count > 0);
	ptr = Arena_alloc(arena, count * nbytes);
	memset(ptr, '\0', count * nbytes);
	return ptr;
}
void 
Arena_free(T arena)
{
	assert(arena);
	while (arena->prev) {
		struct T tmp = *arena->prev;
		if (nfree < THRESHOLD) {
			arena->prev->prev = freechunks;
			freechunks = arena->prev;
			nfree++;
			freechunks->limit = arena->limit;
		} else
			free(arena->prev);
		*arena = tmp;
	}
	assert(arena->limit == NULL);
	assert(arena->avail == NULL);
}
