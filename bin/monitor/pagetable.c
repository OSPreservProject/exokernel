#include <xok/mmu.h>
#include <xok/sys_ucall.h>

#include "config.h"
#include "debug.h"
#include "emu.h"
#include "memutil.h"
#include "descriptors.h"
#include "pagetable.h"


void change_cr3(u_int proposed)
{
    /* do NOT map pages if PG is not set, since that would trash
       our fake physical mapping. */
    
    if (vmstate.cr[3] == proposed) {
	/* Linux does a...
	   mov reg <- cr3
	   mov cr3 <- reg
	   ...to flush the TLB.  Emulating the second statement would
	   remove all guest pages in the host PT, and then re-add them.
	   Fine except that removes the mapping for the page table itself,
	   and we blow up while removing.
	*/
	
	/* FIXME  flush TLBs somehow */
    } else {
	/* FIXME:  this would break for the above reason */
	ASSERT(0);
	
	remove_page_directory(vmstate.cr[3]);
	add_page_directory(proposed & 0xfffff000);
    }
    vmstate.cr[3] = proposed;
}


inline Bit32u remap_pte(Bit32u pte)
{
    return guest_phys_to_host_phys(pte);
}


/* Test if pa is in a page in the guest's page table.  pa is true physical.

   Return 0 if not.  Else return 1 and set level to 0 for page
   directory, 1 for page entry; set entry to the entry number
   referenced by va (0..1023 for level==0, 0..1M-1 for level==1).  */

int is_pt(u_int pa, u_int *level, u_int *entry)
{
    Bit32u pde;
    Bit32u cr3;
    u_int page = PGNO(pa);
    u_int j;
    Set *set = &vmstate.cr3;

    ASSERT(level && entry);

    for(set_iter_init(set); set_iter_get(set, &cr3); set_iter_next(set)) {
	if (gp2hp(PGNO(cr3)) == page) {
	    *level = 0;
	    *entry = ((pa & 0xffc) >> 2);
	    return 1;
	}
	
	for (j=0; j<1024; j++) {
	    pde = *((Bit32u*)cr3+j);
	    if (pde & 1) {
		if (gp2hp(PGNO(pde)) == page) {
		    *level = 1;
		    *entry = j*1024 + ((pa & 0xffc) >> 2);
		    return 1;
		}
	    }
	}
    }

    return 0;
}

int get_host_pte(u_int lina, Bit32u *pte)
{
    int err = 0;

    ASSERT(pte);

    *pte = sys_read_pte(lina, 0, vmstate.eid, &err);
    if (err == -E_NOT_FOUND) {
	err = 0;
	*pte = 0;
    }

    if (err) {
	error_mem("sys_read_pte(0x%x, 0, 0x%x, ...) returned 0x%08x, err=%d\n", lina, sys_geteid(), *pte, err);
	leaveemu(ERR_MEM);
    }

    ASSERT(PGNO(*pte) < PHYSICAL_PAGES);
    return 0;
}


/*
  Set the pte for va to hpte.

  If hpte is present, gpte must be valid.  gpte is used for tracking
  protected structures.  It must be consistent with hpte.
*/
static Bit32u set_pte(u_int va, Bit32u gpte, Bit32u hpte)
{
    int erc, match_offset;
    u_int gpa = (gpte & ~PGMASK) | (va & PGMASK);

    ASSERT(IS_GUEST_PHYS(gpte));
    ASSERT(IS_HOST_PHYS(hpte));
    ASSERT((hpte&1) == 0 || gp2hp(PGNO(gpte)) == PGNO(hpte));

    if (hpte & 1) {
	u_int tmp_level, tmp_entry;
	hpte |= PG_GUEST;

	/* guest page table may contain a mapping for itself.  Do not let
	   it unprotect itself when we parse the guest page table.
	*/
	if (!set_is_empty(&vmstate.cr3) && is_pt(hpte, &tmp_level, &tmp_entry)) {
	    hpte = hpte & ~2;  /* read only */
	}

	/* If we're inserting another mapping for some protected structure
	   (idt, gdt, ...?) then mark this readonly too.
	*/
	if (id_pa_readonly(gpa, NBPG, &match_offset)!=0) {
	    add_readonly(va + match_offset, gpa + match_offset);
	    hpte = hpte & ~2;  /* read only */
	}
    }

    if ((erc=sys_self_insert_pte(0, hpte, va)) < 0) {
	if (erc == -E_BUFFER) {
	    error_mem("0x%08x failed to insert because of buffer\n", va);
	} else {
	    dprintf("set_pte( 0x%08x -> 0x%08x) failed with %d\n", va, hpte, erc);
	    leaveemu(ERR_PT);
	}
    }
    debug_mem("set_pte(0x%08x -> 0x%08x) succeeded\n", va, hpte);

    if (! (hpte & 1)) {
	if (id_pa_readonly(gpa, NBPG, &match_offset)!=0) {
	    remove_readonly(va + match_offset, gpa + match_offset);
	}

	if ((erc = sys_pt_free(va, 0, vmstate.eid)) && va) {
	    /* xok might refuse to free va 0, assuming that's a bug.
	       ignore that case. */
	    dprintf("sys_pt_free(%x,...) failed with %d during set_pte(%x, %x)\n", va, erc, va, hpte);
	    leaveemu(ERR_PT);
	}
    }

    return 0;
}


