#include "opa.h"
#include "debug.h"
#include "types.h"
#include "decode.h"
#include "cpu.h"
#include "handler_utils.h"
#include "memutil.h"
#include "repeat.h"


static Bit32u calculate_sib(unsigned char **lina, Bit8u mod)
{
    Bit8u sib = **lina;
    int ss = SCALE(sib);
    int index = INDEX(sib);
    int base = BASE(sib);
    u_int addr = 0;

    (*lina) ++;	/* past SIB */

    if (index != 4) {
	addr = (1<<ss) * get_reg(index, 4);
    }

    if (base == 5) {
	addr += *(Bit32u*)(*lina);
	(*lina) += 4;	/* past disp32 */

	if (mod != 0) {
	    addr += get_reg(REGNO_EBP, 4);
	}
    } else {
	addr += get_reg(base, 4);
    }
    
    return addr; 
}


/*
  On entry, *csp points to the mod/rm byte.

  On exit, addr holds either an address or the register number;
  is_addr says which.  You'll then need to do a get/set_reg/memory as
  appropriate.  regop holds the other register number or opcode
  extension.  Updates csp to be after last byte of rm/sib/displacement
  stuff.

  The returned addr pointer does NOT have the segment base added to
  it.  The lower level routes to read/write memory should take the
  segment into account.

  Returns 0, or dies trying.

eg:
  rm  sib  disp-lo  disp-hi
  ^                          ^
  csp                        newcsp

*/

int decode_rm16(Bit32u *addr, u_int *is_addr, u_int *regop, unsigned char **csp)
{
    int rm, mod;
    Bit8u op;

    ASSERT(addr && is_addr && regop && csp);

    op = **csp;
    rm = RM(op);
    mod = MOD(op);
    *regop = REG_OPCODE(op);
    *is_addr = 1;

    (*csp) ++;		/* past RM */

    switch (mod) {
    case 0:
    {
	switch (rm) {
	case 0: *addr = _BX+_SI; break;
	case 1: *addr = _BX+_DI; break;
	case 2: *addr = _BP+_SI; break;
	case 3: *addr = _BP+_SI; break;
	case 4: *addr = _SI; break;
	case 5: *addr = _DI; break;
	case 6:
	    *addr = *(Bit16u*)(*csp);
	    (*csp) += 2; /* past disp16 */
	    break;
	case 7: *addr = _BX; break;
	}
	break;
    }
    case 1:
    {
	switch (rm) {
	case 0: *addr = _BX+_SI; break;
	case 1: *addr = _BX+_DI; break;
	case 2: *addr = _BP+_SI; break;
	case 3: *addr = _BP+_SI; break;
	case 4: *addr = _SI; break;
	case 5: *addr = _DI; break;
	case 6: *addr = _BP; break;
	case 7: *addr = _BX; break;
	}
	*addr += *(Bit8s*)(*csp);
	(*csp) ++;	/* past disp8 */
	break;
    }
    case 2:
    {
	switch (rm) {
	case 0: *addr = _BX+_SI; break;
	case 1: *addr = _BX+_DI; break;
	case 2: *addr = _BP+_SI; break;
	case 3: *addr = _BP+_SI; break;
	case 4: *addr = _SI; break;
	case 5: *addr = _DI; break;
	case 6: *addr = _BP; break;
	case 7: *addr = _BX; break;
	}
	*addr += *(Bit16s*)(*csp);
	(*csp) += 2;	/* past disp16 */
	break;
    }
    case 3:
	*addr = rm;
	*is_addr = 0;
	break;
    default:
	leaveemu(ERR_ASSERT);
    }

    return 0;
}



