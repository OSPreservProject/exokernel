
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


/* fix a file system on a disk */

/***************************************************************************
  NOTES:
	This fsck will not properly deal will destroyed sectors.  To do this
        properly will require several things, including addition of a
        directory map.

****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>

#include <xok/sysinfo.h>

#include <exos/mallocs.h>	/* for __malloc, etc... */

#include "cffs_buffer.h"
#include "cffs.h"
#include "cffs_inode.h"
#include "cffs_embdir.h"
#include "cffs_alloc.h"
#include "cffs_path.h"

#include "cffs_proc.h"

#include "cffs_defaultcache.h"

#ifndef XN

/**************************************************************************
 *
 * Support for finding and dealing with duplicated block pointers (i.e.,
 * two inodes with pointers to the same block), which should never actually
 * be possible...
 *
 */

typedef struct blkdup {
   struct blkdup *next;
   u_int ownercnt1;
   u_int ownercnt2;
   block_num_t blkno;
} blkdup_t;

static blkdup_t *blkduplist = NULL;

static void cffs_blkdup_addtolist (block_num_t blkno)
{
   blkdup_t *dup = blkduplist;

   while ((dup) && (dup->blkno != blkno)) {
      dup = dup->next;
   }

   if (dup) {
      dup->ownercnt1++;
   } else {
      dup = (blkdup_t *) __malloc (sizeof(blkdup_t));
      assert (dup != NULL);
      dup->blkno = blkno;
      dup->ownercnt1 = 2;
      dup->ownercnt2 = 0;
      dup->next = blkduplist;
      blkduplist = dup;
   }
}

#if 0				/* not used */
static void cffs_blkdup_checkformatch (block_num_t blkno, inode_t *inode)
{
   blkdup_t *dup = blkduplist;

   while ((dup) && (dup->blkno != blkno)) {
      dup = dup->next;
   }

   if (dup) {
      dup->ownercnt2++;
      cffs_inode_truncInode (inode, 0);
   }
}
#endif

#if 0				/* not used */
static void cffs_blkdup_cleanlist ()
{
   blkdup_t *dup;

   while ((dup = blkduplist) != NULL) {
      blkduplist = dup->next;
      assert (dup->ownercnt1 == dup->ownercnt2);
      __free (dup);
   }
}
#endif

/**************************************************************************
 *
 * Support for tracking links to external dinodes (i.e., dinodes not embedded
 * in the same block) and directory back links.
 *
 */

typedef struct linkcnt {
   struct linkcnt *next;
   u_int dinodeNum;
   int isDir;
   u_int true_links;
   u_int directory_backlinks;
} linkcnt_t;

#define LINKCNT_HASHBUCKETS 1024

static linkcnt_t **linkcnts = NULL;

#define linkcnt_hash(dinodeNum)		((dinodeNum) % (LINKCNT_HASHBUCKETS-1))


static linkcnt_t *cffs_linkcnt_findnalloc (u_int dinodeNum, int alloc)
{
   linkcnt_t *tmp = linkcnts[linkcnt_hash(dinodeNum)];

   while (tmp) {
      if (tmp->dinodeNum == dinodeNum) {
         return (tmp);
      }
      tmp = tmp->next;
   }

   if (alloc) {
      tmp = (linkcnt_t *) __malloc (sizeof(linkcnt_t));
      tmp->dinodeNum = dinodeNum;
      tmp->isDir = 0;
      tmp->true_links = 0;
      tmp->directory_backlinks = 0;
      tmp->next = linkcnts[linkcnt_hash(dinodeNum)];
      linkcnts[linkcnt_hash(dinodeNum)] = tmp;
   } else {
      assert (tmp == NULL);
   }

   return (tmp);
}


static void cffs_linkcnt_setisDir (u_int dinodeNum)
{
   linkcnt_t *tmp = cffs_linkcnt_findnalloc (dinodeNum, 1);
   tmp->isDir = 1;
}


static void cffs_linkcnt_addtruelink (u_int dinodeNum)
{
   linkcnt_t *tmp = cffs_linkcnt_findnalloc (dinodeNum, 1);
   tmp->true_links++;
}


static u_int cffs_linkcnt_gettruelink (u_int dinodeNum)
{
   linkcnt_t *tmp = cffs_linkcnt_findnalloc (dinodeNum, 0);
   if (tmp) {
      return (tmp->true_links);
   }
   return (0);
}


