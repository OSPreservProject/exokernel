
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

#include <xok/disk.h>
#include <xok/sysinfo.h>

#include <exos/cap.h>

struct test2 {
   char dumMe[74];
   unsigned char firstDigit;
   char secondDigit;
   char dumMe2[60];
   char theKey[376];
};


#define MAKEDISKDEV(a,b,c)	(b)
#define disk_request(a)		sys_disk_request (&__sysinfo.si_pxn[diskno], (a), CAP_ROOT)



char buf3[131072];
char buf2[131072];
char buf[131072];
struct test2 *bobo = (struct test2 *) &buf;
struct test2 *bobo2 = (struct test2 *) &buf2;


int main (int argc, char *argv[]) {
  int ret = 0;
  int j;
  int matches;
  char crap = 0;
  char *unable = "unable to initiate request #%d\n";
  struct buf theBuf;
  struct buf theBuf2;
  struct buf theBuf3;
  struct buf theBuf4;
  int diskno = -1;

  if (argc != 2) {
     printf ("Usage: %s <diskno>\n", argv[0]);
     exit (0);
  }
  diskno = atoi(argv[1]);
  
printf ("HELLLLO\n");

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 131072;
  theBuf.b_memaddr = &buf2[0];
  theBuf.b_resid = 0;
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_READ;

  if ( disk_request (&theBuf) == 0) {

    while ((theBuf.b_resid == 0) && (getpid())) ;

    printf ("init read done: b_resid %d, ret %d\n", theBuf.b_resid, ret);

    matches = 0;
    bobo2 = (struct test2 *) &buf2;
    for (j=0; j<(131072/512); j++) {
       if (strncmp (bobo2->theKey, "Hab in einer stern", 10) == 0) {
          matches++;
       }
/*
       printf ("bobo2->firstDigit %d\n", bobo2->firstDigit);
*/
       bobo2++;
    }
    printf ("init req. matched the key %d times\n", matches);
  } else {
    printf ("unable to do initial request\n");
    exit (1);
  }

  /* Now do a scatter read */

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 32768;
  theBuf.b_sgtot = 131072;
  theBuf.b_memaddr = &buf[98304];
  theBuf.b_resid = 0;
  theBuf.b_resptr = 0;
  theBuf.b_sgnext = &theBuf2;
  theBuf.b_flags = B_SCATGATH | B_READ;
  theBuf2.b_memaddr = &buf[32768];
  theBuf2.b_bcount = 32768;
  theBuf2.b_resptr = 0;
  theBuf2.b_sgnext = &theBuf3;
  theBuf2.b_flags = B_SCATGATH | B_READ;
  theBuf3.b_memaddr = &buf[0];
  theBuf3.b_bcount = 32768;
  theBuf3.b_resptr = 0;
  theBuf3.b_sgnext = &theBuf4;
  theBuf3.b_flags = B_SCATGATH | B_READ;
  theBuf4.b_memaddr = &buf[65536];
  theBuf4.b_bcount = 32768;
  theBuf4.b_resptr = &theBuf.b_resid;
  theBuf4.b_flags = B_READ;

  if ( disk_request (&theBuf) == 0) {
/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;

    printf ("read done: b_resid %d, ret %d\n", theBuf.b_resid, ret);

    matches = 0;
    bobo = (struct test2 *) &buf;
    for (j=0; j<(131072/512); j++) {
       if (strncmp (bobo->theKey, "Hab in einer stern", 10) == 0) {
          matches++;
       }
       bobo++;
    }
    printf ("matched the key ourselves %d times\n", matches);
  } else {
    printf (unable, 1);
    exit (1);
  }

  printf ("bcmp (buf, buf2, 131072) == %d\n", bcmp (&buf[0], &buf2[0], 131072));
  printf ("bcmp (&buf[98304], &buf2[0], 32768) == %d\n", bcmp (&buf[98304], &buf2[0], 32768));
  assert (bcmp (&buf[98304], &buf2[0], 32768) == 0);
  printf ("bcmp (&buf[32768], &buf2[32768], 32768) == %d\n", bcmp (&buf[32768], &buf2[32768], 32768));
  assert (bcmp (&buf[32768], &buf2[32768], 32768) == 0);
  printf ("bcmp (&buf[0], &buf2[65536], 32768) == %d\n", bcmp (&buf[0], &buf2[65536], 32768));
  assert (bcmp (&buf[0], &buf2[65536], 32768) == 0);
  printf ("bcmp (&buf[65536], &buf2[98304], 32768) == %d\n", bcmp (&buf[65536], &buf2[98304], 32768));
  assert (bcmp (&buf[65536], &buf2[98304], 32768) == 0);

  /* Now do a small scatter read */

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 2048;
  theBuf.b_sgtot = 8192;
  theBuf.b_memaddr = &buf[6144];
  theBuf.b_resid = 0;
  theBuf.b_resptr = 0;
  theBuf.b_sgnext = &theBuf2;
  theBuf.b_flags = B_SCATGATH | B_READ;
  theBuf2.b_memaddr = &buf[2048];
  theBuf2.b_bcount = 2048;
  theBuf2.b_resptr = 0;
  theBuf2.b_sgnext = &theBuf3;
  theBuf2.b_flags = B_SCATGATH | B_READ;
  theBuf3.b_memaddr = &buf[0];
  theBuf3.b_bcount = 2048;
  theBuf3.b_resptr = 0;
  theBuf3.b_sgnext = &theBuf4;
  theBuf3.b_flags = B_SCATGATH | B_READ;
  theBuf4.b_memaddr = &buf[4096];
  theBuf4.b_bcount = 2048;
  theBuf4.b_resptr = &theBuf.b_resid;
  theBuf4.b_flags = B_READ;

  if ( disk_request (&theBuf) == 0) {
/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;

    printf ("read done: b_resid %d, ret %d\n", theBuf.b_resid, ret);

    matches = 0;
    bobo = (struct test2 *) &buf;
    for (j=0; j<(131072/512); j++) {
       if (strncmp (bobo->theKey, "Hab in einer stern", 10) == 0) {
          matches++;
       }
       bobo++;
    }
    printf ("matched the key ourselves %d times\n", matches);
  } else {
    printf (unable, 1);
    exit (1);
  }

  printf ("bcmp (buf, buf2, 8192) == %d\n", bcmp (&buf[0], &buf2[0], 8192));
  printf ("bcmp (&buf[6144], &buf2[0], 2048) == %d\n", bcmp (&buf[6144], &buf2[0], 2048));
  assert (bcmp (&buf[6144], &buf2[0], 2048) == 0);
  printf ("bcmp (&buf[2048], &buf2[2048], 2048) == %d\n", bcmp (&buf[2048], &buf2[2048], 2048));
  assert (bcmp (&buf[2048], &buf2[2048], 2048) == 0);
  printf ("bcmp (&buf[0], &buf2[4096], 2048) == %d\n", bcmp (&buf[0], &buf2[4096], 2048));
  assert (bcmp (&buf[0], &buf2[4096], 2048) == 0);
  printf ("bcmp (&buf[4096], &buf2[6144], 2048) == %d\n", bcmp (&buf[4096], &buf2[6144], 2048));
  assert (bcmp (&buf[4096], &buf2[6144], 2048) == 0);
/*
  bobo = (struct test2 *) &buf;
  bobo2 = (struct test2 *) &buf2;
  for (j=0; j<(8192/512); j++) {
     printf ("bobo->firstDigit %d, bobo2->firstDigit %d\n", bobo->firstDigit, bobo2->firstDigit);
     bobo++;
     bobo2++;
  }
*/

/* write some crap to re-init */
printf ("re-initialize first 32768\n");

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 32768;
  theBuf.b_memaddr = &buf3[0];
  theBuf.b_resid = 0;
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_WRITE;

  if ( disk_request (&theBuf) == 0) {
/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;

    printf ("write done: b_resid %d, ret %d\n", theBuf.b_resid, ret);
  } else {
    printf (unable, 2);
  }

/* rearrange for altered gather write */
printf ("gather write first 32768\n");

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 8192;
  theBuf.b_sgtot = 32768;
  theBuf.b_memaddr = &buf2[24576];
  theBuf.b_resid = 0;
  theBuf.b_resptr = 0;
  theBuf.b_sgnext = &theBuf2;
  theBuf.b_flags = B_SCATGATH | B_WRITE;
  theBuf2.b_memaddr = &buf2[8192];
  theBuf2.b_bcount = 16384;
  theBuf2.b_resptr = 0;
  theBuf2.b_sgnext = &theBuf3;
  theBuf2.b_flags = B_SCATGATH | B_WRITE;
  theBuf3.b_memaddr = &buf2[0];
  theBuf3.b_bcount = 8192;
  theBuf3.b_resptr = &theBuf.b_resid;
  theBuf3.b_flags = B_WRITE;

  if ( disk_request (&theBuf) == 0) {
/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;

    printf ("write done: b_resid %d, ret %d\n", theBuf.b_resid, ret);
  } else {
    printf (unable, 2);
  }

printf ("Now double-check what was written\n");

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 131072;
  theBuf.b_memaddr = &buf[0];
  theBuf.b_resid = 0;
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_READ;

  if ( disk_request (&theBuf) == 0) {
    while ((theBuf.b_resid == 0) && (getpid())) ;
  } else {
    printf ("unable to do doublecheck request\n");
  }

  printf ("bcmp (buf, buf2, 32768) == %d\n", bcmp (&buf[0], &buf2[0], 32768));
  printf ("bcmp (&buf[24576], &buf2[0], 8192) == %d\n", bcmp (&buf[24576], &buf2[0], 8192));
  assert (bcmp (&buf[24576], &buf2[0], 8192) == 0);
  printf ("bcmp (&buf[8192], &buf2[8192], 16384) == %d\n", bcmp (&buf[8192], &buf2[8192], 16384));
  assert (bcmp (&buf[8192], &buf2[8192], 16384) == 0);
  printf ("bcmp (&buf[0], &buf2[24576], 8192) == %d\n", bcmp (&buf[0], &buf2[24576], 8192));
  assert (bcmp (&buf[0], &buf2[24576], 8192) == 0);

/* rearrange for altered gather write */
printf ("gather write first 8192\n");

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 2048;
  theBuf.b_sgtot = 8192;
  theBuf.b_memaddr = &buf2[6144];
  theBuf.b_resid = 0;
  theBuf.b_resptr = 0;
  theBuf.b_sgnext = &theBuf2;
  theBuf.b_flags = B_SCATGATH | B_WRITE;
  theBuf2.b_memaddr = &buf2[2048];
  theBuf2.b_bcount = 4096;
  theBuf2.b_resptr = 0;
  theBuf2.b_sgnext = &theBuf3;
  theBuf2.b_flags = B_SCATGATH | B_WRITE;
  theBuf3.b_memaddr = &buf2[0];
  theBuf3.b_bcount = 2048;
  theBuf3.b_resptr = &theBuf.b_resid;
  theBuf3.b_flags = B_WRITE;

  if ( disk_request (&theBuf) == 0) {
/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;

    printf ("write done: b_resid %d, ret %d\n", theBuf.b_resid, ret);
  } else {
    printf (unable, 2);
  }

printf ("Now double-check what was written\n");

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 131072;
  theBuf.b_memaddr = &buf[0];
  theBuf.b_resid = 0;
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_READ;

  if ( disk_request (&theBuf) == 0) {
    while ((theBuf.b_resid == 0) && (getpid())) ;
  } else {
    printf ("unable to do doublecheck request\n");
  }

  printf ("bcmp (buf, buf2, 8192) == %d\n", bcmp (&buf[0], &buf2[0], 8192));
  printf ("bcmp (&buf[6144], &buf2[0], 2048) == %d\n", bcmp (&buf[6144], &buf2[0], 2048));
  assert (bcmp (&buf[6144], &buf2[0], 2048) == 0);
  printf ("bcmp (&buf[2048], &buf2[2048], 4096) == %d\n", bcmp (&buf[2048], &buf2[2048], 4096));
  assert (bcmp (&buf[2048], &buf2[2048], 4096) == 0);
  printf ("bcmp (&buf[0], &buf2[6144], 2048) == %d\n", bcmp (&buf[0], &buf2[6144], 2048));
  assert (bcmp (&buf[0], &buf2[6144], 2048) == 0);

printf ("clean up time (just in case)\n");

  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0;
  theBuf.b_bcount = 32768;
  theBuf.b_memaddr = &buf2[0];
  theBuf.b_resid = 0;
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_WRITE;

  if ( disk_request (&theBuf) == 0) {
    while ((theBuf.b_resid == 0) && (getpid())) ;
  } else {
    printf ("unable to do cleanup request\n");
    exit (1);
  }

  printf ("done (crap %x)\n", crap);

  exit (0);
}
