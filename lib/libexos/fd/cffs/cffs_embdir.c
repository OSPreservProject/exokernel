
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


/* functions for accessing and manipulating directories with embedded content */

/* NOTE: this implementation assumes several things about the on-disk	*/
/*       (as it sees it) format of directory entries:			*/
/*       (1) a blank character is included at the end of each name.	*/
/*       (2) all name and embedded content sizes (and therefore entry	*/
/*           sizes) are rounded up to 4-byte boundaries.		*/
/*       (3) no directory entry crosses a "sector" boundary, where the	*/
/*           "sector" size is defined by CFFS_EMBDIR_SECTOR_SIZE.	*/

#include "cffs_buffer.h"
#include "cffs_dinode.h"
#include "cffs_embdir.h"
#include "cffs.h"
#include <assert.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>

static int cffs_embdir_getblockforname (buffer_t *, dinode_t *, const char *, int, int, buffer_t **, embdirent_t **, embdirent_t **, int *);
static embdirent_t * cffs_embdir_compressEntries (char *, int);
void cffs_embdir_initDirBlock (char *);

#define NOCOMPRESSION

static int cffs_embdir_availInodes (char *sector)
{
   dinode_t *dinode;
   int ret = 0;

   dinode = (dinode_t *) (sector + CFFS_DINODE_SIZE);
   ret += (dinode->dinodeNum == 0);
   dinode = (dinode_t *) (sector + (2*CFFS_DINODE_SIZE));
   ret += (dinode->dinodeNum == 0);
   dinode = (dinode_t *) (sector + (3*CFFS_DINODE_SIZE));
   ret += (dinode->dinodeNum == 0);

   return (ret);
}


int cffs_embdir_findfreeInode (char *sector, dinode_t **dinodePtr)
{
   dinode_t *dinode;
   int i;

   for (i=1; i<(CFFS_EMBDIR_SECTOR_SIZE/CFFS_DINODE_SIZE); i++) {
      dinode = (dinode_t *) (sector + (i*CFFS_DINODE_SIZE));
      if (dinode->dinodeNum == 0) {
         if (dinodePtr) {
            *dinodePtr = dinode;
         }
         return (i);
      }
   }

   return (-1);
}


u_int cffs_embdir_activeBlock (char *dirBlock, int len, u_int dirNum)
{
   int i, j;
   u_int status = 0;
   embdirent_t *dirent;

   for (i=0; i<len; i+= CFFS_EMBDIR_SECTOR_SIZE) {
      for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
         dirent = (embdirent_t *) (dirBlock + i + j);
         if (dirent->type != (char) 0) {
            status |= ACTIVE_DIRENT_OWNED;
         }
      }
      for (j=1; j<(CFFS_EMBDIR_SECTOR_SIZE/CFFS_DINODE_SIZE); j++) {
         dinode_t *dinode = (dinode_t *) (dirBlock + i + (j*CFFS_DINODE_SIZE));
         if (dinode->dinodeNum != 0) {
/*
            printf ("valid dinodeNum %x (expected %x, diskBlock %d)\n", dinode->dinodeNum, ((((dirBlockBuffer->header.diskBlock * 8) + i) << 2) + j), dirBlockBuffer->header.diskBlock);
            kprintf ("valid dinodeNum %x (expected %x, diskBlock %d)\n", dinode->dinodeNum, ((((dirBlockBuffer->header.diskBlock * 8) + i) << 2) + j), dirBlockBuffer->header.diskBlock);
*/
            if ((cffs_dinode_getDirNum(dinode)) && (cffs_dinode_getDirNum(dinode) != dirNum)) {
               status |= ACTIVE_INODE_OWNED;
            } else {
               status |= ACTIVE_INODE_NOTOWNED;
            }
         }
      }
   }

   return (status);
}

	/* GROK -- argh! */
#include "cffs_inode.h"

