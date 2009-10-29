
/* send arp requests */

#include <xok/sys_ucall.h>

#include <vos/net/fast_eth.h>
#include <vos/net/fast_arp.h>
#include <vos/net/iptable.h>
#include <vos/net/arptable.h>
#include <vos/net/ae_ether.h>

#include <vos/proc.h>
#include <vos/wk.h>
#include <vos/kprintf.h>


#define dprintf if(0) kprintf
#define dprint_ipaddr if(0) print_ipaddr

int 
arp_send_request(unsigned char ip_addr[4], int ifnum) 
{
  struct arp *arp;
  struct eth *eth;
  struct arprarp *ea;
  int retran = 0;
  static char arp_packet_query[sizeof (struct arprarp)];

  dprintf ("arp_send_request\n");

  ea = (struct arprarp *)&arp_packet_query;
  arp = &ea->arp;
  eth = &ea->eth;

  memcpy(eth->dst_addr, eth_bcast_addr, sizeof eth->dst_addr);
  if_get_ethernet(ifnum,eth->src_addr );

  eth->proto = htons(EP_ARP);
  arp->arp_hrd = htons(1);
  arp->arp_pro = htons(EP_IP);
  arp->arp_hln = 6;
  arp->arp_pln = 4;
  arp->arp_op = htons(ARP_REQ);

  if_get_ethernet(ifnum,(char *)&arp->arp_sha);
  if_get_ipaddr(ifnum,(char *)&arp->arp_spa);

  memset(&arp->arp_tha, 0, sizeof(arp->arp_sha));
  memcpy(&arp->arp_tpa, ip_addr, sizeof(arp->arp_spa));

  dprintf("ARP: querying for Ether of IP: ");
  dprint_ipaddr(ip_addr);
  dprintf("\n");

  while(ae_eth_send(ea, sizeof(struct arprarp), if_get_cardno(ifnum)) < 0) 
  {
    int i;
    printf("arp: retry arp request after 1 seconds or so\n");
    retran++;
    
    for(i=0; i<1000000; i++)
      sys_null();
  }

  return 0;
}

int
arp_resolve_ip(ipaddr_t ip, ethaddr_t eth, int ifnum)
{
  int r;
  extern int arptable_add_timedout_entry(ipaddr_t ip);
  u_int timeout = SECONDS2TICKS(ARP_RETRANS_TIMEOUT);
  u_short retry = 0;

  while((r = arptable_resolve_ip(ip, eth)) < 0)
  {
    if (retry >= ARP_RETRANS)
    {
      printf("arp request retransmit timedout!\n");
      arptable_add_timedout_entry(ip);
      return -1;
    }

    arp_send_request(ip, ifnum);
    retry++;
    dprintf("arp retransmit %d!\n", retry);

    wk_waitfor_timeout(0L, 0, 1, -1);
    errno = 0;

    if ((r = arptable_resolve_ip(ip, eth)) < 0)
    {
      dprintf("need to wait longer!\n");
      wk_waitfor_timeout(0L, 0, timeout, -1);
      errno = 0;
    }
    else
      break;
  }

  if (r == RESOLVED)
    return 0;
  else
    return -1;
}


