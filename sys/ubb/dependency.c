
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
 * Track user-defined dependencies.  Is complicated by the fact that 
 * there are ``phantom'' dependencies tracked by the registry itself
 * to allow discretion in writing blocks that have pointers to uninitialized
 * meta data when the pointing object will not survive reboot.
 *
 * Dependencies are directed graphs.  An edge from A->B means that B
 * must be written before A.  The main thing we guard against are cycles,
 * which would not be possible to satisfy.
 *
 * David Karger considers efficient detection of cycles ``interesting.''
 * As a result we use a simple brute force solution that uses standard
 * mark and sweep to detect when it visits a node more than once.  To
 * remove the need to initialize the path we use an epoch number incremented
 * on every addition or deletion.
 *
 * [ The following assumes of the xn registry. While this is a seperate
 * file it is not nicely seperated. ]
 *
 * Before A can be added to the graph we mark traverse its parent, grandparent,
 * etc up to the first node that is persistent, marking the data they contain
 * (if it is in the dependency registry) as having the current epoch number.
 * 
 * Dependencies are created when a persistent xn node becomes tainted.
 * (Bytes to sectors.)
 *
 * Problem: we have to have fine grained tracking of udf updates.  Each
 * udf should track what offset within the type is it at so that we
 * can taint just those sectors that it screws up.
 *
 * NOTE: we can still lose nodes.  Need to check that we have freed
 * everything.
 *
 * Currently is not graceful if memory runs out.
 */

/* Have sectors track where the tainted pieces are, right? */
#ifdef KERNEL
#include "kernel.h"
#else
#include <stdlib.h>
#include <string.h>
#include <xok/types.h>
#endif
#include <demand.h>
#include <dependency.h>
#include <kexcept.h>
#include <list.h>
#include <registry.h>
#include <table.h>
#include <xn.h>

/* The maximum amount of time we are willing to run with interrupts off. */
#define MAX_VISIT	128

#ifndef TEST
#	define FREE(x) free(x)
#	define MALLOC(x) malloc(x)
#else
#	include <kalloc.h>
#	define MALLOC kalloc
#	define FREE kfree
#endif

/* Used for form edge A -> B. */
struct edge {
	struct sec *a;
	LIST_ENTRY(edge)	out_next;	/* List of out edges. */

	struct sec *b;
	LIST_ENTRY(edge)	in_next;	/* List of in edges. */
};

struct sec {
	unsigned epoch;
	sec_t	sector;
	/* 
	 * Addr to write when node is tainted.  Initial nil values can be
	 * overwritten by subsequent calls to depend.  All non-nil ptrs
	 * must be to the same location.
	 */
	t_flag *tf;		

	unsigned ins;	/* Count of incoming edges. */
	unsigned outs;	/* Count of outgoing edges. */ 
	LIST_HEAD(elist, edge)	in, out;

	/* List of blocks that will be freed when this block is written. */
	LIST_HEAD(wlist, xr)	willfree;
};

/* Routines to track active nodes. */
static Table_T sec_table;	/* Hold dependency entries. */

static int insert(sec_t sec, struct sec *s)
        { return Table_put(sec_table, (void *)sec, s) != (void *)-1; }
static struct sec *delete(sec_t sec)
        { return Table_remove(sec_table, (void *)sec); }
static inline struct sec *lookup(sec_t sec) 
	{ return Table_get(sec_table, (void*)sec); }

/*
 * Support routines for manipulating dependency graphs. 
 */

/* Does d have any connections to the outside world? */
static int sec_can_free(struct sec *d) {
	return !d->ins && !d->outs && !l_first(d->willfree);
}

/* Can A be written? */
xn_err_t sec_can_write(sec_t A, struct sec **a) {
	struct sec *s;

	if((s = lookup(A)) && s->outs)
		return 0;
	if(a)
		*a = s;
	return 1;
}

/* Install sector in table if it does not already exist. */
static inline struct sec *sec_install(sec_t sec, t_flag *tf) { 
	struct sec *s;

	if((s = Table_get(sec_table, (void*)sec))) {
		if(!s->tf)
			s->tf = tf;
		else if(tf && s->tf != tf)
			fatal(Attempt to associate with bogus flag);
		return s;
	}

	if(!(s = MALLOC(sizeof *s)))
		return 0;
	LIST_INIT(&s->in);
	LIST_INIT(&s->out);
	LIST_INIT(&s->willfree);
	s->sector = sec;
	s->epoch = s->ins = s->outs = 0;
	s->tf = tf;

	if(insert(sec, s))
		return s;
	FREE(s);
	return 0;
}

/* Remove sector from our universe if there are no references to it. */
static inline int sec_remove(struct sec *s) {
	if(!sec_can_free(s))
		return 0;
	demand(!l_first(s->in) && !l_first(s->out), Should be nil!);
	demand(s->sector, Bogus sector);
	delete(s->sector);
	s->tf = 0;
	FREE(s);
	return 1;
}

