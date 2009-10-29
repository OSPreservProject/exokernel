#include "opa.h"
#include "debug.h"
#include "types.h"
#include "descriptors.h"
#include "handler_utils.h"
#include "emu.h"
#include "tss.h"
#include "memutil.h"

/* loads the (virtual) task register with a TSS pointed to by the
   selector.  Does all appropriate error checking. */

int load_tr(Bit16u selector)
{
    struct seg_descr desc;
    if (get_descriptor(selector, (struct descr *)&desc))
	return -1;
    if (desc.type != STS_T32A || SEL_IS_LOCAL(selector)) {
	set_guest_exc(13, selector);
	return -1;
    }		    
    if (! desc.p) {
	set_guest_exc(11, selector);
	return -1;
    }
    
#if 1
    {
	int err;
	
	if ((err = sys_set_tss(0, selector))) {
	    dprintf("sys_set_tss returned %d\n", err);
	    leaveemu(ERR_DESCR);
	}
    }
#endif

    vmstate.tr_sel = selector;
    vmstate.tr_base = SEG_BASE(&desc);
    vmstate.tr_limit = SEG_LIMIT(&desc);
    vmstate.tr_loaded = 1;

    return 0;
}



void save_to_tss(struct Ts *tss)
{
    tss->ts_eip		= REG(eip);
    tss->ts_eflags	= REG(eflags);
    tss->ts_eax		= REG(eax);
    tss->ts_ecx		= REG(ecx);
    tss->ts_edx		= REG(edx);
    tss->ts_ebx		= REG(ebx);
    tss->ts_esp		= REG(esp);
    tss->ts_ebp		= REG(ebp);
    tss->ts_esi		= REG(esi);
    tss->ts_edi		= REG(edi);
    tss->ts_es		= REG(es );
    tss->ts_cs		= REG(cs );
    tss->ts_ss		= REG(ss );
    tss->ts_ds		= REG(ds );
    tss->ts_fs	 	= REG(fs );
    tss->ts_gs		= REG(gs );
}

void restore_from_tss(struct Ts *tss)
{
    REG(eip) = tss->ts_eip;
    REG(eflags) = tss->ts_eflags;
    REG(eax) = tss->ts_eax;
    REG(ecx) = tss->ts_ecx;
    REG(edx) = tss->ts_edx;
    REG(ebx) = tss->ts_ebx;
    REG(esp) = tss->ts_esp;
    REG(ebp) = tss->ts_ebp;
    REG(esi) = tss->ts_esi;
    REG(edi) = tss->ts_edi;
    REG(es ) = tss->ts_es;
    REG(cs ) = tss->ts_cs;
    REG(ss ) = tss->ts_ss;
    REG(ds ) = tss->ts_ds;
    REG(fs ) = tss->ts_fs;
    REG(gs ) = tss->ts_gs;
}

/*
  Switches tasks by loading state from the TSS referred to through the
  TSS descriptor.  Assumes privilege rules have already been checked
  and are okay.  Assumes TSS descriptor has been checked and is okay.

  "selector" is the selector for the "tss" descriptor.

  returns 0 if okay, -1 on exception.  */

int switch_tasks(Bit16u selector, struct seg_descr *tss)
{
    struct Ts *nts, *ots;
    struct seg_descr *tss_descr;
    int err;

    ASSERT(tss->p);
    ASSERT(SEG_LIMIT(tss) < 0x67);

    /* FIXME: "check that the old TSS, new TSS, and all segment
       descriptors used in the task switch are paged into system
       memory." Not a problem for Linux but maybe for others. */

    ASSERT(vmstate.tr_loaded);
    ots = (struct Ts*)vmstate.tr_base;
    nts = (struct Ts*)SEG_BASE(tss);

    save_to_tss(ots);

    load_tr(selector);
    
    /* set busy flag on selector's descriptor */
    ASSERT((selector & ~7) == 0);
    ASSERT(vmstate.g_gdt_loaded);
    ASSERT((selector & ~7) >= vmstate.g_gdt_limit);
    err = set_get_any(&vmstate.g_gdt_base, (u_int*)&tss_descr);
    ASSERT(!err);
    tss_descr += (selector >> 3);
    tss_descr->type |= 2;

    vmstate.cr[0] |= TS_MASK;

    restore_from_tss(nts);
    load_ldtr(nts->ts_ldt);
    change_cr3(nts->ts_cr3);

    return -1;
}


/*  This code kind of follows the pseudo code for CALL in PPro Vol 2.

    return 0 if okay, -1 if guest thew exception.
*/

