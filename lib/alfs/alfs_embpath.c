
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
#include "alfs/alfs_embdir.h"
#include <assert.h>
#include <stdio.h>
#include <memory.h>


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
     dinode_t *dinode = startInode->dinode;
     inode_t *inode = startInode;
     embdirent_t *dirent;
     buffer_t *buffer;
     buffer_t *prevbuffer = NULL;
/*
printf ("alfs_path_translateNameToInode: path %s, startInodeNum %d (%x), flags %x\n", path, startInode->inodeNum, startInode->inodeNum, flags);
*/
     /* exploit understanding of "in-core" inode behavior */

     startInode->refCount++;

     /* get the name of the first component to lookup */
     if (path[0] != (char) NULL) {
        path = alfs_path_getNextPathComponent (next, &len);
        next = path + len;
     }

     /* loop through all directory entries looking for the next
        element of our path. */

     while (*path) {

          if (len > ALFS_EMBDIR_MAX_FILENAME_LENGTH) {
              return (ALFS_ELENGTH);
          }
  
          /* get read cap for dir */
          if (!verifyReadPermission(alfs_dinode_getMask(dinode), uid, alfs_dinode_getUid(dinode), gid, alfs_dinode_getGid(dinode))) {
               if (inode) {
                   alfs_inode_releaseInode (inode, 0);
               } else {
                   alfs_buffer_releaseBlock (prevbuffer, 0);
               }
               return (ALFS_EACCESS);	/* no read permission */
          }

          /* look for the name in the directory and get the proper
             inode for it (returned in root). */
          dirent = alfs_embdir_lookupname(startInode->fsdev, dinode, path, len, &buffer);

          if (inode) {
              alfs_inode_releaseInode (inode, 0);
          } else {
              alfs_buffer_releaseBlock (prevbuffer, 0);
          }

          if (dirent == NULL) {		/* Name not found */
              return (ALFS_ENOENT);
          }

          if (dirent->type == (char) 1) {
              /* GROK -- check for existing "in-core" inode !!! */
              dinode = (dinode_t *) embdirentcontent (dirent);
              prevbuffer = buffer;
              inode = NULL;
          } else if ((dirent->type == (char) 3) || (dirent->type == (char) 2)) {
              /* GROK -- need to include flags on last inode acquire ? */
              inode = alfs_inode_getInode (*((int *)embdirentcontent(dirent)), startInode->fsdev, BUFFER_READ);
              assert(inode);
              dinode = inode->dinode;
              alfs_buffer_releaseBlock (buffer, 0);
          } else {
              assert (0);
          }

          /* get the name of the next component to lookup */
          path = alfs_path_getNextPathComponent (next, &len);
          next = path + len;
     }

     if (inode == NULL) {
         inode = alfs_inode_makeInode (startInode->fsdev);
         inode->dinode = dinode;
         inode->inodeNum = dinode->dinodeNum;
     }
     *inodePtr = inode;
