
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


#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include "assert.h"

typedef unsigned short uint16;

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

char buf[65536];

int main (argc, argv)
int argc;
char **argv;
{
   int ret;
   int filefd = 0;
   int filesize = 0;
   long sum = 0;

   if (argc != 2) {
      fprintf (stderr, "Usage: %s <filename>\n", argv[0]);
      exit (0);
   }
   if (strcmp (argv[1], "stdin") != 0) {
      if ((filefd = open (argv[1], 0 /* O_RDONLY */)) < 0) {
         fprintf (stderr, "Unable to open file: %s\n", argv[1]);
         exit (0);
      }
   }

   while ((ret = read (filefd, &buf[0], 8192)) > 0) {
      filesize += ret;
      if (ret % 2) {
         printf ("filesize so far %d (ret %d)\n", filesize, ret);
      }
      assert ((ret % 2) == 0);
      sum = inet_sum ((uint16 *) &buf[0], (uint16) ret, sum);
   }

   if (ret < 0) {
      perror ("file read failed");
   }

   close (filefd);

   fprintf (stderr, "Done: size %d, sum %x\n", filesize, (uint)sum);

   return 0;
}

