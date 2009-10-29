
/* manage arp table */

#include <xok/sysinfo.h>
#include <xok/mmu.h>

#include <vos/proc.h>
#include <vos/uinfo.h>
#include <vos/cap.h>
#include <vos/ipc.h>
#include <vos/vm.h>
#include <vos/vm-layout.h>
#include <vos/net/arptable.h>
#include <vos/kprintf.h>

#include "init.h"


#define dprintf if (0) kprintf
#define dprint_arpe if (0) print_arpe


int
arptable_add(u_char *ip, u_char *eth, u_int status)
{
  int i;
  static int entry_to_punt = 0;
  arp_entry_t *arpe;

  assert(ip);
  assert(eth);

  /* first, see if we can find our entry */
  for(i=0; i<ARP_TABLE_SIZE; i++)
  {
    arpe = ARPTABLE_GETARPE(i);
    
    if (memcmp(ip, arpe->ip, 4)==0)
    {      
      yieldlock_acquire(&(arp_table->lock));

      if (memcmp(ip, arpe->ip, 4)==0)
      {
	arpe->stamp++; /* begin write */
        arpe->status = status;
        memcpy(&arpe->eth[0], eth, 6);
        arpe->expire = __sysinfo.si_system_ticks + SECONDS2TICKS(ARP_TIMEOUT);
	arpe->stamp++; /* write done */

        dprintf("arptable_add: ");
        dprint_arpe(arpe);
        
	yieldlock_release(&(arp_table->lock));
        return 0;
      }
      
      yieldlock_release(&(arp_table->lock));
    }
  }

  /* now, try to find a free entry */
  for(i=0; i<ARP_TABLE_SIZE; i++)
  {
    arpe = ARPTABLE_GETARPE(i);
    
    if (arpe->status == FREE)
    {
      yieldlock_acquire(&(arp_table->lock));

      if (arpe->status == FREE)
      {
	arpe->stamp++; /* begin write */
        arpe->status = status;
        memcpy(&arpe->ip[0], ip, 4);
        memcpy(&arpe->eth[0], eth, 6);
        arpe->expire = __sysinfo.si_system_ticks + SECONDS2TICKS(ARP_TIMEOUT);
	arpe->stamp++; /* write done */
  
        dprintf("arptable_add: ");
        dprint_arpe(arpe);
        
        yieldlock_release(&(arp_table->lock));
        return 0;
      }
      
      yieldlock_release(&(arp_table->lock));
    }
  }

  /* then, try to find an expired entry */
  for(i=0; i<ARP_TABLE_SIZE; i++)
  {
    arpe = ARPTABLE_GETARPE(i);
    
    if (arpe->expire < __sysinfo.si_system_ticks)
    {
      yieldlock_acquire(&(arp_table->lock));
    
      if (arpe->expire < __sysinfo.si_system_ticks)
      {
	arpe->stamp++; /* begin write */
        arpe->status = status;
        memcpy(&arpe->ip[0], ip, 4);
        memcpy(&arpe->eth[0], eth, 6);
        arpe->expire = __sysinfo.si_system_ticks + SECONDS2TICKS(ARP_TIMEOUT);
	arpe->stamp++; /* write done */
   
        dprintf("arptable_add: ");
        dprint_arpe(arpe);
      
        yieldlock_release(&(arp_table->lock));
        return 0;
      }
      
      yieldlock_release(&(arp_table->lock));
    }
  }

  /* lastly, nothing free, punt an entry */
  
  yieldlock_acquire(&(arp_table->lock));
  
  arpe = ARPTABLE_GETARPE(entry_to_punt);
  
  entry_to_punt++;
  if (entry_to_punt >= ARP_TABLE_SIZE)
    entry_to_punt = 0;

  arpe->stamp++; /* begin write */
  arpe->status = status;
  memcpy(&arpe->ip[0], ip, 4);
  memcpy(&arpe->eth[0], eth, 6);
  arpe->expire = __sysinfo.si_system_ticks + SECONDS2TICKS(ARP_TIMEOUT);
  arpe->stamp++; /* write done */
   
  dprintf("arptable_add: ");
  dprint_arpe(arpe);
      
  yieldlock_release(&(arp_table->lock));
  return 0;
}

	


int 
arptable_alloc() 
{
  int r;
  extern char *progname;

  if ((r = vm_alloc_region
	(ARPTABLE_REGION, NBPG, CAP_USER, PG_U|PG_P|PG_W)) < 0)
  {
    printf("%s: cannot initialize arp table, arp may not work\n", progname);
    return -1;
  }
  
  StaticAssert(sizeof(arp_table_t) <= NBPG);

  r = va2ppn(ARPTABLE_REGION);
  uinfo->arptable_where = r;
  
  dprintf("%s: arptable page allocated to physical page %d\n",progname,r);

  bzero(arp_table,sizeof(arp_table_t));
  arp_table->owner_envid = sys_geteid();
  arp_table->ipc_taskid = ARPTABLE_ADD_TIMEDOUT;

  return 0;
}



