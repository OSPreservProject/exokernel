
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

#ifndef __FPU_H__
#define __FPU_H__

#include <xok/env.h>


/* I have no idea if these definitions are correct. I know that the
   size of Fpu_state is correct for storing the result of a FSAVE
   instruction.  I'm not sure about the actual fields though,
   specifically the word (word 4) containg fpu_op and and fpu_z0 since
   figure 6-6 in the Intel Pent. Dev. Man. vol 3 seems to contain 1
   too few bits in that word. Or maybe I just can't count. Tom */

#ifndef __ASSEMBLER__
#include <exos/kprintf.h>
#include <assert.h>

struct Fpu_env {
  unsigned fpu_cw : 16;		/* control word */
  unsigned fpu_r0 : 16;		/* reserved */
  unsigned fpu_sw : 16;		/* status word */
  unsigned fpu_r1 : 16;		/* reserved */
  unsigned fpu_tw : 16;		/* tag word */
  unsigned fpu_r2 : 16;		/* reserved */
  unsigned fpu_ip;		/* ip offset */
  unsigned fpu_cs : 16;		/* cs selector */
  unsigned fpu_op : 11;		/* opcode */
  unsigned fpu_z0 : 5;		/* must be zero */
  unsigned fpu_do;		/* data operand offset */
  unsigned fpu_os: 16;		/* operand selector */
  unsigned fpu_r4: 16;		/* reserved */
};

typedef unsigned char Fpu_reg[10];
typedef Fpu_reg Fpu_st[8];

struct Fpu_state {
  struct Fpu_env fpu_env;
  Fpu_st fpu_st;
};

extern struct Fpu_state _exos_fpus;
extern void _exos_fpu_init();
extern u_int _exos_fpu_used_ctxt;

#endif /* !__ASSEMBLER__ */

/* sizeof (struct Fpu_state) */
#define FPU_SAVE_SZ 108

#endif /* __FPU_H__ */
