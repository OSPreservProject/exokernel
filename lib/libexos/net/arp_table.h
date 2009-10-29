
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

#include <exos/netinet/fast_arp.h>
#include <exos/net/arp.h>



/* seconds ((__sysinfo.si_system_ticks * __sysinfo.si_rate) / 1000000);  */
/* microseconds ((__sysinfo.si_system_ticks * __sysinfo.si_rate);  */

#define SECONDS2TICKS(secs) ((secs * 1000000) / __sysinfo.si_rate)
#define ARP_TIMEOUT_TIMEOUT  40	/* 20 seconds */
#define ARP_RESOLVED_TIMEOUT 1200 /* 20 minutes seconds */

/* 10 retries spaced by one second */
#define ARP_RESEND_TIMEOUT 1	/* 1 second */
#define ARP_RESEND_RETRIES 10

#define ARP_TABLE_SIZE 16

enum arp_entry_status { RESOLVED, PENDING, FREE, TIMEOUT};

typedef long long arp_tick_t;

typedef struct arp_entry {
  enum arp_entry_status status;
  char ip[4];
  char eth[6];
  arp_tick_t ticks;			/* ticks at which point entry times out or
					 * it is necessary to retransmit */
  int count;			/* when entry is PENDING 
				   how many more times to retrans */
  int ifnum;			/* interface which to send out the requests */
} arp_entry_t, *arp_entry_p;

typedef struct arp_table {
  unsigned int arp_envid;
  int new_entry;		/* flag */
  arp_entry_t entries[ARP_TABLE_SIZE];
} *arp_table_p,arp_table_t;

extern arp_table_p arp_table;

/* return codes */
#define ARP_SUCCESS 0
#define ARP_RETRY 1
#define ARP_TIMEDOUT 2
#define ARP_NOTFOUND 3		/* used with arp_remove_ip */

/* status MACROS */
#define EISFREE(e)     (e->status == FREE)
#define EISPENDING(e)  (e->status == PENDING)
#define EISRESOLVED(e) (e->status == RESOLVED)
#define EISTIMEOUT(e)  (e->status == TIMEOUT)

void 
arp_init_table();

arp_entry_p 
arp_search_entry_by_ip(char ip_addr[4]);
arp_entry_p 
arp_search_free_entry(void);

/* Initialize Functions */
void
arp_set_free(arp_entry_p e);
void
arp_set_timeout(arp_entry_p e);
void 
arp_set_pending(arp_entry_p e, char ip_addr[4], int ifnum);
void 
arp_set_resolved(arp_entry_p e, char ip_addr[4], char eth_addr[6]);

int
arp_entry_valid(arp_entry_p e);

int 
ipc_arp_add_entry(char ip_addr[4], int ifnum);
int
arp_ipc_handler(int code, int arg1, int arg2, int arg3, u_int caller);

/* debugging */
void
arp_print_entry(arp_entry_p e);


void 
arp_iter_init(void);
arp_entry_p
arp_iter(void);



void
arp_retransmit(arp_entry_p e);
