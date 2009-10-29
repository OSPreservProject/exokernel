
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dpf/dpf-internal.h>
#include <vos/assert.h>

#define IDENTITY_MASK8	0xffUL
#define IDENTITY_MASK16 0xffffUL
#define IDENTITY_MASK32	0xffffffffUL


void dpf_begin(struct dpf_ir *ir) 
{
  memset(ir, 0, sizeof *ir);
}

static inline struct ir *alloc(struct dpf_ir *ir) 
{
  if(ir->irn >= DPF_MAXELEM)
  {
    printf("Code size exceeded. Increase DPF_MAX.\n");
    assert(0);
  }

  return &ir->ir[ir->irn++];
}

static inline void 
mkeq(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint32 val) 
{
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
{ 
  dpf_meq8(ir, offset, IDENTITY_MASK8, val); 
}

void dpf_eq16(struct dpf_ir *ir, uint16 offset, uint16 val) 
{ 
  dpf_meq16(ir, offset, IDENTITY_MASK16, val); 
}

void dpf_eq32(struct dpf_ir *ir, uint16 offset, uint32 val) 
{ 
  dpf_meq32(ir, offset, IDENTITY_MASK32, val); 
}


/* Is the message quantity of nbits at offset equal to val? */
void dpf_meq8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 val) 
{
  if((mask & IDENTITY_MASK8) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkeq(ir, 8, offset, mask, val);
}

void dpf_meq16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint16 val) 
{
  if((mask & IDENTITY_MASK16) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkeq(ir, 16, offset, mask, val);
}

void dpf_meq32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint32 val) 
{
  if((mask & IDENTITY_MASK32) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkeq(ir, 32, offset, mask, val);
}

static inline void
mkshift(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint8 shift) 
{
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
{ 
  dpf_mshift8(ir, offset, IDENTITY_MASK8, shift); 
}

void dpf_shift16(struct dpf_ir *ir, uint16 offset, uint8 shift) 
{ 
  dpf_mshift16(ir, offset, IDENTITY_MASK16, shift); 
}

void dpf_shift32(struct dpf_ir *ir, uint16 offset, uint8 shift) 
{ 
  dpf_mshift32(ir, offset, IDENTITY_MASK32, shift); 
}

void dpf_mshift8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 shift) 
{ 
  mkshift(ir, 8, offset, mask, shift); 
}

void dpf_mshift16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint8 shift) 
{ 
  mkshift(ir, 16, offset, mask, shift); 
}

void dpf_mshift32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint8 shift) 
{ 
  mkshift(ir, 32, offset, mask, shift); 
}

void dpf_shifti(struct dpf_ir *ir, uint16 offset) 
{ 
  ir->moffset += offset; 
}

static inline void 
mkactioneq(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask) 
{
  struct eq *eq;

  eq = &alloc(ir)->u.eq;
  eq->op = DPF_OP_ACTION;
  /* add in relative offset now so don't have to do at demux time. */
  eq->offset = offset + ir->moffset;
  eq->mask = mask;
  eq->nbits = nbits;
}

void dpf_actioneq8(struct dpf_ir *ir, uint16 offset) 
{ 
  dpf_aeq8(ir, offset, IDENTITY_MASK8); 
}

void dpf_actioneq16(struct dpf_ir *ir, uint16 offset) 
{
  dpf_aeq16(ir, offset, IDENTITY_MASK16); 
}

void dpf_actioneq32(struct dpf_ir *ir, uint16 offset) 
{ 
  dpf_aeq32(ir, offset, IDENTITY_MASK32); 
}

/* Is the message quantity of nbits at offset equal to val? */
void dpf_aeq8(struct dpf_ir *ir, uint16 offset, uint8 mask) 
{
  if((mask & IDENTITY_MASK8) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkactioneq(ir, 8, offset, mask);
}

void dpf_aeq16(struct dpf_ir *ir, uint16 offset, uint16 mask) 
{
  if((mask & IDENTITY_MASK16) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkactioneq(ir, 16, offset, mask);
}

void dpf_aeq32(struct dpf_ir *ir, uint16 offset, uint32 mask) 
{
  if((mask & IDENTITY_MASK32) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkactioneq(ir, 32, offset, mask);
}

static inline void 
mkstateeq(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint32 stateoffset) 
{
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
{ 
  dpf_seq8(ir, offset, IDENTITY_MASK8, val); 
}

void dpf_stateeq16(struct dpf_ir *ir, uint16 offset, uint16 val) 
{
  dpf_seq16(ir, offset, IDENTITY_MASK16, val); 
}

void dpf_stateeq32(struct dpf_ir *ir, uint16 offset, uint32 val) 
{
  dpf_seq32(ir, offset, IDENTITY_MASK32, val); 
}

/* Is the message quantity of nbits at offset equal to val? */
void dpf_seq8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 val) 
{
  if((mask & IDENTITY_MASK8) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkstateeq(ir, 8, offset, mask, val);
}

void dpf_seq16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint16 val) 
{
  if((mask & IDENTITY_MASK16) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkstateeq(ir, 16, offset, mask, val);
}

void dpf_seq32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint32 val) 
{
  if((mask & IDENTITY_MASK32) != mask)
  {
    printf("Mask exceeds width of type.\n");
    assert(0);
  }
  mkstateeq(ir, 32, offset, mask, val);
}

