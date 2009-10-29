
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

/* functions for accessing and manipulating directories */

/* NOTE: this implementation assumes several things about the on-disk     */
/*       (as it sees it) format of directory entries:                     */
/*       (1) a blank character is included at the end of each name.       */
/*       (2) all entry sizes are rounded up to a 4-byte boundaries.       */
/*       (3) no directory entry crosses a "sector" boundary, where the    */
/*           "sector" size is defined by ALFS_DIR_SECTOR_SIZE             */

#include "alfs/alfs_buffer.h"
#include "alfs/alfs_dinode.h"
#include "alfs/alfs_dir.h"
#include <assert.h>
#include <memory.h>
#include <unistd.h>

static int alfs_dir_getblockforname (block_num_t, dinode_t *, char *, int, buffer_t **, dirent_t **, dirent_t **, int *);
static dirent_t * alfs_dir_compressEntries (char *, int);
static void alfs_dir_initDirBlock (char *);


/*************************** alfs_dir_lookupname ****************************/
/*                                                                          */
/* scan the directory for "name" and return the corresponding inodeNum      */
/*    value if found.  Otherwise, return -1.                                */
/*                                                                          */
/* Parameters upon entry:                                                   */
/*    fsdev -- the file system identifier used for buffer acquisition       */
/*    dirInode -- the inode for the directory                               */
/*    name -- the name being looked up                                      */
/*    namelen -- length of the name (in characters)                         */
/*                                                                          */
/* Parameters upon exit:  same as entry.                                    */
/*                                                                          */
/* Changes to state: buffer entries for directory blocks may be acquired    */
/*                   (will be released before exit, with no changes made)   */
/*                                                                          */
/****************************************************************************/

int alfs_dir_lookupname (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen) {
     int currPos = 0;
     int dirBlockLength;
     int dirBlockNumber = 0;
     dirent_t *dirent;
     buffer_t *dirBlockBuffer;
     char *dirBlock;
     int i;

     assert (dirInode);

     /* search through the directory looking for the name */

     while (currPos < alfs_dinode_getLength(dirInode)) {
         dirBlockLength = min((alfs_dinode_getLength(dirInode) - currPos), BLOCK_SIZE);
         dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirInode->dinodeNum, dirBlockNumber, BUFFER_READ);
         dirBlockNumber++;
         dirBlock = dirBlockBuffer->buffer;
         for (i=0; i < dirBlockLength; i += dirent->entryLen) {
             dirent = (dirent_t *) (dirBlock + i);
             if ((namelen == dirent->nameLen) && (name[0] == dirent->name[0]) &&
                 (dirent->inodeNum) && (bcmp(dirent->name, name, dirent->nameLen) == 0)) {
                  /* name found */
                  alfs_buffer_releaseBlock (dirBlockBuffer, 0);
                  return(dirent->inodeNum);
             }
         }
         alfs_buffer_releaseBlock (dirBlockBuffer, 0);
         currPos += dirBlockLength;
     }

     return (-1);		/* name not found */
}


