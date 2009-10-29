
#include <stdio.h>
#include <xok/sys_ucall.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>

#define EXOS_COMPAT_MODE
#include <vos/uinfo.h>
#include <vos/net/arptable.h>

uinfo_t *uinfo = (uinfo_t*) FORK_TEMP_PG;

arp_table_t *vos_arp_table = (arp_table_t*) COW_TEMP_PG;
#define VOS_ARPTABLE_GETARPE(i) (&(vos_arp_table->entries[i]))


static int 
map_vos_uinfo()
{
  int r = sys_uinfo_get();
  Pte pte;
  
  if (r == 0)
  {
    printf("exos arpd: cannot map vos uinfo!\n");
    return -1;
  }

  pte = (r << PGSHIFT) | PG_U | PG_P;

  printf("exos arpd: vos uinfo found.\n");
  r = _exos_self_insert_pte(0, pte, FORK_TEMP_PG, 0, 0);
  if (r < 0)
  {
    printf("exos arpd: cannot map vos uinfo!\n");
    return -1;
  }
  return 0;
}

static int
map_vos_arptable()
{
  Pte pte = (uinfo->arptable_where << PGSHIFT) | PG_U | PG_P;
  int r;

  printf("exos arpd: vos arp table found, mapping to 0x%x.\n",COW_TEMP_PG);
  r = _exos_self_insert_pte(0, pte, COW_TEMP_PG, 0, 0);
  if (r < 0)
  {
    printf("exos arpd: cannot map vos arp table!\n");
    return -1;
  }
  return 0;
}
 
int
vos_compat()
{
  printf("exos arpd: establishing compatibility with vos.\n");
  if (map_vos_uinfo() < 0)
    return -1;
  if (map_vos_arptable() < 0)
    return -1;
  return 0;
}


int 
vos_in_table(ipaddr_t ip,ethaddr_t eth)
{
  int i;
  arp_entry_t *arpe;

  for(i=0; i<ARP_TABLE_SIZE; i++)
  {
    arpe = VOS_ARPTABLE_GETARPE(i);
   
    if (memcmp(arpe->ip, ip, 4)==0)
    {
      if (arpe->expire < __sysinfo.si_system_ticks) /* expired */
      {
	// printf("expired\n");
	return 0;
      }

      if (arpe->status == RESOLVED)
      {
        // printf("arptable found, returning (%d): ",RESOLVED);
        // print_arpe(arpe);
        memcpy(eth, &arpe->eth[0], 6);
	return 1;
      }

      else if (arpe->status == TIMEDOUT)
      {
	// printf("timedout\n");
	return 0;
      }
    }
  }
  // printf("not found\n");
  return 0;
}


