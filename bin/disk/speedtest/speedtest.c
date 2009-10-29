
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

#include <exos/tick.h>
#include <exos/cap.h>

#include <xok/disk.h>
#include <xok/sysinfo.h>


#define START_SECT	0
#define SECT_SIZE	512
#define MAX_NUMSECTS	10240
#define TEST_MAXSIZE	1024
#define BUF_SIZE	(MAX_NUMSECTS * SECT_SIZE)


int main (int argc, char *argv[]) {
   int i;
   int sects;
   int time1;
   int ticks;
   int rate = ae_getrate ();
   int bpms;
   struct buf theBuf;
   char *buf;
   int diskno;
   int ret;

   if (argc != 2) {
      printf ("Usage: %s <diskno>\n", argv[0]);
      exit (0);
   }
   diskno = atoi(argv[1]);

   buf = malloc (BUF_SIZE);
   assert (buf != NULL);
   assert (((u_int)buf % 4096) == 0);
   bzero (buf, BUF_SIZE);

   theBuf.b_dev = diskno;
   theBuf.b_resptr = &theBuf.b_resid;
  
printf ("Read Bandwidth\n");

   for (sects = 1; sects <= TEST_MAXSIZE; sects *= 2) {
      time1 = ae_gettick ();

      for (i = START_SECT; (i+sects) <= (START_SECT + MAX_NUMSECTS); i += sects) {

         theBuf.b_blkno = i;
         theBuf.b_bcount = sects * SECT_SIZE;
         theBuf.b_flags = B_READ;
         theBuf.b_memaddr = &buf[((i-START_SECT) * SECT_SIZE)];
         theBuf.b_resid = 0;
         theBuf.b_resptr = &theBuf.b_resid;
		/* This is iffy, with respect to poorly conceived kernel prot. */
	 if (ret = sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, 
				     CAP_ROOT)) {
            printf ("Unable to initiate request: ret %d, sects %d, i %d\n", ret, sects, i);
            exit (1);
         }
/*
sleep(1);
printf ("request started: %d\n", ret);
*/

         /* poll for completion */
         while ((*((volatile int *)&theBuf.b_resid)) == 0) ;

      }

      ticks = ae_gettick () - time1;
      bpms = (MAX_NUMSECTS * SECT_SIZE) / (ticks * rate / 1000) * 1000;
      printf ("block_size %5d, bytes/sec %4d, ticks %d\n",
               (sects * SECT_SIZE), bpms, ticks);
   }

printf ("\n");
  
printf ("Write Bandwidth\n");

   for (sects = 1; sects <= TEST_MAXSIZE; sects *= 2) {
      time1 = ae_gettick ();

      for (i = START_SECT; (i+sects) <= (START_SECT + MAX_NUMSECTS); i += sects) {

         theBuf.b_blkno = i;
         theBuf.b_bcount = sects * SECT_SIZE;
         theBuf.b_flags = B_WRITE;
         theBuf.b_memaddr = &buf[((i-START_SECT) * SECT_SIZE)];
         theBuf.b_resid = 0;
         theBuf.b_resptr = &theBuf.b_resid;
/*
printf ("going to write block %d for %d sectors\n", i, sects);
*/
	 if (ret = sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, 
				     CAP_ROOT)) {
            printf ("Unable to initiate request: sects %d, i %d\n", sects, i);
            exit (1);
         }

         /* poll for completion */
         while (*((volatile int *)&theBuf.b_resid) == 0) ;

      }

      ticks = ae_gettick () - time1;
      ticks = (ticks == 0) ? 1 : ticks;
      bpms = (MAX_NUMSECTS * SECT_SIZE) / (ticks * rate / 1000) * 1000;
      printf ("block_size %5d, bytes/sec %4d, ticks %d\n",
               (sects * SECT_SIZE), bpms, ticks);

   }

   printf ("completed successfully\n");
   exit (0);
}

