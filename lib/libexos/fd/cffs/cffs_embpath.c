
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


/* rountines for manipulating/traversing pathnames */

#include "cffs_buffer.h"
#include "cffs.h"
#include "cffs_inode.h"
#include "cffs_embdir.h"
#include "cffs_path.h"
#include <assert.h>
#include <stdio.h>
#include <memory.h>
#include <sys/dirent.h>


int cffs_embedinodes = 1;	/* global control variable */

#define is_dot_or_dotdot(name,namelen)	\
	((namelen <= 2) && (name[0] == '.') && ((namelen == 1) || (name[1] == '.')))


/* Convert a pathname to an inode for the file. */

inode_t * cffs_path_translateNameToInode (buffer_t *sb_buf, const char *name, inode_t *dirInode, int flags) {
   int len = strlen (name);
   inode_t *inode;
   embdirent_t *dirent;
   buffer_t *buffer;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
/*
printf ("cffs_path_translateNameToInode: name %s, dirInodeNum %d (%x), flags %x\n", name, dirInode->inodeNum, dirInode->inodeNum, flags);
*/
   assert (dirInode && dirInode->dinode);
   assert (len > 0);

   /* fake '.' and '..' before checking actual directory contents */

   if (is_dot_or_dotdot(name,len)) {
      int inodeNum = (len == 1) ? dirInode->inodeNum : cffs_inode_getDirNum(dirInode);
      inode = cffs_inode_getInode (inodeNum, dirInode->fsdev, sb->blk, (flags | BUFFER_READ));
      return (inode);
   }

   /* look for the name in the directory and get the proper inode for it */

   dirent = cffs_embdir_lookupname(sb_buf, dirInode->dinode, name, len, &buffer);

   if (dirent == NULL) {		/* Name not found */
      return (NULL);
   }

   if (cffs_inode_blkno(dirent->inodeNum) == buffer->header.diskBlock) {
      dinode_t *dinode = (dinode_t *) (cffs_inode_offsetInBlock(dirent->inodeNum) + buffer->buffer);
      assert (dinode->dinodeNum != 0);
      inode = cffs_inode_makeInode (dirInode->fsdev, sb->blk, dinode->dinodeNum, dinode, buffer);
   } else {
      inode = cffs_inode_getInode (dirent->inodeNum, dirInode->fsdev, sb->blk, (flags | BUFFER_READ));
      assert(inode);
      assert (inode->inodeNum != 0);
      cffs_buffer_releaseBlock (buffer, 0);
   }

/*
printf ("OK exit from translateNameToInode: inodeNum %d (%x)\n", inode->inodeNum, inode->inodeNum);
*/
   return (inode);
}


