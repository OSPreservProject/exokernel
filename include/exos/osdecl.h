
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

#ifndef __OSDECL_H__
#define __OSDECL_H__

#include <sys/types.h>

/* From xasm.S */
extern void xue_epilogue (void);
extern void xue_prologue (void);
extern void xue_fault (void);
extern void xue_ipc1 (void);
extern void entrtc (void);
extern char edata;
extern char end;

/* from xrt0.c */
extern u_int __envid;
extern struct Env * __curenv;
extern u_int __brkpt;
extern u_int __stkbot;
extern u_int __xstktop;
void die (void) __attribute__ ((noreturn));

/* Helps with the moving ASH area */
#define ADD_UVA(x) ((typeof(x))(((u_int)(x)) + ((u_int)__curenv->env_ashuva)))
#define SUB_UVA(x) ((typeof(x))(((u_int)(x)) - ((u_int)__curenv->env_ashuva)))

static inline int min (int x, int y) {
  return (x < y ? x : y);
}

static inline int max (int x, int y) {
  return (x < y ? y : x);
}

#endif /* __OSDECL_H__ */