/*
printf ("OK exit from translateNameToInode: inodeNum %d (%x)\n", inode->inodeNum, inode->inodeNum);
*/
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
     inode_t *dirInode = startInode;
     dinode_t *dirDInode = dirInode->dinode;
     embdirent_t *dirent;
     buffer_t *buffer;
     buffer_t *prevbuffer = NULL;

     /* exploit understanding of "in-core" inode management */

     startInode->refCount++;

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
/*
printf ("traversing: path %s, namelen %d, inum %x\n", path, *namelenPtr, dirDInode->dinodeNum);
*/

          if (*namelenPtr > ALFS_EMBDIR_MAX_FILENAME_LENGTH) {
              return (ALFS_ELENGTH);
          }
  
          /* get read cap for dir */
          if (!verifyReadPermission(alfs_dinode_getMask(dirDInode), uid, alfs_dinode_getUid(dirDInode), gid, alfs_dinode_getGid(dirDInode))) {
               if (dirInode) {
                   alfs_inode_releaseInode (dirInode, 0);
               } else {
                   alfs_buffer_releaseBlock (prevbuffer, 0);
               }
               return (ALFS_EACCESS);	/* no read permission */
          }

          /* look for the name in the directory and get the proper
             inode for it (returned in root). */

          dirent = alfs_embdir_lookupname(startInode->fsdev, dirDInode, path, *namelenPtr, &buffer);

          if (dirInode) {
              alfs_inode_releaseInode (dirInode, 0);
          } else {
              alfs_buffer_releaseBlock (prevbuffer, 0);
          }

          if (dirent == NULL) {		/* Name not found */
printf ("traverse: failed to find path component: path %s, namelen %d, inum %x, startinum %x\n", path, *namelenPtr, dirDInode->dinodeNum, startInode->inodeNum);
               return (ALFS_ENOENT);
          }

          if (dirent->type == (char) 1) {
              /* GROK -- check for existing "in-core" inode !!! */
              dirDInode = (dinode_t *) embdirentcontent (dirent);
              prevbuffer = buffer;
              dirInode = NULL;
          } else if ((dirent->type == (char) 2) || (dirent->type == (char) 3)) {
              /* GROK -- need to include flags on last inode acquire ? */
              dirInode = alfs_inode_getInode (*((int*)embdirentcontent(dirent)), startInode->fsdev, BUFFER_READ);
              assert(dirInode);
              dirDInode = dirInode->dinode;
              alfs_buffer_releaseBlock (buffer, 0);
          } else {
              assert ("Unknown directory entry type" == 0);
          }

          /* get the name of the next component to lookup */
          path = next;
          *namelenPtr = len;
          next = alfs_path_getNextPathComponent ((path + len), &len);
     }

     if (*namelenPtr > ALFS_EMBDIR_MAX_FILENAME_LENGTH) {
         return (ALFS_ELENGTH);
     }

     if (dirInode == NULL) {
         /* GROK -- assumes the non-copy type of inode ... */
         dirInode = alfs_inode_makeInode (startInode->fsdev);
         dirInode->dinode = dirDInode;
         dirInode->inodeNum = dirDInode->dinodeNum;
     }

     *namePtr = path;
     *dirInodePtr = dirInode;

     return (OK);
}


/*
int alfs_path_addLink (inode_t *dirInode, char *name, int nameLen, inode_t **inodePtr, char status, int flags)
{
   assert ((inodePtr) && (*inodePtr));
   return (alfs_embdir_addLink (dirInode->fsdev, dirInode->dinode, name, nameLen, (*inodePtr)->inodeNum, status, flags));
}
*/


int alfs_embdir_getUniqueID (embdirent_t *dirent, buffer_t *buffer, unsigned int dinodeNum)
{
   unsigned char avail = (unsigned char) 0xFF;
   embdirent_t *tmp = (embdirent_t *) ((char *) buffer->buffer + ((unsigned int) ((char *) dirent - buffer->buffer) & ~(ALFS_EMBDIR_SECTOR_SIZE - 1)));
   dinode_t *dinode;
   int runlength = 0;
   int i;
/*
printf ("alfs_embdir_getuniqueID: dirent %x, buffer %x, dinodeNum %x\n", (u_int) dirent, (u_int) buffer->buffer, dinodeNum);
*/
   while (runlength < ALFS_EMBDIR_SECTOR_SIZE) {
/*
printf ("tmp %x, type %x, entryLen %d, dinodenum %x\n", (u_int) tmp, tmp->type, tmp->entryLen, ((dinode_t *) embdirentcontent(tmp))->dinodeNum);
*/
      if ((tmp != dirent) && (tmp->type & 1)) {
         dinode = (dinode_t *) embdirentcontent (tmp);
         if ((dinode->dinodeNum & 0xFFFF0000) && (dinode->dinodeNum & 0x0000FFF8)) {
            avail &= ~(1 << (dinode->dinodeNum & 0x00000007));
         }
      }
      runlength += tmp->entryLen;
      tmp = (embdirent_t *) ((char *) tmp + tmp->entryLen);
   }
   if (avail == (char) 0) {
      return (-1);
   }
/*
printf ("hopefully found one: %x\n", avail);
*/
   for (i=0; i<8; i++) {
      if ((avail >> i) & 1) {
         return (i);
      }
   }
   assert (0);
   return (-1);
}


