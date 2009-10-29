
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


#ifndef _ETHER_H_
#define _ETHER_H_

#include <xok/types.h>

#define ETHER_ADDR_LEN	6
#define ETHER_MIN_LEN	64
#define ETHER_MAX_LEN	1514

#define ETHERTYPE_PUP 0x0200        /* PUP protocol */
#define ETHERTYPE_IP 0x0800         /* IP protocol */
#define ETHERTYPE_ARP 0x0806        /* address resolution protocol */
#define ETHERTYPE_REVARP 0x8035     /* reverse addr resolution protocol */

struct ether_pkt {
  u_char ether_dhost[ETHER_ADDR_LEN];
  u_char ether_shost[ETHER_ADDR_LEN];
  u_short ether_type;
  u_char _ether_payload[0];
};

#define ether_header ether_pkt

#define ether_ip(e) ((struct ip_pkt *) (e)->_ether_payload)

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
static inline char * ether_sprintf(ap)
register u_char *ap;
{
   register int i;                                                        
   static char etherbuf[18];                
   register char *cp = etherbuf;                                
 
   for (i = 0; i < 6; i++) {                                         
      *cp++ = digits[*ap >> 4];                                  
      *cp++ = digits[*ap++ & 0xf];    
      *cp++ = ':';
   }                                                                 
   *--cp = 0;                                                      
   return (etherbuf);                                                   
}


#endif /* _ETHER_H_ */
