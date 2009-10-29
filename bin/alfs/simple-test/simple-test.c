
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
#include <stdlib.h>

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
   int fds[MAX_FDS];
   char line[4096];
   int ret;
   uint devno, superblkno;

   if (argc != 2) {
      printf ("Usage: %s <devname>\n", argv[0]);
      exit (0);
   }

   devno = atoi(argv[1]);
   superblkno = alfs_initFS (devno, 4194304, alfs_makeCap());
   alfs_mountFS (devno, superblkno);

   printf("Initialized successfully\n");

   for (ret = 0; ret < 511; ret++) {
      line[ret] = 'Z';
   }
   line[511] = 0;
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("big name alfs_open: %d ", ret);
   printRet(ret);
/*
   alfs_listDirectory(NULL);
*/
   line[255] = 0;
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("ok name alfs_open: %x ", ret);
   printRet(ret);
/*
   alfs_listDirectory(NULL);
*/
   ret = alfs_open("file1", OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open: %x ", ret);
   printRet(ret);
   fds[0] = ret;
/*
   alfs_listDirectory(NULL);
   ret = alfs_open("file1", OPT_RDWR | OPT_CREAT, 0777);
   printf("repeated alfs_open: %d ", ret);
   printRet(ret);
*/
/*
   alfs_listDirectory(NULL);
*/
   sprintf(line, "A");
   ret = alfs_write(fds[0], line, 1);
   printf("alfs_write: ");
   printRet(ret);

   ret = alfs_lseek(fds[0], 0, 0);
   printf("alfs_lseek: ");
   printRet(ret);

   sprintf(line, "B");
   ret = alfs_read(fds[0], line, 1);
   printf("alfs_read: %c ", line[0]);
   printRet(ret);

   ret = alfs_close(fds[0]);
   printf("alfs_close: ");
   printRet(ret);
/*
   alfs_listDirectory(NULL);
   printf("allocedBlocks %d\n", allocedBlocks);
*/
   ret = alfs_unlink("file1");
   printf("alfs_unlink: ");
   printRet(ret);
/*
   printf("allocedBlocks %d\n", allocedBlocks);
   alfs_listDirectory(NULL);
*/
   ret = alfs_open("file1", OPT_RDONLY, 0);
   printf("broken alfs_open: ");
   printRet(ret);

   for (ret = 0; ret < 256; ret++) {
      line[ret] = 'Z';
   }
   line[255] = 0;
   ret = alfs_unlink(line);
   printf("alfs_unlink: ");
   printRet(ret);

   alfs_listDirectory(NULL);
     
   for (ret=0; ret<246; ret++) {
      line[ret] = 'A';
   }
   line[246] = '1';
   line[247] = 0;
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #1: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #1: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '2';
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #2: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #2: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '3';
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #3: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #3: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '4';
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #4: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #4: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '5';
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #5: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #5: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '6';
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #6: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #6: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '7';
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #7: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #7: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '8';
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #8: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #8: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '3';
   ret = alfs_unlink(line);
   printf("alfs_unlink #3: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   line[246] = '8';
/*
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open #8: %d ", ret);
   printRet(ret);
   ret = alfs_close(ret);
   printf("alfs_close #8: ");
   printRet(ret);

   alfs_listDirectory(NULL);
*/
   ret = alfs_unlink(line);
   printf("alfs_unlink #8: ");
   printRet(ret);

   alfs_listDirectory(NULL);

   for (ret = 0; ret < 511; ret++) {
      line[ret] = 'Z';
   }
   line[248] = 0;
   ret = alfs_open(line, OPT_RDWR | OPT_CREAT, 0777);
   printf("ok name alfs_open: %d ", ret);
   printRet(ret);

   alfs_listDirectory(NULL);

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

   ret = alfs_open("file2", OPT_RDWR | OPT_CREAT, 0777);
   printf("alfs_open: %d ", ret);
   printRet(ret);
   fds[0] = ret;

   ret = alfs_write(fds[0], spaceA, 4096);
   printf("alfs_write spaceA: ");
   printRet(ret);
   ret = alfs_write(fds[0], spaceB, 4096);
   printf("alfs_write spaceB: ");
   printRet(ret);
   ret = alfs_write(fds[0], spaceC, 4096);
   printf("alfs_write spaceC: ");
   printRet(ret);
   ret = alfs_write(fds[0], spaceD, 4096);
   printf("alfs_write space[A-D]: ");
   printRet(ret);

   ret = alfs_lseek(fds[0], 8192, 0);
   printf("alfs_lseek: ");
   printRet(ret);

   ret = alfs_read(fds[0], line, 4096);
   printf("alfs_read spaceC: ");
   printRet(ret);
   printf("compare to written values: %d\n", bcmp(spaceC, line, 4096));

   alfs_listDirectory(NULL);
/*
   printf("allocedBlocks %d\n", allocedBlocks);
*/
   ret = alfs_unlink("file2");
   printf("alfs_unlink file2: ");
   printRet(ret);
/*
   printf("allocedBlocks %d\n", allocedBlocks);
*/

#if 1
   ret = alfs_mkdir ("dir1", 0777);
   printf ("alfs_mkdir dir1: ");
   printRet(ret);

   ret = alfs_unlink ("dir1/d");
   printf ("unlink non-existent file: ");
   printRet(ret);

   ret = alfs_mkdir ("dir1/dir2", 0777);
   printf ("alfs_mkdir dir1/dir2: ");
   printRet (ret);

   ret = alfs_rmdir ("dir1");
   printf ("alfs_rmdir nonempty directory: ");
   printRet (ret);

   ret = alfs_unlink ("dir1/dir2");
   printf ("alfs_unlink dir1/dir2: ");
   printRet (ret);

   ret = alfs_rmdir ("dir1");
   printf ("alfs_rmdir dir1: ");
   printRet (ret);
#endif

   alfs_unmountFS ();

   {extern int alfs_alloced;
   printf ("done: alfs_alloced (%d)\n", alfs_alloced);
   }
   exit (0);
}
