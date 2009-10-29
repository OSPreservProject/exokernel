
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
#include <limits.h>
#include <stddef.h>
#include <xok_include/assert.h>
#include <malloc.h>
#endif
#include "mem.h"
#include "table.h"

#define INT_MAX         0x7fffffff      /* max value for an int */

#if 0
struct T {
	int size;
	int (*cmp)(const void *x, const void *y);
	unsigned (*hash)(const void *key);
	int length;
	unsigned timestamp;
	struct binding {
		struct binding *link;
		const void *key;
		void *value;
	} **buckets;
};
#endif

#define T Table_T

static int cmpatom(const void *x, const void *y) {
	return x != y;
}
static unsigned hashatom(const void *key) {
	return (unsigned long)key>>2;
}
T Table_new(int hint,
	int cmp(const void *x, const void *y),
	unsigned hash(const void *key)) {
	T table;
	int i;
	static int primes[] = { 509, 509, 1021, 2053, 4093,
		8191, 16381, 32771, 65521, INT_MAX };
	assert(hint >= 0);
	for (i = 1; primes[i] < hint; i++)
		;
	table = malloc(sizeof (*table) +
		primes[i-1]*sizeof (table->buckets[0]));
	if(!table)
		return 0;
	table->size = primes[i-1];
	table->cmp  = cmp  ?  cmp : cmpatom;
	table->hash = hash ? hash : hashatom;
	table->buckets = (struct binding **)(table + 1);
	for (i = 0; i < table->size; i++)
		table->buckets[i] = NULL;
	table->length = 0;
	table->timestamp = 0;
	return table;
}
void *Table_get(T table, const void *key) {
	int i;
	struct binding *p;
	assert(table);
	assert(key);
	i = (*table->hash)(key)%table->size;
	for (p = table->buckets[i]; p; p = p->link)
		if ((*table->cmp)(key, p->key) == 0)
			break;
	return p ? p->value : NULL;
}
void *Table_put(T table, const void *key, void *value) {
	int i;
	struct binding *p;
	void *prev;
	assert(table);
	assert(key);
	i = (*table->hash)(key)%table->size;
	for (p = table->buckets[i]; p; p = p->link)
		if ((*table->cmp)(key, p->key) == 0)
			break;
	if (p == NULL) {
		if(!NEW(p))
			return (void *)-1;
		p->key = key;
		p->link = table->buckets[i];
		table->buckets[i] = p;
		table->length++;
		prev = NULL;
	} else
		prev = p->value;
	p->value = value;
	table->timestamp++;
	return prev;
}
int Table_length(T table) {
	assert(table);
	return table->length;
}
void Table_map(T table,
	void apply(const void *key, void **value, void *cl),
	void *cl) {
	int i;
	unsigned stamp;
	struct binding *p;
	assert(table);
	assert(apply);
	stamp = table->timestamp;
	for (i = 0; i < table->size; i++)
		for (p = table->buckets[i]; p; p = p->link) {
			apply(p->key, &p->value, cl);
			assert(table->timestamp == stamp);
		}
}
void *Table_remove(T table, const void *key) {
	int i;
	struct binding **pp;
	assert(table);
	assert(key);
	table->timestamp++;
	i = (*table->hash)(key)%table->size;
	for (pp = &table->buckets[i]; *pp; pp = &(*pp)->link)
		if ((*table->cmp)(key, (*pp)->key) == 0) {
			struct binding *p = *pp;
			void *value = p->value;
			*pp = p->link;
			FREE(p);
			table->length--;
			return value;
		}
	return NULL;
}
void **Table_toArray(T table, void *end) {
	int i, j = 0;
	void **array;
	struct binding *p;
	assert(table);
	array = malloc((2*table->length + 1)*sizeof (*array));
	if(!array)
		return 0;
	for (i = 0; i < table->size; i++)
		for (p = table->buckets[i]; p; p = p->link) {
			array[j++] = (void *)p->key;
			array[j++] = p->value;
		}
	array[j] = end;
	return array;
}
void Table_free(T *table) {
	assert(table && *table);
	if ((*table)->length > 0) {
		int i;
		struct binding *p, *q;
		for (i = 0; i < (*table)->size; i++)
			for (p = (*table)->buckets[i]; p; p = q) {
				q = p->link;
				FREE(p);
			}
	}
	FREE(*table);
}
