
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

#define START_SECT	0
#define MAX_NUMSECTS	1024
#define TEST_MAXSECTS	128

/*
char buf[(MAX_NUMSECTS * 512)];
char buf2[(MAX_NUMSECTS * 512)];
struct test2 *bobo2 = (struct test2 *) &buf2[0];
struct test2 *bobo = (struct test2 *) &buf[0];
*/
char *buf;
char *buf2;
struct test2 *bobo;
struct test2 *bobo2;


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
   int diskno = -1;
   int reqcount = 0;

   buf = malloc (MAX_NUMSECTS * 512);
   buf2 = malloc (MAX_NUMSECTS * 512);
   assert (buf && buf2);

   if (argc != 2) {
      printf ("Usage: %s <diskno>\n", argv[0]);
      exit (0);
   }
   diskno = atoi(argv[1]);

   theBuf.b_dev = MAKEDISKDEV (0, diskno, 2);
   theBuf.b_resptr = &reqcount;
  
printf ("Read Bandwidth\n");

   for (sects = 1; sects <= TEST_MAXSECTS; sects *= 2) {
      time1 = ae_gettick ();

      matches = 0;

      for (i = START_SECT; (i+sects) <= (START_SECT + MAX_NUMSECTS); i += sects) {

         theBuf.b_blkno = i;
         theBuf.b_bcount = sects * 512;
         theBuf.b_flags = B_READ;
         theBuf.b_memaddr = &buf[((i-START_SECT) * 512)];
         reqcount++;
         if (disk_request (&theBuf)) {
            printf ("Unable to initiate request: sects %d, i %d\n", sects, i);
            exit (1);
         }

         /* out of the way request! */

         theBuf.b_blkno = 400000;
         theBuf.b_bcount = 8192;
         theBuf.b_flags = B_READ;
         theBuf.b_memaddr = &buf2[0];
         reqcount++;
         if (disk_request (&theBuf)) {
            printf ("Unable to initiate way out request\n");
            exit (1);
         }

         /* watch for overkill */
         while ((*theBuf.b_resptr >= 100) && (getpid())) ;

      }

      /* poll for completion */
      while ((*theBuf.b_resptr != 0) && (getpid())) ;

      /* since can't scan large area */
      bobo = (struct test2 *) &buf[0];

      for (j=0; j<MAX_NUMSECTS; j++) {
         if (strncmp (bobo->theKey, "Hab in einer stern", 10) == 0) {
            matches++;
         }
         bobo++;
      }

      ticks = ae_gettick () - time1;
      bpms = (MAX_NUMSECTS * 512000) / (ticks * rate / 1000);
      printf ("block_size %5d, matches %2d, bytes/sec %4d, ticks %d\n",
               (sects * 512), matches, bpms, ticks);
   }
  
printf ("Write Bandwidth\n");

   for (sects = 1; sects <= TEST_MAXSECTS; sects *= 2) {
      time1 = ae_gettick ();

      matches = 0;

      for (i = START_SECT; (i+sects) <= (START_SECT + MAX_NUMSECTS); i += sects) {

         theBuf.b_blkno = i;
         theBuf.b_bcount = sects * 512;
         theBuf.b_flags = B_WRITE;
         theBuf.b_memaddr = &buf[((i-START_SECT) * 512)];
         reqcount++;
         if (disk_request (&theBuf)) {
            printf ("Unable to initiate request: sects %d, i %d\n", sects, i);
            exit (1);
         }

         /* out of the way request! */

         theBuf.b_blkno = 400000;
         theBuf.b_bcount = 8192;
         theBuf.b_flags = B_READ;
         theBuf.b_memaddr = &buf2[0];
         reqcount++;
         if (disk_request (&theBuf)) {
            printf ("Unable to initiate way out request\n");
            exit (1);
         }

         /* watch for overflow */
         while ((*theBuf.b_resptr >= 100) && (getpid())) ;

      }

      /* poll for completion */
      while ((*theBuf.b_resptr != 0) && (getpid())) ;

      /* since can't scan large area */
      bobo = (struct test2 *) &buf[0];

      for (j=0; j<MAX_NUMSECTS; j++) {
         if (strncmp (bobo->theKey, "Hab in einer stern", 10) == 0) {
            matches++;
         }
         bobo++;
      }

      ticks = ae_gettick () - time1;
      bpms = (MAX_NUMSECTS * 512000) / (ticks * rate / 1000);
      printf ("block_size %5d, matches %2d, bytes/sec %4d, ticks %d\n",
               (sects * 512), matches, bpms, ticks);
/*
      if (sects == 64) {
         sects = 59;
      }
*/
   }

   printf ("done (crap %x)\n", crap);
   exit (1);
}
