
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
 * Routines to insert and delete dpf filters.  Result is a valid dpf_iptr
 * function that can be called to demux packets.
 *
 * Biggest problem is that our code generation methodology is pretty 
 * exposed: front end has to know about the various optimizations and
 * when their invarients are violated.
 */
#include <dpf/dpf-internal.h>
#if 0
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#endif
#include <xok/printf.h>
#include <xok/malloc.h>
#include <dpf/hash.h>
#include <dpf/demand.h>
#include <xok/sysinfo.h>

Atom dpf_active[DPF_MAXFILT+1];/* pointer to last node in active filters */
Atom dpf_base;           	/* base of the tree */
struct atom dpf_fake_head; 	/* Bogus header node. */
struct action_block *dpf_state_list[DPF_MAXFILT];

static int dpf_overlap; /* did the inserted filter overlap? */
static int verbose;

static Atom dpf_merge(Atom dpf, struct ir *ir, int pid) ;

/* Compute log base 2 of x. */
unsigned log2(unsigned x) {
        unsigned i;

        demand(x && x == (x & -x), x is not a power of two!);
        for(i = 0; x; x >>= 1, i++)
                ;
        return i-1;
}

/* If parent has two or more kids, a is in an orlist. */
inline int orlist_p(Atom a) {
        Atom firstor = a->parent->kids.lh_first;
        return firstor && firstor->sibs.le_next;
}

/* Create an atom: many of these fields can be filled in to 0 by default. */
static Atom mkatom(Atom parent, int pid, struct ir ir) {
	Atom a;

	/* Should have a more efficient allocation mechanism */
	a = (Atom)calloc(1, sizeof *a);
	demand(a, Out of memory);

	LIST_INIT(&a->kids);
	a->parent = parent;
	a->pid = pid;
	a->ir = ir;
	return a;
}

/* returns 1 on equality, -1 on lhs equality, 0 on inequality. */
static int isequal(struct ir *ir1, struct ir *ir2) {
	if(ir1->u.eq.op != ir2->u.eq.op)
		return 0;

	/* is an eq? */
	if(dpf_iseq(ir1)) {
		/* check for structural equality */
		return (ir1->u.eq.offset != ir2->u.eq.offset || 
				ir1->u.eq.mask != ir2->u.eq.mask) ? 
			0 : ((ir1->u.eq.val == ir2->u.eq.val) ? 1 : -1);
	}
	if(dpf_isshift(ir1)) {

	/* is a shift */
	return (ir1->u.shift.shift == ir2->u.shift.shift &&
		ir1->u.shift.offset == ir2->u.shift.offset &&
		ir1->u.shift.mask == ir2->u.shift.mask);
	}
	return 0;
}

/*
 *  Create a string of atoms from IR. Returns the last one and pointer
 * to first. 
 */
static Atom swizzle(struct ir *ir, int pid, Atom *first, int alignment) {
        Atom a, n;

        for(n = *first = a = 0; !dpf_isend(ir); ir++, a = n) {
		n = mkatom(a, 0, *ir);
                if(a)
                        LIST_INSERT_HEAD(&a->kids, n, sibs);
                else
                        *first = n;
		if(dpf_isshift(ir))
			alignment = dpf_compute_alignment(&n->ir, alignment);
		n->ir.alignment = alignment;
        }
	if(!n)
		return 0;
        n->pid = pid;
        return n;
}

static Atom ht_lookup(Ht ht, uint32 val, int *coll_p, Atom **succ) {
	Atom hte, *last;
	int coll;

	for(coll = 0, last = &ht->ht[hash(ht, val)], hte = *last; hte; ) {
		if(hte->ir.u.eq.val == val)
			break;
		last = &hte->next;
		if((hte = hte->next))
			coll = 1;
	}
	if(coll_p)
		*coll_p = coll;
	if(succ)
		*succ = last;
	return hte;
}