/* Add edge a-> b (assumes they have been installed). */
static struct edge *add_edge(struct sec *a, struct sec *b, int *found) {
	struct edge *e;
	struct edge *p, *l;

        /* Find sorted place for it. */
        for(p = 0, l = l_first(a->out); l; p = l, l = l_next(l, out_next)) {
		if(l->b > b)
			break;
		if(l->b == b) {
                       *found = 1;
			return l;
		}
	}

	*found = 0;
	if(!(e = MALLOC(sizeof *e)))
		return 0;

	/* If we make transition from non-existent to tainted, mark. */
	if(a->tf && a->outs == 0)
		(*a->tf)++;
	e->a = a;
	a->outs++;
        if(!p) 	l_enq(a->out, e, out_next);
        else 	l_enq_after(p, e, out_next);

	e->b = b;
	b->ins++;
	l_enq(b->in, e, in_next);

	return e;
}

/* Remove edge a -> b. */
static void remove_edge(struct sec *a, struct sec *b, struct edge *e) {
	t_flag *tf;

	demand(e->a == a, Bogus A);
	demand(e->b == b, Bogus B);

	l_remove(e, out_next);
	l_remove(e, in_next);

	a->outs--;
	b->ins--;
	FREE(e);

	/* If we no longer have outgoing edges, untaint. */
	if(!a->outs && (tf = a->tf))
		(*tf)--;
	sec_remove(a);

	sec_remove(b);
#if 0
	tf = b->tf;
	demand(!tf || !*tf, Should not have been able to write);
#endif
}

/*
 * Detection of cycles using mark and sweep. 
 */
static unsigned epoch;		/* [0 to +inf) used to detect cycles. */
static int no_cycle(struct sec *a, int visited) {
	struct edge *e;

	if(a->epoch == epoch)
		return 0;
	if(visited >= MAX_VISIT) {
		fatal(Too many nodes were visited);
		return 0;
	}

	a->epoch = epoch;
	visited++;
	for(e = l_first(a->out); e; e = l_next(e, out_next))
		if(!(visited = no_cycle(e->b, visited)))
			return 0;	
	return visited;
}

static int has_cycle(struct sec *a) {
	epoch++;
	return !no_cycle(a, 0);	
}

/*
 *  External interface.
 */

/* On allocation don't have to check for cycles. */
xn_err_t sec_depend(sec_t A, sec_t B, t_flag *tf, int alloc) {
	struct sec *a, *b;
	struct edge *e;
	int found;

	/* Make sure we have space for edge. */
	if(!(a = sec_install(A, tf)))
		return XN_CANNOT_ALLOC;
	if(!(b = sec_install(B, 0))) {
		sec_remove(a);
		return XN_CANNOT_ALLOC;
	}

	/* The real work: see if adding edge creates a cycle. */
	if(!(e = add_edge(a, b, &found)))
		return XN_CANNOT_ALLOC;
	if(found || alloc || !has_cycle(a))
		return XN_SUCCESS;
	remove_edge(a, b, e);	/* Remove cycle. */
	return XN_CYCLE;
}

/* Make A depend on B. */
xn_err_t sys_sec_depend(sec_t A, sec_t B, t_flag *tf) {
	return sec_depend(A, B, tf, 0);
}

/* 
 * Called when xr is freed, but depends on B.   A write of A
 * frees the chain recursively: all the dependencies below it
 * are removed && each node is freed.
 */
xn_err_t sec_free(sec_t A, struct xr *xr) {
	return XN_SUCCESS;
}

/* Remove all dependencies on B. */
xn_err_t sec_written(sec_t B) {
	struct sec *b;

	/* no dependencies at all. */
	if(!sec_can_write(B, &b))
		return XN_TAINTED;
	else if(b) {
		struct edge *l, *p;

		/* Remove all edges to b and (free is implicit). */
		for(l = l_first(b->in); l; l = p) {
			p = l_next(l, in_next);
			remove_edge(l->a, b, l);
		}
	}
	demand(!lookup(B), Should have been removed);
	return XN_SUCCESS;
}

/* Write range [B, B+n). */
xn_err_t sec_written_r(sec_t B, size_t n) {
	int i;

	for(i = 0; i < n/8; i++)
		try(sec_written(B+i));
	return XN_SUCCESS;
}

/* Internal: allocation cannot create a cycle. */
xn_err_t sec_depend_r(sec_t A, sec_t B, size_t n, t_flag *tf, int alloc) {
	int i;

	for(i = 0; i < n/8; i++)
		try(sec_depend(A, B+i, tf, alloc));
	return XN_SUCCESS;
}

/* 
 * Make A dependent on [B, B+n).  Assumes memory does not
 * fail, currently.
 */
xn_err_t sys_sec_depend_r(sec_t A, sec_t B, size_t n, t_flag *tf) {
	return sec_depend_r(A, B, n, tf, 0);
}