int cffs_path_makeNewFile (buffer_t *sb_buf, inode_t *dirInode, const char *name, int nameLen, inode_t **inodePtr, int flags, int getmatch, u_int type, int *error)
{
   int ret;
   embdirent_t *dirent;
   inode_t *inode = *inodePtr;
   buffer_t *buffer;
   u_int inodeNum;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
/*
printf("cffs_path_makeNewFile: %s (len %d)\n", name, nameLen);
*/

   /* don't allow redundant creation of '.' and '..' */

   if (is_dot_or_dotdot(name,nameLen)) {
      ret = -EEXIST;
      inodeNum = (nameLen == 1) ? dirInode->inodeNum : cffs_inode_getDirNum(dirInode);

#if 0
   } else if (cffs_embedinodes) {
#else
   } else if (1) {
#endif
	/* get INODEALSO */
      ret = cffs_embdir_addEntry (sb_buf, dirInode->dinode, name, nameLen, 1, flags, &dirent, &buffer, error);
      if (error && *error) return 0;
      if (ret == 0) {
	 dinode_t *dinode;
	 inodeNum = cffs_embdir_findfreeInode ((char *)truncto((u_int)dirent, CFFS_EMBDIR_SECTOR_SIZE), &dinode);

	 assert (inodeNum != 0);
	 inodeNum += (cffs_embdir_sector (buffer,dirent)) << 2;

#ifndef CFFS_PROTECTED
	 cffs_dinode_initDInode (dinode, inodeNum, type);
		/* GROK -- dirNum becomes an invarient for directories !! */
	 cffs_dinode_setDirNum (dinode, dirInode->inodeNum);
	 if (cffs_dinode_isDir(dinode)) {
	    cffs_inode_setLinkCount (dirInode, (cffs_inode_getLinkCount(dirInode)+1));
	 }
	 dirent->type = (char) (type >> 8);
	 assert ((dirent->type << 8) == type);
	 dirent->inodeNum = inodeNum;
#else
#ifdef CFFSD
	 cffsd_fsupdate_directory (CFFS_DIRECTORY_SETINODENUM, dirent, inodeNum, 0, 0, 0);
	 cffsd_fsupdate_directory34 (CFFS_DIRECTORY_SETTYPE, dirent, (type >> 8), (u_int)dinode, (u_int)dirInode->dinode, dirInode->fsdev);
#else
	 sys_fsupdate_directory (CFFS_DIRECTORY_SETINODENUM, dirent, inodeNum, 0, 0, 0);
	 sys_fsupdate_directory (CFFS_DIRECTORY_SETTYPE, dirent, (type >> 8), (u_int)dinode, (u_int)dirInode->dinode, dirInode->fsdev);
#endif
	 assert (dinode->dinodeNum == inodeNum);
	 assert (dinode->dinodeNum != 0);
	 assert (cffs_dinode_getDirNum(dinode) == dirInode->inodeNum);
	 assert (cffs_dinode_getType(dinode) == type);
#endif
	 assert ((dirent->type << 8) == type);
	 cffs_buffer_dirtyBlock (buffer);
	 inode = cffs_inode_makeInode (dirInode->fsdev, sb->blk, dinode->dinodeNum, dinode, buffer);
      }
      inodeNum = dirent->inodeNum;

	/* Non-embedded inode */
   } else {
      assert (0);
#if 0
	/* not INODEALSO */
      ret = cffs_embdir_addEntry (dirInode->fsdev, dirInode->dinode, name, nameLen, 0, flags, &dirent, &buffer);
      if (ret == 0) {
		/* No support for non embedded inodes!! */
	 buffer_t *buffer;
	 dinode_t *dinode = cffs_dinode_makeDInode (dirInode->fsdev, 0, &buffer);
	 inode = cffs_inode_makeInode (dirInode->fsdev, dinode->dinodeNum, dinode, buffer);
      }
#endif
   }

/*
printf("checkpoint: ret %d, getmatch %d, inum %x\n", ret, getmatch, ((inode) ? inode->inodeNum : -1));
*/
   if ((ret != 0) && (getmatch)) {
      assert (inode == NULL);
      inode = cffs_inode_getInode (inodeNum, dirInode->fsdev, sb->blk, (BUFFER_READ | BUFFER_WITM));
      assert ((inode) && (inode->dinode));
   }

   *inodePtr = inode;

   return (ret);
}


