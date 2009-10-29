
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


/* gensyms.c     Generates <xok/sys_syms.h> with #defines to all of the
                 previously DEF_SYM'ed addresses.  Needed because the
                 assembly code can't deal with the offsetof operator -dkw */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "gensyms.h"

void gensyms(struct sym_type* defsyms) {
  int i;
  char filler[50];

  printf("/*  sys_syms.h   Automagically generated from gensyms.c */\n");
  printf("\n");
  printf("\n#ifndef _XOK_SYS_SYMS_H_\n# define _XOK_SYS_SYMS_H_\n\n\n");
  memset(filler,' ',50);
  filler[49] = '\0';
  
  printf("\n\n/* Hex addresses  */\n");
  for (i = 0; defsyms[i].addr != 0; i++) {
    printf("#define %s_H_ADDR",defsyms[i].name);
    filler[25 - strlen(defsyms[i].name)] = '\0';
    printf(" %s 0x%x\n",filler,defsyms[i].addr);
    filler[25 - strlen(defsyms[i].name)] = ' ';
  }

  printf("\n\n/* Defines for assembly files  */\n");
  for (i = 0; defsyms[i].addr != 0; i++) {
    printf("#define %s_A_ADDR",defsyms[i].name);
    filler[25 - strlen(defsyms[i].name)] = '\0';
    printf(" %s %d\n",filler,defsyms[i].addr);
    filler[25 - strlen(defsyms[i].name)] = ' ';
  }
#if 0
  printf("\n#ifndef KERNEL\n");
  printf("\n\n/* Typed Defines for c files  */\n");
  for (i = 0; defsyms[i].addr != 0; i++) {
    printf("#define %s",defsyms[i].name);
    filler[32 - strlen(defsyms[i].name)] = '\0';
    printf(" %s (%s0x%x)\n",filler,defsyms[i].dtype,defsyms[i].addr);
    filler[32 - strlen(defsyms[i].name)] = ' ';
  }
  printf("\n\n#endif /* KERNEL */\n\n");
#endif
  printf("\n\n#endif\n\n");
}
