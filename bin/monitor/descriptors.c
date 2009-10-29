#include "opa.h"
#include "types.h"
#include "debug.h"
#include "emu.h"
#include "pagetable.h"
#include "memutil.h"
#include "handler_utils.h"
#include "descriptors.h"
#include "set.h"
#include "int.h"


/*
  does any memory in [pa .. pa+len) overlap with a read only region?
*/

u_int id_pa_readonly(u_int pa, u_int len, int *match_offset)
{
    ASSERT(match_offset);

    /* Cheating a bit.  Assuming guest will allocate its structures on
       contiguous physical pages.  Don't know why it wouldn't.  Can't
       assume this about the mapping to host pages, which is why we
       require guest. */
    ASSERT(IS_GUEST_PHYS(pa));

    /* technically, having vmstate.g_?dt_limit == 0 is valid (means a
       1 byte table, which would be weird), but usually it means it's
       loaded with a dummy value.  No sense protecting that.
    */

    if (vmstate.g_gdt_loaded && vmstate.g_gdt_limit &&
	pa+len > vmstate.g_gdt_pbase && pa <= vmstate.g_gdt_pbase+vmstate.g_gdt_limit) {
	*match_offset = vmstate.g_gdt_pbase-pa;
	return PROT_G_GDT;
    }
    if (vmstate.g_ldt_loaded && vmstate.g_ldt_limit &&
	pa+len > vmstate.g_ldt_pbase && pa <= vmstate.g_ldt_pbase+vmstate.g_ldt_limit) {
	*match_offset = vmstate.g_ldt_pbase-pa;
	return PROT_G_LDT;
    }
    return 0;
}

/* add lina to the appropriate set, by pa. */

void add_readonly(u_int lina, u_int pa)
{
    u_int id;
    int match_offset;

    ASSERT(IS_GUEST_PHYS(pa));
    ASSERT((lina&PGMASK) == (pa&PGMASK));	/* I'd expect them to be at the same offset in the page! */

    id=id_pa_readonly(pa, 1, &match_offset);
    if (id==0)
	return;
    if (id==PROT_G_GDT)
	set_add(&vmstate.g_gdt_base, lina);
    if (id==PROT_G_LDT)
	set_add(&vmstate.g_ldt_base, lina);
}

void remove_readonly(u_int lina, u_int pa)
{
    u_int id;
    int match_offset;

    ASSERT(IS_GUEST_PHYS(pa));
    ASSERT((lina&PGMASK) == (pa&PGMASK));	/* I'd expect them to be at the same offset in the page! */

    id=id_pa_readonly(pa, 1, &match_offset);
    if (id==0)
	return;
    if (id==PROT_G_GDT)
	set_del(&vmstate.g_gdt_base, lina);
    if (id==PROT_G_LDT)
	set_del(&vmstate.g_ldt_base, lina);
}

int id_va_readonly(u_int lina)
{
    u_int result;

    if (set_exists_within_delta(&vmstate.g_gdt_base, lina, vmstate.g_gdt_limit, &result))
	return PROT_G_GDT;
    if (lina >= vmstate.h_gdt_base && lina < vmstate.h_gdt_base+vmstate.h_gdt_limit)
	return PROT_H_GDT;
    if (set_exists_within_delta(&vmstate.g_ldt_base, lina, vmstate.g_ldt_limit, &result))
	return PROT_G_LDT;
    return 0;
}

int id_va_readonly_page(u_int lina)
{
    if (set_exists_within_delta_rounded(&vmstate.g_gdt_base, lina, vmstate.g_gdt_limit))
	return PROT_G_GDT;
    if (lina >= PGROUNDDOWN(vmstate.h_gdt_base) && lina < PGROUNDUP(vmstate.h_gdt_base+vmstate.h_gdt_limit))
	return PROT_H_GDT;
    if (set_exists_within_delta_rounded(&vmstate.g_ldt_base, lina, vmstate.g_ldt_limit))
	return PROT_G_LDT;
    return 0;
}


void _print_dt_mapping(u_int base, u_int pbase, u_int limit, const char *label)
{
    Bit32u pte;
    ASSERT(label);

    printf("%s base:lim    0x%08x:0x%04x -> 0x%08x -> ", label, base, limit, pbase);
    if (get_host_pte(base, &pte))
	printf("get_host_pte failed\n");
    else
	printf("0x%08x\n", pte);
}

void _print_dt_mappings(Set *set, u_int pbase, u_int limit, const char *label)
{
    u_int base;
    ASSERT(set && label);

    for(set_iter_init(set); set_iter_get(set, &base); set_iter_next(set)) {
	_print_dt_mapping(base, pbase, limit, label);
    }
}

