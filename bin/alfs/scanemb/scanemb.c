
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
#include "alfs/alfs.h"
#include "alfs/alfs_buffer.h"
#include "alfs/alfs_inode.h"
#include "alfs/alfs_path.h"
#include "alfs/alfs_embdir.h"
#include "alfs/alfs_alloc.h"

#include <assert.h>
#include <string.h>
#include <memory.h>

extern inode_t *alfs_currentRootInode;

void list_files (char *pathname, int nameLen, inode_t *startInode)
{
   inode_t *inode;
   int ret = alfs_path_translateNameToInode (pathname, startInode, &inode, 0);

   assert (ret == OK);
/*
printf ("list_files: %s (%d, %d)\n", pathname, inode->inodeNum, alfs_inode_getLength(inode));
*/
   if ((alfs_inode_getType(inode) & 0x00000001) == INODE_FILE) {
      printf ("%s\n", pathname);
   } else if ((alfs_inode_getType(inode) & 0x00000001) == INODE_DIRECTORY) {
      char *dirBlock;
      int dirBlockLength;
      buffer_t *dirBlockBuffer;
      int dirBlockNumber = 0;
      embdirent_t *dirent;
      int currPos = 0;
      int i;

printf ("directory: %s (%d, %d)\n", pathname, inode->inodeNum, alfs_inode_getLength(inode));

      while (currPos < alfs_inode_getLength(inode)) {
         dirBlockLength = ((alfs_inode_getLength(inode) - currPos) < BLOCK_SIZE) ? (alfs_inode_getLength(inode) - currPos) : BLOCK_SIZE;
         dirBlockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, dirBlockNumber, BUFFER_READ);
         dirBlockNumber++;
         dirBlock = dirBlockBuffer->buffer;
         for (i=0; i<dirBlockLength; i += dirent->entryLen) {
            dirent = (embdirent_t *) (dirBlock + i);
/*
if (dirent->type != 0) {
printf ("directory entry: %s/%s (%d)\n", pathname, dirent->name, dirent->entryLen);
}
*/
            if ((dirent->type != 0) && ((dirent->nameLen != 1) || (dirent->name[0] != '.')) && ((dirent->nameLen != 2) || (dirent->name[0] != '.') || (dirent->name[1] != '.'))) {
               if (pathname[nameLen] != '/') {
                  pathname[nameLen] = '/';
                  bcopy (dirent->name, &pathname[(nameLen+1)], dirent->nameLen);
                  pathname[(nameLen + dirent->nameLen + 1)] = (char) 0;
                  list_files (pathname, (nameLen + dirent->nameLen + 1), startInode);
               } else {
                  bcopy (dirent->name, &pathname[nameLen], dirent->nameLen);
                  pathname[(nameLen + dirent->nameLen)] = (char) 0;
                  list_files (pathname, (nameLen + dirent->nameLen), startInode);
               }
               pathname[nameLen] = (char) 0;
            }
         }
         alfs_buffer_releaseBlock (dirBlockBuffer, 0);
         currPos += dirBlockLength;
      }
   } else {
      printf ("Unknown file type: %d\n", alfs_inode_getType(inode));
      exit(0);
   }
   alfs_inode_releaseInode (inode, 0);
#ifdef RMALFS
   if (strcmp (pathname, "/") != 0) {
      int ret = alfs_unlink (pathname);
      assert (ret == 0);
   }
#endif
}


void main (int argc, char **argv)
{
   char pathname[4096];
   uint devno, superblkno;
   int ret, val;
   char devname[20];
   FILE *fp;

   if (argc != 2) {
      printf ("Usage: %s <devname>\n", argv[0]);
      exit (0);
   }

#ifdef RMALFS
   sprintf (devname, "%s", argv[1]);
   devno = atoi (&devname[(strlen(devname)-1)]);
#else
   devno = atoi(argv[1]);
   sprintf (devname, "/dev%d", devno);
#endif
   fp = fopen (devname, "r");
   assert (fp != NULL);
   fscanf (fp, "ALFS file system definition\n");
   ret = fscanf (fp, "devno: %d\n", &val);
   assert ((ret == 1) && (val == devno));
   ret = fscanf (fp, "superblkno: %d\n", &val);
   assert ((ret == 1) && (val > 0));
   superblkno = val;

   alfs_mountFS (devno, superblkno);

   pathname[0] = '/';
   pathname[1] = (char) 0;
   list_files (pathname, 1, alfs_currentRootInode);

#ifdef RMALFS
   {
   int ret;
   ret = alfs_alloc_uninitDisk (alfs_FScap, alfs_FSdev, alfs_superblkno, alfs_initmetablks(alfs_FSdev, alfs_superblock->size));
   ret = unlink (devname);
   assert (ret == 0);
   }
#else
   alfs_unmountFS();
#endif

   printf ("Done\n");
   exit (0);
}

