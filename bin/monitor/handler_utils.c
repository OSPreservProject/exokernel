#include "uidt.h"
#include "opa.h"
#include "config.h"
#include "debug.h"
#include "types.h"
#include "descriptors.h"
#include "emu.h"
#include "handler_utils.h"
#include "memutil.h"
#include "tss.h"
#include "repeat.h"
#include "logue.h"
#include "int.h"
#include "decode.h"
#include "pic.h"


const Bit32u masks[] = {0x0000ff00, 0x000000ff, 0x0000ffff, 0, 0xffffffff};



#if 0
int high_mem_init(void)
{
    int i, err;
    Bit32u pte;

    /* remove 64k wrap around */
    for (i = 0; i < 64*1024/NBPG; i++) {
	err = sys_self_insert_pte (0, 0, 1024*1024+i*NBPG);
	ASSERT(err==0);
    }

    /* set up rest of memory */
    for (i=1024*1024/NBPG; i < config.phys_mem_size*1024/NBPG; i++) {
	pte = PG_U | PG_W | PG_P | PG_GUEST;
	if ((err = sys_self_insert_pte (0, pte, i*NBPG))) {
	    dprintf("sys_self_insert_pte(0, %x, %x) failed with %d\n", pte, i*NBPG, err);
	    leaveemu(ERR_MEM);
	}
 	memset((void*)(i*NBPG), 0, NBPG);
	err = get_host_pte(i*NBPG, &pte);
	ASSERT(err==0);
	vmstate.gp2hp[i] = PGNO(pte);
    }

    return 0;
}
#endif


/*
   Tries to enter emulated protected mode.  Sets up memory, sets
   up vital system structures (GDT, IDT, ...), etc.

   Returns 0 on success, or -1 on clean failure (no changes to
   state).  Dies on major error.
 */

static int init_pe(void)
{
    struct seg_descr *ggdt;
    int i;
    int cs=0, ss=0;
    int err;

    trace("in init_pe\n");

#if 0
    high_mem_init();
#endif

    ASSERT(! set_is_empty(&vmstate.g_gdt_base));

    if (vmstate.g_gdt_limit > FREE_GDT*8) {
	dprintf("guest gdt too big:  %x > %x\n", vmstate.g_gdt_limit,
		FREE_GDT*8);
	leaveemu(ERR_ASSERT);
    }

    /* give them a kick to be sure they're loaded. */
    merge_gdt();
    merge_idt();

    /* Find the first executable code segment.  Let's assume this is
       the guest OS's (as is the case for Linux.)  From that we
       calculate a new CS--we can't run with the VM86 CS until it's
       explicitly reset since we can't IRET to protected mode with a
       bad CS.  So for a few instructions, we're running with the
       protected mode CS while Linux usually would be running with the
       old CS.  Similarly for SS.

       Note that a later Linux instruction does a intersegment jump to
       set CS, but since it expects to still be 16 bit (old CS) it
       uses a 0x66 prefix.  But we're 32 bit, so this instruction
       faults (not sure why it doesn't just misbehave...) which gives
       us a chance to fix it.  That's caught in the #13 handler, but
       just so you know.  */

    err = set_get_any(&vmstate.g_gdt_base, (u_int*)&ggdt);
    ASSERT(err == 0);

    for(i=1; i < vmstate.g_gdt_limit/8 && (!cs || !ss); i++) {
 	if (!cs && ggdt[i].p && DESC_IS_CODE(&ggdt[i])) {
	    u_int base = SEG_BASE(&ggdt[i]);
	    REG(eip) = (LWORD(cs)<<4)+LWORD(eip) - base;
	    REG(cs) = i*8 + 1;
	    cs = 1;
	} else	if (!ss && ggdt[i].p && DESC_IS_DATA(&ggdt[i])) {
	    u_int base = SEG_BASE(&ggdt[i]);
	    REG(esp) = (LWORD(ss)<<4)+LWORD(esp) - base;
	    REG(ss) = i*8 + 1;
	    ss = 1;
	}
    }

    if (!cs || !ss) {
	dprintf("failed to find appropriate descriptors for both CS and SS when going protected\n");
	leaveemu(ERR_DESCR);
    }

   /* we're in a state of transition.  guest doesn't yet expect CS to
      be set, but it is. */
    vmstate.prot_transition = 1;

    REG(ds) = REG(es) = REG(fs) = REG(gs) = REG(ss);
    REG(eflags) &= ~VM_MASK;

#if 0
    if (! (vmstate.cr[0] & PG_MASK)) {
	high_mem_init();
    }
#endif

#if 1           /* FIXME:  hack to manually set up PIC since IO isn't trapping */
    write_pic0(0x20, 0x11);
    write_pic1(0xa0, 0x11);
    write_pic0(0x21, 0x20);
    write_pic1(0xa1, 0x20);
    write_pic0(0x21, 0x04);
    write_pic1(0xa1, 0x02);
    write_pic0(0x21, 0x01);
    write_pic1(0xa1, 0x01);
    write_pic1(0xa1, 0xff);
    write_pic0(0x21, 0xfb);
#endif

    register_prologue();

    return 0;
}


