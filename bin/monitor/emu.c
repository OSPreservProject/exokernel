#include "types.h"
#include <xok/pctr.h>

#include "config.h"
#include "cpu.h"
#include "debug.h"
#include "pic.h"
#include "decode.h"
#include "descriptors.h"
#include "emu.h"
#include "handler_utils.h"
#include "opa.h"
#include "int.h"
#include "memutil.h"
#include "port.h"
#include "repeat.h"
#include "tasks.h"
#include "tss.h"


/*
  General form of an emulation function:

int emu_foo(unsigned char *lina)
{
    unsigned char *orig_lina = lina;

    // emulate; move lina to next instruction

 okay:
    REG(eip) += lina-orig_lina;
 control_transfer:
    return 0;

 exception:
    return -1;
}

*/


void unknown(unsigned char *lina)
{
    dprintf("unhandled instruction at %x:  %02x %02x %02x ...\n", (u_int)lina, lina[0], lina[1], lina[2]);
    debugger();
    leaveemu(ERR);
}



/* opcodes 0f ... */

int emu_0f(unsigned char *lina)
{
    unsigned char *orig_lina = lina;

    switch (lina[1]) {
    case 0x00:		/* lldt, ltr */
    {
	switch (REG_OPCODE(lina[2])) {
	case 2:		/* 0F 00 /2 LLDT r/m16 Load segment selector r/m16 into LDTR */
	{
	    u_int addr, is_addr, reg, selector;

	    trace("lldt\n");
	    if (!opa.pe) {
		set_guest_exc(EXC_UD, 0);
		goto exc;
	    }
	    if (opa.cpl != 0) {
		set_guest_exc(EXC_GP, 0);
		goto exc;
	    }
	    lina += 2;	/* move up to RM byte */
	    decode_rm16(&addr, &is_addr, &reg, &lina);
	    if (emu_get(is_addr, addr, &selector, 2))
		goto exc;
	    if (load_ldtr(selector))
		goto exc;

	    break;
	}
	case 3: 	 /* 0F 00 /3 LTR r/m16 Load r/m16 into TR */
	{
	    u_int addr, is_addr, reg;
	    Bit32u dtr;
	    
	    trace("ltr\n");
	    if (!opa.pe) {
		set_guest_exc(EXC_UD, 0);
		goto exc;
	    }
	    if (opa.cpl != 0) {
		set_guest_exc(EXC_GP, 0);
		goto exc;
	    }
	    lina += 2;  /* move up to RM byte */
	    decode_rm16(&addr, &is_addr, &reg, &lina);
	    if (emu_get(is_addr, addr, &dtr, 2))
		goto exc;
	    if (load_tr(dtr))
		goto exc;

	    break;
	}
	default:
	    unknown(lina);
	}
	break;
    }
    
    case 0x01:		/* invlpg, lgdt, lidt, lmsw */
    {
	int reg_op = REG_OPCODE(lina[2]);
	switch (reg_op) {
	case 2:				/* lgdt */
	case 3:				/* lidt */
	{
	    u_int addr, is_addr, reg, limit, base;
	    
	    lina += 2;                       /* move up to RM byte */
	    decode_rm(&addr, &is_addr, &reg, &lina);
	    ASSERT(is_addr);
	    /* addr now points to the m16&32; lina is at next instr */
	    if (get_memory(opa.seg, addr, &limit, 2) != 0 ||
		get_memory(opa.seg, addr+2, &base, opa.opb==4 ? 4 : 3) != 0)
		goto exc;
	    
	    /* by definition of lgdt/lidt, base is a linear address already. */
	    if (reg_op == 2) {
		set_gdt(base, limit);
	    } else {
		set_idt(base, limit);
	    }
	    debug_mem("loaded %cdt from 0x%08x\n", reg_op==2 ? 'g' : 'i', base);
	    break;
	}
	case 6:		/* 0F 01 /6  LMSW r/m16  Loads r/m16 in msw of CR0 */
	{
	    u_int addr, is_addr, reg, val;
	    
	    trace("lmsw\n");
	    
	    if (opa.pe && opa.cpl!=0) {
		set_guest_exc(13, 0);
		goto exc;
	    }
	    
	    lina += 2;         /* move up to RM byte */
	    decode_rm16(&addr, &is_addr, &reg, &lina);
	    if (emu_get(is_addr, addr, &val, 2))
		goto exc;
	    if (vmstate.cr[0] & 1 && !(val & 1))
		val |= 1;  /* can't clear PE with lmsw */
	    mov_to_cr(0, val, 0x0000000f); /* only PE, MP, EM, and TS can be affected */
	    
	    break;
	}
	case 7:		/* invlpg */
	{
	    Bit32u ptr;
	    
	    debug("invlpg\n");
	    
	    lina += 2;         /* move up to memory operand */
	    if (opa.opb==4) {
		ptr = *(Bit32u*)lina;
	    } else {
		ptr = *(Bit16u*)lina;
	    }
	    lina += opa.opb;
	    
	    if (vmstate.cr[0]&PG_MASK) {
		/* Modify a pte with itself.  This should have the desired side
		   effect of flushing that TLB entry. */
		sys_self_mod_pte_range(0, 0, /* add no flag bits */
				       0, /* remove no flag bits */
				       ptr, 1);
	    }

	    break;
	}
	default:
	    unknown(lina);
	}
	break;
    }

    case 0x06:		/* clts  0F 06 */
    {
	if (opa.cpl!=0) {
	    set_guest_exc(13, 0);
	    goto exc;
	} else {	
	    vmstate.cr[0] &= ~TS_MASK;
	    lina += 2;
	}
	break;
    }

    case 0x08:		/* invd  0F 08 */
    case 0x09:		/* wbinvd  0F 09 */
    {
	if (opa.cpl!=0) {
	    set_guest_exc(13, 0);
	    goto exc;
	} else {
	    /* will not implement */
	    lina += 2;
	}
	break;
    }

    case 0x0b:		/* UD2 */
    {
	set_guest_exc(6, 0);
	goto exc;
    }

    case 0x20:		/* MOV r <- CRx */
    {
	int cr = REG_OPCODE(lina[2]);
	int reg = RM(lina[2]);
	
	ASSERT(cr<5);
	set_reg(reg, vmstate.cr[cr], 4);
	lina += 3;
	break;
    }

    case 0x21:		/* MOV r <- DRx */
    {
	int dr = REG_OPCODE(lina[2]);
	int reg = RM(lina[2]);
	
	set_reg(reg, vmstate.dr[dr], 4);
	lina += 3;
	break;
    }

    case 0x22:		/* MOV CRx <- r */
    {
	int cr = REG_OPCODE(lina[2]);
	
	ASSERT(cr<5);
	if (opa.pe && opa.cpl!=0) {
	    set_guest_exc(13, 0);
	    goto exc;
	}
	
	mov_to_cr(cr, get_reg(RM(lina[2]), 4), 0xffffffff);
	lina += 3;
	break;
    }

    case 0x23:		/* MOV DRx <- r */
    {
	int dr = REG_OPCODE(lina[2]);

	debug("mov dr%d <- r%d\n", dr, RM(lina[2]));

	if (opa.pe && opa.cpl!=0) {
	    set_guest_exc(13, 0);
	    goto exc;
	}
	
	vmstate.dr[dr] = get_reg(RM(lina[2]), 4);
	lina += 3;
	break;
    }

    case 0x30:		/* wrmsr */
    {
	int ctr = 0;

	if (REG(ecx) == P6MSR_CTRSEL0)
	    ctr = 0;
	else if (REG(ecx) == P6MSR_CTRSEL1)
	    ctr = 1;
	else
	    unknown(lina);    /* only performance counters are implemented */

	sys_pctr(ctr==0 ? PCIOCS0 : PCIOCS1, 0, &REG(eax));
	lina += 2;
	break;
    }

    case 0x32:		/* rdmsr */
    {
	struct pctrst p;
	int ctr = 0;

	if (REG(ecx) == P6MSR_CTR0)
	    ctr = 0;
	else if (REG(ecx) == P6MSR_CTR1)
	    ctr = 1;
	else
	    unknown(lina);    /* only performance counters are implemented */

	sys_pctr(PCIOCRD, 0, &p);
	REG(eax) = p.pctr_hwc[ctr];
	REG(edx) = p.pctr_hwc[ctr] >> 32;
	lina += 2;
	break;
    }

#if 0
    case 0x33:		/* rdpmc */
    {
	struct pctrst p;

	/* or cpl!=0 and cr4 ... */
	if (REG(ecx)>1) {
	    set_guest_exc(EXC_GP, 0);
	    goto exc;
	}

	sys_pctr(PCIOCRD, 0, &p);
	REG(eax) = p.pctr_hwc[REG(ecx)];
	REG(edx) = p.pctr_hwc[REG(ecx)] >> 32;

	lina += 2;
	break;
    }
#endif

    case 0xa2:		/* cpuid */
    {
	/* cpuid may trap on a Cyrix.  I don't care. */
	leaveemu(ERR_UNIMPL);
	break;
    }

    case 0xb2:		/* lss */
    case 0xb4:		/* lfs */
    case 0xb5:		/* lgs */
    {
	int seg;

	if (lina[1]==0xb2) {
	    seg = REGNO_SS;
	} else if (lina[1]==0xb4) {
	    seg = REGNO_FS;
	} else
	    seg = REGNO_GS;
	if (load_far_pointer(&lina, seg))
	    goto exc;
	break;
    }

    case 0x31:		/* rdtsc ... should be enabled in xok */
    case 0xc8:		/* bswap  should not trap */
    default:
	unknown(lina);
    }

    REG(eip) += lina-orig_lina;
    return 0;

 exc:
    return -1;
}

