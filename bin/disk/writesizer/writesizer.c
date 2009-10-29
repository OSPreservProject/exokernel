
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
#include <assert.h>
#include <unistd.h>

#include <xok/sys_ucall.h>

#include <exos/tick.h>
#include <exos/cap.h>

#include <xok/disk.h>
#include <xok/sysinfo.h>

#define MAKEDISKDEV(a,b,c)	(b)
#define disk_request(a)		sys_disk_request (&__sysinfo.si_pxn[diskno], (a), CAP_ROOT)


struct test2 {
   char dumMe[74];
   char firstDigit;
   char secondDigit;
   char dumMe2[60];
   char theKey[376];
};

#define START_SECT	32768
#define MAX_NUMSECTS	1024

/*
char buf[(MAX_NUMSECTS * 512)];
char buf2[(MAX_NUMSECTS * 512)];
*/
char *buf;
char *buf2;
struct test2 *bobo2;
struct test2 *bobo;

int diskno = -1;


void reset_position (struct buf *bp)
{
   bp->b_blkno = 400000;
   bp->b_bcount = 512;
   bp->b_flags = B_WRITE;
   bp->b_memaddr = &buf2[0];
   bp->b_resid = 0;
   if (disk_request (bp)) {
      printf ("Unable to initiate request: sects %d, i %qu\n", bp->b_bcount, bp->b_blkno);
   }

   /* poll for completion */
   while ((bp->b_resid == 0) && (getpid())) ;
}


int main (int argc, char *argv[]) {
   int i, j;
   int matches;
   char crap = 0;
   int sects;
   int time1;
   int ticks;
   int rate = ae_getrate ();
   int bpms;
   struct buf theBuf;

   buf = malloc (MAX_NUMSECTS * 512);
   buf2 = malloc (MAX_NUMSECTS * 512);

   assert (buf && buf2);

   if (argc != 2) {
      printf ("Usage: %s <diskno>\n", argv[0]);
      exit (0);
   }
   diskno = atoi(argv[1]);

   theBuf.b_dev = MAKEDISKDEV (0, diskno, 2);
   theBuf.b_resptr = &theBuf.b_resid;
  
   theBuf.b_blkno = START_SECT;
   theBuf.b_bcount = MAX_NUMSECTS * 512;
   theBuf.b_flags = B_READ;
   theBuf.b_memaddr = &buf[0];
   theBuf.b_resid = 0;
   if (disk_request (&theBuf)) {
      printf ("Unable to initiate request: sects %d, i %qu\n", theBuf.b_bcount, theBuf.b_blkno);
      exit (1);
   }

   /* poll for completion */
   while ((theBuf.b_resid == 0) && (getpid())) ;

   /* since can't scan large area */
   bobo = (struct test2 *) &buf[0];

   matches = 0;
   for (j=0; j<MAX_NUMSECTS; j++) {
      if (strncmp (bobo->theKey, "Hab in einer stern", 10) == 0) {
         matches++;
      }
      bobo++;
   }

   printf ("main read completed (matches %2d)\n", matches);
  
   theBuf.b_blkno = 400000;
   theBuf.b_bcount = 512;
   theBuf.b_flags = B_READ;
   theBuf.b_memaddr = &buf2[0];
   theBuf.b_resid = 0;
   if (disk_request (&theBuf)) {
      printf ("Unable to initiate request: sects %d, i %qu\n", theBuf.b_bcount, theBuf.b_blkno);
      exit (1);
   }

   /* poll for completion */
   while ((theBuf.b_resid == 0) && (getpid())) ;

   printf ("secondary read completed\n");
  
printf ("Write Bandwidth\n");

   for (sects = 116; sects <= (MAX_NUMSECTS / 2); sects++) {
      time1 = ae_gettick ();

      for (i = START_SECT; i < (START_SECT + (2*sects)); i++) {

         reset_position (&theBuf);

         theBuf.b_blkno = i;
         theBuf.b_bcount = sects * 512;
         theBuf.b_flags = B_WRITE;
         theBuf.b_memaddr = &buf[((i-START_SECT) * 512)];
         theBuf.b_resid = 0;
printf ("going to write block %d for %d sectors\n", i, sects);
         if (disk_request (&theBuf)) {
            printf ("Unable to initiate request: sects %d, i %d\n", sects, i);
            exit (1);
         }

         /* poll for completion */
         while ((theBuf.b_resid == 0) && (getpid())) ;

      }

      ticks = ae_gettick () - time1;
      bpms = 100 * ((2 * sects * sects * 5120) / (ticks * rate / 1000));
      printf ("block_size %5d, sects %3d, bytes/sec %4d, ticks %d\n",
               (sects * 512), sects, bpms, ticks);
   }

   printf ("done (crap %x)\n", crap);
   exit (1);
}
