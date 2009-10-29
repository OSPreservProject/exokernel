
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

#include "../../include/exos/vm-layout.h"
#include "../../sys/xok/mmu.h"
#include <stdio.h>
#include <stdlib.h>

// #define WANT_OLD_FORMAT

typedef struct large {
  int foo;
  int bar;
  char z[17];
} *pp;

#define PRINTHEADER printf("%-30s    %10s   %12s  %s\n","Regions","Hex addr","decimal","PDENO")

#ifdef WANT_OLD_FORMAT
#define PRINT(x) printf("%-30s == 0x%08x   %12d  %d  \n", #x , \
     (unsigned int)(x), (int)(x), PDENO(x))
#else
#define PRINT(x) \
strcpy (els[count].name, #x);\
els[count].va = x; \
els[count].arith = PDENO(x); \
count++;

#define MAX 100
typedef struct _x {
  uint  va;
  uint  arith;
  char  name[40];
} entry_t;

entry_t  els[MAX];
int      count=0;

int compare (const void *arg0, const void *arg1)
{
  entry_t *p1 = (entry_t *) arg0;
  entry_t *p2 = (entry_t *) arg1;

  if (p1->va < p2->va)
    return -1;
  else if (p1->va == p2->va)
    return 0;
  else
    return 1;
}

void dump (void)
{
  int i;
  qsort (els, count, sizeof(entry_t), compare);
  for (i=0 ; i < count ; i++)
    printf("%-30s == 0x%08x   %12d  %d  \n", 
	   els[i].name, els[i].va, els[i].va, els[i].arith);
}

#endif /* WANT_OLD_FORMAT */

int main() {
PRINTHEADER;
PRINT(PAGESIZ);

/* Where variouse system data structures are etc */

PRINT(SHARED_LIBRARY_START);
PRINT(SHARED_LIBRARY_START_RODATA);

/* where shm segment table lives. */
PRINT(GLOBAL_SEGMENT_TABLE);
PRINT(GLOBAL_SEGMENT_TABLE_SZ);



/* where BC pages are mapped */
/* bufcache entries */
PRINT(BUFCACHE_REGION_START);
PRINT(BUFCACHE_REGION_SZ);
/* just past end */
PRINT(BUFCACHE_REGION_END);






PRINT(FD_SHARED_REGION);

/* after this one, used by each module */

PRINT(UDP_SHARED_REGION);
PRINT(UDP_SHARED_REGION_SZ);

PRINT(PTY_SHARED_REGION);
PRINT(PTY_SHARED_REGION_SZ);

PRINT(NFS_SHARED_REGION);
PRINT(NFS_SHARED_REGION_SZ);

PRINT(NFS_LOOKUP_SHARED_REGION);
PRINT(NFS_LOOKUP_SHARED_REGION_SZ);

/* not used for now */
PRINT(NFSDIR_REGION_START);
PRINT(NFSDIR_REGION_START_SZ);

PRINT(TCPSOCKET_DEDICATED_REGION);
PRINT(TCPSOCKET_DEDICATED_REGION_SZ);

PRINT(CONSOLE_REGION);
PRINT(CONSOLE_REGION_SZ);

PRINT(NPTY_SHARED_REGION);
PRINT(NPTY_SHARED_REGION_SZ);

PRINT(FILP_SHARED_REGION);
PRINT(FILP_SHARED_REGION_SZ);

PRINT(MOUNT_SHARED_REGION);
PRINT(MOUNT_SHARED_REGION_SZ);

PRINT(CFFS_SHARED_REGION);
PRINT(CFFS_SHARED_REGION_SZ);

PRINT(PIPE_SHARED_REGION);
PRINT(PIPE_SHARED_REGION_SZ);

/* arp_shared_region */
PRINT(ARP_SHARED_REGION);
PRINT(ARP_SHARED_REGION_SZ);


PRINT(IP_SHARED_REGION);
PRINT(IP_SHARED_REGION_SZ);

/* region not used for now */
//PRINT(TEMP_PIPE_PAGE);

PRINT(SYNCH_SHARED_REGION);
PRINT(SYNCH_SHARED_REGION_SZ);

PRINT(NEWPTY_SHARED_REGION);
PRINT(NEWPTY_SHARED_REGION_SZ);

PRINT(PROC_TABLE_SHARED_REGION);
PRINT(PROC_TABLE_SHARED_REGION_SZ);

PRINT(EXOS_LOCKS_SHARED_REGION);
PRINT(EXOS_LOCKS_SHARED_REGION_SZ);

/* for DMA regions in shared memory */
PRINT(DMAREGIONS_SHM_OFFSET);
PRINT(DMAREGIONS_REGION);
PRINT(DMAREGIONS_REGION_SZ);

/* used for passing the proc_struct (current) to child  */
PRINT(PROC_STRUCT);
PRINT(PROC_STRUCT_SZ);
PRINT(TEMP_PROC_STRUCT);
PRINT(TEMP_PROC_STRUCT_SZ);

PRINT(FORK_TEMP_PG);
PRINT(COW_TEMP_PG);

/* for internal mallocs - this should be last in list */
PRINT(DYNAMIC_MAP_REGION);

PRINT(ARP_SHM_OFFSET);
PRINT(NFS_LOOKUP_SHM_OFFSET);
PRINT(FILP_SHM_OFFSET);
PRINT(FD_SHM_OFFSET);
PRINT(IP_SHM_OFFSET);
PRINT(SYNCH_SHM_OFFSET);
PRINT(NEWPTY_SHM_OFFSET);
PRINT(PROC_TABLE_SHM_OFFSET);
PRINT(DMAREGIONS_SHM_OFFSET);



/* from mmu.h */
PRINT(KERNBASE);
PRINT(VPT);
PRINT(NASHPD);
PRINT(ASHVM);
PRINT(KSTACKTOP);
PRINT(ASHDVM);
PRINT(LAPIC);
PRINT(IOAPIC);
PRINT(ULIM);
PRINT(UVPT);
PRINT(UPPAGES);
PRINT(CPUCXT);
PRINT(SYSINFO);
PRINT(SYSINFO_SIZE);
PRINT(UENVS);
PRINT(UBC);
PRINT(UXNMAP);
PRINT(UXNMAP_SIZE);
PRINT(UXN);
PRINT(UXN_SIZE);
PRINT(URO);
PRINT(UTOP);
PRINT(UADDRS);
PRINT(UADDR);
PRINT(UIDT);
PRINT(USTACKTOP);
PRINT(UTEXT);

#ifndef WANT_OLD_FORMAT
dump();
#endif

return 0;
}
