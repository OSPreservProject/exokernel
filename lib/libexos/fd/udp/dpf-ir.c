
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
#include <malloc.h>
#ifdef KERNEL
#include <xok/printf.h>
#include <dpf/demand.h>
#else
#include <string.h>
#include <assert.h>
#define fatal(f) __fatal(f)
#endif

#define IDENTITY_MASK8	0xffUL
#define IDENTITY_MASK16 0xffffUL
#define IDENTITY_MASK32	0xffffffffUL

void dpf_begin(struct dpf_ir *ir) {
	memset(ir, 0, sizeof *ir);
}

static inline struct ir *alloc(struct dpf_ir *ir) {
	if(ir->irn >= DPF_MAXELEM)
		fatal(Code size exceeded.  Increase DPF_MAX.);
	return &ir->ir[ir->irn++];
}

static inline void 
mkeq(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint32 val) {
	struct eq *eq;

	eq = &alloc(ir)->u.eq;
	eq->op = DPF_OP_EQ;
	/* add in relative offset now so don't have to do at demux time. */
	eq->offset = offset + ir->moffset;
	eq->val = val;
	eq->mask = mask;
	eq->nbits = nbits;
}

void dpf_eq8(struct dpf_ir *ir, uint16 offset, uint8 val) 
	{ dpf_meq8(ir, offset, IDENTITY_MASK8, val); }
void dpf_eq16(struct dpf_ir *ir, uint16 offset, uint16 val) 
	{ dpf_meq16(ir, offset, IDENTITY_MASK16, val); }
void dpf_eq32(struct dpf_ir *ir, uint16 offset, uint32 val) 
	{ dpf_meq32(ir, offset, IDENTITY_MASK32, val); }

/* Is the message quantity of nbits at offset equal to val? */
void dpf_meq8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 val) {
	if((mask & IDENTITY_MASK8) != mask)
		fatal(Mask exceeds width of type.);
	mkeq(ir, 8, offset, mask, val);
}

void dpf_meq16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint16 val) {
	if((mask & IDENTITY_MASK16) != mask)
		fatal(Mask exceeds width of type.);
	mkeq(ir, 16, offset, mask, val);
}

void dpf_meq32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint32 val) {
	if((mask & IDENTITY_MASK32) != mask)
		fatal(Mask exceeds width of type.);
	mkeq(ir, 32, offset, mask, val);
}

static inline void
mkshift(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint8 shift) {
	struct shift *s;

	s = &alloc(ir)->u.shift;
	s->op = DPF_OP_SHIFT;
	s->shift = shift;
	s->offset = offset;
	s->mask = mask;
	s->nbits = nbits;
	ir->moffset = 0;	/* relative offset is zero. */
	
}

/* shift the base message pointer by nbits */
void dpf_shift8(struct dpf_ir *ir, uint16 offset, uint8 shift) 
	{ dpf_mshift8(ir, offset, IDENTITY_MASK8, shift); }
void dpf_shift16(struct dpf_ir *ir, uint16 offset, uint8 shift) 
	{ dpf_mshift16(ir, offset, IDENTITY_MASK16, shift); }
void dpf_shift32(struct dpf_ir *ir, uint16 offset, uint8 shift) 
	{ dpf_mshift32(ir, offset, IDENTITY_MASK32, shift); }

void dpf_mshift8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 shift) 
	{ mkshift(ir, 8, offset, mask, shift); }
void dpf_mshift16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint8 shift) 
	{ mkshift(ir, 16, offset, mask, shift); }
void dpf_mshift32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint8 shift) 
	{ mkshift(ir, 32, offset, mask, shift); }

void dpf_shifti(struct dpf_ir *ir, uint16 offset) 
	{ ir->moffset += offset; }

#if 0
void dpf_action(struct dpf_ir *ir, uint16 offset, struct action_block *prog) {
	struct ir *irt;

	irt = alloc(ir);
	irt->action = (struct action_block *)calloc(1, sizeof(struct action_block));
	irt->action->code = (struct inst *)calloc(prog->codesize, sizeof(struct inst));
	memcpy(irt->action->code, prog->code, prog->codesize*sizeof(struct inst));
	irt->action->codesize = prog->codesize;
	irt->action->statesize = prog->statesize;
	irt->action->state = calloc(1, sizeof(struct state)); /* (int *)calloc(prog->statesize, sizeof(int)); */
}
#endif

static inline void 
mkactioneq(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask) {
	struct eq *eq;

	eq = &alloc(ir)->u.eq;
	eq->op = DPF_OP_ACTION;
	/* add in relative offset now so don't have to do at demux time. */
	eq->offset = offset + ir->moffset;
	eq->mask = mask;
	eq->nbits = nbits;
}

