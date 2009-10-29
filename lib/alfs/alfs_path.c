
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

/* rountines for manipulating/traversing pathnames */

#include "alfs/alfs_buffer.h"
#include "alfs/alfs.h"
#include "alfs/alfs_inode.h"
#include "alfs/alfs_protection.h"
#include "alfs/alfs_dir.h"
#include "alfs/alfs_path.h"
#include <assert.h>
#include <stdio.h>


/* alfs_path_getNextPathComponent

   identify the leftmost component of a pathname and return it

   path is the pathname
   compLen holds length of the pathname component

   returns ptr to first character of the component
 */

char *alfs_path_getNextPathComponent (char *path, int *compLen)
{
     char *start;
     int count = 0;

     /* strip leading slashes, if any */
     while ((*path == '/') && (*path != (char )NULL)) {
         path++;
     }
     start = path;

     /* copy over chars to next */
     while ((*path != '/') && (*path != (char )NULL)) {
          path++;
          count++;
     }
     *compLen = count;
     
     return (start);
}


/* alfs_path_translateNameToInode

   Convert a pathname to an inode for the file. 

   UPON ENTRY:
   path is path to lookup
   startPoint is inode of where to start following path

   UPON RETURN:
   return value contains error status
   startPoint contains inode number corresponding to the name
 */

int alfs_path_translateNameToInode (char *path, inode_t *startInode, inode_t **inodePtr, int flags) {
     char *next = path;
     int len;
     int inodeNumber = startInode->inodeNum;
     inode_t *inode;

     /* get the name of the first component to lookup */
     if (path[0] != (char) NULL) {
        path = alfs_path_getNextPathComponent (next, &len);
        next = path + len;
     }

     /* loop through all directory entries looking for the next
        element of our path. */

     while (*path) {
/*
printf("working on path: %s\n", path);
*/
          if (len > ALFS_DIR_MAX_FILENAME_LENGTH) {
              return (ALFS_ELENGTH);
          }

          inode = (inodeNumber == startInode->inodeNum) ? startInode :
                  alfs_inode_getInode (inodeNumber, startInode->fsdev, BUFFER_READ);
          assert(inode);
  
          /* get read cap for dir */
          if (!verifyReadPermission(alfs_inode_getMask(inode), uid, alfs_inode_getUid(inode), gid, alfs_inode_getGid(inode))) {
               if (inode != startInode) {
                   alfs_inode_releaseInode (inode, 0);
               }
               return (ALFS_EACCESS);	/* no read permission */
          }

          /* look for the name in the directory and get the proper
             inode for it (returned in root). */
          if ((inodeNumber = alfs_dir_lookupname(inode->fsdev, inode->dinode, path, len)) < 0) {
               if (inode != startInode) {
                   alfs_inode_releaseInode (inode, 0);
               }
               return (ALFS_ENOENT);
          }

          if (inode != startInode) {
              alfs_inode_releaseInode (inode, 0);
          }

          /* get the name of the next component to lookup */
          path = alfs_path_getNextPathComponent (next, &len);
          next = path + len;
     }

     *inodePtr = alfs_inode_getInode (inodeNumber, startInode->fsdev, (flags | BUFFER_READ));

     return (OK);
}


int alfs_path_lookupname (inode_t *dirInode, char *name, int nameLen, int *inodeNumPtr)
{
    if (nameLen > ALFS_DIR_MAX_FILENAME_LENGTH) {
        return (ALFS_ELENGTH);
    }

    if (alfs_inode_getType (dirInode) != INODE_DIRECTORY) {
        return (ALFS_ENOTDIR);
    }
  
    /* get read cap for dir */
    if (!verifyReadPermission(alfs_inode_getMask(dirInode), uid, alfs_inode_getUid(dirInode), gid, alfs_inode_getGid(dirInode))) {
        return (ALFS_EACCESS);	/* no read permission */
    }

    if ((*inodeNumPtr = alfs_dir_lookupname(dirInode->fsdev, dirInode->dinode, name, nameLen)) < 0) {
        return (ALFS_ENOENT);
    }

    return (OK);
}


