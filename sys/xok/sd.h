
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


#ifndef __SYS_SD_H__
#define __SYS_SD_H__

struct sd_softc {                                                          
   int sc_devno;
   u_int size, secsize;

   int flags;
#define SDF_LOCKED      0x01
#define SDF_WANTED      0x02
#define SDF_ANCIENT     0x10            /* disk is ancient; for minphys */
   struct scsi_link *sc_link;      /* contains our targ, lun, etc. */
   struct disk_parms {
      u_char heads;           /* number of heads */
      u_short cyls;           /* number of cylinders */
      u_char sectors;         /* number of sectors/track */
      int blksize;            /* number of bytes/sector */
      u_long disksize;        /* total number sectors */
   } params;
   struct  buf *sched_queue;       /* queue of pending operations */
   struct  buf *sched_next;        /* next pending op to schedule */
   daddr_t sched_prevblkno;        /* b_blkno of last scheduled op */
};

#define SD_MAXDEVS	MAX_DISKS

#endif	/* __SYS_SD_H__ */