int cffs_path_addLink (buffer_t *sb_buf, inode_t *dirInode, const char *name, int nameLen, inode_t *inode, int flags, int *error)
{
   int ret;
   embdirent_t *dirent;
   buffer_t *buffer;
/*
printf("cffs_path_prepareEntry: %d\n", (inode->dinode == NULL));
*/
   assert (!cffs_inode_isDir(inode));

   /* don't allow redundant creation of '.' and '..' */

   if (is_dot_or_dotdot(name,nameLen)) {
      return (-1);
   }

	/* not INODEALSO */
   ret = cffs_embdir_addEntry (sb_buf, dirInode->dinode, name, nameLen, 0, flags, &dirent, &buffer, error);
   if (error && *error) return 0;
   if (ret == 0) {
#ifndef CFFS_PROTECTED
      cffs_inode_setLinkCount(inode, (cffs_inode_getLinkCount(inode)+1));
      dirent->type = (char) (cffs_inode_getType(inode) >> 8);
      dirent->inodeNum = inode->inodeNum;
#else
#ifdef CFFSD
      cffsd_fsupdate_directory (CFFS_DIRECTORY_SETINODENUM, dirent, inode->inodeNum, 0, 0, 0);
      cffsd_fsupdate_directory34 (CFFS_DIRECTORY_SETTYPE, dirent, ((cffs_inode_getType(inode)) >> 8), (u_int)inode->dinode, (u_int)dirInode->dinode, dirInode->fsdev);
#else
      sys_fsupdate_directory (CFFS_DIRECTORY_SETINODENUM, dirent, inode->inodeNum, 0, 0, 0);
      sys_fsupdate_directory (CFFS_DIRECTORY_SETTYPE, dirent, ((cffs_inode_getType(inode)) >> 8), (u_int)inode->dinode, (u_int)dirInode->dinode, dirInode->fsdev);
#endif /* CFFSD */
      assert (cffs_inode_getLinkCount(inode) > 1);
#endif

      assert ((dirent->type << 8) == cffs_inode_getType(inode));

	/* if this makes the containing directory ambiguous */
      if (cffs_inode_getDirNum(inode) != dirInode->inodeNum) {
	 cffs_inode_setDirNum (inode, 0);
      }

      if ((cffs_softupdates == 0) && (cffs_inode_sector(inode->inodeNum) != cffs_embdir_sector(buffer,dirent))) {
	 cffs_inode_writeInode (inode, BUFFER_WRITE);
      }

      cffs_buffer_releaseBlock (buffer, (flags | BUFFER_DIRTY));
   }
/*
printf("checkpoint: ret %d, inum %x\n", ret, ((inode) ? inode->inodeNum : -1));
*/
   return (ret);
}


inode_t *cffs_path_grabEntry (buffer_t *sb_buf, inode_t *dirInode, const char *name, int nameLen, void **direntPtr, buffer_t **bufferPtr)
{
    embdirent_t *dirent;
    inode_t *inode;
    cffs_t *sb = (cffs_t *)sb_buf->buffer;

   /* NOTE: '.' and '..' will simply not be found via this call */

    dirent = cffs_embdir_lookupname (sb_buf, dirInode->dinode, name, nameLen, bufferPtr);
    if (dirent == NULL) {
	return (NULL);
    }
    *direntPtr = dirent;

    if (cffs_inode_blkno(dirent->inodeNum) == (*bufferPtr)->header.diskBlock) {
	dinode_t *dinode = (dinode_t *) ((*bufferPtr)->buffer + cffs_inode_offsetInBlock(dirent->inodeNum));
	cffs_buffer_bumpRefCount (*bufferPtr);
	inode = cffs_inode_makeInode (dirInode->fsdev, sb->blk, dinode->dinodeNum, dinode, *bufferPtr);
    } else {
	inode = cffs_inode_getInode (dirent->inodeNum, dirInode->fsdev, sb->blk, (BUFFER_READ | BUFFER_WITM));
	assert ((inode) && (inode->dinode));
    }

    return (inode);
}