static void cffs_linkcnt_addbacklink (u_int dinodeNum)
{
   linkcnt_t *tmp = cffs_linkcnt_findnalloc (dinodeNum, 1);
   tmp->directory_backlinks++;
}


static u_int cffs_linkcnt_getbacklink (u_int dinodeNum)
{
   linkcnt_t *tmp = cffs_linkcnt_findnalloc (dinodeNum, 0);
   if (tmp) {
      return (tmp->directory_backlinks);
   }
   return (0);
}

int fsck_realloc (buffer_t *sb_buf, dinode_t *dinode, void *ptrloc, block_num_t blkno) {
  int micropart;
  static unsigned char alloced_microparts[MAX_DISKS][1024] = {{0}}; /* XXX -- 1024!?!? */
  cffs_t *sb = (cffs_t *)sb_buf->buffer;

  micropart = blkno/MICROPART_SZ_IN_PAGES;
  if (!alloced_microparts[sb->fsdev][micropart]) {  
    cffs_allocMicropart (sb->fsdev, sb_buf, micropart);
    alloced_microparts[sb->fsdev][micropart] = 1;
  }

  return (cffs_alloc_protWrapper (CFFS_SETPTR_ALLOCATE, sb_buf, dinode, ptrloc, blkno));
}

static int cffs_fsck_setAllocationIndir (buffer_t *sb_buf, dinode_t *dinode, block_num_t indirblock, int indirlevel, block_num_t *ptrloc)
{
   buffer_t *indirectBlockBuffer;
   int *indirectBlock;
   int i;
   int ret = 0;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
   int error;

   if (indirblock == 0) {
      return 0;
   }

   indirectBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, dinode->dinodeNum, -indirblock, (BUFFER_READ | BUFFER_WITM), &error);
   if (indirectBlockBuffer == NULL)
     return -1;
   indirectBlock = (int *) indirectBlockBuffer->buffer;
   
   for (i=(NUM_PTRS_IN_SINGLEINDIR-1); i>=0; i--) {
      if (indirectBlock[i]) {
         if (indirlevel > 1) {                                                  
            if (cffs_fsck_setAllocationIndir (sb_buf, dinode, indirectBlock[i], (indirlevel-1), &indirectBlock[i]) == -1)
	      return -1;
         } else {
#ifdef CFFS_PROTECTED
	   ret = fsck_realloc (sb_buf, dinode, &indirectBlock[i], indirectBlock[i]);
#else
            ret = cffs_alloc_allocBlock (indirectBlock[i]);
#endif
            if (ret != 0) {
	      if (errno == EDQUOT)
		return -1;
	      printf ("failed re-allocation: blkno %d has multiple owners??\n", indirectBlock[i]);
	      printf ("BTW, second claimant is dinode %d (%x), indirect (type %x)\n", dinode->dinodeNum, dinode->dinodeNum, cffs_dinode_getType(dinode));
	      cffs_blkdup_addtolist (indirectBlock[i]);
            }
         }
      }
   }

   cffs_buffer_releaseBlock (indirectBlockBuffer, BUFFER_INVALID);
#ifdef CFFS_PROTECTED
   assert (*ptrloc == indirblock);
   ret = fsck_realloc (sb_buf, dinode, ptrloc, indirblock);
#else
   ret = cffs_alloc_allocBlock (indirblock);
#endif
   if (ret != 0) {
     if (errno == EDQUOT)
       return -1;
      printf ("failed re-allocation: blkno %d has multiple owners??\n", indirblock);
      printf ("BTW, second claimant is dinode %d (%x), indirect (type %x)\n", dinode->dinodeNum, dinode->dinodeNum, cffs_dinode_getType(dinode));
      cffs_blkdup_addtolist (indirblock);
   }
   return 0;
}


/* traverse blocks in dinode, setting them to be allocated */

static int cffs_fsck_setAllocationMap (buffer_t *sb_buf, dinode_t *dinode, block_num_t fsdev)
{
   int i;
   unsigned int blkno;
   int ret;

   for (i=0; i<NUM_DIRECT; i++) {
      if ((blkno = dinode->directBlocks[i])) {
#ifdef CFFS_PROTECTED
	ret = fsck_realloc (sb_buf, dinode, &dinode->directBlocks[i], blkno);
#else
         ret = cffs_alloc_allocBlock (blkno);
#endif
         if (ret != 0) {
            printf ("failed re-allocation: blkno %d has multiple owners??\n", blkno);
            printf ("BTW, second claimant is dinode %d (%x), direct block %d (type %x)\n", dinode->dinodeNum, dinode->dinodeNum, i, cffs_dinode_getType(dinode));
            cffs_blkdup_addtolist (blkno);
         }
      }
   }

   if (cffs_fsck_setAllocationIndir (sb_buf, dinode, dinode->indirectBlocks[0], 1, &dinode->indirectBlocks[0]) == -1) return -1;
   if (cffs_fsck_setAllocationIndir (sb_buf, dinode, dinode->indirectBlocks[1], 2, &dinode->indirectBlocks[1]) == -1) return -1;
   if (cffs_fsck_setAllocationIndir (sb_buf, dinode, dinode->indirectBlocks[2], 3, &dinode->indirectBlocks[2]) == -1) return -1;

   return 0;
}