static Ht ht_alloc(int n, int nbits) {
	Ht ht;
	int sz;

	/* Compute size in presence of struct hack */
	sz = sizeof *ht + (n - DPF_INITIAL_HTSZ) * sizeof(struct atom);

	ht = (Ht)calloc(1, sz);
        demand(ht, Out of memory);
        ht->htsz = n;
	ht->log2_sz = log2(n);
	ht->nbits = ht->nbits;
	return ht;
}

/*
 * Expand an existing hash table.
 */
static void ht_expand(Atom a) {
	Ht ht, newh;
        int n;
        Atom p, *l;

	ht = a->ht;
	n = ht->htsz * 2;
	newh = a->ht = ht_alloc(n, ht->nbits);

#if 0
	printf("expanding from %d to %d, ent = %d, coll = %d, nbits = %d\n", 
		ht->htsz, n, ht->ent, ht->coll, ht->nbits);
#endif

	newh->ent = ht->ent;
	newh->nterm = ht->nterm;
	newh->term = ht->term;
        for (p = a->kids.lh_first; p; p = p->sibs.le_next) {
                l = &newh->ht[hash(newh, p->ir.u.eq.val)];
		if((p->next = *l))
			newh->coll++;
		*l = p;
        }
        free((void *)ht);
}

/* should we regen? */
static inline int ht_regenp(Ht ht) {
	return ht->state != ht_state(ht);
}

/* Standard thing: hash into ht.  At some point we would rehash. */
static Atom ht_insert(Atom a, int pid, struct ir *ir) {
	Atom hte, *p;
	Ht ht;

	ht = a->ht;
	if(!(hte = ht_lookup(ht, ir->u.eq.val, 0, 0))) { 
		hte = mkatom(a, pid, *ir);
		LIST_INSERT_HEAD(&a->kids, hte, sibs);

		p = &ht->ht[hash(ht, ir->u.eq.val)];

                /* collision */
                if((hte->next = *p))
                        ++ht->coll;
		*p = hte;
		ht->ent++;

                /* Grow if possible */
                if(ht->coll && (ht->ent * 100 >> ht->log2_sz) >= DPF_HT_USAGE) {
                	ht_expand(a);
			ht->state = DPF_REGEN;
		} else if(ht_regenp(ht))
			ht->state = DPF_REGEN;
	}
	return hte;
}


/* 
 * Delete ht entry - complication from the ht organization.  Caller
 * must ensure that refcnt is 0.
 */
static void ht_delete(Atom a, uint32 val, int term) {
	Atom hte, *last;
	Ht ht;
	int coll_p;

	ht = a->ht;
	hte = ht_lookup(ht, val, &coll_p, &last);
	demand(hte, deleting a bogus hte);
	*last = hte->next;
	ht->ent--;

	demand((ht->term + ht->nterm) == a->refcnt, bogus summation!);
	ht->coll -= coll_p;
	demand(ht->coll >= 0, bogus collision index);

	/* regen if we are not about to demote hash table. */
	if(ht->ent > 1 && ht_regenp(ht))
		dpf_compile_atom(a);
}

/* Create a new hash table and insert a as its first entry.  */
static Atom ht_newht(Atom a) {
	Ht ht;
	Atom deq;

	/* make deq node */
	deq = mkatom(a->parent, 0, a->ir);

	/* insert deq in place of a */
	LIST_INSERT_BEFORE(a, deq, sibs);
	LIST_REMOVE(a, sibs);

	/* create a hash table */
	deq->ht = ht = ht_alloc(DPF_INITIAL_HTSZ, deq->ir.u.eq.nbits);
	deq->refcnt = a->refcnt;

	/* Insert a into hash table. */
	ht->ht[hash(ht, a->ir.u.eq.val)] = a;
	ht->ent = 1;
	if(a->kids.lh_first)
		ht->nterm = a->refcnt;
	else
		ht->term = a->refcnt;
	ht->state = DPF_REGEN;

	LIST_INSERT_HEAD(&deq->kids, a, sibs);	
	a->parent = deq;

	dpf_compile_atom(deq->parent);	/* fix parent to jump here. */
	dpf_compile_atom(a); 	/* atom is now a hte: regen. */
	return deq;
}

