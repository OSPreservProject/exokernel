
/*
 * Copyright MIT 1999
 */

#ifndef __VOS_IPTABLE_H__
#define __VOS_IPTABLE_H__

#include <xok/network.h>
#include <vos/locks.h>
#include <vos/assert.h>


/* interface flags */
#define IF_FREE	     0x0
#define IF_UP        0x1
#define IF_DOWN      0x2
#define IF_LOOPBACK  0x4
#define IF_BROADCAST 0x8


/* other misc definitions */
#define IFNAMSZ      16
#define HOSTNAMELEN  64
#define ETHER_MTU    1500


typedef unsigned char ipaddr_t[4];
typedef unsigned char ethaddr_t[6];

extern char eth_bcast_addr[6];


#define IPADDR_BROADCAST ((ipaddr_t){255,255,255,255})
#define IPADDR_LOOPBACK ((ipaddr_t){127,0,0,1})
#define IPADDR_ANY ((ipaddr_t){0,0,0,0})


typedef struct metric 
{
  short bandwindth;
  short latency;
} metric_t;


typedef struct if_entry 
{
  unsigned int flags; 		/* FREE, UP, DOWN, LOOPBACK, BROADCAST */
  char name[IFNAMSZ];           /* i.e. de0, ed0, lo */
  int cardno;                   /* cardno */
  ethaddr_t ethaddr;            /* ethaddr cached here. */
  char pad[2];			/* padding to make ethaddr_t 8 */
  ipaddr_t ipaddr;              /* ipaddr of this interface */
  ipaddr_t netmask;             /* netmask */
  ipaddr_t net;			/* cached copy of: ipaddr & netmask */
  ipaddr_t broadcast;		/* broadcast to the subnet like 18.26.4.255 
				 * or 18.26.4.0 (depending on bcast style) */
  metric_t metric;		/* initially ignored, and using both styles */
  int mtu;			/* Maximum Transfer Unit, 1500 for Ethernet */
} if_entry_t;

#define IFRET_RESOLVE   0
#define IFRET_LOOPBACK  1
#define IFRET_NOTFOUND  2
#define IFRET_BROADCAST 3


typedef struct route_entry 
{
  int flags;			/* REJECT, BLACKHOLE */
  int bitcount;			/* (cached) number of 1's in the netmask */
  ipaddr_t net;			/* could be a host or a gateway */
  ipaddr_t netmask;
  ipaddr_t dst;			/* could be a host or a gateway */
  metric_t metric;		/* not currently used */
} route_entry_t;


#define IFTABLESZ    XOKNET_MAXNETS   /* localhost and ethernet cards */
#define ROUTETABLESZ 16


/*
 * sync: ip_table is rarely changed, so no need to heavily synchronize this.
 * The only thing that changes are ip address, mask, and mtu, all of them are
 * 32 bit quantities on 32 bit address boundaries. Therefore they are accessed
 * atomically (guaranteed by intel).
 */

typedef struct ip_table 
{
  char hostname[HOSTNAMELEN];
  char domainname[HOSTNAMELEN];
  int num_if_entries;		
  if_entry_t if_entries[IFTABLESZ];
  yieldlock_t if_lock;
  unsigned long if_stamp;	/* for synchronization */

  int num_route_entries;		
  route_entry_t route_entries[ROUTETABLESZ];
  yieldlock_t route_lock;
  unsigned long route_stamp;	/* for synchronization */
} ip_table_t;

extern ip_table_t *ip_table;

#define IPTABLE_GETIFP(i) (&(ip_table->if_entries[i]))
#define IPTABLE_GETROUTEP(i) (&(ip_table->route_entries[i]))


static inline void
print_ipaddr(unsigned char *ip_addr) 
{
  printf("%d.%d.%d.%d",ip_addr[0],ip_addr[1],ip_addr[2],ip_addr[3]);
}

static inline void
print_ethaddr(unsigned char *eth_addr) 
{
  printf("%02x:%02x:%02x:%02x:%02x:%02x",
	 eth_addr[0],eth_addr[1],eth_addr[2],
	 eth_addr[3],eth_addr[4],eth_addr[5]);
}

#define print_ife(ife) \
  { \
    printf("%s (%d, 0x%x)",(ife)->name,(ife)->cardno,(ife)->flags);\
    printf(" eth: "); print_ethaddr((ife)->ethaddr);\
    printf(" ip: "); print_ipaddr((ife)->ipaddr);\
    printf(" netmask: "); print_ipaddr((ife)->netmask); printf("\n"); \
  }


/* 
 * iptable_find_if_name: looks for an interface with given name. Returns the
 * interface number if found. Return -1 otherwise.
 */
extern int iptable_find_if_name(char *name);


/*
 * iptable_find_if_ipaddr: looks for an interface with given ipaddr. Returns
 * the interface number if found. Return -1 otherwise.
 */
extern int iptable_find_if_ipaddr(ipaddr_t ip);


/* 
 * iptable_find_if: looks for a netcard with the given flag. Returns the
 * interface number if found. Returns -1 otherwise. Uses an iteration counter
 * to do incremental search and loop around.
 */
extern int iptable_find_if(u_int flag, u_int *iter);


#define ASSERT_IFNUM(ifnum) \
assert(ifnum < ip_table->num_if_entries && ifnum >= 0)


/*
 * The following inlined functions return an attribute of the interface with
 * the given interface number.
 */

