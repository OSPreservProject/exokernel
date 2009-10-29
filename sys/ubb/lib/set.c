
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

#include "set.h"
#include "list.h"
#include "demand.h"

#define nil ((void *)0)

#define MAXELEM (1024 * 16)
static long set_heap[MAXELEM];
static char *hp = (void *)&set_heap[0];
	
static void *salloc(unsigned n) {
	char *p;

	p = hp;
	hp += n;
	if(hp >= (char *)&set_heap[MAXELEM])
		fatal(Out of memory);
	return p;
}

static struct set_elm *se_alloc(T val) {
	E e;

	e = salloc(sizeof *e);
	e->val = val;
	return e;
}

static Set s_alloc(void) {
	Set s;
	
	s = salloc(sizeof *s);
	s->n = 0;
	l_init(s->set);
	return s;
}

/* Destructively add e as last element of s. */
static Set s_append(Set s, T e) {
	E l, p, elm;

	if(!s)
		s = s_alloc();

	/* find sorted place for it. */
	for(p = 0, l = l_first(s->set); l; p = l, l = l_next(l, next)) {
		if(T_cmp(e, s_val(l)) == 0)
			return s;
		else if(T_cmp(e, s_val(l)) < 0)
			break;
	}
	elm = se_alloc(e);
	s->n++;

	if(!p)
		l_enq(s->set, elm, next);
	else
		l_enq_after(p, elm, next);
	return s;
}

Set set_add(Set s, T e) {
	return s_append(s, e);	
}

/* 
 * Apply apply() to each element in s until it returns 0 or we run
 * out of elements. 
 */
inline int set_apply(Set s, int (*apply)(T e)) {
	E e;

	for(e = l_first(s->set); e; e = l_next(e, next))
		if(!apply(s_val(e)))
			return 0;
	return 1;
}

/*
 * Apply apply() to each element in s.
 */
inline void set_vapply(Set s, void (*map)(T e)) {
	static inline int apply(T e) {
		map(e);
		return 1;
	}
	(void)set_apply(s, apply);
}

/* 
 * map: create a new set that contains each element in s for which 
 *	map(e) is true. 
 */
Set set_map(Set s, int (*map)(T e)) {
	Set s2;
	static inline void append(T e) {
		if(map(e))
			s2 = s_append(s2, e);
	}

	s2 = s_alloc();
	set_vapply(s, append);
	return s2;
}

/* select elements that are not in s2. */

/* Free all memory consumed by this module. */
void set_free(void) {
	hp = (char *)set_heap;
}

Set set_new(void) {
	return s_alloc();
}

/* Make val into a singleton set. */
Set set_single(T val) {
	Set s;

	s = set_new();
	s_append(s, val);	
	return s;
}

/* Create a replica of s1 */
Set set_dup(Set s1) {
	Set s;
	static inline void append(T e) 
		{ s = s_append(s, e); }

	s = s_alloc();
	set_vapply(s1, append);
	return s;
}

int set_member(Set s, T e) {
	static inline int not_found(T e1) 
		{ return T_cmp(e1, e); }

	return !set_apply(s, not_found);
}

/* Return (s1 U s2); rather slow. */
Set set_union(Set s1, Set s2) {
	static inline void append(T e) {
		if(!set_member(s2, e))
			s_append(s2, e);
	}

	s2 = set_dup(s2);
	set_vapply(s1, append);
	return s2;
}

/* compute { s1 - s2 } */
Set set_diff(Set s1, Set s2) { 
        /* select elements that are not in s2. */
        static inline int select(T e1) {
                static int find(T e2) 
                        { return T_cmp(e1, e2); }
                return set_apply(s2, find);
        }
        return set_map(s1, select);
}

/* s1 == s2, which can be a problem on unsorted sets. */
int set_eq(Set s1, Set s2) {
	E e1, e2;

	if(s1->n != s2->n)
		return 0;

	e1 = l_first(s1->set);
	e2 = l_first(s2->set); 
	for(; e1; e1 = l_next(e1, next), e2 = l_next(e2, next))
		if(T_cmp(s_val(e1), s_val(e2)) != 0)
			return 0;
	return 1;
}

int set_neq(Set s1, Set s2) {
	return !set_eq(s1, s2);
}

int set_empty(Set s) {
	return (s->n == 0);
}

int set_size(Set s) {
	return s->n;
}

void set_print(char *msg, Set s) {
	int print(T e) {
		printf(" db=%ld:n=%ld:type=%d, ", e.db, (long)e.nelem, e.type);
		return 1;
	}
	printf("%s {", msg);
	set_map(s, print);
	printf("}\n");
}

/* Is sub fully contained by s? */
int set_issubset(Set s, Set sub) {
	size_t sn, subn;

	sn = set_size(s);
	subn = set_size(sub);
	if(subn > sn)
		return 0;
	return set_size(set_diff(s, sub)) == (sn - subn);
}

/* Is there any intersection between s1 and s2? */
int set_isdisjoint(Set s1, Set s2) {
	return set_size(set_diff(s1, s2));
}

/* Create dynamically typed sets.  Fun (if a bit slow ;-). */
