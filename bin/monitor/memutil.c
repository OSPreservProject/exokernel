#include <xok/sys_ucall.h>
#include <stdlib.h>

#include <xok/mmu.h>

#include "config.h"
#include "debug.h"
#include "types.h"
#include "pagetable.h"
#include "memutil.h"
#include "handler_utils.h"
#include "descriptors.h"
#include "emu.h"



int verify_lina_range(u_int lina, Bit32u flags, u_int bytes)
{
    u_int i;
    debug_mem("verifying %x bytes at %x, %x - %x\n", bytes, lina, PGROUNDDOWN(lina), lina+bytes-1);
    for(i = PGROUNDDOWN(lina); i<lina+bytes; i+=NBPG) {
	if (verify_lina(i, flags))
	    return -1;
    }

    return 0;
}

/* if (flags & PG_W) check if we can write, else read */

int verify_lina(u_int lina, Bit32u flags)
{
    Bit32u pte;
    u_int cpl;
    int P, RW, US;
    int error = 0;

    ASSERT(REGS);
    cpl = GUEST_CPL;

    P = 0;
    RW = (flags & PG_W);
    US = (cpl == 3);

    if ((error = get_host_pte(lina, &pte))) {
	if (error == -E_NOT_FOUND) {
	    error = 1;
	} else {
	    error("verify_lina(0x%x, 0x%x) failed to get pte\n", lina, flags);
	    leaveemu(ERR_PT);
	}
    }
	    
#if 0
    trace_mem("lina 0x%08x maps to pte 0x%08x, %s%s%s%s%s\n", lina, pte,
	      pte & PG_A ? "A " : "- ",
	      pte & PG_D ? "D " : "- ",
	      pte & PG_U ? "U " : "S ",
	      pte & PG_W ? "RW " : "RO ",
	      pte & PG_P ? "P" : "-");
#endif
	
    if (!(pte & PG_P)) {
	error = 1;
    } else if (( !(flags & PG_W) && !(pte & PG_U) && cpl == 3) ||	/* read protection violation */
	       (  (flags & PG_W) && !(pte & PG_W) &&
		  (cpl==3 || vmstate.cr[0] & CR0_WP_MASK))) {		/* write protection violation */
	P = 1;
	error = 1;
    }

    /* FIXME: check permissions in PDE also */
	    
    if (!error)
	return 0;

    /* FIXME:  docs mention setting the A and D flags in PDE */

    vmstate.cr[2] = lina;
    set_guest_exc(14, (US << 2) + (RW << 1) + P);
    
    return -1;
}


static Bit32u set_pte_flags(u_int va, Bit32u pte)
{
    int erc;

    if ((erc=sys_self_insert_pte(0, pte, va)) < 0) {
	dprintf("set_pte_flags( 0x%08x -> 0x%08x) failed with %d\n", va, pte, erc);
	leaveemu(ERR_PT);
    }
    return 0;
}


/* protects (make RO) or unprotects (make RW) the range specified.
   Returns 0 if no change was necessary, 1 if one was made, -1 on
   error.

   base is a linear address.
*/
int _protect_range(Bit32u base, Bit16u bytes, u_int protect)
{
    u_int va, pte;
    int changed = 0;

    if (bytes==0)
	return 0;

    debug_mem("%sprotect 0x%x-0x%x (0x%x-0x%x)\n", protect ? "" : "un", base, base+bytes-1, PGROUNDDOWN(base), PGROUNDUP(base+bytes)-1);
    for (va=PGROUNDDOWN(base); va<PGROUNDUP(base+bytes); va+=NBPG) {
	int erc;
	Bit32u newpte;

	if (get_host_pte(va, &pte) != 0) {
	    dprintf("failed to get_host_pte(0x%08x) in protect_range\n", va);
	    leaveemu(ERR_MEM);
	}
	if ((protect && (pte & 2)) || (!protect && !(pte & 2))) {
	    newpte = protect ? (pte & ~2) : (pte | 2);
	    changed = 1;
	    if (erc = set_pte_flags(va, newpte)) {
		dprintf("failed to to set 0x%08x -> 0x%08x; erc=%d\n", va, newpte, erc);	
		leaveemu(ERR_MEM);
	    }
	    debug_mem("protect=%d, 0x%08x, set 0x%08x -> 0x%08x\n", protect, pte, va, newpte);
	} else {
	    debug_mem("already correctly set for 0x%08x -> 0x%08x  (protect=%d)\n", va, pte, protect);
	}
    }

    return changed;
}


inline Bit32u make_lina(Bit32u sel, Bit32u offset)
{
    if (! IS_PE)
	offset &= 0xffff;

    return get_base(sel)+offset;
}


int set_memory_lina(u_int lina, Bit32u val, int bytes)
{
    ASSERT(bytes==1 || bytes==2 || bytes==4);
    /* don't try to touch the exokernel */
    ASSERT(lina < KSTACKTOP || lina >= KSTACKTOP);

    if (verify_lina_range(lina, PG_W, bytes) != 0) {
	dprintf("set_memory_lina can't write\n");
	return -1;
    }

    if (bytes==1) {
	*(Bit8u *)lina = val;
    } else if (bytes==2) {
	*(Bit16u *)lina = val;
    } else {
	*(Bit32u *)lina = val;
    }
    return 0;
}

inline int set_memory(u_int sel, u_int off, Bit32u val, int bytes)
{
    return set_memory_lina(make_lina(sel, off), val, bytes);
}


int get_memory_lina(u_int lina, Bit32u *result, int bytes)
{
    ASSERT(bytes==1 || bytes==2 || bytes==3 || bytes==4);
    ASSERT(result);
    /* don't try to touch the exokernel */
    ASSERT(lina < KSTACKTOP || lina >= KSTACKTOP);

    if (verify_lina_range(lina, ~PG_W, bytes) != 0) {
	dprintf("get_memory_lina can't read\n");
	return -1;
    }

    if (bytes==1) {
	*result = *(Bit8u *)lina;
    } else if (bytes==2) {
	*result = *(Bit16u *)lina;
    } else if (bytes==3) {  /* yes, actually needed for lidt/lgdt */
	*result = *(Bit16u *)lina + ((*(Bit8u*)(lina+2))<<16);
    } else {
	*result = *(Bit32u *)lina;
    }

    return 0;
}

inline int get_memory(u_int sel, u_int off, Bit32u *result, int bytes)
{
    return get_memory_lina(make_lina(sel, off), result, bytes);
}
