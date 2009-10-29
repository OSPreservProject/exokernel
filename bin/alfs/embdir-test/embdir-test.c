
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


#include "alfs/alfs_buffer.h"
#include "alfs/alfs.h"
#include "alfs/alfs_dinode.h"
#include "alfs/alfs_embdir.h"
#include "alfs/alfs_inode.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

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

void print_content (char type, char *content, int contentLen)
{
   printf ("content: %s\n", content);
}

void listEmbDirectory (inode_t *dirInode)
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   int currPos = 0;
   int i;

   while (currPos < alfs_inode_getLength(dirInode)) {
      dirBlockLength = ((alfs_inode_getLength(dirInode) - currPos) < BLOCK_SIZE) ? (alfs_inode_getLength(dirInode) - currPos) : BLOCK_SIZE;
      dirBlockBuffer = alfs_buffer_getBlock (dirInode->fsdev, dirInode->inodeNum, dirBlockNumber, BUFFER_READ);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      printf("Directory block: %x\n", (unsigned int) dirBlock);
      for (i=0; i<dirBlockLength; i += dirent->entryLen) {
         dirent = (embdirent_t *) (dirBlock + i);
         if (dirent->type != (char) 0) {
            printf("%x\t%x\t%d\t%d\t%d\t%s\t", (unsigned int) dirent, dirent->type, dirent->entryLen, dirent->contentLen, dirent->nameLen, dirent->name);
            print_content (dirent->type, embdirentcontent(dirent), (unsigned int) dirent->contentLen);
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
}

/*
void listDirectory (inode_t *dirInode)
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   int dirBlockNumber = 0;
   dirent_t *dirent;
   int currPos = 0;
   int i;

   while (currPos < alfs_inode_getLength(dirInode)) {
      dirBlockLength = ((alfs_inode_getLength(dirInode) - currPos) < BLOCK_SIZE) ? (alfs_inode_getLength(dirInode) - currPos) : BLOCK_SIZE;
      dirBlockBuffer = alfs_buffer_getBlock (dirInode->fsdev, dirInode->inodeNum, dirBlockNumber, BUFFER_READ);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (i=0; i<dirBlockLength; i += dirent->entryLen) {
         dirent = (dirent_t *) (dirBlock + i);
         if (dirent->inodeNum != 0) {
            printf("%x\t%d\t%d\t%x\t%d\t%s\n", dirent, dirent->inodeNum, dirent->entryLen, dirent->status, dirent->nameLen, dirent->name);
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
}
*/

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

void main (int argc, char **argv) {
     int fds[MAX_FDS];
     char line[4096];
     char line2[4096];
     int ret;
     inode_t *dirInode;
     buffer_t *buffer;
     embdirent_t *dirent;
     embdirent_t *dirent2;
     int i;
     uint devno, superblkno;

     if (argc != 2) {
        printf ("Usage: %s <devno>\n", argv[0]);
        exit (0);
     }

     devno = atoi(argv[1]);
     superblkno = alfs_initFS (devno, 33554432, alfs_makeCap());
     alfs_mountFS (devno, superblkno);

     for (i=0; i<4096; i++) {
        line[i] = (char) 0;
        line2[i] = 'a';
     }

     printf("Initialized successfully\n");

     ret = alfs_open("file1", OPT_RDWR | OPT_CREAT, 0777);
     printf("alfs_open: %d ", ret);
     printRet(ret);
     fds[0] = ret;

     dirInode = (inode_t *) ret;
     printf ("dirInode is %x\n", (unsigned int) dirInode);

     listEmbDirectory (dirInode);

     dirent = alfs_embdir_lookupname (alfs_FSdev, dirInode->dinode, "tongue", 6, &buffer);
     assert (dirent == NULL);

     ret = alfs_embdir_removeLink_byName (alfs_FSdev, dirInode->dinode, "tongue", 6, 0);
     assert (ret == 0);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "tongue", 6, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "aaaaa");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     listEmbDirectory (dirInode);

     dirent2 = alfs_embdir_lookupname (alfs_FSdev, dirInode->dinode, "tongue", 6, &buffer);
     assert (dirent2 == dirent);
     alfs_buffer_releaseBlock (buffer, 0);

     ret = alfs_embdir_removeLink_byName (alfs_FSdev, dirInode->dinode, "tongue", 6, 0);
     assert (ret == 1);

     listEmbDirectory (dirInode);

     dirent = alfs_embdir_lookupname (alfs_FSdev, dirInode->dinode, "tongue", 6, &buffer);
     assert (dirent == NULL);

     printf("should be back to square zero\n");

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "tongue", 6, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "bbbbb");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "in", 2, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "ccccc");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "cheek", 5, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "ddddd");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash", 6, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "eeeee");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     listEmbDirectory (dirInode);

     ret = alfs_embdir_removeLink_byName (alfs_FSdev, dirInode->dinode, "tongue", 6, 0);
     assert (ret == 1);

     printf("removed tongue\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "tongue", 6, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "fffff");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     printf("put it back\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_removeLink_byName (alfs_FSdev, dirInode->dinode, "in", 2, 0);
     assert (ret == 1);

     printf("removed in\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "in", 2, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "ggggg");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     printf("put it back\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_removeLink_byName (alfs_FSdev, dirInode->dinode, "cheek", 5, 0);
     assert (ret == 1);

     printf("removed cheek\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "cheek", 5, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "hhhhh");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     printf("put it back\n");
     listEmbDirectory (dirInode);

     dirent2 = alfs_embdir_lookupname (alfs_FSdev, dirInode->dinode, "squash", 6, &buffer);
     assert (dirent2);
     alfs_embdir_removeLink_byEntry (dirent2, buffer);
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     printf("removed squash\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash", 6, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "iiiii");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     printf("put it back\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_removeLink_byName (alfs_FSdev, dirInode->dinode, "in", 2, 0);
     assert (ret == 1);

     printf("removed in\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash2", 7, 250, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 250);
     strcpy (embdirentcontent(dirent), "jjjjj");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash3", 7, 384, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 384);
     strcpy (embdirentcontent(dirent), "kkkkk");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash4", 7, 384, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 384);
     strcpy (embdirentcontent(dirent), "lllll");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash5", 7, 384, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 384);
     strcpy (embdirentcontent(dirent), "mmmmm");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash6", 7, 384, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 384);
     strcpy (embdirentcontent(dirent), "nnnnn");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash7", 7, 384, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 384);
     strcpy (embdirentcontent(dirent), "ooooo");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "squash8", 7, 384, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 384);
     strcpy (embdirentcontent(dirent), "ppppp");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     printf ("filled up block\n");
     listEmbDirectory (dirInode);

     ret = alfs_embdir_addEntry (alfs_FSdev, dirInode->dinode, "eeenn", 5, 128, 0, &dirent, &buffer);
     assert (ret == 0);
     dirent->type = (char) 1;
     bzero (embdirentcontent(dirent), 128);
     strcpy (embdirentcontent(dirent), "qqqqq");
     alfs_buffer_releaseBlock (buffer, BUFFER_DIRTY);

     printf ("add eeenn\n");
     listEmbDirectory (dirInode);

   alfs_unmountFS ();

   printf ("Done\n");
   exit (0);
}
