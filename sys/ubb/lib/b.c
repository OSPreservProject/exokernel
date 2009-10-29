
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

#include <xok_include/assert.h>
#include "bit.h"

void apply(int n, int b, void *state) {
	if(b == 1)
		printf("bit %d is set!\n", n);
}

int main(void) { 
	Bit_T bv;
	int i, val, n, newp, key, *e;

	n = 100;
 	bv = Bit_new(n);
	
	for(i = 0; i < n; i++) {
		Bit_put(bv, i, 1);
		assert(Bit_count(bv) == (i + 1));
	}
	for(i = 0; i < n; i++) 
		assert(Bit_get(bv, i));

	Bit_clear(bv, 0, n-1);
	for(i = 0; i < n; i++) 
		assert(!Bit_get(bv, i));

	Bit_not(bv, 0, n-1);
	for(i = 0; i < n; i++) 
		assert(Bit_get(bv, i));

	Bit_set(bv, 0, n-1);
	for(i = 0; i < n; i++) 
		assert(Bit_get(bv, i));

	Bit_clear(bv, 0, n-1);
	assert(Bit_run(bv, n) >= 0);
	Bit_set(bv, 0, 0);
	assert(Bit_run(bv, n) < 0);
	assert(Bit_run(bv, n-1) == 1);
	Bit_set(bv, 1, 2);
	assert(!Bit_off(bv, 0, 3));
	assert(Bit_off(bv, 3, n-1));
	assert(Bit_run(bv, n-3) == 3);

	Bit_clear(bv, 0, n-1);
	Bit_set(bv, 10, 20);
	Bit_map(bv, apply, 0);	
	
	Bit_free(&bv);

	return 0;
}
