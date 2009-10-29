#include <xok/sys_ucall.h>
#include <xok/mmu.h>

#include "config.h"
#include "opa.h"
#include "debug.h"
#include "types.h"
#include "emu.h"
#include "descriptors.h"
#include "handler_utils.h"
#include "pagetable.h"
#include "memutil.h"
#include "int.h"
#include "decode.h"


/*
  Handle guest page faults.  All those for the monitor should be
  caught and handled by libexos.
*/

int exc_14_C_handler(unsigned char *lina, Bit32u cr2)
{
    unsigned char *orig_lina = lina;
    u_int pt_level, pt_entry;
    Bit32u src, dst;	/* offsets */
    u_int bytes, srcmem, dstmem;
    int err;

    trace("in #e,%x\n", vm86s.err);

    ASSERT(vm86s.trapno==14);
    ASSERT((IS_VM86 && !opa.pe) || (!IS_VM86 && opa.pe));  /* we use VM86 flag; guest can't */
    ASSERT(! (!opa.pe && (vmstate.cr[0] & PG_MASK)));  /* paging ==> protected */
    ASSERT(opa.pe || (REG(eip) & 0xffff0000)==0);      /* seeing weirdness with high bits */

    if (vm86s.err & 8) {
	dprintf("Use of reserved bit caused #pf.  err=0x%08x, cr2=0x%08x\n",
		vm86s.err, cr2);
	goto dump;
    }
    
    if (decode_mov(&lina, &src, &dst, &bytes, &srcmem, &dstmem)!=0) {
	/* If guest failed to read in supervisor mode because
	   !present, and we failed for the same reason, don't redirect
	   exception to guest yet.  Maybe this is a memory probe or
	   something which needs to be handled.  Otherwise bail.
	*/
	if (! ((vm86s.err & 0x7)==0 && (vmstate.exc_erc & 0x3)==0))
	    goto done;
    }
    dst = make_lina(opa.seg, dst);	/* make dst a linear address.
					   src is data, so don't touch it. */

    /* Just for a sanity check, I figure we faulted trying
       to read or write... makes sense, no? */
    ASSERT((srcmem && src==cr2) || (dstmem && dst==cr2));
    ASSERT(bytes==1 || bytes==2 || bytes==4);

    if (vm86s.err & 1) {		/* protection violation */
	if (vm86s.err & 2) {                  /* write */
	    int id;

	    debug_mem("write to 0x%08x\n", dst);

	    /* If it was a write error, we must have failed to write
	       to MEMORY not a register. */
	    ASSERT(dstmem);

	    id = id_va_readonly(dst);
	    if (id == PROT_G_IDT || id == PROT_H_IDT) {
		ASSERT(id != PROT_G_IDT);  /* no longer protecting this; shouldn't fault */
		ASSERT(dst >= vmstate.h_idt_base);

		unprotect_range(dst, bytes);
		if (vmstate.g_idt_loaded && dst + bytes < vmstate.g_idt_base+vmstate.g_idt_limit) {
		    debug_mem("redirected write to guest's IDT\n");
		    err = set_memory_lina(vmstate.g_idt_base + (vmstate.h_idt_base - dst), src, bytes);
		} else {
		    /* This assumes the host IDT butts up next to usable memory.
		       Not likely, really, but for completeness. */
		    err = set_memory_lina(dst, src, bytes);
		}
		protect_range(dst, bytes);

		if (err)
		    goto done;
	    } else if (id == PROT_G_GDT || id == PROT_H_GDT) {
		u_int limit, base, offset;
		int erc;
		int guest = (id == 3);

		limit = vmstate.g_gdt_limit;
		erc = set_exists_within_delta(&vmstate.g_gdt_base, dst, vmstate.g_gdt_limit, &base);
		ASSERT(erc==1);

		if (guest) {
		    offset = dst - base;
		} else {
		    offset = dst - vmstate.h_gdt_base;
		}

		ASSERT(offset+bytes-1 <= limit);
		ASSERT(offset+bytes-1 < FREE_GDT*8);

		debug_mem("write %d bytes of 0x%x to %s's GDT at offset 0x%x\n", bytes, src, guest ? "guest" : "host", offset);
		unprotect_range(base, limit);
		/* base is linear address */
		err = set_memory_lina(base+offset, src, bytes);
		protect_range(base, limit);
		if (err)
		    goto done;

		if ((erc=sys_set_gdt(0, offset/8, (struct seg_desc *)(base + (offset & ~7))))) {
		    dprintf("sys_set_gdt(%d, %08x) returned %d\n", offset/8, base+offset, erc);
		    leaveemu(ERR_DESCR);
		}
	    } else if (id == PROT_G_LDT) {
		u_int limit, base, offset;
		struct seg_descr *sd;
		int erc;

		limit = vmstate.g_ldt_limit;
		erc = set_exists_within_delta(&vmstate.g_ldt_base, dst, vmstate.g_ldt_limit, &base);
		ASSERT(erc==1);
		offset = dst - base;

		ASSERT(offset+bytes-1 <= limit);

		debug_mem("write %d bytes of 0x%x to guest's LDT at offset 0x%x\n", bytes, src, offset);
		unprotect_range(base, limit);
		/* base is linear address */
		err = set_memory_lina(base+offset, src, bytes);
		if (err) {
		    protect_range(base, limit);
		    goto done;
		}

		sd = (struct seg_descr *)(base + (offset & ~7));
		if (sd->dpl == 0)
		    sd->dpl = 1;
		protect_range(base, limit);
	    } else if (! set_is_empty(&vmstate.cr3) &&
		       is_pt(virtual_to_host_phys(dst), &pt_level, &pt_entry)) {
		Bit32u old_pte, new_pte;
		int pte_offset;

		old_pte = *(Bit32u*)(dst & ~3);
		pte_offset = (dst & 3);
		
		debug_mem("writing guest pte at 0x%08x; level %d, entry %d, %d bytes at %d\n",
			  dst, pt_level, pt_entry, bytes, pte_offset);
		
		/* I suppose other things are possible, but for now
		   it's more likely a bug in my stuff than a strange
		   OS hitting multiple pte's. */
		ASSERT((pte_offset==0) ||
		       (pte_offset==1 && bytes!=4) ||
		       (pte_offset==2 && bytes!=4) ||
		       (pte_offset==3 && bytes==1));
		
		/* overlay their modifications on existing pte.
		   e.g.:  old_pte = 0x11111111  src = 0x22222222
		       pte_offset=0, bytes=1  -->  0x11111122
		       pte_offset=2, bytes=2  -->  0x22221111
		       pte_offset=0, bytes=4  -->  0x22222222
		 */
		new_pte = ((old_pte & ~(masks[bytes]<<(pte_offset*8))) |
			   (src     &  (masks[bytes]<<(pte_offset*8))));

		handle_pte((Bit32u*)PGROUNDDOWN(dst), pt_level, pt_entry, old_pte, new_pte);

		unprotect_range(dst, 1);
		set_memory_lina(dst, src, bytes);
		protect_range(dst, 1);
#if 0
	    } else if (is_pt(vmstate.cr3, dst, &pt_level, &pt_entry)) {
		dprintf("Guest attempting to write to host page table!\n");
		goto dump;
#endif
	    } else if (id_va_readonly_page(dst)) {
		/* some instruction tried to write just next to a
		   protected structure; since the protection
		   granularity is pages, we trapped anyway.  Since the
		   previous tests failed, we know this is not actually
		   in a read-only structure.  Go ahead and emulate it.
		   Note we don't check if it was near a page table;
		   those must be page aligned so it's not that.  */

		debug_mem("write %d bytes of 0x%x to 0x%x, next to protected structure.\n", bytes, src, dst);
		unprotect_range(dst, 1);
		err = set_memory_lina(dst, src, bytes);
		protect_range(dst, 1);
		if (err)
		    goto done;
	    } else if (opa.cpl == 3) {
		/* We don't know what to do in these situations.  Hand
                   this off to the host's PF handler.
		 */

		error("I can't handle #14,%d at eip=0x%08x, va=0x%08x\n", vm86s.err, (u_int)lina, (u_int)cr2);
		vmstate.cr[2] = cr2;
		set_guest_exc(14, vm86s.err);
		goto done;
	    } else {
		dprintf("trying to move %x to %x and i don't know why\n", src, dst);
		debugger();
		leaveemu(ERR_ASSERT);
	    }
	} else {                        /* read */
	    vmstate.cr[2] = cr2;
	    set_guest_exc(EXC_PF, vm86s.err);
	}
    } else {                            /* not present page */
	ASSERT(IS_PE);
	if (vm86s.err & 2) {                  /* write */
	    if (! vmstate.cr[0] & PG_MASK) {
		if (dst < vmstate.ppages*NBPG) {
		    /* FIXME allocate physcial page, if needed, and
                       back up REG(eip) */
		    leaveemu(ERR_UNIMPL);
		    REG(eip) -= lina-orig_lina;
		} else {
		    /* past end of faked physical memory. */
		    /* ignore the write; it might be a memory probe */
		}
	    } else {
		/* not present, write, protected, paging enabled */
		/* pass off to guest handler */
		vmstate.cr[2] = cr2;
		set_guest_exc(EXC_PF, vm86s.err);
	    }
	} else {
	    /* read */
	    if (! vmstate.cr[0] & PG_MASK) {
		if (dst < vmstate.ppages*NBPG) {
		    /* FIXME allocate physcial page and zero it, if
                       needed, and back up REG(eip) */
		    leaveemu(ERR_UNIMPL);
		    REG(eip) -= lina-orig_lina;
		} else {
		    /* past end of faked physical memory. */
		    /* pretend we read a 0 and write that; it might be a memory probe */
		    if (dstmem) {
			if (set_memory_lina(dst, src, bytes) != 0)
			    goto done;
		    } else {
			set_reg(dst, src, bytes);
		    } 
		}
	    } else {
		/* not present, read, protected, paging enabled */
		/* pass off to guest handler */
		vmstate.cr[2] = cr2;
		set_guest_exc(EXC_PF, vm86s.err);
	    }
	}
    }

 done:
    REG(eip) += lina - orig_lina + opa.prefixes;
    return 0;

 dump:
    dprintf("Failed in monitor page_fault_handler; errorcode=0x%x, cr2=0x%08x\n", vm86s.err, cr2);
    leaveemu(ERR);
    return -1;
}
