
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

#ifndef __KERNEL__
#define __KERNEL__
#include <ubb/xn.h>
#include <ubb/kernel/virtual-disk.h>
#include <ubb/lib/bit.h>
#include <xok/printf.h>
#include <xok/malloc.h>
#include <xok/xoktypes.h>
extern Bit_T free_map;

extern int root_pid;
extern cap_t root_cap;
#define PAGESIZ XN_BLOCK_SIZE

/* Used for internal system calls that allocate data on stack. */
extern int xn_in_kernel;

void kernel_init(void);

extern int XN_DEV;
struct bc_entry *bc_alloc_entry(u32 dev, u32 db, int *error);
#define bc_va(bc) ppn_to_phys((bc)->buf_ppn)

/* move data between user/kernel. */
int udf_copyout(void *dst, void *src, size_t nbytes);
int udf_copyin(void *dst, void *src, size_t nbytes);
int vm_writable(void *dst, size_t nbytes);
int read_accessible(void *src, size_t nbytes);
int write_accessible(void *src, size_t nbytes);
xn_cnt_t* valid_user_ptr(xn_cnt_t* ptr);

/* Auth stuff. */
int auth_check(int op, cap_t xncap, cap_t cap, size_t size);
int switch_pid(int npid);

/* Page routines. */
ppn_t palloc(ppn_t base, size_t npages);
int pcheck(ppn_t base, size_t npages, cap_t cap);
void pfree(ppn_t base, size_t npages);
ppn_t phys_to_ppn(void *page);
void *ppn_to_phys(ppn_t ppn);
ppn_t phys_to_ppn(void *phys);

void *db_get_freemap(size_t *nbytes);
void db_put_freemap(void *f_map, size_t nbytes);

int db_alloc(db_t db, size_t n);
int db_free(db_t db, size_t n);
int db_isfree(db_t db, size_t n);
db_t db_find_free(db_t hint, size_t n);

#endif /* __KERNEL__ */
