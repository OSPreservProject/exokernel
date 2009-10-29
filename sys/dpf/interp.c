
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
 *  Simple interpreter.
 */
#include <dpf/dpf-internal.h>
#include <dpf/demand.h>

/* Find the longest match. */
static int interp(uint8 *msg, unsigned nbytes, Atom p, int pid) {
	uint8 *m;
	union ir *ir;
	struct eq *eq;
	struct shift *s;
	int res;
	uint32 v;

	/* try all or branchs */
	for(; p; p = p->or) {
		ir = &p->ir;
		eq = &ir->eq;

		/* is there an overflow? */
		if(eq->offset >= nbytes)
			continue;

		m = &msg[eq->offset];

		demand(eq->nbits % 8 == 0, bogus bit count!);
		v = 0;
		memcpy(&v, m, eq->nbits / 8);	/* will this always work? */

		if(dpf_iseq(eq->op)) {
 			v = v & eq->mask;

			if(p->ht) {
				Atom hte;

				if((hte = ht_lookup(p->ht, v))) {
					res = interp(msg, nbytes, hte->child, hte->pid);
					if(res)
						return res;
				}
			} else if(v == eq->val) {
				res = interp(msg, nbytes, p->child, p->pid);
				if(res)
					return res;
			}
		} else {
			s = &ir->shift;
			demand(dpf_isshift(s->op), bogus op);
			v = (v & s->mask) << s->shift;
			if (v < nbytes)
				if((res = interp(msg+v, nbytes-v, p->child, p->pid)))
					return res;
		}
	}
	return pid;	/* longest match */
}

int dpf_interp(uint8 *msg, unsigned nbytes) {
	return interp(msg, nbytes, dpf_base, 0);
}
