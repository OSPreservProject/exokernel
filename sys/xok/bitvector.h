
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

#ifndef __BITVECTOR_H__
#define __BITVECTOR_H__

#define BV_NBPW 32		/* bits per word */
#define BV_LOG 5		/* log2 (BV_NBPW) */
#define BV_MASK 0x1f		/* lower BV_LOG bits set */

static inline u_int bv_ffs_word (u_int *w) {
  int ret;

  asm ("\tbsf %1, %0\n"
       "\tjnz 0f\n"
       "\tmovl $-1, %0\n"
       "0:"
       : "=r" (ret) : "r" (*w) : "cc");
  return (ret);
}

static inline u_int bv_ffs (u_int *v, u_int len) {
  int ret;
  int word = 0;

  while (word != len) {
    if ((ret = bv_ffs_word (v)) == -1) {
      v++;
      word++;
    } else {
      return (word * BV_NBPW + ret);
    }
  }
  return (-1);
}

static inline u_int bv_bt (u_int *v, u_int bit) {
  u_int word = v[bit / BV_NBPW];
  bit &= BV_MASK;
  return (word & (1 << bit) ? 1 : 0);
}

static inline void bv_bc (u_int *v, u_int bit) {
  u_int *w = &v[bit / BV_NBPW];
  bit &= BV_MASK;
  *w &= ~(1 << bit);
}

static inline void bv_bs (u_int *v, u_int bit) {
  u_int *w = &v[bit / BV_NBPW];
  bit &= BV_MASK;
  *w |= (1 << bit);
}

/* compute the size of int array required to hold `n' bits */
#define BV_SZ(n) ((n+BV_NBPW-1)/BV_NBPW)

#endif /* __BITVECTOR_H__ */
