
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

#ifndef _XOK_SCODE_H_
#define _XOK_SCODE_H_

#include <vcode/vcode.h>

/* Representation of a single instruction. This is not the most space efficient
   arrangement, but it is easy to parse. */

struct Sinsn {
  unsigned char s_op;		/* type of instruction */
#define S_NONE 0
#define S_IMM 1
#define S_LAB 2
#define S_REG 3
  unsigned char s_type[3];	/* type of each arg (imm, reg, or label) */
#define S_WORD 0
#define S_CHAR 1
  unsigned char s_size : 1;	/* size of arg being referenced */
#define S_SIGNED 0
#define S_UNSIGNED 1
  unsigned char s_sign : 1;	/* signed arithmetic or not */
  unsigned int s_arg[3];	/* immediate, label index, register index */
};

/* errors */
#define E_INV_LAB -1
#define E_INV_REG -2

extern int s_regs[];
extern int s_labels[];

#define S_ADD 1
#define s_addu(b,rd,r1,r2) { \
  (b)->s_op = S_ADD; \
  (b)->s_type[0] = (b)->s_type[1] = (b)->s_type[2] = S_REG; \
  (b)->s_arg[0] = rd; \
  (b)->s_arg[1] = r1; \
  (b)->s_arg[2] = r2; \
  (b)->s_size = S_WORD; \
  (b)->s_sign = S_UNSIGNED; \
}

#define S_LD 2
#define s_ldu(b,rd,rs,offset) { \
  (b)->s_op = S_LD; \
  (b)->s_type[0] = (b)->s_type[1] = (b)->s_type[2] = S_REG; \
  (b)->s_arg[0] = rd; \
  (b)->s_arg[1] = rs; \
  (b)->s_arg[2] = offset; \
  (b)->s_size = S_WORD; \
  (b)->s_sign = S_UNSIGNED; \
}

#define s_ldui(b,rd,rs,imm) { \
  (b)->s_op = S_LD; \
  (b)->s_type[0] = (b)->s_type[1] = S_REG; \
  (b)->s_type[2] = S_IMM; \
  (b)->s_arg[0] = rd; \
  (b)->s_arg[1] = rs; \
  (b)->s_arg[2] = imm; \
  (b)->s_size = S_WORD; \
  (b)->s_sign = S_UNSIGNED; \
}

#define S_ST 3
#define s_stu(b,rs,rb,offset) { \
  (b)->s_op = S_ST; \
  (b)->s_type[0] = (b)->s_type[1] = (b)->s_type[2] = S_REG; \
  (b)->s_arg[0] = rs; \
  (b)->s_arg[1] = rb; \
  (b)->s_arg[2] = offset; \
  (b)->s_size = S_WORD; \
  (b)->s_sign = S_UNSIGNED; \
}

#define s_stui(b,rs,rb,imm) { \
  (b)->s_op = S_ST; \
  (b)->s_type[0] = (b)->s_type[1] = S_REG; \
  (b)->s_type[2] = S_IMM; \
  (b)->s_arg[0] = rs; \
  (b)->s_arg[1] = rb; \
  (b)->s_arg[2] = imm; \
  (b)->s_size = S_WORD; \
  (b)->s_sign = S_UNSIGNED; \
}

#define LAST_THREE_OP S_ST

#define S_LABEL (LAST_THREE_OP+2)
#define s_label(b,l) { \
 (b)->s_op = S_LABEL; \
 (b)->s_type[0] = S_LAB; \
 (b)->s_arg[0] = l; \
 (b)->s_type[1] = (b)->s_type[2] = S_NONE; \
}

#define S_FIRST_CTRANSFER 32
#define S_LAST_CTRANSFER 33
#define S_IS_CTRANSFER(i) ((i)->s_op >= S_FIRST_CTRANSFER  && (i)->s_op <= S_LAST_CTRANSFER)

#define S_GET_LABEL(i) (S_IS_BRANCH(i) ? S_BRANCH_GETLABEL(i) : S_JUMP_GETLABEL(i))

#define S_IS_BRANCH(i) ((i)->s_op == S_BLT)
#define S_BRANCH_GETLABEL(i) (s_labels[(i)->s_arg[2]])
#define S_BLT S_FIRST_CTRANSFER
#define s_blt(b,r1,r2,l) { \
  (b)->s_op = S_BLT; \
  (b)->s_type[0] = (b)->s_type[1] = S_REG; \
  (b)->s_type[2] = S_LAB; \
  (b)->s_arg[0] = r1; \
  (b)->s_arg[1] = r2; \
  (b)->s_arg[2] = l; \
}

#define S_IS_JUMP(i) ((i)->s_op == S_JMP)
#define S_JUMP_GETLABEL(i) (s_labels[(i)->s_arg[0]])
#define S_JMP (S_FIRST_CTRANSFER+1)
#define s_jmpv(b,l) { \
  (b)->s_op = S_JMP; \
  (b)->s_type[0] = S_LAB; \
  (b)->s_arg[0] = l; \
  (b)->s_type[1] = (b)->s_type[2] = S_NONE; \
}

#define S_SET (LAST_THREE_OP+4)
#define s_setu(b,r1,i) { \
  (b)->s_op = S_SET; \
  (b)->s_type[0] = S_REG; \
  (b)->s_type[1] = S_IMM; \
  (b)->s_type[2] = S_NONE; \
  (b)->s_arg[0] = r1; \
  (b)->s_arg[1] = i; \
}

/* following are pseudo-instructions implemented by the scheduler */

#define S_SUBSCHED 128
#define s_subsched(b,r1,r2) { \
  (b)->s_op = S_SUBSCHED; \
  (b)->s_type[0] = (b)->s_type[1] = S_REG; \
  (b)->s_type[2] = S_NONE; \
  (b)->s_arg[0] = r1; \
  (b)->s_arg[1] = r2; \
}

#define S_PROCSCHED 129
#define s_procsched(b,r1) { \
  (b)->s_op = S_PROCSCHED; \
  (b)->s_type[0] = S_REG; \
  (b)->s_type[1] = (b)->s_type[2] = S_NONE; \
  (b)->s_arg[0] = r1; \
}

#define S_REJECT 130
#define s_reject(b) { \
  (b)->s_op = S_REJECT; \
  (b)->s_type[0] = (b)->s_type[1] = (b)->s_type[2] = S_NONE; \
}

void s_setlabel (int sl, v_label_t vl);
void s_setreg (int sreg, v_reg_t vreg);
void s_lambda (unsigned char *ip, int nbytes);
union v_fp s_end ();
int s_gen (struct Sinsn *i);

#endif
