
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
 * Track which disk blocks are free and allocated.  Can be called from
 * user-space to get free blocks.
 */
#include <xok/types.h>
#include "xn.h"
#include "virtual-disk.h"
#include "disk.h"

typedef enum { FREE, ALLOCATED } state_t;

#if 0
/* reserve first block */
static unsigned char allocated[XN_NSECTORS] = { ALLOCATED };

static int check_range(state_t val, db_t base, size_t n) {
	int i;

	/* check */
	for(i = base; i < (base + n); i++)
		if(allocated[i] != val)
			return 0;
	return 1;
}

static void mark(state_t val, db_t base, size_t n) {
	int i;

	for(i = base; i < (base + n); i++)
		allocated[i] = val;
}

/* should do this mod the disk size? */
int db_alloc(db_t db, size_t n) {
	if(db >= XN_NSECTORS)
		return 0;
	if(!check_range(FREE, db, n))
		return 0;
	mark(ALLOCATED, db, n);
	return db;
}

int db_isfree(db_t db, size_t n) {
	if(db >= XN_NSECTORS)
		return 0;
	return check_range(FREE, db, n);
}

int db_free(db_t db, size_t n) {
	if(db >= XN_NSECTORS)
		return 0;
	if(!check_range(ALLOCATED, db, n))
		return 0;
	printf("Freeing %ld, n = %d\n", db, n);
	mark(FREE, db, n);
	return db;
}

int db_find_free(db_t hint, size_t n) {
	int i, j;

	printf("hint = %ld\n", hint);

	if(hint == -1)
		hint = random() % 128; 

	for(i = hint % XN_NSECTORS; i < XN_NSECTORS; i++)  {
		if(allocated[i] != FREE)
			continue;
		for(j = 0; j < n; j++)
			if(allocated[i+j] != FREE)
				break;
		if(j == n) {
			printf("Alloc = %ld\n", i);
			return i;
		}
	}

	return XN_CANNOT_ALLOC; 	/* Out of space. */
}
#endif
