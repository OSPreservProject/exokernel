
/*
 * Copyright 1999 MIT
 */

#ifndef __VOS_VM_H__
#define __VOS_VM_H__

#include <xok/types.h>
#include <xok/pxn.h>
#include <xok/mmu.h>  
#include <xok/pmap.h>

/* 
 * bits 9-11 on Intel PTE are available for OS use. VOS uses them for the
 * following convention:
 */
#define PG_SHARED  0x200   /* bit  9 - shared page, no copy on fork */
#define PG_COW 	   0x400   /* bit 10 - copy on write after fork */
#define PG_BIT11   0x800   /* bit 11 - unused */


/* vm_alloc_region: allocates pages for the range va to va+len. Any previous
 * mappings are removed. The new pages are protected by the new capability kp.
 * prot_flags specifies the new protection flag (on PTE) for the page, and is
 * an or'ing of PG_P, PG_U, PG_COW, etc. Returns -1 if failed. Otherwise
 * returns the number of page actually allocated (may not equal to number of
 * pages requested). Errno is set to:
 *
 *   V_NOMEM: no more memory left for PTEs or system call to xok failed.
 */
extern int 
vm_alloc_region (unsigned va, unsigned len, u_int kp, u_int prot_flags);

/*
 * vm_free_region: unmap pages in a virtual range by removing all mappings in
 * the specified range of virtual addresses. It is not an error if any or all
 * of these pages are not mapped. The new va ranges are protected by the new
 * capability kp. Returns the number of page actually freed.  Returns -1 if
 * failed. Errno is set to:
 *
 *   V_NOMEM: cannot allocate empty PTEs or system call to xok failed.
 */
extern int 
vm_free_region (unsigned va, unsigned len, u_int kp);


/*
 * vm_remap_region: copy mappings at oldva to a new address, protected by kp.
 * If successful, returns the number of pages actually moved (may not equal to
 * the number requested). Returns -1 if failed. Errno is set to:
 *
 *   V_INVALID: oldva and newva are not the same offset off a page boundary.
 *   V_NOMEM:   no memory left for PTEs or system call to xok failed.
 */
extern int 
vm_remap_region (u_int oldva, u_int newva, u_int len, u_int kp);


/*
 * vm_<self_>insert_pte<_range>: wrapper functions over xok system calls.
 * Ignore pxn and flag arguments. Returns xok syscall error codes.
 */
extern int
vm_self_unmap_page(u_int k, u_int va);

extern int 
vm_self_insert_pte(u_int k, Pte pte, u_int va, u_int flags, struct Xn_name *pxn);

extern int
vm_insert_pte(u_int k, Pte pte, u_int va, u_int ke, int envid, u_int flags,
	      struct Xn_name *pxn);
extern int
vm_self_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
			 u_int *num_completed, u_int flags,
			 struct Xn_name *pxn);
extern int
vm_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
		    u_int *num_completed, u_int ke, int envid, u_int flags,
		    struct Xn_name *pxn);


#include <vos/malloc.h>



/* the following is a table of fault handlers... */

typedef int (*vos_fault_handler_t)
  (u_int va, u_int errcode, u_int eflags, u_int eip);

typedef struct {
  u_int va;
  u_int size;
  vos_fault_handler_t h;
} fault_handler_t;

#define NUM_FAULT_HANDLERS 128

extern fault_handler_t fault_handlers[NUM_FAULT_HANDLERS];

/*
 * fault_handler_add: add a fault handler for the va range given. Returns 0 if
 * successful. -1 otherwise.
 */
extern int fault_handler_add(vos_fault_handler_t h, u_int va, u_int size);

/*
 * fault_handler_remove: remove a fault handler beginning at the va given.
 * Returns 0 if successful. -1 otherwise.
 */
extern int fault_handler_remove(u_int va);


#endif /* __VOS_VM_H__ */



