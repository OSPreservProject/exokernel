
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

#ifndef __ALFS_DIR_H__
#define __ALFS_DIR_H__

/* on disk directory entry layout information */

	/* NOTE: SECTOR_SIZE (below) must evenly divide BLOCK_SIZE (external) */
        /*       that is, assert ((SS <= BS) && ((BS % SS) == 0));            */

#define ALFS_DIR_SECTOR_SIZE	512	/* directory block "sector" size */

	/* NOTE: SECTOR_SIZE (above) must exceed the maximum directory entry */
	/*       size, which includes the maximum name size (below), the     */
	/*       basic dirent size and any directory entry assumptions (eg,  */
	/*       inclusion of a blank space at end of name, rounding up to   */
	/*       convenient size, etc...)                                    */

#define ALFS_DIR_MAX_FILENAME_LENGTH	255	/* max file name */

struct dirent {
     u_int32_t	inodeNum;	/* inode number of directory entry */
     u_int16_t	entryLen;	/* length of directory entry (in bytes) */
     u_int8_t	status;		/* status info for the directory entry */
     u_int8_t	nameLen;	/* length of link name */
     char	name[4];	/* link name */
};

typedef struct dirent dirent_t;

	/* NOTE: SIZEOF_DIRENT_T (below) must be >= sizeof(dirent_t) */

#define SIZEOF_DIRENT_T 12

/* alfs_dir.c prototypes */

int alfs_dir_lookupname (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen);
int alfs_dir_addEntry (block_num_t fsdev, dinode_t *dirInode, char *name, int nameLen, dirent_t **newDirentPtr, buffer_t **bufferPtr, int flags);
int alfs_dir_addLink (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen, block_num_t inodeNum, char status, int flags);
int alfs_dir_removeLink (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen, int flags);
int alfs_dir_isEmpty (block_num_t fsdev, dinode_t *dirInode, int *dotdot);
int alfs_dir_initDir (block_num_t fsdev, dinode_t *dirInode, block_num_t parentInodeNumber, int flags);
void alfs_dir_initDirRaw (dinode_t *dirInode, block_num_t parentInodeNumber, char *dirbuf);

#endif /* __ALFS_DIR_H__ */