int cffs_path_renameEntry (buffer_t *sb_buf1, inode_t *dirInode1, const char *name1, buffer_t *sb_buf2, inode_t *dirInode2, const char *name2, int *error)
{
   embdirent_t *dirent1;
   buffer_t *dirBlockBuffer1;
   inode_t *inode1;
   int nameLen1 = strlen (name1);
   embdirent_t *dirent2;
   buffer_t *dirBlockBuffer2;
   inode_t *inode2;
   int nameLen2 = strlen (name2);

   inode1 = cffs_path_grabEntry (sb_buf1, dirInode1, name1, nameLen1, (void **) &dirent1, &dirBlockBuffer1);
   if (inode1 == NULL) {
      return (-ENOENT);
   }

   inode2 = cffs_path_grabEntry (sb_buf2, dirInode2, name2, nameLen2, (void **) &dirent2, &dirBlockBuffer2);

   if ((inode2 != NULL) && ((cffs_inode_isDir(inode1)) || (cffs_inode_isDir(inode2)))) {
      if ((!cffs_inode_isDir(inode1)) || (!cffs_inode_isDir(inode2))) {
	 cffs_inode_releaseInode (inode1, 0);
	 cffs_inode_releaseInode (inode2, 0);
	 cffs_buffer_releaseBlock (dirBlockBuffer1, 0);
	 cffs_buffer_releaseBlock (dirBlockBuffer2, 0);
	 return ((cffs_inode_isDir(inode1)) ? -ENOTDIR : -EISDIR);
      }
      if (cffs_path_isEmpty (sb_buf2, inode2) == 0) {
	 cffs_inode_releaseInode (inode1, 0);
	 cffs_inode_releaseInode (inode2, 0);
	 cffs_buffer_releaseBlock (dirBlockBuffer1, 0);
	 cffs_buffer_releaseBlock (dirBlockBuffer2, 0);
	 return (-ENOTEMPTY);
      }
   }

	/* if same directory, source does not yet exist and name is easy, */
	/* then just change the name */
   if ((dirInode1 == dirInode2) && (inode2 == NULL) && ((nameLen2 <= nameLen1) || (nameLen2 <= 24))) {
	/* simply overwrite name for directory entry */
#ifndef CFFS_PROTECTED
      bcopy (name2, dirent1->name, nameLen2);
      dirent1->name[nameLen2] = (unsigned char) 0;
      dirent1->nameLen = nameLen2;
#else
#ifdef CFFSD
     int ret = cffsd_fsupdate_directory4 (CFFS_DIRECTORY_SETNAME, dirent1, (u_int)name2, nameLen2, (uint)dirInode1->dinode, dirInode1->fsdev);
#else
     int ret = sys_fsupdate_directory (CFFS_DIRECTORY_SETNAME, dirent1, (u_int)name2, nameLen2, (uint)dirInode1->dinode, dirInode1->fsdev);
#endif /* CFFSD */
     assert (ret == 0);
#endif
      cffs_inode_releaseInode (inode1, 0);
      cffs_buffer_releaseBlock (dirBlockBuffer1, BUFFER_DIRTY);

	/* A second simple case occurs when all in same sector... */


   } else {	/* handle all other cases the simple (expensive) way */

	/* increment target link count for the temporary extra pointer... */
#ifndef CFFS_PROTECTED
      cffs_inode_setLinkCount (inode1, (cffs_inode_getLinkCount(inode1)+1));
      if (cffs_softupdates == 0) {
	 cffs_inode_writeInode (inode1, BUFFER_WRITE);
      }
#endif

	/* if target exists, then repoint entry and decrement link count */
      if (inode2) {

		/* repoint target at source */
#ifndef CFFS_PROTECTED
	 dirent2->inodeNum = inode1->inodeNum;
	 dirent2->type = dirent1->type;	/* needed? */
#else
	 if (cffs_inode_isDir(inode1)) {
#ifdef CFFSD
	    int ret = cffsd_fsupdate_renamedir (0, dirent2, dirent1, inode1->dinode, dirInode2->dinode, dirInode2->dinode, inode2->dinode);
#else
	    int ret = sys_fsupdate_renamedir (0, dirent2, dirent1, inode1->dinode, dirInode2->dinode, dirInode2->dinode, inode2->dinode);
#endif
	    assert (ret == 0);
	 } else {
#ifdef CFFSD 
	    int ret = cffsd_fsupdate_directory34 (CFFS_DIRECTORY_SETINODENUM, dirent2, ((cffs_inode_getType(inode1)) >> 8), (u_int)inode2->dinode, (u_int)inode1->dinode, inode1->fsdev);
#else
	    int ret = sys_fsupdate_directory (CFFS_DIRECTORY_SETINODENUM, dirent2, inode1->inodeNum, (u_int)inode2->dinode, (u_int)inode1->dinode, inode1->fsdev);
#endif /* CFFSD */
	    assert (ret == 0);
	    assert (cffs_inode_getLinkCount(inode1) > 1);
	 }
#endif
	 cffs_buffer_releaseBlock (dirBlockBuffer2, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));

		/* deal with unlinked inode2 */
#ifndef CFFS_PROTECTED
	 assert (cffs_inode_getLinkCount (inode2) >= 1);
	 cffs_inode_setLinkCount (inode2, (cffs_inode_getLinkCount(inode2)-1));
#endif
	 cffs_inode_releaseInode (inode2, BUFFER_DIRTY);

	/* if target does not exist, then add entry and fill it */
      } else {
		/* not INODEALSO */
	 int ret = cffs_embdir_addEntry (sb_buf2, dirInode2->dinode, name2, nameLen2, 0, 0, &dirent2, &dirBlockBuffer2, error);
	 if (error && *error) return 0;
	 assert (ret == 0);
#ifndef CFFS_PROTECTED
	 dirent2->inodeNum = inode1->inodeNum;
	 dirent2->type = (char) (cffs_inode_getType(inode1) >> 8);
	 cffs_buffer_releaseBlock (dirBlockBuffer2, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));
	 if ((cffs_inode_isDir(inode1)) && (dirInode1 != dirInode2)) {
	    cffs_inode_setLinkCount (dirInode2, (cffs_inode_getLinkCount(dirInode2)+1));
	    if (cffs_softupdates == 0) {
	       cffs_inode_writeInode (dirInode2, BUFFER_WRITE);
	    }
	 }
#else
	 if (cffs_inode_isDir(inode1)) {
#ifdef CFFSD
	    int ret = cffsd_fsupdate_renamedir (0, dirent2, dirent1, inode1->dinode, dirInode2->dinode, dirInode2->dinode, NULL);
#else
	    int ret = sys_fsupdate_renamedir (0, dirent2, dirent1, inode1->dinode, dirInode2->dinode, dirInode2->dinode, NULL);
#endif /* CFFSD */
	    assert (ret == 0);
	 } else {
#ifdef CFFSD
	    ret = cffsd_fsupdate_directory (CFFS_DIRECTORY_SETINODENUM, dirent2, inode1->inodeNum, 0, 0, 0);
#else
	    ret = sys_fsupdate_directory (CFFS_DIRECTORY_SETINODENUM, dirent2, inode1->inodeNum, 0, 0, 0);
#endif /* CFFSD */
	    assert (ret == 0);
#ifdef CFFSD
	    ret = cffsd_fsupdate_directory34 (CFFS_DIRECTORY_SETTYPE, dirent2, ((cffs_inode_getType(inode1)) >> 8), (u_int)inode1->dinode, (u_int)dirInode2->dinode, dirInode1->fsdev);
#else
	    ret = sys_fsupdate_directory (CFFS_DIRECTORY_SETTYPE, dirent2, ((cffs_inode_getType(inode1)) >> 8), (u_int)inode1->dinode, (u_int)dirInode2->dinode, dirInode1->fsdev);
#endif /* CFFSD */
	    assert (ret == 0);
	    assert (cffs_inode_getLinkCount(inode1) > 1);
	 }
	 cffs_buffer_releaseBlock (dirBlockBuffer2, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));