/*************************** alfs_dir_getblockforname ***********************/
/*                                                                          */
/* scan the directory for "name" and return 1 if found and 0 otherwise      */
/*                                                                          */
/* Parameters upon entry:                                                   */
/*    fsdev -- the file system identifier used for buffer acquisition       */
/*    dirInode -- the inode for the directory                               */
/*    name -- the name being looked up                                      */
/*    namelen -- length of the name (in characters)                         */
/*    dirBlockBufferP -- undefined                                          */
/*    direntP -- undefined                                                  */
/*    prevDirentP -- undefined                                              */
/*    freeSpaceCount -- undefined                                           */
/*                                                                          */
/* Parameters upon exit (only differences noted):                           */
/*    if name found:                                                        */
/*        dirBlockBufferP -- points to directory block containing entry     */
/*        direntP -- points to directory entry with matching name           */
/*        prevdirentP -- points to the immediately previous entry, or NULL  */
/*                       if direntP points to start of a "sector".          */
/*        freeSpaceCount -- undefined                                       */
/*    if name not found:                                                    */
/*        dirBlockBufferP -- points to a directory "sector" with enough     */
/*                           free space for "name"d entry to be inserted,   */
/*                           or NULL if no such sector currently exists     */
/*        direntP -- points to an entry with enough extra space for "name"d */
/*                   entry to be inserted after it, or NULL is no such      */
/*                   entry exists                                           */
/*        prevDirentP -- points to the beginning of a "sector" with enough  */
/*                       free space (after compression) for "name"d entry   */
/*                       to be inserted.  (Note: undefined if *direntP not  */
/*                       NULL or *dirBlockBufferP is NULL)                  */
/*        freeSpaceCount -- the amount of free space in the "sector"        */
/*                          identified by prevDirentP (Note: undefined      */
/*                          whenever prevDirentP is undefined)              */
/*                                                                          */
/* Changes to state: buffer entries for directory blocks may be acquired    */
/*                   and released with no changes made. The buffer addressed*/
/*                   by *dirBlockBufferP is acquired WITM and must be       */
/*                   released by caller.                                    */
/*                                                                          */
/****************************************************************************/

static int alfs_dir_getblockforname (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen, buffer_t **dirBlockBufferP, dirent_t **direntP, dirent_t **prevDirentP, int *freeSpaceCount) {
     int currPos = 0;
     dirent_t *dirent;
     buffer_t *dirBlockBuffer = NULL;
     char *dirBlock = NULL;
     int dirBlockNumber = 0;
     dirent_t *prevDirent;
     dirent_t *freeDirent = NULL;
     buffer_t *freeBlockBuffer = NULL;
     int dirBlockLength;
     int entryLen = (namelen + SIZEOF_DIRENT_T) & 0xFFFFFFFC;
     int extraSpace;
     int extraSpaceCount;
     int i;

     assert (dirInode);

     /* search through the directory looking for the name */

     while (currPos < alfs_dinode_getLength(dirInode)) {

			/* scan one "sector" at a time */
         dirBlockLength = min((alfs_dinode_getLength(dirInode) - currPos), ALFS_DIR_SECTOR_SIZE);

         if ((currPos % BLOCK_SIZE) == 0) {
			/* next "sector" begins a new directory block */
             dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirInode->dinodeNum, dirBlockNumber, (BUFFER_READ | BUFFER_WITM));
             dirBlockNumber++;
             dirBlock = dirBlockBuffer->buffer;
         } else {
			/* next "sector" in same block as previous "sector" */
             dirBlock += ALFS_DIR_SECTOR_SIZE;
         }

			/* state initialization for scanning a "sector" */
         extraSpaceCount = 0;
         prevDirent = NULL;

			/* examine each entry in turn */
         for (i=0; i < dirBlockLength; i += dirent->entryLen) {
             dirent = (dirent_t *) (dirBlock + i);
             if ((namelen == dirent->nameLen) && (name[0] == dirent->name[0]) &&
                 (dirent->inodeNum) && (bcmp(dirent->name, name, namelen) == 0)) {
				/* directory entry matches name -- return */
                 *dirBlockBufferP = dirBlockBuffer;
                 *direntP = dirent;
                 *prevDirentP = prevDirent;
                 if ((freeBlockBuffer) && (freeBlockBuffer != dirBlockBuffer)) {
                    alfs_buffer_releaseBlock (freeBlockBuffer, 0);
                 }
                 return(1);
             }
			/* name does not match, so compute extra space in entry */
             extraSpace = (dirent->inodeNum == 0) ? dirent->entryLen : (dirent->entryLen - ((dirent->nameLen + SIZEOF_DIRENT_T) & 0xFFFFFFFC));
             extraSpaceCount += extraSpace;
             if ((freeDirent == NULL) && (extraSpace >= entryLen)) {
			/* entry has enough extra space to accomodate addition of */
			/* "name"d entry.  This takes precedence over finding a */
			/* "sector" that can accomodate the new entry after compression */
                  freeDirent = dirent;   /* set marker for entry with extra space */
                  if ((freeBlockBuffer) && (freeBlockBuffer != dirBlockBuffer)) {
			/* stop holding block that needs compression to be used */
                      alfs_buffer_releaseBlock (freeBlockBuffer, 0);
                  }
                  freeBlockBuffer = dirBlockBuffer;
             }
			/* update pointer to previous entry for next iteration */
             prevDirent = dirent;
         }

         if ((freeBlockBuffer == NULL) && (extraSpaceCount >= entryLen)) {
			/* with compression, block can accomodate addition */
			/* of "name"d entry                                */
             freeBlockBuffer = dirBlockBuffer;
             *freeSpaceCount = extraSpaceCount;
             *prevDirentP = (dirent_t *) dirBlock;
         }

		/* done scanning this "sector", prepare to move on to next */
         currPos += dirBlockLength;
         if ((dirBlockBuffer != freeBlockBuffer) && ((currPos % BLOCK_SIZE) == 0)) {
            alfs_buffer_releaseBlock (dirBlockBuffer, 0);
         }
     }

		/* name not found, set extra space identifiers and return 0 */
     *dirBlockBufferP = freeBlockBuffer;
     *direntP = freeDirent;

     return (0);
}


