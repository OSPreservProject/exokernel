
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
 * Optimization routines for dpf.  They are optional.  
 * 
 * One thing that we should do is make this exploit 64-bit loads.
 * See coalesce for ramblings on how to make the optimizations optimal.
 */
#include <dpf/dpf-internal.h>

/* 
 * Consumes a sorted ir and produces a coalesced version.
 * Very simplistic: coalesces only when bitfields abut. 
 *
 * Sophistication (if it would be useful) would come from
 * coalescing with gaps and (the biggie) making machine 
 * specific decisions as to when to coalesce (as opposed
 * the current greedy strategy) by trading the number of
 * chunks we create vs. the cost of any unaligned loads
 * we introduce.  Fortunately from ease of implementation
 * but unfortunately from a puzzle standpoint, I don't think
 * the extra optimization is particularly important.
 */
void dpf_coalesce(struct dpf_ir *irs) {
/* should make this the wordsize of the machine. */
#define DPF_MAXBITS 32
/* ulword */
	struct ir *ir, *p;
	int i,n, coalesced;
	struct eq *eq1, *eq2;

	for(p = ir = &irs->ir[0], coalesced = 0, i = 1, n = irs->irn; i < n; i++) {
		eq1 = &p->u.eq;
		eq2 = &ir[i].u.eq;

		/*
		 * If both ops are eq's, and they abut, and their
		 * length does not exceed the wordsize, coalesce.
 	 	 * 
		 * (Should we do something useful with overlap?)
		 */
		if (eq1->op == DPF_OP_EQ && eq2->op == DPF_OP_EQ
		&& (eq1->nbits + eq1->offset * 8) == eq2->offset * 8
		&& (eq1->nbits + eq2->nbits) <= DPF_MAXBITS) {
			eq1->mask |= eq2->mask << eq1->nbits;
			eq1->val |= eq2->val << eq1->nbits;
			eq1->nbits += eq2->nbits;
			coalesced++;
			continue;
		} else
			*++p = ir[i];
	}
	irs->irn -= coalesced;
	ir[irs->irn].u.eq.op = DPF_OP_END;
}

/* 
 * Given a shift expression, compute the minimum alignement it guarentees. 
 * This guarentee is used to remove the need to load each word and half 
 * word a byte at a time.  Alignment is monotonically decreasing.  Consider
 * the case where our lower bound is 2 bytes but the actual alignment is
 * 4 --- if we encounter another 2 byte alignment, upgrading the entire
 * alignement would give a bogus guarentee of 4.
 *
 * Recall that a shift represents the expression:
 *	msg + (msg[offset:nbits] & mask) << shift
 * The algorithm:
 *   Alignment starts at DPF_MSG_ALIGNED.
 *	Shifts to the right increase the alignment by 1 << shift.
 *	Masks with lower bits set to zero increase it by the bytes
 *		that are zeroed out.
 *	These alignments are added together.
 *  Addition of alignment and computed alignment is simply min.
 */
int dpf_compute_alignment(struct ir *ir, int alignment) {
	struct shift *s;
	unsigned ca;

	s = &ir->u.shift;
	
	/* (1 << first_bit(mask)) + shift gives the computed alignment */
	if((ca = (s->mask & -s->mask)) == 1)
		ca = 0;		/* correct for mask with low bit set. */

	if(!(ca += s->shift))
		ca = 1;		/* correct again for 0 alignment */

	/* The aligment of ir is minimum of alignment and computed alignment. */
	return ir->u.shift.align =  ca < alignment ? ca : alignment;
}