/* opcode 17 */

int emu_pop(unsigned char *lina)
{
    unsigned char *orig_lina = lina;

    leaveemu(ERR_UNIMPL); /* FIXME */

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcode 62 */

int emu_bound(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    u_int addr, is_addr, array_index, lower_bound, upper_bound;

    lina ++;		/* move up to RM byte */
    decode_rm(&addr, &is_addr, &array_index, &lina);
    ASSERT(is_addr);
    /* addr now points to the m16&16 or m32&32; lina is at next instr */
    if (get_memory(opa.seg, addr, &lower_bound, opa.opb) ||
	get_memory(opa.seg, addr+opa.opb, &upper_bound, opa.opb))
	return -1;

    if (array_index < lower_bound || array_index > (upper_bound + opa.opb)) {
	set_guest_exc(EXC_BR, 0);
	return -1;
    }

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcode 63 */

int emu_arpl(unsigned char *lina)
{
    unsigned char *orig_lina = lina;

    if (!opa.pe) {
	set_guest_exc(EXC_UD, 0);
	return -1;
    }

    leaveemu(ERR_UNIMPL); /* FIXME */

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcodes 6c, 6d */

int emu_ins(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    if (opa.repe || opa.repne) {
	err = repeat(&lina, &emu_s_ins, &emu_e_ins);
    } else {
	err = once(&lina, &emu_s_ins, &emu_e_ins);
    }
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes 6e, 6f */

int emu_outs(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    if (opa.repe || opa.repne) {
	err = repeat(&lina, &emu_s_outs, &emu_e_outs);
    } else {
	err = once(&lina, &emu_s_outs, &emu_e_outs);
    }
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes 88, 89, 8a, 8b, 8c, 8e */

int emu_mov(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;
    u_int src, dst, bytes, srcmem, dstmem;

    if (decode_mov_EG_GE_ES_SE(&lina, &src, &dst, &bytes, &srcmem, &dstmem, *lina))
	return -1;
    err = emu_set(dstmem, dst, src, bytes);

    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcode 9a */

int emu_call_far_p(unsigned char *lina)
{
    leaveemu(ERR_UNIMPL); /* FIXME */

    return 0;
}

/* opcode 9b */

int emu_wait(unsigned char *lina)
{
    if (vmstate.cr[0] & (MP_MASK | TS_MASK)) {
	set_guest_exc(EXC_NM, 0);
	return -1;
    }
	
    leaveemu(ERR_UNIMPL); /* FIXME */

    REG(eip) ++;
    return 0;
}

/* opcode 9c */

int emu_pushf(unsigned char *lina)
{
    Bit32u val;
    
    trace("pushf\n");
    ASSERT(!opa.pe);
    
    if (opa.opb==4) {
	val = REG(eflags);
    } else {
	val = LWORD(eflags);
    }

    LWORD(esp) -= opa.opb;
    val &= ~(VM_MASK | RF_MASK);
    if (set_memory(LWORD(ss), LWORD(esp), val, opa.opb))
	return -1;

    REG(eip) ++;
    return 0;
}

/* opcode 9d */

int emu_popf(unsigned char *lina)
{
    Bit32u tmp, mask;
    
    trace("popf\n");
    ASSERT(!opa.pe);
    
    if (opa.opb==4) {
	mask = VM_MASK | RF_MASK | IOPL_MASK | VIP_MASK | VIF_MASK;
    } else {
	mask = IOPL_MASK | 0xffff0000;
    }
    
    if (get_memory(LWORD(ss), LWORD(esp), &tmp, opa.opb))
	return -1;
    LWORD(esp) += opa.opb;
    REG(eflags) = (REG(eflags) & mask) | (tmp & ~mask);
    
    REG(eip) ++;
    return 0;
}

/* opcode a0 */

int emu_mov_ALOb(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;
    u_int src, dst, bytes, srcmem, dstmem;

    decode_mov_ALOb_AXOv(&lina, &src, &dst, &bytes, &srcmem, &dstmem, 0xa2);
    err = emu_set(dstmem, dst, src, bytes);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcode a1 */

int emu_mov_AXOv(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;
    u_int src, dst, bytes, srcmem, dstmem;

    decode_mov_ALOb_AXOv(&lina, &src, &dst, &bytes, &srcmem, &dstmem, 0xa3);
    err = emu_set(dstmem, dst, src, bytes);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcode a2 */

int emu_mov_ObAL(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;
    u_int src, dst, bytes, srcmem, dstmem;

    decode_mov_ObAL_OvAX(&lina, &src, &dst, &bytes, &srcmem, &dstmem, 0xa2);
    err = emu_set(dstmem, dst, src, bytes);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcode a3 */

int emu_mov_OvAX(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;
    u_int src, dst, bytes, srcmem, dstmem;

    decode_mov_ObAL_OvAX(&lina, &src, &dst, &bytes, &srcmem, &dstmem, 0xa3);
    err = emu_set(dstmem, dst, src, bytes);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes a4, a5 */

int emu_movs(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    if (opa.repe || opa.repne)
	err = repeat(&lina, &emu_s_movs, &emu_e_movs);
    else
	err = once(&lina, &emu_s_movs, &emu_e_movs);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes a6, a7 */

int emu_cmps(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    if (opa.repe || opa.repne)
	err = repeat(&lina, &emu_s_cmps, &emu_e_cmps);
    else
	err = once(&lina,  &emu_s_cmps, &emu_e_cmps);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes aa, ab */

int emu_stos(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    if (opa.repe || opa.repne)
	err = repeat(&lina, &emu_s_stos, &emu_e_stos);
    else
	err = once(&lina, &emu_s_stos, &emu_e_stos);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes ac, ad */

int emu_lods(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    if (opa.repe || opa.repne)
	err = repeat(&lina, &emu_s_lods, &emu_e_lods);
    else
	err = once(&lina, &emu_s_lods, &emu_e_lods);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes b0 ... b7 */

int emu_mov_i8_r8(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    u_int src, dst, bytes, srcmem, dstmem;

    if (decode_mov_imm8_reg8(&lina, &src, &dst, &bytes, &srcmem, &dstmem, *lina))
	return -1;
    ASSERT(!dstmem);
    ASSERT(bytes==1);
    set_reg(dst, src, bytes);

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcodes b8 ... bf */

int emu_mov_i_r(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    u_int src, dst, bytes, srcmem, dstmem;

    if (decode_mov_imm_reg(&lina, &src, &dst, &bytes, &srcmem, &dstmem, *lina))
	return -1;
    ASSERT(!dstmem);
    ASSERT(bytes==2 || bytes==4);
    set_reg(dst, src, bytes);

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcode c4 */

int emu_les(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    err = load_far_pointer(&lina, REGNO_ES);

    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcode c5 */

int emu_lds(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;

    err = load_far_pointer(&lina, REGNO_DS);

    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcodes c6, c7 */

int emu_mov_EI(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int err;
    u_int src, dst, bytes, srcmem, dstmem;

    decode_mov_EbIb_EvIv(&lina, &src, &dst, &bytes, &srcmem, &dstmem, *lina);
    err = emu_set(dstmem, dst, src, bytes);
    if (!err)
	REG(eip) += lina-orig_lina;
    return err;
}

/* opcode c9 */

int emu_leave(unsigned char *lina)
{
    if (opa.pe) {
	u_int base = get_base(REG(ss));
	if (REG(ebp) < base || REG(ebp) > base + get_limit(REG(ss))) {
	    set_guest_exc(EXC_SS, 0);
	    return -1;
	}
    } else {
	if (REG(ebp)>0xffff) {
	    set_guest_exc(EXC_GP, 0);
	    return -1;
	}
    }

    REG(esp) = REG(ebp);
    if (get_memory(REG(ss), REG(esp), &REG(ebp), opa.opb))
	return -1;

    REG(eip) ++;
    return 0;
}

/* opcode cc */

int emu_int3(unsigned char *lina)
{
    trace("int 3\n");
    
    set_guest_exc(3, 0);
    return -1;
}

/* opcode cd */

int emu_int(unsigned char *lina)
{
    Bit8u i = lina[1];
    
    trace("int 0x%02x\n", i);
    if (opa.pe) {
	if (config.idle_int && config.idle_int==i) {
	    yield_now(-1);
	} else {
	    if (do_pe_int(i))
		return -1;
	}
    } else {
	do_int(i);
    }

    REG(eip) += 2;
    return 0;
}

/* opcode ce */

int emu_into(unsigned char *lina)
{
    trace("into\n");
    
    if (REG(eflags) & OF) {
	set_guest_exc(EXC_OF, 0);
	return -1;
    }
    
    REG(eip)++;
    return 0;
}

/* opcode cf */

int emu_iret(unsigned char *lina)
{
    trace("iret\n");

    if (! opa.pe) {
	Bit32u tmp;
	int width = opa.opb;
	
	if (REG(esp) > 0x10000 - 3*width) {
	    set_guest_exc(12, 0);
	    goto exc;
	}
	if (get_memory(LWORD(ss), LWORD(esp), &REG(eip), width))
	    goto exc;
	if (width == 2)
	    REG(eip) &= 0xffff;
	LWORD(esp) += width;
	if (get_memory(LWORD(ss), LWORD(esp), &tmp, 2))  /* always only use 16 bits */
	    goto exc;
	REG(cs) = tmp;
	LWORD(esp) += width;
	if (get_memory(LWORD(ss), LWORD(esp), &tmp, width))
	    goto exc;
	LWORD(eflags) = tmp;
	LWORD(esp) += width;
	
	if (width == 4) {
	    /* 32 bit -- VM, IOPL, VIP, and VIF are unmodified */
	    REG(eflags) = ((tmp        & ~(VM_MASK | IOPL_MASK | VIP_MASK | VIF_MASK)) |
			   (REG(eflags) & (VM_MASK | IOPL_MASK | VIP_MASK | VIF_MASK)));
	} else {
	    /* 16 bit -- IOPL is unmodified */
	    REG(eflags) = ((tmp        & ~(0xffff000 | IOPL_MASK)) |
			   (REG(eflags) & (0xffff000 | IOPL_MASK)));
	}
	
	trace("%d byte iret; eip=%x cs=%x, eflags=%x; new lina=%x\n", opa.opb,
	      REG(eip), REG(cs), REG(eflags), (u_int)lina);
    } else {
	/* PROTECTED-MODE */

	if (REG(eflags) & NT_MASK) {
	    /* TASK-RETURN */
	    Bit16u old_sel = vmstate.tr_sel;
	    struct seg_descr *old_tss;
	    struct Ts *ts = (struct Ts*)vmstate.tr_base;
	    struct seg_descr tss;
	    Bit16u selector = ts->ts_link;
	    int err;

	    if (get_tss_descriptor(selector, &tss, 1) ||
		switch_tasks(selector, &tss))
		goto exc;
	    
	    ts->ts_t = 0; /* We probably got here because this was set, to let us emulate it. */

	    /* mark the descriptor of the abandoned task as "not busy" */
	    ASSERT((old_sel & ~7) == 0);
	    ASSERT((old_sel & ~7) >= vmstate.g_gdt_limit);
	    err = set_get_any(&vmstate.g_gdt_base, (u_int*)&old_tss);
	    ASSERT(!err);
	    old_tss += (old_sel >> 3);
	    old_tss->type &= ~2;
	} else {
	    u_int esp, tmp_eflags, tmp_cs, tmp_eip;
	    struct descr cs;

	    esp = make_lina(REG(ss), REG(esp));
	    if (check_pop_available(esp, 3*opa.opb))
		goto exc;
	    if (get_memory_lina(esp, &tmp_eip, opa.opb)) goto exc;
	    esp += opa.opb;
	    tmp_eip &= masks[opa.opb];
	    if (get_memory_lina(esp, &tmp_cs, opa.opb)) goto exc;
	    esp += opa.opb;
	    if (get_memory_lina(esp, &tmp_eflags, opa.opb)) goto exc;
	    esp += opa.opb;
	    tmp_eflags &= masks[opa.opb];
	   
	    ASSERT(! (opa.cpl==0 && (tmp_eflags & VM_MASK)));	/* we don't support nested virtualization */

	    /* PROTECTED-MODE-RETURN */
	    if (get_descriptor(tmp_cs, (struct descr *)&cs))
		goto exc;

	    if (! DESC_IS_CODE(&cs) ||
		(tmp_cs & 3) < opa.cpl ||
		((cs.type & 4) && cs.dpl > (tmp_cs & 3))) {
		set_guest_exc(EXC_GP, tmp_cs);
		goto exc;
	    }
	    if (! cs.p) {
		set_guest_exc(EXC_NP, tmp_cs);
		goto exc;
	    }

	    if ((tmp_cs & 2) > opa.cpl) {
		/* RETURN-OUTER-PRIVILEGE-LEVEL */
		u_int tmp_esp, tmp_ss;
		struct seg_descr ss;

		if (check_pop_available(esp, 2*opa.opb))
		    goto exc;
		if (get_memory_lina(esp, &tmp_esp, opa.opb)) goto exc;
		esp += opa.opb;
		if (get_memory_lina(esp, &tmp_ss, opa.opb)) goto exc;
		esp += opa.opb;
		
		if (get_descriptor(tmp_ss, (struct descr *)&ss))
		    return -1;
		
		if (! DESC_IS_WRITABLE_DATA(&ss) ||
		    (tmp_ss & 3) != (tmp_cs & 3) ||
		    ss.dpl != (tmp_cs & 3)) {
		    set_guest_exc(EXC_GP, tmp_ss);
		    goto exc;
		}
		if (! ss.p) {
		    set_guest_exc(EXC_NP, tmp_ss);
		    goto exc;
		}
		REG(esp) = tmp_esp;
		REG(ss) = tmp_ss;

		if (simple_iret(tmp_eflags, tmp_cs, tmp_eip))
		    goto exc;
		    
		{
		    /* Error checking on segment registers */
		    struct seg_descr *tmp_desc;
		    int regs_to_check[4] = {REGNO_ES, REGNO_FS, REGNO_GS, REGNO_DS};
		    int err, i;

		    for (i=0; i<4; i++) {
			Bit16u reg = regs_to_check[i];
			if (SEL_IS_LOCAL(get_reg(reg, 2))) {
			    err = set_get_any(&vmstate.g_ldt_base, (u_int*)&tmp_desc);
			} else {
			    err = set_get_any(&vmstate.g_gdt_base, (u_int*)&tmp_desc);
			}
			ASSERT(!err);
			tmp_desc += (get_reg(reg, 2) >> 3);
			if ((DESC_IS_DATA(tmp_desc) || DESC_IS_NCONF_CODE(tmp_desc)) &&
			    opa.cpl > tmp_desc->dpl) {
			    set_reg(reg, 0, 2);
			}			
		    }
		}
	    } else {
		/* RETURN-SAME-PRIVILEGE-LEVEL */

		if (simple_iret(tmp_eflags, tmp_cs, tmp_eip))
		    goto exc;
	    }
	}	
    }

    return 0;

 exc:
    return -1;
}

#if 0
/* must run at CPL 0 */
void fninit(void)
{
    asm("movl %cr0, %eax\n"
	"andl $0x6, %eax\n"
	"movl %eax, %cr0\n"
	"fninit");
}
#endif

/* opcode db */

int emu_fpu(unsigned char *lina)
{
    unsigned char *orig_lina = lina;

    switch(lina[1]) {
    case 0xe3:  /* finit */
	if (vmstate.cr[0] & TS_MASK) {
	    set_guest_exc(7, 0);
	    return -1;
	} else {
	    /* sys_ring0(0, &fninit); */
	    leaveemu(ERR_UNIMPL);
	}
	break;
    default:
	unknown(lina);
    }

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcodes e4, e5, ec, ed */

int emu_in(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int bytes=1, port=0;
    
    trace_io("inb/inw/ind\n");
    
    switch (lina[0]) {
    case 0xe4:
	port = lina[1];
	lina ++;
	break;
    case 0xe5:
	port = lina[1];
	bytes = opa.opb;
	lina ++;
	break;
    case 0xec:
	port = LWORD(edx);
	break;
    case 0xed:
	port = LWORD(edx);
	bytes = opa.opb;
	break;
    default:
	ASSERT(0);
    }
    
    if (opa.pe && !guest_ports_accessible(port, bytes)) {
	set_guest_exc(EXC_GP, 0);
	return -1;
    }
    
    switch (bytes) {
    case 1:
	LO(ax) = inb(port);break;
    case 2:
	LWORD(eax) = inw(port);break;
    case 4:
	REG(eax) = ind(port);break;
    default:
	ASSERT(0);
    }
    
    lina ++;

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcodes e6, e7, ee, ef */

int emu_out(unsigned char *lina)
{
    unsigned char *orig_lina = lina;
    int bytes=1, port=0;
    
    trace_io("outb/outw/outd\n");
    
    switch (lina[0]) {
    case 0xe6:
	port = lina[1];
	lina ++;
	break;
    case 0xe7:
	port = lina[1];
	bytes = opa.opb;
	lina ++;
	break;
    case 0xee:
	port = LWORD(edx);
	break;
    case 0xef:
	port = LWORD(edx);
	bytes = opa.opb;
	break;
    default:
	ASSERT(0);
    }
    
    if (opa.pe && !guest_ports_accessible(port, bytes)) {
	set_guest_exc(EXC_GP, 0);
	return -1;
    }
    
    debug_io("%d bytes of %02x out port %x\n", bytes, REG(eax), port); 
    (out_funcs[bytes])(port, REG(eax));
    
    lina ++;

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcode e8 */

int emu_call_rel(unsigned char *lina)
{
    unsigned char *orig_lina = lina;

    leaveemu(ERR_UNIMPL); /* FIXME */

    REG(eip) += lina-orig_lina;
    return 0;
}

/* opcode ea */

int emu_jmp_far_abs(unsigned char *lina)
{
    /* About the only reason this would #13 is if we try to jump beyond the
       limits of CS.  This can occur if a segment descriptor was changed,
       but the segment register was not reloaded (ie, the segment descriptor
       cache should be stale, but the monitor can't know that.) */

    /* This is to help when the booting process does an intersegment
       jump to set CS.  The monitor is already using 32 bit
       addressing, but Linux expects 16 bits still.  So it uses a 0x66
       prefix.  Kill that and retry.  */

    trace("0xea\n");

    /* FIXME rewrite this correctly.  This is just a hack.  Have a
       function to emulate such jumps, but change opa.opb first if
       needed to hack around the segment descriptor cache issues. */

    if (opa.pe && vmstate.prot_transition) {
	if (opa.opb!=4) {
	    int changed;
	    debug("going to NOP the 0x66 prefix at 0x%08x\n", (u_int)(*lina));
	    changed = unprotect_range((u_int)lina, 1);
	    *(lina-1) = 0x90;  /* NOP  FIXME: what if other prefixes? */
	    if (changed)
		protect_range((u_int)lina, 1);
	}
	vmstate.prot_transition = 0;
    } else {
	unknown(lina);
    }

    return 0;
}

/* opcode f4 */

int emu_hlt(unsigned char *lina)
{
    trace("hlt\n");
    
    if (config.exit_on_hlt) {
	dprintf("hlt statement encountered\n");
	for(;;);
    } else {
	dprintf("hlt statement encountered; starting debugger...\n");
	debugger();
    }

    REG(eip) ++;
    return 0;
}

/* opcode fa */

int emu_cli(unsigned char *lina)
{
    trace("cli\n");

    if (opa.pe) {
	REG(eflags) &= ~IF_MASK;
	pic_maski(0);
    } else {
	if (REG(eflags) & VIF_MASK) {
	    REG(eflags) |= VIP_MASK;
	    /* FIXME:  queue up this interrupt */
	    warn("cli not fully implemented for VM86\n");
	}
    }

    REG(eip) ++;
    return 0;
}

/* opcode fb */

int emu_sti(unsigned char *lina)
{
    trace("sti\n");

    if (opa.pe) {
	REG(eflags) |= IF_MASK;
	pic_unmaski(0);
    } else {
	if (REG(eflags) & VIP_MASK) {
	    REG(eflags) &= ~(VIP_MASK | VIF_MASK);
	    /* FIXME:  handle queued up interrupts */
	    warn("sti not fully implemented for VM86\n");
	}
    }

    REG(eip) ++;
    return 0;
}

/* opcode ff */

int emu_ff(unsigned char *lina)
{
    unsigned char *orig_lina = lina;

    switch (REG_OPCODE(lina[1])) {
    case 3:
    {
	u_int addr, is_addr, reg;
	u_int selector, offset;

	trace("0xff /3 far call");
	ASSERT(opa.pe);		/* else we've probably corrupted CS, assuming not buggy guest OS */

	lina ++;			/* move up to RM byte */
	decode_rm(&addr, &is_addr, &reg, &lina);
	ASSERT(is_addr);
	/* addr now points to the m16:16 or m16:32 lina is at next instr */
	if (get_memory(opa.seg, addr, &selector, 2) != 0 ||
	    get_memory(opa.seg, addr+2, &offset, opa.opb==4 ? 4 : 3) != 0)
	    return -1;
	
	if (selector < 8) {
	    set_guest_exc(EXC_GP, 0);
	    return -1;
	}
	ASSERT(vmstate.g_idt_loaded);
	if (selector > vmstate.g_idt_limit) {
	    set_guest_exc(EXC_GP, selector);
	    return -1;
	}	    
	if (gate(&lina, selector, offset))
	    return -1;
	break;
    }
    case 5:
	/* far jmp */
	leaveemu(ERR_UNIMPL);

	break;
    case 2: /* near call.  shouldn't trap in kernel except for bug. */
    default:
	leaveemu(ERR_UNIMPL);
	break;
    }

    REG(eip) += lina-orig_lina;
    return 0;
}


int emu_none(unsigned char *lina)
{
    ASSERT(0);	/* these should never #GPF and therefore never call here. */
    unknown(lina);
    return -1;
}


/* Don't have to implement an emulation function IF AND ONLY IF, from
   protected mode CPL 3, the instruction can NEVER throw a #gpf.  In
   such cases, the original exception (if any) can be seen (not masked
   by #gpf) and therefore is easy to redirect to the guest's handler,
   and therefore we don't have to emulate it to find the problem.
   Such instructions are marked as "emu_none".

   Instructions marked as "0" probably should be implemented.  The
   ones remaining probably only will trap on bugs, not due to really
   needing emulated.  If we're missing an emulation function, a guest
   application bug can halt the emulation. */

emu_func_t opcode_table[0x100] =
{
0,		0,		0,		0,		0,		0,		0,		0,	
0,		0,		0,		0,		0,		0,		0,		&emu_0f,	/*0f*/
0,		0,		0,		0,		0,		0,		0,		&emu_pop,
0,		0,		0,		0,		0,		0,		0,		0,		/*1f*/
0,		0,		0,		0,		0,		0,		0,		&emu_none,
0,		0,		0,		0,		0,		0,		0,		&emu_none,	/*2f*/
0,		0,		0,		0,		0,		0,		0,		&emu_none,
0,		0,		0,		0,		0,		0,		0,		&emu_none,	/*3f*/
0,		0,		0,		0,		0,		0,		0,		0,	
0,		0,		0,		0,		0,		0,		0,		0,		/*4f*/
0,		0,		0,		0,		0,		0,		0,		0,	
0,		0,		0,		0,		0,		0,		0,		0,		/*5f*/
0,		0,		&emu_bound,	&emu_arpl,	&emu_none,	&emu_none,	&emu_none,	&emu_none,	
0,		0,		0,		0,		&emu_ins,	&emu_ins,	&emu_outs,	&emu_outs,	/*6f*/
0,		0,		0,		0,		0,		0,		0,		0,
0,		0,		0,		0,		0,		0,		0,		0,		/*7f*/
0,		0,		0,		0,		0,		0,		0,		0,	
&emu_mov, 	&emu_mov,	&emu_mov,	&emu_mov,	&emu_mov,	0,		&emu_mov,	0,		/*8f*/
&emu_none,	0,		0,		0,		0,		0,		0,		0,
&emu_none,	&emu_none,	&emu_call_far_p,&emu_wait,	&emu_pushf,	&emu_popf,	&emu_none,	0,		/*9f*/
&emu_mov_ALOb,	&emu_mov_AXOv,	&emu_mov_ObAL,	&emu_mov_OvAX,	&emu_movs,	&emu_movs,	&emu_cmps,	&emu_cmps,	
0,		0,		&emu_stos,	&emu_stos,	&emu_lods,	&emu_lods,	0,		0,		/*af*/
&emu_mov_i8_r8,	&emu_mov_i8_r8,	&emu_mov_i8_r8,	&emu_mov_i8_r8,	&emu_mov_i8_r8,	&emu_mov_i8_r8,	&emu_mov_i8_r8,	&emu_mov_i8_r8,
&emu_mov_i_r,	&emu_mov_i_r,	&emu_mov_i_r,	&emu_mov_i_r,	&emu_mov_i_r,	&emu_mov_i_r,	&emu_mov_i_r,	&emu_mov_i_r,	/*bf*/
0,		0,		0,		0,		&emu_les,	&emu_lds,	&emu_mov_EI,	&emu_mov_EI,	
0,		&emu_leave,	0,		0,		&emu_int3,	&emu_int,	&emu_into,	&emu_iret,	/*cf*/
0,		0,		0,		0,		0,		&emu_none,	0,		0,	
0,		0,		0,		&emu_fpu,	0,		0,		0,		0,		/*df*/
0,		0,		0,		0,		&emu_in,	&emu_in,	&emu_out,	&emu_out,
&emu_call_rel,	0,		&emu_jmp_far_abs,0,		&emu_in,	&emu_in,	&emu_out,	&emu_out,	/*ef*/
0,		0,		&emu_none,	&emu_none,	&emu_hlt,	&emu_none,	0,		0,	
&emu_none,	&emu_none,	&emu_cli,	&emu_sti,	&emu_none,	&emu_none,	0,		&emu_ff		/*ff*/
};