/* 
 * Insert before or with smaller nbits; used to enforce "longest
 * match" in the presence of atom coalescing.
 */
static void insert_or(Atom a, Atom new) {
	Atom succ;

	new->parent = a->parent;
	for(succ = 0; a; succ = a, a = a->sibs.le_next)
		if(a->ir.u.eq.nbits < new->ir.u.eq.nbits)
			break;
	if(a) {
		LIST_INSERT_BEFORE(a, new, sibs);
		/* a is no longer the first node: regen. */
		if(!succ) {
			dpf_compile_atom(a);

			/* parent jumps to us directly, regen if necessary. */
			dpf_compile_atom(a->parent);
		}
	} else {
		LIST_INSERT_AFTER(succ, new, sibs);
		/* succ is no longer the last atom: regen. */
		if(!a)
			dpf_compile_atom(succ);
	}
}

/*
 * Atom merge rules.  If two atoms: 
 *  - are identical, merge
 *  - are an eq & use the same mask, but compare to a different 
 * 	constants, create a hash table.
 *  - otherwise, iterate to next kid.  If no more, add as an or node.
 */
static Atom merge(Atom a, struct ir *ir, int pid) {
	Atom succ, last, first, hte, first_or;
	int res, alignment;

	alignment = DPF_MSG_ALIGN;
	succ = dpf_base;
	a = a->kids.lh_first;

	for(; a && !dpf_isend(ir); succ = a, a = a->kids.lh_first, ir++) {
		/* Walk down or's, checking for match. */
		for(res = 0, first_or = a; a; succ = a, a = a->sibs.le_next) 
			if((res = isequal(&a->ir, ir)))
				break;
		/* if new alignment, update. */
		if(a && dpf_isshift(&a->ir))
			alignment = a->ir.alignment; // a->ir.u.shift.align;

		/* No matches, insert as an OR branch.  */
		if(!res) {
			last = swizzle(ir, pid, &first, alignment);
			insert_or(first_or, first);
			return last;
		}
		/* Matched: continue to the next node. */
		if(res == 1 && !a->ht)
			continue;

		/* Is a disjunction. */

		/* Create ht and insert a. */
		if(!a->ht)
			a = ht_newht(a);

		/* continue merging, unless there is no more. */
		if((hte = ht_lookup(a->ht, ir->u.eq.val, 0, 0))) {
			a = hte;
		} else {
			/* create an entry for this atom. */
			hte = ht_insert(a, 0, ir);
			/* string together the rest */
			if((last = swizzle(ir+1, pid, &first, alignment))) {
				first->parent = hte;
				LIST_INSERT_HEAD(&hte->kids, first, sibs);
				return last;
			} else {
				hte->pid = pid;
				return hte;
			}
		}
	}

	/* Long filter - extended past last entry. */
	if(!a) {
		/* Merged all the way down. */
		if(dpf_isend(ir)) {
			dpf_overlap = 1;
			return succ;
		} 
		last = swizzle(ir, pid, &first, alignment);
		LIST_INSERT_HEAD(&succ->kids, first, sibs);
		first->parent = succ;
		return last;
	/* Short filter. */
	} else {
		demand(succ, nil filter);
		/* filter overlap? */
		if(succ->pid)
			dpf_overlap = 1;
		else
			succ->pid = pid;
		return succ;
	}
}

/* Scan backwards, genning code for each atom. */
static void instantiate_filter(Atom last) {
	Atom a;

	/* always compile the last atom (at very least is a pid). */
	dpf_compile_atom(last);

	/* 
	 * When refcnt > 1 and pid is non-zero it means this filter
	 * was overlayed on top of a short one and we need to regen.
	 * Otherwise we are in an orlist or in a hash table ---
	 * both of these structures have been recompiled at the point
	 * of modification.
	 */
	for(a = last->parent; a != dpf_base; a = a->parent)
		if(a->refcnt == 1)
			dpf_compile_atom(a);
		/* filter has to handle longest match: regen. */
		else if(a->pid) {
			dpf_compile_atom(a);
			break;
		}
}

