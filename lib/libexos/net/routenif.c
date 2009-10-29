
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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h> 
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "fd/proc.h"

#include "routenif.h"

#include <exos/vm-layout.h>
#include <exos/critical.h>

#include <sys/mman.h>

#define ETHER_MTU 1500		/* like if it was going to change */

ip_table_p ip_table = (ip_table_p)0;

static void ip_init_table();
static void if_init(void);
static void route_init(void);
static void print_if_entry(if_entry_p ife);


static void mk_ip_table_ro(void) {
  int status;
  status = mprotect((char *)IP_SHARED_REGION, IP_SHARED_REGION_SZ,PROT_READ);
  if (status != 0) printf("mprotect %p:%d ro failed\n",
			  (char *)IP_SHARED_REGION, IP_SHARED_REGION_SZ);
}

static void mk_ip_table_rw(void) {
  int status;
  status = mprotect((char *)IP_SHARED_REGION, IP_SHARED_REGION_SZ,PROT_READ|PROT_WRITE);
  if (status != 0) printf("mprotect %p:%d rw failed\n",
			  (char *)IP_SHARED_REGION, IP_SHARED_REGION_SZ);
}


int
ip_init() {
  int status;

  status = fd_shm_alloc(IP_SHM_OFFSET,
			sizeof(ip_table_t),
			(char *)IP_SHARED_REGION);
  ip_table = (ip_table_p)IP_SHARED_REGION;

  StaticAssert((sizeof(ip_table_t)) <= IP_SHARED_REGION_SZ);

  if (status == -1) {
    demand(0, problems attaching shm);
    return -1;
  }

  if (status) {
    ip_init_table();
  } 
  mk_ip_table_ro();
  return 0;

}

static void
ip_init_table() {
  ip_table->hostname[0] = 0;
  ip_table->domainname[0] = 0;
  bzero(ip_table,sizeof(ip_table_t));

  if_init();
  route_init();

}

static void 
if_init(void) {
  int nextinterface = 0;
  int i;
  if_entry_p ife;

  ZERONUMIFENTRIES;

#if 0	/* GROK -- not sure that making loopback look like ethernet is     */
	/* a good plan.  However, that's the way it works currently, so... */
  assert(nextinterface < IFTABLESZ);
  ife = GETIFP(nextinterface++);
  INCNUMIFENTRIES;
  /* initialize loopback */
  bzero(ife,sizeof(if_entry_t));
  strcpy(ife->name, "lo0");
  ife->cardno = 0;		/* by convention */
  ife->flags = IF_DOWN;
#endif
  
  /* Ethernet cards */
  for (i=0; i<__sysinfo.si_nnetworks; i++) {
    assert(nextinterface < IFTABLESZ);
    ife = GETIFP(nextinterface++);
    INCNUMIFENTRIES;

    bzero(ife,sizeof(if_entry_t));
    sprintf(ife->name,"%s",__sysinfo.si_networks[i].cardname);
    ife->cardno = i;		/* by convention */
    ife->flags = IF_DOWN;
    bcopy(__sysinfo.si_networks[i].ether_addr,ife->ethaddr, 6);
    ife->mtu = ETHER_MTU;
  }
}

static void
route_init(void) {
  int i;
  route_entry_p routep;
  ZERONUMROUTEENTRIES;

  for (i = 0 ; i < ROUTETABLESZ; i++) {
    routep = GETROUTEP(i);
    bzero(routep,sizeof(route_entry_t));
  }
}
/* INTERFACE ROUTINES */
int
get_ifnum_by_name(char *name) {
  /* names like lo0, ed0, de0 */
  if_entry_p ife;
  int i;
  for (i = 0 ; i < GETNUMIFENTRIES; i++) {
    ife = GETIFP(i);
    if (strcmp(ife->name, name) == 0) {
      return i;
    }
  }
  return -1;

}

static if_entry_p 
get_if_by_name(char *name) {
  /* names like lo0, ed0, de0 */
  int i;
  if ((i = get_ifnum_by_name(name)) != -1)
    return GETIFP(i);
  else 
    return (if_entry_p)0;
}

