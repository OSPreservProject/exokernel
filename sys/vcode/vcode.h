
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

#ifndef __VCODE_H__
#define __VCODE_H__

/* Generic definitions, types, etc ... */

#include <xok_include/stdarg.h>

typedef char v_code;	/* base instruction type */
#if 0
register v_code *v_ip asm ("ebx");
#else
extern v_code *v_ip;
#endif

/* 
 * Each possible function pointer type (ignoring parameters).
 * NOTE: to prevent C's default argument promotion these
 * must be cast to having the correct parameter types (e.g.,
 * char, short, float).
 */

typedef void (*v_vptr)();		/* V */
typedef char (*v_cptr)();		/* C */
typedef unsigned char (*v_ucptr)();	/* UC */
typedef short (*v_sptr)();		/* S */
typedef unsigned short (*v_usptr)();	/* US */
typedef int (*v_iptr)();		/* I */
typedef unsigned (*v_uptr)();		/* U */
typedef long	(*v_lptr)();		/* L */
typedef unsigned long (*v_ulptr)();	/* UL */
typedef void *(*v_pptr)();		/* P */
typedef float (*v_fptr)();		/* F */
typedef double (*v_dptr)();		/* D */

union v_fp {	
	v_vptr	v;
	v_ucptr uc;
	v_cptr	c;
	v_usptr	us;
	v_sptr	s;
	v_iptr	i;
	v_uptr	u;
	v_lptr	l;
	v_ulptr	ul;
	v_pptr	p;
	v_fptr	f;
	v_dptr	d;
};

/* registers are represented by an integer. Positive integers are
   physical register names and negative values represent virtual
   registers which will be stored on the stack. In the case of virtual
   registers, the virtual name (i.e., the negative number), when
   negated, is the offset from EBP where the value is actually
   stored. In particular, we arrange for the first virtual register to
   be the return EIP that CALL instructions push. This allows us to
   make the vcode register RA map to the EIP on the stack quite
   easily. */

typedef int v_reg_t;

/* some pseudo registers */
extern v_reg_t v_ra;
#define v_lp __EBP
#define v_zero 0xf0f0

/* classes of registers. V_TEMP are not saved across procedure calls
   while V_VAR are. */

#define V_TEMP 0
#define V_VAR 1

/* types of functions */

#define V_LEAF 1
#define V_NLEAF 0

/* basic types */

#define V_I 1
#define V_U 2
#define V_L 3
#define V_UL 4
#define V_P 5
#define V_C 6
#define V_UC 7
#define V_S 8
#define V_US 9

int v_getreg (v_reg_t *r,	/* reg struct we fill in with new reg */
	      int type,		/* type of value to be stored in r */
	      int class);	/* temp or perst value */
int v_putreg (v_reg_t r,	/* register to free */
	      int type);	/* type of r */
int v_local (int type);  	/* type of value to alloc space for */
int v_localb (unsigned size);	/* number of bytes needed */

/* labels are just indiices into a table of pointers that mark
   the actual location of the label. */

typedef int v_label_t;

#define MAX_ARGS 32
struct v_cstate {
  int next_args;
  enum type { IMMEDIATE, REGISTER } type[MAX_ARGS];
  int val[MAX_ARGS];
};

v_label_t v_genlabel (void);
void v_label (v_label_t l);
void v_dlabel (void *addr, v_label_t l);
void v_dmark (v_code *addr, v_label_t l);
v_code *v_getlabel (v_label_t l);

/*   XXX -- no chars or shorts yet. */

v_reg_t scall (v_vptr ptr, char *fmt, va_list ap);
v_reg_t ccall (struct v_cstate *cstate, v_vptr ptr);

static void v_scallv(v_vptr ptr, char *fmt, ...) {
  va_list ap;
  v_reg_t ret;

  va_start(ap, fmt);
  ret = scall((v_vptr)ptr, fmt, ap);
  /* dummy ret since function doesn't return a value. This is kind
     of broken. */
  v_putreg (ret, V_U);
  va_end(ap);
}

static v_reg_t v_scalli(v_iptr ptr, char *fmt, ...) {
  va_list ap;
  v_reg_t ret;
  
  va_start(ap, fmt);
  ret = scall((v_vptr)ptr, fmt, ap);
  va_end(ap);
  return ret;
}

static v_reg_t v_scallu(v_uptr ptr, char *fmt, ...) {
  va_list ap;
  v_reg_t ret;

  va_start(ap, fmt);
  ret = scall((v_vptr)ptr, fmt, ap);
  va_end(ap);
  return ret;
}

static v_reg_t v_scallp(v_pptr ptr, char *fmt, ...) {
  va_list ap;
  v_reg_t ret;

  va_start(ap, fmt);
  ret = scall((v_vptr)ptr, fmt, ap);
  va_end(ap);
  return ret;
}

static v_reg_t v_scallul(v_ulptr ptr, char *fmt, ...) {
  va_list ap;
  v_reg_t ret;

  va_start(ap, fmt);
  ret = scall((v_vptr)ptr, fmt, ap);
  va_end(ap);
  return ret;
}

static v_reg_t v_scalll(v_lptr ptr, char *fmt, ...) {
  va_list ap;
  v_reg_t ret;

  va_start(ap, fmt);
  ret = scall((v_vptr)ptr, fmt, ap);
  va_end(ap);
  return ret;
}

/*   XXX -- no char or short's yet. */

static inline void v_ccallv(struct v_cstate *c, v_vptr ptr) { 
  ccall(c, ptr); 
}

static inline v_reg_t v_ccalli(struct v_cstate *c, v_iptr ptr) {
  v_reg_t ret;

  ret = ccall(c, (v_vptr)ptr);
  return ret;
}

static inline v_reg_t v_ccallu(struct v_cstate *c, v_uptr ptr) {
  return v_ccalli(c, (v_iptr)ptr);
}

static inline v_reg_t v_ccallp(struct v_cstate *c, v_pptr ptr) {
  return v_ccalli(c, (v_iptr)ptr);
}

static inline v_reg_t v_ccalll(struct v_cstate *c, v_lptr ptr) {
  return v_ccalli(c, (v_iptr)ptr);
}

static inline v_reg_t v_ccallul(struct v_cstate *c, v_ulptr ptr) {
  return v_ccalli(c, (v_iptr)ptr);
}

void push_arg_reg (struct v_cstate *cstate, v_reg_t r);
void push_arg_imm (struct v_cstate *cstate, int imm);

void v_lambda (char *name,	/* name of function */
	       char *fmt,	/* describes type of arguments */
	       v_reg_t *args, /* regs for each argument */
	       int leaf,	/* 1 if func is a leaf */
	       v_code *ip,	/* where to write instructions */
	       int nbytes);	/* size of ip */
union v_fp v_end (int *nbytes);

void v_dump ();

#include <vcode/x86-codegen.h>

#endif __VCODE_H__
