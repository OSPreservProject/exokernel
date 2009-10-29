
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

#ifndef __DEMAND__
#define __DEMAND__
#include <xok_include/assert.h>

#define NDEBUG

#define fatal(msg) __fatal(__FILE__, __LINE__, "msg")

#ifdef NDEBUG
#if 0
#       define demand(bool, msg)        do { } while(0)
#       define assert(bool)             do { } while(0)
#endif
#	define debug_stmt(stmt)		do { } while(0)
#else /* NDEBUG */
#	define debug_stmt(stmt)		do { stmt; } while(0)
#       if defined(__GNUC__) || defined(__ANSI__)
#if 0
#               define assert(bool) do {                                \
                        if(bool)                                        \
                                (void)0;                                \
                        else                                            \
                                __assert(#bool, __FILE__, __LINE__);    \
                } while(0)

#               define demand(bool, msg) do {                           \
                        if(bool)                                        \
                                (void)0;                                \
                        else                                            \
                                __demand(#bool, #msg, __FILE__, __LINE__);\
                } while(0)
#endif

#               define fatal(msg) __fatal(__FILE__, __LINE__, #msg)

#       else /* __GNUC__ */

#if 0
#               define assert(bool) do {                                \
                        if(bool)                                        \
                                (void)0;                                \
                        else                                            \
                                __assert("bool", __FILE__, __LINE__);   \
                } while(0)

#               define demand(bool, msg) do {                           \
                        if(bool)                                        \
                                (void)0;                                \
                        else                                            \
                                __demand("bool", "msg", __FILE__, __LINE__);\
                } while(0)
#endif

#       endif   /* __GNUC__ */

#define __demand(_bool, _msg, _file, line) do {                         \
        printf("file %s, line %d: %s\n",_file,line,_bool);              \
        printf("%s\n",_msg);                                            \
        panic("demand");                                                       \
} while(0)

#define __assert(_bool, _file, line) do {                               \
        printf("file %s, line %d: %s\n",_file,line,_bool);              \
        panic("demand");                                                       \
} while(0)

#endif /* NDEBUG */

#define __fatal(_file, _line, _msg) do {                                \
        printf("PANIC: %s\n",_msg);                                     \
        printf("(file %s, line %d)\n",_file,_line);                     \
        panic("demand");                                                       \
} while(0)
        
/* Compile-time assertion used in function. */
#define AssertNow(x) switch(1) { case (x): case 0: }

/* Compile time assertion used at global scope. */
#define _assert_now2(y) _ ##  y
#define _assert_now1(y) _assert_now2(y)
#define AssertNowF(x) \
  static inline void _assert_now1(__LINE__)() { \
                /* used to shut up warnings */                  \
                void (*fp)(int) = _assert_now1(__LINE__);       \
                fp(1);                                          \
                switch(1) case (x): case 0: ;                   \
  }

#endif