/* alfs_path_traversePath

   Convert a pathname to an inode # for the directory (not the file itself). 

   UPON ENTRY:
   path is path to lookup
   startPoint is inode number of where to start following path
   dirInode, name, and namelen are for return only

   UPON RETURN:
   return value contains error status
   startPoint contains inode number corresponding to the name
   name contains the final pathname component (i.e., the file name)
   namelen contains the length of the final component
 */

int alfs_path_traversePath (char *path, inode_t *startInode, inode_t **dirInodePtr, char **namePtr, int *namelenPtr, int flags) {
     char *next = path;
     int len;
     int dirInodeNumber = startInode->inodeNum;
     inode_t *dirInode;

     if (*path == (char) NULL) {
        *namePtr = path;
        *namelenPtr = 0;
        *dirInodePtr = alfs_inode_getInode (startInode->inodeNum, startInode->fsdev, (flags | BUFFER_READ));
        return (OK);
     }

     /* get the name of the first component to lookup */
     path = alfs_path_getNextPathComponent (next, namelenPtr);
     if (*path == (char) NULL) {
        *namePtr = path;
        *dirInodePtr = alfs_inode_getInode (startInode->inodeNum, startInode->fsdev, (flags | BUFFER_READ));
        return (OK);
     }

     next = alfs_path_getNextPathComponent ((path + *namelenPtr), &len);

     /* loop through all the directory entries looking for the next
	element of our path. */

     while (*next) {

          if (*namelenPtr > ALFS_DIR_MAX_FILENAME_LENGTH) {
              return (ALFS_ELENGTH);
          }

          dirInode = (dirInodeNumber == startInode->inodeNum) ? startInode :
                     alfs_inode_getInode (dirInodeNumber, startInode->fsdev, BUFFER_READ);
          assert(dirInode);
  
          /* get read cap for dir */
          if (!verifyReadPermission(alfs_inode_getMask(dirInode), uid, alfs_inode_getUid(dirInode), gid, alfs_inode_getGid(dirInode))) {
               if (dirInode != startInode) {
                   alfs_inode_releaseInode (dirInode, 0);
               }
               return (ALFS_EACCESS);	/* no read permission */
          }

          /* look for the name in the directory and get the proper
             inode for it (returned in root). */
          if ((dirInodeNumber = alfs_dir_lookupname(dirInode->fsdev, dirInode->dinode, path, *namelenPtr)) < 0) {
               if (dirInode != startInode) {
                   alfs_inode_releaseInode (dirInode, 0);
               }
               return (ALFS_ENOENT);
          }

          if (dirInode != startInode) {
              alfs_inode_releaseInode (dirInode, 0);
          }

          /* get the name of the next component to lookup */
          path = next;
          *namelenPtr = len;
          next = alfs_path_getNextPathComponent ((path + len), &len);
     }

     if (*namelenPtr > ALFS_DIR_MAX_FILENAME_LENGTH) {
         return (ALFS_ELENGTH);
     }

     *namePtr = path;
     *dirInodePtr = alfs_inode_getInode (dirInodeNumber, startInode->fsdev, (flags | BUFFER_READ));

     return (OK);
}


/*
int alfs_path_addLink (inode_t *dirInode, char *name, int nameLen, inode_t **inodePtr, char status, int flags)
{
   assert ((inodePtr) && (*inodePtr));
   return (alfs_dir_addLink (dirInode->fsdev, dirInode->dinode, name, nameLen, (*inodePtr)->inodeNum, status, flags));
}
*/


int alfs_path_prepareEntry (inode_t *dirInode, char *name, int nameLen, inode_t *inode, void **direntPtr, buffer_t **bufferPtr, int flags, int getmatch)
{
    int ret;
    dirent_t *dirent;
    
    ret = alfs_dir_addEntry (dirInode->fsdev, dirInode->dinode, name, nameLen, &dirent, bufferPtr, flags);
    if (ret == 0) {
        if (inode->dinode == NULL) {
            inode->dinode = alfs_dinode_makeDInode (dirInode->fsdev, 0);
            inode->inodeNum = inode->dinode->dinodeNum;
        }
        *direntPtr = (void *) dirent;
    } else if (getmatch) {
        inode->dinode = alfs_dinode_getDInode (ret, dirInode->fsdev, (BUFFER_READ | BUFFER_WITM));
        inode->inodeNum = ret;
    }
    return (ret);
}


