
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

#ifndef BIT_INCLUDED
#define BIT_INCLUDED
#define T Bit_T
typedef struct T *T;
extern T   Bit_new   (int length);
extern T Bit_build(int length, char* bytes);
extern int Bit_length(T set);
extern int Bit_count (T set);
extern void Bit_free(T *set);
extern int Bit_get(T set, int n);
extern int Bit_put(T set, int n, int bit);
extern int Bit_on(T set, int lo, int hi);
extern int Bit_off(T set, int lo, int hi);
extern int Bit_run(T set, int start, int n);
extern int Bit_run_bounded(T set, int start, int end, int n);

extern void Bit_clear(T set, int lo, int hi);
extern void Bit_set  (T set, int lo, int hi);
extern void Bit_not  (T set, int lo, int hi);
extern int Bit_lt (T s, T t);
extern int Bit_eq (T s, T t);
extern int Bit_overlap (T s, T t);
extern int Bit_leq(T s, T t);
extern void Bit_map(T set,
 	void apply(int n, int bit, void *cl), void *cl);
void Bit_smap(T set, int lb, int ub,
        void apply(int n, int bit, void *cl), void *cl) ;
extern T Bit_union(T s, T t);
extern T Bit_inter(T s, T t);
extern T Bit_minus(T s, T t);
extern T Bit_diff (T s, T t);

/* Get pointer to free map. */
extern void * Bit_handle(T t, unsigned bytes);
void Bit_copyin(T t, void *handle, unsigned bytes);

#undef T
#endif
