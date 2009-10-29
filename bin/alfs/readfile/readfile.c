
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

#include <assert.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>


void printRet (ret)
int ret;
{
     if (ret >= 0) {
	  printf ("OK\n");
	  return;
     }

     switch (ret) {
	case ALFS_ELENGTH: printf ("ALFS_ELENGTH\n"); break;
	case ALFS_ENOENT: printf ("ALFS_ENOENT\n"); break;
	case ALFS_EACCESS: printf ("ALFS_EACCESS\n"); break;
	case ALFS_EPERM: printf ("ALFS_EPERM\n"); break;
	case ALFS_EINUSE: printf ("ALFS_EINUSE\n"); break;
	case ALFS_ENFILE: printf ("ALFS_ENFILE\n"); break;
	case ALFS_EISDIR: printf ("ALFS_EISDIR\n"); break;
	case ALFS_EBADF: printf ("ALFS_EBADF\n"); break;
	case ALFS_EINVAL: printf ("ALFS_EINVAL\n"); break;
	case ALFS_EEXIST: printf ("ALFS_EEXIST\n"); break;
	case ALFS_ENOTEMPTY: printf ("ALFS_ENOTEMPTY\n"); break;
	default: printf ("unknown error return val: %d\n", ret); break;
     }
}

int main (int argc, char **argv) {
   int filefd;
   char line[4100];
   int ret;
   int bytes = 0;
   struct alfs_stat statbuf;
   uint devno, superblkno;
   FILE *fp;
   char devname[20];
   uint val;

   if (argc != 3) {
      printf ("Usage: %s <devname> <filename>\n", argv[0]);
      exit (0);
   }

   devno = atoi(argv[1]);
   sprintf (devname, "/dev%d", devno);
   fp = fopen (devname, "r");
   assert (fp != NULL);
   fscanf (fp, "ALFS file system definition\n");
   ret = fscanf (fp, "devno: %d\n", &val);
   assert ((ret == 1) && (val == devno));
   ret = fscanf (fp, "superblkno: %d\n", &val);
   assert ((ret == 1) && (val > 0));
   superblkno = val;

   alfs_mountFS (devno, superblkno);

   printf("Initialized successfully\n");

   ret = alfs_open(argv[2], OPT_RDONLY, 0777);
   if (ret < 0) {
      printf ("unable to open file %s: ", argv[1]);
      printRet(ret);
      exit (0);
   }
   printf("open file: %d ", ret);
   printRet(ret);
   filefd = ret;

   ret = alfs_fstat (filefd, &statbuf);
   printf ("alfs_fstat: ");
   printRet(ret);
   if (ret != OK) {
      exit(0);
   }
   printf ("File length: %d\n", statbuf.st_size);

   bytes = 0;
   while ((ret = alfs_read(filefd, line, 4096)) > 0) {
      line[ret] = (char) 0;
      printf ("%s", line);
      bytes += ret;
   }
/*
printf ("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");

   printf ("bytes read: %d\n", bytes);
*/

   printf ("ret %d: \n", ret);
   printRet(ret);

   ret = alfs_close(filefd);
   printf("alfs_close: ");
   printRet(ret);

   printf ("bytes read: %d\n", bytes);

   alfs_unmountFS ();

   printf ("\ndone\n");
   exit (0);
}