/*
  guest virtual to guest (ie, fake) physical
*/

Bit32u guest_pte(Bit32u va)
{
    Bit32u cr3, pde, pte;
    Bit32u *pt;
    int paging = (vmstate.cr[0] & PG_MASK);

    ASSERT(paging || va < PHYSICAL_MEGS_RAM*1024*1024);
    if (! paging) {
	return va;
    }

    ASSERT(set_get_any(&vmstate.cr3, &cr3)==0);			/* page table defined? */
    cr3 = vmstate.cr[3];
    pde = cr3 + ((va>>20) & 0xffc);
    ASSERT(verify_lina(pde, ~PG_W)==0);				/* page dir readable? */
    pt = (Bit32u*)(*(Bit32u*)pde);
    if (! ((Bit32u)pt&1))
	return 0;
    pt = (Bit32u*)((Bit32u)pt & PG_BASE);
    ASSERT(verify_lina(((Bit32u)pt) & PG_BASE, ~PG_W)==0);	/* page table page readable? */
    pte = pt[((Bit32u)va>>12) & 0x3ff];
    return pte;
}


inline u_int virtual_to_host_phys(u_int v)
{
    return gp2hp(guest_pte(v)/NBPG)*NBPG + (v & 0xfff);
}

inline u_int host_phys_to_guest_phys(u_int h)
{
    ASSERT(IS_HOST_PHYS(h));
    return hp2gp(h/NBPG)*NBPG + (h & 0xfff);
}

inline u_int guest_phys_to_host_phys(u_int g)
{
    ASSERT(IS_GUEST_PHYS(g));
    return gp2hp(g/NBPG)*NBPG + (g & 0xfff);
}


/*
   host page to guest page mapping
*/
Bit32u hp2gp(Bit32u rp)
{
    Bit32u i;

    ASSERT(vmstate.gp2hp);
    ASSERT(rp < PHYSICAL_PAGES);

    for(i=0; i<vmstate.ppages; i++) {
	ASSERT(vmstate.gp2hp[i] < PHYSICAL_PAGES);
	if (vmstate.gp2hp[i] == rp)
	    return i;
    }

    dprintf("hp2gp(%d) failed\n", rp);
    leaveemu(ERR_PT);

    return -1;
}

/*
   guest page to host page mapping
*/
inline Bit32u gp2hp(Bit32u fp)
{
#ifndef NDEBUG
    ASSERT(vmstate.gp2hp);
    if (! (fp<vmstate.ppages))
	printf("fp  %x\n", fp);
    ASSERT(fp < vmstate.ppages);
    if (! (vmstate.gp2hp[fp] < PHYSICAL_PAGES))
	printf("%x %x\n", fp, vmstate.gp2hp[fp]);
    ASSERT(vmstate.gp2hp[fp] < PHYSICAL_PAGES);
#endif

    return (vmstate.gp2hp[fp]);
}


/* handle_new_pte does everything.  You tell it what change you want to make
   where in the guest's page tables, and it does that and also updates the
   host appropriately.

   pt_entry is the entry number _at_ _that_ _level_, not just on that
   particular page directory or page table.

   All PTEs of the guest to be modified must already be writable.
*/

