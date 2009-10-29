
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
 * Public dpf interfaces. 
 */
#ifndef __DPF_H__
#define __DPF_H__

/* Errors */
enum { DPF_TOOMANYFILTERS = -2, DPF_TOOMANYELEMS = -3, DPF_BOGUSOP = -4, 
	DPF_BOGUSID = -5 , DPF_OVERLAP = -6, DPF_NILFILTER = -7, DPF_NOMEM = -8};
	
/* Types we use.  Will be parameterized for the different machines. */
#include <xok/types.h>
#include <xok/xoktypes.h>
#if 0
/* defined in include/types.h */
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
#endif

#include <dpf/action.h>

struct dpf_ir;	/* opaque data structure. */

/* Filter insertion/deletion. */
int dpf_insert(struct dpf_ir *filter);
int dpf_delete(unsigned pid);

/* dump a filter */
void dpf_printir(struct dpf_ir *ir);

/* initialize a filter structure. */
void dpf_begin(struct dpf_ir *ir);

/* Called to classify a packet. */
int (*dpf_iptr)(uint8 *msg, unsigned nbytes, struct frag_return *retmsg);

/* 
 *  Filter creation routines.  nbits corresponds to 8, 16, 32 depending on
 * the operation.  msg[byte_offset:nbits] means to load nbits of the message
 * at byte_offset.
 */

/*
 * Compare message value to constant:
 * 	msg[byte_offset:nbits] == val
 */
void dpf_eq8(struct dpf_ir *ir, uint16 byte_offset, uint8 val);
void dpf_eq16(struct dpf_ir *ir, uint16 byte_offset, uint16 val);
void dpf_eq32(struct dpf_ir *ir, uint16 byte_offset, uint32 val);

/* 
 * Compare message value to constant:
 * 	msg[byte_offset:nbits] & mask == val
 */
void dpf_meq8(struct dpf_ir *ir, uint16 byte_offset, uint8 mask, uint8 val);
void dpf_meq16(struct dpf_ir *ir, uint16 byte_offset, uint16 mask, uint16 val);
void dpf_meq32(struct dpf_ir *ir, uint16 byte_offset, uint32 mask, uint32 val);

/* 
 * Shift the base message pointer: 
 * 	msg += msg[byte_offset:nbits] << shift; 
 */
void dpf_shift8(struct dpf_ir *ir, uint16 byte_offset, uint8 shift);
void dpf_shift16(struct dpf_ir *ir, uint16 byte_offset, uint8 shift);
void dpf_shift32(struct dpf_ir *ir, uint16 byte_offset, uint8 shift);

/* 
 * Shift the base message pointer: 
 *	msg += (msg[byte_offset:nbits] & mask) << shift; 
 */
void dpf_mshift8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 shift);
void dpf_mshift16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint8 shift);
void dpf_mshift32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint8 shift);

/* Shift the base message pointer by a constant: msg += nbytes. */
void dpf_shifti(struct dpf_ir *ir, uint16 nbytes);

/* add action to filter
void dpf_action(struct dpf_ir *ir, uint16 offset, struct action_block *prog); */
/*
 * get val from message, store in statelist
 */
void dpf_actioneq8(struct dpf_ir *ir, uint16 byte_offset);
void dpf_actioneq16(struct dpf_ir *ir, uint16 byte_offset);
void dpf_actioneq32(struct dpf_ir *ir, uint16 byte_offset);
void dpf_aeq8(struct dpf_ir *ir, uint16 byte_offset, uint8 mask);
void dpf_aeq16(struct dpf_ir *ir, uint16 byte_offset, uint16 mask);
void dpf_aeq32(struct dpf_ir *ir, uint16 byte_offset, uint32 mask);

/*
 * Compare message value to constant:
 *      msg[byte_offset:nbits] == state[val]
 */
void dpf_stateeq8(struct dpf_ir *ir, uint16 byte_offset, uint8 val);
void dpf_stateeq16(struct dpf_ir *ir, uint16 byte_offset, uint16 val);
void dpf_stateeq32(struct dpf_ir *ir, uint16 byte_offset, uint32 val);

/* 
 * Compare message value to constant:
 *      msg[byte_offset:nbits] & mask == state[val] & mask
 */
void dpf_seq8(struct dpf_ir *ir, uint16 byte_offset, uint8 mask, uint8 val);
void dpf_seq16(struct dpf_ir *ir, uint16 byte_offset, uint16 mask, uint16 val);
void dpf_seq32(struct dpf_ir *ir, uint16 byte_offset, uint32 mask, uint32 val); 

void dpf_attach(struct dpf_ir *ir1, struct dpf_ir *ir2);

#endif /* __DPF_H__ */
