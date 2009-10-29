
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


#ifndef __WEB_FS_H__
#define __WEB_FS_H__

#include <alfs/buffer_tab.h>
#include <xok/disk.h>
#include <exos/bufcache.h>

/* buffer get flags */

#define BUFFER_GET		0x00000000
#define BUFFER_READ		0x00000001
#define BUFFER_WITM		0x00000002
#define BUFFER_ALLOCMEM		0x00000004
#define BUFFER_ALLOCSTORE	0x00000008

/* buffer release flags */

#define BUFFER_DIRTY	0x00000001
#define BUFFER_WRITE	0x00000002
#define BUFFER_ASYNC	0x00000004
#define BUFFER_INVALID	0x00000008

/* buffer states */

#define BUFFER_CLEAN	0x00000000
#define BUFFER_DIRTY	0x00000001
#define BUFFER_READING	0x00000002
#define BUFFER_WRITING	0x00000004
#define BUFFER_INVALID	0x00000008
#define BUFFER_RDONLY	0x00000010


/* the general buffer structure used by the file system code */

struct buffer_t {
   bufferHeader_t header;	/* used by buffer_tab routines */
   struct bc_entry *bc_entry;
   struct buf buf;		/* used to initiate/poll disk I/O */
   int flags;			/* current state of the buffer */
   int inUse;			/* number of people using this block */
   char *buffer;		/* disk block buffer */
};
typedef struct buffer_t buffer_t;


/* structure of on-disk inodes */

/* Things are really simple now. Limited file size (no double indirect blocks)
   and all we really do is keep a list of the blocks that form the file. */

/* inodes either point to file data or to directories */
#define INODE_DIRECTORY 0
#define INODE_FILE 1

/* uids and gids only inform a process who owns something so that the
   process can use the proper capabilities to access the data. The
   gid and uid in no way actually implment any form of security. */

extern _uid_t uid;
extern _gid_t gid;

struct dinode_t {
     unsigned int dinodeNum;    /* self-identification for checking and convenience */
     unsigned int type;         /* do we hold data for a file or directory */
     length_t length;           /* length of file in bytes */
     int refCount;              /* number of links */
     _uid_t uid;                        /* so progs know which cap to use */
     _gid_t gid;
     umask_t mask;              /* access permissions (only informational) */
     int accTime;		/* time of last access */
     int creTime;
     int modTime;		/* time of last modification */
#define NUM_DIRECT 15           /* XXX -- this makes the dinode 2048 bytes long */
     block_num_t directBlocks[NUM_DIRECT];      /* block nums. of first data blocks */
#define NUM_SINGLE_INDIRECT (BLOCK_SIZE/sizeof (block_num_t))
     block_num_t singleIndirect;        /* point to block holding pointers */
     block_num_t doubleIndirect;        /* point to block holding pointers */
     block_num_t tripleIndirect;        /* point to block holding pointers */
     block_num_t groupstart;
     int groupsize;
     int headerlen;		/* HTTP header for file */
     int extrareadlen;		/* HTTP header for file */
};
typedef struct dinode_t dinode_t;


/* globally visible web_fs.c values */

extern block_num_t alfs_FSdev;
extern dinode_t *alfs_currentRootInode;

extern buffer_t *web_fs_waitee;

extern int web_fs_diskreqs_outstanding;

/* web_fs.c prototypes */

buffer_t *web_fs_buffer_getBlock (block_num_t dev, block_num_t inodeNum, int block, int flags);
void web_fs_buffer_releaseBlock (buffer_t *buffer, int flags);
int web_fs_buffer_affectBlock (block_num_t dev, block_num_t inodeNum, int block, int flags);
void web_fs_buffer_affectFile (block_num_t dev, block_num_t inodeNum, int flags, int notneg);
void web_fs_buffer_affectDev (block_num_t dev, int flags);
void web_fs_buffer_affectAll (int flags);
void web_fs_buffer_shutdownBufferCache ();
void web_fs_buffer_printBlock (bufferHeader_t *bufHead);
void web_fs_buffer_printCache ();
void web_fs_dinode_mountDisk ();
dinode_t *web_fs_getDInode (block_num_t dinodeNum, block_num_t fsdev, int flags);
int web_fs_releaseDInode (dinode_t *dinode, block_num_t fsdev, int flags);
block_num_t web_fs_offsetToBlock (dinode_t *dinode, block_num_t fsdev, unsigned offset, int flags);
void mountFS (char *devname);
char *web_fs_path_getNextPathComponent (char *path, int *compLen);
dinode_t * web_fs_translateNameToInode (char *path, block_num_t fsdev, dinode_t *startInode, int flags);
int web_fs_lookupname (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen, int flags);
int web_fs_isdone (buffer_t *waitee);

#endif  /* __WEB_FS_H__ */