/*************************** alfs_dir_addLink *******************************/
/*                                                                          */
/* add an entry to a directory.  If successful, return 0.  If name already  */
/*    exists, return the inode number of the corresponding entry.  No other */
/*    failures are tolerated.                                               */
/*                                                                          */
/* Parameters upon entry:                                                   */
/*    fsdev -- the file system identifier used for buffer acquisition       */
/*    dirInode -- the inode for the directory                               */
/*    name -- the name to be associated with the new entry                  */
/*    nameLen -- length of the name (in characters)                         */
/*    inodeNum -- the inode number to be associated with the new entry      */
/*    status -- the status to be associated with the new entry              */
/*    flags -- flags used during interactions with the buffer manager.      */
/*             if a new block must be added to the directory, the flags     */
/*             field for BUFFER_ALLOCSTORE is associated with the buffer    */
/*             "get".  All other fields are associated with the buffer      */
/*             release (independent of block allocation needs).             */
/*                                                                          */
/* Parameters upon exit:  same as entry.                                    */
/*                                                                          */
/* Changes to state: buffer entries for directory blocks may be acquired    */
/*                   (will be released before exit).  One directory block   */
/*                   will be modified if the entry addition is successful.  */
/*                                                                          */
/****************************************************************************/

int alfs_dir_addEntry (block_num_t fsdev, dinode_t *dirInode, char *name, int nameLen, dirent_t **newDirentPtr, buffer_t **bufferPtr, int flags) {
     buffer_t *dirBlockBuffer;
     dirent_t *newDirent;
     dirent_t *prevDirent;
     int extraSpace;

     assert (nameLen <= ALFS_DIR_MAX_FILENAME_LENGTH);

     /* find space for the new directory entry and check for existence */

     if (alfs_dir_getblockforname (fsdev, dirInode, name, nameLen, &dirBlockBuffer, &prevDirent, &newDirent, &extraSpace)) {
			/* directory entry is already present */
			/* NOTE: using extraSpace as a temporary variable */
         extraSpace = prevDirent->inodeNum;
         alfs_buffer_releaseBlock (dirBlockBuffer, 0);
         return (extraSpace);
     }

     if (dirBlockBuffer) {
			/* space for the entry is available */
         if (prevDirent == NULL) {
			/* however, must compress the block first */
             prevDirent = alfs_dir_compressEntries ((char *)newDirent, ALFS_DIR_SECTOR_SIZE);
         }
			/* minimize space used by immediately previous entry */
         extraSpace = (prevDirent->inodeNum == 0) ? prevDirent->entryLen : (prevDirent->entryLen - ((prevDirent->nameLen + SIZEOF_DIRENT_T) & 0xFFFFFFFC));
         prevDirent->entryLen -= extraSpace;
         newDirent = (dirent_t *) ((char *) prevDirent + prevDirent->entryLen);
         assert (extraSpace >= ((nameLen + SIZEOF_DIRENT_T) & 0xFFFFFFFC));
     } else {
			/* must add a block to the directory */
         assert((alfs_dinode_getLength(dirInode) % BLOCK_SIZE) == 0);
         dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirInode->dinodeNum, (alfs_dinode_getLength(dirInode) / BLOCK_SIZE), (BUFFER_GET | BUFFER_WITM | (flags & BUFFER_ALLOCSTORE)));
         alfs_dinode_setLength(dirInode, (alfs_dinode_getLength(dirInode) + BLOCK_SIZE));
			/* initialize the new directory block */
         alfs_dir_initDirBlock (dirBlockBuffer->buffer);
			/* the new entry should "own" all of the first "sector" */
         newDirent = (dirent_t *) dirBlockBuffer->buffer;
         extraSpace = newDirent->entryLen;
     }

     /* fill in new dirent */

     bcopy(name, newDirent->name, nameLen);
     newDirent->name[nameLen] = (char) NULL;
     newDirent->entryLen = extraSpace;
     newDirent->nameLen = nameLen;

     *newDirentPtr = newDirent;
     *bufferPtr = dirBlockBuffer;
     
     return (0);
}


