
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

#include <string.h>
#include <assert.h>
#include <exos/debug.h>
#include <exos/netinet/hosttable.h>
#include <exos/site-conf.h>

int 
eth_eq_addr(char const *a1, char const *a2)
{
    return ((a1[0] == a2[0]) && (a1[1] == a2[1]) && 
	    (a1[2] == a2[2]) && (a1[3] == a2[3]) &&
	    (a1[4] == a2[4]) && (a1[5] == a2[5]));
}


static inline int 
my_ip_eq_addr(char const *a1, char const *a2) {
    return ((a1[0] == a2[0]) && (a1[1] == a2[1]) && 
	    (a1[2] == a2[2]) && (a1[3] == a2[3]));
}


static char eth_temp[6];

unsigned char *
get_ether_from_ip(unsigned char const * ip_addr, int absolute)
{
    map_t *m = hosttable_map;
    static unsigned char localnet[3] = MY_NET_NUMBER;
    static unsigned char router[] = MY_GATEWAY;
    static unsigned char localhost[] = { 127,0,0,1 };

    assert(ip_addr);
    if (!absolute) {
       if (memcmp(ip_addr,localhost,4) == 0) {
	  /* localhost */
          bzero (eth_temp, 6);
	  return &eth_temp[0];
       }
       if (memcmp(ip_addr,localnet,3) != 0) {
	  /* not on local subnet */
	  ip_addr = (char *)&router;
       }
    }


    while(m->name[0] != 0) {
	if(my_ip_eq_addr(ip_addr, m->ip_addr)) {
	    return(m->eth_addr);
	}
	m++;
    }

    kprintf("ip: %08x\n",(*((int *)ip_addr)));
    demand(0,unknown ip add it to hosttable.c);
    return 0;

}

unsigned char const *
get_ip_from_ether(unsigned char const * eth_addr)
{
  map_t *m = hosttable_map;

  assert(eth_addr);

  while(m->name[0] != 0) {
    if(eth_eq_addr(eth_addr, m->eth_addr)) {
      return(m->ip_addr);
    }
    m++;
  }

  
  kprintf("could not find ip for ethernet address:");
  kprintf("%2x %2x %2x %2x %2x %2x\n",
	  eth_addr[0],eth_addr[1],eth_addr[2],
	  eth_addr[3],eth_addr[4],eth_addr[5]);
  
  demand(0,unknown ip for a given ethernet add it to hosttable.c);
  return 0;

}
