
#ifndef __VOS_ARP_TABLE_H__
#define __VOS_ARP_TABLE_H__

#include <xok/sysinfo.h>
#include <xok/types.h>
#include <vos/locks.h>
#include <vos/net/fast_arp.h>

#define SECONDS2TICKS(secs) ((secs * 1000000) / __sysinfo.si_rate)
#define ARP_TIMEOUT 120 	/* 2 minutes */
#define ARP_RETRANS_TIMEOUT 1   /* 1 second retransmission timeout */
#define ARP_RETRANS 5		/* retransmit 5 times */


enum arp_entry_status {FREE, RESOLVED, TIMEDOUT};


typedef long long arp_tick_t;


typedef struct arp_entry 
{
  enum arp_entry_status status;
  u_char  ip[4];
  u_char  eth[6];
  u_char  retrans;
  arp_tick_t expire;		/* ticks at which point entry times out */
  unsigned long stamp;     	/* used for read-only synchronization */
} arp_entry_t;


typedef struct arp_table 
{
  u_int owner_envid;
  yieldlock_t lock;
  arp_entry_t entries[ARP_TABLE_SIZE];
  u_int ipc_taskid;

} arp_table_t;

extern arp_table_t* arp_table;

#define ARPTABLE_ENVID (arp_table->owner_envid)
#define ARPTABLE_GETARPE(i) (&(arp_table->entries[i]))


#include <vos/net/iptable.h>
#define print_arpe(arpe) \
  { \
    printf("arp: %ld ", (long)arpe->stamp); \
    print_ipaddr(arpe->ip); printf(" "); \
    print_ethaddr(arpe->eth); \
    printf(" hey, expire at: %ld\n",(long)arpe->expire); \
  }


/*
 * arptable_resolve_ip: attempt to resolve an ip address by examining the arp
 * table. Returns -1 if not found. RESOLVED if found. TIMEDOUT if found but
 * arp entry is timedout. RESOLVED and TIMEDOUT are defined in arptable.h.
 */
extern int
arptable_resolve_ip(ipaddr_t ip, ethaddr_t eth);

/*
 * arp_resolve_ip: attempt to resolve an ip address by either using the arp
 * table to send out arp requests. Returns -1 if not resolved. Otherwise,
 * address is copied to eth.
 */
extern int
arp_resolve_ip(ipaddr_t ip, ethaddr_t eth, int ifnum);

/* 
 * arptable_remap: remaps arptable writable. Returns 0 if successful. Returns
 * -1 otherwise with errno set to V_NOMEM.
 */
extern int
arptable_remap();


#endif /* __ARP_TABLE_H__ */


