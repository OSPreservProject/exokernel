
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

#ifndef __UBB_INST_H__
#define __UBB_INST_H__

/* 
 * Calling convention: first n args are passed in registers, result is
 * returned in r0.
 */

#define U_REG_MAX 64
#define U_NO_REG 0
typedef unsigned u_reg;


/* ops */
typedef enum  {
		U_BEGIN,
#               define  u_op(n,n1)        	U_ ## n1,
#               include "uops.h"
		U_IMM_BEGIN,
#               define  u_op(n,n1)        	U_ ## n1 ## I,
#               include "uops.h"
		U_END
} uop_t;

/* 
 * All instructions take 4 words: op, rd, rs1, rs2|imm.  Optional 
 * parts are ignored, and we provide infinite registers. 
 */
struct udf_inst {
	uop_t op;
	unsigned val[3];
};

#define u_is_imm(op) ((op) > U_IMM_BEGIN && (op) < U_END)
#define u_valid_op(op) ((op) > U_BEGIN && (op) < U_END && (op) != U_IMM_BEGIN)
#define u_valid_reg(reg) ((reg) >= 0 && (reg) < U_REG_MAX)

#define U_OP(i) ((i)->op)
#define U_RD(i) ((i)->val[0])
#define U_RS1(i) ((i)->val[1])
#define U_RS2(i) ((i)->val[2])
#define U_IMM(i) ((i)->val[2])

#define u_bop(inst, op, rd, rs1, reg_or_imm) do { 		\
	struct udf_inst *_inst = inst;				\
	U_OP(_inst) =  U_ ## op; 					\
	U_RD(_inst) =  rd; 					\
	U_RS1(_inst) = rs1; 					\
	U_RS2(_inst) = reg_or_imm; 					\
} while(0)

/* instructions. */
#define u_add(inst, rd, rs1, rs2) 	u_bop(inst, ADD, rd, rs1, rs2)
#define u_sub(inst, rd, rs1, rs2) 	u_bop(inst, SUB, rd, rs1, rs2)
#define u_div(inst, rd, rs1, rs2) 	u_bop(inst, DIV, rd, rs1, rs2)
#define u_mul(inst, rd, rs1, rs2) 	u_bop(inst, MUL, rd, rs1, rs2)

#define u_addi(inst, rd, rs1, imm) 	u_bop(inst, ADDI, rd, rs1, imm)
#define u_subi(inst, rd, rs1, imm) 	u_bop(inst, SUBI, rd, rs1, imm)
#define u_divi(inst, rd, rs1, imm) 	u_bop(inst, DIVI, rd, rs1, imm)
#define u_muli(inst, rd, rs1, imm) 	u_bop(inst, MULI, rd, rs1, imm)

#define u_and(inst, rd, rs1, rs2) 	u_bop(inst, AND, rd, rs1, rs2)
#define u_or(inst, rd, rs1, rs2) 	u_bop(inst, OR, rd, rs1, rs2)
#define u_xor(inst, rd, rs1, rs2) 	u_bop(inst, XOR, rd, rs1, rs2)

#define u_andi(inst, rd, rs1, imm) 	u_bop(inst, ANDI, rd, rs1, imm)
#define u_ori(inst, rd, rs1, imm) 	u_bop(inst, ORI, rd, rs1, imm)
#define u_xori(inst, rd, rs1, imm) 	u_bop(inst, XORI, rd, rs1, imm)

#define u_beq(inst, rs1, rs2, offset) 	u_bop(inst, BEQ, rs1, rs2, offset)
#define u_bne(inst, rs1, rs2, offset) 	u_bop(inst, BNE, rs1, rs2, offset)
#define u_blt(inst, rs1, rs2, offset) 	u_bop(inst, BLT, rs1, rs2, offset)
#define u_ble(inst, rs1, rs2, offset) 	u_bop(inst, BLE, rs1, rs2, offset)
#define u_bgt(inst, rs1, rs2, offset) 	u_bop(inst, BGT, rs1, rs2, offset)
#define u_bge(inst, rs1, rs2, offset) 	u_bop(inst, BGE, rs1, rs2, offset)

#define u_beqi(inst, rs1, imm, offset) 	u_bop(inst, BEQI, rs1, imm, offset)
#define u_bnei(inst, rs1, imm, offset) 	u_bop(inst, BNEI, rs1, imm, offset)
#define u_blti(inst, rs1, imm, offset) 	u_bop(inst, BLTI, rs1, imm, offset)
#define u_blei(inst, rs1, imm, offset) 	u_bop(inst, BLEI, rs1, imm, offset)
#define u_bgti(inst, rs1, imm, offset) 	u_bop(inst, BGTI, rs1, imm, offset)
#define u_bgei(inst, rs1, imm, offset) 	u_bop(inst, BGEI, rs1, imm, offset)

#define u_mov(inst, rd, rs1) 		u_bop(inst, MOV, rd, rs1, 0)
#define u_neg(inst, rd, rs1) 		u_bop(inst, NEG, rd, rs1, 0)
#define u_not(inst, rd, rs1) 		u_bop(inst, NOT, rd, rs1, 0)

#define u_movi(inst, rd, imm) 		u_bop(inst, MOVI, rd, imm, 0)
#define u_negi(inst, rd, imm) 		u_bop(inst, NEGI, rd, imm, 0)
#define u_noti(inst, rd, imm) 		u_bop(inst, NOTI, rd, imm, 0)

#define u_ldii(inst, rd, rs1, imm) 	u_bop(inst, LDII, rd, rs1, imm)
#define u_ldsi(inst, rd, rs1, imm) 	u_bop(inst, LDSI, rd, rs1, imm)
#define u_ldbi(inst, rd, rs1, imm) 	u_bop(inst, LDBI, rd, rs1, imm)
#define u_stii(inst, rd, rs1, imm) 	u_bop(inst, STII, rd, rs1, imm)
#define u_stsi(inst, rd, rs1, imm) 	u_bop(inst, STSI, rd, rs1, imm)
#define u_stbi(inst, rd, rs1, imm) 	u_bop(inst, STBI, rd, rs1, imm)

#define u_ret(inst, rd)			u_bop(inst, RET, rd, 0, 0)
#define u_j(inst, rd)			u_bop(inst, J, rd, 0, 0)
#define u_add_ext(inst, rd, rn, type)	u_bop(inst, ADD_EXT, rd, rn, type)
#define u_add_cext(inst, rd, n, type)	u_bop(inst, ADD_CEXT, rd, n, type)

#define u_reti(inst, imm)		u_bop(inst, RETI, 0, 0, imm)
#define u_ji(inst, imm)			u_bop(inst, J, 0, 0, imm)

#define u_add_exti(inst, rd, rn, typei)	u_bop(inst, ADD_EXTI, rd, rn, typei)
#define u_add_cexti(inst, rd, n, typei)	u_bop(inst, ADD_CEXTI, rd, n, typei)

#define u_sw_seg(inst, segno)		u_bop(inst, SW_SEG, 0, 0, segno)

/* rd = meta[offset + index * scale] */
#define u_index_ldi(inst, rd, offset, scalei) 		\
	u_bop(inst, INDEX_LDI, rd, offset, scalei)

#endif /* __UBB_INST_H__ */