/* walk thru the external inodes, resetting them based on the links already */
/* found and their current states */

static int cffs_fsck_setLinkCounts (buffer_t *sb_buf, inode_t *dirInode)
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   int dirBlockNumber = 0;
   int currPos = 0;
   int i, j;
   int linkcnt;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
   int error;

   while (currPos < cffs_inode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode->dinode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = ((cffs_inode_getLength(dirInode) - currPos) < BLOCK_SIZE) ? (cffs_inode_getLength(dirInode) - currPos) : BLOCK_SIZE;
      dirBlockBuffer = cffs_buffer_getBlock (dirInode->fsdev, sb->blk, -dirInode->inodeNum, physicalDirBlockNumber, BUFFER_READ, &error);
      if (!dirBlockBuffer)
	return -1;
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
/*
printf("length %d, buffer %x, block %x\n", dirBlockLength, dirBlockBuffer, dirBlock);
*/
      for (i=0; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
         for (j=1; j<(CFFS_EMBDIR_SECTOR_SIZE/CFFS_DINODE_SIZE); j++) {
            u_int dinodeNum = (((dirBlockBuffer->header.diskBlock*8) + (i/CFFS_EMBDIR_SECTOR_SIZE)) << 2) + j;
            dinode_t *dinode = (dinode_t *) (dirBlock + i + (j*CFFS_DINODE_SIZE));
	    if (dinode->dinodeNum != 0 && dinode->dinodeNum != dinodeNum) {
	      printf ("Bogus dinodeNum in dinode...skipping\n");
	      continue;
	    }
            if (dinode->dinodeNum) {
		/* walk dinode structure (plus indirect blocks) and set alloc */
               if (cffs_fsck_setAllocationMap (sb_buf, dinode, dirInode->fsdev) == -1)
		 return -1;
            }

		/* clear open count on inode (will be reclaimed later if 0) */
            cffs_dinode_setOpenCount (dinode, 0);

            linkcnt = cffs_linkcnt_gettruelink(dinodeNum);
            if (linkcnt == 0) {
               if (dinode->dinodeNum != 0) {
		   /* GROK - should probably link to lost+found */
                  printf ("Deleting live inode (%x) with no links (linkcount %d)\n", dinodeNum, cffs_dinode_getLinkCount(dinode));
                  cffs_dinode_setLinkCount(dinode, 0);
                  cffs_dinode_truncDInode (dinode, sb_buf, 0);
#ifdef CFFS_PROTECTED
#ifdef CFFSD
                  cffsd_fsupdate_dinode (CFFS_DINODE_DELAYEDFREE, dinode, 0,0, 0);
#else
                  sys_fsupdate_dinode (CFFS_DINODE_DELAYEDFREE, dinode, 0,0, 0);
#endif /* CFFSD */
#else
                  dinode->dinodeNum = 0;
#endif
               }
            } else if (cffs_dinode_getLinkCount(dinode) == 0) {
               printf ("Deleting inode (%x) with links (%d) but no link count\n", dinodeNum, linkcnt);
               if (dinode->dinodeNum) {
                  cffs_dinode_truncDInode (dinode, sb_buf, 0);
#ifdef CFFS_PROTECTED
#ifdef CFFSD
                  cffsd_fsupdate_dinode (CFFS_DINODE_DELAYEDFREE, dinode, 0,0, 0);
#else
                  sys_fsupdate_dinode (CFFS_DINODE_DELAYEDFREE, dinode, 0,0, 0);
#endif /* CFFSD */
#else
                  dinode->dinodeNum = 0;
#endif
               }
            } else {
	      if (dinode->dinodeNum == 0) {
		printf ("dinodeNum 0 unepextedly in setLinkCounts...skipping.\n");
		continue;
	      }
               if (cffs_dinode_isDir(dinode)) {
                  linkcnt += cffs_linkcnt_getbacklink(dinodeNum);
                  linkcnt++;	/* for . entry */
                  if (cffs_linkcnt_gettruelink(dinodeNum) != 1) {
                     printf ("Directory has multiple hard links (%d) -- not allowed\n", cffs_linkcnt_gettruelink(dinodeNum));
			/* GROK - fix me, by choosing one that .. does not point to! (what?) */
		     printf ("Skipping...\n");
		     continue;
                  }
               }
               if (cffs_dinode_getLinkCount(dinode) != linkcnt) {
                  printf ("External inode's (%x) link count doesn't match real link count (%d != %d)\n", dinode->dinodeNum, cffs_dinode_getLinkCount(dinode), linkcnt);
printf ("truelinks %d, backlinks %d\n", cffs_linkcnt_gettruelink(dinodeNum), cffs_linkcnt_getbacklink(dinodeNum));
                  cffs_dinode_setLinkCount (dinode, linkcnt);
               }
            }
         }
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, BUFFER_DIRTY);
      currPos += dirBlockLength;
   }

   return 0;
}


