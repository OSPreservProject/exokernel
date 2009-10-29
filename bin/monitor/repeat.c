#include "opa.h"
#include "debug.h"
#include "types.h"
#include "cpu.h"
#include "handler_utils.h"
#include "repeat.h"
#include "memutil.h"
#include "emu.h"
#include "port.h"

/*

  All repeated instructions have one same prototype for setup and
  another for emulation functions:

  int emu_s_foo(unsigned char **lina, u_int *arg1, u_int *arg2)
  int emu_e_foo(unsigned char **lina, u_int arg1, u_int arg2)

  emu_s_foo does any possible setup, to keep emu_e_foo faster.
  emu_e_foo does just one iteration.
  calling repeat() with emu_e_foo repeates foo.
  Note that you'll never call emu_s_foo directly; repeat does that.

  emu_s_foo returns 0 if okay,
  returns -1 if exception.

  *lina points to the instruction to be emulated (after prefixes).
  emu_e_foo returns 0 and updates *lina to next instruction if handled.
  Returns -1 if exception.

*/


/* write other functions in repeat.c:        scas */

int emu_s_lods(unsigned char **lina, u_int *bytes_mask, u_int *delta)
{
    u_int bytes;
    u_int mask_num;

    if (**lina == 0xaa) {
	bytes = 1;		/* LOD to AL */
	mask_num = 2;		/* SI */
    } else {
	bytes = opa.opb;	/* LOD to AX or EAX */
	mask_num = opa.opb;	/* SI or ESI */
    }
    *delta = (REG(eflags) & DF_MASK) ? -bytes : bytes;
    *bytes_mask = (bytes | (mask_num << 8));
    return 0;
}

int emu_e_lods(unsigned char **lina, u_int bytes_mask, u_int delta)
{
    u_int tmp_eax;
    u_int bytes = (bytes_mask & 0xff);
    u_int mask = masks[bytes_mask >> 8];
    
    if (get_memory(REG(es), REG(edi) & mask, &tmp_eax, bytes) != 0)
	return -1;

    REG(eax) = ((REG(eax) & ~masks[bytes]) |
		(tmp_eax  &  masks[bytes]));

    if (mask == 0x0000ffff)
	LWORD(esi) += delta;
    else
	REG(esi) += delta;

    (*lina) ++;
    return 0;
}


int emu_s_cmps(unsigned char **lina, u_int *bytes_mask, u_int *delta)
{
    u_int bytes;
    u_int mask_num;

    if (**lina == 0xa6) {
	bytes = 1;
    } else {
	bytes = opa.opb;
    }
    mask_num = opa.opb;	/* SI/DI or ESI/EDI  ... strange that it's independent of bytes */
    *delta = (REG(eflags) & DF_MASK) ? -bytes : bytes;
    *bytes_mask = (bytes | (mask_num << 8));
    return 0;
}

int emu_e_cmps(unsigned char **lina, u_int bytes_mask, u_int delta)
{
    u_int eflags;
    u_int data1, data2;
    u_int bytes = (bytes_mask & 0xff);
    u_int mask = masks[bytes_mask >> 8];
    
    if (get_memory(opa.seg, REG(esi) & mask, &data1, bytes) != 0 ||
	get_memory(REG(es) , REG(edi) & mask, &data2, bytes) != 0)
	return -1;

    /* More efficient than doing the tests myself. */
    asm volatile("cmp %0, %1" :: "r" (data1), "r" (data2));
    asm volatile("pushfl");
    asm volatile("popl %0" : "=r" (eflags));
    REG(eflags) = ((REG(eflags) & ~(CF | OF | SF | ZF | AF | PF)) |
		   (    eflags  &  (CF | OF | SF | ZF | AF | PF)));

    if (mask == 0x0000ffff) {
	LWORD(esi) += delta;
	LWORD(edi) += delta;
    } else {
	REG(esi) += delta;
	REG(edi) += delta;
    }

    (*lina) ++;
    return 0;
}


int emu_s_movs(unsigned char **lina, u_int *bytes_mask, u_int *delta)
{
    u_int bytes;
    u_int mask_num;

    if (**lina == 0xa4) {
	bytes = 1;		/* MOVB */
    } else {
	bytes = opa.opb;	/* MOVW/D */
    }
    mask_num = opa.opb;	/* DI or EDI  ... strange that it's independent of bytes */
    *delta = (REG(eflags) & DF_MASK) ? -bytes : bytes;
    *bytes_mask = (bytes | (mask_num << 8));
    return 0;
}

