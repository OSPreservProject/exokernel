
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

#include "arp_table.h"
#include <exos/netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <exos/critical.h>
#include <exos/ipc.h>
#include <exos/ipcdemux.h>

#include <exos/uwk.h>
#include <exos/cap.h>

#include <unistd.h>		/* for sleep */

void 
arp_init_table() {
  arp_entry_p e;
  int i;
  
  assert(arp_table);
  
  for (i = 0, e = &arp_table->entries[0] ; i < ARP_TABLE_SIZE ; i++,e++) 
    arp_set_free(e);
  
  arp_table->arp_envid = 0;
  arp_table->new_entry = 0;
}

/* Initialize Functions */
void
arp_set_free(arp_entry_p e) {
  memset(e, 0, sizeof(arp_entry_t));
  e->status = FREE;
}
void
arp_set_timeout(arp_entry_p e) {
  e->status = TIMEOUT;
  e->ticks = TICKS + SECONDS2TICKS(ARP_TIMEOUT_TIMEOUT);
  e->count = 0;
}
void 
arp_set_pending(arp_entry_p e, char ip_addr[4], int ifnum) {
  e->status = PENDING;
  /* ideally should be TICKS + SECONDS2TICKS(ARP_RESEND_TIMEOUT); 
   * but we set it to ticks, so that the first time the daemon checks it will
   * send the request. */
  e->ticks = TICKS;		
  e->count = ARP_RESEND_RETRIES;
  e->ifnum = ifnum;
  memcpy(e->ip, ip_addr, 4);
  memset(e->eth, 0, 6);
  arp_table->new_entry = 1;
}
void 
arp_set_resolved(arp_entry_p e, char ip_addr[4], char eth_addr[6]) {
  e->status = RESOLVED;
  e->ticks = TICKS + SECONDS2TICKS(ARP_RESOLVED_TIMEOUT);
  e->count = 0;
  memcpy(e->ip, ip_addr, 4);
  memcpy(e->eth, eth_addr, 6);
}

/* Retransmission, sends requests and decrements retry count */
void
arp_retransmit(arp_entry_p e) {
  assert(EISPENDING(e));
  assert(e->count > 0);
  /* send_request(e) */
  e->count--;
  e->ticks = TICKS + SECONDS2TICKS(ARP_RESEND_TIMEOUT);
}

/* Search Functions */
arp_entry_p 
arp_search_free_entry(void) {
  register int i;
  register arp_entry_p e;
  register arp_entry_p tentative = NULL;
  EnterCritical();
  for (i = 0, e = &arp_table->entries[0] ; i < ARP_TABLE_SIZE ; i++,e++) {
    if (EISFREE(e)) {
      ExitCritical();
      return e;
    }
    /* if we dont have a tentative entry, and is not pending and not valid */
    if (!tentative && !EISPENDING(e) && !arp_entry_valid(e)) tentative = e;
  }
  ExitCritical();
  return tentative;
}
arp_entry_p 
arp_search_entry_by_ip(char ip_addr[4]) {
  register int i;
  register arp_entry_p e;
  EnterCritical();
  for (i = 0, e = &arp_table->entries[0] ; i < ARP_TABLE_SIZE ; i++,e++) 
    if (memcmp(e->ip, ip_addr,4) == 0) {
      ExitCritical();
      return e;
    }
  ExitCritical();
  return (arp_entry_p)0;
}

static int arp_iterator;
void 
arp_iter_init(void) {
  arp_iterator = 0;
}
arp_entry_p
arp_iter(void) {
  if (arp_iterator >= ARP_TABLE_SIZE) return (arp_entry_p)0;
  return &arp_table->entries[arp_iterator++];
}


/* returns 0 if entry is not valid (timed out) 
 * which could be in state TIMEOUT or RESOLVED
 * returns 1 if entry is valid, 
 * which could be in state TIMEOUT or RESOLVED or PENDING */
int
arp_entry_valid(arp_entry_p e) {
  switch(e->status) {
  case FREE:
    assert(0);
  case RESOLVED:
    return (TICKS <= e->ticks) ? 1 : 0;
  case TIMEOUT:
    return (TICKS <= e->ticks) ? 1 : 0;
  case PENDING:
    if (e->count > 0) {
      return 1;
    } else {
      /* count is 0 */
      return (TICKS <= e->ticks) ? 1 : 0;
    }
  }
  assert(0);			/* should never reach here. */
}

/* add an ip_addr to the table.  Usually done via IPC */
int 
ipc_arp_add_entry(char ip_addr[4],int ifnum) {
  arp_entry_p e;
  //printf("IPC_ARP_ADD_ENTRY %s\n",pripaddr(ip_addr));
  if ((e = arp_search_entry_by_ip(ip_addr)) != NULL) {
    /* entry is not FREE */
    if (arp_entry_valid(e)) {
      /* the entry is valid */
      if (EISPENDING(e) || EISRESOLVED(e)) return ARP_SUCCESS;
      if (EISTIMEOUT(e)) return ARP_TIMEDOUT;
      assert(0);
    } else {
      /* entry is not valid */
      if (EISPENDING(e) && e->count == 0 && TICKS >= e->ticks) {
	arp_set_timeout(e);
	return ARP_SUCCESS;
      }
      /* refresh entry since it is not valid */
      arp_set_pending(e,ip_addr,ifnum);
      return ARP_SUCCESS;
    }
  } else {
    /* there is no entry */
    EnterCritical();
    e = arp_search_free_entry();
    if (e == NULL) {
      ExitCritical();
      return ARP_RETRY;
      
    }
    arp_set_pending(e,ip_addr,ifnum);
    ExitCritical();
    return ARP_SUCCESS;
  }
}

