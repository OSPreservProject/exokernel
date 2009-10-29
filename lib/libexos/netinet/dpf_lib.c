
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

#include <dpf/old-dpf.h>
#include <exos/mallocs.h>

#if 0
/* this is defined in libos/udp/dpf_lib.c */
static struct dpf_frozen_ir df[] = {
        /*      op              len     r0      r1      imm     */
        {       DPF_EQI,        2,      1,      0,      0x11}, /* 0 */
        {       DPF_BITS8I,     0,      0,      0,      0x17}, /* 1 */
        {       DPF_EQI,        2,      1,      0,      0x0}, /* 2 */
        {       DPF_BITS16I,    0,      0,      0,      0x22}, /* 3 */
        {       DPF_EQI,        2,      1,      0,      0x9919}, /* 4 */
        {       DPF_BITS16I,    0,      0,      0,      0x24}, /* 5 */
};
void *mk_udp(int *sz, int src_port, int dst_port) {
	df[2].imm = src_port;
	df[4].imm = dst_port;
        *sz = sizeof df / sizeof df[0];
        return (void *)df;
}
#endif




/* RARP STRING */
static unsigned char rarp_string[] = {
				/* Ethernet Layer */
  0,0,0,0,0,0,			/* ether destination */
  0,0,0,0,0,0,			/* ether source */
  0x80, 0x35,			/* packet type ie RARP */
  
				/* RARP Layer */
  0, 1,				/* Hardware Protocol */
  0x08, 0x00,			/* Target Protocol */
  6, 4,				/* Har Len, Tar Len */
  0, 4,				/* Operation (response to RARP) */
  0,0,0,0,0,0,			/* Sender Hardware (ether) */
  0,0,0,0,			/* Sender Target (IP) */
  0,0,0,0,0,0,			/* Response Hardware */
  0,0,0,0			/* Response IP */
};

char *mk_rarp_string(int *sz, unsigned char *ethaddr) {

  char *new;
  *sz = (sizeof rarp_string);
  new = (char *) __malloc (*sz);
  memcpy(new, rarp_string, *sz);
  
  memcpy(new + 32, ethaddr, 6);
  return (void *)new;
}

/* UDP STRING */
static unsigned char udpc_string[] = {
				/* Ethernet Layer */
  0,0,0,0,0,0,			/* ether destination */
  0,0,0,0,0,0,			/* ether source */
  0x08, 0x00,			/* packet type ie IP */

				/* IP Layer */
  0x45, 0,			/* IP vers, type of service */
  0, 0,				/* total length */
  0, 0, 0, 0,			/* Ident and Fragment */
  0, 0x11,			/* TTL, protocol ie UDP */
  0, 0,				/* header checksum */
  18, 26, 0, 89,		/* source IP addr */
  18, 26, 0, 136,		/* dest IP addr */

				/* UDP Layer */
  0, 13,			/* source UDP port */
  0x13, 0x88,			/* dest   UDP port */
  0, 0,				/* UDP length */
  0, 0				/* UDP checksum */
};

char *mk_udpc_string(int *sz,
		     unsigned short src_port, unsigned char *src_ip,
		     unsigned short dst_port, unsigned char *dst_ip) 
{
  char *new;

  *sz = (sizeof udpc_string);
  new = (char *) __malloc (*sz);
  memcpy(new, udpc_string, *sz);
  
  memcpy(new + 26, dst_ip, 4);
  memcpy(new + 30, src_ip, 4);
  memcpy(new + 34, (unsigned char *)&dst_port, 2);
  memcpy(new + 36, (unsigned char *)&src_port, 2);
  return (void *)new;
}


