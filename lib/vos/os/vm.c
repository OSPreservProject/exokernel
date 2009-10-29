
/*
 * Copyright MIT 1999
 */

#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/pmap.h>

#include <vos/errno.h>
#include <vos/vm.h>


#define alloca __builtin_alloca


int 
vm_alloc_region (unsigned va, unsigned len, u_int kp, u_int prot_flags) 
{
  Pte pte = (prot_flags & PGMASK) & ~PG_MBZ;
  u_int num_pages = PGNO(PGROUNDUP(va+len)) - PGNO(va);
  Pte *ptes = alloca(num_pages * sizeof(Pte));
  u_int num_completed;
  int i;

  if (len == 0) return 0;

  if (!ptes) 
    RETERR(V_NOMEM);

  for (i=num_pages-1; i >= 0; i--)
    ptes[i] = pte;

  num_completed = 0;
  if (vm_self_insert_pte_range 
      (kp, ptes, num_pages, va, &num_completed, 0, NULL) < 0)
    RETERR(V_NOMEM);
  return num_completed;
}


int 
vm_free_region (unsigned va, unsigned len, u_int kp) 
{
  u_int num_pages = PGNO(PGROUNDUP(va+len)) - PGNO(va);
  Pte *ptes = alloca(num_pages * sizeof(Pte));
  u_int num_completed;

  if (len == 0) return 0;

  if (!ptes) RETERR(V_NOMEM);

  bzero(ptes, num_pages * sizeof(Pte));

  num_completed = 0;
  if (vm_self_insert_pte_range 
      (kp, ptes, num_pages, va, &num_completed, 0, NULL) < 0)
    RETERR(V_NOMEM);
  return num_completed;
}


/*
 * Copy mappings at oldva to a new address, protected by kp. Return error
 * when offsets within page aren't equal.
 *
 * Returns the number of page actually copied. If error occurs, return -1.
 */
int 
vm_remap_region (u_int oldva, u_int newva, u_int len, u_int kp) 
{
  u_int num_pages = PGNO(PGROUNDUP(oldva+len)) - PGNO(oldva);
  Pte *ptes = alloca(num_pages * sizeof(Pte));
  u_int num_completed;
  int i;

  if ((oldva & PGMASK) != (newva & PGMASK)) 
  {
     RETERR(V_INVALID);
  }

  if (len == 0) return 0;
  if (!ptes) RETERR(V_NOMEM);

  for (i=num_pages-1; i >= 0; i--) 
  {
     ptes[i] = vpt[PGNO(oldva+(i*NBPG))];
  }

  num_completed = 0;
  if (vm_self_insert_pte_range 
      (kp, ptes, num_pages, newva, &num_completed, 0, NULL) < 0) 
    RETERR(V_NOMEM);
  return num_completed;
}



/*
 * Unmaps a page at the given VA. 
 * 
 * Return 0 if successful. If error occurs, return kernel error code.
 */
int
vm_self_unmap_page(u_int k, u_int va) 
{
  return sys_self_insert_pte(k, 0, va);
}



/*
 * inserts a PTE into our own page table for virtual address va. If table
 * entry is 0, remove the current mapping for va. k is the capability used to
 * modify/guard the page.
 *
 * Return 0 if successful. If error occurs, return kernel error code.
 *
 * flag contains paging policies: XXX
 */
int 
vm_self_insert_pte (u_int k, Pte pte, u_int va, u_int flags, 
                    struct Xn_name *pxn) 
{
  if (pte == 0) return vm_self_unmap_page(k, va);
  return sys_self_insert_pte(k, pte, va);
}



/*
 * inserts a PTE into the page table of the given environment for virtual
 * address va. k is the capability used to modify/guard the page. ke is the
 * capability used to access the given environment.
 *
 * Return 0 if successful. If error occurs, return kernel error code.
 *
 * flag contains paging policies: XXX
 */
int
vm_insert_pte(u_int k, Pte pte, u_int va, u_int ke, int envid, u_int flags,
	       struct Xn_name *pxn)
{
  return sys_insert_pte(k, pte, va, ke, envid);
}



/*
 * inserts a range of PTEs, starting at va, into our page table. k is the
 * capability used to modify/guard the page. 
 *
 * Return 0 if successful. If error occurs, return kernel error code.
 * 
 * flag contains paging policies: XXX
 */
int
vm_self_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
			  u_int *num_completed, u_int flags,
			  struct Xn_name *pxn)
{
  return sys_self_insert_pte_range(k, ptes, num, va, num_completed);
}


/*
 * inserts a range of PTEs, starting at va, into the page table of the given
 * environment. k is the capability used to modify/guard the page. ke is the
 * capability used to access the given environment.
 *
 * Return 0 if successful. If error occurs, return kernel error code.
 *
 * flag contains paging policies: XXX
 */
int
vm_insert_pte_range(u_int k, Pte *ptes, u_int num, u_int va,
		     u_int *num_completed, u_int ke, int envid, u_int flags,
		     struct Xn_name *pxn)
{
  return sys_insert_pte_range(k, ptes, num, va, num_completed, ke, envid);
}

