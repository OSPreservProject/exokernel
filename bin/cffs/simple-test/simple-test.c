
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


#include <assert.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <fd/cffs/cffs.h>

#define MAX_FDS 10

char space0[2048];
char spaceA[4096];
char spaceB[4096];
char spaceC[4096];
char spaceD[4096];
char spaceE[4096];
char spaceF[4096];
char spaceG[4096];
char spaceH[4096];


void printRet (ret)
int ret;
{
   if (ret >= 0) {
      printf ("OK\n");
      return;
   }

   switch (errno) {
      case ENOENT: printf ("ALFS_ENOENT\n"); break;
      case EACCES: printf ("ALFS_EACCESS\n"); break;
      case EPERM: printf ("ALFS_EPERM\n"); break;
      case EBUSY: printf ("ALFS_EBUSY\n"); break;
      case ENFILE: printf ("ALFS_ENFILE\n"); break;
      case EISDIR: printf ("ALFS_EISDIR\n"); break;
      case EBADF: printf ("ALFS_EBADF\n"); break;
      case EINVAL: printf ("ALFS_EINVAL\n"); break;
      case EEXIST: printf ("ALFS_EEXIST\n"); break;
      case ENOTEMPTY: printf ("ALFS_ENOTEMPTY\n"); break;
      default: printf ("unknown error return val: %d\n", ret); break;
   }
}

int main (int argc, char **argv) {
   int fds[MAX_FDS];
   char line[4096];
   int ret;

   if (argc != 2) {
      printf ("Usage: %s <devname>\n", argv[0]);
      exit (0);
   }
/*
   alfs_initFS (argv[1], 4194304, alfs_makeCap());
   alfs_mountFS (argv[1]);
*/
   printf("Initialized successfully\n");

   for (ret = 0; ret < 511; ret++) {
      line[ret] = 'Z';
   }
   line[511] = 0;
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("big name open: %d ", ret);
   printRet(ret);

   line[255] = 0;
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("ok name open: %x ", ret);
   printRet(ret);

   ret = open("file1", O_RDWR | O_CREAT, 0777);
   printf("open: %x ", ret);
   printRet(ret);
   fds[0] = ret;
/*
   ret = open("file1", O_RDWR | O_CREAT, 0777);
   printf("repeated open: %d ", ret);
   printRet(ret);
*/

   sprintf(line, "A");
   ret = write(fds[0], line, 1);
   printf("write: ");
   printRet(ret);

   ret = lseek(fds[0], 0, 0);
   printf("lseek: ");
   printRet(ret);

   sprintf(line, "B");
   ret = read(fds[0], line, 1);
   printf("read: %c ", line[0]);
   printRet(ret);

   ret = close(fds[0]);
   printf("close: ");
   printRet(ret);
/*
   printf("allocedBlocks %d\n", allocedBlocks);
*/
   ret = unlink("file1");
   printf("unlink: ");
   printRet(ret);
/*
   printf("allocedBlocks %d\n", allocedBlocks);
*/
   ret = open("file1", O_RDONLY, 0);
   printf("broken open: ");
   printRet(ret);

   for (ret = 0; ret < 256; ret++) {
      line[ret] = 'Z';
   }
   line[255] = 0;
   ret = unlink(line);
   printf("unlink: ");
   printRet(ret);

   for (ret=0; ret<246; ret++) {
      line[ret] = 'A';
   }
   line[246] = '1';
   line[247] = 0;
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #1: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #1: ");
   printRet(ret);

   line[246] = '2';
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #2: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #2: ");
   printRet(ret);

   line[246] = '3';
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #3: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #3: ");
   printRet(ret);

   line[246] = '4';
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #4: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #4: ");
   printRet(ret);

   line[246] = '5';
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #5: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #5: ");
   printRet(ret);

   line[246] = '6';
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #6: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #6: ");
   printRet(ret);

   line[246] = '7';
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #7: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #7: ");
   printRet(ret);

   line[246] = '8';
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #8: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #8: ");
   printRet(ret);

   line[246] = '3';
   ret = unlink(line);
   printf("unlink #3: ");
   printRet(ret);

   line[246] = '8';
/*
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("open #8: %d ", ret);
   printRet(ret);
   ret = close(ret);
   printf("close #8: ");
   printRet(ret);
*/
   ret = unlink(line);
   printf("unlink #8: ");
   printRet(ret);

   for (ret = 0; ret < 511; ret++) {
      line[ret] = 'Z';
   }
   line[248] = 0;
   ret = open(line, O_RDWR | O_CREAT, 0777);
   printf("ok name open: %d ", ret);
   printRet(ret);

   for (ret=0; ret<2048; ret++) {
      space0[ret] = (char) 0;
   }

   for (ret=0; ret<4096; ret++) {
      spaceA[ret] = 'A';
      spaceB[ret] = 'B';
      spaceC[ret] = 'C';
      spaceD[ret] = 'D';
      spaceE[ret] = 'E';
      spaceF[ret] = 'F';
      spaceG[ret] = 'G';
      spaceH[ret] = 'H';
   }

   ret = open("file2", O_RDWR | O_CREAT, 0777);
   printf("open: %d ", ret);
   printRet(ret);
   fds[0] = ret;

   ret = write(fds[0], spaceA, 4096);
   printf("write spaceA: ");
   printRet(ret);
   ret = write(fds[0], spaceB, 4096);
   printf("write spaceB: ");
   printRet(ret);
   ret = write(fds[0], spaceC, 4096);
   printf("write spaceC: ");
   printRet(ret);
   ret = write(fds[0], spaceD, 4096);
   printf("write space[A-D]: ");
   printRet(ret);

   ret = lseek(fds[0], 8192, 0);
   printf("lseek: ");
   printRet(ret);

   ret = read(fds[0], line, 4096);
   printf("read spaceC: ");
   printRet(ret);
   printf("compare to written values: %d\n", bcmp(spaceC, line, 4096));

/*
   printf("allocedBlocks %d\n", allocedBlocks);
*/
   ret = unlink("file2");
   printf("unlink file2: ");
   printRet(ret);
/*
   printf("allocedBlocks %d\n", allocedBlocks);
*/

#if 1
   ret = mkdir ("dir1", 0777);
   printf ("mkdir dir1: ");
   printRet(ret);

   ret = unlink ("dir1/d");
   printf ("unlink non-existent file: ");
   printRet(ret);

   ret = mkdir ("dir1/dir2", 0777);
   printf ("mkdir dir1/dir2: ");
   printRet (ret);

   ret = rmdir ("dir1");
   printf ("rmdir nonempty directory: ");
   printRet (ret);

   ret = unlink ("dir1/dir2");
   printf ("unlink dir1/dir2: ");
   printRet (ret);

   ret = rmdir ("dir1");
   printf ("rmdir dir1: ");
   printRet (ret);
#endif
/*
   alfs_unmountFS ();
*/
   {extern int alloced;
   printf ("done: alloced (%d)\n", alloced);
   }
   exit (0);
}