int decode_rm32(Bit32u *addr, u_int *is_addr, u_int *regop, unsigned char **csp)
{
    int rm, mod;
    Bit8u op;

    ASSERT(addr && is_addr && regop && csp);

    op = **csp;
    rm = RM(op);
    mod = MOD(op);
    *regop = REG_OPCODE(op);
    *is_addr = 1;

    (*csp) ++;		/* past RM */

    switch (mod) {
    case 0:
    {
	switch (rm) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 6:
	case 7:
	    *addr = get_reg(rm, 4);
	    break;
	case 4:
	    *addr = calculate_sib(csp, mod);	/* past SIB and any disp */
	    break;
	case 5:
	    *addr = *(Bit32u *)(*csp);
	    (*csp) += 4;	/* past disp32 */
	    break;
	}
	break;
    }
    case 1:
    {
	switch (rm) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 5:
	case 6:
	case 7:
	    *addr = get_reg(rm, 4);
	    break;
	case 4:
	    *addr = calculate_sib(csp, mod);	/* past SIB and any disp */
	    break;
	}
	*addr += **csp;
	(*csp) ++;		/* past disp8 */

	break;
    }
    case 2:
    {
	switch (rm) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 5:
	case 6:
	case 7:
	    *addr = get_reg(rm, 4);
	    break;
	case 4:
	    *addr = calculate_sib(csp, mod);	/* past SIB and any disp */
	    break;
	}
	*addr += *(Bit32u*)(*csp);
	(*csp) += 4;		/* past disp32 */
	break;
    }
    case 3:
	*addr = rm;
	*is_addr = 0;
	break;
    default:
	leaveemu(ERR_ASSERT);
    }
    
    return 0;
}


int decode_rm(Bit32u *addr, u_int *is_addr, u_int *regop, unsigned char **csp)
{
    if (opa.adb == 4)
	return decode_rm32(addr, is_addr, regop, csp);
    else
	return decode_rm16(addr, is_addr, regop, csp);
}

/* opcodes a0, a1    move mem to AL/AX/EAX */

int decode_mov_ALOb_AXOv(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op)
{
    if (op == 0xa0) {
	*src = *(Bit8u*)(*lina);
	*bytes = 1;
    } else {
	if (opa.adb == 4) {
	    *src = *(Bit32u*)(*lina);
	} else {
	    *src = *(Bit16u*)(*lina);
	}
	*bytes = opa.adb;
    }
    (*lina) += *bytes;	/* past moffs8, 16, or 32 */
    *srcmem = 1;
    if (get_memory(opa.seg, *src, dst, *bytes))
	return -1;
    *dst = REGNO_EAX;
    *dstmem = 0;

    return 0;
}

/* opcodes a2, a3    move AL/AX/EAX to mem */

int decode_mov_ObAL_OvAX(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op)
{
    if (op == 0xa2) {
	*dst = *(Bit8u*)(*lina);
	(*lina) ++;	/* past moffs8 */
	*bytes = 1;
    } else {
	if (opa.adb == 4) {
	    *dst = *(Bit32u*)(*lina);
	} else {
	    *dst = *(Bit16u*)(*lina);
	}
	(*lina) += opa.adb;	/* past moffs16 or 32 */
	*bytes = opa.adb;
    }
    *dstmem = 1;
    *src = get_reg(REGNO_EAX, *bytes);
    *srcmem = 0;

    return 0;
}


int decode_mov_EG_GE_ES_SE(unsigned char **lina, Bit32u *src, Bit32u *dst,
			   u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op)
{
    u_int addr, is_addr;
    int regop;
    int from_reg = (op == 0x88 || op == 0x89 || op == 0x8c);
    
    if (op == 0x88 || op == 0x8a) {
	*bytes = 1;
    } else if (op==0x8e || op==0x8c) {
	*bytes = 2;
    } else {
	*bytes = opa.opb;
    }
    
    decode_rm(&addr, &is_addr, &regop, lina);  /* was 32 */
    if (from_reg) {
	if (op==0x8c) regop+=8;  /* Sreg */
	*src = get_reg(regop, *bytes);
	*srcmem = 0;
	*dst = addr;
	*dstmem = is_addr;
    } else {
	if (op==0x8e) regop+=8;  /* Sreg */
	
	if (emu_get(is_addr, addr, src, *bytes))
	    return -1;
	*srcmem = is_addr;
	*dst = regop;
	*dstmem = 0;
	
	if (op==0x8e) {
	    if (regop==10 && (*src & ~3)==0) {
		/* loading NULL (global) selector into SS */
		set_guest_exc(EXC_GP, 0);
		return -1;
	    } else if (regop==9) {
		/* can't load CS with a MOV */
		set_guest_exc(6, 0);
		return -1;
	    }
	}
    }
    return 0;
}