static inline void
if_get_ethernet(int ifnum, ethaddr_t ethaddr) 
{
  if_entry_t* ife;
  ASSERT_IFNUM(ifnum);
  ife = IPTABLE_GETIFP(ifnum);
  memcpy(ethaddr, ife->ethaddr, 6);
}

static inline void
if_get_ipaddr(int ifnum, ipaddr_t ipaddr) 
{
  if_entry_t* ife;
  ASSERT_IFNUM(ifnum);
  ife = IPTABLE_GETIFP(ifnum);
  
  /* XXX - need to change this to a 32 bit read */
  bcopy(ife->ipaddr,ipaddr,4); 
}

static inline void
if_get_netmask(int ifnum, ipaddr_t netmask) 
{
  if_entry_t* ife;
  ASSERT_IFNUM(ifnum);
  ife = IPTABLE_GETIFP(ifnum);
  
  /* XXX - need to change this to a 32 bit read */
  bcopy(ife->netmask,netmask,4); 
}

static inline void
if_get_broadcast(int ifnum, ipaddr_t broadcast) 
{
  if_entry_t* ife;
  ASSERT_IFNUM(ifnum);
  ife = IPTABLE_GETIFP(ifnum);
  
  /* XXX - need to change this to a 32 bit read */
  bcopy(ife->broadcast,broadcast,4); 
}

static inline char *
if_get_name(int ifnum) 
{
  if_entry_t* ife;
  ASSERT_IFNUM(ifnum);
  ife = IPTABLE_GETIFP(ifnum);
  return (char *)&ife->name[0];
}

static inline void
if_get_flags(int ifnum, int *flags) 
{
  if_entry_t* ife;
  ASSERT_IFNUM(ifnum);
  ife = IPTABLE_GETIFP(ifnum);
  *flags = ife->flags;
}

static inline int
if_get_cardno(int ifnum) 
{
  if_entry_t* ife;
  int cardno;
  ASSERT_IFNUM(ifnum);
  ife = IPTABLE_GETIFP(ifnum);
  cardno = ife->cardno;
  return cardno;
}

/*
 * apply_netmask_net: applies the netmask to the ip address to determine
 * network address.
 */
static inline void 
apply_netmask_net(ipaddr_t net, ipaddr_t src, ipaddr_t netmask) 
{
  net[0] = src[0] & netmask[0];
  net[1] = src[1] & netmask[1];
  net[2] = src[2] & netmask[2];
  net[3] = src[3] & netmask[3];
}


/*
 * apply_netmask_broadcast: applies the netmask to the ip address to determine
 * broadcast address.
 */
static inline void 
apply_netmask_broadcast(ipaddr_t broadcast, ipaddr_t src, ipaddr_t netmask) 
{
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


/* 
 * ipaddr_find_if: search for route on interface that can be used to reach the
 * ip address given. Returns IFRET_RESOLVE if found; IFRET_LOOPBACK if ip
 * address is a loopback address; IFRET_BROADCAST if interface is broadcast
 * interface; and IFRET_NOTFOUND if no appropriate interface found. The
 * interface number, if found, is stored in *ifnum  
 */
static inline int 
ipaddr_find_if(ipaddr_t ipaddr, int *ifnum) 
{
  if_entry_t* ife;
  ipaddr_t net;
  int i;

  for (i=0; i < ip_table->num_if_entries; i++) 
  {
    ife = IPTABLE_GETIFP(i);

    if (ife->flags & IF_DOWN) continue;

    apply_netmask_net(net, ipaddr, ife->netmask);

    if (memcmp(&net,ife->net,4) == 0) 
    {
      *ifnum = i;
      
      if (ife->flags & IF_BROADCAST &&
	  memcmp(ipaddr,ife->broadcast,4) == 0) 
      {
	return IFRET_BROADCAST;
      } 
      
      else if (ife->flags & IF_LOOPBACK) 
	return IFRET_LOOPBACK;

      else if (memcmp(ipaddr,ife->ipaddr,4) == 0)
	/* if it is our address, and we are not loopback reject */
	return IFRET_NOTFOUND;
      else
	return IFRET_RESOLVE;
    }
  }
  return IFRET_NOTFOUND;
}


/* 
 * ipaddr_find_route: search for a route to the ip address in the ip table.
 * Returns 0 if successful, with the route address copied to gateway. Returns
 * -1 otherwise.
 */
static inline int 
ipaddr_find_route(ipaddr_t ipaddr, ipaddr_t gateway) 
{
  route_entry_t* routep;
  ipaddr_t net;
  int i;
 
  for (i = 0 ; i < ip_table->num_route_entries; i++) 
  {
    routep = IPTABLE_GETROUTEP(i);
    apply_netmask_net(net, ipaddr, routep->netmask);
    
    if (memcmp(&net,routep->net,4) == 0) 
    {
      bcopy(routep->dst,gateway,4);
      return 0;
    }
  }
  return -1;
}


/* 
 * ipaddr_get_dsteth: get destination ethaddr from ip address. Returns -1 if
 * not found. Otherwise, address is copied into ethaddr. Interface number is
 * stored in *ifnum. 
 */
extern int ipaddr_get_dsteth(ipaddr_t dstaddr,ethaddr_t ethaddr,int *ifnum);


/* 
 * iptable_remap: remaps iptable writable. Returns 0 if successful. Returns -1
 * otherwise with errno set to V_NOMEM.
 */
extern int
iptable_remap();


#endif /* __VOS_IPTABLE_H__ */


