
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

#ifndef __SET_H__
#define __SET_H__
#include "list.h"

/* Things are a bit different here: sets are allocated from
   an arena that we deallocate intantaneously.  If you want
   persistence, you have to call set_persist(s).
*/

/* 
 * Make a preprocessor to automatically parameterize sets,
 * ostensibly user defined.   This really should be cleaner.
 */

#include <xn-struct.h>
typedef struct xn_update T;

static inline int T_cmp(T v1, T v2) {
	if(v1.db != v2.db)
		return v1.db - v2.db;
	else if(v1.type != v2.type)
		return v1.type - v2.type;
	else
		return 0;
}

/* (Implementation: expose for speed): Entries are stored in sorted order. */
typedef struct set_elm {
        T val;
#       define s_val(s) ((s)->val)

        LIST_ENTRY(set_elm)     next;
} *E;

typedef struct set {
        LIST_HEAD(slist, set_elm) set;  /* (sorted) list of elements. */
        int n;                          /* number of elements in the set. */
} *Set;

/* Make val into a singleton set. */
Set set_single(T v);

/* New set. */
Set set_new(void);

/* Free all memory of set module. */
void set_free(void);

/* Returns the difference of s1 and s2 */
Set set_diff(Set s1, Set s2);

/* s1 == s2 */
int set_eq(Set s1, Set s2);

/* Create a duplicate of s1. */
Set set_dup(Set s1);

/* Return (s1 U s2) */
Set set_union(Set s1, Set s2);

/* Create a new set of the elements in s for which map(e) is 1. */
Set set_map(Set s, int (*map)(T e));

/* apply the function pointer to all elements in s. */
int set_apply(Set s, int (*map)(T e));

/* (s eq {}) */
int set_empty(Set s);

void set_print(char *msg, Set s);

/* e in s? */
int set_member(Set s, T e);

Set set_add(Set s, T e);

int set_size(Set s);

/* Is sub fully contained by s? */
int set_issubset(Set s, Set sub);

/* Does { s1 - s2 } = nil? */
int set_isdisjoint(Set s1, Set s2);

#endif /* __SET_H__ */
