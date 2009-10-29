
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

#ifndef __BUFFER_TAB_H__
#define __BUFFER_TAB_H__

#include "cffs_general.h"

/* structure used by the buffer_tab routines */

struct bufferHeader_t {
     struct bufferHeader_t *next, *prev;
     struct bufferHeader_t *nextFree, *prevFree;
     int inodeNum;
     int block;
     block_num_t dev;
     block_num_t diskBlock;
};
typedef struct bufferHeader_t bufferHeader_t;

typedef void bufferCache_t;

/* flags passed to findEntry */

#define FIND_VALID 1
#define FIND_EMPTY 2
#define FIND_ALLOCATE 3

/* buffer_tab.c prototypes */

bufferHeader_t *cffs_buffertab_findEntry (bufferCache_t *, block_num_t, int, int, int);
bufferHeader_t *cffs_buffertab_getEmptyEntry (bufferCache_t *, block_num_t, int, int);
int cffs_buffertab_revokeFreeEntries (bufferCache_t *bufferCache, int cnt);
int cffs_buffertab_renameEntry (bufferCache_t *, bufferHeader_t *, block_num_t, int, int);
void cffs_buffertab_markFree (bufferCache_t *, bufferHeader_t *);
void cffs_buffertab_markInvalid (bufferCache_t *, bufferHeader_t *);
bufferHeader_t *cffs_buffertab_findDiskBlock (bufferCache_t *bufferCache, block_num_t dev, block_num_t diskBlock);
void cffs_buffertab_affectFile (bufferCache_t *bufferCache, block_num_t dev, int inodeNum, void (*action)(bufferCache_t *,bufferHeader_t *,int), int flags, int notneg);
void cffs_buffertab_affectDev (bufferCache_t *bufferCache, block_num_t dev, void (*action)(bufferCache_t *,bufferHeader_t *,int), int flags);
void cffs_buffertab_affectAll (bufferCache_t *bufferCache, void (*action)(bufferCache_t *,bufferHeader_t *,int), int flags);
bufferCache_t *initBufferTab (int queues, int max, bufferHeader_t *(*new)(void *), int (*dump)(void *));
void shutdownBufferTab (bufferCache_t *);
void cffs_buffertab_printContents (bufferCache_t *bufferCache, void (*print)(bufferHeader_t *));

#endif /* __BUFFER_TAB_H__ */