int alfs_dir_addLink (block_num_t fsdev, dinode_t *dirInode, char *name, int nameLen, block_num_t inodeNum, char status, int flags)
{
    int ret;
    dirent_t *newDirent;
    buffer_t *dirBlockBuffer;

    ret = alfs_dir_addEntry (fsdev, dirInode, name, nameLen, &newDirent, &dirBlockBuffer, flags);

    if (ret != 0) {
        return (ret);
    } else {
        newDirent->inodeNum = inodeNum;
        newDirent->status = status;
    }

    alfs_buffer_releaseBlock (dirBlockBuffer, ((flags & ~BUFFER_ALLOCSTORE) | BUFFER_DIRTY));

    return (0);
}


/*************************** alfs_dir_removeLink ****************************/
/*                                                                          */
/* Remove a named entry from a directory.  If successful, return the inode  */
/*    number of the removed entry.  If name not found, return 0.  No other  */
/*    failures are tolerated.                                               */
/*                                                                          */
/* Parameters upon entry:                                                   */
/*    fsdev -- the file system identifier used for buffer acquisition       */
/*    dirInode -- the inode for the directory                               */
/*    name -- the name to be associated with the new entry                  */
/*    nameLen -- length of the name (in characters)                         */
/*    flags -- flags used during for the buffer release of the directory    */
/*             block that had an entry removed (if any).                    */
/*                                                                          */
/* Parameters upon exit:  same as entry.                                    */
/*                                                                          */
/* Changes to state: buffer entries for directory blocks may be acquired    */
/*                   (will be released before exit).  One directory block   */
/*                   will be modified if the entry removal is successful.   */
/*                                                                          */
/****************************************************************************/

int alfs_dir_removeLink (block_num_t fsdev, dinode_t *dirInode, char *name, int nameLen, int flags) {
    buffer_t *dirBlockBuffer;
    dirent_t *dirent;
    dirent_t *prevDirent;
    block_num_t inodeNum;

    /* find the dirent for name we're removing                      */
    /* NOTE -- using "inodeNum" as a garbage variable for this call */

    if (alfs_dir_getblockforname (fsdev, dirInode, name, nameLen, &dirBlockBuffer, &dirent, &prevDirent, &inodeNum) == 0) {
		/* name not found */
        if (dirBlockBuffer) {
            alfs_buffer_releaseBlock (dirBlockBuffer, 0);
        }
        return (0);
    }

    /* hold inode number to be returned */

    inodeNum = dirent->inodeNum;

    /* invalidate entry */

    if (prevDirent) {
        prevDirent->entryLen += dirent->entryLen;
    } else {
        dirent->inodeNum = 0;
    }

    alfs_buffer_releaseBlock (dirBlockBuffer, (flags | BUFFER_DIRTY));

    return (inodeNum);
}