static char *dirname = NULL;


/* traverse a directory, resetting structures appropriately... */

static int  cffs_fsck_walkdirectory (buffer_t *sb_buf, inode_t *dirInode, char *dirname)
{
   char *dirBlock;
   int dirBlockLength;
   buffer_t *dirBlockBuffer;
   int dirBlockNumber = 0;
   embdirent_t *dirent;
   embdirent_t *prevdirent = NULL;
   int currPos = 0;
   int i, j;
   inode_t *inode;
   int dirlength = strlen(dirname);
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
   int error;
   int loop_count;

   cffs_linkcnt_setisDir (dirInode->inodeNum);

	/* bump backlink count on parent directory (for "..") */
   cffs_linkcnt_addbacklink (cffs_inode_getDirNum(dirInode));

   while (currPos < cffs_inode_getLength(dirInode)) {
      block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dirInode->dinode, NULL, dirBlockNumber * BLOCK_SIZE, NULL, NULL);
      assert (physicalDirBlockNumber);
      dirBlockLength = ((cffs_inode_getLength(dirInode) - currPos) < BLOCK_SIZE) ? (cffs_inode_getLength(dirInode) - currPos) : BLOCK_SIZE;
      dirBlockBuffer = cffs_buffer_getBlock (dirInode->fsdev, sb->blk, -dirInode->inodeNum, physicalDirBlockNumber, BUFFER_READ, &error);
      if (!dirBlockBuffer)
	return -1;
      dirBlockNumber++;
      dirBlock = dirBlockBuffer->buffer;
/*
printf("length %d, buffer %x, block %x\n", dirBlockLength, dirBlockBuffer, dirBlock);
*/
      loop_count = 0;
      for (i=0; i < dirBlockLength; i += CFFS_EMBDIR_SECTOR_SIZE) {
         for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
	   if (loop_count++ > 4000) {
	     kprintf ("breaking infinite loop in cffs_walk_directory...\n"); goto break_out;
	   }
            dirent = (embdirent_t *) (dirBlock + i + j);
            if (dirent->type == (char) 0) {
		/* dead directory entry -- if not start of sector, then */
		/*                         give space to previous entry */
               if ((prevdirent) && ((i % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) != 0)) {
#ifndef CFFS_PROTECTED
                  prevdirent->entryLen += dirent->entryLen;
                  if (((int)dirent - (int)dirBlockBuffer->buffer + dirent->entryLen) % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
                     embdirent_t *nextDirent = (embdirent_t *) ((char *)dirent + dirent->entryLen);
                     nextDirent->preventryLen = prevdirent->entryLen;
                  }
#else
#ifdef CFFSD 
                  cffsd_fsupdate_directory (CFFS_DIRECTORY_MERGEENTRY, prevdirent, dirent->entryLen, 0, 0, 0);
#else
                  sys_fsupdate_directory (CFFS_DIRECTORY_MERGEENTRY, prevdirent, dirent->entryLen, 0, 0, 0);
#endif /* CFFSD */
		  if (((u_int)prevdirent % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) && (prevdirent->preventryLen == 0)) {
		    printf ("Bogus prevdirent values after call to CFFS_DIRECTORY_MERGEENTRY...skipping.\n");
		    continue;
		  }
#endif
               } else {
                  prevdirent = dirent;
               }
               continue;	/* to avoid other stuff */

            } else if (cffs_inode_blkno(dirent->inodeNum) == dirBlockBuffer->header.diskBlock) {
               dinode_t *dinode = (dinode_t *) (cffs_inode_offsetInBlock(dirent->inodeNum) + dirBlockBuffer->buffer);
	       if (dinode->dinodeNum == 0) {
		 printf ("dinodeNum == 0...skipping\n");
		 continue;
	       } else {
		 inode = cffs_inode_makeInode (dirInode->fsdev, sb->blk, dinode->dinodeNum, dinode, dirBlockBuffer);
		 cffs_buffer_bumpRefCount (dirBlockBuffer);
	       }

            } else {

		/* external pointer -- deal with case that it doesn't actually exist */
               inode = cffs_inode_getInode (dirent->inodeNum, dirInode->fsdev, sb->blk, (BUFFER_READ|BUFFER_WITM));
               assert (inode);
            }

            if (cffs_inode_getLinkCount(inode) == 0) {
               printf ("External pointer to non-active inode (%x)\n", inode->inodeNum);
               printf ("Deleting directory entry: name %s%s\n", dirname, dirent->name);
               inode->dinode = NULL;
               cffs_inode_tossInode (inode);
               dirent->type = (char) 0;
               if ((prevdirent) && ((i % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) != 0)) {
#ifndef CFFS_PROTECTED
                  prevdirent->entryLen += dirent->entryLen;
                  if (((int)dirent - (int)dirBlockBuffer->buffer + dirent->entryLen) % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
                     embdirent_t *nextDirent = (embdirent_t *) ((char *)dirent + dirent->entryLen);
                     nextDirent->preventryLen = prevdirent->entryLen;
                  }
#else
#ifdef CFFSD
                  cffsd_fsupdate_directory (CFFS_DIRECTORY_MERGEENTRY, prevdirent, dirent->entryLen, 0, 0, 0);
#else
                  sys_fsupdate_directory (CFFS_DIRECTORY_MERGEENTRY, prevdirent, dirent->entryLen, 0, 0, 0);
#endif /* CFFSD */
		  if (((u_int)prevdirent % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) && (prevdirent->preventryLen == 0)) {
		    printf ("Bogus dirent after call to CFFS_EMBDIR_SECTOR_SPACEFORNAMES...skipping.\n");
		    continue;
		  }
#endif
               } else {
                  prevdirent = dirent;
               }
               continue;
            }

            cffs_linkcnt_addtruelink (inode->inodeNum);

		/* recurse depth-first on directories (also update link cnt) */
            if (cffs_inode_isDir(inode)) {
               sprintf (dirname, "%s%s/", dirname, dirent->name);
               if (cffs_fsck_walkdirectory (sb_buf, inode, dirname) == -1)
		 return -1;
               dirname[dirlength] = (char) 0;
            }

            if (cffs_inode_blkno(dirent->inodeNum) == dirBlockBuffer->header.diskBlock) {
               inode->dinode = NULL;
               cffs_inode_tossInode (inode);
            } else {
               cffs_inode_releaseInode (inode, BUFFER_DIRTY);
            }
         
            prevdirent = dirent;
         }
      }
      cffs_buffer_releaseBlock (dirBlockBuffer, BUFFER_DIRTY);
      currPos += dirBlockLength;
   }
   break_out:
   dirname[dirlength] = (char) 0;
   
   return 0;
}