void alfs_path_fillEntry (inode_t *dirInode, void *direntIn, inode_t *inode, char status, buffer_t *buffer, int seqinode, int flags)
{
    dirent_t *dirent = (dirent_t *) direntIn;

    if (seqinode) {
        alfs_inode_writeInode (inode, BUFFER_WRITE);
    }

    dirent->inodeNum = inode->inodeNum;
    dirent->status = status;

    alfs_buffer_releaseBlock (buffer, (flags | BUFFER_DIRTY));
/*
    alfs_path_listDirectory (dirInode);
*/
}


int alfs_path_initDir (inode_t *dirInode, inode_t *parentInode, int flags)
{
   return (alfs_dir_initDir (dirInode->fsdev, dirInode->dinode, ((parentInode) ? ((inode_t *)parentInode)->inodeNum : 0), flags));
}


void alfs_path_initDirRaw (dinode_t *dirDInode, dinode_t *parentDInode, char *dirbuf)
{
   alfs_dir_initDirRaw (dirDInode, ((parentDInode) ? ((dinode_t *)parentDInode)->dinodeNum : 0), dirbuf);
}

/*
int alfs_path_removeLink (inode_t *dirInode, char *name, int nameLen, int flags)
{
   return (alfs_dir_removeLink (dirInode->fsdev, dirInode->dinode, name, nameLen, flags));
}
*/


inode_t * alfs_path_grabEntry (inode_t *dirInode, char *name, int nameLen, void **dirent, buffer_t **buffer)
{
   int inodeNum = alfs_dir_lookupname (dirInode->fsdev, dirInode->dinode, name, nameLen);
   if (inodeNum > 0) {
      return (alfs_inode_getInode (inodeNum, dirInode->fsdev, (BUFFER_READ | BUFFER_WITM)));
   } else {
      return (NULL);
   }
}


void alfs_path_removeEntry (inode_t *dirInode, char *name, int nameLen, void *dirent, inode_t *inode, buffer_t *buffer, int flags)
{
   int inodeNum = alfs_dir_removeLink (dirInode->fsdev, dirInode->dinode, name, nameLen, flags);
   assert (inodeNum == inode->inodeNum);
}


int alfs_path_isEmpty (inode_t *dirInode, int *dotdot)
{
   return (alfs_dir_isEmpty(dirInode->fsdev, dirInode->dinode, dotdot));
}


void alfs_path_listDirectory (inode_t *dirInode)
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   int dirBlockNumber = 0;
   dirent_t *dirent;
   int currPos = 0;
   int i;

   printf("printing contents of directory %08x %d\n", (unsigned int) dirInode,dirInode->inodeNum);

   while (currPos < alfs_inode_getLength(dirInode)) {
      dirBlockLength = ((alfs_inode_getLength(dirInode) - currPos) < BLOCK_SIZE) ? (alfs_inode_getLength(dirInode) - currPos) : BLOCK_SIZE;
      dirBlockBuffer = alfs_buffer_getBlock (dirInode->fsdev, dirInode->inodeNum, dirBlockNumber, BUFFER_READ);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (i=0; i<dirBlockLength; i += dirent->entryLen) {
         dirent = (dirent_t *) (dirBlock + i);
         if (dirent->inodeNum != 0) {
            printf("%d\t%d\t%x\t%d\t%s\n", dirent->inodeNum, dirent->entryLen, dirent->status, dirent->nameLen, dirent->name);
/*
	    printf("currPos: %d, i: %d\n",currPos,i);
*/
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
}


int alfs_path_initmetablks ()
{
   return (0);
}


unsigned int alfs_path_initDisk (cap2_t fscap, block_num_t fsdev, int blkno, int rootdirno, int rootinum)
{
   return (blkno);
}


unsigned int alfs_path_mountDisk (cap2_t fscap, block_num_t fsdev, int blkno)
{
   return (blkno);
}


unsigned int alfs_path_unmountDisk (cap2_t fscap, block_num_t fsdev, int blkno)
{
   return (blkno);
}

