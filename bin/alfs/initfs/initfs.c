
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


#include "alfs/alfs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void main (int argc, char **argv)
{
   uint devno, superblkno;

   if (argc != 3) {
      printf("Usage: %s <devname> <size>\n", argv[0]);
      exit(0);
   }
   devno = atoi(argv[1]);
   superblkno = alfs_initFS (devno, atoi(argv[2]), alfs_makeCap());

   if (superblkno > 0) {
      int fd;
      FILE *fp;
      char devname[20];
      sprintf (devname, "/dev%d", devno);
      fd = open (devname, (O_CREAT|O_EXCL|O_RDWR), 0755);
      if (fd < 0) {
         printf ("could not create ALFS descriptor file named \"%s\"\n", devname);
         perror ("");
         exit(0);
      }
      fp = fdopen (fd, "w");
      assert (fp != NULL);
      fprintf (fp, "ALFS file system definition\n");
      fprintf (fp, "devno: %d\n", devno);
      fprintf (fp, "superblkno: %d\n", superblkno);
      fclose(fp);
   }

   printf ("Done (devno %d, superblkno %d)\n", devno, superblkno);
   exit (0);
}

