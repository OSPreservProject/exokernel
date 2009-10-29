
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


#ifndef _DEFS_H_
#define _DEFS_H_

#ifdef KERNEL

#ifndef NULL
#define NULL ((void *) 0)
#endif /* !NULL */

#define min(_a, _b)				\
({						\
  typeof (_a) __a = (_a);			\
  typeof (_b) __b = (_b);			\
  __a <= __b ? __a : __b;			\
})

#define max(_a, _b)				\
({						\
  typeof (_a) __a = _a;				\
  typeof (_b) __b = _b;				\
  __a >= __b ? __a : __b;				\
})
#endif

#if 0
#define DEF_ALIAS_FN(alias, fn)			\
     asm (".globl _" #alias "\n"		\
	  "\t.set _" #alias ",_" #fn)

#else
#define DEF_ALIAS_FN(alias, fn)			\
     asm (".stabs \"_" #alias "\",11,0,0,0\n"	\
	  "\t.stabs \"_" #fn "\",1,0,0,0");
#endif
#define DEF_SYM(symbol, addr)			\
     asm (".globl _" #symbol "\n"		\
	  "\t.set _" #symbol ",%P0"		\
	  :: "i" (addr))
#define WARN_SYM(fn, warning)			\
     asm (".stabs \"_" #warning "\",30,0,0,0\n"	\
	  "\t.stabs \"_" #fn "\",1,0,0,0");


/* Static assert, for compile-time assertion checking */
#define StaticAssert(c) switch (c) case 0: case (c):

#define offsetof(type, member)  ((size_t)(&((type *)0)->member))

#define __XOK_NOSYNC
#define __XOK_SYNC(what)
#define __XOK_REQ_SYNC(what)

#endif /* !_DEFS_H_ */