#if 0
static if_entry_p
get_if_by_cardno(int cardno) {
  if_entry_p ife;
  int i;
  for (i = 0 ; i < GETNUMIFENTRIES; i++) {
    ife = GETIFP(i);
    if (ife->cardno == cardno) {
      return ife;
    }
  }
  return (if_entry_p)0;
}
#endif

static void
print_if_entry(if_entry_p ife) {
  int notfirst = 0;
  printf("%s: flags=%x<",
	 ife->name, 
	 ife->flags);
  if (ife->flags & IF_UP) printf("%sUP",(notfirst) ? "," : (notfirst++,""));
  if (ife->flags & IF_DOWN) printf("%sDOWN",(notfirst) ? "," : (notfirst++,""));
  if (ife->flags & IF_LOOPBACK) printf("%sLOOPBACK",(notfirst) ? "," : (notfirst++,""));
  if (ife->flags & IF_BROADCAST) printf("%sBROADCAST",(notfirst) ? "," : (notfirst++,""));
  printf("> cardno: %d\n",ife->cardno);
  if (ife->flags & IF_UP) {
    printf("     inet %s",pripaddr(ife->ipaddr));
    printf(" netmask %s",pripaddr(ife->netmask));
    if (ife->flags & IF_BROADCAST)
      printf(" bcast %s",pripaddr(ife->broadcast));
    printf("\n");
  } 
}
/* ROUTE INTERFACES */
route_entry_p
get_route(ipaddr_t net, ipaddr_t netmask) {
  int i;
  route_entry_p routep;
  for (i = 0 ; i < GETNUMROUTEENTRIES; i++) {
    routep = GETROUTEP(i);
    if (bcmp(net,routep->net,4) == 0 &&
	bcmp(netmask,routep->netmask,4) == 0)
      return routep;
  }
  return (route_entry_p)0;
}
int
get_routen(ipaddr_t net, ipaddr_t netmask) {
  int i;
  route_entry_p routep;
  for (i = 0 ; i < GETNUMROUTEENTRIES; i++) {
    routep = GETROUTEP(i);
    if (bcmp(net,routep->net,4) == 0 &&
	bcmp(netmask,routep->netmask,4) == 0)
      return i;
  }
  return -1;
}
void
print_route(route_entry_p routep) {
  if (routep == NULL) {
    printf("%-15s %-19s %-15s %s\n","Network","Netmask","Destination","Flags");
  } else {
    printf("%-15s ",pripaddr(routep->net));
    printf("%-15s/%3d ",pripaddr(routep->netmask),routep->bitcount);
    printf("%-15s ",pripaddr(routep->dst));
    if (routep->flags & ROUTE_INET) printf("I");
    if (routep->flags & ROUTE_REJECT) printf("R");
    if (routep->flags & ROUTE_BLACKHOLE) printf("B");
    printf("\n");
  }
}

/* SEARCHING ROUTINES */
/* searches only on interfaces */
int 
find_if(ipaddr_t ipaddr, int *ifnum) {
  if_entry_p ife;
  ipaddr_t net;
  int i;
  for (i = 0 ; i < GETNUMIFENTRIES; i++) {
    ife = GETIFP(i);
    if (ife->flags & IF_DOWN) continue;
    apply_netmask(net, ipaddr, ife->netmask);
    if (bcmp(&net,ife->net,4) == 0) {
      *ifnum = i;
      if (ife->flags & IF_BROADCAST &&
	  bcmp(ipaddr,ife->broadcast,4) == 0) {
	return IFRET_BROADCAST;
      } else if (ife->flags & IF_LOOPBACK) 
	return IFRET_LOOPBACK;
      else if (bcmp(ipaddr,ife->ipaddr,4) == 0)
	/* if it is our address, and we are not loopback reject */
	return IFRET_NOTFOUND;
      else
	return IFRET_RESOLVE;
    }
  }
  return IFRET_NOTFOUND;
}
/* searches only on routes */
int 
find_route(ipaddr_t ipaddr, ipaddr_t gateway) {
  route_entry_p routep;
  ipaddr_t net;
  int i;
  for (i = 0 ; i < GETNUMROUTEENTRIES; i++) {
    routep = GETROUTEP(i);
    apply_netmask(net, ipaddr, routep->netmask);
    if (bcmp(&net,routep->net,4) == 0) {
      bcopy(routep->dst,gateway,4);
      return 0;
    }
  }
  return -1;
}

