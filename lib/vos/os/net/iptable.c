
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h> 
#include <unistd.h>

#include <xok/mmu.h>

#include <vos/net/iptable.h>
#include <vos/net/arptable.h>
#include <vos/assert.h>
#include <vos/vm-layout.h>
#include <vos/vm.h>
#include <vos/proc.h>
#include <vos/cap.h>
#include <vos/uinfo.h>
#include <vos/errno.h>


#define dprintf if (0) kprintf

#define printife(ife) \
dprintf("ife: no: %d, name: %s, flag: 0x%x\n",ife->cardno,ife->name,ife->flags);


ip_table_t *ip_table = (ip_table_t*) IPTABLE_REGION;
char eth_bcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


void
iptable_init() 
{
  Pte pte = (uinfo->iptable_where << PGSHIFT) | PG_U | PG_P | PG_SHARED;
  int r;

  dprintf("iptable_init: mapping %d onto 0x%x\n",pte>>PGSHIFT,IPTABLE_REGION);
  r = vm_self_insert_pte(CAP_USER, pte, IPTABLE_REGION, 0, 0);
  if (r < 0)
  {
    kprintf("iptable_init: cannot map ip_table!\n");
    exit(-1);
  }
}


int
ipaddr_get_dsteth(ipaddr_t dstaddr,ethaddr_t ethaddr,int *ifnum) 
{
  ipaddr_t gateway;

  switch(ipaddr_find_if(dstaddr,ifnum)) 
  {
    case IFRET_LOOPBACK:
      dprintf("ipaddr_get_dsteth: directly on interface (loopback)\n");
      bzero(ethaddr,6); 
      return 0;
    
    case IFRET_RESOLVE:
      dprintf("ipaddr_get_dsteth: directly on interface\n");
      return arp_resolve_ip(dstaddr, ethaddr, *ifnum);

    case IFRET_BROADCAST:
      dprintf("Broadcast\n");
      memset(ethaddr,0xff,6);
      return 0;
    
    case IFRET_NOTFOUND:
      /* fall through, go look in the route table */
  }

  if (ipaddr_find_route(dstaddr,gateway) == 0) 
  {
    switch(ipaddr_find_if(gateway,ifnum)) 
    {
      case IFRET_LOOPBACK:
        dprintf("ipaddr_get_dsteth: from route: loopback\n");
	bzero(ethaddr,6); 
	return 0;
     
      case IFRET_RESOLVE:
        dprintf("ipaddr_get_dsteth: from route: directly on interface\n");
        return arp_resolve_ip(dstaddr, ethaddr, *ifnum);
    
      case IFRET_BROADCAST:
        dprintf("ipaddr_get_dsteth: from route: broadcast\n");
        memset(ethaddr,0xff,6);
        return 0;
      
      case IFRET_NOTFOUND:
	/* fall through */
    }
  }
    
  dprintf("ipaddr_get_dsteth: could not find route to destination!\n");
  return -1;
}


int 
iptable_remap(void)
{
  Pte pte = (uinfo->iptable_where << PGSHIFT) | PG_U | PG_P | PG_W;
  int r;

  r = vm_self_insert_pte(CAP_USER, pte, IPTABLE_REGION, 0, 0);
 
  if (r < 0)
    RETERR(V_NOMEM);
  return 0;
}



int
iptable_find_if_name(char *name)
{
  if_entry_t *ife;
  int i;

  for(i = 0; i < ip_table->num_if_entries; i++)
  {
    ife = IPTABLE_GETIFP(i);
    if (strncmp(ife->name,name,strlen(ife->name))==0)
      return i;
  }

  return -1;
}


int
iptable_find_if_ipaddr(ipaddr_t ip)
{
  if_entry_t *ife;
  int i;

  for(i = 0; i < ip_table->num_if_entries; i++)
  {
    ife = IPTABLE_GETIFP(i);
    if (bcmp(ife->ipaddr,ip,sizeof(ipaddr_t))==0)
      return i;
  }

  return -1;
}


int
iptable_find_if(u_int flag, u_int *iter)
{
  if_entry_t* ife;

  if (*iter >= ip_table->num_if_entries || *iter < 0)
    return -1;

  while(*iter < ip_table->num_if_entries)
  {
    if (flag)
    {
      ife = IPTABLE_GETIFP(*iter);
      if ((ife->flags & flag) == flag)
        return (*iter)++;

      (*iter)++;
    }

    else
      return (*iter)++;
  }
  return -1;
}

