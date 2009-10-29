
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


#include "xok/env.h"

#ifndef NOTEXOS 
#undef assert
#endif

#ifdef NDEBUG

#define assert(x) ((void) 0)

#else /* !NDEBUG */

#ifdef KERNEL

extern void panic (const char *, ...) __attribute__ ((noreturn));
extern void warn (const char *, ...);

#define assert(x)                                                       \
do {									\
  if (! (x))								\
    panic ("%s:%d: assertion failed: %s", __FILE__, __LINE__, #x);	\
} while (0)

#define demand(x,y) \
do {									 \
  if (! (x)) {								 \
    panic ("%s:%d: demand %s failed: %s", __FILE__, __LINE__, #x, #y); \
  }									\
} while (0)

#else /* !KERNEL */

#ifndef NOTEXOS 

#include <exos/kprintf.h>
#include <exos/ptrace.h>
#include <unistd.h>
#include <signal.h>

extern void panic ( char *) __attribute__ ((noreturn));
extern void __stacktrace (int tostdout);
extern int printf (const char *, ...);




#define assert(x)							\
do {									\
  if (! (x)) {								\
    extern int __in_assert;						\
    __in_assert++;							\
    if (__in_assert == 2) {						\
       kprintf ("Recursive asserti, exiting...\n");			\
       exit (-1);							\
    }									\
    if (__in_assert >= 3) {						\
       kprintf("Double Recursive assert, looping forever.\n");		\
       while(1);							\
    }									\
    UAREA.u_in_critical = 0;						\
    kprintf ("%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #x);	\
    __stacktrace (0);							\
    if (is_being_ptraced())						\
       asm("int $3");			       				\
    else {								\
       kprintf("[%s]Pid: %d. Sending SIGSTOP to self.\n", UAREA.name,	\
               getpid());						\
       kill(getpid(), SIGSTOP);						\
       if (is_being_ptraced()) asm("int $3");				\
    }									\
    printf ("%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #x);	\
    __stacktrace (1);							\
    exit(-1);								\
  }									\
} while (0)

     /* use assert_die under ashes, asserts calls exit which is proper,
      * but not proper for not proper under ash, since we don't want to 
      * pull in exit.*/
extern void __die (void) __attribute__ ((noreturn));
#define assert_die(x)							\
do {									\
  if (! (x)) {								\
    kprintf ("%s:%d: assertion failed: %s", __FILE__, __LINE__, #x);	\
    printf ("%s:%d: assertion failed: %s", __FILE__, __LINE__, #x);	\
    __die();								\
  }									\
} while (0)

#undef __fatal
#define __fatal(_msg) do {                                  \
        kprintf("PANIC: %s\n","_msg");                                     \
        printf("PANIC: %s\n","_msg");                                     \
        kprintf("(file %s, line %d)\n",__FILE__,__LINE__);               \
        printf("(file %s, line %d)\n",__FILE__,__LINE__);               \
        exit(-1);                                                \
} while(0)

#undef demand
#define demand(x,y) \
do {									 \
  if (! (x)) {								 \
    kprintf ("%s:%d:\n  demand %s failed: %s\n", __FILE__, __LINE__, #x, #y);\
    __stacktrace (0);							\
    printf ("%s:%d: demand %s failed: %s", __FILE__, __LINE__, #x, #y); \
    __stacktrace (1);							\
    exit(-1);								\
  }									\
} while (0)

#endif /* NOEXOS */
#endif /* !KERNEL */
#endif /* !NDEBUG */
