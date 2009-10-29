
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

#ifndef __ALFS_DINODE_H__
#define __ALFS_DINODE_H__

#include "alfs/alfs_general.h"

/* structure of on-disk inodes */

/* Things are really simple now. Limited file size (no double indirect blocks)
   and all we really do is keep a list of the blocks that form the file. */

/* inodes either point to file data or to directories */
#define INODE_DIRECTORY 0x00000000
#define INODE_FILE 0x00000001
#define INODE_SYMLINK 0x00000002

/* uids and gids only inform a process who owns something so that the
   process can use the proper capabilities to access the data. The
   gid and uid in no way actually implment any form of security. */

extern _uid_t uid;
extern _gid_t gid;

struct dinode_t {
     unsigned int dinodeNum;	/* self-identification for checking and convenience */
     unsigned int type;		/* do we hold data for a file or directory */
     length_t length;		/* length of file in bytes */
     int refCount;		/* number of links */
     _uid_t uid;			/* so progs know which cap to use */
     _gid_t gid;
     umask_t mask;              /* access permissions (only informational) */
     int accTime;		/* time of last access */
     int modTime;		/* time of last modification */
     int creTime;		/* time of creation */
#define NUM_DIRECT 15		/* XXX -- this makes the dinode 128 bytes long */
     block_num_t directBlocks[NUM_DIRECT];	/* block nums. of first data blocks */
#define NUM_SINGLE_INDIRECT (BLOCK_SIZE/sizeof (block_num_t))
     block_num_t singleIndirect;	/* point to block holding pointers */
     block_num_t doubleIndirect;	/* point to block holding pointers */
     block_num_t tripleIndirect;	/* point to block holding pointers */
     block_num_t groupstart;
     int groupsize;
     int extrafield1;
     int extrafield2;
};
typedef struct dinode_t dinode_t;

#define alfs_dinode_getMask(dinode)		(dinode)->mask
#define alfs_dinode_getUid(dinode)		(dinode)->uid
#define alfs_dinode_getGid(dinode)		(dinode)->gid
#define alfs_dinode_getAccTime(dinode)		(dinode)->accTime
#define alfs_dinode_getModTime(dinode)		(dinode)->modTime
#define alfs_dinode_getCreTime(dinode)		(dinode)->creTime
#define alfs_dinode_getLength(dinode)		(dinode)->length
#define alfs_dinode_getType(dinode)		(dinode)->type
#define alfs_dinode_getLinkCount(dinode)	(dinode)->refCount
#define alfs_dinode_getDirect(dinode,ptrno)	(dinode)->directBlocks[ptrno]
#define alfs_dinode_getSingleIndirect(dinode)	(dinode)->singleIndirect
#define alfs_dinode_getDoubleIndirect(dinode)	(dinode)->doubleIndirect
#define alfs_dinode_getTripleIndirect(dinode)	(dinode)->tripleIndirect
#define alfs_dinode_getGroupstart(dinode)	(dinode)->groupstart
#define alfs_dinode_getGroupsize(dinode)	(dinode)->groupsize
#define alfs_dinode_getExtrafield1(dinode)	(dinode)->extrafield1
#define alfs_dinode_getExtrafield2(dinode)	(dinode)->extrafield2
#define alfs_dinode_setMask(dinode, val)	(dinode)->mask = val
#define alfs_dinode_setUid(dinode, val)		(dinode)->uid = val
#define alfs_dinode_setGid(dinode, val)		(dinode)->gid = val
#define alfs_dinode_setAccTime(dinode, val)	(dinode)->accTime = val
#define alfs_dinode_setModTime(dinode, val)	(dinode)->modTime = val
#define alfs_dinode_setCreTime(dinode, val)	(dinode)->creTime = val
#define alfs_dinode_setLength(dinode, val)	(dinode)->length = val
#define alfs_dinode_setType(dinode, val)	(dinode)->type = val
#define alfs_dinode_setLinkCount(dinode, val)	(dinode)->refCount = val
#define alfs_dinode_setDirect(dinode,ptrno,val)	(dinode)->directBlocks[ptrno] = val
#define alfs_dinode_setSingleIndirect(dinode,val)	(dinode)->singleIndirect = val
#define alfs_dinode_setDoubleIndirect(dinode,val)	(dinode)->doubleIndirect = val
#define alfs_dinode_setTripleIndirect(dinode,val)	(dinode)->tripleIndirect = val
#define alfs_dinode_setGroupstart(dinode,val)	(dinode)->groupstart = val
#define alfs_dinode_setGroupsize(dinode,val)	(dinode)->groupsize = val
#define alfs_dinode_setExtrafield1(dinode,val)	(dinode)->extrafield1 = val
#define alfs_dinode_setExtrafield2(dinode,val)	(dinode)->extrafield2 = val

struct dinodeMapEntry_t {
    block_num_t dinodeBlockAddr;	/* address of the dinode block */
    unsigned int freeDInodes;		/* free map for dinodes in block */
};
typedef struct dinodeMapEntry_t dinodeMapEntry_t;

/* global dinode map information */
extern int alfs_dinodeMapEntries;
extern dinodeMapEntry_t alfs_dinodeMap[];

#define ALFS_DINODE_SIZE		128
#define ALFS_DINODES_PER_BLOCK	(BLOCK_SIZE / ALFS_DINODE_SIZE)
#define ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK	(BLOCK_SIZE / sizeof(dinodeMapEntry_t))
#define ALFS_DINODEMAPBLOCKS	1
#define ALFS_MAX_DINODECOUNT	(ALFS_DINODEMAPBLOCKS * ALFS_DINODES_PER_BLOCK * ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK)

#define MAX_FILE_SIZE ((NUM_DIRECT + NUM_SINGLE_INDIRECT)*BLOCK_SIZE)
#define fileOffsetToBlockOffset(off) ((off)%BLOCK_SIZE)

/* alfs_dinode.c prototypes */

int alfs_dinode_initmetablks ();
block_num_t alfs_dinode_initDisk (cap2_t fscap, block_num_t fsdev, char *buf, int blkno);
void alfs_dinode_mountDisk (cap2_t fscap, block_num_t fsdev, int blkno);
void alfs_dinode_unmountDisk (cap2_t fscap, block_num_t fsdev, int blkno);
dinode_t *alfs_dinode_makeDInode (block_num_t fsdev, int allocHint);
int alfs_dinode_deallocDInode (block_num_t fsdev, block_num_t dinodeNum);
dinode_t *alfs_dinode_getDInode (block_num_t dinodeNum, block_num_t fsdev, int flags);
int alfs_dinode_releaseDInode (dinode_t *dinode, block_num_t fsdev, int flags);
void alfs_dinode_writeDInode (dinode_t *dinode, block_num_t fsdev);
void alfs_dinode_truncDInode (dinode_t *dinode, block_num_t fsdev, int length);
block_num_t alfs_dinode_offsetToBlock (dinode_t *dinode, block_num_t fsdev, unsigned offset, int *allocate);

#endif /* __ALFS_INODE_H__ */

