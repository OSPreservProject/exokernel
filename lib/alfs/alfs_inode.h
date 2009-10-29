
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

#ifndef __ALFS_INODE_H__
#define __ALFS_INODE_H__

#include "alfs/alfs_general.h"
#include "alfs/alfs_dinode.h"

/* in-memory inode structure */

struct inode_t {
    struct inode_t *next;
    struct inode_t *prev;
    unsigned int inodeNum;	/* self-identification */
    block_num_t fsdev;		/* the file system device identifier */
    int refCount;		/* number of application references */
    dinode_t *dinode;		/* the corresponding on-disk data structure */
    int fileptr;		/* used to be fd.fp -- used to hold place */
    int flags;			/* used to be fd.flags */
};
typedef struct inode_t inode_t;

#define alfs_inode_getMask(inode)	alfs_dinode_getMask((inode)->dinode)
#define alfs_inode_getUid(inode)	alfs_dinode_getUid((inode)->dinode)
#define alfs_inode_getGid(inode)	alfs_dinode_getGid((inode)->dinode)
#define alfs_inode_getModTime(inode)	alfs_dinode_getModTime((inode)->dinode)
#define alfs_inode_getAccTime(inode)	alfs_dinode_getAccTime((inode)->dinode)
#define alfs_inode_getCreTime(inode)	alfs_dinode_getCreTime((inode)->dinode)
#define alfs_inode_getLength(inode)	alfs_dinode_getLength((inode)->dinode)
#define alfs_inode_getType(inode)	alfs_dinode_getType((inode)->dinode)
#define alfs_inode_getLinkCount(inode)	alfs_dinode_getLinkCount((inode)->dinode)
#define alfs_inode_getDirect(inode,ptrno)	alfs_dinode_getDirect((inode)->dinode,ptrno)
#define alfs_inode_getSingleIndirect(inode)	alfs_dinode_getSingleIndirect((inode)->dinode)
#define alfs_inode_getDoubleIndirect(inode)	alfs_dinode_getDoubleIndirect((inode)->dinode)
#define alfs_inode_getTripleIndirect(inode)	alfs_dinode_getTripleIndirect((inode)->dinode)
#define alfs_inode_getGroupstart(inode)		alfs_dinode_getGroupstart((inode)->dinode)
#define alfs_inode_getGroupsize(inode)		alfs_dinode_getGroupsize((inode)->dinode)
#define alfs_inode_getExtrafield1(inode)	alfs_dinode_getExtrafield1((inode)->dinode)
#define alfs_inode_getExtrafield2(inode)	alfs_dinode_getExtrafield2((inode)->dinode)
#define alfs_inode_setMask(inode,val)		alfs_dinode_setMask((inode)->dinode,val)
#define alfs_inode_setUid(inode,val)		alfs_dinode_setUid((inode)->dinode,val)
#define alfs_inode_setGid(inode,val)		alfs_dinode_setGid((inode)->dinode,val)
#define alfs_inode_setAccTime(inode,val)	alfs_dinode_setAccTime((inode)->dinode,val)
#define alfs_inode_setModTime(inode,val)	alfs_dinode_setModTime((inode)->dinode,val)
#define alfs_inode_setCreTime(inode,val)	alfs_dinode_setCreTime((inode)->dinode,val)
#define alfs_inode_setLength(inode,val)		alfs_dinode_setLength((inode)->dinode,val)
#define alfs_inode_setType(inode,val)		alfs_dinode_setType((inode)->dinode,val)
#define alfs_inode_setLinkCount(inode,val)	alfs_dinode_setLinkCount((inode)->dinode,val)
#define alfs_inode_setDirect(inode,ptrno,val)	alfs_dinode_setDirect((inode)->dinode,ptrno,val)
#define alfs_inode_setSingleIndirect(inode,val)	alfs_dinode_setSingleIndirect((inode)->dinode,val)
#define alfs_inode_setDoubleIndirect(inode,val)	alfs_dinode_setDoubleIndirect((inode)->dinode,val)
#define alfs_inode_setTripleIndirect(inode,val)	alfs_dinode_setTripleIndirect((inode)->dinode,val)
#define alfs_inode_setGroupstart(inode,val)	alfs_dinode_setGroupstart((inode)->dinode,val)
#define alfs_inode_setGroupsize(inode,val)	alfs_dinode_setGroupsize((inode)->dinode,val)
#define alfs_inode_setExtrafield1(inode,val)	alfs_dinode_setExtrafield1((inode)->dinode,val)
#define alfs_inode_setExtrafield2(inode,val)	alfs_dinode_setExtrafield2((inode)->dinode,val)

/* alfs_inode.c */

inode_t *alfs_inode_makeInode (block_num_t fsdev);
inode_t *alfs_inode_getInode (block_num_t inodeNum, block_num_t fsdev, int flags);
int alfs_inode_releaseInode (inode_t *inode, int flags);
void alfs_inode_writeInode (inode_t *inode, int flags);
void alfs_inode_truncInode (inode_t *inode, int length);
block_num_t alfs_inode_offsetToBlock (inode_t *inode, unsigned offset, int *allocate);

#endif  /* __ALFS_INODE_H__ */