/* opcodes b0 ... b7 */

int decode_mov_imm8_reg8(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op)
{
    *src = **lina;
    (*lina)++;	/* past imm8 */
    *srcmem = 1;
    *dst = op - 0xb0;
    *dstmem = 0;
    *bytes = 1;

    return 0;
}

/* opcodes b8 ... bf */

int decode_mov_imm_reg(unsigned char **lina, Bit32u *src, Bit32u *dst,
		       u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op)
{
    if (opa.opb==4) {
	*src = *(Bit32u*)(*lina);
    } else {
	*src = *(Bit16u*)(*lina);
    }
    (*lina) += opa.opb;	/* past imm32 or imm16 */
    *bytes = opa.opb;
    *srcmem = 1;
    *dst = op - 0xb8;
    *dstmem = 0;

    return 0;
}

/* opcodes c6, c7 */

int decode_mov_EbIb_EvIv(unsigned char **lina, Bit32u *src, Bit32u *dst,
			 u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op)
{
    int regop;
    *bytes = (op == 0xc6) ? 1 : opa.opb;
    
    decode_rm(dst, dstmem, &regop, lina);
    ASSERT(regop==0);
    if (*bytes==1) {
	*src = *(Bit8u*)(*lina);
    } else if (*bytes==2) {
	*src = *(Bit16u*)(*lina);
    } else {
	*src = *(Bit32u*)(*lina);
    }
    (*lina) += *bytes;	/* past imm */
    *srcmem = 1;

    return 0;
}

/* opcodes 08, 09, 20, 21 */

int decode_and_or_EG(unsigned char **lina, Bit32u *src, Bit32u *dst,
		     u_int *bytes, u_int *srcmem, u_int *dstmem, Bit8u op)
{
    int reg;
    
    decode_rm(dst, dstmem, &reg, lina);		/* past r/m */
    
    if (op==0x08 || op==0x20) {
	*bytes = 1;
    } else {
	*bytes = opa.opb;
    }
    *srcmem = 0; /* not strictly true; what if we OR mem and reg? */
    if (*dstmem) {
	if (get_memory(opa.seg, *dst, src, *bytes) != 0)
	    return -1;
    } else {
	*src = get_reg(*dst, *bytes);
    }
    if (op == 0x08 || op == 0x09) {
	*src |= get_reg(reg, *bytes);
    } else {
	*src &= (get_reg(reg, *bytes) | ~masks[*bytes]);
    }
    
    return 0;
}


/*
  Decodes the mov instruction (or anything else which writes to
  memory) sitting at csp (invokes debugger if not a known instr).
  Gets the data (1, 2, or 4 bytes) and puts it in src.  If it was from
  memory, sets srcmem; from a reg, clears srcmem.  dst points to the
  intended destination--if dstmem set, it's a pointer to memory, if
  clear, it is a register number.
  
  decode_mov doesn't actually copy the data; it's expected that you
  the caller want to do some manipulation on the data, and then
  actually place it in the destination yourself.

  The returned dst pointer does NOT have the segment base added to it.
  The lower level routes to read/write memory should take the segment
  into account.

  Updates the csp to next instruction.

  Returns 0 on success.
       -1 if mov generated an exception.  Should then return out of handler.  */

