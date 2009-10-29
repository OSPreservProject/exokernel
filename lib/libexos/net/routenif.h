
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

#include <sys/param.h>
#include <xok/sysinfo.h>

#include <exos/netinet/in.h>
#include <exos/net/if.h>
#include <exos/net/route.h>
#include "arp_table.h"
typedef struct metric {
  /* anything you want */
  short bandwindth;
  short latency;
} metric_t;

typedef struct if_entry {
  /* if flags == 0, then entry is free. */
  unsigned int flags;			/* FREE, UP, DOWN MULTICAST(maybe) */
  char name[IFNAMSZ];		/* i.e. de0, ed0, lo */
  int cardno;			/* cardno */
  ethaddr_t ethaddr;		/* ethaddr cached here. */
  ipaddr_t ipaddr;		/* ipaddr of this interface */
  ipaddr_t netmask;		/* netmask */
  ipaddr_t net;		        /* cached copy of: ipaddr & netmask */
  ipaddr_t broadcast;		/* broadcast to the subnet like 18.26.4.255 
				 * or 18.26.4.0 (depending on bcast style) */
  metric_t metric;		/* initially ignored, and using both styles */
  int mtu;			/* Maximum Transfer Unit, 1500 for Ethernet */
} if_entry_t, *if_entry_p;


#define IFRET_RESOLVE   0
#define IFRET_LOOPBACK  1
#define IFRET_NOTFOUND  2
#define IFRET_BROADCAST 3



/* Default routes use netmask of 0.0.0.0 and net of 0.0.0.0, and dst of gateway */
/* Host routes use netmask of 255.255.255.255 and net hostaddres and dst of gateway 
   to host */
/* INVARIANT: Gateways should be reachable via one of the interfaces. */
typedef struct route_entry {
  int flags;			/* REJECT, BLACKHOLE */
  int bitcount;			/* (cached) number of 1's in the netmask */
  ipaddr_t net;			/* could be a host or a gateway */
  ipaddr_t netmask;
  ipaddr_t dst;			/* could be a host or a gateway */
  metric_t metric;		/* not currently used */
} route_entry_t, *route_entry_p;

/*
 * Routes are sorted by the number of 0's in their netmask.  That is the
 * first routes in the table are the most specific (because they contain most 1's
 * the default route (with netmask 0.0.0.0), will be the last in the table 
 * This algorithm assumes contiguous 1's on the netmask, and has not be analysed 
 * for other kinds of netmasks.
 * everytime and entry is deleted or added to the table, the whole table is 
 * sorted again.
 * 
 * the routing algorithm works as follows in the common case:
 * you give me an IP address, and you want find ethernet address of destination 
 * and interface to send it through.
 * first we check the interfaces, we mask the IP address with the netmask of each
 * interface, if that matches the network of the interface. we know the interface
 * by which we can send the packet.  Then we use arp to resolve the address.
 * if that fails, we go by each entry in the routing table and compare its network
 * with the IP address masked with its mask, if they match, then we know
 * that we have to use the dst address.  The interface for this destination must be
 * computable by the first half of this algorithm.
 */

#define IFTABLESZ (XOKNET_MAXNETS)  /* localhost and ethernet cards */
#define ROUTETABLESZ (16)

typedef struct ip_table {
  char hostname[MAXHOSTNAMELEN];
  char domainname[MAXHOSTNAMELEN];
  /* number of interfaces set at initialization time, if we later allow virtual 
   interfaces (multihome), then they must be packed everytime they are 
   deleted or added
   */
  int num_if_entries;		
  if_entry_t if_entries[IFTABLESZ];
  /* routes are sorted (and compacted) by their netmask length */
  int num_route_entries;		
  route_entry_t route_entries[ROUTETABLESZ];
} ip_table_t, *ip_table_p;

#define GETIFP(i) &ip_table->if_entries[i] 
#define GETROUTEP(i) &ip_table->route_entries[i] 

#define GETNUMIFENTRIES (ip_table->num_if_entries)
#define INCNUMIFENTRIES (ip_table->num_if_entries++)
#define DECNUMIFENTRIES (ip_table->num_if_entries--)
#define ZERONUMIFENTRIES (ip_table->num_if_entries = 0)
#define SETNUMIFENTRIES(i) (ip_table->num_if_entries = i)

#define GETNUMROUTEENTRIES (ip_table->num_route_entries)
#define INCNUMROUTEENTRIES (ip_table->num_route_entries++)
#define DECNUMROUTEENTRIES (ip_table->num_route_entries--)
#define ZERONUMROUTEENTRIES (ip_table->num_route_entries = 0)
#define SETNUMROUTEENTRIES(i) (ip_table->num_route_entries = i)


extern ip_table_p ip_table;







 

