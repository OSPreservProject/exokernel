
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


#include "fd/cffs/cffs.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/ioctl.h>

	/* GROK - to support "-e" option */
#include "fd/cffs/cffs_proc.h"


void usage(char *name) {
   printf("Usage: %s [ <pathname> | -e ]", name);
   exit(0);
}

int main (int argc, char **argv) {
   char *dirname = argv[1];
   int fd, ret;

   if (argc != 1 && argc != 2) usage(argv[0]);

   if (argc == 1) {
      dirname = getcwd (NULL, MAXPATHLEN);
   } else if (strcmp(argv[1], "-e") == 0) {
      dirname = getcwd (NULL, MAXPATHLEN);
   }

   assert (dirname != NULL);

   fd = open (dirname, O_RDONLY, 0);
   if (fd < 0) {
      printf ("Unable to open %s\n", dirname);
      perror ("Open failed");
      exit (0);
   }

   if ((argc > 1) && (strcmp(argv[1], "-e") == 0)) {
      struct file fil;
	/* Superblock is disk blkno #1 in fs */
      //FILEP_SETINODENUM(&fil,(((1 * 8) << 2) + 2));
      FILEP_SETINODENUM(&fil,(superblock->rootDInodeNum+1));
      cffs_printdirinfo (&fil);
      exit (0);
   }

   ret = ioctl (fd, CFFS_IOCTL_PRINTDIRINFO, NULL);
   if (ret != 0) {
      perror ("Unable to print dirinfo\n");
      exit (0);
   }

   exit (0);
}

