
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


#ifndef __CFFS_DINODE_H__
#define __CFFS_DINODE_H__

#ifndef KERNEL
#ifndef CFFSD_INTERNAL /* we want the "direct access" version if we're being
		included from cffsd.c */
#include "exos/conf.h"
#endif
#include "cffs_general.h"
#include "cffs_buffer.h"

#include <ubb/xn.h>

	/* GROK -- temporary */
#include <sys/stat.h>
#endif

#ifdef CFFS_PROTECTED
#include <xok/sys_ucall.h>
#ifdef CFFSD
#include <exos/cffs_prot.h>
#else /* CFFSD */
#include <xok/fsprot.h>
#endif /* CFFSD */
#endif /* CFFS_PROTECTED */

/* structure of on-disk inodes */

typedef struct dinode {
     unsigned int dinodeNum;	/* self-identification for checking and convenience */
     off_t length;		/* (64-bit field) : length of file in bytes */
     u_int16_t type;		/* do we hold data for a file or directory */
     int16 openCount;		/* number of opens */
     uid_t uid;			/* so progs know which cap to use */
     gid_t gid;
     nlink_t linkCount;		/* number of links */
     mode_t mask;               /* access permissions */
     int accTime;		/* time of last access */
     int modTime;		/* time of last modification */
     int creTime;		/* time of creation */
#define NUM_DIRECT 15		/* XXX -- this makes the dinode 128 bytes long */
     block_num_t directBlocks[NUM_DIRECT];	/* ptrs to on-disk data blocks */
#define NUM_INDIRECT 3
     block_num_t indirectBlocks[NUM_INDIRECT];	/* ptrs to on-disk indirect blocks */
     u_int32_t dirNum;		/* inode number of containing directory */
     block_num_t groupstart;
     int groupsize;
     uint memory_sanity;
} dinode_t;

#define CFFS_DINODE_MEMORYSANITY	0xd1d0de77
#define cffs_dinode_sanityOK(dinode)	((dinode)->memory_sanity == CFFS_DINODE_MEMORYSANITY)

#define cffs_dinode_da(dinode)		((da_t)(dinode)->dinodeNum * 128)
#define cffs_dinodeNum_da(dinode)	((da_t)(dinodeNum) * 128)

#define cffs_dinode_isDir(dinode)	((dinode)->type == S_IFDIR)
#define cffs_dinode_isRegFile(dinode)	((dinode)->type == S_IFREG)
#define cffs_dinode_isSymLink(dinode)	((dinode)->type == S_IFLNK)

#define cffs_dinode_getMask(dinode)		(dinode)->mask
#define cffs_dinode_getUid(dinode)		(dinode)->uid
#define cffs_dinode_getGid(dinode)		(dinode)->gid
#define cffs_dinode_getAccTime(dinode)		(dinode)->accTime
#define cffs_dinode_getModTime(dinode)		(dinode)->modTime
#define cffs_dinode_getCreTime(dinode)		(dinode)->creTime
#define cffs_dinode_getLength(dinode)		(dinode)->length
#define cffs_dinode_getType(dinode)		(dinode)->type
#define cffs_dinode_getLinkCount(dinode)	(dinode)->linkCount
#define cffs_dinode_getOpenCount(dinode)	(dinode)->openCount
#define cffs_dinode_getDirNum(dinode)		(dinode)->dirNum
#define cffs_dinode_getDirect(dinode,ptrno)	(dinode)->directBlocks[(ptrno)]
#define cffs_dinode_getIndirect(dinode,ptrno)	(dinode)->indirectBlocks[(ptrno)]
#define cffs_dinode_getGroupstart(dinode)	(dinode)->groupstart
#define cffs_dinode_getGroupsize(dinode)	(dinode)->groupsize
#define cffs_dinode_getExtrafield1(dinode)	(dinode)->extrafield1
#define cffs_dinode_getExtrafield2(dinode)	(dinode)->extrafield2

#ifndef CFFS_PROTECTED
#define cffs_dinode_setMask(dinode, val)	(dinode)->mask = val
#define cffs_dinode_setUid(dinode, val)		(dinode)->uid = val
#define cffs_dinode_setGid(dinode, val)		(dinode)->gid = val
#define cffs_dinode_setAccTime(dinode, val)	(dinode)->accTime = val
#define cffs_dinode_setModTime(dinode, val)	(dinode)->modTime = val
#define cffs_dinode_setCreTime(dinode, val)	(dinode)->creTime = val
#define cffs_dinode_setLength(dinode, val)	(dinode)->length = val
#define cffs_dinode_setType(dinode, val)	(dinode)->type = val
#define cffs_dinode_setLinkCount(dinode, val)	(dinode)->linkCount = val
#define cffs_dinode_setOpenCount(dinode, val)	(dinode)->openCount = val
#define cffs_dinode_setDirNum(dinode, val)	(dinode)->dirNum = val
#define cffs_dinode_setDirect(dinode,ptrno,val)	(dinode)->directBlocks[(ptrno)] = (val)
#define cffs_dinode_setIndirect(dinode,ptrno,val)	(dinode)->indirectBlocks[(ptrno)] = (val)
#define cffs_dinode_setGroupstart(dinode,val)	(dinode)->groupstart = val
#define cffs_dinode_setGroupsize(dinode,val)	(dinode)->groupsize = val
#define cffs_dinode_setMemorySanity(dinode,val)	(dinode)->memory_sanity = (val)
#define cffs_dinode_setExtrafield1(dinode,val)	(dinode)->extrafield1 = val
#define cffs_dinode_setExtrafield2(dinode,val)	(dinode)->extrafield2 = val

