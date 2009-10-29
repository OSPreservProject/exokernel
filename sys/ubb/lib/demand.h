
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

#ifndef DEMAND_H
#define DEMAND_H

#ifdef EXOPC
#include <xok_include/assert.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include "fucking-broken-sunos.h"
#endif /* EXOPC */

/*@-ansireserved@*/
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifdef NDEBUG
#if 0
#       define demand(boolexp, msg)        do { } while(0)
#       define assert(boolexp)             do { } while(0)
#endif
#	define debug_stmt(s)		do { } while(0)
#else /* NDEBUG */
#	define debug_stmt(s)		do { s; } while(0)
#       if defined(__GNUC__) || defined(__ANSI__)
#if 0
#               define assert(boolexp) do {                                \
                        if(!(boolexp))                                        \
                                AAssert(#boolexp, __FILE__, __LINE__);    \
                } while(0)

#               define demand(boolexp, msg) do {                           \
                        if(!(boolexp))                                        \
                                DDemand(#boolexp, #msg, __FILE__, __LINE__);\
                } while(0)
#endif

#       else /* __GNUC__ */

#               define assert(boolexp) do {                                \
                        if(!(boolexp))                                        \
                                AAssert("boolexp", __FILE__, __LINE__);   \
                } while(0)

#               define demand(boolexp, msg) do {                           \
                        if(!(boolexp))                                        \
                                DDemand("boolexp", "msg", __FILE__, __LINE__);\
                } while(0)

#               define fatal(msg) FFatal(__FILE__, __LINE__, "msg")
#       endif   /* __GNUC__ */

#ifdef EXOPC
#define DDemand(_boolexp, _msg, _file, line) do {                         \
        printf("file %s, line %d: %s\n",_file,line,_boolexp);    \
        panic(_msg);							  \
} while(0)
#else
#define DDemand(_boolexp, _msg, _file, line) do {                         \
        fprintf(stderr, "file %s, line %d: %s\n",_file,line,_boolexp);    \
        fprintf(stderr, "%s\n",_msg);                                     \
        abort();							  \
} while(0)
#endif

#ifdef EXOPC
#define AAssert(_boolexp, _file, line) do {			\
        printf("file %s, line %d: %s\n",_file,line,_boolexp);	\
        panic("assertion failed");				\
} while(0)
#else
#define AAssert(_boolexp, _file, line) do {                               \
        fprintf(stderr, "file %s, line %d: %s\n",_file,line,_boolexp);    \
        abort();							  \
} while(0)
#endif

#endif /* NDEBUG */

#ifdef EXOPC
#define FFatal(_file, _line, _msg) do {			\
        printf("PANIC: %s\n",_msg);			\
        printf("(file %s, line %d)\n",_file,_line);	\
        panic("Fatal");					\
} while(0)
#else
#define FFatal(_file, _line, _msg) do {                                \
        fprintf(stderr, "PANIC: %s\n",_msg);                            \
        fprintf(stderr, "(file %s, line %d)\n",_file,_line);           \
        abort();							\
} while(0)
#endif

#define fatal(msg) FFatal(__FILE__, __LINE__, #msg)
        
/* Compile-time assertion used in function. */
#define AssertNow(x) switch(1) { case (x): case 0: }


/* Compile time assertion used at global scope; need expansion. */
#define _assert_now2(y) _ ##  y
#define _assert_now1(y) _assert_now2(y)
#define AssertNowF(x)                                                   \
  static inline void _assert_now1(__LINE__)()                           \
        { AssertNow(x); (void)_assert_now1(__LINE__);  /* shut up -Wall. */}
#endif /* DEMAND_H */
