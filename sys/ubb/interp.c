
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

#ifdef EXOPC
#include "kernel.h"
#else
#include <stdio.h>
#endif
#include "ubb.h"
#include "udf.h"
#include "ubb-inst.h"
#include "demand.h"
#include "kexcept.h"

/* ALU */
#define bop(oper,im) do { (RD = RS1 oper ((im) ? IMM : RS2)); next; } while(0)
#define unary(oper, im) do { RD = oper ((im) ? IMM : RS1); next; } while(0)

/* mem */
#define memory(stmt, cast, access) do {					\
	if(udf_illegal_access(IMM+RS1, sizeof(* (cast) 0), access))	\
		{ fatal(death); return 0; } 				\
	stmt;								\
	next;								\
} while(0)

#define store(cast) \
	memory((*(cast) ((char *)xn + RS1 + IMM) = RD), cast, w_access)
#define load(cast)  \
	memory((RD = *(cast) ((char *)xn + RS1 + IMM)), cast, r_access)

/* control flow */
#define u_cond(oper, s1, s2, offset) do {				\
	ip += ((s1) oper (s2)) ? ((int)offset+1) : 1;			\
	check_budget;							\
	goto *jtab[U_OP(ip)];						\
} while(0)

#define cond(oper) 	u_cond(oper, RD, RS1, IMM)
#define condi(oper) 	u_cond(oper, RD, U_RS1(ip), IMM)

#define INST_BUDGET	1024

#define check_budget do {					\
	if(ninsts++ > INST_BUDGET) {				\
		fatal(Runtime exceeded);			\
		return 0;					\
	}							\
} while(0)

#define next do { 						\
	ip+=1; 							\
	ninsts++;						\
	goto *jtab[U_OP(ip)]; 					\
} while(0)

/* sugar. */
#define RD 	(regs[U_RD(ip)])
#define RS1 	(regs[U_RS1(ip)])
#define RS2 	(regs[U_RS2(ip)])
#define IMM 	(U_IMM(ip))


/* uses threaded gotos to interpret code */
int udf_interp(struct udf_fun *f, struct udf_ctx *ctx, void *xn, unsigned *params, int nargs, struct udf_ext *r_access, struct udf_ext *w_access) {
        void *jtab[] = {
			&&NIL,
#			define u_op(n,n1) &&n1,
#			include "uops.h"
			&&NIL, 
#			define u_op(n,n1) &&n1 ## I,
#			include "uops.h"
			&&NIL
        };
	static unsigned regs[U_REG_MAX];
	struct udf_inst *ip;
	int n, ninsts;
	struct udf_ext *w_orig, *r_orig;
	void *xn_orig;

	ninsts = 0;

	xn_orig = xn;
	w_orig = w_access;
	r_orig = r_access;
	regs[0] = 0;
	/* load the parameters. */
	memcpy(&regs[1], params, sizeof params[0] * nargs);

	ip = f->insts;
	n = f->ninst;

	/* start it off */
	goto *jtab[U_OP(ip)];

	/* +,-,/,* */
	ADD : bop(+,0);
	SUB : bop(-,0);
	DIV : bop(/,0);
	MUL : bop(*,0);

	AND : bop(&,1);
	OR : bop(|,0);
	XOR : bop(^,0);

	ADDI: bop(+,1);
	SUBI: bop(-,1);
	MULI: bop(*,1);
	DIVI: bop(/,1);

	ANDI : bop(&,1);
	ORI : bop(|,1);	
	XORI : bop(^,1);

	/* !=,==,>=,<=, >, < */
        BNEI: condi(!=);
        BNE : cond(!=);
	BEQI: condi(==);
	BEQ : cond(==);
	BGEI: condi(>=);
	BGE : cond(>=);
	BGTI: condi(>);
	BGT : cond(>);
	BLEI: condi(<=);
	BLE : cond(<=);
	BLTI: condi(<);
	BLT : cond(<);

	MOV : unary(/**/ , 0);
	NEG : unary(~, 0); 
	NOT : unary(!, 0); 

	MOVI : unary(/**/, 1);
	NEGI : unary(~, 1);
	NOTI : unary(!, 1);

	INDEX_LDI:	fatal(Not handling yet.);
		
	LDII: load(unsigned *); 
	LDSI: load(unsigned short *);
	LDBI: load(unsigned char *); 
	STII: store(unsigned *);
	STSI: store(unsigned short *);
	STBI: store(unsigned char *);

	JI: 	
		ip += (int)U_IMM(ip); 
		check_budget; 
		goto *jtab[U_OP(ip)];
	RETI: 	
		return U_IMM(ip);
	RET: 	
		if(f->result_t == UBB_SET)
			return 1;
		else if(U_OP(ip) == U_RET || U_OP(ip) == U_RETI)
			return RD;
		else
			return regs[U_IMM(ip)]; 
	SW_SEGI: J: STB: STS: STI: LDB: LDS: LDI: INDEX_LD:
		fatal(Should not get here.);
		
	SW_SEG: 
		if(IMM != UDF_SELF) 
			try(udf_switch_seg(ctx,IMM, &xn, &r_access, &w_access));
		else  {
			 xn = xn_orig;
			 r_access = r_orig;
			 w_access = w_orig;
		}
		next;

	/* 
	 * Should do this more extensibly: don't want an 
	 * opcode per call.  Should just have a pointer to
	 * the procedure, and send the entire instruction along.
	 */
	ADD_EXT:  udf_add(RD, RS1, RS2); 		next;
	ADD_EXTI: udf_add(RD, RS1, IMM); 		next;
	ADD_CEXT: udf_add(RD, U_RS1(ip), RS2);		next;
	ADD_CEXTI: udf_add(RD, U_RS1(ip), IMM);		next;

	NIL:
		check(0, Hit nil inst in UDF; should not get here, 0);
	return 0;
}

