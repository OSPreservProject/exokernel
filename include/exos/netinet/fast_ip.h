
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

#ifndef _FAST_IP_H_
#define _FAST_IP_H_
#include <sys/types.h>
#include <xok/xoktypes.h>

#define ip_offset(field) ((u_int) &((struct ip *) 0)->field)
#define ip_sz (sizeof (struct ip))

/* 
 * IP header, see RFC791
 *
 * Should define these somewhere: uint8, uint16, uint32.
 */
struct ip {
	uint8 vrsihl;      	/* Version and Internet Header Length */
	uint8 tos;		/* Type of Service                    */
	uint16 totallength;	/* Length of datagram                 */
	uint16 identification; 	/* Identification for reassembly   */
	uint16 fragoffset;	/* Fragment offset and flags          */
	uint8 ttl;		/* Time to Live                       */
	uint8 protocol;		/* Superior protocol                  */
	uint16 checksum;	/* header checksum                    */
	uint16 source[2];	/* Source IP address                  */
	uint16 destination[2]; 	/* Destination IP address          */
};

/* ip_vrsihl */
#define MK_VRSIHL(version, length)	(((version)<<4)|((length)>>2))
#define DEF_VRSIHL	MK_VRSIHL(4, sizeof(struct ip_hdr))

/* ip_tos (Type of Service) */
enum { 
	IP_TOS_PRC_ROUTINE, 
	IP_TOS_PRC_PRIORITY,
	IP_TOS_PRC_IMMEDIATE,
	IP_TOS_PRC_FLASH,
	IP_TOS_PRC_FLSHOVR,
	IP_TOS_PRC_CRITIC,
	IP_TOS_PRC_INC,
	IP_TOS_PRC_NC
};

enum {
	IP_TOS_LOWDELAY	= 8,
	IP_TOS_HIGHTHRU = 16,
	IP_TOS_HIGHRELIAB = 32
};

/* ip_fragoffset */
#define IP_FRG_DF		0x4000
#define IP_FRG_MF		0x2000
#define IP_FRAGOFF              0x1fff

/* ip_protocol */
#define IP_PROTO_ICMP		1
#define IP_PROTO_IGMP		2
#define IP_PROTO_GGP		3
#define IP_PROTO_ST		5
#define IP_PROTO_TCP		6
#define IP_PROTO_UCL		7
#define IP_PROTO_EGP		8
#define IP_PROTO_IGP		9
#define IP_PROTO_BBN		10
#define IP_PROTO_NVPII		11
#define IP_PROTO_PUP		12
#define IP_PROTO_ARGUS		13
#define IP_PROTO_EMCON		14
#define IP_PROTO_XNET		15
#define IP_PROTO_CHAOS		16
#define IP_PROTO_UDP		17
#define IP_PROTO_MUX		18
#define IP_PROTO_DCN		19
#define IP_PROTO_HMP		20
#define IP_PROTO_PRM		21
#define IP_PROTO_XNS		22
#define IP_PROTO_TRUNK1		23
#define IP_PROTO_TRUNK2		24
#define IP_PROTO_LEAF1		25
#define IP_PROTO_LEAF2		26
#define IP_PROTO_RDP		27
#define IP_PROTO_IRTP		28
#define IP_PROTO_ISOTP4		29
#define IP_PROTO_NETBLT		30
#define IP_PROTO_MFENSP		31
#define IP_PROTO_MERIT		32
#define IP_PROTO_SEP		33
#define IP_PROTO_CFTP		62
#define IP_PROTO_SATNET		64
#define IP_PROTO_MITSUBNET	65
#define IP_PROTO_RVD		66
#define IP_PROTO_IPPC		67
#define IP_PROTO_SATMON		69
#define IP_PROTO_IPCV		71
#define IP_PROTO_BRSATMON	76
#define IP_PROTO_WBMON		78
#define IP_PROTO_WBEXPAK	79

#endif /* _FAST_IP_H_ */
