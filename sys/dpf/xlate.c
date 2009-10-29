
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
   Translate old style IR to new style.

   We translate six trees: 
 	eqi 
	 |
	bits(8|16|32)

	eqi
	 |
	andi
	 |
	bits(8|16|32)
 */
#include <dpf/dpf-internal.h>
#include <dpf/demand.h>
#include <dpf/old-dpf.h>
#include <xok/printf.h>

/* 
 * Given a pointer to previous IR, returns a pointer to new-style. Not 
 * reentrant.
 */
struct dpf_ir *dpf_xlate(struct dpf_frozen_ir *ir, int nelems) {
	uint32 val, mask;
	static struct dpf_ir xir;
	int i;

	dpf_begin(&xir);

	for(i = 0; i < nelems; i++) {
		if(ir[i].op != DPF_EQI)
			fatal(unexpected op);

		val = ir[i++].imm;
		
		/* check to see if there is an and */
		mask = (ir[i].op == DPF_ANDI) ? ir[i++].imm : 0xffffffff;

		/* load the value */
		switch(ir[i].op) {
		case DPF_BITS8I:
			dpf_meq8(&xir, ir[i].imm, mask, val);
			break;
		case DPF_BITS16I:
			dpf_meq16(&xir, ir[i].imm, mask, val);
			break;
		case DPF_BITS32I:
			dpf_meq32(&xir, ir[i].imm, mask, val);
			break;
		default: fatal(Unexpected op);
		}
	}
	return &xir;
}