void print_dt_mappings(void)
{
    if (vmstate.g_gdt_loaded)
	_print_dt_mappings(&vmstate.g_gdt_base, vmstate.g_gdt_pbase, vmstate.g_gdt_limit, "gdt");
    if (vmstate.g_ldt_loaded)
	_print_dt_mappings(&vmstate.g_ldt_base, vmstate.g_ldt_pbase, vmstate.g_ldt_limit, "ldt");
    if (vmstate.g_idt_loaded)
    _print_dt_mapping(vmstate.g_idt_base, vmstate.g_idt_pbase, vmstate.g_idt_limit, "idt");
}

void print_dt_entry(u_int i, const struct descr *d)
{
    int dtype;

    ASSERT(i<256);

    d += i;
    printf(" dt[0x%04x] | 0x%08x  ", i, *((u_int*)d+1));
    if (DESC_IS_DATA(d)) {
	printf("data segment descriptor");
	dtype = DESC_SEG;
    } else if (DESC_IS_CODE(d)) {
	printf("code segment descriptor");
	dtype = DESC_SEG;
    } else if (DESC_IS_TSS(d)) {
	printf("TSS");
	dtype = DESC_SEG;
    } else if (DESC_IS_BUSY_TSS(d)) {
	printf("busy TSS");
	dtype = DESC_SEG;
    } else if (DESC_IS_TSS(d)) {
	printf("task gate");
	dtype = DESC_GATE;
    } else if (DESC_IS_INTGATE(d)) {
	printf("interrupt gate");
	dtype = DESC_GATE;
    } else if (DESC_IS_TRAPGATE(d)) {
	printf("trap gate");
	dtype = DESC_GATE;
    } else if (DESC_IS_CALLGATE(d)) {
	printf("trap gate");
	dtype = DESC_GATE;
    } else if (DESC_IS_LDT(d)) {
	printf("LDT descriptor");
	dtype = DESC_SEG;
    } else {
	printf("unknown type of gate");
	return;
    }
    printf(" (0x%02x)\n", DESC_TYPE(d));
    
    printf("0x%08x  | 0x%08x  dpl: %d ", (u_int)d, *(u_int*)d, d->dpl);
    if (dtype == DESC_SEG) {
	struct seg_descr *sd = (struct seg_descr *)d;
	printf("base: 0x%08x limit: 0x%08x g: %d\n", SEG_BASE(sd), SEG_LIMIT(sd), sd->g);
    } else {
	struct gate_descr *sg = (struct gate_descr *)d;
	printf("selector: 0x%08x", sg->segsel);
	if (! DESC_IS_TASKGATE(sg)) {
	    printf(" offset: 0x%08x", GATE_OFFSET(sg));
	}
	printf("\n");
    }
}


u_int _get_base(u_int seg)
{
    u_int base;

    if (opa.pe) {
	struct seg_descr sd;
	if (get_descriptor(seg, (struct descr*)&sd)) {
	    error_mem("get_descriptor(%x, %x) failed in get_base(%x)\n", seg, (u_int)&sd, seg);
	    return -1;
	}
	ASSERT(! (DESC_IS_TRAPGATE(&sd) || DESC_IS_INTGATE(&sd) || DESC_IS_TASKGATE(&sd)));
	base = SEG_BASE(&sd);
    } else {
	base = ((seg&0xffff)<<4);
    }
    return base;
}

#ifndef NDEBUG
u_int get_base_debug(u_int seg, const char *file, u_int line)
{
    int base;
    if (opa.pe && (seg & ~7) > vmstate.g_gdt_limit) {
	dprintf("invalid get_base(%x) at %x:%x %x:%x, %s:%d\n", seg, REG(cs), REG(eip),
		REG(ss), REG(esp), file, line);
	if ((seg & ~7) == GD_UT)
	    dprintf("probably a bug in the monitor\n");
	debugger();
    }
    base = _get_base(seg);
    if (base == -1) {
	dprintf("get_base(%x) failed at %s:%d\n", seg, file, line);
	debugger();
    }

    return base;
}
#endif

u_int get_limit(u_int seg)
{
    u_int limit;

    if (opa.pe) {
	struct seg_descr sd;
	if (get_descriptor(seg, (struct descr *)&sd)) {
	    dprintf("get_descriptor(%x, %x) failed in get_limit(%x)\n", seg, (u_int)&sd, seg);
	    leaveemu(ERR_DESCR);
	}
	ASSERT(! (DESC_IS_TRAPGATE(&sd) || DESC_IS_INTGATE(&sd) || DESC_IS_TASKGATE(&sd)));
	limit = SEG_LIMIT(&sd);
    } else {
	limit = 0x10000;
    }
    return limit;
}


