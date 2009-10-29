
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

#ifndef __ALFS_EMBDIR_H__
#define __ALFS_EMBDIR_H__

/* on-disk layout information for directory entries with embedded content */

	/* NOTE: SECTOR_SIZE (below) must evenly divide BLOCK_SIZE (external) */
	/*       that is, assert ((SS <= BS) && ((BS % SS) == 0));            */

#define ALFS_EMBDIR_SECTOR_SIZE 	512	/* directory block "sector" size */

	/* NOTE: SECTOR_SIZE (above) must exceed the maximum directory entry */
	/*       size, which includes the maximum name size (below), the     */
	/*       basic dirent size and any directory entry assumptions (eg,  */
	/*       inclusion of a blank space at end of name, rounding up to   */
	/*       convenient size, etc...)                                    */

	/* The exception to the above rule is when the user-embedded portion */
	/* causes an entry's size to exceed SECTOR_SIZE.  In this case, the  */
	/* one entry will span consecutive sectors within a block, and then  */
	/* consecutive blocks.                                               */

#define ALFS_EMBDIR_MAX_FILENAME_LENGTH 	255	/* max file name */

struct embdirent {
     u_int16_t	entryLen;	/* length of directory entry (in bytes) */
     u_int16_t	preventryLen;	/* length of previous entry (in bytes) */
     u_int16_t	contentLen;	/* length of embedded content (in bytes) */
     u_int8_t	type;		/* status info for the directory entry */
     u_int8_t	nameLen;	/* length of link name */
     char	name[4];	/* link name - size is any multiple of 4 */
   /*char	content[];*/	/* embedded content */
};

typedef struct embdirent embdirent_t;

	/* NOTE: SIZEOF_EMBDIRENT_T (below) must be >= sizeof(dirent_t).      */
	/*       This is a minimum size, based on assuming 0 bytes of content */

#define SIZEOF_EMBDIRENT_T 12

#define embdirentsize(dirent) \
        (((dirent)->nameLen + (dirent)->contentLen + 3 + SIZEOF_EMBDIRENT_T) \
                   & 0xFFFFFFFC)

#define embdirentcontent(dirent) \
	((char *)(dirent) + (((dirent)->nameLen + SIZEOF_EMBDIRENT_T) & 0xFFFFFFFC))

/* inode number structure is as follows:  0xAAAABBBB			*/
/*     where AAAA indicates the directory number and BBBB indicates the	*/
/*     ID within the directory (BBBB & 0x0000FFF8 will indicate the	*/
/*     containing sector -- the remainder is a unique ID).  BBBB == 0	*/
/*     indicates the directory itself.  This needs to pass thru the	*/
/*     directory table to disambiguate the location of the directory's	*/
/*     inode (within another directory).  AAAA == 0 indicates that the	*/
/*     inode is not embedded and rather uses the more conventional	*/
/*     level of indirection (alfs_embdinode*).  This should only be	*/
/*     used for hard links other than '..' directory entries.		*/

#define alfs_embdir_isEmbedded(dinodeNum) \
	((dinodeNum) & 0xFFFF0000)

#define alfs_embdir_isEntry(dinodeNum) \
	((dinodeNum) & 0x0000FFF8)

#define alfs_embdir_dirDInodeNum(dinodeNum) \
	((dinodeNum) & 0xFFFF0000)

#define alfs_embdir_entryBlkno(dinodeNum) \
	((((((dinodeNum) & 0x0000FFFF) >> 3) - 1) * ALFS_EMBDIR_SECTOR_SIZE) / BLOCK_SIZE)

/* alfs_embdir.c prototypes */

int alfs_embdir_initmetablks ();
unsigned int alfs_embdir_initDisk (cap2_t fscap, block_num_t fsdev, int blkno, int rootdirno, int rootinum);
unsigned int alfs_embdir_mountDisk (cap2_t fscap, block_num_t fsdev, int blkno);
unsigned int alfs_embdir_unmountDisk (cap2_t fscap, block_num_t fsdev, int blkno);
embdirent_t * alfs_embdir_lookupname (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen, buffer_t **bufferPtr);
int alfs_embdir_addEntry (block_num_t fsdev, dinode_t *dirInode, char *name, int nameLen, int contentLen, int flags, embdirent_t **direntPtr, buffer_t **bufferPtr);
int alfs_embdir_initDir (block_num_t fsdev, dinode_t *dirInode, char type, block_num_t parentInodeNumber, int flags);
void alfs_embdir_initDirRaw (dinode_t *dirInode, char type, block_num_t parentInodeNumber, char *dirbuf);
int alfs_embdir_removeLink_byName (block_num_t fsdev, dinode_t *dirInode, char *name, int nameLen, int flags);
void alfs_embdir_removeLink_byEntry (embdirent_t *dirent, buffer_t *dirBlockBuffer);
int alfs_embdir_isEmpty (block_num_t fsdev, dinode_t *dirInode, int *dotdot);
unsigned int alfs_embdir_transDirNum (unsigned int dinodeNum);
unsigned int alfs_embdir_allocDirNum (unsigned int dinodeNum);
void alfs_embdir_deallocDirNum (unsigned int dirnum);
dinode_t * alfs_embdir_getInode (unsigned int fsdev, unsigned int effdinodeNum, unsigned int dinodeNum, buffer_t **dirBlockBufferPtr);

#endif /* __ALFS_EMBDIR_H__ */

