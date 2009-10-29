
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

#include <xok/env.h>
#include <xok/sys_ucall.h>
#include <exos/osdecl.h>
#include <exos/process.h>
#include <exos/vm-layout.h>

/* determine_staticness will be at the beginning of the data segment of the
   executable and thus the loader can determine whether the program was
   linked statically or dynamically by comparing the value to
   SHARED_LIBRARY_START */
extern void __start (void *args);
/* the second word will tell the loader the text start address */
int ___dummy (int argc, char *argv[], char *envp[]);

/* this is the first code a process runs by virtue of it being
   linked in at the start of the program. It initializes entry points and
   tells the shared lib where it's main is and then calls into the shared
   lib so that it can initialize itself. */

int ___dummy (int argc, char *argv[], char *envp[]) {
  /* Leave some room for an exception stack at the top of the real stack */
  char xstack[1024];   
  extern unsigned __main_trampoline;
  extern int main (int, char *[], char *[]);

  /* if we are a dynamically linked program then the demand loaded should
     have already run the startup code and we should jump straight to main.
     There are three situations we need to check for:
     1. we're running the shared library's ___dummy
     2. we're running a dynamically linked program's ___dummy
     3. we're running a statically linked program's ___dummy
     For 1 and 3 we want to continue.  For 2 we want to jump to main */
  if ((u_int)__start > SHARED_LIBRARY_START &&
      (u_int)___dummy < SHARED_LIBRARY_START) {
    return main(argc, argv, envp);
  }

  UAREA.u_xsp = (u_int) xstack + sizeof (xstack);
  UAREA.u_entfault2 = (u_int) xue_fault;

  __main_trampoline = (unsigned)main;
  __start((void*)&argc);
  return 0;
}
