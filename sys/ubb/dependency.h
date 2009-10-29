
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

#ifndef __DEPENDENCY_H__
#define __DEPENDENCY_H__

#include <registry.h>
struct sec;

/* Pointer to location to write 1 when tainted, 0 when not. */
typedef unsigned char t_flag;

xn_err_t sec_can_write(sec_t A, struct sec **a);
xn_err_t sys_sec_depend(sec_t A, sec_t B, t_flag *tf);
xn_err_t sec_free(sec_t A, struct xr *xr);
xn_err_t sec_written(sec_t B);
xn_err_t sec_written_r(sec_t B, size_t n);
xn_err_t sys_sec_depend_r(sec_t A, sec_t B, size_t n, t_flag *tf);
xn_err_t sys_sec_can_write(sec_t A);
xn_err_t sys_sec_can_write_r(sec_t A, size_t n);

/* 
 * alloc = 1 is allocation.  Partial order allows us to avoid checking
 * for cycles. 
 */
xn_err_t sec_depend_r(sec_t A, sec_t B, size_t n, t_flag *tf, int alloc);

#endif /* __DEPENDENCY_H__ */