/* IMPORTANT CALLS */
int
get_dsteth(ipaddr_t dstaddr,ethaddr_t ethaddr,int *ifnum) {
  /* first search interfaces */
  ipaddr_t gateway;

  switch(find_if(dstaddr,ifnum)) {
  case IFRET_LOOPBACK:
    //printf("Directly on interface (loopback)\n");
    bzero(ethaddr,6); 
    return 0;
  case IFRET_RESOLVE:
    //printf("Directly on interface\n");
    return arp_resolve_ip(dstaddr,ethaddr,*ifnum);
  case IFRET_BROADCAST:
    //printf("Broadcast\n");
    memset(ethaddr,0xff,6);
    return 0;
  case IFRET_NOTFOUND:
    /* fall through */
  }
  if (find_route(dstaddr,gateway) == 0) {
    //printf("Via gateway: %s\n",pripaddr(gateway));
    switch(find_if(gateway,ifnum)) {
    case IFRET_LOOPBACK:
      bzero(ethaddr,6); 
      return 0;
    case IFRET_RESOLVE:
      //printf("Directly on interface\n");
      return arp_resolve_ip(gateway,ethaddr,*ifnum);
    case IFRET_BROADCAST:
      //printf("Broadcast\n");
      memset(ethaddr,0xff,6);
      return 0;
    case IFRET_NOTFOUND:
      /* fall through */
    }
  }
    
  printf("get_dsteth: could not find route to destination: %s\n",pripaddr(dstaddr));
  return -1;
}

#define ASSERTIFNUM(ifnum)				\
  EnterCritical(); 					\
  assert(ifnum < GETNUMIFENTRIES && ifnum >= 0);	\
  ExitCritical();

void
get_ifnum_ethernet(int ifnum, ethaddr_t ethaddr) {
  if_entry_p ife;
  ASSERTIFNUM(ifnum);
  EnterCritical();
  ife = GETIFP(ifnum);
  bcopy(ife->ethaddr,ethaddr, 6);
  ExitCritical();
}

void
get_ifnum_ipaddr(int ifnum, ipaddr_t ipaddr) {
  if_entry_p ife;
  ASSERTIFNUM(ifnum);
  EnterCritical();
  ife = GETIFP(ifnum);
  bcopy(ife->ipaddr,ipaddr, 4);
  ExitCritical();
}

void
get_ifnum_netmask(int ifnum, ipaddr_t netmask) {
  if_entry_p ife;
  ASSERTIFNUM(ifnum);
  EnterCritical();
  ife = GETIFP(ifnum);
  bcopy(ife->netmask,netmask, 4);
  ExitCritical();
}

void
get_ifnum_broadcast(int ifnum, ipaddr_t broadcast) {
  if_entry_p ife;
  ASSERTIFNUM(ifnum);
  EnterCritical();
  ife = GETIFP(ifnum);
  bcopy(ife->broadcast,broadcast, 4);
  ExitCritical();
}

char *
get_ifnum_name(int ifnum) {
  if_entry_p ife;
  ASSERTIFNUM(ifnum);
  ife = GETIFP(ifnum);
  return (char *)&ife->name[0];
}

void
get_ifnum_flags(int ifnum, int *flags) {
  if_entry_p ife;
  ASSERTIFNUM(ifnum);
  EnterCritical();
  ife = GETIFP(ifnum);
  *flags = ife->flags;
  ExitCritical();
}

