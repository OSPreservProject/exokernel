
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

#include <stdlib.h>
#ifdef EXOPC
#include <exos/mallocs.h>
#include <dpf/old-dpf.h>
#endif /* EXOPC */


/* ARP FILTER */

static struct dpf_frozen_ir arp_all[] = {
    /*      op              len     r0      r1      imm     */
{       DPF_EQI,        2,      1,      0,   0x0608},  /* Protocol ARP */
{       DPF_BITS16I,    0,      0,      0,      0xc},  /* 2 */
{       DPF_EQI,        2,      1,      0,    0x100},  /* HW proto Ethernet */
{       DPF_BITS16I,    0,      0,      0,      0xe},  /* 4 */
{       DPF_EQI,        2,      1,      0,      0x8},  /* Target proto EP_IP*/
{       DPF_BITS16I,    0,      0,      0,     0x10},  /* 6 */
};

struct elem *mk_arp_getall(int *sz) {
  *sz = sizeof arp_all / sizeof arp_all[0];
  return (void *)arp_all;
}


static struct dpf_frozen_ir arp_reply[] = {
    /*      op              len     r0      r1      imm     */
{       DPF_EQI,        2,      1,      0,   0x0608},  /* Protocol ARP */
{       DPF_BITS16I,    0,      0,      0,      0xc},  /* 2 */
{       DPF_EQI,        2,      1,      0,    0x100},  /* HW proto Ethernet */
{       DPF_BITS16I,    0,      0,      0,      0xe},  /* 4 */
{       DPF_EQI,        2,      1,      0,      0x8},  /* Target proto EP_IP*/
{       DPF_BITS16I,    0,      0,      0,     0x10},  /* 6 */
{       DPF_EQI,        2,      1,      0,    0x200},  /* Operation ARP_REP */
{       DPF_BITS16I,    0,      0,      0,     0x14}   /* 8 */
};

struct elem *mk_arp_getreplies(int *sz) {
  *sz = sizeof arp_reply / sizeof arp_reply[0];
  return (void *)arp_reply;
}

static struct dpf_frozen_ir arp_request[] = {
    /*      op              len     r0      r1      imm     */
{       DPF_EQI,        2,      1,      0,   0x0608},  /* Protocol ARP */
{       DPF_BITS16I,    0,      0,      0,      0xc},  /* 2 */
{       DPF_EQI,        2,      1,      0,    0x100},  /* HW proto Ethernet */
{       DPF_BITS16I,    0,      0,      0,      0xe},  /* 4 */
{       DPF_EQI,        2,      1,      0,     0x08},  /* Target proto EP_IP*/
{       DPF_BITS16I,    0,      0,      0,     0x10},  /* 6 */
{       DPF_EQI,        2,      1,      0,    0x100},  /* Operation ARP_REQ*/
{       DPF_BITS16I,    0,      0,      0,     0x14},  /* 8 */
{       DPF_EQI,        2,      1,      0,      0x0},  /* Target Phys Addr [0] */
{       DPF_BITS8I,     0,      0,      0,     0x26},  /* 10 */
{       DPF_EQI,        2,      1,      0,      0x0},  /* Target Phys Addr [1] */
{       DPF_BITS8I,     0,      0,      0,     0x27},  /* 12 */
{       DPF_EQI,        2,      1,      0,      0x0},  /* Target Phys Addr [2] */
{       DPF_BITS8I,     0,      0,      0,     0x28},  /* 14 */
{       DPF_EQI,        2,      1,      0,      0x0},  /* Target Phys Addr [3] */
{       DPF_BITS8I,     0,      0,      0,     0x29}   /* 16 */
};

static struct dpf_frozen_ir arp_request_only[] = {
    /*      op              len     r0      r1      imm     */
{       DPF_EQI,        2,      1,      0,   0x0608},  /* Protocol ARP */
{       DPF_BITS16I,    0,      0,      0,      0xc},  /* 2 */
{       DPF_EQI,        2,      1,      0,    0x100},  /* HW proto Ethernet */
{       DPF_BITS16I,    0,      0,      0,      0xe},  /* 4 */
{       DPF_EQI,        2,      1,      0,     0x08},  /* Target proto EP_IP*/
{       DPF_BITS16I,    0,      0,      0,     0x10},  /* 6 */
{       DPF_EQI,        2,      1,      0,    0x100},  /* Operation ARP_REQ*/
{       DPF_BITS16I,    0,      0,      0,     0x14}  /* 8 */
};

struct elem *mk_arp_getrequests(int *sz, unsigned char *ipaddr) {

  if (ipaddr) {
    arp_request[8].imm = ipaddr[0];
    arp_request[10].imm = ipaddr[1];
    arp_request[12].imm = ipaddr[2];
    arp_request[14].imm = ipaddr[3];
    *sz = sizeof arp_request / sizeof arp_request[0];
    return (void *)arp_request;
  } else {
    *sz = sizeof arp_request_only / sizeof arp_request_only[0];
    return (void *)arp_request_only;
  }
}


/* ARP STRING */
/* The filter for ARP packets should match them all, since we do the final 
   filtering in the ARP client, the reason is that we may want to cache other ARP
   messages that are in the network. */
static unsigned char arp_string[] = {
				/* Ethernet Layer */
  0,0,0,0,0,0,			/* ether destination */
  0,0,0,0,0,0,			/* ether source */
  0x08, 0x06,			/* packet type ie ARP */
  
				/* ARP Layer */
  0, 1,				/* Hardware Protocol */
  0x08, 0x00,			/* Target Protocol */
  6, 4,				/* Har Len, Tar Len */
  0, 2,				/* Operation (response to query) */
  0,0,0,0,0,0,			/* Sender Hardware (ether) */
  0,0,0,0,			/* Sender Target (IP) */
  0,0,0,0,0,0,			/* Response Hardware */
  0,0,0,0			/* Response IP */
  };

char *mk_arp_string(int *sz, unsigned char *ipaddr) {

  char *new;

  *sz = (sizeof arp_string);
  new = (char *) __malloc (*sz);
  memcpy(new, arp_string, *sz);
  
  memcpy(new + 26, ipaddr, 4);
  return (void *)new;
}


static struct dpf_frozen_ir icmp_all[] = {
    /*      op              len     r0      r1      imm     */
  {       DPF_EQI,        2,      1,      0,      0x8},  /* 0 EP_IP protocol */
  {       DPF_BITS16I,    0,      0,      0,      0xc},  /* 1 */
  {       DPF_EQI,        2,      1,      0,      0x45}, /* 3 IP version/length */
  {       DPF_BITS8I,     0,      0,      0,      0xe},  /* 4 */
  {       DPF_EQI,        2,      1,      0,      0x1 }, /* 5 ICMP Protocol
  {       DPF_BITS8I,     0,      0,      0,      0x17}, /* 6 */
};

struct elem *mk_icmp_getall(int *sz) {
  *sz = sizeof icmp_all / sizeof icmp_all[0];
  return (void *)icmp_all;
}