int mov_to_cr(int cr, u_int val, u_int mask)
{
    Bit32u proposed;

    ASSERT(cr>=0 && cr<=4);

    proposed = (vmstate.cr[cr] & ~mask) | (val & mask);

    debug("mov cr%d <- 0x%08x\n", cr, proposed);
    
    if ((cr==0 && (proposed&PG_MASK && !(proposed&PE_MASK))) ||
	(cr==4 && (proposed&CR4_RESERVED_MASK))) {
	proposed = vmstate.cr[cr];
	dprintf("trying to set a bad combo of bits on CR%d: 0x%08x\n", cr, proposed);
	leaveemu(ERR_ASSERT);
    }

    if (cr==0) {
	if (proposed&PG_MASK && !(vmstate.cr[0]&PG_MASK)) {
	    ASSERT(vmstate.cr[3]!=0 && (vmstate.cr[3] & 0x00000fff) == 0);
	    /* unmap the fake physical mem first, or else it might
               show through any holes in the guest's page table. */
	    unmap_pages(0, vmstate.ppages);

	    add_page_directory(vmstate.cr[3]);
	} else if (!(proposed&PG_MASK) && (vmstate.cr[0]&PG_MASK)) {
	    /* FIXME: This is bad, bad, bad!  If we let go of pages in
	       the real page table, their contents might change or we
	       might not get them back, but the guest doesn't expect
	       that! */
	    remove_page_directory(vmstate.cr[3]);
	}

	if (proposed&PE_MASK && !(vmstate.cr[0]&PE_MASK)) {
	    vmstate.cr[0] |= 1;
	    init_pe();
	}
    } else if (cr==3 && vmstate.cr[0]&PG_MASK) {
	change_cr3(proposed);
    }
    
    vmstate.cr[cr] = proposed;

    return 0;
}



/* Sets a value (src) in memory or register (dst).
   Returns 0 if okay, -1 on exception.
*/
int emu_set(int dstmem, Bit32u dst, Bit32u src, u_int bytes)
{
    if (dstmem) {
	if (set_memory(opa.seg, dst, src, bytes) != 0)
	    return -1;
    } else {
	set_reg(dst, src, bytes);
    } 
    return 0;
}


/* Gets a value (*dst) from memory or register (src).
   Returns 0 if okay, -1 on exception.
*/
int emu_get(int srcmem, Bit32u src, Bit32u *dst, u_int bytes)
{
    if (srcmem) {
	if (get_memory(opa.seg, src, dst, bytes) != 0)
	    return -1;
    } else {
	*dst = get_reg(src, bytes);
    } 
    return 0;
}



