
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

/* +,-,/,* */
u_bin(add,         ADD)         /* arithimetic. */
u_bin(sub,         SUB)
u_bin(div,         DIV)
u_bin(mul,         MUL)

/* & | ^ */
u_bit(and,         AND)         /* bitwise ops */
u_bit(or,          OR)
u_bit(xor,         XOR)

/* if(rs1 op rs2) */
u_bool(beq,     BEQ)
u_bool(bne,     BNE)
u_bool(blt,     BLT)
u_bool(ble,     BLE)
u_bool(bgt,     BGT)
u_bool(bge,     BGE)

/* =, ~, ! */
u_un(mov,         MOV)
u_un(neg,         NEG)
u_un(not,         NOT)

u_mem(ldi,       LDI)
u_mem(lds,       LDS)
u_mem(ldb,       LDB)
u_mem(sti,       STI)
u_mem(sts,       STS)
u_mem(stb,       STB)

/* op val */
u_special(ret,          	RET)    
u_special(j,          	J)    
u_special(add_ext,      	ADD_EXT)
u_special(add_cext,      	ADD_CEXT)

/* add src + index * dst (src & dst are constants). */
u_special(index_ld,      	INDEX_LD)

/* Switch to named segment. */
u_special(sw_seg,      	SW_SEG)

#undef u_op