int emu_e_movs(unsigned char **lina, u_int bytes_mask, u_int delta)
{
    u_int data;
    u_int bytes = (bytes_mask & 0xff);
    u_int mask = masks[bytes_mask >> 8];
    
    if (get_memory(REG(ds), REG(esi) & mask, &data, bytes) != 0 ||
	set_memory(REG(es), REG(edi) & mask, data, bytes) != 0)
	return -1;

    if (mask == 0x0000ffff)
	LWORD(edi) += delta;
    else
	REG(edi) += delta;

    (*lina) ++;
    return 0;
}


int emu_s_stos(unsigned char **lina, u_int *bytes_mask, u_int *delta)
{
    u_int bytes;
    u_int mask_num;

    if (**lina == 0xaa) {
	bytes = 1;		/* STO from AL */
	mask_num = 2;		/* DI */
    } else {
	bytes = opa.opb;	/* STO from AX or EAX */
	mask_num = opa.opb;	/* DI or EDI */
    }
    *delta = (REG(eflags) & DF_MASK) ? -bytes : bytes;
    *bytes_mask = (bytes | (mask_num << 8));
    return 0;
}

int emu_e_stos(unsigned char **lina, u_int bytes_mask, u_int delta)
{
    u_int bytes = (bytes_mask & 0xff);
    u_int mask = masks[bytes_mask >> 8];
    
    if (set_memory(REG(es), REG(edi) & mask, REG(eax), bytes) != 0)
	return -1;

    if (mask == 0x0000ffff)
	LWORD(edi) += delta;
    else
	REG(edi) += delta;

    (*lina) ++;
    return 0;
}


int emu_s_ins(unsigned char **lina, u_int *bytes, u_int *delta)
{
    int port = LWORD(edx);

    if ((*lina)[0] == 0x6c)
	*bytes = 1;
    else
	*bytes = opa.opb;

    *delta = (REG(eflags) & DF_MASK) ? -(*bytes) : *bytes;
    
    if (opa.pe && !guest_ports_accessible(port, *bytes)) {
	set_guest_exc(13, 0);
	return -1;
    }
    return 0;
}

int emu_e_ins(unsigned char **lina, u_int bytes, u_int delta)
{
    int port = LWORD(edx);
	
    if (set_memory(REG(es), REG(edi) & masks[opa.opb], (in_funcs[bytes])(port), bytes) != 0)
	return -1;
    REG(edi) += delta;
    
    (*lina)++;
    return 0;
}


int emu_s_outs(unsigned char **lina, u_int *bytes, u_int *delta)
{
    int port = LWORD(edx);

    if ((*lina)[0] == 0x6e)
	*bytes = 1;
    else
	*bytes = opa.opb;

    *delta = (REG(eflags) & DF_MASK) ? -(*bytes) : *bytes;
    
    if (opa.pe && !guest_ports_accessible(port, *bytes)) {
	set_guest_exc(13, 0);
	return -1;
    }
    return 0;
}

int emu_e_outs(unsigned char **lina, u_int bytes, u_int delta)
{
    int port = LWORD(edx);
    Bit32u data;
	
    if (get_memory(REG(es), REG(esi) & masks[opa.opb], &data, bytes) != 0)
	return -1;
    (out_funcs[bytes])(port, data);
    REG(esi) += delta;
    
    (*lina)++;
    return 0;
}


int once(unsigned char **lina, repeat_setup_func_t setup_func, repeat_emu_func_t emu_func)
{
    u_int arg1, arg2;

    if (setup_func(lina, &arg1, &arg2))
	return -1;
    if (emu_func(lina, arg1, arg2))
	return -1;
    return 0;
}

int repeat(unsigned char **lina, repeat_setup_func_t setup_func, repeat_emu_func_t emu_func)
{
    u_int arg1, arg2;
    int repe = 0;
    unsigned char op = **lina;
    unsigned char **saved_lina = lina;

    /* For some instructions it's REP, for others, it's REPE.  Stupid Intel. */
    if (op==0xa6 || op==0xa7 || op==0xae || op==0xaf) {
	ASSERT(opa.repe);
	repe = 1;
    }

    if (setup_func(lina, &arg1, &arg2))
	return -1;

    while (REG(ecx) & masks[opa.adb]) {
	lina = saved_lina;
	if (emu_func(lina, arg1, arg2))
	    return -1;
	REG(ecx)--;
	if ((repe       && (REG(eflags) & ZF_MASK)==0) ||
	    (opa.repne && (REG(eflags) & ZF_MASK)==1))
	    break;
    }
    return 0;
}

