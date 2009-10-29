
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


#ifndef __CFFS_EMBDIR_H__
#define __CFFS_EMBDIR_H__

/* on-disk layout information for directory entries with embedded content */

/* Directory entry types */

#define CFFS_EMBDIR_INVALID	0
#define CFFS_EMBDIR_PERM	1

#define CFFS_EMBDIR_ENTRYTYPE(dirent)	((dirent)->type & (unsigned char) 3)

	/* NOTE: SECTOR_SIZE (below) must evenly divide BLOCK_SIZE (external) */
	/*       that is, assert ((SS <= BS) && ((BS % SS) == 0));            */

#define CFFS_EMBDIR_SECTOR_SIZE 	512	/* directory block "sector" size */

	/* Each sector is partitioned (statically, for now) into space for */
	/* entries (names) and space for embedded inodes.                  */

#define CFFS_EMBDIR_SECTOR_SPACEFORNAMES	128

	/* NOTE: SECTOR_SPACEFORNAMES (above) must exceed the maximum dirent */
	/*       size, which includes the maximum name size (below), the     */
	/*       basic dirent size and any directory entry assumptions (eg,  */
	/*       inclusion of a blank space at end of name, rounding up to   */
	/*       convenient size, etc...)                                    */

typedef struct embdirent {
     u_int8_t	entryLen;	/* length of directory entry (in bytes) */
     u_int8_t	preventryLen;	/* length of previous entry (in bytes) */
     u_int8_t	type;		/* status info for the directory entry */
     u_int8_t	nameLen;	/* length of link name */
     u_int32_t	inodeNum;	/* inode number that this entry points to */
#define SIZEOF_PREHELD_NAME	24
     char	name[SIZEOF_PREHELD_NAME];	/* link name - size is any multiple of 4 */
} embdirent_t;

	/* NOTE: SIZEOF_EMBDIRENT_T (below) must be >= sizeof(dirent_t).      */
	/*       This is a minimum size, based on assuming 0 bytes of content */

#define SIZEOF_NONNAME_PORTION	8
#define SIZEOF_EMBDIRENT_T	(SIZEOF_NONNAME_PORTION + SIZEOF_PREHELD_NAME)

	/* max file name length */

#define CFFS_EMBDIR_MAX_FILENAME_LENGTH 	(CFFS_EMBDIR_SECTOR_SPACEFORNAMES - SIZEOF_NONNAME_PORTION)

#define embdirentsize(dirent) \
        (SIZEOF_EMBDIRENT_T + (((dirent)->nameLen <= SIZEOF_PREHELD_NAME) ? 0 : (((dirent)->nameLen + 1 + 3 - SIZEOF_PREHELD_NAME) & 0xFFFFFFFC)))

#define cffs_embdir_direntsize2(namelen) \
        (SIZEOF_EMBDIRENT_T + (((namelen) <= SIZEOF_PREHELD_NAME) ? 0 : (((namelen) + 1 + 3 - SIZEOF_PREHELD_NAME) & 0xFFFFFFFC)))

#define cffs_embdir_sector(buft,dirent) \
	(((buft)->header.diskBlock * BLOCK_SIZE / CFFS_EMBDIR_SECTOR_SIZE) + \
	 (((u_int)dirent - (u_int)(buft)->buffer) / CFFS_EMBDIR_SECTOR_SIZE))


/* cffs_embdir.c prototypes */

embdirent_t * cffs_embdir_lookupname (buffer_t *sb_buf, dinode_t *dirInode, const char *name, int namelen, struct buffer_t **bufferPtr);
int cffs_embdir_addEntry (buffer_t *sb_buf, dinode_t *dirInode, const char *name, int nameLen, int inodealso, int flags, embdirent_t **direntPtr, struct buffer_t **bufferPtr, int *error);
void cffs_embdir_removeLink_byEntry (embdirent_t *dirent, struct buffer_t *dirBlockBuffer);
int cffs_embdir_isEmpty (buffer_t *sb_buf, dinode_t *dirInode);
int cffs_embdir_findfreeInode (char *dirSector, dinode_t **dinodePtr);
void cffs_embdir_emptyDir (buffer_t *sb_buf, dinode_t *dinode);
void cffs_embdir_initDirBlock (char *dirBlock);

	/* scan thru block and check for active fields (and who owns them) */

u_int cffs_embdir_activeBlock (char *dirBlock, int len, u_int dirNum);

	/* return flags for previous call (NOTE: these flags may be or'd) */

#define ACTIVE_DIRENT_OWNED	1
#define ACTIVE_INODE_OWNED	2
#define ACTIVE_INODE_NOTOWNED	4	/* NOTE: not unambiguously owned */

#endif /* __CFFS_EMBDIR_H__ */

