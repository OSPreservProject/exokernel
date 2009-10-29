
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

#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <unistd.h>

#ifndef JJ
// #include <xok/pmap.h>
#endif

#include <ubb/lib/bit.h>
#include <ubb/lib/mem.h>

#define T Bit_T
struct T {
	int     length;
	unsigned char *bytes;
	unsigned long *words;
};

#define BPW (8*sizeof (unsigned long))
#define nwords(len) ((((len) + BPW - 1)&(~(BPW-1)))/BPW)
#define nbytes(len) ((((len) + 8 - 1)&(~(8-1)))/8)
#define setop(sequal, snull, tnull, op) \
	if (s == t) { assert(s); return sequal; } \
	else if (s == NULL) { assert(t); return snull; } \
	else if (t == NULL) return tnull; \
	else { \
		int i; T set; \
		assert(s->length == t->length); \
		set = Bit_new(s->length); \
		for (i = nwords(s->length); --i >= 0; ) \
			set->words[i] = s->words[i] op t->words[i]; \
		return set; }
unsigned char msbmask[] =
{
    0xFF, 0xFE, 0xFC, 0xF8,
    0xF0, 0xE0, 0xC0, 0x80
};
unsigned char lsbmask[] =
{
    0x01, 0x03, 0x07, 0x0F,
    0x1F, 0x3F, 0x7F, 0xFF
};
static T 
copy(T t)
{
	T       set;
	assert(t);
	set = Bit_new(t->length);
	if (t->length > 0)
		memcpy(set->bytes, t->bytes, nbytes(t->length));
	return set;
}

void *
Bit_handle(T t, unsigned bytes) 
{
	/* make sure it is as large as we think. */
	assert(bytes == nbytes(t->length));
	return t->bytes;
}
void 
Bit_copyin(T t, void *m, unsigned bytes) 
{
	assert(bytes == nbytes(t->length));
	memcpy(t->bytes, m, nbytes(t->length));
}

T 
Bit_new(int length)
{
	T       set;
	assert(length >= 0);
	NEW(set);
	if (length > 0)
		set->words = CALLOC(nwords(length),
		    sizeof(unsigned long));
	else
		set->words = NULL;
	set->bytes = (unsigned char *) set->words;
	set->length = length;
	return set;
}


T 
Bit_build(int length, char* bytes)
{
	T set; 
	assert(length >= 0);
	assert(bytes || length == 0);
	NEW(set);
	set->words = (unsigned long*)bytes;
	set->bytes = (unsigned char*)bytes;
	set->length = length;
	return (set);
}


void 
Bit_free(T * set)
{
	assert(set && *set);
	FREE((*set)->words);
	FREE(*set);
}

