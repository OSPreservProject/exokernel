
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

#ifndef __VM_H__
#define __VM_H__

#include <sys/types.h>
#include <xok/mmu.h>
#include <xok/pxn.h>

/* Userlevel vm defines */

/* User-defined bits in PTE */

#define PG_SHARED 0x200		/* page is shared, not copied on fork */
#define PG_COW    0x400		/* page is copy-on-write */
#define PG_RO     0x800		/* page is readonly */

#define EMPTY_PTE 0x0

/* entry point into executables that are being traced. Stops them as soon
   as they start so parent can control them. The magic 5 is there so we
   skip over the 'jmp start' that appeares at the start of entry.S */

#define UTEXT_TRACED (UTEXT + 5)

void do_cow_fault (u_int va, Pte pte, u_int eip); /* cow.c */
int ppn_is_dirty (u_int ppn, u_int cap); /* vm.c */
int __vm_alloc_region (unsigned va, unsigned len, u_int kp, u_int prot_flags);
int __vm_share_region (unsigned va, unsigned len, u_int kp, u_int ke, 
		       int envid,unsigned newva);
int __vm_free_region (unsigned va, unsigned len, u_int kp);
int __vm_remap_region (unsigned oldva, unsigned newva, unsigned len, u_int kp);
int __vm_count_refs (unsigned va);

caddr_t __mmap (void *addr, size_t len, int prot, int flags, int fd, 
		off_t offset, u_int ke, int envid);
int __mprotect (void *addr, size_t len, int prot, int k, int envid);

int register_pagefault_handler (uint vastart, int len,
				int (*handler)(uint,int));
extern u_int __fault_test_enable;
extern u_int __fault_test_va;

#define ESIP_DONTPAGE  0x1 /* don't let the pager page this page out */
#define ESIP_DONTSAVE  0x2 /* don't save the page if dirty, just unmap */
#define ESIP_DONTWAIT  0x4 /* don't wait for memory to become available */
#define ESIP_MMAPED    0x8 /* make clean via msync before unmapping */
#define ESIP_URGENT   0x10 /* use a reserved page */
int _exos_self_insert_pte(u_int k, Pte pte, u_int va, u_int flags,
			  struct Xn_name *);
int _exos_insert_pte(u_int k, Pte pte, u_int va, u_int ke, int envid,
		     u_int flags, struct Xn_name *);
int _exos_self_unmap_page(u_int k, u_int va);
int _exos_self_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
				u_int *num_completed, u_int flags,
				struct Xn_name *pxn);
int _exos_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
			   u_int *num_completed, u_int ke, int envid,
			   u_int flags, struct Xn_name *pxn);
struct bc_entry *_exos_ppn_to_bce(u_int ppn);
int _exos_bc_read_and_insert(u_int dev, u_quad_t block, u_int num,
			     int *resptr);
int __remap_reserved_page(u_int va, u_int pte_flags);
void __replinish(void);

#endif /* __VM_H__ */
