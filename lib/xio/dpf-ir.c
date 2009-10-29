
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

#include <dpf/dpf-internal.h>
#include <assert.h>
#define fatal(x) do { printf(#x); assert(0); } while (0)
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define IDENTITY_MASK8	0xffUL
#define IDENTITY_MASK16 0xffffUL
#define IDENTITY_MASK32	0xffffffffUL


void dpf_begin(struct dpf_ir *ir) {
	memset(ir, 0, sizeof *ir);
}

int
dpf_sizeof(struct dpf_ir* ir) {
  return offsetof(struct dpf_ir, ir[ir->irn + 1]);
}

static inline struct ir *alloc(struct dpf_ir *ir) {
	if(ir->irn >= DPF_MAXELEM)
		fatal(Code size exceeded.  Increase DPF_MAX.);
	return &ir->ir[ir->irn++];
}

static inline void 
mkeq(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint32 val) {
	struct eq *eq;

	eq = &alloc(ir)->u.eq;
	eq->op = DPF_OP_EQ;
	/* add in relative offset now so don't have to do at demux time. */
	eq->offset = offset + ir->moffset;
	eq->val = val;
	eq->mask = mask;
	eq->nbits = nbits;
}

void dpf_eq8(struct dpf_ir *ir, uint16 offset, uint8 val) 
	{ dpf_meq8(ir, offset, IDENTITY_MASK8, val);   }
void dpf_eq16(struct dpf_ir *ir, uint16 offset, uint16 val) 
	{ dpf_meq16(ir, offset, IDENTITY_MASK16, val); }
void dpf_eq32(struct dpf_ir *ir, uint16 offset, uint32 val) 
	{ dpf_meq32(ir, offset, IDENTITY_MASK32, val); }

/* Is the message quantity of nbits at offset equal to val? */
void dpf_meq8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 val) {
	if((mask & IDENTITY_MASK8) != mask)
		fatal(Mask exceeds width of type.);
	mkeq(ir, 8, offset, mask, val);
}

void dpf_meq16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint16 val) {
	if((mask & IDENTITY_MASK16) != mask)
		fatal(Mask exceeds width of type.);
	mkeq(ir, 16, offset, mask, val);
}

void dpf_meq32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint32 val) {
	if((mask & IDENTITY_MASK32) != mask)
		fatal(Mask exceeds width of type.);
	mkeq(ir, 32, offset, mask, val);
}

static inline void
mkshift(struct dpf_ir *ir, uint8 nbits, uint16 offset, uint32 mask, uint8 shift) {
	struct shift *s;

	s = &alloc(ir)->u.shift;
	s->op = DPF_OP_SHIFT;
	s->shift = shift;
	s->offset = offset;
	s->mask = mask;
	s->nbits = nbits;
	ir->moffset = 0;	/* relative offset is zero. */
	
}

/* shift the base message pointer by nbits */
void dpf_shift8(struct dpf_ir *ir, uint16 offset, uint8 shift) 
	{ dpf_mshift8(ir, offset, IDENTITY_MASK8, shift); }
void dpf_shift16(struct dpf_ir *ir, uint16 offset, uint8 shift) 
	{ dpf_mshift16(ir, offset, IDENTITY_MASK16, shift); }
void dpf_shift32(struct dpf_ir *ir, uint16 offset, uint8 shift) 
	{ dpf_mshift32(ir, offset, IDENTITY_MASK32, shift); }

void dpf_mshift8(struct dpf_ir *ir, uint16 offset, uint8 mask, uint8 shift) 
	{ mkshift(ir, 8, offset, mask, shift); }
void dpf_mshift16(struct dpf_ir *ir, uint16 offset, uint16 mask, uint8 shift) 
	{ mkshift(ir, 16, offset, mask, shift); }
void dpf_mshift32(struct dpf_ir *ir, uint16 offset, uint32 mask, uint8 shift) 
	{ mkshift(ir, 32, offset, mask, shift); }

void dpf_shifti(struct dpf_ir *ir, uint16 offset) 
	{ ir->moffset += offset; }



/* Useful ir construction utilities */

#ifdef __linux__
#include <linux/if_ether.h>

#define ETH_HDR_START (0)

void
dpf_eq_eth_src_mac(struct dpf_ir* ir, uint8* mac) {
  dpf_eq8(ir, ETH_HDR_START + offsetof(struct ethhdr, h_source[0]), mac[0]);
  dpf_eq8(ir, ETH_HDR_START + offsetof(struct ethhdr, h_source[1]), mac[1]);
  dpf_eq8(ir, ETH_HDR_START + offsetof(struct ethhdr, h_source[2]), mac[2]);
  dpf_eq8(ir, ETH_HDR_START + offsetof(struct ethhdr, h_source[3]), mac[3]);
  dpf_eq8(ir, ETH_HDR_START + offsetof(struct ethhdr, h_source[4]), mac[4]);
  dpf_eq8(ir, ETH_HDR_START + offsetof(struct ethhdr, h_source[5]), mac[5]);
}

void
dpf_eq_eth_dst_mac(struct dpf_ir* ir, uint8* mac) {
  dpf_eq8(ir, offsetof(struct ethhdr, h_dest[0]), mac[0]);
  dpf_eq8(ir, offsetof(struct ethhdr, h_dest[1]), mac[1]);
  dpf_eq8(ir, offsetof(struct ethhdr, h_dest[2]), mac[2]);
  dpf_eq8(ir, offsetof(struct ethhdr, h_dest[3]), mac[3]);
  dpf_eq8(ir, offsetof(struct ethhdr, h_dest[4]), mac[4]);
  dpf_eq8(ir, offsetof(struct ethhdr, h_dest[5]), mac[5]);
}

void
dpf_eq_eth_proto(struct dpf_ir* ir, uint16 proto) {
  dpf_eq16(ir, offsetof(struct ethhdr, h_proto), proto);
}
#endif


#ifdef __linux__
#include <sys/types.h>
#include <linux/ip.h>

#define IP_HDR_START (ETH_HDR_START + sizeof(struct ethhdr))

void
dpf_eq_ip_proto(struct dpf_ir* ir, uint8 proto) {
  dpf_eq8(ir, IP_HDR_START + offsetof(struct iphdr, protocol), proto);
}

void
dpf_eq_ip_src_addr(struct dpf_ir* ir, uint32 addr) {
  dpf_eq32(ir, IP_HDR_START + offsetof(struct iphdr, saddr), addr);
}

void
dpf_eq_ip_dst_addr(struct dpf_ir* ir, uint32 addr) {
  dpf_eq32(ir, IP_HDR_START + offsetof(struct iphdr, daddr), addr);
}
#endif


#ifdef __linux__
#include <linux/tcp.h>

#define TCP_HDR_START (IP_HDR_START + sizeof(struct iphdr))

void
dpf_eq_tcp_src_port(struct dpf_ir* ir, uint16 port) {
  dpf_eq16(ir, TCP_HDR_START + offsetof(struct tcphdr, source), port);
}

void
dpf_eq_tcp_dst_port(struct dpf_ir* ir, uint16 port) {
  dpf_eq16(ir, TCP_HDR_START + offsetof(struct tcphdr, dest), port);
}
#endif