#endif
      }

      assert ((dirent2->type << 8) == cffs_inode_getType(inode1));

	/* deal with source */
#ifndef CFFS_PROTECTED
      dirent1->type = (char) 0;
      cffs_embdir_removeLink_byEntry (dirent1, dirBlockBuffer1);
#else
      if (!cffs_inode_isDir(inode1)) {
	 int ret;
#ifdef CFFSD 
	 ret = cffsd_fsupdate_directory34 (CFFS_DIRECTORY_SETTYPE, dirent1, 0, (u_int)inode1->dinode, (u_int)dirInode1->dinode, dirInode1->fsdev);
#else
	 ret = sys_fsupdate_directory (CFFS_DIRECTORY_SETTYPE, dirent1, 0, (u_int)inode1->dinode, (u_int)dirInode1->dinode, dirInode1->fsdev);
#endif /* CFFSD */
	 assert (ret == 0);
      }
#endif
      cffs_buffer_releaseBlock (dirBlockBuffer1, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));
#ifndef CFFS_PROTECTED
      cffs_inode_setLinkCount (inode1, (cffs_inode_getLinkCount(inode1)-1));
#endif

	/* if renaming directory between directories, fix up dotdot entry */
#ifndef CFFS_PROTECTED
      if ((cffs_inode_isDir(inode1)) && (dirInode1 != dirInode2)) {
	 cffs_inode_setDirNum (inode1, dirInode2->inodeNum);
	 cffs_inode_setLinkCount (dirInode1, (cffs_inode_getLinkCount(dirInode1)-1));
      }
