
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

#include <xok/defs.h>
#include <exos/osdecl.h>
#include <exos/conf.h>
#include <exos/vm-layout.h>
#include <xok/env.h>

#include <exos/process.h>	/* for debugging stuff */
#include <exos/kprintf.h>
#include <stdio.h>

extern char *__progname;
extern char* symlookup(char *fname, u_int val);

/* do a stack backtrace */
void __stacktrace (int tostdout) {
  unsigned int ebp;
  unsigned int eip;
  unsigned int temp;
  int repeats;
  int foundend = 0;
  char *s;
  int (*pf)(const char *format, ...);
  extern void __start_start (), __end_start ();

  pf = (tostdout ? printf : kprintf);

  pf ("***** starting stack backtrace ***** envid: %d, (%s)\n", __envid,
      __progname);
#define BACKTRACE_MAXFRAMES	8

  asm volatile ("movl %%ebp, %0\n" : "=g" (ebp));  
  repeats = BACKTRACE_MAXFRAMES;
  while (repeats > 0) {
    repeats--;
    
    if (!isvamapped(ebp) || !isvamapped(ebp+4)) {
      /* if the stack is screwy don't cause another fault... */
      foundend = 1;
      break;
    }
    asm volatile ("movl 4(%1), %0\n" : "=r" (eip) : "r" (ebp));
    asm volatile ("movl 0(%1), %0\n" : "=r" (temp) : "r" (ebp));
    eip -= 4;
    ebp = temp;
    if (eip > (unsigned )__start_start && eip < (unsigned )__end_start) {
      foundend = 1;
      break;
    }
    pf ("\tcalled from 0x%x", eip);
    s = symlookup(((eip >= SHARED_LIBRARY_START) &&
		   (eip < SHARED_LIBRARY_START + 0x10000000)) ?
		  "/usr/lib/libexos.so" : __progname, eip);
    if (s > 0)
      pf(": %s",s);
    else
      pf(": symlookup failed");
    pf("\n");
  }
  if (foundend) {
    pf ("***** traced back to start *****\n");
  } else {
    pf ("***** quit backtrace at %d levels *****\n", BACKTRACE_MAXFRAMES);
  }
}

/* a lighter version, no symlookup */

void __stacktrace_light () {
  unsigned int ebp;
  unsigned int eip;
  unsigned int temp;
  int repeats;
  int foundend = 0;
  extern void __start_start (), __end_start ();

  kprintf ("***** starting stack backtrace LITE *****\n");

#define BACKTRACE_MAXFRAMES	8

  asm volatile ("movl %%ebp, %0\n" : "=g" (ebp));  
  repeats = BACKTRACE_MAXFRAMES;
  while (repeats > 0) {
    repeats--;
    
    asm volatile ("movl 4(%1), %0\n" : "=r" (eip) : "r" (ebp));
    asm volatile ("movl 0(%1), %0\n" : "=r" (temp) : "r" (ebp));
    eip -= 4;
    ebp = temp;
    if (eip > (unsigned )__start_start && eip < (unsigned )__end_start) {
      foundend = 1;
      break;
    }
    kprintf ("\tcalled from 0x%x", eip);
    kprintf("\n");
  }
  if (foundend) {
     kprintf ("***** traced back to start *****\n");
  } else {
     kprintf ("***** quit backtrace at %d levels *****\n", BACKTRACE_MAXFRAMES);
  }


  printf ("***** starting stack backtrace LITE *****\n");

  asm volatile ("movl %%ebp, %0\n" : "=g" (ebp));  
  repeats = BACKTRACE_MAXFRAMES;
  while (repeats > 0) {
    repeats--;
    
    asm volatile ("movl 4(%1), %0\n" : "=r" (eip) : "r" (ebp));
    asm volatile ("movl 0(%1), %0\n" : "=r" (temp) : "r" (ebp));
    eip -= 4;
    ebp = temp;
    if (eip > (unsigned )__start_start && eip < (unsigned )__end_start) {
      foundend = 1;
      break;
    }
    printf ("\tcalled from 0x%x", eip);
    printf("\n");
  }
  if (foundend) {
     printf ("***** traced back to start *****\n");
  } else {
     printf ("***** quit backtrace at %d levels *****\n", BACKTRACE_MAXFRAMES);
  }

}