static int cffs_do_fsck (buffer_t *sb_buf, int devno, int nukeAllocMap)
{
   inode_t *rootInode;
   int i;
   cffs_t *cffs = (cffs_t *)sb_buf->buffer;

	/* do fake mount, re-init'ing free map and reading other structures */
   cffs_inode_initInodeCache ();
   if (nukeAllocMap) {
     cffs_alloc_initDisk (sb_buf, cffs->size, cffs->allocMap);
   }

	/* allocate a structure for both link counts on externalized inodes */
	/* and directory back link counts.                                  */

   linkcnts = (linkcnt_t **) __malloc (LINKCNT_HASHBUCKETS * sizeof(linkcnt_t *));
   for (i=0; i<LINKCNT_HASHBUCKETS; i++) {
      linkcnts[i] = 0;
   }

	/* get root inode to start making progress */
   rootInode = cffs_inode_getInode (cffs->rootDInodeNum, devno, cffs->blk, (BUFFER_READ|BUFFER_WITM));
   assert (rootInode != NULL);

	/* Give one to the root directory! */
   assert (cffs_inode_isDir (rootInode));
   cffs_linkcnt_addtruelink (rootInode->inodeNum);

	/* walk directory hierarchy in depth first manner */
   dirname = __malloc (4096);
   assert (dirname != 0);
   sprintf (dirname, "/");
   if (cffs_fsck_walkdirectory (sb_buf, rootInode, dirname) == -1)
     return -1;
   __free (dirname);

   for (i=0; i<LINKCNT_HASHBUCKETS; i++) {
      linkcnt_t *tmp = linkcnts[i];
      while (tmp) {
         if (tmp->isDir) {
            inode_t *inode = cffs_inode_getInode (tmp->dinodeNum, rootInode->fsdev, cffs->blk, (BUFFER_READ|BUFFER_WITM));
            if (cffs_fsck_setLinkCounts (sb_buf, inode) == -1)
	      return -1;
            cffs_inode_releaseInode (inode, BUFFER_DIRTY);
         }
         tmp = tmp->next;
      }
   }

	/* walk dinode structure (plus indirect blocks) and set alloc */
   if (cffs_fsck_setAllocationMap (sb_buf, rootInode->dinode, rootInode->fsdev) == -1)
     return -1;

	/* clear open count on inode (will be reclaimed later if 0) */
   cffs_inode_setOpenCount (rootInode, 0);

   cffs_inode_setLinkCount (rootInode, (2 + cffs_linkcnt_getbacklink(rootInode->inodeNum)));
	/* GROK -- if we actually care, we need to free all linkcnt_t's too */
   __free (linkcnts);

	/* Re-scan for duplicates, if any found... */
   if (blkduplist != NULL) {
      printf ("Not yet dealing with duplicate block pointers\n");
   }

	/* release root inode */
   cffs_inode_releaseInode (rootInode, BUFFER_WRITE);

	/* do a real unmount, writing out the various structures */
   cffs_inode_shutdownInodeCache ();

   return 0;
}