/* Merge dpf with trie. */
static Atom dpf_merge(Atom a, struct ir *ir, int pid) {
	Atom p, last;
	Ht ht;

	last = a = merge(a, ir, pid);

	/* incr filter's atom's refcnt. */
	for(last = a; a; a = p) {
		p = a->parent;
		a->refcnt++; 

		/* fix up counts of all hash tables we are in. */
		if(p && (ht = p->ht)) {
			if(a == last)
				ht->term++;
			else
				ht->nterm++;

		}
		/* regen ht if necessary. */
		if((ht = a->ht)) {
			demand((a->ht->term + a->ht->nterm) == a->refcnt, 
				bogus summation!);

			if(ht_regenp(ht))
				dpf_compile_atom(a);
		}
	}

	instantiate_filter(last);
	return last;
}

/* get last element in list; gross. */
static Atom getlast(Atom a) {
	for(a = a->kids.lh_first; a->sibs.le_next; a = a->sibs.le_next);
	return a;
}

/* Delete filter from tree. */
SYSCALL int dpf_delete(unsigned pid) {
	Atom a, p, f, fl[DPF_MAXELEM * 2 + 2], last;
	int i;
	Ht ht;

	if(pid >= DPF_MAXFILT || !(a = dpf_active[pid]))
		return DPF_BOGUSID;
	
	/* nuke the pid. */
	if(!dpf_overlap)
		a->pid = 0;	
	
	/* Scan backwards, decrementing ref counts. */
	for(last = a, i = 0; a != dpf_base; a = p) {
	 	/* All atoms with refcnt = 1 need to be deleted. */
		if(a->refcnt-- == 1)
			fl[i++] = a;

		p = a->parent;

		/* decrement hash table counters as appropriate. */
		if((ht = p->ht)) {
			if(a == last)
				ht->term--;
			else
				ht->nterm--;
		}
		demand(!a->ht || (a->ht->term + a->ht->nterm) == a->refcnt, 
				bogus summation!);
	}
	
	/* 
	 * If the filter had any elements to free, do so.  Note that only
	 * the last atom (going backwards) can be in anything 
	 * interesting (i.e., a hash table or real or list).
	 *
	 * If we delete from an orlist need to regen code for first and 
	 * last elements (if these change).  
	 * 
	 * If we delete from a hash table, need to regen code if it is
	 * demoted.  For performance, we might consider regening if 
	 * collisions are eliminated.
	 */
	if(i-- > 0) {
		int lastor, orlist;
		Atom first;

		f = fl[i];
		p = f->parent;
		lastor = 0; first = 0;

		if((orlist = orlist_p(f))) {
			demand(orlist_p(f), bogus: should be an orlist);

			/* get firstor. */
			first = (p->kids.lh_first != f) ? 0 : f->sibs.le_next;

			lastor = (f->sibs.le_next == 0);
		}
		LIST_REMOVE(f, sibs); /* Remove f from kid list. */

		/* 
	 	 * Check first atom to see if it is in a ht.  If it is remove 
	 	 * and, if necessary, demote. 
	 	 */
		if(p->ht) {
			ht_delete(p, f->ir.u.eq.val, i == 0);
	
			/* If only one unique entry, delete ht. */
			if(p->ht->ent == 1) {
				/* last element in hash table */
				last = p->kids.lh_first;
				last->parent = p->parent;
				LIST_INSERT_BEFORE(p, last, sibs);
				LIST_REMOVE(p, sibs);
				free(p->ht);
				free(p);
				/* regen code for last atom in trie */
				dpf_compile_atom(last);

				/* if parent jumps to ht, regen parent */
				p = last->parent;
				if(p->kids.lh_first == last)
					dpf_compile_atom(p);
				p = 0;
			}
		} else if(orlist) {
			/* was the first or: need to regen new firstor. */
			if(first) {
				dpf_compile_atom(first);
				/* fix up parent; cheaper way, certainly */
				dpf_compile_atom(first->parent);
			}
			/* was last or, need to regen last or */
			else if(lastor)
				/* eek: use tail queue */
				dpf_compile_atom(getlast(p));
		/* is the last in a chain: regen this node. */
		} else 
			dpf_compile_atom(p);

		for(; i >= 0; i--) {
			demand(fl[i]->refcnt == 0, bogus refcnt!);
			free(fl[i]);
		}
	}

	dpf_iptr = dpf_compile(dpf_base);
	dpf_active[pid] = 0;
	dpf_freepid(pid);
	debug_stmt(dpf_check());
	return pid;
}

