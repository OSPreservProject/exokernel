
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
#include "alfs/alfs_inode.h"
#include "alfs/alfs_buffer.h"
#include "alfs/alfs_path.h"
#include "alfs/alfs_dir.h"

#include <assert.h>
#include <string.h>
#include <memory.h>

#include "webfs.h"

extern inode_t *alfs_currentRootInode;

void list_files (char *pathname, int nameLen, inode_t *startInode)
{
   inode_t *inode;
   int ret = alfs_path_translateNameToInode (pathname, startInode, &inode, 0);

   if (ret != OK) {
      printf("ret %d\n", ret);
   }
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
      dirent_t *dirent;
      int currPos = 0;
      int i;

      while (currPos < alfs_inode_getLength(inode)) {
         dirBlockLength = ((alfs_inode_getLength(inode) - currPos) < BLOCK_SIZE) ? (alfs_inode_getLength(inode) - currPos) : BLOCK_SIZE;
         dirBlockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, dirBlockNumber, BUFFER_READ);
         dirBlockNumber++;
         assert (dirBlockBuffer != 0);
         dirBlock = dirBlockBuffer->buffer;
         assert (dirBlock != 0);
         for (i=0; i<dirBlockLength; i += dirent->entryLen) {
            dirent = (dirent_t *) (dirBlock + i);
/*
if (dirent->inodeNum != 0) {
printf ("directory entry: %s/%s (%d)\n", pathname, dirent->name, dirent->entryLen);
}
*/
            if ((dirent->inodeNum != 0) && (dirent->inodeNum != inode->inodeNum) && ((dirent->nameLen != 2) || (dirent->name[0] != '.') || (dirent->name[1] != '.'))) {
               pathname[nameLen] = '/';
               bcopy (dirent->name, &pathname[(nameLen+1)], dirent->nameLen);
               pathname[(nameLen + dirent->nameLen + 1)] = (char) 0;
               list_files (pathname, (nameLen + dirent->nameLen + 1), startInode);
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
}


int main (int argc, char **argv)
{
   char pathname[4096];

   if (argc != 2) {
      printf ("Usage: %s <devname>\n", argv[0]);
      exit (0);
   }
   webfs_mountFS (argv[1]);
   webfs_listDirectory(NULL);
   pathname[0] = '/';
   pathname[0] = (char) 0;
   list_files (pathname, 0, alfs_currentRootInode);

   printf ("done\n");
   exit (0);
}

