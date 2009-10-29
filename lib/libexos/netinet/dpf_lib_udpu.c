
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

/* make a udp packet-filter */

#include <dpf/old-dpf.h>
#include <string.h>
#include <exos/netinet/in.h>

#include <exos/netinet/dpf_lib.h>

/* Disconnected UDP filter  */

static struct dpf_frozen_ir udpu[] = {
  /*      op              len     r0      r1      imm     */
  {       DPF_EQI,        2,      1,      0,      0x8},  /* 0 Ether Type IP */
  {       DPF_BITS16I,    0,      0,      0,      0xc},  /* 1 */
  {       DPF_EQI,        2,      1,      0,      0x45}, /* 2 IP ver:len */
  {       DPF_BITS8I,     0,      0,      0,      0xe},  /* 3 */
  {       DPF_EQI,        2,      1,      0,      0x11}, /* 4 UDP Type */
  {       DPF_BITS8I,     0,      0,      0,      0x17}, /* 5 */
  
  {       DPF_EQI,        2,      1,      0,      0},    /* 6 IP DEST (ours)*/
  {       DPF_BITS8I,     0,      0,      0,      0x1e}, /* 7 */
  {       DPF_EQI,        2,      1,      0,      0},    /* 8 */
  {       DPF_BITS8I,     0,      0,      0,      0x1f}, /* 9 */
  {       DPF_EQI,        2,      1,      0,      0},    /* 10 */
  {       DPF_BITS8I,     0,      0,      0,      0x20}, /* 11 */
  {       DPF_EQI,        2,      1,      0,      0},    /* 12 */
  {       DPF_BITS8I,     0,      0,      0,      0x21}, /* 13 */
  
  {       DPF_EQI,        2,      1,      0,      0x00}, /* 14 PORT DEST (ours) */
  {       DPF_BITS16I,    0,      0,      0,      0x24}, /* 15 */
};

static struct dpf_frozen_ir udpu_unbound[] = {
  /*      op              len     r0      r1      imm     */
  {       DPF_EQI,        2,      1,      0,      0x8},  /* 0 Ether Type IP */
  {       DPF_BITS16I,    0,      0,      0,      0xc},  /* 1 */
  {       DPF_EQI,        2,      1,      0,      0x45}, /* 2 IP ver:len */
  {       DPF_BITS8I,     0,      0,      0,      0xe},  /* 3 */
  {       DPF_EQI,        2,      1,      0,      0x11}, /* 4 UDP Type */
  {       DPF_BITS8I,     0,      0,      0,      0x17}, /* 5 */
  
  {       DPF_EQI,        2,      1,      0,      0x00}, /* 6 PORT DEST (ours) */
  {       DPF_BITS16I,    0,      0,      0,      0x24}, /* 7 */
};

/* make a udp packet-filter */
void *mk_udpu(int *sz, 
	     unsigned short src_port, unsigned char *src_ip) {
#if 0
  printf("mk_udpu: src ip: ");  ip_praddr(src_ip); printf("  src port: %d\n",src_port);
#endif
  /* if the source port is any address, don't restrict this field. */
  if (bcmp(src_ip, IPADDR_ANY, 4) == 0) {
    udpu_unbound[6].imm = src_port;
    *sz = sizeof udpu_unbound / sizeof udpu_unbound[0];
    return (void *)udpu_unbound;
  } else {
    udpu[6].imm = src_ip[0];  udpu[8].imm = src_ip[1];
    udpu[10].imm = src_ip[2]; udpu[12].imm = src_ip[3];
    
    udpu[14].imm = src_port;
    *sz = sizeof udpu / sizeof udpu[0];
    return (void *)udpu;
  }
}