#else	/* CFFS_PROTECTED */
#ifdef CFFSD

#define cffs_dinode_setMask(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETMASK, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setUid(dinode, val)		{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETUID, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setGid(dinode, val)		{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETGID, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setAccTime(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETACCTIME, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setModTime(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETMODTIME, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setCreTime(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETCRETIME, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setLength(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETLENGTH, (dinode), 0, (u_int)(val), 0); assert(ret == 0);}
#define cffs_dinode_setType(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETTYPE, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setLinkCount(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETLINKCOUNT, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setOpenCount(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETOPENCOUNT, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setDirNum(dinode, val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETDIRNUM, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setDirect(dinode,ptrno,val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETDIRECT, (dinode), (ptrno), (val), 0); assert(ret == 0);}
#define cffs_dinode_setIndirect(dinode,ptrno,val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETINDIRECT, (dinode), (ptrno), (val), 0); assert(ret == 0);}
#define cffs_dinode_setGroupstart(dinode,val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETGROUPSTART, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setGroupsize(dinode,val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETGROUPSIZE, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setMemorySanity(dinode,val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETMEMORYSANITY, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setExtrafield1(dinode,val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETEXTRA1, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setExtrafield2(dinode,val)	{int ret = cffsd_fsupdate_dinode(CFFS_DINODE_SETEXTRA2, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}

#else /* CFFSD */

#define cffs_dinode_setMask(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETMASK, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setUid(dinode, val)		{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETUID, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setGid(dinode, val)		{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETGID, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setAccTime(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETACCTIME, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setModTime(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETMODTIME, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setCreTime(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETCRETIME, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setLength(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETLENGTH, (dinode), 0, (u_int)(val), 0); assert(ret == 0);}
#define cffs_dinode_setType(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETTYPE, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setLinkCount(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETLINKCOUNT, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setOpenCount(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETOPENCOUNT, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setDirNum(dinode, val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETDIRNUM, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setDirect(dinode,ptrno,val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETDIRECT, (dinode), (ptrno), (val), 0); assert(ret == 0);}
#define cffs_dinode_setIndirect(dinode,ptrno,val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETINDIRECT, (dinode), (ptrno), (val), 0); assert(ret == 0);}
#define cffs_dinode_setGroupstart(dinode,val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETGROUPSTART, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setGroupsize(dinode,val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETGROUPSIZE, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setMemorySanity(dinode,val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETMEMORYSANITY, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setExtrafield1(dinode,val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETEXTRA1, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}
#define cffs_dinode_setExtrafield2(dinode,val)	{int ret = sys_fsupdate_dinode(CFFS_DINODE_SETEXTRA2, (dinode), (u_int)(val), 0, 0); assert(ret == 0);}

#endif /* CFFSD */
#endif /* CFFS_PROTECTED */


#define CFFS_DINODE_SIZE		128

#define NUM_PTRS_IN_SINGLEINDIR (BLOCK_SIZE/sizeof (block_num_t))
#define NUM_PTRS_IN_DOUBLEINDIR (NUM_PTRS_IN_SINGLEINDIR * NUM_PTRS_IN_SINGLEINDIR)
#define NUM_PTRS_IN_TRIPLEINDIR (NUM_PTRS_IN_DOUBLEINDIR * NUM_PTRS_IN_SINGLEINDIR)

#define FIRST_PTR_IN_SINGLEINDIR (NUM_DIRECT)
#define FIRST_PTR_IN_DOUBLEINDIR (FIRST_PTR_IN_SINGLEINDIR + NUM_PTRS_IN_SINGLEINDIR)
#define FIRST_PTR_IN_TRIPLEINDIR (FIRST_PTR_IN_DOUBLEINDIR + NUM_PTRS_IN_DOUBLEINDIR)

#if 0
#define MAX_FILE_SIZE (((off_t)((NUM_DIRECT + NUM_PTRS_IN_SINGLEINDIR + NUM_PTRS_IN_DOUBLEINDIR + NUM_PTRS_IN_TRIPLEINDIR))*((off_t)BLOCK_SIZE)))
#else
#define MAX_FILE_SIZE	((u_int)0xFFFFFFFF)
#endif

