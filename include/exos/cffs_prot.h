/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

#ifndef __CFFS_PROT_H__
#define __CFFS_PROT_H__

/* parameter blocks passed in on each call to cffsd */

struct bc_ref {
  u32 br_dev;
  u32 br_blk;
  u32 br_off;
};

struct cffsd_fsupdate_dinode_arg {
  struct bc_ref dinode;
  uint param1, param2, param3;
};

struct cffsd_fsupdate_superblock_arg {
  struct bc_ref superblock;
  uint param1, param2;
};

struct cffsd_fsupdate_directory_arg {
  struct bc_ref dirent;
  uint param2;
  struct bc_ref ref_param3, ref_param4;
  uint uint_param3, uint_param4;
  uint param5;
};

struct cffsd_fsupdate_renamedir_arg {
};

struct cffsd_fsupdate_setptr_arg {
  struct bc_ref sb;
  struct bc_ref dinode;
  struct bc_ref allocmap;
  struct bc_ref ptrloc;
  uint val;
};

#define CFFSD_MAX_INITALLOC_SZ 256
  
struct cffsd_fsupdate_initAllocMap_arg {
  struct bc_ref allocmap[CFFSD_MAX_INITALLOC_SZ];
  u_int count;
  int deadcnt;
  u_int part;
};

#define CFFSD_MAX_ALLOCMICROPART_SZ 4
#define MICROPART_SZ_IN_PAGES ((32*1024*1024)/4096)
#define FREEMAP_PAGES_PER_MICROPART (MICROPART_SZ_IN_PAGES/BLOCK_SIZE)

struct cffsd_fsupdate_allocMicropart_arg {
  struct bc_ref allocmap[CFFSD_MAX_ALLOCMICROPART_SZ];
  int count;
  u_int part;
  u_int micropart;
};

/* major commands */

#define CFFSD_FSUPDATE_SUPERBLOCK 1
#define CFFSD_FSUPDATE_DINODE 2
#define CFFSD_FSUPDATE_DIRECTORY 3
#define CFFSD_FSUPDATE_SETPTR 4
#define CFFSD_FSUPDATE_RENAME 5
#define CFFSD_FSUPDATE_INITALLOCMAP 6
#define CFFSD_FSUPDATE_ALLOCMICROPART 7
#define CFFSD_BOOT 8

/* minor subcommands */

#define CFFS_DINODE_SETMASK	        100
#define CFFS_DINODE_SETUID	        101
#define CFFS_DINODE_SETGID	        102
#define CFFS_DINODE_SETACCTIME	        103
#define CFFS_DINODE_SETMODTIME	        104
#define CFFS_DINODE_SETCRETIME	        105
#define CFFS_DINODE_SETLENGTH	        106
#define CFFS_DINODE_SETTYPE	        107
#define CFFS_DINODE_SETLINKCOUNT	108
#define CFFS_DINODE_SETOPENCOUNT	109
#define CFFS_DINODE_SETDIRNUM	        110
#define CFFS_DINODE_SETDIRECT	        111
#define CFFS_DINODE_SETINDIRECT	        112
#define CFFS_DINODE_SETGROUPSTART	113
#define CFFS_DINODE_SETGROUPSIZE	114
#define CFFS_DINODE_SETEXTRA1	        115
#define CFFS_DINODE_SETEXTRA2	        116
#define CFFS_DINODE_CLEARBLOCKPOINTERS	117
#define CFFS_DINODE_DELAYEDFREE	        118
#define CFFS_DINODE_SETMEMORYSANITY	119
#define CFFS_DINODE_INITDINODE	        120
#define CFFS_DINODE_INITINDIRBLOCK	121

#define CFFS_SUPERBLOCK_SETROOTDINODENUM      200
#define CFFS_SUPERBLOCK_SETDIRTY	201
#define CFFS_SUPERBLOCK_SETFSNAME	202
#define CFFS_SUPERBLOCK_SETFSDEV	203
#define CFFS_SUPERBLOCK_SETALLOCMAP	204
#define CFFS_SUPERBLOCK_SETNUMBLOCKS	205
#define CFFS_SUPERBLOCK_SETSIZE		206
#define CFFS_SUPERBLOCK_SETNUMALLOCED	207
#define CFFS_SUPERBLOCK_SETXNTYPE	208
#define CFFS_SUPERBLOCK_INIT		209
#define CFFS_SUPERBLOCK_SETBLK          210
#define CFFS_SUPERBLOCK_SETQUOTA        211
#define CFFS_SUPERBLOCK_DELETE          212

#define CFFS_DIRECTORY_SPLITENTRY	300
#define CFFS_DIRECTORY_SETNAME	        301
#define CFFS_DIRECTORY_MERGEENTRY	302
#define CFFS_DIRECTORY_SHIFTENTRY	303
#define CFFS_DIRECTORY_SETINODENUM	304
#define CFFS_DIRECTORY_SETTYPE	        305
#define CFFS_DIRECTORY_INITDIRBLOCK	306
#define CFFS_DIRECTORY_RENAMEDIR	307

#define	CFFS_SETPTR_ALLOCATE	        400
#define	CFFS_SETPTR_DEALLOCATE	        401
#define	CFFS_SETPTR_REPLACE	        402
#define	CFFS_SETPTR_NULLIFY	        403
#define CFFS_SETPTR_EXTERNALALLOC       404
#define CFFS_SETPTR_EXTERNALFREE        405

struct superblock;
struct dinode;
struct embdirent;
struct cffs_t;

#include <exos/nameserv.h>
#include <fd/proc.h>

#include <assert.h>

int cffsd_fsupdate_superblock (int action, 
			       struct superblock *superblock, 
			       u_int val1,
			       u_int val2);
int cffsd_fsupdate_dinode (int action,
			    struct dinode *dinode,
			    u_int val1,
			    u_int val2,
			    u_int val3);  

int cffsd_fsupdate_directory (int action,
			      struct embdirent *dirent,
			      u_int val2,
			      u_int val3,
			      u_int val4,
			      u_int val5);

int cffsd_fsupdate_directory3 (int action,
			       struct embdirent *dirent,
			       u_int val2,
			       u_int val3,
			       u_int val4,
			       u_int val5);

int cffsd_fsupdate_directory4 (int action,
			       struct embdirent *dirent,
			       u_int val2,
			       u_int val3,
			       u_int val4,
			       u_int val5);

int cffsd_fsupdate_directory34 (int action,
				struct embdirent *dirent,
				u_int val2,
				u_int val3,
				u_int val4,
				u_int val5);

int cffsd_fsupdate_setptr (int action, 
			   void *sb,
			   struct dinode *dinode, 
			   char *allocmap, 
			   u_int *ptrloc, 
			   u_int val);

int cffsd_fsupdate_renamedir (int action,
			      struct embdirent *to,
			      struct embdirent *from,
			      struct dinode *dinode,
			      struct dinode *parentTo,
			      struct dinode *parentFrom,
			      struct dinode *dying);

int cffsd_fsupdate_initAllocMap (char *map[],
				 u_int count,
				 u_int val,
				 u_int dev);

int cffsd_fsupdate_allocMicropart (char *map[], 
				   u_int count,
				   u_int dev,
				   u_int micropart);

int cffsd_boot (u_int part);

static inline unsigned int cffsd_get_eid () {
  static int ret = 0;

  if (!ret) {
    ret = nameserver_lookup (NS_NM_CFFSD);
  }
  return ret;
}

#endif /* __CFFS_PROT_H__ */

