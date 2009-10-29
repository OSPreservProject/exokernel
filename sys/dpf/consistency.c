
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

/* check internal consistency of dpf_base */
#include <dpf/dpf-internal.h>
#include <dpf/demand.h>
#include <xok/printf.h>

#define NDEBUG

static int ck_pid_table[DPF_MAXFILT];

static inline int seen_pid(int pid) {
        if(pid) {
                if(!ck_pid_table[pid])
                        ck_pid_table[pid] = 1;
                else {
                        printf("***\npid %d has been seen twice\n\n", pid);
                        dpf_output();
                        return 1;
                }
        }
        return 0;
}

static void cktrie(Atom parent, Atom a, int refcnt) {
        int n, htn;
        Ht ht;
        Atom or, prev;

	if(!a)
		return;

        for(prev = 0, n = 0; a; prev = a, a = a->sibs.le_next) {
                if(a->parent != parent) {
			printf("\n********\nbogus parent!\n\n");
			dpf_output();
 			fatal(bogus parent);
		}
                n += a->refcnt;

                demand(!seen_pid(a->pid), already seen pid);

                if(!(ht = a->ht)) {
                        cktrie(a, a->kids.lh_first, a->refcnt - !!a->pid);
                        continue;
                }
		demand(!a->pid, ht should not have a pid);

		for(htn = 0, or = a->kids.lh_first; or; or = or->sibs.le_next) {
                	demand(or->parent == a, bogus parent);
                	htn += or->refcnt;
                	cktrie(or, or->kids.lh_first, or->refcnt - !!or->pid);
		}
	
                if(htn != a->refcnt) {
                        printf("****\na->refcnt == %d, htn = %d\n\n",
                                a->refcnt, htn);
                        dpf_output();
                        fatal(bogus refcnt);
                }
        }

        if(n == refcnt)
                return;

        printf("****\nrefcnt == %d, n = %d\n\n", refcnt, n);
        dpf_output();
        fatal(bogus refcnt);
}

void dpf_check(void) {
#ifndef NDEBUG
        int i, refcnt;
	Atom base;

        if(!(base = dpf_base->kids.lh_first))
                return;

        memset(ck_pid_table, 0, sizeof ck_pid_table);

        for(refcnt = i = 0; i < DPF_MAXFILT; i++)
                if(dpf_active[i])
                        refcnt++;

        cktrie(dpf_base, base, refcnt);
#endif
}