void dpf_actioneq8(struct dpf_ir *ir, uint16 offset) 
	{ dpf_aeq8(ir, offset, IDENTITY_MASK8); }
void dpf_actioneq16(struct dpf_ir *ir, uint16 offset) 
	{ dpf_aeq16(ir, offset, IDENTITY_MASK16); }
void dpf_actioneq32(struct dpf_ir *ir, uint16 offset) 
	{ dpf_aeq32(ir, offset, IDENTITY_MASK32); }

/* Is the message quantity of nbits at offset equal to val? */
void dpf_aeq8(struct dpf_ir *ir, uint16 offset, uint8 mask) {
	if((mask & IDENTITY_MASK8) != mask)
		fatal(Mask exceeds width of type.);
	mkactioneq(ir, 8, offset, mask);
}

void dpf_aeq16(struct dpf_ir *ir, uint16 offset, uint16 mask) {
	if((mask & IDENTITY_MASK16) != mask)
		fatal(Mask exceeds width of type.);
	mkactioneq(ir, 16, offset, mask);
}

void dpf_aeq32(struct dpf_ir *ir, uint16 offset, uint32 mask) {
	if((mask & IDENTITY_MASK32) != mask)
		fatal(Mask exceeds width of type.);
	mkactioneq(ir, 32, offset, mask);
}

static inline void 
mkstateeq(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint32 stateoffset) {
	struct stateeq *seq;

	seq = &alloc(ir)->u.stateeq;
	seq->op = DPF_OP_STATEEQ;
	/* add in relative offset now so don't have to do at demux time. */
	seq->offset = offset + ir->moffset;
	seq->stateoffset = stateoffset;
	seq->mask = mask;
	seq->nbits = nbits;
}

void dpf_stateeq8(struct dpf_ir *ir, uint16 offset, uint8 val) 
	{ dpf_seq8(ir, offset, IDENTITY_MASK8, val); }
void dpf_stateeq16(struct dpf_ir *ir, uint16 offset, uint16 val) 
	{ dpf_seq16(ir, offset, IDENTITY_MASK16, val); }
void dpf_stateeq32(struct dpf_ir *ir, uint16 offset, uint32 val) 
	{ dpf_seq32(ir, offset, IDENTITY_MASK32, val); }

/* Is the message quantity of nbits at offset equal to val? */
void dpf_seq8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 val) {
	if((mask & IDENTITY_MASK8) != mask)
		fatal(Mask exceeds width of type.);
	mkstateeq(ir, 8, offset, mask, val);
}

void dpf_seq16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint16 val) {
	if((mask & IDENTITY_MASK16) != mask)
		fatal(Mask exceeds width of type.);
	mkstateeq(ir, 16, offset, mask, val);
}

void dpf_seq32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint32 val) {
	if((mask & IDENTITY_MASK32) != mask)
		fatal(Mask exceeds width of type.);
	mkstateeq(ir, 32, offset, mask, val);
}

#if 0
void dpf_removestate(struct dpf_ir *ir, uint32 state) {
	int i;
	struct state *cur;
	struct state **last;

	for(i = 0; i < ir->irn; i++)
		if(dpf_isstateeq(&ir->ir[i])) {
		        last = &ir->ir[i].u.stateeq.state;
			for(cur=ir->ir[i].u.stateeq.state; cur->next2; cur=cur->next2) {
				if(cur->state == state)
					*last = cur->next2;
				last = &cur->next2;
			}
		}
}

/* current cheesy requirements: 
    first filter must have action,
    second filter must have stateeq 
    now with TWICE the crap! */
void dpf_attach(struct dpf_ir *ir1, struct dpf_ir *ir2) {
	int i;
	struct state *state = NULL;
	struct action_block *action = NULL;

	/* ir1->next = ir2; */
	for(i = 0; i < ir1->irn; i++)
		if(ir1->ir[i].action != NULL) {
		        action = ir1->ir[i].action;
			state = ir1->ir[i].action->state;
			break;
		}

	for(i = 0; i < ir2->irn; i++)
	  if(dpf_isstateeq(&ir2->ir[i])) {
	    ir2->ir[i].u.stateeq.state = state;
	    action->stateoffset = ir2->ir[i].u.stateeq.stateoffset;
	    action->statemask = ir2->ir[i].u.stateeq.mask;
	  }
	
}

struct state *dpf_attach1(struct dpf_ir *ir1) {
	int i;

	for(i = 0; i < ir1->irn; i++)
		if(ir1->ir[i].action != NULL) {
			return ir1->ir[i].action->state;
		}
	fatal(no action found!);
}

void dpf_attach2(struct dpf_ir *ir2, struct state *state) {
	int i;

	for(i = 0; i < ir2->irn; i++)
		if(dpf_isstateeq(&ir2->ir[i]))
			ir2->ir[i].u.stateeq.state = state;
}
#endif