int alfs_path_prepareEntry (inode_t *dirInode, char *name, int nameLen, inode_t *inode, void **direntPtr, buffer_t **bufferPtr, int flags, int getmatch)
{
    int ret;
    embdirent_t *dirent;

/*
printf("alfs_path_prepareEntry: %d\n", (inode->dinode == NULL));
*/
    if (inode->dinode == NULL) {
        ret = alfs_embdir_addEntry (dirInode->fsdev, dirInode->dinode, name, nameLen, sizeof(dinode_t), flags, &dirent, bufferPtr);
        if (ret == 0) {
            unsigned int dinodeNum = (*bufferPtr)->header.block * BLOCK_SIZE;
            dinodeNum += (char *)dirent - (*bufferPtr)->buffer;
            dinodeNum /= ALFS_EMBDIR_SECTOR_SIZE;
            dinodeNum = (dinodeNum + 1) << 3;
            assert (dinodeNum < 65536);
            dinodeNum |= dirInode->inodeNum;  /* should be XXXX0000 */
            ret = alfs_embdir_getUniqueID (dirent, *bufferPtr, dinodeNum);
            assert (ret >= 0); 
            dinodeNum += ret;
            ret = 0;

            inode->dinode = (dinode_t *) embdirentcontent (dirent);
            bzero ((char *)inode->dinode, sizeof (dinode_t));
            inode->dinode->dinodeNum = dinodeNum;
            inode->dinode->refCount = 1;
            inode->inodeNum = dinodeNum;
            alfs_buffer_getBlock ((*bufferPtr)->header.dev, (*bufferPtr)->header.inodeNum, (*bufferPtr)->header.block, BUFFER_GET);
        }
    } else {
        ret = alfs_embdir_addEntry (dirInode->fsdev, dirInode->dinode, name, nameLen, sizeof(unsigned int), flags, &dirent, bufferPtr);
        if (ret == 0) {
            assert ("Need to handle link demotion !!!!" == 0);
        }
    }
/*
printf("checkpoint: ret %d, inum %x\n", ret, inode->inodeNum);
*/
    *direntPtr = (void *) dirent;

    if ((ret != 0) && (getmatch)) {
        inode->dinode = alfs_dinode_getDInode (ret, dirInode->fsdev, (BUFFER_READ | BUFFER_WITM));
        inode->inodeNum = ret;
    }

    return (ret);
}


void alfs_path_fillEntry (inode_t *dirInode, void *direntIn, inode_t *inode, char type, buffer_t *buffer, int seqinode, int flags)
{
    embdirent_t *dirent = (embdirent_t *) direntIn;
    char efftype = type & ~((char) 0x03);

    if (alfs_inode_getType (inode) == INODE_DIRECTORY) {
/*
printf ("resetting inode number state for directory: old inodeNum %x\n", inode->inodeNum);
*/
        inode->inodeNum = alfs_embdir_allocDirNum (inode->inodeNum);
        assert (inode->inodeNum != -1);
/*
printf ("resetting inode number state for directory: new inodeNum %x\n", inode->inodeNum);
*/
        inode->dinode->dinodeNum = inode->inodeNum;
    }
    if (dirent->contentLen == sizeof (dinode_t)) {
        dirent->type = efftype | (char) 1;
    } else {
        assert (dirent->contentLen == sizeof (unsigned int));

        if (seqinode) {
            alfs_inode_writeInode (inode, BUFFER_WRITE);
        }

        dirent->type = efftype | (char) 2;
        *((unsigned int *) embdirentcontent(dirent)) = inode->inodeNum;
    }

    alfs_buffer_releaseBlock (buffer, (flags | BUFFER_DIRTY));
}


