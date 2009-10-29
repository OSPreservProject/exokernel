
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

#include <xok/sys_ucall.h>

#include <exos/tick.h>
#include <exos/cap.h>

#include <xok/disk.h>
#include <xok/sysinfo.h>

#define MAKEDISKDEV(a,b,c)	(b)
#define disk_request(a)		sys_disk_request (&__sysinfo.si_pxn[diskno], (a), CAP_ROOT)


#define PAGESIZ 4096

#define TEST_SECONDS	10

#define SECTSPERTRACK	38
#define TRACKSPERCYL	8
#define ONECYLBASE	0
#define RANGEFROMBASE	100

#define MAX_NUMSECTS	1024
#define TEST_MAXSECTS	1024

char buf2[((MAX_NUMSECTS * 512)+PAGESIZ)];
char *buf = &buf2[0];

int diskno = -1;


int main (int argc, char *argv[]) {
   int sects;
   int time1;
   int rate = ae_getrate ();
   int bpms;
   struct buf theBuf;
   int numsects;
   int maxticks;
   int requests;
   int writestoo = 0;

   if (argc < 2) {
      printf ("Usage: %s <diskno> [ <writestoo> ]\n", argv[0]);
      exit (0);
   }
   diskno = atoi(argv[1]);

   if (argc > 2) {
      writestoo = atoi (argv[2]);
   }

printf ("buf %x, buf2 %x\n", (unsigned) buf, (unsigned) &buf2[0]);
   buf = &buf[(PAGESIZ - ((unsigned)buf & (PAGESIZ-1)))];

   theBuf.b_dev = MAKEDISKDEV (0, diskno, 2);
   theBuf.b_resptr = &theBuf.b_resid;
   theBuf.b_bcount = 512;
   theBuf.b_memaddr = buf;
   theBuf.b_blkno = 0;
   theBuf.b_flags = B_READ;
   theBuf.b_resid = -1;
   theBuf.b_next = NULL;

   numsects = __sysinfo.si_disks[diskno].d_size / __sysinfo.si_disks[diskno].d_bsize;
   maxticks = (TEST_SECONDS * 1000000) / rate;
  
printf ("Random Reads: buf %p, numsects %d, maxticks %d\n", buf, numsects, maxticks);

   theBuf.b_flags = B_READ;
   theBuf.b_memaddr = &buf[0];

   for (sects = 1; sects <= TEST_MAXSECTS; sects *= 2) {

      requests = 0;
      time1 = ae_gettick ();

      theBuf.b_bcount = sects * 512;

      while ((ae_gettick() - time1) < maxticks) {

         theBuf.b_blkno = random() % (numsects - sects);
         theBuf.b_resid = 0;

         if (disk_request (&theBuf)) {
            printf ("Unable to initiate request: sects %d, blkno %qu\n", sects, theBuf.b_blkno);
            exit (1);
         }
/*
printf ("request started: %d (itrs %d)\n", disk_poll(), itrs);
*/
         requests++;

         /* poll for completion */
         while ((theBuf.b_resid == 0) && (getpid())) ;

      }

      bpms = ((requests * sects) * 512) / (maxticks * rate / 1000) * 1000;
      printf ("block_size %5d, requests %d, bytes/sec %4d, request/sec %d\n", (sects * 512), requests, bpms, (requests / TEST_SECONDS));
   }
  
printf ("One-cyl Reads: buf %p, numsects %d, maxticks %d\n", buf, numsects, maxticks);

   theBuf.b_flags = B_READ;
   theBuf.b_memaddr = &buf[0];

   for (sects = 1; sects <= TEST_MAXSECTS; sects *= 2) {

      int n = 1;
      requests = 0;
      time1 = ae_gettick ();

      theBuf.b_bcount = sects * 512;

      while ((ae_gettick() - time1) < maxticks) {

         theBuf.b_blkno = ONECYLBASE + (n * SECTSPERTRACK * TRACKSPERCYL) + (random() % SECTSPERTRACK);
         theBuf.b_resid = 0;

         if (disk_request (&theBuf)) {
            printf ("Unable to initiate request: sects %d, blkno %qu\n", sects, theBuf.b_blkno);
            exit (1);
         }
/*
printf ("request started: %d (itrs %d)\n", disk_poll(), itrs);
*/
         n += ((requests / 100) % 2) ? -1 : 1;
         requests++;

         /* poll for completion */
         while ((theBuf.b_resid == 0) && (getpid())) ;

      }

      bpms = ((requests * sects) * 512) / (maxticks * rate / 1000) * 1000;
      printf ("block_size %5d, requests %d, bytes/sec %4d, request/sec %d\n", (sects * 512), requests, bpms, (requests / TEST_SECONDS));
   }
  
if (writestoo) {
printf ("Random Writes: buf %p, numsects %d, maxticks %d\n", buf, numsects, maxticks);

   theBuf.b_flags = B_WRITE;
   theBuf.b_memaddr = &buf[0];

   for (sects = 1; sects <= TEST_MAXSECTS; sects *= 2) {

      requests = 0;
      time1 = ae_gettick ();

      theBuf.b_bcount = sects * 512;

      while ((ae_gettick() - time1) < maxticks) {

         theBuf.b_blkno = random() % (numsects - sects);
         theBuf.b_resid = 0;

         if (disk_request (&theBuf)) {
            printf ("Unable to initiate request: sects %d, blkno %qu\n", sects, theBuf.b_blkno);
            exit (1);
         }
/*
printf ("request started: %d (itrs %d)\n", disk_poll(), itrs);
*/
         requests++;

         /* poll for completion */
         while ((theBuf.b_resid == 0) && (getpid())) ;

      }

      bpms = ((requests * sects) * 512) / (maxticks * rate / 1000) * 1000;
      printf ("block_size %5d, requests %d, bytes/sec %4d, request/sec %d\n", (sects * 512), requests, bpms, (requests / TEST_SECONDS));
   }
  
printf ("One-cyl Writes: buf %p, numsects %d, maxticks %d\n", buf, numsects, maxticks);

   theBuf.b_flags = B_WRITE;
   theBuf.b_memaddr = &buf[0];

   for (sects = 1; sects <= TEST_MAXSECTS; sects *= 2) {

      int n = 1;
      requests = 0;
      time1 = ae_gettick ();

      theBuf.b_bcount = sects * 512;

      while ((ae_gettick() - time1) < maxticks) {

         theBuf.b_blkno = ONECYLBASE + (n * SECTSPERTRACK * TRACKSPERCYL) + (random() % SECTSPERTRACK);
         theBuf.b_resid = 0;

         if (disk_request (&theBuf)) {
            printf ("Unable to initiate request: sects %d, blkno %qu\n", sects, theBuf.b_blkno);
            exit (1);
         }
/*
printf ("request started: %d (itrs %d)\n", disk_poll(), itrs);
*/
         n += ((requests / 100) % 2) ? -1 : 1;
         requests++;

         /* poll for completion */
         while ((theBuf.b_resid == 0) && (getpid())) ;

      }

      bpms = ((requests * sects) * 512) / (maxticks * rate / 1000) * 1000;
      printf ("block_size %5d, requests %d, bytes/sec %4d, request/sec %d\n", (sects * 512), requests, bpms, (requests / TEST_SECONDS));
   }
}

   printf ("Done\n");
   exit (0);
}