int handle_new_pte(Bit32u * pt, u_int pt_level, u_int pt_entry, Bit32u old_pte,
		   Bit32u new_pte)
{
    u_int va = pt_entry * NBPG;

    trace_mem("handle_new_pte(%x, %x, %x, %x, %x); %s at offset %x, 0x%08x -> 0x%08x\n",
	      (u_int)pt, pt_level, pt_entry, old_pte, new_pte, pt_level ? "pte" : "pde", pt_entry, old_pte, new_pte);

    ASSERT(pt_level == 0 || pt_level == 1);
    ASSERT(pt_level==0 && pt_entry<1024 || pt_level==1 && pt_entry<1024*1024);

    if ((old_pte & 1) && (new_pte & 1)) {
	/* modifying present with present */
	
	Bit32u old_page = PGNO(old_pte);
	Bit32u new_page = PGNO(new_pte);
	
	if (old_page == new_page) {
	    if (pt_level==0) {
		/* hopefully this is benign... */
		error_mem("can't yet change permissions on pde\n");
	    } else {
		/* cool, just changing some permissions. */
		/* could opt. with lighter weight set_pte_flags if they're not unprotecting or such ... */
		/* set_pte(va, (gp2hp(new_page) << PGSHIFT) | (new_pte & ~PG_BASE)); */
		set_pte(va, new_pte, (gp2hp(new_page) << PGSHIFT) | (new_pte & ~PG_BASE));
	    }
	} else if (pt_level==0) {
	    /* replacing one pde with another */
	    debug_mem("replacing a page table at 0x%08x with 0x%08x\n", old_pte, new_pte);
	    if (new_pte & PG_PS) {
		dprintf("4M page tables not implemented\n");
		leaveemu(ERR_UNIMPL);
	    }
	    replace_page_table(&pt[pt_entry & 0x3ff], new_pte);
	} else {
	    /* replacing one pte with another */
	    set_pte(va, new_pte, (gp2hp(new_page) << PGSHIFT) | (new_pte & ~PG_BASE));
	}
    } else if (! (new_pte & 1)) {
	/* setting not present */
	if (old_pte & 1) {
	    if (pt_level==0) {
		debug_mem("setting not present  0x%08x\n", old_pte);
		remove_page_table(old_pte & ~(PGMASK), pt_entry & 0xfffffc00);
	    } else {
#ifndef NDEBUG
		int err;
		Bit32u pte;
		err = get_host_pte(va, &pte);
		ASSERT(pte & 1);
		ASSERT(err==0);
#endif
		set_pte(va, 0, 0);
	    }
	}
    } else {
	/* adding new */
	
	if (pt_level == 0) {
	    if (new_pte & PG_PS) {
		dprintf("4M page tables not implemented\n");
		leaveemu(ERR_UNIMPL);
	    }
	    add_page_table(new_pte & ~PGMASK, pt_entry & 0xfffffc00);
	} else {
	    Bit32u new_page = PGNO(new_pte);
	    set_pte(va, new_pte, (gp2hp(new_page) << PGSHIFT) | (new_pte & ~PG_BASE));
	}
    }

    return 0;
}

void handle_pte(Bit32u * pt, u_int pt_level, u_int pt_entry, Bit32u old_pte,
	       Bit32u new_pte)
{
    handle_new_pte(pt, pt_level, pt_entry, old_pte, new_pte);
}


/* Add mappings in guest's page table pointed to by pt to the host,
   treating all entries as new.  Do not touch the guest's page tables.

   pt_entry is the offset (at that level) of pt[0].
   pt is a linear address.
*/