int check_pop_available(u_int esp, u_int bytes)
{
    u_int base;

    base = get_base(REG(ss));
    if (esp >= base && esp <= base+get_limit(REG(ss)-bytes))
	return 0;
    set_guest_exc(EXC_SS, 0);
    return -1;
}

int check_push_available(u_int esp, u_int bytes)
{
    u_int base;

    base = get_base(REG(ss));
    if (! (esp >= base+bytes && esp <= base+get_limit(REG(ss)))) {
	set_guest_exc(EXC_SS, 0);
	return -1;
    }
    if ((vmstate.cr[0] & PG_MASK) && (guest_pte(esp) & 1) == 0) {
	/* We can't push onto guest's stack; it's not mapped in.
	   Let's pretend the guest page faulted here.  Hopefully it
	   will map a page and rerun the instruction, and the next
	   time we get here it will work.

	   Hopefully no state change has been committed to the REGS
	   yet.  Let's just set an exception directly (set_guest_exc
	   should be smart enough to not touch this stack again, so we
	   won't recurse). */

	vmstate.cr[2] = esp;
	set_guest_exc(EXC_PF, ((opa.cpl==3 ? 4 : 0) +
			       2)); 	/* write to nonpresent page*/
	return -1;
    }
    return 0;
}

int check_eip(u_int tmp_cs, u_int tmp_eip)
{
    /* FIXME: check that eip is in CS */
    return 0;
}




/* gets the value from the register, by number.  The top (4-bytes)
   bytes are zeroed out.
*/

u_int get_reg(int r, unsigned int bytes)
{
    u_int val=0;

    ASSERT(r>=REGNO_EAX && r<REGNO_GS);
    ASSERT(((bytes==1 || bytes==2 || bytes==4) && r<REGNO_ES) ||
	   bytes==2);

    /* ModR/M FUNKINESS WARNING:  When dealing with a r8, AH/CH/DH/BH are
       overlaid on what are normally SP/BP/SI/DI.  You can't refer to just a
       byte of those registers.  I handle that at the lowest level (get/set_reg)
       to keep things simple.
    */
    if (bytes==1 && r>=REGNO_ESP && r<=REGNO_EDI) {
	r -= 4;
	bytes = 0;
    }

    switch (r) {
    case REGNO_EAX: val = REG(eax); break;
    case REGNO_ECX: val = REG(ecx); break;
    case REGNO_EDX: val = REG(edx); break;
    case REGNO_EBX: val = REG(ebx); break;
    case REGNO_ESP: val = REG(esp); break;
    case REGNO_EBP: val = REG(ebp); break;
    case REGNO_ESI: val = REG(esi); break;
    case REGNO_EDI: val = REG(edi); break;
    case REGNO_ES : val = REG(es);  break;
    case REGNO_CS : val = REG(cs);  break;
    case REGNO_SS : val = REG(ss);  break;
    case REGNO_DS : val = REG(ds);  break;
    case REGNO_FS : val = REG(fs);  break;
    case REGNO_GS : val = REG(gs);  break;
    default: ASSERT(0);
    }

    val &= masks[bytes];

    /* ModR/M FUNKINESS WARNING */
    if (bytes==0) {
	val = val>>8;
    }

    return val;
}

