
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

#ifdef EXOPC
#include "kernel.h"
#else
#include <stdlib.h>
#endif
#include <xok/types.h>
#include "demand.h"
#include "kernel.h"
#include "arena.h"
#include "kexcept.h"
#include "mv.h"
#include "xn-struct.h"

/* Make sure the user is not trying to screw us. */
int mv_access(size_t nbytes, struct xn_m_vec *mv, size_t n) {
	int i;
	size_t len;

	if(!read_accessible(mv, sizeof *mv * n))
		return -1;
	for(i = 0; i < n; i++, mv++) {
		if(!read_accessible(mv->addr, mv->nbytes))
			return -1;

		len = mv->offset + mv->nbytes;
		if(len > nbytes)
			return -1;
		/* overflow? */
		if(len < mv->offset || len < mv->nbytes) {
			fatal(overflow);
			return -1;
		}
	}
	return 0;
}

/* Modify meta as per the instructions of mv. */
void mv_write(void * meta, struct xn_m_vec *mv, size_t n) {
	void * src, * dst;
	size_t nbytes;
	int i;

	for(i = 0; i < n; i++, mv++) {
		src = mv->addr;
		nbytes = mv->nbytes;
		dst = (char *)meta + mv->offset;
		memcpy(dst, src, nbytes);
	}
}

/* Need to store the difference somewhere... */
struct xn_m_vec *mv_read(void * meta, struct xn_m_vec *mv, size_t n) {
	struct xn_m_vec *m, *p;
	int i;
	void * src, * dst;
	size_t nbytes;
	

       	p = m = (struct xn_m_vec *)Arena_alloc(sys_arena, sizeof *m * n);
	assert(m);

	for(i = 0; i < n; i++, p++, mv++) {
		*p = *mv;

		assert(p->nbytes);
		p->addr = (void *)Arena_alloc(sys_arena, mv->nbytes);
		assert(p->addr);

		src = (char *)meta + mv->offset;
		dst = p->addr;
		nbytes = mv->nbytes;
		memcpy(dst, src, nbytes);
	}

        return m;
}
