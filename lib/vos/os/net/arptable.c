
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

#include <vos/vm-layout.h>
#include <vos/proc.h>
#include <vos/ipc.h>
#include <vos/cap.h>
#include <vos/vm.h>
#include <vos/uinfo.h>
#include <vos/net/arptable.h>
#include <vos/errno.h>

arp_table_t *arp_table = (arp_table_t*) ARPTABLE_REGION;

#define dprintf if(0) kprintf
#define dprint_arpe if(0) print_arpe

void
arptable_init() 
{
  Pte pte = (uinfo->arptable_where << PGSHIFT) | PG_U | PG_P | PG_SHARED;
  int r;

  dprintf("arptable_init: mapping %d onto 0x%x\n",pte>>PGSHIFT,ARPTABLE_REGION);
  r = vm_self_insert_pte(CAP_USER, pte, ARPTABLE_REGION, 0, 0);
  if (r < 0)
  {
    printf("arptable_init: cannot map arp_table!\n");
    exit(-1);
  }
}

int
arptable_resolve_ip(ipaddr_t ip, ethaddr_t eth)
{
  int i;
  arp_entry_t *arpe;
  arp_entry_t copied_entry;

  for(i=0; i<ARP_TABLE_SIZE; i++)
  {
    arpe = ARPTABLE_GETARPE(i);
retry:
    if (memcmp(arpe->ip, ip, 4)==0)
    {
      unsigned long v1, v2;
      v1 = arpe->stamp;
      __sync_read();
      copied_entry = *arpe;
      __sync_read();
      v2 = arpe->stamp;
      if (v1 != v2 || (v1 & 1)) 
      {
	printf("arptable resolve ip: detect contention, retry\n");
	goto retry;
      }

      if (memcmp(copied_entry.ip, ip, 4)!=0)
	continue;

      if (copied_entry.expire < __sysinfo.si_system_ticks) /* expired */
      {
        dprintf("arptable found expired entry %ld, %ld\n",
	    (long)copied_entry.expire, (long)__sysinfo.si_system_ticks);
	return -1;
      }

      if (copied_entry.status == RESOLVED)
      {
        dprintf("arptable found, returning (%d): ",RESOLVED);
        dprint_arpe((&copied_entry));
        memcpy(eth, &copied_entry.eth[0], 6);
	return RESOLVED;
      }

      else if (copied_entry.status == TIMEDOUT)
      {
	dprintf("arptable found timedout entry: ");
	dprint_arpe((&copied_entry));
	return TIMEDOUT;
      }
    }
  }
  return -1;
}

int
arptable_add_timedout_entry(ipaddr_t ip)
{
  u_int ipaddr;
  int r;
  extern int errno;
  
  memcpy(&ipaddr, &ip[0], 4);
 
  errno = 0;
  r = ipc_sleepwait(arp_table->ipc_taskid, ipaddr, 0, 0, 0L, 
                    0L, arp_table->owner_envid, 1);
  return 0;
}



int 
arptable_remap(void)
{
  Pte pte = (uinfo->arptable_where << PGSHIFT) | PG_U | PG_P | PG_W;
  int r;

  r = vm_self_insert_pte(CAP_USER, pte, ARPTABLE_REGION, 0, 0);
  
  if (r < 0)
    RETERR(V_NOMEM);
  return 0;
}


