
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


#ifndef __CFFS_PATH_H__
#define __CFFS_PATH_H__

/* cffs_[emb]path.c prototypes */

const char *cffs_path_getNextPathComponent (const char *path, int *compLen);

inode_t *cffs_path_translateNameToInode (buffer_t *sb_buf, const char *path, inode_t *startInode, int flags);

int cffs_path_traversePath (const char *path, inode_t *startInode, inode_t **dirInodePtr, const char **namePtr, int *namelenPtr, int flags);

int cffs_path_lookupname (inode_t *dirInode, const char *name, int nameLen, int *inodeNumPtr);

int cffs_path_makeNewFile (buffer_t *sb_buf, inode_t *dirInode, const char *name, int nameLen, inode_t **inodePtr, int flags, int getmatch, u_int type, int *error);

int cffs_path_addLink (buffer_t *sb_buf, inode_t *dirInode, const char *name, int nameLen, inode_t *inode, int flags, int *error);

inode_t * cffs_path_grabEntry (buffer_t *sb_buf, inode_t *dirInode, const char *name, int nameLen,  void **dirent, buffer_t **buffer);

int cffs_path_renameEntry (buffer_t *sb_buf1, inode_t *dirInode1, const char *name1, buffer_t *sb_buf2, inode_t *dirInode2, const char *name2, int *error);

void cffs_path_removeEntry (inode_t *dirInode, const char *name, int nameLen, void *dirent, inode_t *inode, buffer_t *buffer, int flags);

int cffs_path_isEmpty (buffer_t *sb_buf, inode_t *dirInode);

int cffs_path_getdirentries (buffer_t *sb_buf, inode_t *dirInode, char *buf, int nbytes, off_t *fileposP);

int cffs_path_printdirinfo (buffer_t *sb_buf, inode_t *dirInode);

#endif  /* __CFFS_PATH_H */