void cffs_embdir_emptyDir (buffer_t *sb_buf, dinode_t *dirInode)
{
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   buffer_t *dirBlockBuffer;
   char *dirBlock;
   int keepblock;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   assert (dirInode);
   assert (cffs_dinode_isDir(dirInode));
/*
printf ("cffs_embdir_emptyDir: dirInodeNum %d (%x), dirLength %qd\n", dirInode->dinodeNum, dirInode->dinodeNum, cffs_dinode_getLength(dirInode));
*/

   while (currPos < cffs_dinode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = min((cffs_dinode_getLength(dirInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, -dirInode->dinodeNum, physicalDirBlockNumber, BUFFER_READ, NULL);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;

      keepblock = cffs_embdir_activeBlock (dirBlock, dirBlockLength, dirInode->dinodeNum);

      if (keepblock) {
/*
         printf ("need to keep directory block!! (block %d, diskBlock %d)\n", dirBlockBuffer->header.block, dirBlockBuffer->header.diskBlock);
         kprintf ("need to keep directory block!! (block %d, diskBlock %d)\n", dirBlockBuffer->header.block, dirBlockBuffer->header.diskBlock);
*/
#ifdef XN
	/* just zero the pointer and eat the resulting space lossage down */
	/* the road...                                                    */
         cffs_dinode_zeroPtr (dirInode, sb_buf, dirBlockBuffer->header.block);
#else
         inode_t *inode = cffs_inode_getInode (((sb->blk << 5) + 2), sb->fsdev, sb->blk, BUFFER_READ);
         cffs_dinode_transferBlock (inode->dinode, sb_buf, dirBlockBuffer->header.diskBlock);
         cffs_inode_releaseInode (inode, BUFFER_WRITE);
         cffs_dinode_zeroPtr (dirInode, sb_buf, dirBlockBuffer->header.block);
#endif
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }
}


/*************************** cffs_embdir_lookupname *********************/
/*									*/
/* scan the directory for "name" and return a pointer to the		*/
/*    corresponding directory entry if found.  Otherwise, return NULL.	*/
/*									*/
/* Parameters upon entry:						*/
/*    fsdev -- the file system identifier used for buffer acquisition	*/
/*    dirInode -- the inode for the directory				*/
/*    name -- the name being looked up					*/
/*    namelen -- length of the name (in characters)			*/
/*    bufferPtr -- pointer to a buffer_t * whose value is undefined	*/
/*									*/
/* Parameters upon exit:  first four same as entry.			*/
/*    bufferPtr -- if name found, (*bufferPtr) contains pointer to	*/
/*                 buffer containing corresponding directory block.	*/
/*                 Must be freed by caller.  Otherwise, undefined.	*/
/*									*/
/* Changes to state: buffer entries for directory blocks may be		*/
/*                   acquired (as indicated above, *bufferPtr held if	*/
/*                   name found)					*/
/*									*/
/************************************************************************/

embdirent_t * cffs_embdir_lookupname (buffer_t *sb_buf, dinode_t *dirInode, const char *name, int namelen, buffer_t **bufferPtr) {
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   buffer_t *dirBlockBuffer;
   char *dirBlock;
   int i, j;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   StaticAssert (CFFS_EMBDIR_SECTOR_SPACEFORNAMES <= (2 << (8 * sizeof(dirent->entryLen))));
   assert (dirInode);
/*
printf ("cffs_embdir_lookupname: name %s, namelen %d, dirInodeNum %d (%x), dirLength %qd\n", name, namelen, dirInode->dinodeNum, dirInode->dinodeNum, cffs_dinode_getLength(dirInode));
*/

   if (namelen == 0) {
      return (NULL);
   }

   /* search through the directory looking for the name */

   while (currPos < cffs_dinode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = min((cffs_dinode_getLength(dirInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, -dirInode->dinodeNum, physicalDirBlockNumber, BUFFER_READ, NULL);
      if (!vpt[PGNO((unsigned int)dirBlockBuffer->buffer)] & PG_P) {
	cffs_buffer_printBlock ((void *)dirBlockBuffer);
	assert (0);
      }
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (i=0; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
         for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
            dirent = (embdirent_t *) (dirBlock + i + j);
            if ((namelen == dirent->nameLen) && (name[0] == dirent->name[0]) && (dirent->type != (char) 0) && (bcmp(dirent->name, name, dirent->nameLen) == 0)) {
               /* name found */
               *bufferPtr = dirBlockBuffer;
               return(dirent);
            }
         }
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }

   return (NULL);		/* name not found */
}


/************************ cffs_embdir_getblockforname *******************/
/*									*/
/* scan the directory for "name" and return 1 if found and 0 otherwise	*/
/*									*/
/* Parameters upon entry:						*/
/*    fsdev -- the file system identifier used for buffer acquisition	*/
/*    dirInode -- the inode for the directory				*/
/*    name -- the name being looked up					*/
/*    namelen -- length of the name (in characters)			*/
/*    entryLen -- length of the new entry (in characters)		*/
/*    dirBlockBufferP -- undefined					*/
/*    direntP -- undefined						*/
/*    prevDirentP -- undefined						*/
/*    freeSpaceCount -- undefined					*/
/*									*/
/* Parameters upon exit (only differences noted):			*/
/*    if name found:							*/
/*        dirBlockBufferP -- points to directory block containing entry	*/
/*        direntP -- points to directory entry with matching name	*/
/*        prevdirentP -- points to the immediately previous entry, or	*/
/*                       NULL if direntP points to start of a "sector".	*/
/*        freeSpaceCount -- undefined					*/
/*    if name not found:						*/
/*        dirBlockBufferP -- points to a directory "sector" with enough	*/
/*                           free space for "name"d entry to be		*/
/*                           inserted, or NULL if no such sector	*/
/*                           currently exists				*/
/*        direntP -- points to an entry with enough extra space for	*/
/*                   "name"d entry to be inserted after it, or NULL if	*/
/*                   no such entry exists				*/
/*        prevDirentP -- points to the beginning of a "sector" with	*/
/*                       enough free space (after compression) for	*/
/*                       "name"d entry to be inserted. (Note: undefined	*/
/*                       if *direntP not NULL or *dirBlockBufferP NULL)	*/
/*        freeSpaceCount -- the amount of free space in the "sector"	*/
/*                          identified by prevDirentP (Note: undefined	*/
/*                          whenever prevDirentP is undefined)		*/
/*									*/
/* Changes to state: buffer entries for directory blocks may be		*/
/*                   acquired and released with no changes made. The	*/
/*                   buffer addressed by *dirBlockBufferP is acquired	*/
/*                   WITM and must be released by caller.		*/
/*									*/
/************************************************************************/

static int cffs_embdir_getblockforname (buffer_t *sb_buf, dinode_t *dirInode, const char *name, int namelen, int inodealso, buffer_t **dirBlockBufferP, embdirent_t **direntP, embdirent_t **prevDirentP, int *freeSpaceCount) {
     int currPos = 0;
     embdirent_t *dirent;
     buffer_t *dirBlockBuffer = NULL;
     char *dirBlock = NULL;
     int dirBlockNumber = 0;
     embdirent_t *prevDirent;
     embdirent_t *freeDirent = NULL;
     buffer_t *freeBlockBuffer = NULL;
     int dirBlockLength;
     int extraSpace;
     int extraSpaceCount;
     int entryLen = cffs_embdir_direntsize2(namelen);
     int i;
     cffs_t *sb = (cffs_t *)sb_buf->buffer;

     assert (dirInode);

     /* search through the directory looking for the name */

     while (currPos < cffs_dinode_getLength(dirInode)) {

			/* scan one "sector" at a time */
         dirBlockLength = min((cffs_dinode_getLength(dirInode) - currPos), CFFS_EMBDIR_SECTOR_SIZE);
         assert (dirBlockLength == CFFS_EMBDIR_SECTOR_SIZE);

         if ((currPos % BLOCK_SIZE) == 0) {
			/* next "sector" begins a new directory block */

	     block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
	     assert (physicalDirBlockNumber);
             dirBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, -dirInode->dinodeNum, physicalDirBlockNumber, (BUFFER_READ | BUFFER_WITM), NULL);
             dirBlockNumber++;
             dirBlock = dirBlockBuffer->buffer;
         } else {
			/* next "sector" in same block as previous "sector" */
             dirBlock += CFFS_EMBDIR_SECTOR_SIZE;
         }

			/* state initialization for scanning a "sector" */
         extraSpaceCount = 0;
         prevDirent = NULL;

			/* examine each entry in turn */
         for (i=0; i < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; i += dirent->entryLen) {
             dirent = (embdirent_t *) (dirBlock + i);
             if ((namelen == dirent->nameLen) && (name[0] == dirent->name[0]) &&
                 (dirent->type != (char) 0) && (bcmp(dirent->name, name, namelen) == 0)) {
				/* directory entry matches name -- return */
                 *dirBlockBufferP = dirBlockBuffer;
                 *direntP = dirent;
                 *prevDirentP = prevDirent;
                 if ((freeBlockBuffer) && (freeBlockBuffer != dirBlockBuffer)) {
                    cffs_buffer_releaseBlock (freeBlockBuffer, 0);
                 }
                 return(1);
             }
			/* name does not match, so compute extra space in entry */
             extraSpace = (dirent->type == (char) 0) ? dirent->entryLen : (dirent->entryLen - embdirentsize(dirent));
             extraSpaceCount += extraSpace;
             if ((freeDirent == NULL) && (extraSpace >= entryLen) && ((inodealso == 0) || (cffs_embdir_availInodes(dirBlock)))) {
			/* entry has enough extra space to accomodate addition of */
			/* "name"d entry.  This takes precedence over finding a */
			/* "sector" that can accomodate the new entry after compression */
                  freeDirent = dirent;   /* set marker for entry with extra space */
                  if ((freeBlockBuffer) && (freeBlockBuffer != dirBlockBuffer)) {
			/* stop holding block that needs compression to be used */
                      cffs_buffer_releaseBlock (freeBlockBuffer, 0);
                  }
                  freeBlockBuffer = dirBlockBuffer;
             }
			/* update pointer to previous entry for next iteration */
             prevDirent = dirent;
         }

         if ((freeBlockBuffer == NULL) && (extraSpaceCount >= entryLen) && ((inodealso == 0) || (cffs_embdir_availInodes(dirBlock)))) {
			/* with compression, block can accomodate addition */
			/* of "name"d entry                                */
             freeBlockBuffer = dirBlockBuffer;
             *freeSpaceCount = extraSpaceCount;
             *prevDirentP = (embdirent_t *) dirBlock;
         }

		/* done scanning this "sector", prepare to move on to next */
         currPos += dirBlockLength;
         if ((dirBlockBuffer != freeBlockBuffer) && ((currPos % BLOCK_SIZE) == 0)) {
            cffs_buffer_releaseBlock (dirBlockBuffer, 0);
         }
     }

		/* name not found, set extra space identifiers and return 0 */
     *dirBlockBufferP = freeBlockBuffer;
     *direntP = freeDirent;

     return (0);
}


/*************************** cffs_embdir_addEntry ***********************/
/*									*/
/* Add an entry to a directory.  If successful, return NULL.  If name	*/
/*    already exists, return a pointer to the corresponding directory	*/
/*    entry.  No other failures are tolerated.				*/
/*									*/
/* Parameters upon entry:						*/
/*    fsdev -- the file system identifier used for buffer acquisition	*/
/*    dirInode -- the inode for the directory				*/
/*    name -- the name to be associated with the new entry		*/
/*    nameLen -- length of the name (in characters)			*/
/*    contentLen -- the length of the content to be embedded		*/
/*    flags -- flags used during interactions with the buffer manager.	*/
/*             if a new block must be added to the directory, the flags	*/
/*             field for BUFFER_ALLOCSTORE is associated with buffer	*/
/*             "get".  All other fields are associated with the buffer	*/
/*             release (independent of block allocation needs).		*/
/*									*/
/* Parameters upon exit:  same as entry.				*/
/*									*/
/* Changes to state: buffer entries for directory blocks may be		*/
/*                   acquired (will be released before exit).  One	*/
/*                   directory block will be modified if the entry	*/
/*                   addition is successful.  If name conflicts, the	*/
/*                   buffer for the corresponding directory block is	*/
/*                   returned and not released.				*/
/*									*/
/************************************************************************/

int cffs_embdir_addEntry (buffer_t *sb_buf, dinode_t *dirInode, const char *name, int nameLen, int inodealso, int flags, embdirent_t **direntPtr, buffer_t **bufferPtr,
			  int *error) {
     buffer_t *dirBlockBuffer;
     embdirent_t *newDirent;
     embdirent_t *prevDirent;
     int extraSpace;
     int entryLen = cffs_embdir_direntsize2(nameLen);
     cffs_t *sb = (cffs_t *)sb_buf->buffer;
     block_num_t dirBlockNum;
/*
printf ("cffs_embdir_addEntry: nameLen %d, inodealso %d, entryLen %d, inum %x\n", nameLen, inodealso, entryLen, dirInode->dinodeNum);
*/
     assert ((nameLen > 0) && (nameLen <= CFFS_EMBDIR_MAX_FILENAME_LENGTH));
     assert (entryLen <= CFFS_EMBDIR_SECTOR_SPACEFORNAMES);

     /* find space for the new directory entry and check for existence */

     if (cffs_embdir_getblockforname (sb_buf, dirInode, name, nameLen, inodealso, &dirBlockBuffer, &prevDirent, &newDirent, &extraSpace)) {
			/* directory entry is already present */
/*
printf ("name conflicts\n");
*/
         *direntPtr = prevDirent;
         *bufferPtr = dirBlockBuffer;
         return(1);
     }

#ifdef NOCOMPRESSION
     if ((dirBlockBuffer) && (prevDirent)) {
#else
     if (dirBlockBuffer) {
#endif
		/* space for the entry is available */
        if (prevDirent == NULL) {
		/* however, must compress the block first */
           prevDirent = cffs_embdir_compressEntries ((char *)newDirent, CFFS_EMBDIR_SECTOR_SIZE);
        }
     } else {
        block_num_t physicalDirBlockNumber;
	int allocate = 0;

			/* must add a block to the directory */
        assert((cffs_dinode_getLength(dirInode) % BLOCK_SIZE) == 0);

        dirBlockNum = cffs_dinode_getLength (dirInode)/BLOCK_SIZE;
			
	*error = 0;
	physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode, sb_buf, dirBlockNum * BLOCK_SIZE, &allocate, error);
	if (*error) return 0;
	assert (allocate);
	assert (physicalDirBlockNumber);

	dirBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, -dirInode->dinodeNum, 
					       physicalDirBlockNumber, (BUFFER_READ | BUFFER_WITM | (flags & BUFFER_ALLOCSTORE)), NULL);
	assert (dirBlockBuffer);

	cffs_embdir_initDirBlock (dirBlockBuffer->buffer); 

        cffs_dinode_setLength(dirInode, (cffs_dinode_getLength(dirInode) + BLOCK_SIZE));
        prevDirent = (embdirent_t *) dirBlockBuffer->buffer;
		/* the new entry should "own" all of the first "sector" */
        assert ((prevDirent->type == (char)0) && (prevDirent->entryLen == CFFS_EMBDIR_SECTOR_SPACEFORNAMES));
     }

			/* minimize space used by immediately previous entry */
     extraSpace = (prevDirent->type == (char) 0) ? prevDirent->entryLen : (prevDirent->entryLen - embdirentsize(prevDirent));
     assert (extraSpace >= entryLen);

#ifndef CFFS_PROTECTED
     prevDirent->entryLen -= extraSpace;
     newDirent = (embdirent_t *) ((char *) prevDirent + prevDirent->entryLen);
     if (newDirent != prevDirent) {
        newDirent->preventryLen = prevDirent->entryLen;
     }
     newDirent->entryLen = extraSpace;
     newDirent->type = (char) 0;
     newDirent->inodeNum = 0;
     assert (((u_int)dirBlockBuffer->buffer % BLOCK_SIZE) == 0);
     if (((int)newDirent + extraSpace) % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
		/* reusing prevDirent for nextDirent */
        prevDirent = (embdirent_t *) ((char *)newDirent + extraSpace);
        prevDirent->preventryLen = extraSpace;
     }
#else
#ifdef CFFSD
     newDirent = (embdirent_t *) cffsd_fsupdate_directory (CFFS_DIRECTORY_SPLITENTRY, prevDirent, extraSpace, 0, 0, 0);
#else 
     newDirent = (embdirent_t *) sys_fsupdate_directory (CFFS_DIRECTORY_SPLITENTRY, prevDirent, extraSpace, 0, 0, 0);
#endif /* CFFSD */
     assert (!((u_int)newDirent % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) || (newDirent->preventryLen != 0));
#endif

	     /* fill in new dirent */

#ifndef CFFS_PROTECTED
     bcopy(name, newDirent->name, nameLen);
     //     newDirent->name[nameLen] = (char) 0;
     newDirent->nameLen = nameLen;
#else
#ifdef CFFSD
     cffsd_fsupdate_directory4 (CFFS_DIRECTORY_SETNAME, newDirent, (u_int)name, nameLen, (uint)dirInode, sb->fsdev);
#else
     sys_fsupdate_directory (CFFS_DIRECTORY_SETNAME, newDirent, (u_int)name, nameLen, (uint)dirInode, sb->fsdev);
#endif /* CFFSD */
#endif

     *direntPtr = newDirent;
     *bufferPtr = dirBlockBuffer;
/*
printf ("cffs_embdir_addEntry done\n");
*/
     return (0);
}


/********************* cffs_embdir_removeLink_byEntry *******************/
/*									*/
/* Remove an entry from a directory.  No failure conditions are 	*/
/*     tolerated.							*/
/*									*/
/* Parameters upon entry:						*/
/*    dirent -- pointer to the embdirent_t to be removed (it is assumed	*/
/*              that the directory entry is valid and that the		*/
/*              directory block is held by the caller)			*/
/*    dirBlockBuffer -- buffer for directory block containing dirent	*/
/*									*/
/* Parameters upon exit:  same as entry					*/
/*									*/
/* Changes to state: directory block contents are modified to remove	*/
/*                   the directory entry.				*/
/*									*/
/************************************************************************/

void cffs_embdir_removeLink_byEntry (embdirent_t *dirent, buffer_t *dirBlockBuffer)
{
   assert (dirent->type == (char) 0);
   if (((int)dirent - (int)dirBlockBuffer->buffer) % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
      embdirent_t *prevDirent = (embdirent_t *) ((char *)dirent - dirent->preventryLen);
      assert (prevDirent->entryLen == dirent->preventryLen);
#ifndef CFFS_PROTECTED
      prevDirent->entryLen += dirent->entryLen;
      if (((int)dirent - (int)dirBlockBuffer->buffer + dirent->entryLen) % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
         embdirent_t *nextDirent = (embdirent_t *) ((char *)dirent + dirent->entryLen);
         nextDirent->preventryLen = prevDirent->entryLen;
      }
#else
#ifdef CFFSD 
      cffsd_fsupdate_directory (CFFS_DIRECTORY_MERGEENTRY, prevDirent, dirent->entryLen, 0, 0, 0);
#else
      sys_fsupdate_directory (CFFS_DIRECTORY_MERGEENTRY, prevDirent, dirent->entryLen, 0, 0, 0);
#endif /* CFFSD */
     assert (!((u_int)prevDirent % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) || (prevDirent->preventryLen != 0));
#endif
   }
}


int cffs_embdir_isEmpty (buffer_t *sb_buf, dinode_t *dirInode)
{
   int currPos = 0;
   int dirBlockLength;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   buffer_t *dirBlockBuffer;
   char *dirBlock;
   int i, j;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;

   assert (dirInode);

   /* search through the directory looking for the name */

   while (currPos < cffs_dinode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = min((cffs_dinode_getLength(dirInode) - currPos), BLOCK_SIZE);
      dirBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, -dirInode->dinodeNum, physicalDirBlockNumber, BUFFER_READ, NULL);
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
      for (i=0; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
         for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
            dirent = (embdirent_t *) (dirBlock + i + j);
            if (dirent->type != (char) 0) {
/*
printf ("cffs_embdir_isEmpty: directory not empty.  Contains name %s (type %d)\n", dirent->name, dirent->type);
*/
               cffs_buffer_releaseBlock (dirBlockBuffer, 0);
               return (0);
            }
         }
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, 0);
      currPos += dirBlockLength;
   }

   return (1);		/* directory empty */
}


/************************* cffs_embdir_compressEntries ******************/
/*									*/
/* compress the space in a region of a directory by moving entries	*/
/*    forward and combining any scattered extra space.  returns a	*/
/*    pointer to the last entry in the region, which contains all of	*/
/*    the region's extra space.  No error conditions are tolerated.	*/
/*									*/
/* Parameters upon entry:						*/
/*    dirBlock -- points to the region to be compressed.		*/
/*    dirBlockLength -- length of the region to be compressed.		*/
/*									*/
/* Parameters upon exit:  same as entry.				*/
/*									*/
/* Changes to state: the contents of the region may be modified		*/
/*                   physically.  The logical directory state should	*/
/*                   remain unchanged.					*/
/*									*/
/************************************************************************/

static embdirent_t *cffs_embdir_compressEntries (char *dirBlock, int dirBlockLength)
{
   embdirent_t *dirent = NULL;
   int extraSpace;
   int i;

kprintf ("compressing directory block\n");
   assert (dirBlockLength == CFFS_EMBDIR_SECTOR_SIZE);

   /* examine/manipulate each directory entry in turn */

   for (i=0; i<CFFS_EMBDIR_SECTOR_SPACEFORNAMES; i+=dirent->entryLen) {
      dirent = (embdirent_t *) (dirBlock + i);
      extraSpace = (dirent->type != (char)0) ? dirent->entryLen - embdirentsize(dirent) : dirent->entryLen;
      assert(extraSpace >= 0);

      /* if entry has extra space and it is not the last entry, then abut the */
      /* next entry to it and "give" the extra space to same.                 */

      if ((extraSpace) && (CFFS_EMBDIR_SECTOR_SPACEFORNAMES > (i + dirent->entryLen))) {
#ifndef CFFS_PROTECTED
         embdirent_t *nextDirent = (embdirent_t *) ((char *) dirent + dirent->entryLen);
         dirent->entryLen -= extraSpace;
         nextDirent->entryLen += extraSpace;
         nextDirent->preventryLen = dirent->entryLen;
         bcopy((char *) nextDirent, ((char *) dirent + dirent->entryLen), embdirentsize(nextDirent));
#else
#ifdef CFFSD
         cffsd_fsupdate_directory (CFFS_DIRECTORY_SHIFTENTRY, dirent, -extraSpace, 0, 0, 0);
#else
         sys_fsupdate_directory (CFFS_DIRECTORY_SHIFTENTRY, dirent, -extraSpace, 0, 0, 0);
#endif /* CFFSD */
#endif
      }
   }

   return (dirent);
}


/*************************** cffs_embdir_initDirBlock *******************/
/*									*/
/* initialize the contents of a new directory block.  In particular,	*/
/*    this is used to set up the "sectors" that make up the block.	*/
/*    No value is returned and no errors conditions are tolerated.	*/
/*									*/
/* Parameters upon entry:						*/
/*    dirBlock -- points to the new directory block.			*/
/*									*/
/* Parameters upon exit:  same as entry.				*/
/*									*/
/* Changes to state: the contents of the new block will be modified.	*/
/*									*/
/************************************************************************/

void cffs_embdir_initDirBlock (char *dirBlock)
{
   embdirent_t *dirent = (embdirent_t *) dirBlock;
#ifndef CFFS_PROTECTED
   dinode_t *dinode;
   int i;

   for (i=0; i<(BLOCK_SIZE/CFFS_EMBDIR_SECTOR_SIZE); i++) {
      dirent->type = (char) 0;
      dirent->entryLen = CFFS_EMBDIR_SECTOR_SPACEFORNAMES;
      dirent->preventryLen = 0;

      dinode = (dinode_t *) ((char *)dirent + CFFS_DINODE_SIZE);
      dinode->dinodeNum = 0;
      dinode = (dinode_t *) ((char *)dirent + (2*CFFS_DINODE_SIZE));
      dinode->dinodeNum = 0;
      dinode = (dinode_t *) ((char *)dirent + (3*CFFS_DINODE_SIZE));
      dinode->dinodeNum = 0;

      dirent = (embdirent_t *) ((char *) dirent + CFFS_EMBDIR_SECTOR_SIZE);
   }
#else
#ifdef CFFSD
   cffsd_fsupdate_directory (CFFS_DIRECTORY_INITDIRBLOCK, dirent, 0, 0, 0, 0);
#else
   sys_fsupdate_directory (CFFS_DIRECTORY_INITDIRBLOCK, dirent, 0, 0, 0, 0);
#endif /* CFFSD */
#endif
}

