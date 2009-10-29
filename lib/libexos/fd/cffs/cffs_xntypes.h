
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


#ifndef __CFFS_XNTYPES_H__
#define __CFFS_XNTYPES_H__

int specify_cffs_types(void);
#define CFFS_XN_SUPER_T			0
extern int cffs_super_t;
#define CFFS_XN_INODE_T			1
extern int cffs_inode_t;
#define CFFS_XN_INODE_DATA_T		2
extern int cffs_inode_data_t;
#define CFFS_XN_SINDIRECT_DATA_T	3
extern int cffs_sindirect_data_t;
#define CFFS_XN_DINDIRECT_DATA_T		4
extern int cffs_dindirect_data_t;
#define CFFS_XN_TINDIRECT_DATA_T	5
extern int cffs_tindirect_data_t;
#define CFFS_XN_INODE_DIR_T		6
extern int cffs_inode_dir_t;
#define CFFS_XN_SINDIRECT_DIR_T		7
extern int cffs_sindirect_dir_t;
#define CFFS_XN_DINDIRECT_DIR_T		8
extern int cffs_dindirect_dir_t;
#define CFFS_XN_TINDIRECT_DIR_T		9
extern int cffs_tindirect_dir_t;
#define CFFS_XN_DIR_T			10
extern int cffs_dir_t;
#define CFFS_XN_DATA_T			11
extern int cffs_data_t;
#define CFFS_XN_NUMTYPES		12

#include "cffs.h"

void cffs_fill_xntypes (buffer_t *superblockBuf);
void cffs_pull_xntypes (cffs_t *superblock);

struct xn_op*
cffs_inode_op(size_t slot, db_t from, db_t to, unsigned level, int dir);

struct xn_op*
cffs_indirect_op(size_t slot, db_t from, db_t to, unsigned level, int dir);


#endif	/* __CFFS_XNTYPES_H__ */

