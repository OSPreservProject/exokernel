
/*
 * Copyright MIT 1999
 */

#ifndef __VOS_UINFO_H__
#define __VOS_UINFO_H__

#include <xok/types.h>

#ifndef EXOS_COMPAT_MODE
#include <vos/vm-layout.h>
#endif

typedef struct
{
  u_int iptable_where;		/* the IP table: route and interfaces */
  u_int arptable_where; 	/* the ARP table: arp entries */
  u_int fsrv_envid;
#ifndef EXOS_COMPAT_MODE
  u_int ports_where[PGNO(PORTS_REGION_SZ)]; /* the port table */
#endif
} uinfo_t;

extern uinfo_t *uinfo;

/*
 * uinfo_remap: remap uinfo page writable. Returns 0 if successful. Returns -1
 * otherwise. Errno set to V_NOMEM.
 */
extern int uinfo_remap();

#endif


