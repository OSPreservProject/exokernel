
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
#include <assert.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <xok/sys_ucall.h>

#include <exos/tick.h>
#include <exos/osdecl.h>
#include <exos/cap.h>

#include <xok/disk.h>
#include <xok/sysinfo.h>


static long inet_sum(addr, count, start)
uint16 *addr;
uint16 count;
long start;
{
    register long sum = start;

    /*  This is the inner loop */
    while( count > 1 )  {
        sum += * (unsigned short *) addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if(count > 0)
        sum += * (unsigned char *) addr;

    return sum;
}


char buf[131072];

int main (int argc, char *argv[])
{
   struct buf theBuf;
   int rate = ae_getrate ();
   int ticks;
   int time1;
   int bpms;
   int bytes = 0;
   int lasttime1;
   int diskno = -1;
   int disksize = -1;
   long sum = 0;

   if (argc != 2) {
      printf ("Usage: %s <diskno>\n", argv[0]);
      exit (1);
   }
   diskno = atoi(argv[1]);

   assert ((diskno >= 0) && (diskno < __sysinfo.si_ndisks));

   bzero (&buf[0], 131072);

   theBuf.b_dev = diskno;
   theBuf.b_memaddr = &buf[0];
   theBuf.b_resptr = &theBuf.b_resid;
   theBuf.b_flags = B_READ;
   theBuf.b_bcount = 512;
   disksize = __sysinfo.si_disks[diskno].d_size;
   assert (disksize > 0);

   theBuf.b_blkno = 0;

   time1 = ae_gettick();
   lasttime1 = time1;

   while (disksize > 0) {

      int bcount = 512 * min (disksize, (32768 / 512));
      theBuf.b_bcount = bcount;
      theBuf.b_resid = 0;
      if ( sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, CAP_ROOT) != 0) {

         printf ("unable to start request\n");
         break;
      }
      while ((getpid()) && (theBuf.b_resid == 0)) ;

      sum = inet_sum ((uint16 *) &buf[0], bcount, sum);

      bytes += bcount;

      theBuf.b_blkno += bcount / 512;
      disksize -= bcount / 512;
   }

   ticks = ae_gettick () - time1;
   bpms = ((theBuf.b_blkno * 512) / (ticks * rate / 1000)) * 1000;
   printf ("total bytes: %8qu, overall bytes/sec %6d, total seconds %d\n", theBuf.b_blkno * 512, bpms, (ticks * rate / 1000000));

   printf ("Done: sum %x\n", (unsigned) sum);
   exit (1);
}