int
get_ifnum_cardno(int ifnum) {
  if_entry_p ife;
  int cardno;
  ASSERTIFNUM(ifnum);
  EnterCritical();
  ife = GETIFP(ifnum);
  cardno = ife->cardno;
  ExitCritical();
  return cardno;
}

static int bcast_iter;
void
init_iter_ifnum_broadcastifs(void) {bcast_iter = 0;}
int
iter_ifnum_broadcastifs(void) {
  if_entry_p ife;
  EnterCritical();
  for(;;) {
    if (bcast_iter >= GETNUMIFENTRIES || bcast_iter < 0) {
      ExitCritical();
      return -1;
    }
    ife = GETIFP(bcast_iter);
    if (ife->flags & IF_BROADCAST && ife->flags & IF_UP) {
      ExitCritical();
      return bcast_iter++;
    }
    bcast_iter++;
  }
}

/* if flags are 0, iterate over all interfaces. */
static int ifnum_iter;
void
init_iter_ifnum(void) {ifnum_iter = 0;}
int
iter_ifnum(unsigned int flags) {
  if_entry_p ife;
  EnterCritical();
  for(;;) {
    if (ifnum_iter >= GETNUMIFENTRIES || ifnum_iter < 0) {
      ExitCritical();
      return -1;
    }
    if (flags) {
      ife = GETIFP(ifnum_iter);
      if ((ife->flags & flags) == flags) {
	ExitCritical();
	return ifnum_iter++;
      }
    } else {
      ExitCritical();
      return ifnum_iter++;
    }
    ifnum_iter++;
  }
}

/* INTERFACE MANAGEMENT */
int
ifconfig(char *name, 
	 int flags, 
	 ipaddr_t ipaddr, 
	 ipaddr_t netmask, 
	 ipaddr_t broadcast) {
  if_entry_p ife;
  int ret;
  mk_ip_table_rw();
  EnterCritical();
  if ((ife = get_if_by_name(name))) {
    bcopy(ipaddr, ife->ipaddr, 4);
    bcopy(netmask, ife->netmask, 4);
    bcopy(broadcast, ife->broadcast, 4);
    apply_netmask(ife->net, ipaddr, netmask);
    ife->flags = flags;
    ret = 0;
  } else {
    fprintf(stderr,"ifconfig: Device not found: %s\n",name);
    ret = -1;
  }
  ExitCritical();
  mk_ip_table_ro();
  return ret;
}
void 
if_show(void) {
  int i;
  if_entry_p ife;
  for (i = 0 ; i < GETNUMIFENTRIES; i++) {
    ife = GETIFP(i);
    print_if_entry(ife);
  }
  
}
/* ROUTE MANAGEMENT */
/* with route_{add,delete,exists} we can easily implement route_change  */

int
route_change(int flags, ipaddr_t net, ipaddr_t netmask, ipaddr_t dst) {
  route_entry_p routep;
  
  routep = get_route(net,netmask);
  if (routep == 0) {
    /* no route exists */
    fprintf(stderr,"No route exists\n");
    return -1;
  }

  mk_ip_table_rw();
  EnterCritical();
  bcopy(dst,routep->dst,4);
  routep->flags = flags;
  ExitCritical();
  mk_ip_table_ro();
  return 0;
}
/* The algorithm for inserting and deleting entries could be made more efficient 
 * by taking advantage of the fact that entries are always sorted. 
 * for simplicity, we insert entries at the end of table, we delete entries
 * from their location and copy the entry at the end of the table, and then resort 
 * the table. */
static inline int bitcount(unsigned char *a) {
  int i = 0;
  if (0x80 & *a) i++;  if (0x40 & *a) i++;  if (0x20 & *a) i++;  if (0x10 & *a) i++;
  if (0x08 & *a) i++;  if (0x04 & *a) i++;  if (0x02 & *a) i++;  if (0x01 & *a) i++;
  return i;
}
static inline int get_ipaddr_bitcount(ipaddr_t ip) {
  return bitcount(&ip[0]) + bitcount(&ip[1]) + bitcount(&ip[2]) + bitcount(&ip[3]);
}

