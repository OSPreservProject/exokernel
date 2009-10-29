
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

#ifndef _VM_LAYOUT_H_
#define _VM_LAYOUT_H_

#include <xok/mmu.h>


#define PAGESIZ NBPG

/* Where variouse system data structures are etc */

#define SHARED_LIBRARY_START 0x10000000	/* also -T in bin/shlib/GNUmakefile */
#define SHARED_LIBRARY_START_RODATA 0x80000000 /*read-only copy of shared lib*/


/* where BC pages are mapped */
/* bufcache entries */
#define BUFCACHE_REGION_START	((unsigned int) 0x20000000)
#define BUFCACHE_REGION_SZ      65536*PAGESIZ /* 256MB */
/* just past end */
#define BUFCACHE_REGION_END	(BUFCACHE_REGION_START + BUFCACHE_REGION_SZ)


/* where shm segment table lives. */
#define GLOBAL_SEGMENT_TABLE    (BUFCACHE_REGION_START + BUFCACHE_REGION_SZ)
#define GLOBAL_SEGMENT_TABLE_SZ 20*PAGESIZ


#define FD_SHARED_REGION        (GLOBAL_SEGMENT_TABLE + GLOBAL_SEGMENT_TABLE_SZ)

#define FILP_SHM_OFFSET         789212
#define FD_SHM_OFFSET 789213 /* be careful using the next say 32 offsets */
			     /* after this one, used by each module */

#define UDP_SHARED_REGION       (FD_SHARED_REGION)
#define UDP_SHARED_REGION_SZ    32*PAGESIZ

#define PTY_SHARED_REGION       (UDP_SHARED_REGION + UDP_SHARED_REGION_SZ)
#define PTY_SHARED_REGION_SZ    8*PAGESIZ

#define NFS_LOOKUP_SHM_OFFSET   (FD_SHM_OFFSET + 33)
#define NFS_SHARED_REGION  (PTY_SHARED_REGION + PTY_SHARED_REGION_SZ)
#define NFS_SHARED_REGION_SZ 48*PAGESIZ

#define NFS_LOOKUP_SHARED_REGION (NFS_SHARED_REGION + NFS_SHARED_REGION_SZ)
#define NFS_LOOKUP_SHARED_REGION_SZ 8*PAGESIZ

/* not used for now */
#define NFSDIR_REGION_START	(NFS_LOOKUP_SHARED_REGION + NFS_LOOKUP_SHARED_REGION_SZ)
#define NFSDIR_REGION_START_SZ	0*PAGESIZ

#define TCPSOCKET_DEDICATED_REGION	(NFSDIR_REGION_START + NFSDIR_REGION_START_SZ)
#define TCPSOCKET_DEDICATED_REGION_SZ	1024*PAGESIZ

#define CONSOLE_REGION	(TCPSOCKET_DEDICATED_REGION + TCPSOCKET_DEDICATED_REGION_SZ)
#define CONSOLE_REGION_SZ	2*PAGESIZ

#define NPTY_SHARED_REGION	(CONSOLE_REGION + CONSOLE_REGION_SZ)
#define NPTY_SHARED_REGION_SZ	21*PAGESIZ

#define FILP_SHARED_REGION (NPTY_SHARED_REGION + NPTY_SHARED_REGION_SZ)
#define FILP_SHARED_REGION_SZ 5*PAGESIZ

#define MOUNT_SHARED_REGION (FILP_SHARED_REGION + FILP_SHARED_REGION_SZ)
#define MOUNT_SHARED_REGION_SZ PAGESIZ

#define CFFS_SHARED_REGION (MOUNT_SHARED_REGION + MOUNT_SHARED_REGION_SZ)
#define CFFS_SHARED_REGION_SZ 3*PAGESIZ

#define PIPE_SHARED_REGION (CFFS_SHARED_REGION + CFFS_SHARED_REGION_SZ) 
#define PIPE_SHARED_REGION_SZ 402*PAGESIZ

/* arp_shared_region */
#define ARP_SHM_OFFSET          234235
#define ARP_SHARED_REGION   (PIPE_SHARED_REGION + PIPE_SHARED_REGION_SZ)
#define ARP_SHARED_REGION_SZ 1*PAGESIZ

/* ip_table used for ifconfig and routing */
#define IP_SHM_OFFSET      (ARP_SHM_OFFSET + 1)
#define IP_SHARED_REGION   (ARP_SHARED_REGION + ARP_SHARED_REGION_SZ)
#define IP_SHARED_REGION_SZ 1*PAGESIZ

/* region not used for now */
/*#define TEMP_PIPE_PAGE      (FD_SHARED_REGION + 0x4000*PAGESIZ)*/

/* synch_table used for tsleep and wakeup  */
#define SYNCH_SHM_OFFSET       (IP_SHM_OFFSET + 1)
#define SYNCH_SHARED_REGION    (IP_SHARED_REGION + IP_SHARED_REGION_SZ)
#define SYNCH_SHARED_REGION_SZ 1*PAGESIZ

/* pty shared memory */
#define NEWPTY_SHM_OFFSET      (SYNCH_SHM_OFFSET + 1)    
#define NEWPTY_SHARED_REGION   (SYNCH_SHARED_REGION + SYNCH_SHARED_REGION_SZ)
#define NEWPTY_SHARED_REGION_SZ 16*PAGESIZ

/* process management table */
#define PROC_TABLE_SHM_OFFSET      (NEWPTY_SHM_OFFSET + 1)    
#define PROC_TABLE_SHARED_REGION   (NEWPTY_SHARED_REGION + NEWPTY_SHARED_REGION_SZ)
#define PROC_TABLE_SHARED_REGION_SZ 17*PAGESIZ
    
/* Exos locks in shared memory */
#define EXOS_LOCKS_SHM_OFFSET	  (PROC_TABLE_SHM_OFFSET + 1)
#define EXOS_LOCKS_SHARED_REGION  (PROC_TABLE_SHARED_REGION + PROC_TABLE_SHARED_REGION_SZ)
#ifdef __LOCKS_DEBUG__
#define EXOS_LOCKS_SHARED_REGION_SZ  2*PAGESIZ
#else
#define EXOS_LOCKS_SHARED_REGION_SZ  PAGESIZ
#endif

/* for DMA regions in shared memory */
#define DMAREGIONS_SHM_OFFSET  (EXOS_LOCKS_SHM_OFFSET + 1)
#define DMAREGIONS_REGION      (EXOS_LOCKS_SHARED_REGION + EXOS_LOCKS_SHARED_REGION_SZ)
#define DMAREGIONS_REGION_SZ   4*PAGESIZ

/* used for passing the proc_struct (current) to child  */
#define PROC_STRUCT         (DMAREGIONS_REGION + DMAREGIONS_REGION_SZ)   
#define PROC_STRUCT_SZ      3*PAGESIZ
#define TEMP_PROC_STRUCT    (PROC_STRUCT + PROC_STRUCT_SZ)
#define TEMP_PROC_STRUCT_SZ PROC_STRUCT_SZ

#define USER_TEMP_PGS	(TEMP_PROC_STRUCT + TEMP_PROC_STRUCT_SZ)
#define USER_TEMP_PGS_SZ	32*PAGESIZ

#define FORK_TEMP_PG (USER_TEMP_PGS + USER_TEMP_PGS_SZ)
#define COW_TEMP_PG (FORK_TEMP_PG+NBPG)

/* for internal mallocs - this should be last in list */
#define DYNAMIC_MAP_REGION     (COW_TEMP_PG + NBPG)

#endif /* _VM_LAYOUT_H_ */