#endif

	/* change dirNum if rename moves the inode to another directory */
      if ((dirInode1 != dirInode2) && (!cffs_inode_isDir(inode1)) && (cffs_inode_getDirNum(inode1) == dirInode1->inodeNum)) {
		/* if unambiguous "owner", say so */
	 if (cffs_inode_getLinkCount(inode1) == 1) {
	    cffs_inode_setDirNum (inode1, dirInode2->inodeNum);
	 } else {
		/* otherwise, punt */
	    cffs_inode_setDirNum (inode1, 0);
	 }
      }

      cffs_inode_releaseInode (inode1, BUFFER_DIRTY);
   }

   return (0);
}


void cffs_path_removeEntry (inode_t *dirInode, const char *name, int nameLen, void *direntIn, inode_t *inode, buffer_t *buffer, int flags)
{
   embdirent_t *dirent = (embdirent_t *) direntIn;

   assert (inode->dinode);

#ifndef CFFS_PROTECTED
   dirent->type = (char) 0;
   if (cffs_inode_isDir(inode)) {
      assert (cffs_inode_getDirNum(inode) == dirInode->inodeNum);
      cffs_inode_setLinkCount (dirInode, (cffs_inode_getLinkCount(dirInode)-1));
   }
   assert (cffs_inode_getLinkCount(inode) > 0);
   cffs_inode_setLinkCount (inode, (cffs_inode_getLinkCount(inode) - 1));
#else
   {int ret;
#ifdef CFFSD
   ret = cffsd_fsupdate_directory34 (CFFS_DIRECTORY_SETTYPE, dirent, 0, (u_int)inode->dinode, (u_int)dirInode->dinode, dirInode->fsdev);
#else
   ret = sys_fsupdate_directory (CFFS_DIRECTORY_SETTYPE, dirent, 0, (u_int)inode->dinode, (u_int)dirInode->dinode, dirInode->fsdev);
#endif /* CFFSD */
   assert (ret == 0);
   }
#endif

   cffs_embdir_removeLink_byEntry (dirent, buffer);

   if (cffs_inode_sector(inode->inodeNum) != cffs_embdir_sector(buffer,dirent)) {
      cffs_buffer_releaseBlock (buffer, (flags | BUFFER_DIRTY));
   } else {
      cffs_buffer_releaseBlock (buffer, BUFFER_DIRTY);
   }

   if ((cffs_inode_getLinkCount(inode) == 0) && (cffs_inode_getOpenCount(inode) == 0)) {
      assert (cffs_inode_getRefCount(inode) == 1);
   }
	/* the release will properly deal with deleting the inode (and UNIX opens) */
   cffs_inode_releaseInode (inode, BUFFER_DIRTY);
}


int cffs_path_isEmpty (buffer_t *sb_buf, inode_t *dirInode)
{
   return (cffs_embdir_isEmpty(sb_buf, dirInode->dinode));
}


int cffs_path_printdirinfo (buffer_t *sb_buf, inode_t *dirInode)
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   embdirent_t *dirent;
   int dirBlockNumber = 0;
   int currPos = 0;
   int i, j;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   printf ("PrintDirInfo: inodeNum %x, length %qd\n", dirInode->inodeNum, cffs_inode_getLength (dirInode));

   printf("%x\t%d\t%x\t%d\t%s\n", dirInode->inodeNum, 0, 0xff, 1, ".");
   printf("%x\t%d\t%x\t%d\t%s\n", cffs_inode_getDirNum(dirInode), 0, 0xff, 2, "..");

   while (currPos < cffs_inode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode->dinode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = min (cffs_inode_getLength(dirInode), BLOCK_SIZE);
      dirBlockBuffer = cffs_buffer_getBlock (dirInode->fsdev, sb->blk, -dirInode->inodeNum, physicalDirBlockNumber, BUFFER_READ, NULL);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
/*
printf("length %d, buffer %p, block %p, diskBlock %d\n", dirBlockLength, dirBlockBuffer, dirBlock, dirBlockBuffer->header.diskBlock);
*/
      for (i=0; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
	 for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
	    dirent = (embdirent_t *) (dirBlock + i + j);
	    if (dirent->type != (char) 0) {
	       printf("%x\t%d\t%x\t%d\t%s\n", dirent->inodeNum, dirent->entryLen, dirent->type, dirent->nameLen, dirent->name);
	       if (cffs_inode_blkno(dirent->inodeNum) == dirBlockBuffer->header.diskBlock) {
		  dinode_t *dinode = (dinode_t *) (dirBlockBuffer->buffer + cffs_inode_offsetInBlock(dirent->inodeNum));
		  printf ("   offset %d\tlen %qd\tptr[0] %d\tptr[1] %d\n", (currPos + i + j), dinode->length, dinode->directBlocks[0], dinode->directBlocks[1]);
	       }
	    }
	 }
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
   return (0);
}