int alfs_path_initDir (inode_t *dirInode, inode_t *parentInode, int flags)
{
   return (alfs_embdir_initDir (dirInode->fsdev, dirInode->dinode, (char) 3, ((parentInode) ? parentInode->inodeNum : 0), flags));
}


void alfs_path_initDirRaw (dinode_t *dirDInode, dinode_t *parentDInode, char *dirbuf)
{
   alfs_embdir_initDirRaw (dirDInode, (char) 3, ((parentDInode) ? parentDInode->dinodeNum : 0), dirbuf);
}

/*
int alfs_path_removeLink (inode_t *dirInode, char *name, int nameLen, int flags)
{
   return (alfs_embdir_removeLink_byName (dirInode->fsdev, dirInode->dinode, name, nameLen, flags));
}
*/

inode_t *alfs_path_grabEntry (inode_t *dirInode, char *name, int nameLen, void **direntPtr, buffer_t **bufferPtr)
{
    embdirent_t *dirent;
    inode_t *inode;

    dirent = alfs_embdir_lookupname (dirInode->fsdev, dirInode->dinode, name, nameLen, bufferPtr);
    if (dirent == NULL) {
        return (NULL);
    }
    *direntPtr = dirent;

    if (dirent->type == (char) 1) {
        inode = alfs_inode_makeInode (dirInode->fsdev);
        inode->dinode = (dinode_t *) embdirentcontent (dirent);
        inode->inodeNum = inode->dinode->dinodeNum;
    } else {
        assert ((dirent->type == (char) 2) || (dirent->type == (char) 3));
        inode = alfs_inode_getInode (*((unsigned int *) embdirentcontent(dirent)), dirInode->fsdev, (BUFFER_READ | BUFFER_WITM));
    }

    return (inode);
}


void alfs_path_removeEntry (inode_t *dirInode, char *name, int nameLen, void *direntIn, inode_t *inode, buffer_t *buffer, int flags)
{
   embdirent_t *dirent = (embdirent_t *) direntIn;

   if ((alfs_inode_getType(inode) == INODE_DIRECTORY) && (alfs_inode_getLinkCount(inode) == 1)) {
      alfs_embdir_deallocDirNum (inode->inodeNum >> 16);
   }

   if ((char *) inode->dinode == embdirentcontent(dirent)) {
      inode->dinode = NULL;
      inode->inodeNum = 0;
   }

   alfs_embdir_removeLink_byEntry (dirent, buffer);

   alfs_buffer_releaseBlock (buffer, (flags | BUFFER_DIRTY));
}


int alfs_path_isEmpty (inode_t *dirInode, int *dotdot)
{
   return (alfs_embdir_isEmpty(dirInode->fsdev, dirInode->dinode, dotdot));
}


void alfs_path_listDirectory (inode_t *dirInode)
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
/*
printf("length %d, buffer %x, block %x\n", dirBlockLength, dirBlockBuffer, dirBlock);
*/
      for (i=0; i<dirBlockLength; i += dirent->entryLen) {
         dirent = (embdirent_t *) (dirBlock + i);
         if (dirent->type != (char) 0) {
            printf("%d\t%d\t%x\t%d\t%s\n", *((unsigned int *) embdirentcontent(dirent)), dirent->entryLen, dirent->type, dirent->nameLen, dirent->name);
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
}


int alfs_path_initmetablks ()
{
   return (alfs_embdir_initmetablks());
}


unsigned int alfs_path_initDisk (cap2_t fscap, block_num_t fsdev, int blkno, int rootdirno, int rootinum)
{
   return (alfs_embdir_initDisk (fscap, fsdev, blkno, rootdirno, rootinum));
}


unsigned int alfs_path_mountDisk (cap2_t fscap, block_num_t fsdev, int blkno)
{
   return (alfs_embdir_mountDisk (fscap, fsdev, blkno));
}


unsigned int alfs_path_unmountDisk (cap2_t fscap, block_num_t fsdev, int blkno)
{
   return (alfs_embdir_unmountDisk (fscap, fsdev, blkno));
}

