#ifndef PAGETABLE_H_
#define PAGETABLE_H_

#include <xok/mmu.h>

#include "types.h"

#define PHYSICAL_PAGES  PGNO(PHYSICAL_MEGS_RAM*1024*1024)

#define PG_BASE      0xfffff000
#undef  PGROUNDUP
#define PGROUNDUP(va) (((u_int)(va) + PGMASK) & ~PGMASK)
#undef  PGROUNDDOWN
#define PGROUNDDOWN(va) ((u_int)(va) & ~PGMASK)

/* not definitive, but good for asserts. */
#define IS_HOST_PHYS(pa)	(pa < PHYSICAL_PAGES*NBPG)
#define IS_GUEST_PHYS(pa)	(pa < vmstate.ppages*NBPG)

#define FREE_GDT  GD_NULLS

#define PG_GUEST     0  /* 0x800 */

#define find_va(s, l) _find_va(s, l, 0);
#define claim_va(s, l) _find_va(s, l, 1);

int is_pt(u_int va, u_int *, u_int *);
Bit32u remap_pte(Bit32u pte);
inline Bit32u gp2hp(Bit32u fp);
Bit32u hp2gp(Bit32u rp);
inline u_int guest_phys_to_host_phys(u_int v);
inline u_int host_phys_to_guest_phys(u_int r);
inline u_int virtual_to_host_phys(u_int v);
Bit32u guest_pte(Bit32u va);
void replace_page_table(Bit32u *entry, Bit32u new_pde);
void add_page_table(Bit32u pt, u_int pt_entry);
void remove_page_table(Bit32u pt, u_int pt_entry);
int add_page_directory(Bit32u gcr3);
int remove_page_directory(Bit32u gcr3);
Bit32u *_find_va(Bit32u start, Bit32u len, int requirestart);
void handle_pte(Bit32u * pt, u_int pt_level, u_int pt_entry, Bit32u old_pte, Bit32u new_pte);
int get_host_pte(u_int lina, Bit32u *pte);
int _protect_range(Bit32u base, Bit16u limit, u_int protect);
#define   protect_range(base, limit) _protect_range(base, limit, 1);
#define unprotect_range(base, limit) _protect_range(base, limit, 0);
int map_pages(unsigned int startpage, unsigned int numpages);
int unmap_pages(unsigned int startpage, unsigned int numpages);
void change_cr3(u_int proposed);

void verify_guest_pt(void);
#ifndef NDEBUG
#define VERIFY_GUEST_PT verify_guest_pt()
#else
#define VERIFY_GUEST_PT
#endif


#endif