int gate(unsigned char **lina, Bit16u selector, Bit32u offset)
{
    struct gate_descr sg;
    u_int rpl = selector & 7;
    int is_tg, is_tss, is_cg, is_cs;

    if (get_descriptor(selector, (struct descr *)&sg))
	return -1;

    is_tg  = DESC_IS_TASKGATE(&sg);
    is_tss = DESC_IS_TSS(&sg);
    is_cg  = DESC_IS_CALLGATE(&sg);
    is_cs  = DESC_IS_CODE(&sg);

    if (! (is_tg || is_tss || is_cg || is_cs)) {
	set_guest_exc(EXC_GP, selector);
	return -1;
    }

    if (is_tg) {
	struct seg_descr tss;

	if (sg.dpl < opa.cpl || sg.dpl < rpl) {
	    set_guest_exc(EXC_GP, selector);
	    return -1;
	}
	if (! sg.p) {
	    set_guest_exc(EXC_NP, selector);
	    return -1;
	}
	if (get_tss_descriptor(selector, &tss, 1) ||
	    switch_tasks(selector, &tss))
	    return -1;
	if (check_eip(REG(cs), REG(eip)))
	    return -1;
    } else if (is_tss) {
	struct seg_descr *tss = (struct seg_descr *)&sg;

	if (tss->dpl < opa.cpl || tss->dpl < rpl || tss->type & 2) {
	    set_guest_exc(EXC_GP, selector);
	    return -1;
	}
	if (! tss->p) {
	    set_guest_exc(EXC_NP, selector);
	    return -1;
	}
	if (switch_tasks(selector, tss))
	    return -1;
	if (check_eip(REG(cs), REG(eip)))
	    return -1;
    } else if (is_cs) {
	struct seg_descr *cs = (struct seg_descr *)&sg;
	u_int tmp_eip = offset & masks[opa.opb];
	if (cs->type & 4) {
	    /* CONFORMING-CODE-SEGMENT */
	    u_int esp;

	    if (cs->dpl > opa.cpl) {
		set_guest_exc(EXC_GP, selector);
		return -1;
	    }
	    if (! cs->p) {
		set_guest_exc(EXC_NP, selector);
		return -1;
	    }
	    
	    esp = make_lina(REG(ss), REG(esp));
	    if (check_push_available(esp, 2*opa.opb))	/* docs are screwy here... */
		return -1;
	    if (check_eip(selector, tmp_eip))
		return -1;
	    esp -= opa.opb; set_memory_lina(esp, REG(cs), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(eip), opa.opb);
	    REG(cs) = ((selector & 0xfffc) | opa.cpl);
	    REG(eip) = tmp_eip;
	} else {
	    /* NONCONFORMING-CODE-SEGMENT */
	    u_int esp;

	    if (rpl > opa.cpl || cs->dpl != opa.cpl) {
		set_guest_exc(EXC_GP, selector);
		return -1;
	    }
	    if (! cs->p) {
		set_guest_exc(EXC_NP, selector);
		return -1;
	    }
	    
	    esp = make_lina(REG(ss), REG(esp));
	    if (check_push_available(esp, 2*opa.opb))	/* docs are screwy here... */
		return -1;
	    if (check_eip(selector, tmp_eip))
		return -1;
	    esp -= opa.opb; set_memory_lina(esp, REG(cs), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(eip), opa.opb);
	    REG(cs) = ((selector & 0xfffc) | opa.cpl);
	    REG(eip) = tmp_eip;
	}
    } else if (is_cg) {
	struct seg_descr cs;

	if (sg.dpl < opa.cpl || sg.dpl < rpl || sg.type & 2) {
	    set_guest_exc(EXC_GP, selector);
	    return -1;
	}
	if (! sg.p) {
	    set_guest_exc(EXC_NP, selector);
	    return -1;
	}
	if (sg.segsel < 4 || sg.segsel > vmstate.g_gdt_limit) {
	    set_guest_exc(EXC_GP, sg.segsel);
	    return -1;
	}
	if (get_descriptor(sg.segsel, (struct descr *)&cs))
	    return -1;
	if (! DESC_IS_CODE(&cs) || cs.dpl > opa.cpl) {
	    set_guest_exc(EXC_GP, sg.segsel);
	    return -1;
	}
	if (! cs.p) {
	    set_guest_exc(EXC_NP, sg.segsel);
	    return -1;
	}
	if ((cs.type & 4) && cs.dpl < opa.cpl) {
	    /* MORE-PRIVILEGE */

	    leaveemu(ERR_UNIMPL); /* FIXME */
	} else {
	    /* SAME-PRIVILEGE */
	    u_int esp;
	    u_int tmp_eip = GATE_OFFSET(&sg) & masks[opa.opb];

	    esp = make_lina(REG(ss), REG(esp));
	    if (check_push_available(esp, 2*opa.opb))	/* docs are screwy here... */
		return -1;
	    if (check_eip(sg.segsel, tmp_eip))
		return -1;
	    esp -= opa.opb; set_memory_lina(esp, REG(cs), opa.opb);
	    esp -= opa.opb; set_memory_lina(esp, REG(eip), opa.opb);
	    REG(cs) = ((sg.segsel & ~3) | opa.cpl);
	    REG(eip) = tmp_eip;
	}
    }

    return 0;
}
