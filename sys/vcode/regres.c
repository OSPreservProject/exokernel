
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

int v_errors;
#include <math.h>
#include "vcode.h"


#define vdemand(bool, msg) \
	(void)((bool) || __vdemand(#bool, #msg, __FILE__, __LINE__, ip))
#define vdemand2(bool, msg) \
	(void)((bool) || __vdemand(#bool, #msg, __FILE__, __LINE__, ip2))

static int __vdemand(char *bool, char *msg, char *file, int line, int (*ip)()) {
        printf("file %s, line %d: %s\n",file,line,bool);
        printf("%s\n",msg);
	v_errors++;
	v_dump((void *)ip);
	return 0;
}
#define BITSPERBYTE 8

int main(int argc, char *argv[]) {
	v_reg_t	arg_list[100];		/* make sure 100 is big enough */
	static v_reg_t	zero;		/* hack */
	v_reg_t	reg;
	v_reg_t 	rdi, rs1i, rs2i;
	char		dc, s1c, s2c, cvc;
	v_reg_t 	rdc, rs1c, rs2c;
	unsigned char	duc, s1uc, s2uc, cvuc;
	v_reg_t 	rduc, rs1uc, rs2uc;
	short		ds, s1s, s2s, cvs;
	v_reg_t 	rds, rs1s, rs2s;
	unsigned short	dus, s1us, s2us, cvus;
	v_reg_t 	rdus, rs1us, rs2us;
	int 	     	di, s1i, s2i, cvi;
	v_reg_t 	rdu, rs1u, rs2u;
	unsigned     	du, s1u, s2u, cvu;
	v_reg_t 	rdul, rs1ul, rs2ul;
	unsigned long   dul, s1ul, s2ul, cvul;
	v_reg_t 	rdl, rs1l, rs2l;
	long     	dl, s1l, s2l, cvl;
	v_reg_t	rdf, rs1f, rs2f;
	float		df, s1f, s2f, cvf;
	v_reg_t	rdd, rs1d, rs2d;
	double		dd, s1d, s2d, cvd;
	v_reg_t 	rdp, rs1p, rs2p;
	char 		*dp, *s1p, *s2p, *cvp;
	v_label_t	l;
	static v_code insn[1000];
	static v_code insn2[1000];
	v_iptr 	ip;
	v_iptr 	ip2;
	int 	iters = (argc == 2) ? atoi(argv[1]) : 1;
	int 	aligned_offset, unaligned_offset;
	int 	shifti, shiftu, shiftl, shiftul;

	/* v_init(); */

loop:
	s1p = (void *)rand();
	s2p = (void *)rand();

	s1i = rand() - rand(); 
	s2i = rand() - rand();
	if(!(s2i = rand() - rand()))
		s2i = rand() + 1;

	s1u = rand() - rand();
	if(!(s2u = rand() - rand()))
		s2u = rand() + 1;

	s1ul = rand() - rand();
	if(!(s2ul = rand() - rand()))
		s2ul = rand() + 1;

	s1l = rand() - rand();
	if(!(s2l = rand() - rand()))
		s2l = rand() + 1;

	s2us = rand() - rand();
	if(!(s2us = rand() - rand()))
		s2us = rand() + 1;

	s1f = (float)rand() / rand();
	s2f = (float)rand() / (rand()+1) * ((rand()%1) ? 1. : -1.);

	s1d = (double)rand() / rand();
	s2d = (double)rand() / (rand()+1) * ((rand()%1) ? 1. : -1.);

	shifti = rand() % (sizeof(int) * BITSPERBYTE);
	shiftu = rand() % (sizeof(unsigned) * BITSPERBYTE);
	shiftl = rand() % (sizeof(long) * BITSPERBYTE);
	shiftul = rand() % (sizeof(unsigned long) * BITSPERBYTE);
	/* 
	 * Rip off the lower bits to give 8 byte alignement; will have
	 * to change this for machines with more stringent requirements.
	 */
	aligned_offset = (rand() - rand()) & ~7;
	unaligned_offset = (rand() - rand());


	/* reg <- (reg + reg) */
        v_lambda("addi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_addi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i + s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), addi failed);

	/* reg <- (reg + imm) */
        v_lambda("addii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_addii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), addii failed);


	/* reg <- (reg + reg) */
        v_lambda("addu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_addu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u + s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), addu failed);

	/* reg <- (reg + imm) */
        v_lambda("addui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_addui(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), addui failed);


	/* reg <- (reg + reg) */
        v_lambda("addul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_addul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul + s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), addul failed);

	/* reg <- (reg + imm) */
        v_lambda("adduli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_adduli(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), adduli failed);


	/* reg <- (reg + reg) */
        v_lambda("addl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_addl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l + s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), addl failed);

	/* reg <- (reg + imm) */
        v_lambda("addli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_addli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), addli failed);


	/* reg <- (reg - reg) */
        v_lambda("subi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_subi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i - s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), subi failed);

	/* reg <- (reg - imm) */
        v_lambda("subii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_subii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), subii failed);


	/* reg <- (reg - reg) */
        v_lambda("subu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_subu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u - s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), subu failed);

	/* reg <- (reg - imm) */
        v_lambda("subui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_subui(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), subui failed);


	/* reg <- (reg - reg) */
        v_lambda("subul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_subul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul - s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), subul failed);

	/* reg <- (reg - imm) */
        v_lambda("subuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_subuli(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), subuli failed);


	/* reg <- (reg - reg) */
        v_lambda("subl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_subl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l - s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), subl failed);

	/* reg <- (reg - imm) */
        v_lambda("subli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_subli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), subli failed);


	/* reg <- (reg * reg) */
        v_lambda("muli", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_muli(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i * s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), muli failed);

	/* reg <- (reg * imm) */
        v_lambda("mulii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_mulii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), mulii failed);


	/* reg <- (reg * reg) */
        v_lambda("mulu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_mulu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u * s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), mulu failed);

	/* reg <- (reg * imm) */
        v_lambda("mului", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_mului(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), mului failed);


	/* reg <- (reg * reg) */
        v_lambda("mulul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_mulul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul * s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), mulul failed);

	/* reg <- (reg * imm) */
        v_lambda("mululi", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_mululi(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), mululi failed);


	/* reg <- (reg * reg) */
        v_lambda("mull", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_mull(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l * s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), mull failed);

	/* reg <- (reg * imm) */
        v_lambda("mulli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_mulli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), mulli failed);


	/* reg <- (reg / reg) */
        v_lambda("divi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_divi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i / s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), divi failed);

	/* reg <- (reg / imm) */
        v_lambda("divii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_divii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), divii failed);


	/* reg <- (reg / reg) */
        v_lambda("divu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_divu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u / s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), divu failed);

	/* reg <- (reg / imm) */
        v_lambda("divui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_divui(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), divui failed);


	/* reg <- (reg / reg) */
        v_lambda("divul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_divul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul / s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), divul failed);

	/* reg <- (reg / imm) */
        v_lambda("divuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_divuli(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), divuli failed);


	/* reg <- (reg / reg) */
        v_lambda("divl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_divl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l / s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), divl failed);

	/* reg <- (reg / imm) */
        v_lambda("divli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_divli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), divli failed);


	/* reg <- (reg % reg) */
        v_lambda("modi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_modi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i % s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), modi failed);

	/* reg <- (reg % imm) */
        v_lambda("modii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_modii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), modii failed);


	/* reg <- (reg % reg) */
        v_lambda("modu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_modu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u % s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), modu failed);

	/* reg <- (reg % imm) */
        v_lambda("modui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_modui(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), modui failed);


	/* reg <- (reg % reg) */
        v_lambda("modul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_modul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul % s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), modul failed);

	/* reg <- (reg % imm) */
        v_lambda("moduli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_moduli(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), moduli failed);


	/* reg <- (reg % reg) */
        v_lambda("modl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_modl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l % s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), modl failed);

	/* reg <- (reg % imm) */
        v_lambda("modli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_modli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), modli failed);


	/* reg <- (reg ^ reg) */
        v_lambda("xori", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_xori(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i ^ s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), xori failed);

	/* reg <- (reg ^ imm) */
        v_lambda("xorii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_xorii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), xorii failed);


	/* reg <- (reg ^ reg) */
        v_lambda("xoru", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_xoru(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u ^ s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), xoru failed);

	/* reg <- (reg ^ imm) */
        v_lambda("xorui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_xorui(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), xorui failed);


	/* reg <- (reg ^ reg) */
        v_lambda("xorul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_xorul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul ^ s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), xorul failed);

	/* reg <- (reg ^ imm) */
        v_lambda("xoruli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_xoruli(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), xoruli failed);


	/* reg <- (reg ^ reg) */
        v_lambda("xorl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_xorl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l ^ s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), xorl failed);

	/* reg <- (reg ^ imm) */
        v_lambda("xorli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_xorli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), xorli failed);


	/* reg <- (reg & reg) */
        v_lambda("andi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_andi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i & s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), andi failed);

	/* reg <- (reg & imm) */
        v_lambda("andii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_andii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), andii failed);


	/* reg <- (reg & reg) */
        v_lambda("andu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_andu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u & s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), andu failed);

	/* reg <- (reg & imm) */
        v_lambda("andui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_andui(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), andui failed);


	/* reg <- (reg & reg) */
        v_lambda("andul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_andul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul & s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), andul failed);

	/* reg <- (reg & imm) */
        v_lambda("anduli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_anduli(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), anduli failed);


	/* reg <- (reg & reg) */
        v_lambda("andl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_andl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l & s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), andl failed);

	/* reg <- (reg & imm) */
        v_lambda("andli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_andli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), andli failed);


	/* reg <- (reg | reg) */
        v_lambda("ori", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_ori(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i | s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), ori failed);

	/* reg <- (reg | imm) */
        v_lambda("orii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_orii(rdi, arg_list[0], s2i);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), orii failed);


	/* reg <- (reg | reg) */
        v_lambda("oru", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_oru(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u | s2u);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, s2u), oru failed);

	/* reg <- (reg | imm) */
        v_lambda("orui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_orui(rdu, arg_list[0], s2u);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), orui failed);


	/* reg <- (reg | reg) */
        v_lambda("orul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_orul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul | s2ul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), orul failed);

	/* reg <- (reg | imm) */
        v_lambda("oruli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_oruli(rdul, arg_list[0], s2ul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), oruli failed);


	/* reg <- (reg | reg) */
        v_lambda("orl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_orl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l | s2l);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, s2l), orl failed);

	/* reg <- (reg | imm) */
        v_lambda("orli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_orli(rdl, arg_list[0], s2l);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), orli failed);


	/* reg <- (reg << reg) */
        v_lambda("lshi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_lshi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i << shifti);
        vdemand(di == ((int(*)(int,int))ip)(s1i, shifti), lshi failed);

	/* reg <- (reg << imm) */
        v_lambda("lshii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_lshii(rdi, arg_list[0], shifti);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), lshii failed);


	/* reg <- (reg << reg) */
        v_lambda("lshu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_lshu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u << shiftu);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, shiftu), lshu failed);

	/* reg <- (reg << imm) */
        v_lambda("lshui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_lshui(rdu, arg_list[0], shiftu);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), lshui failed);


	/* reg <- (reg << reg) */
        v_lambda("lshul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_lshul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul << shiftul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, shiftul), lshul failed);

	/* reg <- (reg << imm) */
        v_lambda("lshuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_lshuli(rdul, arg_list[0], shiftul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), lshuli failed);


	/* reg <- (reg << reg) */
        v_lambda("lshl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_lshl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l << shiftl);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, shiftl), lshl failed);

	/* reg <- (reg << imm) */
        v_lambda("lshli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_lshli(rdl, arg_list[0], shiftl);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), lshli failed);


	/* reg <- (reg >> reg) */
        v_lambda("rshi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_rshi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = (s1i >> shifti);
        vdemand(di == ((int(*)(int,int))ip)(s1i, shifti), rshi failed);

	/* reg <- (reg >> imm) */
        v_lambda("rshii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_rshii(rdi, arg_list[0], shifti);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), rshii failed);


	/* reg <- (reg >> reg) */
        v_lambda("rshu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_rshu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = (s1u >> shiftu);
        vdemand(du == ((unsigned(*)(unsigned,unsigned))ip)(s1u, shiftu), rshu failed);

	/* reg <- (reg >> imm) */
        v_lambda("rshui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_rshui(rdu, arg_list[0], shiftu);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), rshui failed);


	/* reg <- (reg >> reg) */
        v_lambda("rshul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_rshul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = (s1ul >> shiftul);
        vdemand(dul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul, shiftul), rshul failed);

	/* reg <- (reg >> imm) */
        v_lambda("rshuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_rshuli(rdul, arg_list[0], shiftul);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), rshuli failed);


	/* reg <- (reg >> reg) */
        v_lambda("rshl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_rshl(rdl, arg_list[0], arg_list[1]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = (s1l >> shiftl);
        vdemand(dl == ((long(*)(long,long))ip)(s1l, shiftl), rshl failed);

	/* reg <- (reg >> imm) */
        v_lambda("rshli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_rshli(rdl, arg_list[0], shiftl);
        	v_retl(rdl);
        ip = v_end(0).i;
        vdemand(dl == ((long(*)(long))ip)(s1l), rshli failed);


	/* reg <- ~ reg */
        v_lambda("comi", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_comi(rdi, arg_list[0]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = ~ s1i;
        vdemand(di == ((int(*)(int))ip)(s1i), comi failed);


	/* reg <- ~ reg */
        v_lambda("comu", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_comu(rdu, arg_list[0]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = ~ s1u;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), comu failed);


	/* reg <- ~ reg */
        v_lambda("comul", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_comul(rdul, arg_list[0]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = ~ s1ul;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), comul failed);


	/* reg <- ~ reg */
        v_lambda("coml", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_coml(rdl, arg_list[0]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = ~ s1l;
        vdemand(dl == ((long(*)(long))ip)(s1l), coml failed);


	/* reg <- ! reg */
        v_lambda("noti", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_noti(rdi, arg_list[0]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = ! s1i;
        vdemand(di == ((int(*)(int))ip)(s1i), noti failed);


	/* reg <- ! reg */
        v_lambda("notu", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_notu(rdu, arg_list[0]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = ! s1u;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), notu failed);


	/* reg <- ! reg */
        v_lambda("notul", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_notul(rdul, arg_list[0]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = ! s1ul;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), notul failed);


	/* reg <- ! reg */
        v_lambda("notl", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_notl(rdl, arg_list[0]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = ! s1l;
        vdemand(dl == ((long(*)(long))ip)(s1l), notl failed);


	/* reg <-   reg */
        v_lambda("movi", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_movi(rdi, arg_list[0]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di =   s1i;
        vdemand(di == ((int(*)(int))ip)(s1i), movi failed);


	/* reg <-   reg */
        v_lambda("movu", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_movu(rdu, arg_list[0]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du =   s1u;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), movu failed);


	/* reg <-   reg */
        v_lambda("movul", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_movul(rdul, arg_list[0]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul =   s1ul;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), movul failed);


	/* reg <-   reg */
        v_lambda("movl", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_movl(rdl, arg_list[0]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl =   s1l;
        vdemand(dl == ((long(*)(long))ip)(s1l), movl failed);


	/* reg <-   reg */
        v_lambda("movp", "%p", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdp, V_P, V_TEMP))
			v_fatal("out of registers!");

        	v_movp(rdp, arg_list[0]);
        	v_retp(rdp);
        ip = v_end(0).i;
        dp =   s1p;
        vdemand(dp == ((void *(*)(void *))ip)(s1p), movp failed);


	/* reg <- - reg */
        v_lambda("negi", "%i", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_negi(rdi, arg_list[0]);
        	v_reti(rdi);
        ip = v_end(0).i;
        di = - s1i;
        vdemand(di == ((int(*)(int))ip)(s1i), negi failed);


	/* reg <- - reg */
        v_lambda("negu", "%u", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_negu(rdu, arg_list[0]);
        	v_retu(rdu);
        ip = v_end(0).i;
        du = - s1u;
        vdemand(du == ((unsigned(*)(unsigned))ip)(s1u), negu failed);


	/* reg <- - reg */
        v_lambda("negul", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_negul(rdul, arg_list[0]);
        	v_retul(rdul);
        ip = v_end(0).i;
        dul = - s1ul;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)(s1ul), negul failed);


	/* reg <- - reg */
        v_lambda("negl", "%l", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdl, V_L, V_TEMP))
			v_fatal("out of registers!");

        	v_negl(rdl, arg_list[0]);
        	v_retl(rdl);
        ip = v_end(0).i;
        dl = - s1l;
        vdemand(dl == ((long(*)(long))ip)(s1l), negl failed);


	/* ret reg */
        v_lambda("reti", "%i", arg_list, V_LEAF, insn, sizeof insn);
        	v_reti(arg_list[0]);
        ip = v_end(0).i;
        vdemand(s1i == ((int(*)(int))ip)(s1i), reti failed);

	/* ret imm */
        v_lambda("retii", "", arg_list, V_LEAF, insn, sizeof insn);
        	v_retii(s1i);
        ip = v_end(0).i;
        vdemand(s1i == ((int(*)())ip)(), retii failed);


	/* ret reg */
        v_lambda("retu", "%u", arg_list, V_LEAF, insn, sizeof insn);
        	v_retu(arg_list[0]);
        ip = v_end(0).i;
        vdemand(s1u == ((unsigned(*)(unsigned))ip)(s1u), retu failed);

	/* ret imm */
        v_lambda("retui", "", arg_list, V_LEAF, insn, sizeof insn);
        	v_retui(s1u);
        ip = v_end(0).i;
        vdemand(s1u == ((unsigned(*)())ip)(), retui failed);


	/* ret reg */
        v_lambda("retul", "%ul", arg_list, V_LEAF, insn, sizeof insn);
        	v_retul(arg_list[0]);
        ip = v_end(0).i;
        vdemand(s1ul == ((unsigned long(*)(unsigned long))ip)(s1ul), retul failed);

	/* ret imm */
        v_lambda("retuli", "", arg_list, V_LEAF, insn, sizeof insn);
        	v_retuli(s1ul);
        ip = v_end(0).i;
        vdemand(s1ul == ((unsigned long(*)())ip)(), retuli failed);


	/* ret reg */
        v_lambda("retl", "%l", arg_list, V_LEAF, insn, sizeof insn);
        	v_retl(arg_list[0]);
        ip = v_end(0).i;
        vdemand(s1l == ((long(*)(long))ip)(s1l), retl failed);

	/* ret imm */
        v_lambda("retli", "", arg_list, V_LEAF, insn, sizeof insn);
        	v_retli(s1l);
        ip = v_end(0).i;
        vdemand(s1l == ((long(*)())ip)(), retli failed);


	/* ret reg */
        v_lambda("retp", "%p", arg_list, V_LEAF, insn, sizeof insn);
        	v_retp(arg_list[0]);
        ip = v_end(0).i;
        vdemand(s1p == ((void *(*)(void *))ip)(s1p), retp failed);

	/* ret imm */
        v_lambda("retpi", "", arg_list, V_LEAF, insn, sizeof insn);
        	v_retpi(s1p);
        ip = v_end(0).i;
        vdemand(s1p == ((void *(*)())ip)(), retpi failed);


	s2ul = (unsigned long)&dc - aligned_offset;

	/* mem [ reg + reg ] <- reg */
        v_lambda("stc", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdc, V_C, V_TEMP))
			v_fatal("out of registers!");

		v_setc(rdc, s2ul);
        	v_stc(rdc, arg_list[0], arg_list[1]);
        ip = v_end(0).i;
        ((void(*)(unsigned long, unsigned long))ip)(s2ul, aligned_offset);
        vdemand(dc == (char)(s2ul), stc failed);


	/* mem [ reg + reg ] <- reg */
	dc = 0;
        v_lambda("stci", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdc, V_C, V_TEMP))
			v_fatal("out of registers!");

		v_setc(rdc, s2ul);
        	v_stci(rdc, arg_list[0], aligned_offset);
        ip = v_end(0).i;
        ((void(*)(unsigned long))ip)(s2ul);

        vdemand(dc == (char)(s2ul), stci failed);

	s2ul = (unsigned long)&ds - aligned_offset;

	/* mem [ reg + reg ] <- reg */
        v_lambda("sts", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rds, V_S, V_TEMP))
			v_fatal("out of registers!");

		v_sets(rds, s2ul);
        	v_sts(rds, arg_list[0], arg_list[1]);
        ip = v_end(0).i;
        ((void(*)(unsigned long, unsigned long))ip)(s2ul, aligned_offset);
        vdemand(ds == (short)(s2ul), sts failed);


	/* mem [ reg + reg ] <- reg */
	ds = 0;
        v_lambda("stsi", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rds, V_S, V_TEMP))
			v_fatal("out of registers!");

		v_sets(rds, s2ul);
        	v_stsi(rds, arg_list[0], aligned_offset);
        ip = v_end(0).i;
        ((void(*)(unsigned long))ip)(s2ul);

        vdemand(ds == (short)(s2ul), stsi failed);

	s2ul = (unsigned long)&di - aligned_offset;

	/* mem [ reg + reg ] <- reg */
        v_lambda("sti", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

		v_seti(rdi, s2ul);
        	v_sti(rdi, arg_list[0], arg_list[1]);
        ip = v_end(0).i;
        ((void(*)(unsigned long, unsigned long))ip)(s2ul, aligned_offset);
        vdemand(di == (int)(s2ul), sti failed);


	/* mem [ reg + reg ] <- reg */
	di = 0;
        v_lambda("stii", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

		v_seti(rdi, s2ul);
        	v_stii(rdi, arg_list[0], aligned_offset);
        ip = v_end(0).i;
        ((void(*)(unsigned long))ip)(s2ul);

        vdemand(di == (int)(s2ul), stii failed);

	s2ul = (unsigned long)&du - aligned_offset;

	/* mem [ reg + reg ] <- reg */
        v_lambda("stu", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

		v_setu(rdu, s2ul);
        	v_stu(rdu, arg_list[0], arg_list[1]);
        ip = v_end(0).i;
        ((void(*)(unsigned long, unsigned long))ip)(s2ul, aligned_offset);
        vdemand(du == (unsigned)(s2ul), stu failed);


	/* mem [ reg + reg ] <- reg */
	du = 0;
        v_lambda("stui", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

		v_setu(rdu, s2ul);
        	v_stui(rdu, arg_list[0], aligned_offset);
        ip = v_end(0).i;
        ((void(*)(unsigned long))ip)(s2ul);

        vdemand(du == (unsigned)(s2ul), stui failed);

	s2ul = (unsigned long)&dul - aligned_offset;

	/* mem [ reg + reg ] <- reg */
        v_lambda("stul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

		v_setul(rdul, s2ul);
        	v_stul(rdul, arg_list[0], arg_list[1]);
        ip = v_end(0).i;
        ((void(*)(unsigned long, unsigned long))ip)(s2ul, aligned_offset);
        vdemand(dul == (unsigned long)(s2ul), stul failed);


	/* mem [ reg + reg ] <- reg */
	dul = 0;
        v_lambda("stuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

		v_setul(rdul, s2ul);
        	v_stuli(rdul, arg_list[0], aligned_offset);
        ip = v_end(0).i;
        ((void(*)(unsigned long))ip)(s2ul);

        vdemand(dul == (unsigned long)(s2ul), stuli failed);

	s2ul = (unsigned long)&dp - aligned_offset;

	/* mem [ reg + reg ] <- reg */
        v_lambda("stp", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdp, V_P, V_TEMP))
			v_fatal("out of registers!");

		v_setp(rdp, s2ul);
        	v_stp(rdp, arg_list[0], arg_list[1]);
        ip = v_end(0).i;
        ((void(*)(unsigned long, unsigned long))ip)(s2ul, aligned_offset);
        vdemand(dp == (void *)(s2ul), stp failed);


	/* mem [ reg + reg ] <- reg */
	dp = 0;
        v_lambda("stpi", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdp, V_P, V_TEMP))
			v_fatal("out of registers!");

		v_setp(rdp, s2ul);
        	v_stpi(rdp, arg_list[0], aligned_offset);
        ip = v_end(0).i;
        ((void(*)(unsigned long))ip)(s2ul);

        vdemand(dp == (void *)(s2ul), stpi failed);

	/* reg <- mem[reg + reg]  */
        v_lambda("ldc", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdc, V_C, V_TEMP))
			v_fatal("out of registers!");

        	v_ldc(rdc, arg_list[0], arg_list[1]);
        	v_reti(rdc);
        ip = v_end(0).i;
        vdemand(dc == ((char(*)(unsigned long, unsigned long))ip)((unsigned long)&dc - aligned_offset, aligned_offset), ldc failed);

	/* reg <- mem[reg + imm] */
        v_lambda("ldci", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdc, V_C, V_TEMP))
			v_fatal("out of registers!");

        	v_ldci(rdc, arg_list[0], aligned_offset);
        	v_reti(rdc);
        ip = v_end(0).i;
        vdemand(dc == ((char(*)(unsigned long))ip)((unsigned long)&dc - aligned_offset), ldci failed);


	/* reg <- mem[reg + reg]  */
        v_lambda("lds", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rds, V_S, V_TEMP))
			v_fatal("out of registers!");

        	v_lds(rds, arg_list[0], arg_list[1]);
        	v_reti(rds);
        ip = v_end(0).i;
        vdemand(ds == ((short(*)(unsigned long, unsigned long))ip)((unsigned long)&ds - aligned_offset, aligned_offset), lds failed);

	/* reg <- mem[reg + imm] */
        v_lambda("ldsi", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rds, V_S, V_TEMP))
			v_fatal("out of registers!");

        	v_ldsi(rds, arg_list[0], aligned_offset);
        	v_reti(rds);
        ip = v_end(0).i;
        vdemand(ds == ((short(*)(unsigned long))ip)((unsigned long)&ds - aligned_offset), ldsi failed);


	/* reg <- mem[reg + reg]  */
        v_lambda("ldi", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_ldi(rdi, arg_list[0], arg_list[1]);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long, unsigned long))ip)((unsigned long)&di - aligned_offset, aligned_offset), ldi failed);

	/* reg <- mem[reg + imm] */
        v_lambda("ldii", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdi, V_I, V_TEMP))
			v_fatal("out of registers!");

        	v_ldii(rdi, arg_list[0], aligned_offset);
        	v_reti(rdi);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long))ip)((unsigned long)&di - aligned_offset), ldii failed);


	/* reg <- mem[reg + reg]  */
        v_lambda("ldu", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_ldu(rdu, arg_list[0], arg_list[1]);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned long, unsigned long))ip)((unsigned long)&du - aligned_offset, aligned_offset), ldu failed);

	/* reg <- mem[reg + imm] */
        v_lambda("ldui", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdu, V_U, V_TEMP))
			v_fatal("out of registers!");

        	v_ldui(rdu, arg_list[0], aligned_offset);
        	v_retu(rdu);
        ip = v_end(0).i;
        vdemand(du == ((unsigned(*)(unsigned long))ip)((unsigned long)&du - aligned_offset), ldui failed);


	/* reg <- mem[reg + reg]  */
        v_lambda("ldul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_ldul(rdul, arg_list[0], arg_list[1]);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long, unsigned long))ip)((unsigned long)&dul - aligned_offset, aligned_offset), ldul failed);

	/* reg <- mem[reg + imm] */
        v_lambda("lduli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdul, V_UL, V_TEMP))
			v_fatal("out of registers!");

        	v_lduli(rdul, arg_list[0], aligned_offset);
        	v_retul(rdul);
        ip = v_end(0).i;
        vdemand(dul == ((unsigned long(*)(unsigned long))ip)((unsigned long)&dul - aligned_offset), lduli failed);


	/* reg <- mem[reg + reg]  */
        v_lambda("ldp", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdp, V_P, V_TEMP))
			v_fatal("out of registers!");

        	v_ldp(rdp, arg_list[0], arg_list[1]);
        	v_retp(rdp);
        ip = v_end(0).i;
        vdemand(dp == ((void *(*)(unsigned long, unsigned long))ip)((unsigned long)&dp - aligned_offset, aligned_offset), ldp failed);

	/* reg <- mem[reg + imm] */
        v_lambda("ldpi", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		if(!v_getreg(&rdp, V_P, V_TEMP))
			v_fatal("out of registers!");

        	v_ldpi(rdp, arg_list[0], aligned_offset);
        	v_retp(rdp);
        ip = v_end(0).i;
        vdemand(dp == ((void *(*)(unsigned long))ip)((unsigned long)&dp - aligned_offset), ldpi failed);


	/* reg <- (reg == reg) */
        v_lambda("beqi", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_beqi(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1i == s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), beqi failed);

	/* reg <- (reg == imm) */
        v_lambda("beqii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_beqii(arg_list[0], s2i, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), beqii failed);


	/* reg <- (reg == reg) */
        v_lambda("bequ", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bequ(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1u == s2u);
        vdemand(di == ((int(*)(unsigned,unsigned))ip)(s1u, s2u), bequ failed);

	/* reg <- (reg == imm) */
        v_lambda("bequi", "%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bequi(arg_list[0], s2u, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned))ip)(s1u), bequi failed);


	/* reg <- (reg == reg) */
        v_lambda("bequl", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bequl(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1ul == s2ul);
        vdemand(di == ((int(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), bequl failed);

	/* reg <- (reg == imm) */
        v_lambda("bequli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bequli(arg_list[0], s2ul, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long))ip)(s1ul), bequli failed);


	/* reg <- (reg == reg) */
        v_lambda("beql", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_beql(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1l == s2l);
        vdemand(di == ((int(*)(long,long))ip)(s1l, s2l), beql failed);

	/* reg <- (reg == imm) */
        v_lambda("beqli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_beqli(arg_list[0], s2l, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(long))ip)(s1l), beqli failed);


	/* reg <- (reg == reg) */
        v_lambda("beqp", "%p%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_beqp(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1p == s2p);
        vdemand(di == ((int(*)(void *,void *))ip)(s1p, s2p), beqp failed);

	/* reg <- (reg == imm) */
        v_lambda("beqpi", "%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_beqpi(arg_list[0], s2p, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(void *))ip)(s1p), beqpi failed);


	/* reg <- (reg != reg) */
        v_lambda("bnei", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bnei(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1i != s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), bnei failed);

	/* reg <- (reg != imm) */
        v_lambda("bneii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bneii(arg_list[0], s2i, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), bneii failed);


	/* reg <- (reg != reg) */
        v_lambda("bneu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bneu(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1u != s2u);
        vdemand(di == ((int(*)(unsigned,unsigned))ip)(s1u, s2u), bneu failed);

	/* reg <- (reg != imm) */
        v_lambda("bneui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bneui(arg_list[0], s2u, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned))ip)(s1u), bneui failed);


	/* reg <- (reg != reg) */
        v_lambda("bneul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bneul(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1ul != s2ul);
        vdemand(di == ((int(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), bneul failed);

	/* reg <- (reg != imm) */
        v_lambda("bneuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bneuli(arg_list[0], s2ul, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long))ip)(s1ul), bneuli failed);


	/* reg <- (reg != reg) */
        v_lambda("bnel", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bnel(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1l != s2l);
        vdemand(di == ((int(*)(long,long))ip)(s1l, s2l), bnel failed);

	/* reg <- (reg != imm) */
        v_lambda("bneli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bneli(arg_list[0], s2l, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(long))ip)(s1l), bneli failed);


	/* reg <- (reg != reg) */
        v_lambda("bnep", "%p%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bnep(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1p != s2p);
        vdemand(di == ((int(*)(void *,void *))ip)(s1p, s2p), bnep failed);

	/* reg <- (reg != imm) */
        v_lambda("bnepi", "%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bnepi(arg_list[0], s2p, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(void *))ip)(s1p), bnepi failed);


	/* reg <- (reg < reg) */
        v_lambda("blti", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_blti(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1i < s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), blti failed);

	/* reg <- (reg < imm) */
        v_lambda("bltii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltii(arg_list[0], s2i, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), bltii failed);


	/* reg <- (reg < reg) */
        v_lambda("bltu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltu(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1u < s2u);
        vdemand(di == ((int(*)(unsigned,unsigned))ip)(s1u, s2u), bltu failed);

	/* reg <- (reg < imm) */
        v_lambda("bltui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltui(arg_list[0], s2u, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned))ip)(s1u), bltui failed);


	/* reg <- (reg < reg) */
        v_lambda("bltul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltul(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1ul < s2ul);
        vdemand(di == ((int(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), bltul failed);

	/* reg <- (reg < imm) */
        v_lambda("bltuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltuli(arg_list[0], s2ul, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long))ip)(s1ul), bltuli failed);


	/* reg <- (reg < reg) */
        v_lambda("bltl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltl(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1l < s2l);
        vdemand(di == ((int(*)(long,long))ip)(s1l, s2l), bltl failed);

	/* reg <- (reg < imm) */
        v_lambda("bltli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltli(arg_list[0], s2l, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(long))ip)(s1l), bltli failed);


	/* reg <- (reg < reg) */
        v_lambda("bltp", "%p%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltp(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1p < s2p);
        vdemand(di == ((int(*)(void *,void *))ip)(s1p, s2p), bltp failed);

	/* reg <- (reg < imm) */
        v_lambda("bltpi", "%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bltpi(arg_list[0], s2p, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(void *))ip)(s1p), bltpi failed);


	/* reg <- (reg <= reg) */
        v_lambda("blei", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_blei(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1i <= s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), blei failed);

	/* reg <- (reg <= imm) */
        v_lambda("bleii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bleii(arg_list[0], s2i, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), bleii failed);


	/* reg <- (reg <= reg) */
        v_lambda("bleu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bleu(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1u <= s2u);
        vdemand(di == ((int(*)(unsigned,unsigned))ip)(s1u, s2u), bleu failed);

	/* reg <- (reg <= imm) */
        v_lambda("bleui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bleui(arg_list[0], s2u, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned))ip)(s1u), bleui failed);


	/* reg <- (reg <= reg) */
        v_lambda("bleul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bleul(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1ul <= s2ul);
        vdemand(di == ((int(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), bleul failed);

	/* reg <- (reg <= imm) */
        v_lambda("bleuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bleuli(arg_list[0], s2ul, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long))ip)(s1ul), bleuli failed);


	/* reg <- (reg <= reg) */
        v_lambda("blel", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_blel(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1l <= s2l);
        vdemand(di == ((int(*)(long,long))ip)(s1l, s2l), blel failed);

	/* reg <- (reg <= imm) */
        v_lambda("bleli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bleli(arg_list[0], s2l, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(long))ip)(s1l), bleli failed);


	/* reg <- (reg <= reg) */
        v_lambda("blep", "%p%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_blep(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1p <= s2p);
        vdemand(di == ((int(*)(void *,void *))ip)(s1p, s2p), blep failed);

	/* reg <- (reg <= imm) */
        v_lambda("blepi", "%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_blepi(arg_list[0], s2p, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(void *))ip)(s1p), blepi failed);


	/* reg <- (reg > reg) */
        v_lambda("bgti", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgti(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1i > s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), bgti failed);

	/* reg <- (reg > imm) */
        v_lambda("bgtii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtii(arg_list[0], s2i, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), bgtii failed);


	/* reg <- (reg > reg) */
        v_lambda("bgtu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtu(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1u > s2u);
        vdemand(di == ((int(*)(unsigned,unsigned))ip)(s1u, s2u), bgtu failed);

	/* reg <- (reg > imm) */
        v_lambda("bgtui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtui(arg_list[0], s2u, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned))ip)(s1u), bgtui failed);


	/* reg <- (reg > reg) */
        v_lambda("bgtul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtul(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1ul > s2ul);
        vdemand(di == ((int(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), bgtul failed);

	/* reg <- (reg > imm) */
        v_lambda("bgtuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtuli(arg_list[0], s2ul, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long))ip)(s1ul), bgtuli failed);


	/* reg <- (reg > reg) */
        v_lambda("bgtl", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtl(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1l > s2l);
        vdemand(di == ((int(*)(long,long))ip)(s1l, s2l), bgtl failed);

	/* reg <- (reg > imm) */
        v_lambda("bgtli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtli(arg_list[0], s2l, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(long))ip)(s1l), bgtli failed);


	/* reg <- (reg > reg) */
        v_lambda("bgtp", "%p%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtp(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1p > s2p);
        vdemand(di == ((int(*)(void *,void *))ip)(s1p, s2p), bgtp failed);

	/* reg <- (reg > imm) */
        v_lambda("bgtpi", "%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgtpi(arg_list[0], s2p, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(void *))ip)(s1p), bgtpi failed);


	/* reg <- (reg >= reg) */
        v_lambda("bgei", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgei(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1i >= s2i);
        vdemand(di == ((int(*)(int,int))ip)(s1i, s2i), bgei failed);

	/* reg <- (reg >= imm) */
        v_lambda("bgeii", "%i", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgeii(arg_list[0], s2i, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(int))ip)(s1i), bgeii failed);


	/* reg <- (reg >= reg) */
        v_lambda("bgeu", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgeu(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1u >= s2u);
        vdemand(di == ((int(*)(unsigned,unsigned))ip)(s1u, s2u), bgeu failed);

	/* reg <- (reg >= imm) */
        v_lambda("bgeui", "%u", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgeui(arg_list[0], s2u, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned))ip)(s1u), bgeui failed);


	/* reg <- (reg >= reg) */
        v_lambda("bgeul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgeul(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1ul >= s2ul);
        vdemand(di == ((int(*)(unsigned long,unsigned long))ip)(s1ul, s2ul), bgeul failed);

	/* reg <- (reg >= imm) */
        v_lambda("bgeuli", "%ul", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgeuli(arg_list[0], s2ul, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(unsigned long))ip)(s1ul), bgeuli failed);


	/* reg <- (reg >= reg) */
        v_lambda("bgel", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgel(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1l >= s2l);
        vdemand(di == ((int(*)(long,long))ip)(s1l, s2l), bgel failed);

	/* reg <- (reg >= imm) */
        v_lambda("bgeli", "%l", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgeli(arg_list[0], s2l, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(long))ip)(s1l), bgeli failed);


	/* reg <- (reg >= reg) */
        v_lambda("bgep", "%p%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgep(arg_list[0], arg_list[1], l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        di = (s1p >= s2p);
        vdemand(di == ((int(*)(void *,void *))ip)(s1p, s2p), bgep failed);

	/* reg <- (reg >= imm) */
        v_lambda("bgepi", "%p", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
        	v_bgepi(arg_list[0], s2p, l);
        		v_retii(0);
		v_label(l);
			v_retii(1);
        ip = v_end(0).i;
        vdemand(di == ((int(*)(void *))ip)(s1p), bgepi failed);


	/* ret reg */
        v_lambda("v_jv", "", arg_list, V_LEAF, insn, sizeof insn);
		l = v_genlabel();
		v_jv(l);
			v_retii(0);
		v_label(l);
        		v_retii(1);
        ip = v_end(0).i;
        vdemand(ip(), v_jv failed);

	/* ret imm */
        v_lambda("v_jp", "", arg_list, V_LEAF, insn, sizeof insn);
	{
		static v_code linked_addr;

		l = v_genlabel();
		v_dlabel(&linked_addr, l);
		if(!v_getreg(&rdp, V_P, V_TEMP))
			v_fatal("out of registers!");

		v_ldpi(rdp, v_zero, &linked_addr);

		v_jp(rdp);
			v_retii(0);
		v_label(l);
        		v_retii(1);
	}
        ip = v_end(0).i;
        vdemand(ip(), v_jp failed);


	v_lambda("param1i", "%i", arg_list, V_LEAF, insn, sizeof insn);
	v_reti(arg_list[1-1]);
	ip = v_end(0).i;
	vdemand(s2i == ((int(*)(int))ip)(s2i), param1i failed);

	v_lambda("call1i", "%i", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalli((v_iptr)ip, "%i", arg_list[0]);
	v_reti(reg);
	ip2 = v_end(0).i;
	vdemand2(s2i == ((int(*)(int))ip2)(s2i), call1i failed);

	v_lambda("param2i", "%i%i", arg_list, V_LEAF, insn, sizeof insn);
	v_reti(arg_list[2-1]);
	ip = v_end(0).i;
	vdemand(s2i == ((int(*)(int,int))ip)(s1i,s2i), param2i failed);

	v_lambda("call2i", "%i%i", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalli((v_iptr)ip, "%i%i", arg_list[0],arg_list[1]);
	v_reti(reg);
	ip2 = v_end(0).i;
	vdemand2(s2i == ((int(*)(int,int))ip2)(s1i,s2i), call2i failed);

	v_lambda("param3i", "%i%i%i", arg_list, V_LEAF, insn, sizeof insn);
	v_reti(arg_list[3-1]);
	ip = v_end(0).i;
	vdemand(s2i == ((int(*)(int,int,int))ip)(s1i,s1i,s2i), param3i failed);

	v_lambda("call3i", "%i%i%i", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalli((v_iptr)ip, "%i%i%i", arg_list[0],arg_list[1],arg_list[2]);
	v_reti(reg);
	ip2 = v_end(0).i;
	vdemand2(s2i == ((int(*)(int,int,int))ip2)(s1i,s1i,s2i), call3i failed);

	v_lambda("param4i", "%i%i%i%i", arg_list, V_LEAF, insn, sizeof insn);
	v_reti(arg_list[4-1]);
	ip = v_end(0).i;
	vdemand(s2i == ((int(*)(int,int,int,int))ip)(s1i,s1i,s1i,s2i), param4i failed);

	v_lambda("call4i", "%i%i%i%i", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalli((v_iptr)ip, "%i%i%i%i", arg_list[0],arg_list[1],arg_list[2],arg_list[3]);
	v_reti(reg);
	ip2 = v_end(0).i;
	vdemand2(s2i == ((int(*)(int,int,int,int))ip2)(s1i,s1i,s1i,s2i), call4i failed);

	v_lambda("param5i", "%i%i%i%i%i", arg_list, V_LEAF, insn, sizeof insn);
	v_reti(arg_list[5-1]);
	ip = v_end(0).i;
	vdemand(s2i == ((int(*)(int,int,int,int,int))ip)(s1i,s1i,s1i,s1i,s2i), param5i failed);

	v_lambda("call5i", "%i%i%i%i%i", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalli((v_iptr)ip, "%i%i%i%i%i", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4]);
	v_reti(reg);
	ip2 = v_end(0).i;
	vdemand2(s2i == ((int(*)(int,int,int,int,int))ip2)(s1i,s1i,s1i,s1i,s2i), call5i failed);

	v_lambda("param6i", "%i%i%i%i%i%i", arg_list, V_LEAF, insn, sizeof insn);
	v_reti(arg_list[6-1]);
	ip = v_end(0).i;
	vdemand(s2i == ((int(*)(int,int,int,int,int,int))ip)(s1i,s1i,s1i,s1i,s1i,s2i), param6i failed);

	v_lambda("call6i", "%i%i%i%i%i%i", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalli((v_iptr)ip, "%i%i%i%i%i%i", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4],arg_list[5]);
	v_reti(reg);
	ip2 = v_end(0).i;
	vdemand2(s2i == ((int(*)(int,int,int,int,int,int))ip2)(s1i,s1i,s1i,s1i,s1i,s2i), call6i failed);

	v_lambda("param1u", "%u", arg_list, V_LEAF, insn, sizeof insn);
	v_retu(arg_list[1-1]);
	ip = v_end(0).i;
	vdemand(s2u == ((unsigned(*)(unsigned))ip)(s2u), param1u failed);

	v_lambda("call1u", "%u", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallu((v_uptr)ip, "%u", arg_list[0]);
	v_retu(reg);
	ip2 = v_end(0).i;
	vdemand2(s2u == ((unsigned(*)(unsigned))ip2)(s2u), call1u failed);

	v_lambda("param2u", "%u%u", arg_list, V_LEAF, insn, sizeof insn);
	v_retu(arg_list[2-1]);
	ip = v_end(0).i;
	vdemand(s2u == ((unsigned(*)(unsigned,unsigned))ip)(s1u,s2u), param2u failed);

	v_lambda("call2u", "%u%u", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallu((v_uptr)ip, "%u%u", arg_list[0],arg_list[1]);
	v_retu(reg);
	ip2 = v_end(0).i;
	vdemand2(s2u == ((unsigned(*)(unsigned,unsigned))ip2)(s1u,s2u), call2u failed);

	v_lambda("param3u", "%u%u%u", arg_list, V_LEAF, insn, sizeof insn);
	v_retu(arg_list[3-1]);
	ip = v_end(0).i;
	vdemand(s2u == ((unsigned(*)(unsigned,unsigned,unsigned))ip)(s1u,s1u,s2u), param3u failed);

	v_lambda("call3u", "%u%u%u", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallu((v_uptr)ip, "%u%u%u", arg_list[0],arg_list[1],arg_list[2]);
	v_retu(reg);
	ip2 = v_end(0).i;
	vdemand2(s2u == ((unsigned(*)(unsigned,unsigned,unsigned))ip2)(s1u,s1u,s2u), call3u failed);

	v_lambda("param4u", "%u%u%u%u", arg_list, V_LEAF, insn, sizeof insn);
	v_retu(arg_list[4-1]);
	ip = v_end(0).i;
	vdemand(s2u == ((unsigned(*)(unsigned,unsigned,unsigned,unsigned))ip)(s1u,s1u,s1u,s2u), param4u failed);

	v_lambda("call4u", "%u%u%u%u", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallu((v_uptr)ip, "%u%u%u%u", arg_list[0],arg_list[1],arg_list[2],arg_list[3]);
	v_retu(reg);
	ip2 = v_end(0).i;
	vdemand2(s2u == ((unsigned(*)(unsigned,unsigned,unsigned,unsigned))ip2)(s1u,s1u,s1u,s2u), call4u failed);

	v_lambda("param5u", "%u%u%u%u%u", arg_list, V_LEAF, insn, sizeof insn);
	v_retu(arg_list[5-1]);
	ip = v_end(0).i;
	vdemand(s2u == ((unsigned(*)(unsigned,unsigned,unsigned,unsigned,unsigned))ip)(s1u,s1u,s1u,s1u,s2u), param5u failed);

	v_lambda("call5u", "%u%u%u%u%u", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallu((v_uptr)ip, "%u%u%u%u%u", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4]);
	v_retu(reg);
	ip2 = v_end(0).i;
	vdemand2(s2u == ((unsigned(*)(unsigned,unsigned,unsigned,unsigned,unsigned))ip2)(s1u,s1u,s1u,s1u,s2u), call5u failed);

	v_lambda("param6u", "%u%u%u%u%u%u", arg_list, V_LEAF, insn, sizeof insn);
	v_retu(arg_list[6-1]);
	ip = v_end(0).i;
	vdemand(s2u == ((unsigned(*)(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned))ip)(s1u,s1u,s1u,s1u,s1u,s2u), param6u failed);

	v_lambda("call6u", "%u%u%u%u%u%u", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallu((v_uptr)ip, "%u%u%u%u%u%u", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4],arg_list[5]);
	v_retu(reg);
	ip2 = v_end(0).i;
	vdemand2(s2u == ((unsigned(*)(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned))ip2)(s1u,s1u,s1u,s1u,s1u,s2u), call6u failed);

	v_lambda("param1l", "%l", arg_list, V_LEAF, insn, sizeof insn);
	v_retl(arg_list[1-1]);
	ip = v_end(0).i;
	vdemand(s2l == ((long(*)(long))ip)(s2l), param1l failed);

	v_lambda("call1l", "%l", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalll((v_lptr)ip, "%l", arg_list[0]);
	v_retl(reg);
	ip2 = v_end(0).i;
	vdemand2(s2l == ((long(*)(long))ip2)(s2l), call1l failed);

	v_lambda("param2l", "%l%l", arg_list, V_LEAF, insn, sizeof insn);
	v_retl(arg_list[2-1]);
	ip = v_end(0).i;
	vdemand(s2l == ((long(*)(long,long))ip)(s1l,s2l), param2l failed);

	v_lambda("call2l", "%l%l", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalll((v_lptr)ip, "%l%l", arg_list[0],arg_list[1]);
	v_retl(reg);
	ip2 = v_end(0).i;
	vdemand2(s2l == ((long(*)(long,long))ip2)(s1l,s2l), call2l failed);

	v_lambda("param3l", "%l%l%l", arg_list, V_LEAF, insn, sizeof insn);
	v_retl(arg_list[3-1]);
	ip = v_end(0).i;
	vdemand(s2l == ((long(*)(long,long,long))ip)(s1l,s1l,s2l), param3l failed);

	v_lambda("call3l", "%l%l%l", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalll((v_lptr)ip, "%l%l%l", arg_list[0],arg_list[1],arg_list[2]);
	v_retl(reg);
	ip2 = v_end(0).i;
	vdemand2(s2l == ((long(*)(long,long,long))ip2)(s1l,s1l,s2l), call3l failed);

	v_lambda("param4l", "%l%l%l%l", arg_list, V_LEAF, insn, sizeof insn);
	v_retl(arg_list[4-1]);
	ip = v_end(0).i;
	vdemand(s2l == ((long(*)(long,long,long,long))ip)(s1l,s1l,s1l,s2l), param4l failed);

	v_lambda("call4l", "%l%l%l%l", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalll((v_lptr)ip, "%l%l%l%l", arg_list[0],arg_list[1],arg_list[2],arg_list[3]);
	v_retl(reg);
	ip2 = v_end(0).i;
	vdemand2(s2l == ((long(*)(long,long,long,long))ip2)(s1l,s1l,s1l,s2l), call4l failed);

	v_lambda("param5l", "%l%l%l%l%l", arg_list, V_LEAF, insn, sizeof insn);
	v_retl(arg_list[5-1]);
	ip = v_end(0).i;
	vdemand(s2l == ((long(*)(long,long,long,long,long))ip)(s1l,s1l,s1l,s1l,s2l), param5l failed);

	v_lambda("call5l", "%l%l%l%l%l", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalll((v_lptr)ip, "%l%l%l%l%l", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4]);
	v_retl(reg);
	ip2 = v_end(0).i;
	vdemand2(s2l == ((long(*)(long,long,long,long,long))ip2)(s1l,s1l,s1l,s1l,s2l), call5l failed);

	v_lambda("param6l", "%l%l%l%l%l%l", arg_list, V_LEAF, insn, sizeof insn);
	v_retl(arg_list[6-1]);
	ip = v_end(0).i;
	vdemand(s2l == ((long(*)(long,long,long,long,long,long))ip)(s1l,s1l,s1l,s1l,s1l,s2l), param6l failed);

	v_lambda("call6l", "%l%l%l%l%l%l", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scalll((v_lptr)ip, "%l%l%l%l%l%l", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4],arg_list[5]);
	v_retl(reg);
	ip2 = v_end(0).i;
	vdemand2(s2l == ((long(*)(long,long,long,long,long,long))ip2)(s1l,s1l,s1l,s1l,s1l,s2l), call6l failed);

	v_lambda("param1ul", "%ul", arg_list, V_LEAF, insn, sizeof insn);
	v_retul(arg_list[1-1]);
	ip = v_end(0).i;
	vdemand(s2ul == ((unsigned long(*)(unsigned long))ip)(s2ul), param1ul failed);

	v_lambda("call1ul", "%ul", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallul((v_ulptr)ip, "%ul", arg_list[0]);
	v_retul(reg);
	ip2 = v_end(0).i;
	vdemand2(s2ul == ((unsigned long(*)(unsigned long))ip2)(s2ul), call1ul failed);

	v_lambda("param2ul", "%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
	v_retul(arg_list[2-1]);
	ip = v_end(0).i;
	vdemand(s2ul == ((unsigned long(*)(unsigned long,unsigned long))ip)(s1ul,s2ul), param2ul failed);

	v_lambda("call2ul", "%ul%ul", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallul((v_ulptr)ip, "%ul%ul", arg_list[0],arg_list[1]);
	v_retul(reg);
	ip2 = v_end(0).i;
	vdemand2(s2ul == ((unsigned long(*)(unsigned long,unsigned long))ip2)(s1ul,s2ul), call2ul failed);

	v_lambda("param3ul", "%ul%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
	v_retul(arg_list[3-1]);
	ip = v_end(0).i;
	vdemand(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long))ip)(s1ul,s1ul,s2ul), param3ul failed);

	v_lambda("call3ul", "%ul%ul%ul", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallul((v_ulptr)ip, "%ul%ul%ul", arg_list[0],arg_list[1],arg_list[2]);
	v_retul(reg);
	ip2 = v_end(0).i;
	vdemand2(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long))ip2)(s1ul,s1ul,s2ul), call3ul failed);

	v_lambda("param4ul", "%ul%ul%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
	v_retul(arg_list[4-1]);
	ip = v_end(0).i;
	vdemand(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long,unsigned long))ip)(s1ul,s1ul,s1ul,s2ul), param4ul failed);

	v_lambda("call4ul", "%ul%ul%ul%ul", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallul((v_ulptr)ip, "%ul%ul%ul%ul", arg_list[0],arg_list[1],arg_list[2],arg_list[3]);
	v_retul(reg);
	ip2 = v_end(0).i;
	vdemand2(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long,unsigned long))ip2)(s1ul,s1ul,s1ul,s2ul), call4ul failed);

	v_lambda("param5ul", "%ul%ul%ul%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
	v_retul(arg_list[5-1]);
	ip = v_end(0).i;
	vdemand(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long,unsigned long,unsigned long))ip)(s1ul,s1ul,s1ul,s1ul,s2ul), param5ul failed);

	v_lambda("call5ul", "%ul%ul%ul%ul%ul", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallul((v_ulptr)ip, "%ul%ul%ul%ul%ul", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4]);
	v_retul(reg);
	ip2 = v_end(0).i;
	vdemand2(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long,unsigned long,unsigned long))ip2)(s1ul,s1ul,s1ul,s1ul,s2ul), call5ul failed);

	v_lambda("param6ul", "%ul%ul%ul%ul%ul%ul", arg_list, V_LEAF, insn, sizeof insn);
	v_retul(arg_list[6-1]);
	ip = v_end(0).i;
	vdemand(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long,unsigned long,unsigned long,unsigned long))ip)(s1ul,s1ul,s1ul,s1ul,s1ul,s2ul), param6ul failed);

	v_lambda("call6ul", "%ul%ul%ul%ul%ul%ul", arg_list, V_NLEAF, insn2, sizeof insn2);
	reg = v_scallul((v_ulptr)ip, "%ul%ul%ul%ul%ul%ul", arg_list[0],arg_list[1],arg_list[2],arg_list[3],arg_list[4],arg_list[5]);
	v_retul(reg);
	ip2 = v_end(0).i;
	vdemand2(s2ul == ((unsigned long(*)(unsigned long,unsigned long,unsigned long,unsigned long,unsigned long,unsigned long))ip2)(s1ul,s1ul,s1ul,s1ul,s1ul,s2ul), call6ul failed);


	if(!v_errors && iters-- > 0) goto loop;

	if(!v_errors) {
		printf("No errors!
");
		return 0;
	}

	printf("*** %d Errors! ****
", v_errors);
	printf("s1i %d, s2i %d, s1u %x, s2u %x\n", s1i,s2i,s1u,s2u);
	printf("s1ul %lu, s2ul %lu, s1l %ld, s2l %ld\n", 
						s1ul,s2ul,s1l,s2l);
	printf("s1f = %f, s2f = %f, s1d = %f, s2d = %f\n",
						s1f,s2f,s1d,s2d);
	printf(" aligned offset = %d, unaligned offset %d\n", 
		aligned_offset, unaligned_offset);
	printf("shifti = %d, shiftu = %d, shiftl = %d, shiftul = %d\n",
		shifti, shiftu, shiftl, shiftul);	
	return 1;
}

