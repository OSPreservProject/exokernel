
#include <xok/defs.h>
#include <xok/mmu.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>

#include <vos/uinfo.h>
#include <vos/kprintf.h>
#include <vos/proc.h>
#include <vos/vm-layout.h>
#include <vos/vm.h>
#include <vos/errno.h>
#include <vos/net/iptable.h>
#include <vos/cap.h>

#include "def_if.h"


#define dprintf if (0) kprintf


/*
 * ifconfig_find_if: find an interface from the ifconfig table
 */
static int
ifconfig_find_if(char *name)
{
  int i;

  for(i=0; i < IFCONFIG_DEF_SZ; i++)
  {
    ifconfig_t* ifc = &def_ifconfig[i];
    if (strncmp(ifc->name,name,strlen(name))==0)
      return i;
  }

  return -1;
}


/*
 * if_init: initializes network interfaces: loopback is 0, and ethernet cards
 * starts from 1 
 */
static void 
if_init(void) 
{
  int nextinterface = 0;
  int i, c;
  if_entry_t* ife;

  ip_table->num_if_entries = 0;
  yieldlock_reset(&(ip_table->if_lock));
  ip_table->if_stamp = 0;

  /* ethernet cards */
  for (i=0; i<__sysinfo.si_nnetworks && nextinterface < IFTABLESZ; i++) 
  {
    ife = IPTABLE_GETIFP(nextinterface++);
    ip_table->num_if_entries++;

    bzero(ife,sizeof(if_entry_t));
    strcpy(ife->name,__sysinfo.si_networks[i].cardname);

    ife->cardno = i;		/* by convention */

    if ((c = ifconfig_find_if(ife->name)) < 0 ||
        !__sysinfo.si_networks[i].inited)
    {
      ife->flags = IF_DOWN;
      bcopy(__sysinfo.si_networks[i].ether_addr,ife->ethaddr, 6);
    }

    else
    {
      ife->flags = IF_UP;
      
      bcopy(__sysinfo.si_networks[i].ether_addr,ife->ethaddr, 6);
      bcopy(def_ifconfig[c].ipaddr,ife->ipaddr, 4);
      bcopy(def_ifconfig[c].netmask,ife->netmask, 4);
      ife->mtu = def_ifconfig[c].mtu;
      apply_netmask_net(ife->net, ife->ipaddr, ife->netmask);

      if (__sysinfo.si_networks[i].cardtype == XOKNET_LOOPBACK)
        ife->flags |= IF_LOOPBACK;

      // print_ife(ife);
    }
  }
}


/*
 * route_init: initialize the route table 
 */
static void
route_init(void) 
{
  int i;
  route_entry_t* routep;

  ip_table->num_route_entries = 0;
  yieldlock_reset(&(ip_table->route_lock));
  ip_table->route_stamp = 0;

  for (i = 0 ; i < ROUTETABLESZ; i++) 
  {
    routep = IPTABLE_GETROUTEP(i);
    bzero(routep,sizeof(route_entry_t));
  }
}


int 
iptable_alloc(void)
{
  int r;
  extern char *progname;

  if ((r = vm_alloc_region(IPTABLE_REGION, NBPG, CAP_USER, PG_U|PG_P|PG_W)) < 0)
  {
    printf("%s: cannot initialize if and route tables. "
	   "some network programs may not work!\n", progname);
    return -1;
  }
  
  StaticAssert(sizeof(ip_table_t) <= NBPG);
  r = va2ppn(IPTABLE_REGION);
  uinfo->iptable_where = r;
  
  dprintf("%s: iptable page allocated to physical page %d.\n", progname, r);

  ip_table->hostname[0] = 0;
  ip_table->domainname[0] = 0;
  bzero(ip_table,sizeof(ip_table_t));

  if_init();
  route_init();

  return 0;
}