int 
Bit_length(T set)
{
	assert(set);
	return set->length;
}
int 
Bit_count(T set)
{
	int     length = 0, n;
	static char count[] =
	{
	    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
	assert(set);
	for (n = nbytes(set->length); --n >= 0;) {
		unsigned char c = set->bytes[n];
		length += count[c & 0xF] + count[c >> 4];
	}
	return length;
}
int 
Bit_get(T set, int n)
{
	assert(set);
	assert(0 <= n && n < set->length);
	return ((set->bytes[n / 8] >> (n % 8)) & 1);
}

int 
Bit_off(T set, int lb, int ub)
{
	int i;

	for(i = lb; i < ub; i++)
		if(Bit_get(set, i))
			return 0;
	return 1;
}
int 
Bit_run(T set, int start, int n)
{
  int ret;
  assert(set);
  ret = Bit_run_bounded(set, start, set->length-1, n);
  if ((ret >= 0) || (start == 0))	/* success || done anyway */
    return ret;
  return Bit_run_bounded(set, 0, start + n - 2, n);
}
int 
Bit_run_bounded(T set, int start, int end, int n)
{
	int     i, len, j;

	assert(set);
	len = set->length;
	assert(end <= len);
	assert(start >= 0);
	assert(n <= end - start + 1);
	for (i = start; i <= end - n + 1; i++) {
		/* find the first run. */
		if (!Bit_get(set, i)) {
			/* see if this is a run. */
			for (j = i + 1; j < (i + n); j++)
				if (Bit_get(set, j))
					break;

			/* was the run long enough? */
			if (j == (i + n))
				return i;
			/* nope: skip over it. */
			i = j;
		}
	}
	return -1;		/* not found. */
}


int 
Bit_put(T set, int n, int bit)
{
	int     prev;
	assert(set);
	assert(bit == 0 || bit == 1);
	assert(0 <= n && n < set->length);
	prev = ((set->bytes[n / 8] >> (n % 8)) & 1);
	if (bit == 1)
		set->bytes[n / 8] |= 1 << (n % 8);
	else
		set->bytes[n / 8] &= ~(1 << (n % 8));
	return prev;
}
void 
Bit_set(T set, int lo, int hi)
{
	assert(set);
	assert(0 <= lo && hi < set->length);
	assert(lo <= hi);
	if (lo / 8 < hi / 8) {
		set->bytes[lo / 8] |= msbmask[lo % 8];
		{
			int     i;
			for (i = lo / 8 + 1; i < hi / 8; i++)
				set->bytes[i] = 0xFF;
		}
		set->bytes[hi / 8] |= lsbmask[hi % 8];
	} else
		set->bytes[lo / 8] |= (msbmask[lo % 8] & lsbmask[hi % 8]);
}
void 
Bit_clear(T set, int lo, int hi)
{
	assert(set);
	assert(0 <= lo && hi < set->length);
	assert(lo <= hi);
	if (lo / 8 < hi / 8) {
		int     i;
		set->bytes[lo / 8] &= ~msbmask[lo % 8];
		for (i = lo / 8 + 1; i < hi / 8; i++)
			set->bytes[i] = 0;
		set->bytes[hi / 8] &= ~lsbmask[hi % 8];
	} else
		set->bytes[lo / 8] &= ~(msbmask[lo % 8] & lsbmask[hi % 8]);
}
void 
Bit_not(T set, int lo, int hi)
{
	assert(set);
	assert(0 <= lo && hi < set->length);
	assert(lo <= hi);
	if (lo / 8 < hi / 8) {
		int     i;
		set->bytes[lo / 8] ^= msbmask[lo % 8];
		for (i = lo / 8 + 1; i < hi / 8; i++)
			set->bytes[i] ^= 0xFF;
		set->bytes[hi / 8] ^= lsbmask[hi % 8];
	} else
		set->bytes[lo / 8] ^= (msbmask[lo % 8] & lsbmask[hi % 8]);
}
void 
Bit_smap(T set, int lb, int ub,
    void apply(int n, int bit, void *cl), void *cl)
{
	int     n;

	assert(set);
	for (n = lb; n <= ub; n++)
		apply(n, ((set->bytes[n / 8] >> (n % 8)) & 1), cl);
}

void 
Bit_map(T set, void apply(int n, int bit, void *cl), void *cl)
{
	assert(set);
	return Bit_smap(set, 0, set->length, apply, cl);
}

int 
Bit_eq(T s, T t)
{
	int     i;
	assert(s && t);
	assert(s->length == t->length);
	for (i = nwords(s->length); --i >= 0;)
		if (s->words[i] != t->words[i])
			return 0;
	return 1;
}
int 
Bit_leq(T s, T t)
{
	int     i;
	assert(s && t);
	assert(s->length == t->length);
	for (i = nwords(s->length); --i >= 0;)
		if ((s->words[i] & ~t->words[i]) != 0)
			return 0;
	return 1;
}

int 
Bit_overlap(T s, T t)
{
	int     i;
	assert(s && t);
	assert(s->length == t->length);
	for (i = nwords(s->length); --i >= 0;)
		if ((s->words[i] & t->words[i]) != 0)
			return 1;
	return 0;
}
int 
Bit_lt(T s, T t)
{
	int     i, lt = 0;
	assert(s && t);
	assert(s->length == t->length);
	for (i = nwords(s->length); --i >= 0;)
		if ((s->words[i] & ~t->words[i]) != 0)
			return 0;
		else if (s->words[i] != t->words[i])
			lt |= 1;
	return lt;
}
T 
Bit_union(T s, T t)
{
	setop(copy(t), copy(t), copy(s), |)
}
T 
Bit_inter(T s, T t)
{
	setop(copy(t),
	    Bit_new(t->length), Bit_new(s->length), &)
}
T 
Bit_minus(T s, T t)
{
	setop(Bit_new(s->length),
	    Bit_new(t->length), copy(s), &~)
}
T 
Bit_diff(T s, T t)
{
	setop(Bit_new(s->length), copy(t), copy(s), ^)
}
