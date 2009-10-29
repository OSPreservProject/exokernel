
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
#include <xok/disk.h>
#include <xok/sysinfo.h>

#include <exos/cap.h>


#define TEST_SIZE	65536


struct test2 {
   char dumMe[74];
   char firstDigit;
   char secondDigit;
   char dumMe2[60];
   char theKey[376];
};


char buf[131072];
char buf2[131072];
struct test2 *bobo = (struct test2 *) &buf;
struct test2 *bobo2 = (struct test2 *) &buf2;


int main (int argc, char *argv[]) {
  int ret = 0;
  int j;
  int matches;
  char crap = 0;
  char *unable = "unable to initiate request #%d\n";
  struct buf theBuf;
  int diskno = -1;
  
printf ("HELLLLO: buf %x, buf2 %x\n", (u_int) &buf[0], (u_int) &buf2[0]);

  if (argc != 2) {
     printf ("Usage: %s <diskno>\n", argv[0]);
     exit (0);
  }
  diskno = atoi(argv[1]);

  theBuf.b_dev = diskno;
  theBuf.b_blkno = 0; /*417200;*/
  theBuf.b_bcount = TEST_SIZE;
  theBuf.b_memaddr = &buf[0];
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_READ;
  theBuf.b_resid = 0;

  if ( sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, CAP_ROOT) == 0) {

/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;

    printf ("read done: b_resid %d, ret %d\n", theBuf.b_resid, ret);

    matches = 0;
    for (j=0; j<(131072/512); j++) {
       if (strncmp (bobo->theKey, "Hab in einer stern", 10) == 0) {
          matches++;
       }
       bobo++;
    }
    printf ("matched the key ourselves %d times\n", matches);

    theBuf.b_flags = B_WRITE;
    theBuf.b_bcount = TEST_SIZE;
    theBuf.b_resid = 0;
    if ( sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, CAP_ROOT) == 0) {

       printf ("waiting for write to complete\n");

       while ((theBuf.b_resid == 0) && (getpid())) ;

       printf ("write completed: b_resid %d\n", theBuf.b_resid);

       theBuf.b_flags = B_READ;
       theBuf.b_bcount = TEST_SIZE;
       theBuf.b_memaddr = &buf2[0];
       theBuf.b_resid = 0;
       if ( sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, CAP_ROOT) == 0) {


          while ((theBuf.b_resid == 0) && (getpid())) ;

          matches = 0;
          for (j=0; j<(131072/512); j++) {
             if (strncmp (bobo2->theKey, "Hab in einer stern", 10) == 0) {
                matches++;
             }
             bobo2++;
          }
          printf ("matched the key ourselves %d times\n", matches);

          if ((ret = bcmp (&buf[0], &buf2[0], 8192)) == 0) {
             printf ("reread of written block matches (%d times)\n", matches);
          } else {
             printf ("reread of written block failed: ret %d\n", ret);
          }

       } else {
          printf (unable, 3);
          exit (0);
       }
    } else {
       printf (unable, 2);
       exit (0);
    }

  } else {
    printf (unable, 1);
    exit (0);
  }

  printf ("done (%x)\n", crap);

  exit (1);
}
