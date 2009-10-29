
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

#ifndef __ALFS_ALLOC_H__
#define __ALFS_ALLOC_H__

#define ALFS_ALLOC_DATA		0x00000001
#define ALFS_ALLOC_DIRECT	0x00000002
#define ALFS_ALLOC_INDIRECT	0x00000004

#define ALFS_ALLOC_DATA_DIR	(ALFS_ALLOC_DATA | ALFS_ALLOC_DIRECT)
#define ALFS_ALLOC_DATA_INDIR	(ALFS_ALLOC_DATA | ALFS_ALLOC_INDIRECT)

extern int alfs_blockcount;

/* alfs_alloc.c prototypes */

int alfs_alloc_initmetablks (int size);
block_num_t alfs_alloc_initDisk (cap2_t fscap, block_num_t fsdev, int size, int initmetablks);
int alfs_alloc_uninitDisk (cap2_t fscap, block_num_t fsdev, block_num_t blkno, int initmetablks);
block_num_t alfs_alloc_mountDisk (cap2_t fscap, block_num_t fsdev, block_num_t blkno, int size);
block_num_t alfs_alloc_unmountDisk (cap2_t fscap, block_num_t fsdev, block_num_t blkno);
int alfs_alloc_allocBlock (uint fsdev, block_num_t block);
int alfs_alloc_deallocBlock (uint fsdev, block_num_t block);
block_num_t alfs_findFree (uint fsdev, int startsearch);
struct dinode_t;
block_num_t alfs_allocBlock (block_num_t, struct dinode_t *, int *, int, int, int);
int alfs_allocDInode (block_num_t fsdev, int allocHint);

#endif