xn_err_t sys_sec_can_write(sec_t A) {
	return sec_can_write(A, 0) ? 
		XN_SUCCESS : XN_TAINTED;
}

xn_err_t sys_sec_can_write_r(sec_t A, size_t n) {
	int i;

	for(i = 0; i < n/8; i++)
		try(sys_sec_can_write(A+i));
	return XN_SUCCESS;
}

void sec_init(void) {
	sec_table = Table_new(128, 0, 0);	
}

#ifdef TEST

#define EDGES	20

static void make_edges(int n) {
	int i, j;
	t_flag tf[EDGES*2];

	memset(tf, 0, sizeof tf);
	printf("Testing simple linear dependencies.\n");
	for(i = 1; i <= n; i++) {
		printf("i = %d\n", i);
		demand(sys_sec_depend_r(i, i+1,1, &tf[i]) == XN_SUCCESS, sec failed!);
		demand(tf[i], Must be tainted!);
		demand(sys_sec_can_write_r(i+1, 1) == XN_SUCCESS, cannot fail); 
	}

	printf("Exhaustive circularity test.\n");
	for(j = 1; j < n; j++)
		for(i = j; i > 0; i--) {
			printf("(i, j) = (%d, %d)\n", i, j);
			demand(sys_sec_depend_r(j, i, 1, &tf[j]) == XN_CYCLE, Must fail);
			demand(tf[j], Must be tainted!);
			
			demand(sys_sec_can_write(n+1) == XN_SUCCESS, cannot fail); 
		}


	for(i = 1; i < n; i++) {
		demand(tf[i], Must be tainted!);
		demand(sec_written_r(i,1) == XN_TAINTED, Must fail);
		demand(tf[i], Must be tainted!);
	}
	for(i = n+1; i > 0; i--) {
		printf("(i) = %d\n", i);
		demand(sys_sec_can_write_r(i, 1) == XN_SUCCESS, must succeed);
		demand(!tf[i], Cannot be tainted!);
		demand(sec_written_r(i, 1) == XN_SUCCESS, Must succeed);
		demand(!tf[i], Cannot be tainted!);
	}
	demand(!k_memcheck(), memcheck failed!);

	printf("Many to one test.\n");
	for(i = 1; i < n; i++) {
		assert(sys_sec_depend_r(i, n + 1, 1, &tf[i]) == XN_SUCCESS);
		demand(tf[i], Must be tainted!);
	}
	for(i = 1; i < n; i++) {
		demand(!tf[n+1], Cannot be tainted);
		assert(sys_sec_depend_r(n+1, i, 1, &tf[n+1]) != XN_SUCCESS);
		demand(!tf[n+1], Cannot be tainted);
	}
	demand(sec_written_r(n + 1, 1) == XN_SUCCESS, Must succeed);
	demand(!tf[n+1], Cannot be tainted!);
	demand(!k_memcheck(), Should have removed all dependencies);
	
	printf("One to many test.\n");
	for(i = 1; i < n; i++) {
		printf("i = %d\n", i);
		assert(sys_sec_depend_r(n+1, i, 1, &tf[n+1]) == XN_SUCCESS);
		assert(sys_sec_depend_r(n+1, i, 1, &tf[n+1]) == XN_SUCCESS);
		demand(tf[n+1], Must be tainted!);
	}

	for(i = 1; i < n; i++) {
		assert(sys_sec_depend_r(i, n+1, 1, &tf[i]) == XN_CYCLE);
		demand(tf[n+1],  Must be tainted);
		assert(sys_sec_can_write_r(n+1, 1) == XN_TAINTED);
		assert(sys_sec_depend_r(n+1, i, 1, &tf[n+1]) == XN_SUCCESS);
		demand(tf[n+1],  Must be tainted);
	}

	for(i = 1; i < n; i++) {
		assert(sys_sec_can_write_r(n+1, 1) == XN_TAINTED);
		demand(tf[n+1],  Must be tainted);
		assert(sys_sec_depend_r(i, n+1, 1, &tf[i]) == XN_CYCLE);
		demand(!tf[i],  Cannot be tainted);
	}
	for(i = 1; i < n; i++) {
		demand(sec_written_r(i, 1) == XN_SUCCESS, Must succeed);
		demand(!tf[i],  Cannot be tainted);
	}
	demand(sys_sec_can_write_r(n+1, 1) == XN_SUCCESS, Must succeed);
	demand(!tf[n+1],  Cannot be tainted);
	demand(!k_memcheck(), Should have removed all dependencies);

	/* Now all schedules should succeed. */
	for(i = n; i > 1; i--) {
		demand(sys_sec_depend_r(i, i-1, 1, &tf[i]) == XN_SUCCESS, Must succeed);
		demand(tf[i],  Must be tainted);
	}
}

int main() {
	sec_init();
	make_edges(EDGES);
	printf("Success!\n");
	return 0;
}
#endif