int decode_mov(unsigned char **csp, Bit32u *src, Bit32u *dst,
	       u_int *bytes, u_int *srcmem, u_int *dstmem)
{
    int erc = 0;
    Bit8u op;

    ASSERT(csp && *csp && src && dst && bytes && srcmem && dstmem);

    trace_mem("in decode_mov\n");

    op = **csp;
    (*csp)++;	/* past op */

    switch (op) {
    case 0xa0:	/* mov AL, Ob */
    case 0xa1:	/* mov AX, Ov */
	erc = decode_mov_ALOb_AXOv(csp, src, dst, bytes, srcmem, dstmem, op);
	break;

    case 0xa2:	/* mov Ob, AL */
    case 0xa3:	/* mov Ov, AX */
	erc = decode_mov_ObAL_OvAX(csp, src, dst, bytes, srcmem, dstmem, op);
	break;

    case 0x88:	/* mov Eb, Gb */
    case 0x89:	/* mov Ev, Gv */
    case 0x8a:	/* mov Gb, Eb */
    case 0x8b:	/* mov Gv, Ev */
    case 0x8c:	/* mov Ew, Sw */
    case 0x8e:	/* mov Sw, Ew */
	erc = decode_mov_EG_GE_ES_SE(csp, src, dst, bytes, srcmem, dstmem, op);
	break;

    case 0xb0 ... 0xb7:	/* mov imm8 -> reg8 */
	erc = decode_mov_imm8_reg8(csp, src, dst, bytes, srcmem, dstmem, op);
	break;

    case 0xb8 ... 0xbf:	/* mov imm -> reg */
	erc = decode_mov_imm_reg(csp, src, dst, bytes, srcmem, dstmem, op);
	break;

    case 0xc6:	/* mov Eb, Ib */
    case 0xc7:	/* mov Ev, Iv */
	erc = decode_mov_EbIb_EvIv(csp, src, dst, bytes, srcmem, dstmem, op);
	break;

    case 0x08:	/* or r/m8, r8 */
    case 0x09:	/* or r/m32, r32 */
    case 0x20:	/* and r/m8, r8 */
    case 0x21:	/* and r/m32, r32 */
	erc = decode_and_or_EG(csp, src, dst, bytes, srcmem, dstmem, op);
	break;

#if 0  /* ack, don't want full emulation, just decoding... */
    case 0xaa:	/* stosb */
    case 0xab:	/* stosw/d */
	erc = emu_stos(*csp);
	break;

    case 0xa4:	/* movs */
    case 0xa5:	/* movs */
	erc = emu_movs(*csp);
	break;
#endif

    case 0x80:	/* or r/m8, imm8 */
    case 0x81:	/* or r/m32, imm16 */
    case 0x83:	/* or r/m32, imm32 */
    {
	int regop, imm;

	decode_rm(dst, dstmem, &regop, csp);	/* past r/m */

	switch (regop) {
	case 1:  /* or */
	case 4:  /* and */
	{
	    if (op==0x80) {
		*bytes = 1; imm = 1;
	    } else if (op==0x81) {
		*bytes = imm = opa.opb;
	    } else {
		*bytes = opa.opb;
		imm = 1;
	    }

	    *srcmem = 1; /* not strictly true; what if we OR mem and reg or imm? */
	    if (*dstmem) {
		if (get_memory(opa.seg, *dst, src, *bytes) != 0)
		    return -1;
	    } else {
		*src = get_reg(*dst, *bytes);
	    }
	    if (regop==1) {
		if (imm==1) {
		    *src |= *(Bit8u*)(*csp);
		} else if (imm==2) {
		    *src |= *(Bit16u*)(*csp);
		} else {
		    *src |= *(Bit32u*)(*csp);
		}
	    } else {
		if (imm==1) {
 		    *src &= *(Bit8u*)(*csp);
		} else if (imm==2) {
		    *src &= *(Bit16u*)(*csp);
		} else {
		    *src &= *(Bit32u*)(*csp);
		}
	    }

	    (*csp) += imm;	/* past imm */
	    break;
	}
	default:
	    goto unknown;
	}
	break;
    }

    default:
	goto unknown;
    }
    
    trace_mem("out decode_mov\n");
    return erc;

 unknown:
    dprintf("\n\ndecode_mov called on unknown instruction %02x around %08x\n", op, (u_int)*csp);
    leaveemu(ERR_ASSERT);
    return -1;
}