void add_page_table(Bit32u pt, u_int pt_entry)
{
    const int pt_level = 1;  /* we're in the page table, not page directory */
    const Bit32u old_pte = 0;  /* and they better not already exist. */
    Bit32u new_pte;
    u_int i;

    trace_mem("in add_page_table(0x%x, 0x%x)\n", pt, pt_entry);

    ASSERT((pt & (NBPG-1)) == 0);  /* must be page aligned */
    ASSERT((pt_entry & 0x3ff) == 0); /* pt_entry must be at beginning of a pt */

    protect_range(pt, NBPG);
    for (i=pt_entry; i<pt_entry+1024; i++) {
	new_pte = ((Bit32u*)pt)[i-pt_entry];
	debug_mem("pt=%x  pte[%x]=0x%08x\n", pt, i, new_pte);
	if (handle_new_pte((Bit32u*)pt, pt_level, i, old_pte, new_pte) != 0) {
	    dprintf("failed to add new pte[%x] <- 0x%08x, pt=%x, pt_entry=%x\n", i&0x3ff, new_pte, (u_int)pt, pt_entry);
	    leaveemu(ERR_PT);
	}
    }
}


/* Replace one page table with another, changing host mappings as approprite.
   Only modification to guest page tables is the single PDE changed.

   entry is a pointer to the pde to be modified.
*/

void replace_page_table(Bit32u *entry, Bit32u new_pde)
{
    Bit32u *old_pt, *new_pt;
    u_int i;

    ASSERT(((u_int)entry & 3) == 0);

    old_pt = (Bit32u*)((*entry) & ~PGMASK);
    new_pt = (Bit32u*)(new_pde  & ~PGMASK);

    ASSERT(((*entry) & 1) && (new_pde & 1));

    protect_range((u_int)new_pt, NBPG);
    for (i=0; i<1024; i++) {
	if (handle_new_pte(new_pt, 1, i, old_pt[i], new_pt[i]) != 0) {
	    dprintf("handle_new_pte(%x, 1, %d, %x, %x) failed in replace_page_table(%x, %x)\n",
		    (u_int)new_pt, i, (u_int)old_pt[i], (u_int)new_pt[i], (u_int)entry, new_pde);
	    leaveemu(ERR_PT);
	}
    }
    unprotect_range((u_int)old_pt, NBPG);
    
    unprotect_range((u_int)entry, 4);
    /* page table stores linear addresses, not offset from base */
    if (set_memory_lina((u_int)entry, new_pde, 4) != 0) {
	dprintf("set_memory_lina(%x, %x, 4) failed to set new pde\n", (u_int)entry, new_pde);
	leaveemu(ERR_PT);
    }
    protect_range((u_int)entry, 4);
}


/* Remove mappings in guest's page table pointed to by pt from the host.
   Do not modify guest entries.

   pt_entry is the offset (at that level) of pt[0].
*/

void remove_page_table(Bit32u pt, u_int pt_entry)
{
    const int pt_level = 1;  /* we're in the page table, not page directory */
    const Bit32u new_pte = 0;
    Bit32u old_pte;
    u_int i;

    ASSERT((pt & PGMASK) == 0);  /* must be page aligned */
    ASSERT((pt_entry & 0x3ff) == 0); /* pt_entry must be at beginning of a pt */

    debug_mem("removing page table, pt = 0x%08x\n", pt);
    for (i=pt_entry; i<pt_entry+1024; i++) {
	old_pte = ((Bit32u*)pt)[i-pt_entry];
	if (handle_new_pte((Bit32u*)pt, pt_level, i, old_pte, new_pte) != 0) {
	    dprintf("failed to remove pte[%d] = 0x%08x\n", i, new_pte);
	    leaveemu(ERR_PT);
	}
    }
    unprotect_range(pt, NBPG);
}

int add_page_directory(Bit32u gcr3)
{
    u_int i;

    debug_mem("add_page_directory using cr3=0x%08x\n", gcr3);

    set_add(&vmstate.cr3, gcr3);
    protect_range(gcr3, NBPG);

    for (i=0; i<1024; i++) {
	Bit32u pde = ((Bit32u*)gcr3)[i];
	if (pde & 1) {
	    debug_mem("pde[%d]==0x%08x\n", i, pde);
	    add_page_table(pde & PG_BASE, i*1024);
	}
    }

    return 0;
}

int remove_page_directory(Bit32u gcr3)
{
    u_int i;

    for (i=0; i<1024; i++) {
	Bit32u pde = ((Bit32u*)gcr3)[i];
	if (pde & 1) {
	    debug_mem("removing page dir ent 0x%08x\n", pde);
	    remove_page_table(pde & PG_BASE, i*1024);
	}
    }
    unprotect_range(gcr3, NBPG);
    set_del(&vmstate.cr3, gcr3);

    return 0;
}