/* check of 0 length filter */
SYSCALL int dpf_insert(struct dpf_ir *irp) {
	/* scratch memory to hold copied filter */
	struct dpf_ir ir;
	struct ir *r;
	int pid, n, i;
	Atom tail;
	static struct state *laststate = 0;

	/* Make sure these fields are at identical offsets. */
	AssertNow(offsetof(struct atom, ir.u.eq.op) == \
		  offsetof(struct atom, ir.u.shift.op));
	AssertNow(offsetof(struct atom, ir.u.eq.offset) == \
		  offsetof(struct atom, ir.u.shift.offset));
	AssertNow(offsetof(struct atom, ir.u.eq.nbits) == \
		  offsetof(struct atom, ir.u.shift.nbits));
	AssertNow(offsetof(struct atom, ir.u.eq.mask) == \
		  offsetof(struct atom, ir.u.shift.mask));

	/* One-time initialization. */
	if(!dpf_base) {
		dpf_base = &dpf_fake_head;
		dpf_iptr = dpf_compile(dpf_base);
		LIST_INIT(&dpf_base->kids);
		dpf_allocpid();
	}

	debug_stmt(dpf_check());
        if ((pid = dpf_allocpid()) < 0)
		return DPF_TOOMANYFILTERS;

        demand(pid < DPF_MAXFILT, bogus pid);

	/* copy in the header */
        dpf_copyin(&ir, irp, offsetof(struct dpf_ir, ir)); 
	/* copy in the elements */
        dpf_copyin(&ir.ir[0], &irp->ir[0], irp->irn * sizeof irp->ir[0]);

	if(ir.irn >= DPF_MAXELEM)
		return DPF_TOOMANYELEMS;
	else if(!ir.irn)
		return DPF_NILFILTER;

#if 0
	if(ir.version != 0) {
		dpf_freepid(pid);
		pid = ir.version;
		printf("warning: filterid manually set to %d\n", pid);
	}
#endif

	r = &ir.ir[0];

	/* 
	 * Make sure all atoms are well-formed.  quicker to do in single
	 * pass, but what the hell. 
	 */
	for(i = 0, n = ir.irn; i < n; i++) {

		/* 
		 * I don't think we have to check these: the only
		 * thing that could be wrong is the mask size, but
		 * we just truncate that (serves them right). 
		 */
		if(!dpf_iseq(&r[i]) && !dpf_isshift(&r[i]) &&
		   !dpf_isstateeq(&r[i]) && !dpf_isaction(&r[i])) {
			printf("op: %d\n", r[n].u.eq.op);
			return DPF_BOGUSOP;
		}
		r[i].level = i;
		if(dpf_isaction(&r[i])) {
			struct action_block *tmp;

			tmp = (struct action_block *)calloc(1, sizeof(struct action_block));
/*			copyin(r[i].action, tmp, sizeof(struct action_block));
			tmp->code = (struct inst *)calloc(tmp->codesize, sizeof(struct inst));
			copyin(r[i].action->code, tmp->code, tmp->codesize * sizeof(struct inst)); */
			r[i].action = tmp;

			r[i].action->state = calloc(1, sizeof(struct state));
//			r[i].action->last = r[i].action->state;
			laststate = r[i].action->state;
			r[i].action->coffset = 18;
			dpf_state_list[pid] = r[i].action;
		}
		if(dpf_isstateeq(&r[i])) {
			r[i].u.stateeq.state = laststate;
		}
	}
	r[n].u.eq.op = DPF_OP_END;	/* mark it */
	dpf_coalesce(&ir);

	/* XXX: Need to compute alignment and whether there has been a shift. 
		alignment starts at DPF_MSG_ALIGN, shift = 0. */
	
	/* Now merge it in with the main tree.  */
	dpf_overlap = 0;
//	demand(!dpf_active[pid], bogus active!);
	tail = dpf_active[pid] = dpf_merge(dpf_base, &ir.ir[0], pid);


	if(!dpf_overlap) {
		dpf_iptr = dpf_compile(dpf_base);
		if(verbose) {
			printf("Compiled trie:\n");
			dpf_output();
			dpf_dump((void *)dpf_iptr);
		}
	} else {
		if(dpf_delete(pid) < 0)
			fatal(Should not fail);
		dpf_overlap = 0;
		pid = DPF_OVERLAP;
	}
	debug_stmt(dpf_check());
	return pid;
}

