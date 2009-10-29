
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

#ifndef __KEXCEPT_H__
#define __KEXCEPT_H__
#include "arena.h"

/* Holds memory allocated only for the duration of a kernel call. */
extern Arena_T sys_arena;

/*#define K_VERBOSE */
#ifndef K_VERBOSE
#  define sys_return(res) do { Arena_free(sys_arena); return res; } while(0)
#else
#  define sys_return(res) do {						\
	Arena_free(sys_arena);           				\
	if(((int)(res)) < 0)						\
		printf("File %s, line %d: returning res %ld (%s)\n", 	\
				__FILE__, __LINE__, (long)res, #res);	\
	return res; 							\
   } while(0)
#endif

#if 0

#  define check(expr, msg, res) do {                                    \
        if(!(expr)) return res;                                         \
   } while(0)

#else

#   define check(expr, msg, res) do {                                   \
        if(!(expr)) {                                                   \
                printf("%s, %d: %s\n", __FILE__, __LINE__, #msg);       \
                return res;                                             \
        }                                                               \
   } while(0)

#endif


#define ensure(bool, condition) \
        do { if(!(bool)) sys_return(condition); } while(0)
#define ensure_s(bool, stmt) \
        do { if(!(bool)) stmt; } while(0)
#define try(foo) \
        do { xn_err_t res; if((res = (foo)) < 0) sys_return(res); } while(0)

#define try_s(foo, stmt) \
        do { xn_err_t res; if((res = (foo)) < 0) { stmt; sys_return(res); } } while(0)

#endif /*  __KEXCEPT_H__ */