#ifndef NDEBUG
void verify_guest_pt(void)
{
    Bit32u h_pte, g_pte;
    Bit32u *pt;
    u_int va;
    int err;

    if (! (vmstate.cr[0] & PG_MASK))
	return;

    for (va=0; va<vmstate.ppages*NBPG; va+=NBPG) {
	pt = (Bit32u*)(*(Bit32u*)(vmstate.cr[3] + ((va>>20) & 0xffc)));
	if (! ((Bit32u)pt&1))
	    continue;
	pt = (Bit32u*)((Bit32u)pt & PG_BASE);
	g_pte = pt[((Bit32u)va>>12) & 0x3ff];
	if (! (g_pte & 1))
	    continue;

	/* if guest VA present, host VA must be present. */
	err = get_host_pte(va, &h_pte);
	ASSERT(err==0);

	/* every present guest VA must eventually point to same
	   physical page as the VA in host's page table. */
	ASSERT(gp2hp(PGNO(guest_pte(va))) == PGNO(h_pte));
    }
}
#endif

int unmap_pages(unsigned int startpage, unsigned int numpages)
{
    int i, erc;

    /* FIXME  Yes, I would prefer to use sys_self_insert_pte_range too, but
       it faults. */
#if 0
    sys_self_insert_pte_range(0, i*NBPG, 0  ... 
#else
    for (i = 0; i < numpages; i++)
	erc = sys_self_insert_pte(0, i*NBPG, 0);
#endif

    return 0;
}

int map_pages(unsigned int startpage, unsigned int numpages)
{
    int i, erc;
    u_int numcompleted=0;
    Pte *ptes;

    debug_mem("map_pages for %x pages starting at page %x\n", numpages, startpage);

    ptes = alloca(numpages * sizeof(Pte));
    if (!ptes) {
	dprintf("alloca failed for map_pages");
	leaveemu(ERR_MEM);
    }

    for (i = 0; i < numpages; i++)
	ptes[i] = PG_U | PG_W | PG_P | PG_GUEST;	/* user, writable, present */

    debug_mem("going to do insert_pte_range in map_pages\n");
    if ((erc = sys_self_insert_pte_range(0, ptes, numpages, startpage*NBPG,
					 &numcompleted)) < 0
	|| numcompleted!=numpages) {
	dprintf("failed to insert %d pages for va 0x%x through 0x%x\ninserted %d, erc=%d\n",
		numpages, startpage*NBPG, (startpage+numpages)*NBPG-1,
		numcompleted, erc);
	leaveemu(ERR_PT);
    } else {
	debug_mem("0x%x pages of memory mapped, VA 0x%08x - 0x%08x\n",
		numpages, startpage*NBPG, (startpage+numpages)*NBPG-1);
    }	

    return 0;
}

/*
   finds an unused section of the virtual address space and claims it.
   args:
     start - (suggested?) address to start searching
     len - required length in bytes
     requirestart - start is a hint or required
   returns:
     -1 if unable to find a suitable spot
     VA otherwise
 */

Bit32u *_find_va(Bit32u start, Bit32u len, int requirestart)
{
    int i;
    int startpage = PGNO(start);
    int fend=-1, fstart=-1, flen=0;

    len = PGNO(PGROUNDUP(len));
    start = PGNO(start);

    for (i=startpage; i!=0; i--) {
	Bit32u pte;

	if (get_host_pte(i*NBPG, &pte)) {
	    dprintf("failed to get_host_pte(0x%08x) in _find_va\n", i*NBPG);
	    leaveemu(ERR_PT);
	}
	if ((pte & 1) == 0) {
	    if (fend == -1) {
		fend = i;
		flen = 0;
	    }
	    flen ++;
	    if (flen == len) {
		fstart = i;
		break;
	    }
	} else {
	    fend = -1;
	    fstart = -1;
	    if (requirestart)
		break;
	}
    }

    if (fstart != -1) {
	map_pages(fstart, flen);
	return (Bit32u *)(fstart*NBPG);
    }

    error_mem("didn't find %d contiguous virtual pages at 0x%08x (%s)\n",
	      len, start*NBPG,
	      requirestart ? "required start" : "suggested start");
    return (Bit32u *)-1;
}