int cffs_fsck (u_int devno, u_int sb, int force, int nukeAllocMap)
{
  buffer_t *superblockBuf;
  cffs_t *superblock;

  if (devno >= __sysinfo.si_ndisks) {
    errno = EINVAL;
    return -1;
  }

  if (global_ftable->mounted_disks[devno] == 1) {
      errno = EBUSY;
      return (-1);
  }

  ENTERCRITICAL

  CFFS_CHECK_SETUP (devno);

  superblockBuf = cffs_get_superblock (devno, sb, BUFFER_WITM);
  if (!superblockBuf) {
    errno = EINVAL;
    RETURNCRITICAL (-1);
  }
  superblock = (cffs_t *) superblockBuf->buffer;

  if (strcmp (superblock->fsname, CFFS_FSNAME) != 0) {
    cffs_buffer_releaseBlock (superblockBuf, 0);
    cffs_cache_shutdown ();
    errno = EINVAL;
    RETURNCRITICAL (-1);
  }

  if (superblock->fsdev != devno) {
    cffs_buffer_releaseBlock (superblockBuf, 0);
    cffs_cache_shutdown ();
    errno = EINVAL;
    RETURNCRITICAL (-1);
  }

  if ((superblock->dirty == 0) && !force) {
    cffs_buffer_releaseBlock (superblockBuf, 0);
    cffs_cache_shutdown ();
    RETURNCRITICAL (0);
  }

  /*
printf ("size %d, cap %x, blkno %d, rootINO %d (%x)\n", cffs->size, cffs->fscap, blkno, cffs->rootDInodeNum, cffs->rootDInodeNum);
*/

  if (cffs_do_fsck (superblockBuf, devno, nukeAllocMap) == -1) {
    errno = EINVAL;
    RETURNCRITICAL (-1);
  }

  /* mark the superblock block itself as allocated */
  if (sb != CFFS_SUPERBLKNO) {
    if (cffs_alloc_protWrapper (CFFS_SETPTR_EXTERNALALLOC, superblockBuf, NULL, 
				NULL, sb) != 0) {
      errno = EINVAL;
      RETURNCRITICAL (-1);
    }
  }

  cffs_superblock_setDirty (superblockBuf, 0);

  cffs_buffer_releaseBlock (superblockBuf, BUFFER_WRITE);
  cffs_cache_shutdown ();
  RETURNCRITICAL (0);
}

#else /* XN */

int cffs_fsck (u_int devno, int force)
{
  assert (0);
  return (0);
}

#endif /* XN */

