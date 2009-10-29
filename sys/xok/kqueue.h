
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

#ifndef __KLIST_H__
#define __KLIST_H__

/* 
 * Lists that can be used in kerenel space and traversed in user space
 * even if pages are mapped differently in kernel and user space. The
 * only requirements are that the pages be mapped into user space 
 * contiguously in the same order they were allocated in the kernel
 * and that each object on the lists contains the count of how many
 * pages had already been allocated to other objects on the list. 
 *
 * This way we can easily compute the user space va for an object. If
 * we know that an object is on the Nth page allocated and pages are
 * maped sequentially starting at BASE then our va is BASE+N*NBPG+off
 * where off is the same offset as in the kernel address of the
 * object.
 */

/*
 * List definitions.  */
#define KLIST_HEAD(name, type, vpn_type)	       		        \
struct __##name##_link {                                                \
        struct type *lnk_kptr;	/* kernel va ptr */                     \
        vpn_type lnk_pg;         /* page number above base in user space*/ \
};                                                                      \
struct name {                                                           \
        struct __##name##_link lh_first;                                \
}

#define KLIST_ENTRY(type, name)					        \
struct {								\
	struct __##name##_link le_next;	/* next element */	        \
	struct __##name##_link *le_prev; /* address of previous next element */	\
}

/*
 * List functions.
 */
#define	KLIST_INIT(head) {						\
	 (head)->lh_first.lnk_kptr = NULL;       			\
}

#define KLIST_INSERT_AFTER(listelm, elm, field, pg) {		\
	(elm)->field.le_next = (listelm)->field.le_next;        \
	  if ((elm)->field.le_next.lnk_kptr != NULL)                    \
		(listelm)->field.le_next.lnk_kptr->field.le_prev =	\
		    &(elm)->field.le_next;				\
	(listelm)->field.le_next.lnk_kptr = (elm);			\
        (listelm)->field.le_next.lnk_pg = (elm)->pg;               \
	(elm)->field.le_prev = &(listelm)->field.le_next;		\
}

#define	KLIST_INSERT_BEFORE(listelm, elm, field, pg) {	        \
	(elm)->field.le_prev = (listelm)->field.le_prev;		\
	(elm)->field.le_next.lnk_kptr = (listelm);		        \
	(elm)->field.le_next.lnk_pg = (listelm)->pg;		\
	(listelm)->field.le_prev->lnk_kptr = (elm);			\
        (listelm)->field.le_prev->lnk_pg = (elm)->pg;              \
	(listelm)->field.le_prev = &(elm)->field.le_next;		\
}

#define KLIST_INSERT_HEAD(head, elm, field, pg) {				\
	(elm)->field.le_next = (head)->lh_first;         		\
        if ((head)->lh_first.lnk_kptr != NULL)                          \
		(head)->lh_first.lnk_kptr->field.le_prev = &(elm)->field.le_next;\
	(head)->lh_first.lnk_kptr = (elm);				\
        (head)->lh_first.lnk_pg = (elm)->pg;                       \
	(elm)->field.le_prev = &(head)->lh_first;			\
}

#define KLIST_REMOVE(elm, field) {					\
	if ((elm)->field.le_next.lnk_kptr != NULL)     		        \
		(elm)->field.le_next.lnk_kptr->field.le_prev =		\
		    (elm)->field.le_prev;				\
	*(elm)->field.le_prev = (elm)->field.le_next;			\
}

#ifdef KERNEL

/* get a kernel pointer */
#define KLIST_KPTR(field)  ((field)->lnk_kptr)

#else /* KERNEL */

#include <xok/mmu.h>

/* get a user-space pointer to the next element */
#define KLIST_UPTR(field, base)                                         \
        ((field)->lnk_kptr == NULL ? NULL : (void *)                \
	 (((u_int )base & ~PGMASK) +                                            \
	  ((field)->lnk_pg * NBPG) +                                \
	  ((u_int )((field)->lnk_kptr) & PGMASK)))

#endif /* KERNEL */

#endif /* !__KLIST_H__ */
