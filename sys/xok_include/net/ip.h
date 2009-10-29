
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


#ifndef _IP_H_
#define _IP_H_

#include <xok/defs.h>
#include <xok/types.h>

#define IPPROTO_IP 0        /* dummy for IP */
#define IPPROTO_ICMP 1      /* control message protocol */
#define IPPROTO_IGMP 2      /* group mgmt protocol */
#define IPPROTO_GGP 3       /* gateway^2 (deprecated) */
#define IPPROTO_IPIP 4      /* IP inside IP */
#define IPPROTO_TCP 6       /* tcp */
#define IPPROTO_EGP 8       /* exterior gateway protocol */
#define IPPROTO_PUP 12      /* pup */
#define IPPROTO_UDP 17      /* user datagram protocol */
#define IPPROTO_IDP 22      /* xns idp */
#define IPPROTO_TP 29       /* tp-4 w/ class negotiation */
#define IPPROTO_EON 80      /* ISO cnlp */
#define IPPROTO_ENCAP 98    /* encapsulation header */
#define IPPROTO_RAW 255     /* raw IP packet */
#define IPPROTO_MAX 256

struct udp_pkt {
  u_short udp_sport;
  u_short udp_dport;
  u_short udp_len;
  u_short udp_sum;
  u_char udp_data[0];
};

struct ip_pkt {
  u_char ip_hl : 4;         /* Header length */
  u_char ip_v : 4;          /* Version (4) */
  u_char ip_tos;            /* Type of service */
  u_short ip_len;           /* Total length */
  u_short ip_id;            /* Identification */
  u_short ip_off : 13;      /* Fragment offset */
  u_short ip_mf : 1;        /* More fragments flag */
  u_short ip_df : 1;        /* Don't fragment flag */
  u_char ip_ttl;            /* Time to live */
  u_char ip_p;              /* Protocol */
  u_short ip_sum;           /* Checksum */
  u_int ip_src;             /* Source address */
  u_int ip_dst;             /* Destination address */
  union {
    struct udp_pkt udp;
  } ip_data[0];
#define ip_udp ip_data[0].udp
};

/* Copy a packet using a source segment override prefix.  For
 * instance, use:
 *    pkt_copy (buf, "%fs:", NULL, len);
 * to copy a packet to buf from a kernel buffer in segment register
 * "%fs".  */
#define pkt_copy(dst, sseg, src, len)				\
do {								\
  if (__builtin_constant_p (len) && ! (len & 0x3))		\
    asm volatile ("cld\n"					\
		  "\tshrl $2,%%ecx\n"				\
		  "\trep\n"					\
		  "\t "sseg"movsl\n"				\
		  :: "S" (src), "D" (dst), "c" (len)		\
		  : "ecx", "esi", "edi", "cc", "memory");	\
  else								\
    asm volatile ("cld\n"					\
		  "\tmovl %2,%%ecx\n"				\
		  "\tshrl $2,%%ecx\n"				\
		  "\trep\n"					\
		  "\t "sseg"movsl\n"				\
		  "\tmovl %2,%%ecx\n"				\
		  "\tandl $3,%%ecx\n"				\
		  "\trep\n"					\
		  "\t "sseg"movsb"				\
		  :: "S" (src), "D" (dst), "g" (len)		\
		  : "ecx", "esi", "edi", "cc", "memory");	\
} while (0)


#endif /* !_IP_H_ */

