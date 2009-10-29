
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


#ifndef _ASM_H_
#define _ASM_H_

#ifdef __ASSEMBLER__

/*
 * Entry point for a procedure called from C
 */
#define ASENTRY(proc) .align 2; .globl proc; .type proc,@function; proc:
#define ENTRY(proc) ASENTRY(_ ## proc)

#define DEF_ALIAS_FN(alias, fn)			\
	.globl _ ## alias;			\
	.set _ ## alias , _ ## fn

/*
 * Align a branch target and fill the gap with NOP's
 */
#define ALIGN_TEXT .align 2,0x90

/*
 * Fucking gas won't do logial shifts in computed immediates!
 */
#define SRL(val, shamt) (((val) >> (shamt)) & ~(-1 << (32 - (shamt))))

/*
 * Various override prefixes for instructions.
 */
#define oso .byte 0x66   /* Operand size override */
#define aso .byte 0x67   /* Address size override */
#define cso .byte 0x2e   /* CS segment override prefix */
#define sso .byte 0x36   /* SS segment override prefix */
#define dso .byte 0x3e   /* DS segment override prefix */
#define eso .byte 0x26   /* ES segment override prefix */
#define fso .byte 0x64   /* FS segment override prefix */
#define gso .byte 0x65   /* GS segment override prefix */

#else /* !__ASSEMBLER__ */

#define oso ".byte 0x66\n\t " /* Operand size override */
#define aso ".byte 0x67\n\t " /* Address size override */
#define cso ".byte 0x2e\n\t " /* CS segment override prefix */
#define sso ".byte 0x36\n\t " /* SS segment override prefix */
#define dso ".byte 0x3e\n\t " /* DS segment override prefix */
#define eso ".byte 0x26\n\t " /* ES segment override prefix */
#define fso ".byte 0x64\n\t " /* FS segment override prefix */
#define gso ".byte 0x65\n\t " /* GS segment override prefix */

#endif /* !__ASSEMBLER__ */

#define RCSID(x)        .text; .asciz x

#endif /* _ASM_H_ */
