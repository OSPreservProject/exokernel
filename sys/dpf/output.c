
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

#include <dpf/dpf-internal.h>
#include <dpf/demand.h>
#include <xok/printf.h>

static void dump(Atom p, int n);

static void indent(int n) {
        while(n-- > 0)
                printf(" ");
}

/* Print out the tree. */
void dpf_output(void) {
	if(!dpf_base->kids.lh_first) {
		printf("Nil trie\n");
		return;
	}
	printf("\nDumping ir.\n");
	printf("base: \n");
	dpf_dump(dpf_iptr);
	printf("rest: \n");
	dump(dpf_base->kids.lh_first, 3);
}

static void dump_eqnode(Atom p, int n, int bits) {
	struct ir *ir;

	/* have to handle hash table as well. */
	ir = &p->ir;

	if(!p->ht) {
		indent(n);
		printf("eq: a: %x sib: %x msg[%d:%d] & 0x%x == 0x%x, refcnt = %d, level = %d, align = %d", 
			(int)p, (int)p->sibs.le_next, ir->u.eq.offset, bits, ir->u.eq.mask, ir->u.eq.val, p->refcnt, ir->level, ir->alignment);
		if(p->pid)
			printf(", pid = %d\n", p->pid);
		printf("\n");
		dpf_dump(p->code);
		dump(p->kids.lh_first, n + 5);
	} else {
		Atom hte, h;
		int i, sz;
		Ht ht;

		printf("ht: \n");
		dpf_dump(p->code);
		for(ht = p->ht, i = 0, sz = ht->htsz; i < sz; i++) {
			if(!(hte = ht->ht[i]))
				continue;

			indent(n);

			printf("{ val = 0x%x, ref = %d", hte->ir.u.eq.val, hte->refcnt);
			if(hte->pid)
				printf(", pid = %d", hte->pid);
			printf(" }\n");
			dump(hte->kids.lh_first, n + 5);
			dpf_dump(hte->code);

			for(h = hte->next; h; h = h->next) {
				indent(n);
				printf("-> { val = 0x%x, ref = %d", h->ir.u.eq.val, h->refcnt);
				if(h->pid)
					printf(", pid = %d", h->pid);
				printf("}\n");
				dump(h->kids.lh_first, n + 5);
				dpf_dump(h->code);
			}
		}
		indent(n);
		printf(" ==  msg[%d:%d] & 0x%x,  refcnt = %d, level = %d\n", 
			ir->u.eq.offset, bits, ir->u.eq.mask, p->refcnt,
ir->level);
	}

}

static void dump_shiftnode(Atom p, int n, int bits) {
	struct ir *ir;

	indent(n);
	ir = &p->ir;
	printf("msg + msg[%d:%d] & 0x%x << %d, align = %d, refcnt = %d, level = %d", 
		ir->u.shift.offset, bits, ir->u.shift.mask, ir->u.shift.shift, ir->u.shift.align, p->refcnt, ir->level);
	if(p->pid)
		printf(", pid = %d\n", p->pid);
	printf("\n");
	dpf_dump(p->code);
	dump(p->kids.lh_first, n + 5);
}

static void dump(Atom p, int n) {
	if(!p) return;

	for(;; p = p->sibs.le_next) {

		if(dpf_iseq(&p->ir))
			dump_eqnode(p, n, p->ir.u.eq.nbits);
		else if(dpf_isshift(&p->ir))
			dump_shiftnode(p, n, p->ir.u.shift.nbits);
		else if(dpf_isaction(&p->ir)) {
		        indent(n);
			printf("action\n");
			dump(p->kids.lh_first, n+5);
		} else if (dpf_isstateeq(&p->ir)){
		        indent(n);
			printf("stateeq\n");
			dump(p->kids.lh_first, n+5);
		} else {
		        indent(n);
			printf("other\n");
			dump(p->kids.lh_first, n+5);
		}

		if(!p->sibs.le_next) 
			return;

		indent(n - 2);
		printf("OR\n");
	}
}
