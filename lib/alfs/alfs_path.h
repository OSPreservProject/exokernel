
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


#ifndef __ALFS_PATH_H__
#define __ALFS_PATH_H__

/* alfs_[emb]path.c prototypes */

char *alfs_path_getNextPathComponent (char *path, int *compLen);

int alfs_path_translateNameToInode (char *path, inode_t *startInode, inode_t **inodePtr, int flags);

int alfs_path_traversePath (char *path, inode_t *startInode, inode_t **dirInodePtr, char **namePtr, int *namelenPtr, int flags);

int alfs_path_lookupname (inode_t *dirInode, char *name, int nameLen, int *inodeNumPtr);

/*
int alfs_path_addLink (inode_t *dirInode, char *name, int nameLen, inode_t **inodePtr, char status, int flags);
*/

int alfs_path_prepareEntry (inode_t *dirInode, char *name, int nameLen, inode_t *inode, void **dirent, buffer_t **buffer, int flags, int getmatch);

void alfs_path_fillEntry (inode_t *dirInode, void *dirent, inode_t *inode, char status, buffer_t *buffer, int seqinode, int flags);

inode_t * alfs_path_grabEntry (inode_t *dirInode, char *name, int nameLen,  void **dirent, buffer_t **buffer);

/* the extra arguments (for both alfs_path.c and alfs_embpath.c) are to */
/* keep the interface common between the two... */

void alfs_path_removeEntry (inode_t *dirInode, char *name, int nameLen, void *dirent, inode_t *inode, buffer_t *buffer, int flags);

int alfs_path_removeLink (inode_t *dirInode, char *name, int nameLen, int flags);

int alfs_path_isEmpty (inode_t *dirInode, int *dotdot);

int alfs_path_initDir (inode_t *dirInode, inode_t *parentInode, int flags);

void alfs_path_initDirRaw (dinode_t *dirDInode, dinode_t *parentDInode, char *dirbuf);

void alfs_path_listDirectory (inode_t *curRoot);

int alfs_path_initmetablks ();
unsigned int alfs_path_initDisk (cap2_t fscap, block_num_t fsdev, int blkno, int rootdirno, int rootinum);
unsigned int alfs_path_mountDisk (cap2_t fscap, block_num_t fsdev, int blkno);
unsigned int alfs_path_unmountDisk (cap2_t fscap, block_num_t fsdev, int blkno);

#endif  /* __ALFS_PATH_H */

