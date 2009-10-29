
#ifndef __VOS_INIT_IFCONFIG_H__
#define __VOS_INIT_IFCONFIG_H__

#include <xok/types.h>
#include <vos/net/iptable.h>

typedef struct
{
  char *name;
  ipaddr_t ipaddr;
  ipaddr_t netmask;
  u_short mtu;
} ifconfig_t;

#define IFCONFIG_DEF_SZ 2
ifconfig_t def_ifconfig[IFCONFIG_DEF_SZ] = 
{
 /* lo0 */ {"lo0", {127,0,0,1}, {255,0,0,0}, 3924},
 /* ed0 */ {"ed0", {18,26,4,98}, {255,255,255,0}, 1500}
};


#endif

