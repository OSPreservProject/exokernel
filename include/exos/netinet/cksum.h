
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


#ifndef _CKSUM_H_
#define _CKSUM_H_

#include <sys/types.h>
#include <xok/xoktypes.h>
#ifndef EXOPC
#define uint8	u_int8_t
#define uint16	u_int16_t
#define uint32	u_int32_t
#endif

#define fold_32bit_sum(sum32)			 	\
({						 	\
  register u_int __m_sum16;			 	\
  asm ("movl %1,%0           # Fold 32-bit sum\n"	\
       "\tshr $16,%0\n"				 	\
       "\taddw %w1,%w0\n"			 	\
       "\tadcw $0,%w0\n"			 	\
       : "=r" (__m_sum16) : "r" (sum32) : "cc"); 	\
  __m_sum16 == 0xffff ? 0 : __m_sum16;		 	\
})

#define ip_sum(sum32, _src, _len)					\
do {									\
  u_int __m_src = (u_int) (_src);					\
  const u_int __m_len = (_len);						\
									\
  assert (sizeof (sum32) >= 4);						\
									\
  if (__m_len >= 4)							\
    asm ("testl %%eax,%%eax        # Clear CF\n"			\
	 "1:\n"								\
	 "\tmovl (%1),%%eax\n"						\
	 "\tleal 4(%1),%1\n"						\
	 "\tadcl %%eax,%0\n"						\
	 "\tdecl %4\n"							\
	 "\tjnz 1b\n"							\
	 "\tadcl $0,%0             # Add the carry bit back in\n"	\
	 : "=d" (sum32), "=D" (__m_src)					\
	 : "0" (sum32), "1" (__m_src), "c" (__m_len>>2)			\
	 : "memory", "cc", "eax", "ecx");				\
  if (__m_len & 2) {							\
    asm ("addw (%1),%w0\n"						\
	 "\tadcl $0,%0\n"						\
	 : "=r" (sum32) : "r" (__m_src), "0" (sum32)			\
	 : "cc", "memory");						\
    __m_src += 2;							\
  }		                                                        \
  if (__m_len & 1)                                                      \
    asm ("movb (%1),%%al\n"                                             \
         "\tandl $0xff,%%eax\n"                                         \
         "\taddl %%eax,%0\n"                                            \
         "\tadcl $0,%0\n"                                               \
         : "=r" (sum32) : "r" (__m_src), "0" (sum32)                    \
         : "eax", "cc", "memory");                                      \
} while (0)


static inline uint16 fold_32bit_checksum (uint32 sum)
{
   /*  Fold 32-bit sum to 16 bits */
   while (sum>>16) {
      sum = (sum & 0xffff) + (sum >> 16);
   }
   sum = ~sum & 0xffff;
   if (sum == 0) {
      sum = 0xffff;
   }
   return (sum);
}

struct ae_recv;
uint16 inet_cksum_recv(struct ae_recv *a, uint16 count, long start);
unsigned short inet_cksum_recv_cheat(struct ae_recv *a, unsigned long count, unsigned long start);
int otto_in_cksum(struct ae_recv *a, register int len, unsigned long input);

long inet_sum(uint16 *addr, uint16 count, long start);
uint16 inet_cksum(uint16 *addr, uint16 count, long start);
uint inet_checksum(unsigned short *addr, int count, uint start, int last);
int netbsd_in_cksum(register char *p, register int len, unsigned long input);

#endif
