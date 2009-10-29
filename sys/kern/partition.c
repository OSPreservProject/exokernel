
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */
#define __DISK_MODULE__

#include <xok/pmap.h>
#include <xok/sysinfo.h>
#include <xok/partition.h>
#include <xok/disk.h>
#include <xok/bc.h>
#include <xok_include/assert.h>
#include <xok/printf.h>

#define PARTIONS_PER_DISK 4

/* add a single disk entry for a given partition based on the specified
   disk dev */
static void add_partition (int dev, struct Partition *pt, int part_no,
			   struct disk *disk) {
  struct disk* d = SYSINFO_PTR_AT(si_disks, dev);
  printf ("filling in partition with start %d and size %d\n", pt->par_first,
	  pt->par_size);
  disk->d_id = part_no;
  disk->d_dev = dev;
  disk->d_strategy = d->d_strategy;
  disk->d_bshift = d->d_bshift;
  disk->d_bsize = d->d_bsize;
  disk->d_bmod = d->d_bmod;
  disk->d_cyls = d->d_cyls;
  disk->d_heads = d->d_heads;
  disk->d_sectors = d->d_sectors;
  disk->d_size = pt->par_size;
  disk->d_part_off = pt->par_first;
  disk->d_readcnt = 0;
  disk->d_readsectors = 0;
  disk->d_writesectors = 0;
  disk->d_queuedReads = 0;
  disk->d_ongoingReads = 0;
  disk->d_writecnt = 0;
  disk->d_queuedWrites = 0;
  disk->d_ongoingWrites = 0;
  disk->d_intrcnt = 0;
}

/* for each xok partition in pt add it to disks and return the number
   of xok partitions found */
static int add_partitions (int dev, struct Partition *pt,
			   int next_partition, struct disk *disks) {
  int i;

  for (i = 0; i < PARTIONS_PER_DISK; i++) {
    if (pt[i].par_id == XOK_PAR_ID) {
      if (next_partition >= MAX_DISKS)
	printf ("Too many xok partitions! Unable to add partition %d on "
		"dev %d\n", i, dev);
      else {
	printf ("found one...adding partition number %d\n", next_partition);
	add_partition (dev, &pt[i], next_partition, &disks[next_partition]);
	next_partition++;
      }
    }
  }

  return next_partition;
}

/*set to 1 if ide driver already polled for interrupts*/
int ide_already_polled = 0;

/* read the partition table off the specified device */
static struct Partition *load_partition_table (int dev, int *error) {
  struct Partition *pt;
  int ret;
  struct bc_entry *bc;

  ret = bc_read_extent (dev, 0, 1, NULL);
  if (ret) {
    *error = ret;
    return 0;
  }

  while ((!ide_already_polled) && (!disk_pollforintrs (0)));

  ide_already_polled = 0;

  bc = bc_lookup (dev, 0);
  if (!bc) {
    *error = -E_UNSPECIFIED;
    return 0;
  }

  printf ("loaded partition table from device %d\n", dev);
  pt = (struct Partition *) 
    ((char *)pp2va (ppages_get(bc->buf_ppn)) + PAR_TBL_START);
  return pt;
}

/* for each physical disk the system knows about, read its partition
   table and replace the disk entry in si_disks with a series of
   "disks" that each point to a particular partition.

   returns negative on error, else number of partitions found. */

int partition_init () {
  struct Partition *pt;
  struct disk new_si_disks[MAX_DISKS];
  int next_partition = 0;
  int i;
  int error;

  /* at this point si->si_disks has one entry for each PHYSICAL
     disk in the system. We replace these entries with one entry
     for each PARTITION on each physical disk. */

  /* build up a new si_disks array in which each entry corresponds to
     a partition rather than to a disk */

  for (i = 0; i < SYSINFO_GET(si_ndisks); i++) {
    if (!(pt = load_partition_table (i, &error))) {
      return error;
    }
    next_partition = add_partitions (i, pt, next_partition, new_si_disks);
  }

  /* only install the partition info if partitions were found */
  if (next_partition != 0) {
    bcopy (new_si_disks, SYSINFO_GET(si_disks), sizeof(struct disk)*MAX_DISKS);
    SYSINFO_ASSIGN(si_ndisks, next_partition);
  }

  return next_partition;
}

#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