void alfs_dir_initDirRaw (dinode_t *dirInode, block_num_t parentInodeNumber, char *dirbuf)
{
    dirent_t *dirent;
    int entryLen;

    /* add the "." entry */

    alfs_dinode_setLength(dirInode, BLOCK_SIZE);
    alfs_dir_initDirBlock (dirbuf);
    dirent = (dirent_t *) dirbuf;
    dirent->inodeNum = dirInode->dinodeNum;
    dirent->status = 0;
    dirent->nameLen = 1;
    dirent->name[0] = '.';
    dirent->name[1] = 0;

    /* add the ".." entry, if appropriate.  Note that this uses the extra */
    /* space originally contained in the "." entry.                       */

    if (parentInodeNumber) {
        entryLen = dirent->entryLen;
        dirent->entryLen = SIZEOF_DIRENT_T;
        dirent = (dirent_t *) ((char *) dirent + SIZEOF_DIRENT_T);
        dirent->inodeNum = parentInodeNumber;
        dirent->status = 0;
        dirent->nameLen = 2;
        dirent->entryLen = entryLen - SIZEOF_DIRENT_T;
        dirent->name[0] = '.';
        dirent->name[1] = '.';
        dirent->name[2] = 0;
    }
}


/*************************** alfs_dir_initDir *******************************/
/*                                                                          */
/* initialize a directory file to contain a "." entry linked to itself and, */
/*     optionally, a ".." entry.  If successful, returns OK.  If the        */
/*     directory file is not empty, returns NOTOK.  No other failures are   */
/*     tolerated.                                                           */
/*                                                                          */
/* Parameters upon entry:                                                   */
/*    fsdev -- the file system identifier used for buffer acquisition       */
/*    dirInode -- the inode for the directory                               */
/*    parentInodeNumber -- the inode number to be assocatiated with the     */
/*                         ".." entry.  If 0, no ".." entry is added.       */
/*    flags -- flags used during interactions with the buffer manager.      */
/*             the flags field for BUFFER_ALLOCSTORE is associated with     */
/*             the buffer "get" of the new directory block.  All other      */
/*             fields are associated with the buffer release.               */
/*                                                                          */
/* Parameters upon exit:  same as entry.                                    */
/*                                                                          */
/* Changes to state: a buffer entry for the new directory block will be     */
/*                   acquired, modified and released.                       */
/*                                                                          */
/****************************************************************************/

int alfs_dir_initDir(block_num_t fsdev, dinode_t *dirInode, block_num_t parentInodeNumber, int flags)
{
    buffer_t *dirBlockBuffer;

    if (alfs_dinode_getLength(dirInode) != 0) {
        return(NOTOK);
    }

    /* get the new directory block */

    dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirInode->dinodeNum, 0, (BUFFER_GET | BUFFER_WITM | (flags & BUFFER_ALLOCSTORE)));
    assert (dirBlockBuffer);

    alfs_dir_initDirRaw (dirInode, parentInodeNumber, dirBlockBuffer->buffer);

    alfs_buffer_releaseBlock (dirBlockBuffer, ((flags & ~BUFFER_ALLOCSTORE) | BUFFER_DIRTY));
    return(OK);
}