int merge_gdt(void)
{
    static Bit32u old_base = 0;
    static Bit16u old_limit = 0;
    static unsigned char initialized = 0;
    int i, err;
    Bit32u base;
    Bit16u limit = vmstate.g_gdt_limit;

    err = set_get_any(&vmstate.g_gdt_base, &base);
    ASSERT(err == 0);

    trace("in merge_gdt 0x%x:0x%x\n", base, limit);

    if (initialized) {
	unprotect_range(old_base, old_limit);
    }

    if (IS_PE) {
	struct seg_descr hgdt, *ggdt;

	if (limit > FREE_GDT*8) {
	    dprintf("guest gdt has %d entries; max %d allowed.\n", limit/8, FREE_GDT);
	    leaveemu(ERR_DESCR);
	}
	if (verify_lina_range(base, ~PG_W, limit)) {
	    dprintf("merge_gdt: can't read guest GDT 0x%08x-0x%08x\n", base, base+limit-1);
	    leaveemu(ERR_DESCR);
	}
	
	/* Merge guest's GDT into host's; leave guest's alone.  Change all
	   DPL=0 to DPL=1.  Also remember, limit is offset of last valid
	   byte. */
	ggdt = (struct seg_descr *)base;
	for(i=1; i < limit/8; i++) {
	    int erc;

	    hgdt = ggdt[i];
	    if (hgdt.dpl == 0)
		hgdt.dpl = 1;
	    if (erc=sys_set_gdt(0, i, (struct seg_desc*)&hgdt)) {
		error_mem("sys_set_gdt(%d, %08x, %08x) returned %d\n", i, *(u_int*)&hgdt, *((u_int*)&hgdt+1), erc);
	    }
	}

	/* unregister any old descriptors */
	if (old_limit > limit) {
	    memset(&hgdt, 0, sizeof(hgdt));
	    for(i=limit/8; i<old_limit/8; i++)
		sys_set_gdt(0, i, (struct seg_desc*)&hgdt);
	}
    }

    protect_range(base, limit);
    old_base = base;
    old_limit = limit;
    initialized = 1;

    trace("out merge_gdt\n");
    return 0;
}

static void start_new_set(Bit32u new_base, Bit16u new_limit, Bit32u new_pbase, Set *set, Bit16u old_limit)
{
    u_int i;
    u_int tmp_base;

    for(set_iter_init(set); set_iter_get(set, &tmp_base); set_iter_next(set)) {
	unprotect_range(tmp_base, old_limit);
    }
    set_init(set);
    set_add(set, new_base);
    
    /* Now we need to find out if any other virtual addresses point to
       this this same physical page, and add them too.  Good enough to
       check if guest physical == guest physical because we know that
       implies host physical == host physical.  */
    if (vmstate.cr[0] & PG_MASK) {
	Bit32u pde, pte;
	u_int j;
	for (i=0; i<1024; i++) {
	    pde = *((Bit32u*)vmstate.cr[3]+i);
	    if (pde & 1) {
		pde &= ~PGMASK;
		for (j=0; j<1024; j++) {
		    pte = *((Bit32u*)pde+j);
		    if ((pte & 1) && PGNO(pte) == PGNO(new_pbase)) {
			u_int va = (i*1024*NBPG)+(j*NBPG)+(new_base & PGMASK);
			set_add(set, va);
			protect_range(va, new_limit);
		    }
		}
	    }
	}
    }
}



int load_ldtr(Bit16u selector)
{
    struct seg_descr ldt_sd;

    if (selector == 0) {
	set_ldt(0, 0, 0);
	return 0;
    }
    
    if (get_descriptor(selector, (struct descr *)&ldt_sd))
	return -1;
    if (! DESC_IS_LDT(&ldt_sd)) {
	set_guest_exc(EXC_GP, selector);
	return -1;
    }
    if (! ldt_sd.p) {
	set_guest_exc(EXC_NP, selector);
	return -1;
    }
    
    set_ldt(selector, SEG_BASE(&ldt_sd), SEG_LIMIT(&ldt_sd));

    return 0;
}

