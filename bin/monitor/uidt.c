/* no globals */

/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

#include <xok/mmu.h>
#include <xok_include/string.h>

#include "config.h"
#include "debug.h"
#include "cpu.h"
#include "pagetable.h"
#include "uidt.h"

/*
 * Just does an 'iret'.
 */
void uidt_unregister(u_int vect) 
{
    char *code = (char *)UIDT_VECT_TO_ADDR(vect);
    u_int size;
    char *start;

    if (interrupt_has_error_code_p(vect)) 
	start = &&unregister_skeleton_ec;
    else
	start = &&unregister_skeleton;
    size = (u_int)&&unregister_skeleton_end - (u_int)start;

    bcopy(start, &code[0], size);
    return;

 unregister_skeleton_ec:
#if 0
    asm("addl $4, %esp\n"); /* throw away the error code */ 
#else
    asm("popl %edx\n"); /* throw away the error code */ 
#endif
 unregister_skeleton:
    asm("iret");
 unregister_skeleton_end:
}

/*
 * Just does a jump to handler.
 */
void uidt_register_sfunc(u_int vect, u_int handler)
{
    char *code = (char *)UIDT_VECT_TO_ADDR(vect);
    ASSERT(handler);
    code[0] = JMP_REL32_OPCODE;
    /* The jump is relative the instruction after the jump. */
    *(u_int*)&code[1] = handler - (u_int)&code[5];
}


/*
 * Passes the trap number and the trap time %eip to the (presumably) c
 * function 'cfunc'.  (Also, takes care of saving and restoring the
 * caller-saved registers.)  
 */
void uidt_register_cfunc(u_int vect, u_int cfunc)
{
    char *code = (char *)UIDT_VECT_TO_ADDR(vect);
    int size;
    char *start;

#if 0
    debug("uidt_register_cfunc vect=%d cfunc=0x%08x code=0x%08x\n",
	  vect, cfunc, (u_int)code);
#endif

    if (interrupt_has_error_code_p(vect)) 
	start = &&cfunc_skeleton_ec;
    else
	start = &&cfunc_skeleton_no_ec;

    size = (u_int)&&cfunc_skeleton_end - (u_int)start;
#if 0
    debug("uidt_register_cfunc stub size=%d max size=%d\n", size,
	  UIDT_STUB_SIZE);
#endif
    ASSERT(size<=UIDT_STUB_SIZE);
    bcopy(start, &code[0], size);

    code += (u_int)&&cfunc_pushl_vector - (u_int)start;
    code[0] = PUSH_IMM32_OPCODE;
    *(u_int*)&code[1] = vect;

    code += &&cfunc_call_cfunc - &&cfunc_pushl_vector;
    code[0] = CALL_REL32_OPCODE;
    /* the call is relative to the next instr, (call is 5 bytes long) */
    *(u_int*)&code[1] = cfunc - (u_int)&code[5];
    return;

    /*** stack on entry:  (tt == trap time)
     * tt_eflags
     * tt_cs
     * tt_eip
     * [optional] tt_error_code  <--- %esp
     * (If the given trap has no error code %esp will point to tt_eip.)
     */
 cfunc_skeleton_ec:
   asm("addl $4, %esp");   /* throw away the error code */
 cfunc_skeleton_no_ec:
   /* do caller-saves, and push the tt_eip */
   asm("pushl %eax\n"
       "\tpushl %edx\n"
       "\tpushl %ecx\n"
       "\tpushl 0x0c(%esp)\n");
 cfunc_pushl_vector:
   asm (".byte 0x0, 0x0, 0x0, 0x0, 0x0"); /* leave space for a 'pushl num_vector' */
 cfunc_call_cfunc:
   asm (".byte 0x0, 0x0, 0x0, 0x0, 0x0"); /* leave space for a 'call' */
   asm("popl	%ecx\n" /* ideally we'd used "addl 8, %esp", and not     */ 
       "\tpopl	%ecx\n" /* 2 popl's, but then the stub would be too big. */
       "\tpopl	%ecx\n"
       "\tpopl	%edx\n"
       "\tpopl	%eax\n"
       "\tiret\n");
 cfunc_skeleton_end:
}


void uidt_default_chandler(int trapno, u_int err)
{
    exit(0);
}

/*
 * Sets all interrups to simply 'iret'.
 */
void uidt_initialize(void) 
{
    int i;
    u_int uidt_start = UIDT;
    u_int uidt_size  = UIDT_STUB_SIZE*UIDT_NUM_STUBS;
    
    debug("uidt_initialize...\n");

    map_pages(PGNO(uidt_start), PGNO(PGROUNDUP(uidt_size)));
    
    for (i=0; i < 256; i++) {
	uidt_register_cfunc(i, (u_int)&uidt_default_chandler);
    }
}