int
route_add(int flags, ipaddr_t net, ipaddr_t netmask, ipaddr_t dst) {
  route_entry_p routep;
  int bitcount;
  int i,location,ret;

  mk_ip_table_rw();
  EnterCritical();
  routep = get_route(net,netmask);
  if (routep) {
    /* route exists */
    fprintf(stderr,"Route exists\n");
    ret = -1;
  } else if (GETNUMROUTEENTRIES == ROUTETABLESZ) {
    /* no space left */
    fprintf(stderr,"No space for new route\n");
    ret = -1;
  } else {
    bitcount = get_ipaddr_bitcount(netmask);
    /* search for place of insertion: first entry that is smaller, and take previous */
    for (i = 0 ; i < GETNUMROUTEENTRIES ; i++) {
      routep = GETROUTEP(i);
      if (bitcount >= routep->bitcount) 
	break;
    }
    if (i == GETNUMROUTEENTRIES) {
      /* there were no entries, append at bottom */
      location = GETNUMROUTEENTRIES;
    } else {
      location = i;
      /* shift down all entries, to make space */
      for (i = GETNUMROUTEENTRIES; i >= location ; i--) {
	routep = GETROUTEP(i);
	*routep = *(GETROUTEP(i-1));
      }
    }
    
    routep = GETROUTEP(location);
    INCNUMROUTEENTRIES;
    routep->flags = flags;
    routep->bitcount = bitcount;
    bcopy(net, routep->net,4);
    bcopy(netmask, routep->netmask,4);
    bcopy(dst, routep->dst,4);
    ret = 0;
  }
  ExitCritical();
  mk_ip_table_ro();
  return ret;
}
int
route_delete(ipaddr_t net, ipaddr_t netmask) {
  int i,ret;
  route_entry_p routep;

  mk_ip_table_rw();
  EnterCritical();
  i = get_routen(net,netmask);
  if (i == -1) {
    /* not found */
    fprintf(stderr,"Route not found\n");
    ret = -1;
  } else if (i == GETNUMROUTEENTRIES - 1) {
    /* last route */
    DECNUMROUTEENTRIES;
    ret = 0;
  } else {
    /* shift up all entries */
    for (  ; i < GETNUMROUTEENTRIES ; i++) {
      routep = GETROUTEP(i);
      *routep = *(GETROUTEP(i + 1));
    }
    DECNUMROUTEENTRIES;
    ret = 0;
  }
  ExitCritical();
  mk_ip_table_ro();
  return ret;

}

void
route_show(void) {
  int i;
  print_route(NULL);
  EnterCritical();
  for (i = 0 ; i < GETNUMROUTEENTRIES ; i++) {
    print_route(GETROUTEP(i));
  }
  ExitCritical();
}
void
route_flush(void) {
  mk_ip_table_rw();
  EnterCritical();
  ZERONUMROUTEENTRIES;
  ExitCritical();
  mk_ip_table_ro();
}




/* hostname */
int if_sethostname(char *name, int namelen) {
  int len;

  mk_ip_table_rw();
  len = MIN(namelen, MAXHOSTNAMELEN-1);
  strncpy(ip_table->hostname,name,len);
  ip_table->hostname[len] = 0;
  mk_ip_table_ro();
  return 0;
}
int if_gethostname(char *name, int namelen) {
  strncpy(name,ip_table->hostname,namelen);
  return 0;
}

/* domainname */
int if_setdomainname(char *name, int namelen) {
  int len;

  mk_ip_table_rw();
  len  = MIN (namelen, MAXHOSTNAMELEN-1);
  strncpy(ip_table->domainname,name, len);
  ip_table->domainname[len] = 0;
  mk_ip_table_ro();
  return 0;
}
int if_getdomainname(char *name, int namelen) {
  strncpy(name,ip_table->domainname,namelen);
  return 0;
}
