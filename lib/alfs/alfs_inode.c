
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

/* ALFS -- Application-Level Filesystem */

#include "alfs/alfs_dinode.h"
#include "alfs/alfs_buffer.h"
#include "alfs/alfs.h"
#include "alfs/alfs_inode.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

static inode_t *allocInode ();
static void deallocInode (inode_t *inode);
static inode_t *checkforInode (block_num_t dinodeNum, block_num_t fsdev);
static void unlistInode (inode_t *inode);
static void listInode (inode_t *inode);

static inode_t *listedInodes = NULL;

static void listInode (inode_t *inode)
{
   assert ((inode->next == NULL) && (inode->prev == NULL));
   assert (checkforInode (inode->inodeNum, inode->fsdev) == NULL);
   inode->next = listedInodes;
   inode->prev = NULL;
   if (listedInodes) {
      listedInodes->prev = inode;
   }
   listedInodes = inode;
}

static void unlistInode (inode_t *inode)
{
   if (inode->next) {
      inode->next->prev = inode->prev;
   }
   if (inode->prev) {
      inode->prev->next = inode->next;
   } else {
      assert (listedInodes == inode);
      listedInodes = inode->next;
   }
}

static inode_t *checkforInode (block_num_t inodeNum, block_num_t fsdev)
{
   inode_t *inode = listedInodes;

   while ((inode) && ((inode->inodeNum != inodeNum) || (inode->fsdev != fsdev))) {
      inode = inode->next;
   }
   return (inode);
}

static inode_t *allocInode ()
{
   inode_t *inode = (inode_t *) malloc_and_alloc (sizeof(inode_t));
   assert (inode);
   bzero ((char *)inode, sizeof(inode_t));
   return (inode);
}

static void deallocInode (inode_t *inode)
{
   free (inode);
}

inode_t *alfs_inode_makeInode (block_num_t fsdev)
{
   inode_t *inode;

   inode = allocInode ();
   inode->dinode = NULL;
   inode->refCount = 1;
   inode->fsdev = fsdev;
   inode->inodeNum = 0;
   inode->fileptr = 0;
   inode->flags = 0;
   inode->next = inode->prev = NULL;
   listInode (inode);
   return (inode);
}


inode_t *alfs_inode_getInode (block_num_t inodeNum, block_num_t fsdev, int flags)
{
   inode_t *inode = checkforInode (inodeNum, fsdev);
   
   if (inode == NULL) {
      inode = allocInode();
      if ((inode->dinode = alfs_dinode_getDInode (inodeNum, fsdev, flags)) == NULL) {
	  printf("alfs_inode_getInode, Bad Inode Number, Stale Inode\n");
	  deallocInode(inode);
	  return (NULL);
      }
      inode->inodeNum = inodeNum;
      inode->fsdev = fsdev;
      inode->refCount = 0;
      inode->fileptr = 0;
      inode->flags = 0;
      listInode (inode);
   }
   inode->refCount++;
/*
printf ("got Inode: inodeNum %d (%x), refCount %d\n", inodeNum, inodeNum, inode->refCount);
*/
   return (inode);
}


int alfs_inode_releaseInode (inode_t *inode, int flags)
{
/*
   assert (inode->dinode);
*/
   assert (inode->refCount > 0);

/*
printf ("releaseInode: inodeNum %d (%x), refCount %d\n", inode->inodeNum, inode->inodeNum, inode->refCount);
*/
   inode->refCount--;
   if (inode->refCount == 0) {
      unlistInode (inode);
      if (inode->dinode) {
         alfs_dinode_releaseDInode (inode->dinode, inode->fsdev, flags);
         inode->dinode = NULL;
      }
      deallocInode (inode);
   }
   return (OK);
}


void alfs_inode_writeInode (inode_t *inode, int flags)
{
   alfs_dinode_writeDInode (inode->dinode, inode->fsdev);
/*
   printf("need to implement writeInode\n");
*/
}

void alfs_inode_truncInode (inode_t *inode, int length)
{
   assert (inode->dinode);

   alfs_dinode_truncDInode (inode->dinode, inode->fsdev, length);
}

block_num_t alfs_inode_offsetToBlock (inode_t *inode, unsigned offset, int *allocate)
{
   assert (inode->dinode);

   return (alfs_dinode_offsetToBlock (inode->dinode, inode->fsdev, offset, allocate));
}