/* should really be a char *, int, and an memcmp() */
SYSCALL int dpf_free_state(int pid, int data) {
	struct state *cur, *tmp;
return 0;
	cur = dpf_state_list[pid]->state;
	while(cur != NULL && cur->state != NULL) {
		if(cur->state == data) {
//		if(memcmp(cur->state, data, sz*4)) {
//			free(cur->state);
			if(cur->next2->state != NULL) {
				cur->state = cur->next2->state;
			} else {
				cur->state=NULL;
			}
			tmp = cur->next2;
//			if(dpf_state_list[pid]->last == tmp)
//				dpf_state_list[pid]->last = cur;
			cur->next2 = cur->next2->next2;
			free(tmp);
			break;
		} else {
			cur = cur->next2;
		}
	}

	return 0;
}

void stateclean(int pid) {
	struct state *cur, *tmp;
	unsigned long long t;
	int r;

	t = SYSINFO_GET(si_system_ticks);
	r = SYSINFO_GET(si_rate);

	cur = dpf_state_list[pid]->state;
	while(cur != NULL && cur->state != NULL) {
		if(cur->time < (t - (10000000 / r))) {
printf("freeing state %d\n", cur->state);
			if(cur->next2->state != NULL) {
				cur->state = cur->next2->state;
			} else {
				cur->state=NULL;
			}
			tmp = cur->next2;
//			if(dpf_state_list[pid]->last == tmp)
//				dpf_state_list[pid]->last = cur;
			cur->next2 = cur->next2->next2;
			free(tmp);
			break;
		} else {
			cur = cur->next2;
		}
	}
}

SYSCALL int dpf_insert_old(void *p, int sz) {
	int pid;

	pid = dpf_insert(dpf_xlate(p, sz));
	debug_stmt(dpf_check());
	return pid;
}

void dpf_verbose(int v) 
	{ verbose = v; }

#include <xok/sysinfo.h>
#include <xok/pkt.h>

struct pkthold {
	struct xokpkt *p;
	struct pkthold *n;
};

#if 0
struct msghold {
	int m;
	struct pkthold *pktlist;
	struct msghold *n;
};
#endif

struct msghold *msglist = NULL;

/* need cleanup func that removes:
   frags from pktring (hhmmm, can't do)
   msghold lists that are too old
   state entries that are too old
*/

void msgclean(void) {
	struct msghold *cur, *last;
	struct pkthold *tmp;
	unsigned long long t;
	int r;

	t = SYSINFO_GET(si_system_ticks);
	r = SYSINFO_GET(si_rate);

	cur = last = msglist;
	while(cur != NULL) {
		if(cur->time < (t - (10000000 / r))) {
printf("freeing %d\n",msglist->m);
			while(cur->pktlist != NULL) {
				tmp = cur->pktlist;
				cur->pktlist = cur->pktlist->n;
				free(tmp);
			}
			if(cur == msglist)
				msglist = cur->n;
			else
				last->n = cur->n;
			free(cur);
		}
		last = cur;
		cur = cur->n;
	}
}

