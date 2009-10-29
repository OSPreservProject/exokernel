
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


#ifndef _BSWAP_H_
#define _BSWAP_H_

#include <sys/types.h>

this file = dead code!!

#if 1
#include <machine/endian.h>	/* no need to replicate */
#else
/*
 *  Byte swap routines for the 486 and above.
 */

static inline u_short
ntohs (u_short n)
{
  asm ("rorw $8,%w0" : "=r" (n) : "0" (n));
  return (n);
}
#define __const_htons(n) ((((n) & 0xff) << 8) | (((n) >> 8) & 0xff))
#define ntohs(x) (__builtin_constant_p (x) ? __const_htons (x) : ntohs (x))

static inline u_long
ntohl (u_long n)
{
  asm ("bswap %0" : "=r" (n) : "0" (n));
  return (n);
}
#define __const_htonl(n) (((unsigned) (n) >> 24) | (((n) >> 8) & 0xff00) \
			  | (((n) & 0xff00) << 8) | ((n) << 24))
#define ntohl(x) (__builtin_constant_p (x) ? __const_htonl (x) : ntohl (x))

#define htons(n) ntohs(n)
#define htonl(n) ntohl(n)
#endif

#endif /* _BSWAP_H_ */