#define cffs_dinode_blksPerIndirPtr(indirlevel) \
	(((indirlevel) == 2) ? NUM_PTRS_IN_SINGLEINDIR : (((indirlevel) == 3) ? NUM_PTRS_IN_DOUBLEINDIR : 1))

#define cffs_dinode_indirlevel(ptrno) \
	(((ptrno) >= FIRST_PTR_IN_TRIPLEINDIR) ? 3 : (((ptrno) >= FIRST_PTR_IN_DOUBLEINDIR) ? 2 : (((ptrno) >= FIRST_PTR_IN_SINGLEINDIR) ? 1 : 0)))

#define cffs_dinode_ptrinIndirBlock(ptrno,indirlevel) \
	((ptrno) - NUM_DIRECT - (((indirlevel) > 1) ? NUM_PTRS_IN_SINGLEINDIR : 0) - (((indirlevel) > 2) ? NUM_PTRS_IN_DOUBLEINDIR : 0))

#define cffs_dinode_firstPtr(indirlevel) \
	(((indirlevel) == 1) ? FIRST_PTR_IN_SINGLEINDIR : (((indirlevel) == 2) ? FIRST_PTR_IN_DOUBLEINDIR : FIRST_PTR_IN_TRIPLEINDIR))

#define fileOffsetToBlockOffset(off) ((off)%BLOCK_SIZE)

#ifndef CFFS_PROTECTED
#define cffs_dinode_clearBlockPointers(dinode, length) \
	{ int i; \
	  int numblocks = (length + BLOCK_SIZE -1) / BLOCK_SIZE; \
	  for (i=numblocks; i<NUM_DIRECT; i++) { \
	     (dinode)->directBlocks[i] = 0; \
	  } \
	  if (numblocks <= FIRST_PTR_IN_SINGLEINDIR) { \
	     (dinode)->indirectBlocks[0] = 0; \
	  } \
	  if (numblocks <= FIRST_PTR_IN_DOUBLEINDIR) { \
	     (dinode)->indirectBlocks[1] = 0; \
	  } \
	  if (numblocks <= FIRST_PTR_IN_TRIPLEINDIR) { \
	     (dinode)->indirectBlocks[2] = 0; \
	  } \
	}
#else /* CFFS_PROTECTED */
#ifdef CFFSD
#define cffs_dinode_clearBlockPointers(dinode, length) \
	cffsd_fsupdate_dinode(CFFS_DINODE_CLEARBLOCKPOINTERS, (dinode), 0, (u_int)(length));
#else /* CFFSD */
#define cffs_dinode_clearBlockPointers(dinode, length) \
	sys_fsupdate_dinode(CFFS_DINODE_CLEARBLOCKPOINTERS, (dinode), 0, (u_int)(length));
#endif /* CFFSD */
#endif /* CFFS_PROTECTED */

	/* GROK -- the 512 here should be made correct relative to BLOCK_SIZE and CFFS_EMBDIR_SECTOR_SIZE!! */
#define cffs_inode_blkno(inodenum)	(inodenum >> (2 + 3))

#define cffs_inode_sector(inodenum)	(inodenum >> 2)

	/* GROK -- same for the 1F here */
#define cffs_inode_offsetInBlock(inodenum)	(CFFS_DINODE_SIZE * (inodenum & 0x0000001F))

/* cffs_dinode.c prototypes */

dinode_t* cffs_dinode_create(dinode_t* ino, da_t da);
void cffs_dinode_initDInode (dinode_t *dinode, u_int dinodeNum, u_int type);
dinode_t *cffs_dinode_getDInode (block_num_t dinodeNum, block_num_t fsdev, int flags, struct buffer_t **dinodeBlockBufferP);
int cffs_dinode_releaseDInode (dinode_t *dinode, buffer_t *fs_buf, int flags, struct buffer_t *dinodeBlockBuffer);
void cffs_dinode_writeDInode (dinode_t *dinode, block_num_t fsdev, struct buffer_t *dinodeBlockBuffer);
void cffs_dinode_truncDInode (dinode_t *dinode, buffer_t *sb_buf,  off_t length);
block_num_t cffs_dinode_offsetToBlock (dinode_t *dinode, buffer_t *sb_buf, off_t offset, int *allocate, int *error);
int cffs_dinode_getNumContig (dinode_t *dinode, buffer_t *sb_buf, block_num_t blkno);
void cffs_dinode_transferBlock (dinode_t *dinode, buffer_t *sb_buf, block_num_t blkno);
void cffs_dinode_zeroPtr (dinode_t *dinode, buffer_t *sb_buf, int ptrno);

#endif /* __CFFS_DINODE_H__ */