void defilter(int msg, struct xokpkt *pkt) {
	struct msghold *cur;
	struct pkthold *tmp;
//struct eiu *eiu = (struct eiu *)pkt->data;

	cur = msglist;
	while(cur != NULL) {
		if(cur->m == msg) {
			cur->time = SYSINFO_GET(si_system_ticks);

//printf("adding pkt for %d\n", msg);
			tmp = (struct pkthold *)malloc(sizeof(struct pkthold));
			tmp->p = pkt;
			tmp->n = cur->pktlist;
			cur->pktlist = tmp;
			return;
		}
		cur = cur->n;
	}
//printf("adding pktlist for %d\n", msg);
//printf("adding pktlist for id %d offset %d length %d\n", eiu->ip.identification, (ntohs(eiu->ip.fragoffset) & IP_FRAGOFF) * 8, ntohs(eiu->ip.totallength));
	cur = (struct msghold *)malloc(sizeof(struct msghold));
	cur->m = msg;
	cur->time = SYSINFO_GET(si_system_ticks);
	cur->pktlist = (struct pkthold *)malloc(sizeof(struct pkthold));
	cur->pktlist->p = pkt;
	cur->pktlist->n = NULL;
	cur->n = msglist;
	msglist = cur;
}

void refilter(struct msghold *cur, struct msghold *last) {
	struct pkthold *tmp;
	struct xokpkt *pkt;
	int filterid;
	int ringid;
	struct frag_return result;

	while(cur->pktlist != NULL) {
	  pkt = cur->pktlist->p;

	  filterid = dpf_iptr (pkt->data, pkt->len, &result);

	  if (filterid <= 0) {
	    /* nobody wants this packet */
	    if (pkt->discardcntP) {
	      (*pkt->discardcntP)++;
	    }
	  } else if ((ringid = dpf_fid_getringval(filterid)) > 0) {
	    /* somebody with a valid ring buffer wants this packet */
	    pktring_handlepkt (ringid, (struct ae_recv *) &pkt->count);
	  }
	  tmp = cur->pktlist;
	  cur->pktlist = cur->pktlist->n;
	  free(tmp);
	}
	if(cur == msglist)
	  msglist = cur->n;
	else
	  last->n = cur->n;
	free(cur);
}

void refilterold(int msg) {
	struct msghold *cur, *last;
	struct pkthold *tmp;
	struct xokpkt *pkt;

	cur = msglist;
	last = msglist;
	while(cur != NULL) {
	  if(cur->m == msg) {
            while(cur->pktlist != NULL) { 
	      pkt = cur->pktlist->p; 
	      {   
		int filterid;
		int ringid;
		struct frag_return result;
  
		filterid = dpf_iptr (pkt->data, pkt->len, &result);
  
		if (filterid <= 0) {
		  printf("dumped on refilter\n");
		  /* nobody wants this packet */
		  if (pkt->discardcntP)
		    (*pkt->discardcntP)++;
		} 
		
		else if ((ringid = dpf_fid_getringval(filterid)) > 0) 
		{
		  /* somebody with a valid ring buffer wants this packet */
		  pktring_handlepkt (ringid, (struct ae_recv *) &pkt->count);
		} 
		
		else printf("no ash support\n"); 
	      }
	      tmp = cur->pktlist;
	      cur->pktlist = cur->pktlist->n;
	      free(tmp);
	    }
	    if(cur == msglist)
	      msglist = cur->n;
	    else 
	      last->n = cur->n;
	    free(cur);
	    return;
	  }
	  last = cur;
	  cur = cur->n;
	}
	//printf("refiltering fail for %d\n", msg);
}
