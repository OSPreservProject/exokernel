
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


#ifndef __CFFS_INODE_H__
#define __CFFS_INODE_H__

#include "cffs_general.h"
#include "cffs_dinode.h"

/* in-memory inode structure */

struct inode_t {
   struct inode_t *next;
   struct inode_t *prev;
   struct inode_t *lru_next;
   struct inode_t *lru_prev;
   u_int inodeNum;		/* self-identification */
   u_int fsdev;			/* the file system device identifier */
   u_int sprblk;		/* superblock block num for this filesystem */
   int refCount;		/* local ref count */
   dinode_t *dinode;		/* the corresponding on-disk data structure */
   buffer_t *dinodeBlockBuffer;	/* the disk block that contains the dinode */
   u_int hashbucket;
};
typedef struct inode_t inode_t;

#define cffs_inode_dinode(inode)	(inode)->dinode

#define cffs_inode_isDir(inode)		cffs_dinode_isDir((inode)->dinode)
#define cffs_inode_isRegFile(inode)	cffs_dinode_isRegFile((inode)->dinode)
#define cffs_inode_isSymLink(inode)	cffs_dinode_isSymLink((inode)->dinode)

#define cffs_inode_getMask(inode)	cffs_dinode_getMask((inode)->dinode)
#define cffs_inode_getUid(inode)	cffs_dinode_getUid((inode)->dinode)
#define cffs_inode_getGid(inode)	cffs_dinode_getGid((inode)->dinode)
#define cffs_inode_getModTime(inode)	cffs_dinode_getModTime((inode)->dinode)
#define cffs_inode_getAccTime(inode)	cffs_dinode_getAccTime((inode)->dinode)
#define cffs_inode_getCreTime(inode)	cffs_dinode_getCreTime((inode)->dinode)
#define cffs_inode_getLength(inode)	cffs_dinode_getLength((inode)->dinode)
#define cffs_inode_getType(inode)	cffs_dinode_getType((inode)->dinode)
#define cffs_inode_getLinkCount(inode)	cffs_dinode_getLinkCount((inode)->dinode)
#define cffs_inode_getOpenCount(inode)	cffs_dinode_getOpenCount((inode)->dinode)
#define cffs_inode_getDirNum(inode)	cffs_dinode_getDirNum((inode)->dinode)
#define cffs_inode_getDirect(inode,ptrno)	cffs_dinode_getDirect((inode)->dinode,ptrno)
#define cffs_inode_getGroupstart(inode)		cffs_dinode_getGroupstart((inode)->dinode)
#define cffs_inode_getGroupsize(inode)		cffs_dinode_getGroupsize((inode)->dinode)
#define cffs_inode_getExtrafield1(inode)	cffs_dinode_getExtrafield1((inode)->dinode)
#define cffs_inode_getExtrafield2(inode)	cffs_dinode_getExtrafield2((inode)->dinode)
#define cffs_inode_setMask(inode,val)		cffs_dinode_setMask((inode)->dinode,val)
#define cffs_inode_setUid(inode,val)		cffs_dinode_setUid((inode)->dinode,val)
#define cffs_inode_setGid(inode,val)		cffs_dinode_setGid((inode)->dinode,val)
#define cffs_inode_setAccTime(inode,val)	cffs_dinode_setAccTime((inode)->dinode,val)
#define cffs_inode_setModTime(inode,val)	cffs_dinode_setModTime((inode)->dinode,val)
#define cffs_inode_setCreTime(inode,val)	cffs_dinode_setCreTime((inode)->dinode,val)
#define cffs_inode_setLength(inode,val)		cffs_dinode_setLength((inode)->dinode,val)
#define cffs_inode_setType(inode,val)		cffs_dinode_setType((inode)->dinode,val)
#define cffs_inode_setLinkCount(inode,val)	cffs_dinode_setLinkCount((inode)->dinode,val)
#define cffs_inode_setOpenCount(inode,val)	cffs_dinode_setOpenCount((inode)->dinode,val)
#define cffs_inode_setDirNum(inode,val)		cffs_dinode_setDirNum((inode)->dinode,val)
#define cffs_inode_setDirect(inode,ptrno,val)	cffs_dinode_setDirect((inode)->dinode,ptrno,val)
#define cffs_inode_setGroupstart(inode,val)	cffs_dinode_setGroupstart((inode)->dinode,val)
#define cffs_inode_setGroupsize(inode,val)	cffs_dinode_setGroupsize((inode)->dinode,val)
#define cffs_inode_setExtrafield1(inode,val)	cffs_dinode_setExtrafield1((inode)->dinode,val)
#define cffs_inode_setExtrafield2(inode,val)	cffs_dinode_setExtrafield2((inode)->dinode,val)

/* cffs_inode.c */

void cffs_inode_initInodeCache();
void cffs_inode_shutdownInodeCache();
void cffs_inode_printInodeCache();
void cffs_inode_flushInodeCache();
inode_t *cffs_inode_makeInode (block_num_t fsdev, block_num_t sprblk, uint inodeNum, dinode_t *dinode, buffer_t *dinodeBlockBuffer);
inode_t *cffs_inode_getInode (uint inodeNum, block_num_t fsdev, block_num_t sprblk, int flags);
void cffs_inode_tossInode (inode_t *inode);
int cffs_inode_releaseInode (inode_t *inode, int flags);
buffer_t *cffs_inode_getDinodeBuffer (inode_t *inode);
void cffs_inode_dirtyInode (inode_t *inode);
void cffs_inode_writeInode (inode_t *inode, int flags);
int cffs_inode_truncInode (inode_t *inode, off_t length);
block_num_t cffs_inode_offsetToBlock (inode_t *inode, off_t offset, int *allocate, int *error);
int cffs_inode_getNumContig (inode_t *inode, block_num_t blkno);
int cffs_inode_getRefCount (inode_t *inode);

#endif  /* __CFFS_INODE_H__ */