int alfs_dir_isEmpty (block_num_t fsdev, dinode_t *dirInode, int *dotdot)
{
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   dirent_t *dirent;
   buffer_t *dirBlockBuffer;
   char *dirBlock;
   int i;

   assert (dirInode);
   assert (dotdot);

   *dotdot = 0;

   /* search through the directory looking for the name */

   while (currPos < alfs_dinode_getLength(dirInode)) {
      dirBlockLength = min((alfs_dinode_getLength(dirInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = alfs_buffer_getBlock (fsdev, dirInode->dinodeNum, dirBlockNumber, BUFFER_READ);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (i=0; i < dirBlockLength; i += dirent->entryLen) {
         dirent = (dirent_t *) (dirBlock + i);
         if ((dirent->inodeNum) && (dirent->nameLen == 2) && (dirent->name[0] == '.') && (dirent->name[1] == '.')) {
            *dotdot = dirent->inodeNum;
         } else if ((dirent->inodeNum) && ((dirent->nameLen > 1) || (dirent->name[0] != '.'))) {
            /* real entry found */
            alfs_buffer_releaseBlock (dirBlockBuffer, 0);
            return(0);
         }
      }
      alfs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }

   return (1);
}


/*************************** alfs_dir_compressEntries ***********************/
/*                                                                          */
/* compress the space in a region of a directory by moving entries forward  */
/*    and combining any scattered extra space.  returns a pointer to the    */
/*    last entry in the region, which contains all of the region's extra    */
/*    space.  No error conditions are tolerated.                            */
/*                                                                          */
/* Parameters upon entry:                                                   */
/*    dirBlock -- points to the region to be compressed.                    */
/*    dirBlockLength -- length of the region to be compressed.              */
/*                                                                          */
/* Parameters upon exit:  same as entry.                                    */
/*                                                                          */
/* Changes to state: the contents of the region may be modified physically. */
/*                   The logical directory state should remain unchanged.   */
/*                                                                          */
/****************************************************************************/

static dirent_t *alfs_dir_compressEntries (char *dirBlock, int dirBlockLength)
{
   dirent_t *dirent = NULL;
   dirent_t *nextDirent;
   int extraSpace;
   int i;

   /* examine/manipulate each directory entry in turn */

   for (i=0; i<dirBlockLength; i+=dirent->entryLen) {
      dirent = (dirent_t *) (dirBlock + i);
      extraSpace = dirent->entryLen - ((dirent->nameLen + SIZEOF_DIRENT_T) & 0xFFFFFFFC);
      assert(extraSpace >= 0);

      /* if entry has extra space and it is not the last entry, then abut the */
      /* next entry to it and "give" the extra space to same.                 */

      if ((extraSpace) && (dirBlockLength > (i + dirent->entryLen))) {
         nextDirent = (dirent_t *) ((char *) dirent + dirent->entryLen);
         dirent->entryLen -= extraSpace;
         nextDirent->entryLen += extraSpace;
         bcopy((char *) nextDirent, ((char *) dirent + dirent->entryLen), ((nextDirent->nameLen + SIZEOF_DIRENT_T) & 0xFFFFFFFC));
      }
   }

   return (dirent);
}


/*************************** alfs_dir_initDirBlock ***********************/
/*                                                                          */
/* initialize the contents of a new directory block.  In particular, this   */
/*    is used to set up the "sectors" that make up the block.  No value     */
/*    is returned and no errors conditions are tolerated.                   */
/*                                                                          */
/* Parameters upon entry:                                                   */
/*    dirBlock -- points to the new directory block.                        */
/*                                                                          */
/* Parameters upon exit:  same as entry.                                    */
/*                                                                          */
/* Changes to state: the contents of the new block will be modified.        */
/*                                                                          */
/****************************************************************************/

static void alfs_dir_initDirBlock (char *dirBlock)
{
   dirent_t *dirent = (dirent_t *) dirBlock;
   int i;

   for (i=0; i<(BLOCK_SIZE/ALFS_DIR_SECTOR_SIZE); i++) {
      dirent->inodeNum = 0;
      dirent->entryLen = ALFS_DIR_SECTOR_SIZE;
      dirent = (dirent_t *) ((char *) dirent + ALFS_DIR_SECTOR_SIZE);
   }
}

