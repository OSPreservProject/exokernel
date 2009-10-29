
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


#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <xok/sys_ucall.h>

#include <exos/osdecl.h>
#include <exos/tick.h>
#include <exos/cap.h>
#include <exos/osdecl.h>

#include <xok/disk.h>
#include <xok/sysinfo.h>

#define __ExoPC__
#include <dev/scsi_all.h>
#include <dev/scsi_disk.h>
#include <dev/scsi_message.h>
#include <dev/scsiconf.h>
#define SDRETRIES	4


#define START_SECT	0
#define SECT_SIZE	512
#define MAX_NUMSECTS	10240
#define TEST_MAXSIZE	1024
#define BUF_SIZE	(MAX_NUMSECTS * SECT_SIZE)


struct scsi_generic * build_scsi_rw_command (int rw, uint blkno, uint bcount, int *cmdlen)
{
   uint nblks = howmany (bcount, SECT_SIZE);
   if (((blkno & 0x1fffff) == blkno) && ((nblks & 0xff) == nblks)) {
	/* transfer can be described in small RW SCSI command, so do it */
      struct scsi_rw *cmd_small = (struct scsi_rw *) malloc (sizeof(struct scsi_rw));
      bzero (cmd_small, sizeof(struct scsi_rw));
      cmd_small->opcode = (rw == B_READ) ? READ_COMMAND : WRITE_COMMAND;
      _lto3b (blkno, cmd_small->addr);
      cmd_small->length = nblks & 0xff;
      *cmdlen = sizeof (struct scsi_rw);
      return ((struct scsi_generic *) cmd_small);
      
   } else {
      struct scsi_rw_big *cmd_big = (struct scsi_rw_big *) malloc (sizeof(struct scsi_rw_big));
      bzero (cmd_big, sizeof(struct scsi_rw_big));
      cmd_big->opcode = (rw == B_READ) ? READ_BIG : WRITE_BIG;
      _lto4b (blkno, cmd_big->addr);
      _lto2b (nblks, cmd_big->length);
      *cmdlen = sizeof (struct scsi_rw_big);
      return ((struct scsi_generic *) cmd_big);
   }
}


uint flags_for_scsi_command (int opcode)
{
   switch (opcode) {
	case READ_COMMAND:
	case READ_BIG:
	case MODE_SENSE:
	case READ_CAPACITY:
	case INQUIRY:
		return SCSI_DATA_IN;

	case WRITE_COMMAND:
	case WRITE_BIG:
	case REASSIGN_BLOCKS:
		return SCSI_DATA_OUT;

	default:
		printf ("unknown SCSI command opcode: %d\n", opcode);
		exit (-1);
   }
}


void do_io (uint diskno, uint blkno, uint bcount, char *addr, int flags)
{
   struct scsicmd theScsicmd;
   struct buf theBuf;
   int ret;

   theBuf.b_dev = diskno;
   theBuf.b_blkno = 0;
   theBuf.b_bcount = sizeof(struct scsicmd);
   theBuf.b_flags = B_SCSICMD;
   theBuf.b_memaddr = &theScsicmd;

   theScsicmd.sc_link = NULL;
   theScsicmd.scsi_cmd = build_scsi_rw_command ((flags & B_READ), blkno, bcount, &theScsicmd.cmdlen);
   theScsicmd.data_addr = addr;
   theScsicmd.datalen = bcount;
   theScsicmd.retries = SDRETRIES;
   theScsicmd.timeout = 10000;
   theScsicmd.bp = &theBuf;
   theScsicmd.flags = flags_for_scsi_command (theScsicmd.scsi_cmd->opcode);

   theBuf.b_resid = 0;
   theBuf.b_resptr = &theBuf.b_resid;
	/* This is iffy, with respect to poorly conceived kernel prot. */
   if (ret = sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, CAP_ROOT)) {
      printf ("Unable to initiate request: ret %d, bcount %d, blkno %d\n", ret, bcount, blkno);
      exit (1);
   }
/*
sleep(1);
printf ("request started: %d\n", ret);
*/

   /* poll for completion */
   while (*((volatile int *)&theBuf.b_resid) == 0) ;

   free (theScsicmd.scsi_cmd);
}


int main (int argc, char *argv[]) {
   int i;
   int sects;
   int time1;
   int ticks;
   int rate = ae_getrate ();
   int bpms;
   char *buf;
   int diskno;

   if (argc != 2) {
      printf ("Usage: %s <diskno>\n", argv[0]);
      exit (0);
   }
   diskno = atoi(argv[1]);

   buf = malloc (BUF_SIZE);
   assert (buf != NULL);
   assert (((u_int)buf % 4096) == 0);
   bzero (buf, BUF_SIZE);

printf ("Read Bandwidth\n");

   for (sects = 1; sects <= TEST_MAXSIZE; sects *= 2) {
      time1 = ae_gettick ();

      for (i = START_SECT; (i+sects) <= (START_SECT + MAX_NUMSECTS); i += sects) {
         do_io (diskno, i, (sects * SECT_SIZE), &buf[((i-START_SECT) * SECT_SIZE)], B_READ);
      }

      ticks = max (1, (ae_gettick () - time1));
      bpms = (MAX_NUMSECTS * SECT_SIZE) / (ticks * rate / 1000) * 1000;
      printf ("block_size %5d, bytes/sec %4d, ticks %d\n",
               (sects * SECT_SIZE), bpms, ticks);
   }

printf ("\n");
  
printf ("Write Bandwidth\n");

   for (sects = 1; sects <= TEST_MAXSIZE; sects *= 2) {
      time1 = ae_gettick ();

      for (i = START_SECT; (i+sects) <= (START_SECT + MAX_NUMSECTS); i += sects) {
/*
printf ("going to write block %d for %d sectors\n", i, sects);
*/
         do_io (diskno, i, (sects * SECT_SIZE), &buf[((i-START_SECT) * SECT_SIZE)], B_WRITE);
      }

      ticks = max (1, (ae_gettick () - time1));
      bpms = (MAX_NUMSECTS * SECT_SIZE) / (ticks * rate / 1000) * 1000;
      printf ("block_size %5d, bytes/sec %4d, ticks %d\n",
               (sects * SECT_SIZE), bpms, ticks);

   }

   printf ("completed successfully\n");
   exit (0);
}