void set_reg(int r, Bit32u v, int bytes)
{
    Bit32u mask;

    ASSERT(r>=REGNO_EAX && r<REGNO_GS);
    ASSERT(((bytes==1 || bytes==2 || bytes==4) && r<REGNO_ES) ||
	   bytes==2);

    /* ModR/M FUNKINESS WARNING:  When dealing with a r8, AH/CH/DH/BH are
       overlaid on what are normally SP/BP/SI/DI.  You can't refer to just a
       byte of those registers.  I handle that at the lowest level (get/set_reg)
       to keep things simple.
    */
    if (bytes==1 && r>=REGNO_ESP && r<=REGNO_EDI) {
	r -= 4;
	bytes = 0;
	v = v<<8;
    }

    mask = masks[bytes];
    v = v & mask;

    switch (r) {
    case REGNO_EAX: REG(eax) = (REG(eax) & ~mask) | v; break;
    case REGNO_ECX: REG(ecx) = (REG(ecx) & ~mask) | v; break;
    case REGNO_EDX: REG(edx) = (REG(edx) & ~mask) | v; break;
    case REGNO_EBX: REG(ebx) = (REG(ebx) & ~mask) | v; break;
    case REGNO_ESP: REG(esp) = (REG(esp) & ~mask) | v; break;
    case REGNO_EBP: REG(ebp) = (REG(ebp) & ~mask) | v; break;
    case REGNO_ESI: REG(esi) = (REG(esi) & ~mask) | v; break;
    case REGNO_EDI: REG(edi) = (REG(edi) & ~mask) | v; break;
    case REGNO_ES : REG(es) = v; break;
    case REGNO_CS : REG(cs) = v; break;
    case REGNO_SS : REG(ss) = v; break;
    case REGNO_DS : REG(ds) = v; break;
    case REGNO_FS : REG(fs) = v; break;
    case REGNO_GS : REG(gs) = v; break;
    default: ASSERT(0);
    }
}



/* 
   The exception CS:EIP will point to the first prefix that effects
   the faulting instruction, hence, 0x65 0x66 is same as 0x66 0x65.
   Sets the prefix flags.
*/

void get_prefixes(const unsigned char *csp)
{
    int done = 0;
    int prefixes = 0;
    int pe = IS_PE;
    int prefix66 = 0;
    int prefix67 = 0;

    ASSERT(csp);

    opa.seg = REG(ds);
    opa.repe = opa.repne = 0;

    do {
	switch (*(csp+prefixes)) {
	case 0x66:		/* operand prefix */
	    prefix66 = 1;
	    break;
	case 0x67:		/* address prefix */
	    prefix67 = 1;
	    break;
	case 0x2e:		/* CS */
	    opa.seg = REG(cs);
	    break;
	case 0x3e:		/* DS */
	    opa.seg = REG(ds);
	    break;
	case 0x26:		/* ES */
	    opa.seg = REG(es);
	    break;
	case 0x36:		/* SS */
	    opa.seg = REG(ss);
	    break;
	case 0x65:		/* GS */
	    opa.seg = REG(gs);
	    break;
	case 0x64:		/* FS */
	    opa.seg = REG(fs);
	    break;
	case 0xf2:		/* repne */
	    opa.repne = 1;
	    break;
	case 0xf3:		/* repe / rep */
	    opa.repe = 1;
	    break;
	default:
	    done = 1;
	    break;
	}
	prefixes++;
    } while (!done);
    prefixes--;

    opa.opb = ((pe && !prefix66) || (!pe && prefix66)) ? 4 : 2;
    opa.adb = ((pe && !prefix67) || (!pe && prefix67)) ? 4 : 2;
    opa.prefixes = prefixes;
}


int load_far_pointer(unsigned char **lina, int seg)
{
    u_int addr, is_addr, reg;
    u_int segval, regval;
    
    trace("in load_far_pointer(%x, %x)\n", (u_int)lina, (u_int)seg);

    ASSERT(lina);
    ASSERT(seg==REGNO_DS ||
	   seg==REGNO_SS ||
	   seg==REGNO_ES ||
	   seg==REGNO_FS ||
	   seg==REGNO_GS);

    (*lina) += 2;                       /* move up to RM byte */
    decode_rm(&addr, &is_addr, &reg, lina);
    ASSERT(is_addr);
    /* addr now points to the m16:16 or m16:32; lina is at next instr */
    if (get_memory(opa.seg, addr, &regval, opa.opb) != 0 ||
	get_memory(opa.seg, addr+opa.opb, &segval, 2) != 0) {
	error("get_memory failed for l?s\n");
	return -1;
    }
    /* debug("l?s(%x) loaded %x:%x from addr %x, lina %x\n", seg, segval, regval, addr, (u_int)*lina); */
    ASSERT(!IS_PE || (segval & 3) == 0);  /* else why did it trap?  RPL = CPL */
    set_reg(seg, segval | 1, 2);
    set_reg(reg, regval, opa.opb);

    return 0;
}

