
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


#include <exos/netinet/in.h>

/* Interface Flags */
#define IF_UP        0x1
#define IF_DOWN      0x2
#define IF_LOOPBACK  0x4
#define IF_BROADCAST 0x8

/* max size of interface names */
#define IFNAMSZ        16

/* Get information from Interface */
int  get_ifnum_by_name(char *name);
char *get_ifnum_name(int ifnum);
int  get_ifnum_cardno(int ifnum);
void get_ifnum_flags(int ifnum, int *flags);
void get_ifnum_ethernet(int ifnum, ethaddr_t ethaddr);
void get_ifnum_ipaddr(int ifnum, ipaddr_t ipaddr);
void get_ifnum_netmask(int ifnum, ipaddr_t netmask);
void get_ifnum_broadcast(int ifnum, ipaddr_t broadcast);

/* Configure Interfaces */
int  ifconfig(char *name, int flags, 
	      ipaddr_t ipaddr, ipaddr_t netmask, ipaddr_t broadcast);

/* Get destination */
int  get_dsteth(ipaddr_t dstaddr,ethaddr_t ethaddr,int *ifnum);

/* Iterators */
void init_iter_ifnum(void);
int  iter_ifnum(unsigned int flags);

/* hostname */
int if_sethostname(char *name, int namelen);
int if_gethostname(char *name, int namelen);

/* domainname */
int if_setdomainname(char *name, int namelen);
int if_getdomainname(char *name, int namelen);

/* debugging */
void if_show(void);

/* Common Functions */
static inline void 
apply_netmask(ipaddr_t net, ipaddr_t src, ipaddr_t netmask) {
  net[0] = src[0] & netmask[0];
  net[1] = src[1] & netmask[1];
  net[2] = src[2] & netmask[2];
  net[3] = src[3] & netmask[3];
}

static inline void 
apply_netmask_broadcast(ipaddr_t broadcast, ipaddr_t src, ipaddr_t netmask) {
  /* apply netmask */
  broadcast[0] = src[0] & netmask[0];
  broadcast[1] = src[1] & netmask[1];
  broadcast[2] = src[2] & netmask[2];
  broadcast[3] = src[3] & netmask[3];
  /* add broadcast */
  broadcast[0] |= (0xff & ~netmask[0]);
  broadcast[1] |= (0xff & ~netmask[1]);
  broadcast[2] |= (0xff & ~netmask[2]);
  broadcast[3] |= (0xff & ~netmask[3]);
  
}
