
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


#ifndef _XOK_TRAP_H_
#define _XOK_TRAP_H_

#define KERNSRV_OFFSET 0x30 /* KERNEL service offset */
#define IPITRAP_OFFSET 0x50 /* KERNEL service offset */
#define MAX_IPIS       16

/* 1 + Highest numbered syscall.  Must be a power of 2! */
#define MAX_SYSCALL 0x100

#ifndef __ASSEMBLER__

#include <xok/types.h>

#ifdef KERNEL

#include <xok/mmu.h>
#include <xok/cpu.h>

struct Syscall {
  void *sc_fn;
  int sc_nargs;
};

extern struct Syscall sctab[];
extern struct gate_desc idt[];

#endif /* KERNEL */

#define BASIC_TRAP_FRAME \
  u_int tf_edi;\
  u_int tf_esi;\
  u_int tf_ebp;\
  u_int tf_oesp;                      /* Useless */\
  u_int tf_ebx;\
  u_int tf_edx;\
  u_int tf_ecx;\
  u_int tf_eax;\
  u_short tf_es;\
  u_int : 0;\
  u_short tf_ds;\
  u_int : 0;\
  u_int tf_eip;\
  u_short tf_cs;\
  u_int : 0;\
  u_int tf_eflags;\
  u_int tf_esp;\
  u_short tf_ss;\
  u_int : 0;\
  char _tf_hi[0];

#define tf_lo tf_edi                  /* Bottom of trap frame */
#define tf_hi _tf_hi[0]               /* Top of trap frame */

typedef struct Trapframe {
    BASIC_TRAP_FRAME
} *tfp;

#if __HOST__
typedef struct Trapframe_vm86 {
    BASIC_TRAP_FRAME
    u_short  tf_es86;       /* pushed by VM86 */
    u_short _tf_es86;
    u_short  tf_ds86;       /* pushed by VM86 */
    u_short _tf_ds86;
    u_short  tf_fs;         /* pushed by VM86; used in monitor for VM86/prot */
    u_short _tf_fs;
    u_short  tf_gs;         /* pushed by VM86; used in monitor for VM86/prot */
    u_short _tf_gs;
} *tfp_vm86;
#endif



/* The number of bytes in the partial trapframe above and including
 * field <lo> up to but not including field <hi>
 */
#define tf_size(lo, hi) ((u_int) &((tfp) 0)->hi - (u_int) &((tfp) 0)->lo)

/* Set trapframe pointer <tf> to the current trap frame (which
 * contains all fields of the Trapframe structure above <lo).  <last>
 * is the last argument passed to the trap handler (either the trap
 * number, or the error code if the trap takes an error code.
 */
#define tfp_set(tf, last, lo)						\
	tf = (tfp) ((u_int) &last + 4 - tf_size (tf_lo, lo))

#ifdef KERNEL
extern int get_cpu_id();
/* The user trap frame is always at the top of the kernel stack */
#define utf ((tfp) (KSTACKTOP-sizeof(struct Trapframe)))
#endif /* KERNEL */

#endif /* !__ASSEMBLER__ */

#endif /* _XOK_TRAP_H_ */