static int
arp_delete_entry(char ip_addr[4]) {
  arp_entry_p e;
  EnterCritical();
  if ((e = arp_search_entry_by_ip(ip_addr))) {
    arp_set_free(e);
    ExitCritical();
    return ARP_SUCCESS;
  } else {
    ExitCritical();
    return ARP_NOTFOUND;
  }
}

int
arp_ipc_handler(int code, int ip_addr, int ifnum, int arg3, u_int caller) {
  switch(code) {
  case  IPC_ARP_ADD:
    //kprintf("IPC_ARP_ADD: ip: %08x from caller: %d\n",ip_addr,caller);
    return ipc_arp_add_entry((char *)&ip_addr,ifnum);
    break;
  case IPC_ARP_DELETE:
    //kprintf("IPC_ARP_DELETE: ip: %08x from caller: %d\n",ip_addr,caller);
    return arp_delete_entry((char *)&ip_addr);
  default:
    //kprintf("BAD ARP IPC CODE: %d from caller: %ud\n",code,caller);
    return -1;
  }
}


/* Print Functions */
void
arp_print_entry(arp_entry_p e) {
  char *status ="?";
  switch(e->status) {
  case FREE: status = "F"; break;
  case PENDING: status = "P"; break;
  case RESOLVED: status = "R"; break;
  case TIMEOUT: status = "T"; break;
  }
  printf("pid %d: %s (%s) at %s, timeout: %qd:%d ifnum: %d\n",
	 getpid(),status,
	 pripaddr(e->ip),
	 prethaddr(e->eth),
	 e->ticks,
	 e->count,e->ifnum);
}

void
arp_print_table(void) {
  arp_entry_p e;
  if (!arp_table) kprintf("arp_table pointer is NULL\n");
  EnterCritical();
  printf("ARP_TABLE %p new_entry: %d envid: %d TIME: %qd\n",arp_table,
	 arp_table->new_entry,arp_table->arp_envid,TICKS);
  if (arp_table == NULL) goto done;
  arp_iter_init();
  while ((e = arp_iter()))
    if (!EISFREE(e)) arp_print_entry(e);
  done:
  ExitCritical();
}


/* ARP ENVID */
unsigned int
get_arp_envid(void) {return arp_table->arp_envid;}
void
set_arp_envid(unsigned int envid) {arp_table->arp_envid = envid;}

int
arp_resolve_ip(char ip_addr[4], char eth_addr[6], int ifnum) {
  unsigned int ip;
  int status;
  int retry = 0;
  unsigned int arp_envid;
  arp_entry_p e = NULL;
  /* Search the arp table */
retry:
  if (retry > 2) {
    kprintf("TOO MANY RETRIES SOMETHING WRONG WITH IMPLEMENTATION\n");
    if (e) arp_print_entry(e);
    return -2;
  }
  if ((e = arp_search_entry_by_ip(ip_addr)) != NULL) {
    if (arp_entry_valid(e)) {
      if (EISRESOLVED(e)) {memcpy(eth_addr,e->eth,6);return 0;}
      if (EISTIMEOUT(e)) {return -1;}
      /* entry is pending */
      wk_waitfor_value_neq((int *)&e->status,PENDING,CAP_ROOT);
      // arp_print_entry(e);
      retry++;
      goto retry;
    } 
  } 
  arp_envid = get_arp_envid();
  if (arp_envid == 0) {
    printf("Arp daemon not running\n");
    return -1;
  }
  
  /* make sure is word aligned */
  memcpy(&ip ,ip_addr, 4);
  //  kprintf("ARP_ENVID: %d. SIPCOUT\n",arp_envid);
  status = sipcout(arp_envid,IPC_ARP_ADD,ip,ifnum,0);
  if (status == ARP_TIMEDOUT) return -1;
  if (status == ARP_RETRY) {
    printf("NO FREE ENTRIES, sleeping a little then retrying (table full)\n");
    sleep(1);
    goto retry;
  }
  retry++;
  goto retry;
  return 0;
}

int
arp_remove_ip(char ip_addr[4]) {
  unsigned int ip;
  int status;
  unsigned int arp_envid;

  arp_envid = get_arp_envid();
  if (arp_envid == 0) {
    printf("Arp daemon not running\n");
    return -1;
  }
  
  /* make sure is word aligned */
  memcpy(&ip ,ip_addr, 4);
  printf("ARP_ENVID: %d. SIPCOUT ip: %s\n",arp_envid,pripaddr((char *)&ip));
  status = sipcout(arp_envid,IPC_ARP_DELETE,ip,0,0);
  switch(status) {
  case -1: printf("SIPCOUT ERROR: %d, arp_envid: %d\n",status,arp_envid); return -1;
  case ARP_SUCCESS: return 0;
  case ARP_NOTFOUND: printf("Entry %s not found\n",pripaddr(ip_addr)); return -1;
  default:
    assert(0);			/* bad return code */
    return 0;
  }
}
