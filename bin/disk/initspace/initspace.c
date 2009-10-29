
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

#include <xok/mmu.h>

#include <exos/cap.h>

extern struct Env * __curenv;

unsigned int va2pa (unsigned int vaddr)
{
/*
  Pde *const pdir = __curenv->env_pdir;                                 
*/
  Pde pde;                                                       
  Pte pte;                                                       
           
/*
  pde = pdir[PDENO ((u_int)vaddr)];
*/
  pde = vpd[PDENO ((u_int)vaddr)];
  if (! (pde & PG_P))                                                  
    return 0;
  if (pde & PG_PS)                                              
    return ((pde & ~PDMASK) | ((u_int) vaddr & PDMASK));
/*
  pte = ((Pte*) ptov (pde & ~PGMASK))[PTENO (vaddr)];
  pte = ((Pte*) (pde & ~PGMASK))[PTENO (vaddr)];
*/
  pte = vpt [vaddr >> PGSHIFT];
  if (! (pte & PG_P))
    return 0;
  return ((pte & ~PGMASK) | ((u_int) vaddr & PGMASK));
}

void print_va2pa (unsigned int vaddr, int size)
{
   while (size > 0) {
      printf ("vaddr 0x%x translates to 0x%x\n", vaddr, va2pa(vaddr));
      vaddr += 4096;
      size -= 4096;
   }
}

struct test2 {
   char dumMe[74];
   unsigned char firstDigit;
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
  int i, j;
  int matches;
  char crap = 0;
  char *unable = "unable to initiate request #%d\n";
  struct buf theBuf;
  int diskno = -1;

  if (argc != 2) {
     printf ("Usage: %s <diskno>\n", argv[0]);
     exit (0);
  }
  diskno = atoi(argv[1]);

  theBuf.b_dev = diskno;
  theBuf.b_blkno = 0; /*417200;*/
/*
  theBuf.b_bcount = 131072;
*/
  theBuf.b_bcount = 65536;
  theBuf.b_memaddr = &buf[0];
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_READ;
  theBuf.b_resid = 0;

  if ( sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, CAP_ROOT) == 0) {
/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;
    printf ("read done: b_resid %d, ret %d (buf %x)\n", theBuf.b_resid, ret, (unsigned) buf);

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

printf ("now modify it\n");

  bobo = (struct test2 *) &buf;
  for (i=0; i<(131072/512); i++) {
     sprintf (bobo->theKey, "Hab in einer stern");
     bobo->firstDigit = i;
/*
printf ("(bobo-buf) %d, bobo->firstDigit %d\n", ((unsigned int)bobo - (unsigned int)buf), bobo->firstDigit);
*/
     bobo++;
  }
  
printf ("Write it out, baby\n");

  for (i = 0; i<4; i++) {
     theBuf.b_dev = diskno;
     theBuf.b_blkno = (i * (32768/512));
     theBuf.b_bcount = 32768;
     theBuf.b_memaddr = &buf[(i*32768)];
     theBuf.b_resptr = &theBuf.b_resid;
     theBuf.b_flags = B_WRITE;
     theBuf.b_resid = 0;

     if ( sys_disk_request (&__sysinfo.si_pxn[diskno], &theBuf, CAP_ROOT) == 0) {
/*
        printf ("request started successfully: %d\n", disk_poll());
*/
        while ((theBuf.b_resid == 0) && (getpid())) ;

        printf ("write done: b_resid %d, ret %d\n", theBuf.b_resid, ret);
     } else {
        printf (unable, 2);
     } 
  }

  /* read it back and check it */

#if 0
  theBuf.b_dev = MAKEDISKDEV (0, diskno, 1);
  theBuf.b_blkno = 0; /*417200;*/
  theBuf.b_bcount = 131072;
  theBuf.b_memaddr = &buf2[0];
  theBuf.b_resid = 0;
  theBuf.b_resptr = &theBuf.b_resid;
  theBuf.b_flags = B_READ;

  if ( disk_request (&theBuf) == 0) {
/*
    printf ("request started successfully: %d\n", disk_poll());
*/
    while ((theBuf.b_resid == 0) && (getpid())) ;
    printf ("re-read done: b_resid %d, ret %d (buf %x)\n", theBuf.b_resid, ret, (unsigned) buf);

    matches = 0;
    bobo2 = (struct test2 *) &buf2;
    for (j=0; j<(131072/512); j++) {
       if (strncmp (bobo2->theKey, "Hab in einer stern", 10) == 0) {
          matches++;
       }
/*
printf ("bobo2->firstDigit %d, match? %d\n", bobo2->firstDigit, strncmp(bobo2->theKey, "Hab in einer stern", 10));
*/
       bobo2++;
    }
    printf ("matched the key ourselves %d times (exact match %d)\n", matches, bcmp (buf, buf2, 131072));
  } else {
    printf (unable, 1);
    exit (1);
  } 
#endif

  print_va2pa ((unsigned int) &buf[0], 131072);

  printf ("done (crap %x)\n", crap);

  exit (1);
}
