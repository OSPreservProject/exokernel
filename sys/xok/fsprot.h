
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


#ifndef __CFFS_PROT_H__
#define __CFFS_PROT_H__

#define CFFS_DINODE_SETMASK	1
#define CFFS_DINODE_SETUID	2
#define CFFS_DINODE_SETGID	3
#define CFFS_DINODE_SETACCTIME	4
#define CFFS_DINODE_SETMODTIME	5
#define CFFS_DINODE_SETCRETIME	6
#define CFFS_DINODE_SETLENGTH	7
#define CFFS_DINODE_SETTYPE	8
#define CFFS_DINODE_SETLINKCOUNT	9
#define CFFS_DINODE_SETOPENCOUNT	10
#define CFFS_DINODE_SETDIRNUM	11
#define CFFS_DINODE_SETDIRECT	12
#define CFFS_DINODE_SETINDIRECT	13
#define CFFS_DINODE_SETGROUPSTART	14
#define CFFS_DINODE_SETGROUPSIZE	15
#define CFFS_DINODE_SETEXTRA1	16
#define CFFS_DINODE_SETEXTRA2	17
#define CFFS_DINODE_CLEARBLOCKPOINTERS	18
#define CFFS_DINODE_DELAYEDFREE	19
#define CFFS_DINODE_SETMEMORYSANITY	20
#define CFFS_DINODE_INITDINODE	21

#define CFFS_DINODE_INITINDIRBLOCK	25

#define CFFS_SUPERBLOCK_SETROOTDINODENUM	30
#define CFFS_SUPERBLOCK_SETDIRTY	31
#define CFFS_SUPERBLOCK_SETFSNAME	32
#define CFFS_SUPERBLOCK_SETFSDEV	33
#define CFFS_SUPERBLOCK_SETALLOCMAP	34
#define CFFS_SUPERBLOCK_SETNUMBLOCKS	35
#define CFFS_SUPERBLOCK_SETSIZE		36
#define CFFS_SUPERBLOCK_SETNUMALLOCED	37
#define CFFS_SUPERBLOCK_SETXNTYPE	38
#define CFFS_SUPERBLOCK_INIT		39

#define CFFS_DIRECTORY_SPLITENTRY	40
#define CFFS_DIRECTORY_SETNAME	41
#define CFFS_DIRECTORY_MERGEENTRY	42
#define CFFS_DIRECTORY_SHIFTENTRY	43
#define CFFS_DIRECTORY_SETINODENUM	44
#define CFFS_DIRECTORY_SETTYPE	45
#define CFFS_DIRECTORY_INITDIRBLOCK	46

#define CFFS_DIRECTORY_RENAMEDIR	47

#define	CFFS_SETPTR_ALLOCATE	50
#define	CFFS_SETPTR_DEALLOCATE	51
#define	CFFS_SETPTR_REPLACE	52
#define	CFFS_SETPTR_NULLIFY	53

#endif /* __CFFS_PROT_H__ */

