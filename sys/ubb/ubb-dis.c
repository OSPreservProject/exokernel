
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

#include "demand.h"
#include "ubb.h"

static char *op_s[] = {
                "U_BEGIN",
#               define  u_op(n,n1)              #n,
#               include "uops.h"
                "U_IMM_BEGIN",
#               define  u_op(n,n1)              #n "i",
#               include "uops.h"
                "U_END",
		0
};

static void print_inst(struct udf_inst *ip) {
	unsigned op, rd, rs1, rs2, imm;
	char *ops;

	op = U_OP(ip);
	rd = U_RD(ip);
	rs1 = U_RS1(ip);
	rs2 = U_RS2(ip);
	imm = U_IMM(ip);
	ops = op_s[op];

	switch(op) {

	case U_ADD: case U_SUB: case U_DIV: case U_MUL: 
	case U_AND: case U_OR: case U_XOR:
		printf("%s r%u, r%u, r%u\n", ops, rd, rs1, rs2);
		break;

	case U_ADDI: case U_SUBI: case U_DIVI: case U_MULI: 
		printf("%s r%u, r%u, #%u\n", ops, rd, rs1, rs2);
		break;

	case U_ANDI: case U_ORI: case U_XORI:
		printf("%s r%u, r%u, #0x%x\n", ops, rd, rs1, imm);
		break;

	case U_BNE: case U_BEQ: case U_BGT: 
	case U_BGE: case U_BLT: case U_BLE:
		printf("%s r%u, r%u, cur + %d\n", ops, rd, rs1, imm); 
		break;

	case U_BNEI: case U_BEQI: case U_BGTI: 
	case U_BGEI: case U_BLTI: case U_BLEI:
		printf("%s r%u, #%u, cur + %d\n", ops, rd, rs1, imm); 
		break;

	case U_MOV: case U_NEG: case U_NOT:
		printf("%s r%u, r%u\n", ops, rd, rs1);
		break;

	case U_LDI: case U_LDS: case U_LDB: 
	case U_STI: case U_STS: case U_STB: 
		printf("%s r%u, [ r%d + r%d]\n", ops, rd, rs1, rs2);
		break;

	case U_LDII: case U_LDSI: case U_LDBI: 
	case U_STII: case U_STSI: case U_STBI: 
		printf("%s r%u, [ r%d + #%d]\n", ops, rd, rs1, rs2);
		break;

	case U_MOVI: case U_NEGI: case U_NOTI:
		printf("%s r%u, #%u\n", ops, rd, rs1);
		break;

	case U_RETI: case U_JI: 
		printf("%s #%u\n", ops, imm);
		break;
	case U_ADD_EXT: 
	case U_ADD_EXTI: 
		printf("%s base = r%u, n = r%u, type = %s%u\n", ops, 
			rd, rs1, u_is_imm(op) ? "#" : "r", rs2);
		break;
	case U_ADD_CEXT:
	case U_ADD_CEXTI:
		printf("%s base = r%u, n = #%u, type = %s%u\n", ops, 
			rd, rs1, u_is_imm(op) ? "#" : "r", rs2);
		break;

	case U_SW_SEG:
		printf("%s seg no = %u\n", ops, imm);
		break;

	case U_RET: case U_J: 
		printf("%s r%u\n", ops, rd);
		break;

	default:
		printf("(nil op) = %d\n", op);
		fatal(Bogus op);
	}
}
void udf_dis(struct udf_inst *ip, int n) {
	int i;

	for(i = 0; i < n; i++) {
		printf("[%u] = ", i);
		print_inst(ip+i);
	}
}