void set_ldt(Bit16u selector, Bit32u base, Bit16u limit)
{
    Bit32u new_pbase;

    if (selector == 0) {
	vmstate.g_ldt_loaded = 0;
	set_init(&vmstate.g_ldt_base);
	sys_set_ldt(0, 0);
	return;
    }

    new_pbase = (guest_pte(base) & ~PGMASK) | (base & PGMASK);

    if (!vmstate.g_ldt_loaded || new_pbase != vmstate.g_ldt_pbase) {
	int err;
	u_int i;

	/* it's a new physical page.  flush and start over. */
	start_new_set(base, limit, new_pbase, &vmstate.g_ldt_base, vmstate.g_ldt_limit);
	vmstate.g_ldt_pbase = new_pbase;
	vmstate.g_ldt_limit = limit;

	/* FIXME: don't yet have a function to translate from guest or
	   host physical to virtual, which is what we need to handle
	   LLDT with paging off. */
	ASSERT(vmstate.cr[0] & PG_MASK);

	for(i=0; i<(vmstate.g_ldt_limit+1)/8; i++) {
	    struct seg_descr *sd = (struct seg_descr *)base + i;
	    if (sd->dpl == 0)
		sd->dpl = 1;
	}
	protect_range(vmstate.g_ldt_pbase, vmstate.g_ldt_limit);
	
	if ((err = sys_set_ldt(0, selector))) {
	    dprintf("sys_set_ldt(%d) returned %d\n", err, selector);
	    leaveemu(ERR_DESCR);
	}
    } else {
	/* same physical page. */
	set_add(&vmstate.g_ldt_base, base);
    }
    vmstate.g_ldt_limit = limit;
    vmstate.g_ldt_loaded = 1;
}

/* base must be a linear address */
void set_gdt(Bit32u base, Bit16u limit)
{
    Bit32u new_pbase;

    new_pbase = (guest_pte(base) & ~PGMASK) | (base & PGMASK);

    if (!vmstate.g_gdt_loaded || new_pbase != vmstate.g_gdt_pbase) {
	/* it's a new physical page.  flush and start over. */
	start_new_set(base, limit, new_pbase, &vmstate.g_gdt_base, vmstate.g_gdt_limit);
	vmstate.g_gdt_pbase = new_pbase;
	vmstate.g_gdt_limit = limit;
    } else {
	/* same physical page. */
	set_add(&vmstate.g_gdt_base, base);
    }
    vmstate.g_gdt_limit = limit;

    vmstate.g_gdt_loaded = 1;
    merge_gdt();
    trace("out set_gdt %x,%x\n", base, limit);
}

/* base must be a linear address */
void set_idt(Bit32u base, Bit16u limit)
{
    Bit32u new_pbase;

    new_pbase = (guest_pte(base) & ~PGMASK) | (base & PGMASK);

    vmstate.g_idt_base = base;
    vmstate.g_idt_pbase = new_pbase;
    vmstate.g_idt_limit = limit;

    vmstate.g_idt_loaded = 1;
    merge_idt();
}

/* load the descriptor from selector, expecting it to be busy if "busy" is set. */
int get_tss_descriptor(u_int selector, struct seg_descr *descr, int busy)
{    if (SEL_IS_LOCAL(selector) ||
	selector > vmstate.g_gdt_limit) {
	set_guest_exc(EXC_GP, selector);
	return -1;
    }
    if (get_descriptor(selector, (struct descr *)descr))
	return -1;
    if ((!busy && (descr->type & 2)) || (busy && !(descr->type & 2))) {
	set_guest_exc(EXC_GP, selector);
	return -1;
    }
    if (! descr->p) {
	set_guest_exc(EXC_NP, selector);
	return -1;
    }

    return 0;
}

int get_descriptor(u_int selector, struct descr *descr)
{
    struct descr *g_descr;

    ASSERT(!IS_VM86 && IS_PE);
    ASSERT(descr);

    if (SEL_IS_LOCAL(selector)) {
	/* LDT */
	if ((selector & ~7) == 0 ||
	    (selector & ~7) >= vmstate.g_ldt_limit ||
	    set_get_any(&vmstate.g_ldt_base, (u_int*)&g_descr)) {
	    set_guest_exc(EXC_GP, selector);
	    return -1;
	}
	g_descr += (selector >> 3);
    } else {
	/* GDT */
	if ((selector & ~7) == 0 ||
	    (selector & ~7) >= vmstate.g_gdt_limit ||
	    set_get_any(&vmstate.g_gdt_base, (u_int*)&g_descr)) {
	    set_guest_exc(EXC_GP, selector);
	    return -1;
	}
	g_descr += (selector >> 3);
    }
    
    *descr = *g_descr;

    return 0;
}

#if 0
int segment_writable(u_int selector, int *writable)
{
    ASSERT(writable);

    if (! IS_VM86) {
	struct descr sd;
	if (get_descriptor(selector, &sd) != 0)
	    return -1;
	ASSERT(! (DESC_IS_TRAPGATE(&sd) || DESC_IS_INTGATE(&sd) || DESC_IS_TASKGATE(&sd)));
	*writable = sd.type & 2;
    } else {
	*writable = 1;
    }
    return 0;
}
#endif