void get_guest_mode(void)
{
    int _pe, _cpl, _iopl;

    _pe = IS_PE ? 1 : 0;
    _cpl = GUEST_CPL;
    _iopl = GUEST_IOPL;

    if (! _pe)
	_cpl=3;
    else if (_cpl==1)
	_cpl=0;            /* FIXME: any way we can tell 1-er-0
			      vs. real 1?  could look in guest GDT at
			      the DPL of that seg.  Would give a bound. */

    opa.pe = _pe;
    opa.cpl = _cpl;
    opa.iopl = _iopl;
}


/*
  Handles any (async) call that goes through idt: faults, traps, exceptions...
  Does not handle explicit "int n" instructions; those go to do_pe_int().
  Does not handle explicit far jmp or call instructions; those go to gate().

  eip, cs, eflags are gotten from REG().  If the guest generates an
  exception, be sure old state is still in REG().

  Also, if the exception to be handled is a page fault, be
  lenient--don't want to fault again if we can't push onto the app
  stack.  Because of this, I don't call check_push_available so much.  */

void _set_guest_exc(Bit8u vect, Bit32u erc)
{
    extern int no_exceptions;
    u_int width, stacksize;
    u_int esp, dest_cpl;
    int err;
    int no_push = 0;
    struct gate_descr *gidt;

    ASSERT(vect < 0x20);	/* this fn is only for on-chip exceptions, not hardware ints */

    if (no_exceptions)		/* the debugger (ie, user) is allowed to misbehave */
	return;

    if (vmstate.exc != 0) {	/* catch guest double faults and general monitor screwups */
	ASSERT(vmstate.exc);
	printf(" first exception: #%02x,%x\n", vmstate.exc_vect, vmstate.exc_erc);
	printf("second exception: #%02x,%x\n", vect, erc);
	debugger();
	leaveemu(ERR_ASSERT);
    }

    vmstate.exc = 1;
    vmstate.exc_vect = vect;
    vmstate.exc_erc = erc;

    ASSERT(vmstate.g_idt_loaded);
    if (opa.pe) {
        struct seg_descr code_seg;
	gidt = (struct gate_descr *)get_idt_desc(vect);
	ASSERT (DESC_IS_INTGATE(gidt) || DESC_IS_TRAPGATE(gidt));
	err = get_descriptor(gidt->segsel, (struct descr *)&code_seg);
	ASSERT(err == 0);
	dest_cpl = code_seg.dpl;
	ASSERT (gidt->dpl >= dest_cpl);  /* else a bug in the guest or us */
    } else {
	gidt = (struct gate_descr *)vmstate.g_idt_pbase;
	dest_cpl = 3;
    }

    esp = make_lina(REG(ss), REG(esp));

    width = opa.pe ? 4 : 2;
    if (!opa.pe || opa.cpl==dest_cpl)
	stacksize = 3;
    else
	stacksize = 5;
    if (interrupt_has_error_code_p(vect))
	stacksize ++;
    stacksize *= width;

    /* don't use check_push_available; that could recursively fault. */
    if (verify_lina_range(esp-stacksize, PG_W, stacksize)) {
	if (vect == EXC_PF) {
	    no_push = 1;
	} else {
	    dprintf("needed %d bytes of stack for guest exception #%d,%d\n", stacksize, vect, erc);
	    leaveemu(ERR_MEM);
	}
    }

    if (opa.pe) {
	if (no_push) {
	    /* If we can't push since stack isn't paged in, we have
	       to change privilege levels... */
	    ASSERT(opa.cpl != dest_cpl);
	    /* ...else we can't handle it and'd be fucked. */
	}

	if (! no_push) {
	    if (opa.cpl != dest_cpl) {
		esp -= width; set_memory_lina(esp, REG(ss), width);
		esp -= width; set_memory_lina(esp, REG(esp), width);
	    }
	    esp -= width; set_memory_lina(esp, REG(eflags), width);
	    esp -= width; set_memory_lina(esp, REG(cs), width);
	    esp -= width; set_memory_lina(esp, REG(eip), width);
	}

	if (opa.cpl != dest_cpl) {
	    u_int new_ss=0, new_esp=0;
	    struct Ts *tss;

	    ASSERT(vmstate.tr_loaded);
	    tss = (struct Ts *)vmstate.tr_base;
	    if (dest_cpl==0) {
		new_ss = tss->ts_ss0;
		new_esp = tss->ts_esp0;
	    } else if (dest_cpl==1) {
		new_ss = tss->ts_ss1;
		new_esp = tss->ts_esp1;
	    } else if (dest_cpl==2) {
		new_ss = tss->ts_ss2;
		new_esp = tss->ts_esp2;
	    } else {
		ASSERT(0);
	    }
	    ASSERT(new_ss < GD_KT);
	    esp = make_lina(new_ss, new_esp);
	    /* FIXME:  check if stack is here.  ASSERT, I think. */
	    esp -= width; set_memory_lina(esp, REG(eflags), width);
	    esp -= width; set_memory_lina(esp, REG(cs), width);
	    esp -= width; set_memory_lina(esp, REG(eip), width);
	    REG(ss) = new_ss;	    
	}
	if (!no_push && interrupt_has_error_code_p(vect)) {
	    esp -= width; set_memory_lina(esp, erc, width);
	}
	REG(esp) = (u_int)esp - get_base(REG(ss));
	REG(cs) = gidt->segsel;
	REG(eip) = GATE_OFFSET(gidt);
	REG(eflags) &= ~(TF_MASK | VM_MASK | RF_MASK | NT_MASK);
	/* FIXME:  we may need to munge CS or SS on stack so that
	   the iret traps, so we can reset TF, NT, IOPL, etc. since
	   that can't be done at CPL 1.  */
    } else {
	/* real mode */
	ASSERT((u_int)gidt + vect*4 < 1024*1024);

	esp -= width; set_memory_lina(esp, LWORD(eflags), width);
	esp -= width; set_memory_lina(esp, LWORD(cs), width);
	esp -= width; set_memory_lina(esp, LWORD(eip), width);
	REG(eflags) &= ~VIF_MASK;
	LWORD(cs) = *(Bit16u*)(gidt + vect*4 + 2);
	LWORD(eip) = *(Bit16u*)(gidt + vect*4);
    }
}

int simple_iret(u_int tmp_eflags, u_int tmp_cs, u_int tmp_eip)
{
    if (check_eip(tmp_cs, tmp_eip))
	return -1;
    REG(eip) = tmp_eip;
    REG(cs) = tmp_cs;
    REG(eflags) = ((REG(eflags) & ~(CF | PF | AF | ZF | SF | TF | DF | OF | NT)) |
		   tmp_eflags   &  (CF | PF | AF | ZF | SF | TF | DF | OF | NT));
    if (opa.opb == 4)
	REG(eflags) = ((REG(eflags) & ~(RF | AC | ID)) |
		       tmp_eflags   &  (RF | AC | ID));
    if (opa.cpl <= opa.iopl)
	REG(eflags) = ((REG(eflags) & ~(IF)) |
		       tmp_eflags   &  (IF));
    if (opa.cpl == 0) {
	REG(eflags) = ((REG(eflags) & ~(IOPL_MASK)) |
		       tmp_eflags   &  (IOPL_MASK));
	if (opa.opb == 4)
	    REG(eflags) = ((REG(eflags) & ~(VM | VIF | VIP)) |
			   tmp_eflags   &  (VM | VIF | VIP));
    }
    return 0;
}