int cffs_path_getdirentries (buffer_t *sb_buf, inode_t *dirInode, char *buf, int nbytes, off_t *fileposP)
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   embdirent_t *dirent;
   int currPos = max (0, ((*fileposP)-2));
   int dirBlockNumber = currPos / BLOCK_SIZE;
   int j = currPos % CFFS_EMBDIR_SECTOR_SPACEFORNAMES;
   int i = (currPos % BLOCK_SIZE) - j;
   int direntlen;
   int bufused = 0;
   struct dirent *direntp;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   while ((*fileposP) < 2) {
      direntlen = ((2 + 1 + 3) & (~3)) + (sizeof(struct dirent) - (MAXNAMLEN+1));
      if ((direntlen + bufused) >= nbytes) {
	 return (bufused);
      }
      direntp = (struct dirent *) &buf[bufused];
      direntp->d_fileno = ((*fileposP) == 0) ? dirInode->inodeNum : cffs_inode_getDirNum(dirInode);
      direntp->d_namlen = ((*fileposP) == 0) ? 1 : 2;
      direntp->d_type = 0;
      direntp->d_name[0] = '.';
      direntp->d_name[1] = '.';
      direntp->d_name[((*fileposP)+1)] = (char) 0;
      direntp->d_reclen = direntlen;
      bufused += direntlen;
      (*fileposP)++;
   }

   currPos -= i + j;

   while (currPos < cffs_inode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode->dinode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = ((cffs_inode_getLength(dirInode) - currPos) < BLOCK_SIZE) ? (cffs_inode_getLength(dirInode) - currPos) : BLOCK_SIZE;
      dirBlockBuffer = cffs_buffer_getBlock (dirInode->fsdev, sb->blk, -dirInode->inodeNum, physicalDirBlockNumber, BUFFER_READ, NULL);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
	 for (; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
	    *fileposP = currPos + i + j + 2;
	    dirent = (embdirent_t *) (dirBlock + i + j);
	    if (dirent->type != (char) 0) {
	      int len;
/*
	       printf("%d\t%d\t%x\t%d\t%s\n", dirent->inodeNum, dirent->entryLen, dirent->type, dirent->nameLen, dirent->name);
*/
	       direntlen = ((dirent->nameLen + 1 + 3) & (~3)) + (sizeof(struct dirent) - (MAXNAMLEN+1));
	       if ((direntlen + bufused) > nbytes) {
/*
printf ("returning early from getdirentries: direntlen %d, bufused %d, nbytes %d\n", direntlen, bufused, nbytes);
printf ("dirBlockNumber %d, i %d, j %d, currPos %d, filePos %qd\n", (dirBlockNumber-1), i, j, currPos, *fileposP);
*/
		  cffs_buffer_releaseBlock (dirBlockBuffer, 0);
		  return (bufused);
	       }
	       direntp = (struct dirent *) &buf[bufused];
	       direntp->d_fileno = dirent->inodeNum;
	       if (dirent->nameLen > 254)
		 len = 254;
	       else
		 len = dirent->nameLen;
	       direntp->d_namlen = len;
	       direntp->d_type = 0;
	       bcopy (dirent->name, direntp->d_name, len);
	       direntp->d_name[len] = (char )0;
	       direntp->d_reclen = direntlen;
	       bufused += direntlen;
	    }
	 }
	 j = 0;
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
      i = 0;
   }
   *fileposP = currPos + 2;
   return (bufused);
}

