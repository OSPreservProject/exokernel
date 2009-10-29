
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

#ifndef __ALFS_BUFFER_H__
#define __ALFS_BUFFER_H__

#include "alfs/alfs_general.h"
#include "alfs/buffer_tab.h"

#include <xok/buf.h>

/* GROK - temporary? */

extern bufferCache_t *alfs_localCache;

/* buffer get flags */

#define BUFFER_GET		0x00000000
#define BUFFER_READ		0x00000001
#define BUFFER_WITM		0x00000002
#define BUFFER_ALLOCMEM		0x00000004
#define BUFFER_ALLOCSTORE	0x00000008
#define BUFFER_ASYNC		0x00000010
#define BUFFER_HITONLY		0x00000020
#define BUFFER_MISSONLY		0x00000040

/* buffer release flags */

#define BUFFER_DIRTY    0x00000080
#define BUFFER_WRITE    0x00000100
#define BUFFER_INVALID  0x00000200
/* #define BUFFER_ASYNC         0x00000010 */   /* match above */
/* #define BUFFER_HITONLY       0x00000020 */   /* match above */
/* #define BUFFER_MISSONLY      0x00000040 */   /* match above */

/* additional buffer state flags */

#define BUFFER_RDONLY   0x00001000
#define BUFFER_READING  0x00002000
#define BUFFER_WRITING  0x00004000

/* the general buffer structure used by the file system code */

struct buffer_t {
     bufferHeader_t header;     /* used by buffer_tab routines */
     struct buf buf;		/* used to initiate/poll disk I/O */
     unsigned int flags;	/* state of the buffer */
     int inUse;                 /* number of people using this block */
     char *buffer;              /* disk block buffer */
     struct bc_entry *bc_entry; /* a pointer to the corresponding bc entry */
};
typedef struct buffer_t buffer_t;

/* alfs_buffer.c prototypes */

void alfs_buffer_initBufferCache (unsigned int numQueues, int maxBufferCount, buffer_t *(*handleMiss)(block_num_t,block_num_t,int,int), void (*flushEntry)(buffer_t *), void (*writeBack)(buffer_t *,int));
buffer_t *alfs_buffer_getBlock (block_num_t dev, block_num_t inodeNum, int block, int flags);
void alfs_buffer_releaseBlock (buffer_t *buffer, int flags);
int alfs_buffer_affectBlock (block_num_t dev, block_num_t inodeNum, int block, int flags);
void alfs_buffer_affectFile (block_num_t dev, block_num_t inodeNum, int flags, int notneg);
void alfs_buffer_affectDev (block_num_t dev, int flags);
void alfs_buffer_affectAll (int flags);
void alfs_buffer_shutdownBufferCache ();
void alfs_buffer_printBlock (bufferHeader_t *bufHead);
void alfs_buffer_printCache ();

#endif /* __ALFS_BUFFER_H__ */

