
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

#include <xok/types.h>
#include <xok/kerrno.h>
#include <xok/env.h>
#include <xok/sys_ucall.h>

#include <exos/ubc.h>
#include <exos/cffs_prot.h>
#include <exos/cap.h>
#include <exos/bufcache.h>
#include <exos/nameserv.h>
#include <exos/ipcdemux.h>
#include <exos/critical.h>

#include <assert.h>
#include <sys/stat.h>

#define FSID 17

struct buffer_t;

#undef CFFS_PROTECTED

#include "fd/cffs/cffs_general.h"
#define CFFSD_INTERNAL			/* cffs_dinode checks for this symbol */
#include "fd/cffs/cffs_dinode.h"
#undef CFFSD_INTERNAL
#include "fd/cffs/cffs.h"
#include "fd/cffs/cffs_embdir.h"
#include "fd/cffs/cffs_alloc.h"

int debug_request = -1;
int slap = 0;

static void *translate_address (struct bc_ref *r) {
  void *ptr;

  ptr = exos_bufcache_map (NULL, r->br_dev, r->br_blk, 1);
  if (!ptr) {
    kprintf ("cffsd (translate_address): could not map bc entry dev %u blk %u off %u. debug_request = %d\n",
	    r->br_dev, r->br_blk, r->br_off, debug_request);
    slap = 1;
    return NULL;
  }

  ptr = (char *)ptr + r->br_off;
  return ptr;
}

int cffsd_copyin (uint eid, uint src, uint dst, uint len) {
  int ret;

  ret = sys_vcopyin (eid, src, (uint *)dst, len);
  if (ret < 0) {
    printf ("cffsd (cffsd_copyin): failed with ret = %d\n", ret);
    return -1;
  }
  return 0;
}

// int cffsd_fsupdate_dinode (u_int sn, int action, struct dinode *d, u_int param1, u_int param2, u_int param3)
int serv_fsupdate_dinode (u_int client_eid, int action, struct cffsd_fsupdate_dinode_arg *arg)
{
   int i;
   char *indirblock;
   struct dinode *d;
   u_int param1 = arg->param1;
   u_int param2 = arg->param2;
   u_int param3 = arg->param3;

   dinode_t *dinode;

   debug_request = 1;

   dinode = d  = translate_address (&arg->dinode);

   if (dinode == NULL) {
      kprintf ("sys_fsupdate_dinode: bad address supplied\n");
      return (-E_INVAL);
   }

   StaticAssert (sizeof(dinode_t) == CFFS_DINODE_SIZE);
/*
   printf ("sys_fsupdate_dinode: action %d, param1 %d (%x), param2 %d\n", action, param1, param1, (u_int)param2);
   printf ("dinodeNum %d\n", dinode->dinodeNum);
*/

   switch (action) {
	case CFFS_DINODE_SETMASK: cffs_dinode_setMask (dinode, (mode_t)param1);
				break;
	case CFFS_DINODE_SETUID: cffs_dinode_setUid (dinode, (uid_t)param1);
				break;
	case CFFS_DINODE_SETGID: cffs_dinode_setGid (dinode, (gid_t)param1);
				break;
	case CFFS_DINODE_SETACCTIME: cffs_dinode_setAccTime (dinode, param1);
				break;
	case CFFS_DINODE_SETMODTIME: cffs_dinode_setModTime (dinode, param1);
				break;
	case CFFS_DINODE_SETCRETIME: cffs_dinode_setCreTime (dinode, param1);
				break;
	case CFFS_DINODE_SETLENGTH: cffs_dinode_setLength (dinode, param2); 
	                        assert (dinode->length == param2);
				break;

	case CFFS_DINODE_SETTYPE: 
				if (dinode->type == param1) {
				   return (0);
				}
				if ((dinode->dinodeNum != 0) && ((cffs_dinode_isDir(dinode)) || (param1 == S_IFDIR))) {
				   printf ("sys_fsupdate_dinode (%d): trying to change type to or from directory (%d -> %d)\n", action, dinode->type, param1);
				   return (-E_INVAL);
				}
				cffs_dinode_setType (dinode, (u_int16_t)param1);
				break;

	case CFFS_DINODE_SETLINKCOUNT:
                                if ((dinode->dinodeNum != 0) && (dinode->linkCount != param1)) {
				   printf ("sys_fsupdate_dinode (%d): changing link count directly (%d -> %d)\n", action, dinode->linkCount, param1);
				   //return (-E_INVAL);
				}
				cffs_dinode_setLinkCount (dinode, (nlink_t)param1);
				break;

	case CFFS_DINODE_SETOPENCOUNT: cffs_dinode_setOpenCount (dinode, param1);
				break;

	case CFFS_DINODE_SETDIRNUM:
				if (cffs_dinode_isDir(dinode)) {
				   printf ("sys_fsupdate_dinode (%d): trying to change dirNum of directory directly (%d -> %d) requested\n", action, dinode->dirNum, param1);
				   return (-E_INVAL);
				}
				cffs_dinode_setDirNum (dinode, param1);
				break;

	case CFFS_DINODE_SETDIRECT:
				cffs_dinode_setDirect (dinode, param1, (u_int)param2);
				break;

	case CFFS_DINODE_SETINDIRECT:
				cffs_dinode_setIndirect (dinode, param1, (u_int)param2);
				break;

	case CFFS_DINODE_SETGROUPSTART: cffs_dinode_setGroupstart (dinode, param1);
				break;
	case CFFS_DINODE_SETGROUPSIZE: cffs_dinode_setGroupsize (dinode, param1);
				break;
	case CFFS_DINODE_CLEARBLOCKPOINTERS: cffs_dinode_clearBlockPointers(dinode, param2);
				break;

	case CFFS_DINODE_DELAYEDFREE:
				if ((dinode->linkCount > (cffs_dinode_isDir(dinode))) || (dinode->openCount != 0)) {
				   printf ("sys_fsupdate_dinode (%d): can't free dinode with links or opens (%d, %d) requested\n", action, dinode->linkCount, dinode->openCount);
				   return (-E_INVAL);
				}
				for (i=0; i<NUM_DIRECT; i++) {
				   if (dinode->directBlocks[i] != 0) {
				      printf ("sys_fsupdate_dinode (%d): can't free dinode with direct blocks (%d == %d) requested\n", action, i, dinode->directBlocks[i]);
				      return (-E_INVAL);
				   }
				}
				for (i=0; i<NUM_INDIRECT; i++) {
				   if (dinode->indirectBlocks[i] != 0) {
				      printf ("sys_fsupdate_dinode (%d): can't free dinode with indirect blocks (%d == %d) requested\n", action, i, dinode->indirectBlocks[i]);
				      return (-E_INVAL);
				   }
				}
				/* In the real system, this wants to occur only after the block is written (unless it is not dirty) */
				dinode->dinodeNum = 0;
				break;

	case CFFS_DINODE_INITINDIRBLOCK:
				indirblock = (char *) dinode;

				/* XXX % */
				if ((indirblock == NULL) || ((u_int)indirblock % BLOCK_SIZE)) {
				    printf ("sys_fsupdate_dinode (%d): bad indirblock address supplied (%p)\n", action, indirblock);
				   return (-E_INVAL);
				}
				bzero (indirblock, BLOCK_SIZE);
				break;
	case CFFS_DINODE_INITDINODE:
				bzero ((char *)dinode, CFFS_DINODE_SIZE);
				dinode->memory_sanity = CFFS_DINODE_MEMORYSANITY;
				dinode->type = param1;
				dinode->linkCount = (cffs_dinode_isDir(dinode)) ? 2 : 1;
				dinode->dinodeNum = param2;
				dinode->dirNum = param3;
				break;

	default: printf ("sys_fsupdate_dinode: unknown action (%d) requested\n", action);
		return (-E_INVAL);
   }

   return (0);
}


// int cffsd_fsupdate_superblock (u_int sn, int action, struct superblock *superblock, u_int param1, u_int param2)
int serv_fsupdate_superblock (u_int client_eid, int action, struct cffsd_fsupdate_superblock_arg *arg)
{
  struct superblock *superblock;
  u_int param1 = arg->param1;
  u_int param2 = arg->param2;

  debug_request = 2;

  superblock = translate_address (&arg->superblock);

   StaticAssert (sizeof(struct superblock) == NBPG);
   StaticAssert (BLOCK_SIZE == NBPG);

   //   superblock = (struct superblock *) translate_address ((uint)superblock, NBPG);
   if (superblock == NULL) {
      return (-E_INVAL);
   }

   if (action == CFFS_SUPERBLOCK_SETDIRTY) {
	/* allow anyone to set dirty to a non-zero value at any time.    */
	/* However, must verify that there are no dirty blocks from this */
	/* file system before allowing anyone to set dirty to 0.         */
      superblock->dirty = param1;

   } else if (action == CFFS_SUPERBLOCK_SETROOTDINODENUM) {
	/* verify that the provided rootDInodeNum is in fact the correct     */
	/* system value -- we should know this based on the superblocks addr */
      superblock->rootDInodeNum = param1;

   } else if (action == CFFS_SUPERBLOCK_SETFSNAME) {
	/* verify that the provided fsname is in fact the correct value */
	/* -- should match XN's expectation...                          */
	/* Also, don't allow it to go out of bounds in superblock...    */
     //     char *str = (char *) trup(param1); /* XXX */
     strcpy (superblock->fsname, CFFS_FSNAME);
   } else if (action == CFFS_SUPERBLOCK_DELETE) {
     superblock->fsname[0] = 0;	/* just clear the name for now */
   } else if (action == CFFS_SUPERBLOCK_SETFSDEV) {
	/* verify that the provided fsdev is in fact the correct system value */
	/* -- XN should certainly know this...                                */
      superblock->fsdev = param1;

   } else if (action == CFFS_SUPERBLOCK_SETALLOCMAP) {
	/* this is a totally temporary call.  This field should be maintained */
	/* solely by XN...                                                    */
      superblock->allocMap = param1;

   } else if (action == CFFS_SUPERBLOCK_SETNUMBLOCKS) {
	/* this is a totally temporary call.  This field should be maintained */
	/* solely by XN...                                                    */
      superblock->numblocks = param1;

   } else if (action == CFFS_SUPERBLOCK_SETSIZE) {
	/* this is a totally temporary call.  This field should be maintained */
	/* solely by XN...                                                    */
      superblock->size = param1;

   } else if (action == CFFS_SUPERBLOCK_SETNUMALLOCED) {
	/* this is a totally temporary call.  This field should be maintained */
	/* by the ALLOCATE and DEALLOCATE methods...                          */
      superblock->numalloced = param1;

   } else if (action == CFFS_SUPERBLOCK_SETXNTYPE) {
	/* this is a totally temporary call.  This field should be maintained */
	/* and updated only by newfs!!...                                     */
      if ((param1 < 0) || (param1 >= CFFS_MAX_XNTYPES)) {
         printf ("sys_fsupdate_superblock (%d): bad typeno %d (NUM %d)\n", action, param1, CFFS_MAX_XNTYPES);
         return -E_INVAL;
      }
      superblock->xntypes[param1] = param2;

   } else if (action == CFFS_SUPERBLOCK_INIT) {
	/* this is a totally temporary call.  Just for when not using XN */
      bzero (superblock, BLOCK_SIZE);

   } else if (action == CFFS_SUPERBLOCK_SETBLK) {
     superblock->blk = param1;
   } else if (action == CFFS_SUPERBLOCK_SETQUOTA) {
     if (param1 > superblock->numblocks) {
       printf ("sys_fsupdate_superblock (%d): quota (%d) is bigger than available space (%d)\n", action, param1, superblock->numblocks);
       return -E_INVAL;
     }
     superblock->quota = param1;
   } else {
      printf ("sys_fsupdate_superblock: unknown action requested (%d)\n", action);
      return (-E_INVAL);
   }

   return (0);
}


static int isValidDirent (char *dirBlock, embdirent_t *dirent)
{
   embdirent_t *scanner;
   char *dirSector = (dirBlock + ((((u_int)dirent - (u_int)dirBlock) / CFFS_EMBDIR_SECTOR_SIZE) * CFFS_EMBDIR_SECTOR_SIZE));
   int i;

   for (i=0; i < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; i += scanner->entryLen) {
      scanner = (embdirent_t *) (dirSector + i);
      if (scanner == dirent) {
         return (1);
      }
   }
   return (0);
}


/* largely copied from cffs_embdir.c:lookupname() */
static int nameconflict (char *dirBlock, char *name, int namelen)
{
   int i, j;
   embdirent_t *dirent;

   for (i=0; i < BLOCK_SIZE; i += CFFS_EMBDIR_SECTOR_SIZE) {
      for (j=0; j < CFFS_EMBDIR_SECTOR_SPACEFORNAMES; j += dirent->entryLen) {
         dirent = (embdirent_t *) (dirBlock + i + j);
         if ((namelen == dirent->nameLen) && (name[0] == dirent->name[0]) && (dirent->type != (char) 0) && (bcmp(dirent->name, name, dirent->nameLen) == 0)) {
            return (1);
         }
      }
   }
   return (0);
}


static int isNameUnique (uint fsdev, dinode_t *dinode, char *name, u_int namelen)
{
   int i;
   void *ptr;

   for (i=0; i<NUM_DIRECT; i++) {
      if (dinode->directBlocks[i]) {
	ptr = exos_bufcache_map (NULL, fsdev, dinode->directBlocks[i], 1);
	if (!ptr) {
	  printf ("WARN: isNameUnique fails because one of the directory blocks is not resident.\n");
	  return (0);
	} 
	//	if (nameconflict ((char *) (KERNBASE + (((u32)buf->buf_pp) << PGSHIFT)), name, namelen)) {
	if (nameconflict (ptr, name, namelen)) {
	  printf ("name conflict found\n");
	  return (0);
	}
      }
   }
   return (1);
}


// int cffsd_fsupdate_directory (u_int sn, int action, struct embdirent *dirent, u_int param2, u_int param3, u_int param4, u_int param5)
int serv_fsupdate_directory (u_int client_eid, int action, struct cffsd_fsupdate_directory_arg *arg)
{
  struct embdirent *dirent;
  u_int param2 = arg->param2;
  u_int param3 = arg->uint_param3;
  //  u_int param4 = arg->uint_param4;
  u_int param5 = arg->param5;

   u_int param1;
   /* XXX % */
   // char *dirBlock = (char *) translate_address ((param1 - (param1 % BLOCK_SIZE)), BLOCK_SIZE);
   char *dirBlock;

   debug_request = 3;

   dirent = translate_address (&arg->dirent);
   param1 = (u_int) dirent;
   dirBlock = (char *)((param1 - (param1 % BLOCK_SIZE)));

   if (dirBlock == NULL) {
      return (-E_INVAL);
   }

   /* XXX % */
   dirent = (embdirent_t *) (dirBlock + (param1 % BLOCK_SIZE));

   /* GROK -- need to check for write permission on directory.  The relevant  */
   /* inode should be provided automatically by base disk protection software */

   if (!isValidDirent(dirBlock, dirent)) {
      printf ("sys_fsupdate_directory (%d): invalid dirent specified (%x)\n", action, param1);
      return (-E_INVAL);
   }

/************************** CFFS_DIRECTORY_INITDIRBLOCK ***********************/

   if (action == CFFS_DIRECTORY_INITDIRBLOCK) {
      int i;
      dinode_t *dinode;

      /* XXX */
      if ((u_int)dirent % BLOCK_SIZE) {
         printf ("sys_fsupdate_directory (%d): dirent not start of block (%p)\n", action, dirent);
         return (-E_INVAL);
      }

      bzero ((char *)dirent, BLOCK_SIZE);

      for (i=0; i<(BLOCK_SIZE/CFFS_EMBDIR_SECTOR_SIZE); i++) {
         dirent->type = (char) 0;
         dirent->entryLen = CFFS_EMBDIR_SECTOR_SPACEFORNAMES;
         dirent->preventryLen = 0;

         dinode = (dinode_t *) ((char *)dirent + CFFS_DINODE_SIZE);
         dinode->dinodeNum = 0;
         dinode = (dinode_t *) ((char *)dirent + (2*CFFS_DINODE_SIZE));
         dinode->dinodeNum = 0;
         dinode = (dinode_t *) ((char *)dirent + (3*CFFS_DINODE_SIZE));
         dinode->dinodeNum = 0;

         dirent = (embdirent_t *) ((char *) dirent + CFFS_EMBDIR_SECTOR_SIZE);
      }


/*************************** CFFS_DIRECTORY_SPLITENTRY ************************/

   } else if (action == CFFS_DIRECTORY_SPLITENTRY) {
      embdirent_t *newDirent;
      int splitspace = param2;
      int extraspace;

      extraspace = (dirent->type == (char)0) ? dirent->entryLen : (dirent->entryLen - embdirentsize(dirent));
      if ((extraspace < SIZEOF_EMBDIRENT_T) || (extraspace < splitspace)) {
         printf ("sys_fsupdate_directory (%d): not enough extra space (%d) (wanted %d)\n", action, extraspace, splitspace);
         return (-E_INVAL);
      }

      if (splitspace == 0) {
         printf ("sys_fsupdate_directory (%d): bad splitspace (%d) (extraspace %d)\n", action, splitspace, extraspace);
         return (-E_INVAL);
      }

      dirent->entryLen -= splitspace;
      newDirent = (embdirent_t *) ((u_int)dirent + dirent->entryLen);
      if (newDirent != dirent) {
         newDirent->preventryLen = dirent->entryLen;
      }
      newDirent->entryLen = splitspace;
      newDirent->type = 0;
      newDirent->nameLen = 0;
      newDirent->name[0] = (char) 0;
      if (((u_int)newDirent + newDirent->entryLen) % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
         dirent = (embdirent_t *) ((char *)newDirent + newDirent->entryLen);
         dirent->preventryLen = splitspace;
      }
      /* XXX % */
      //      ret = (param1 - (param1 % BLOCK_SIZE)) + ((u_int)newDirent - (u_int)dirBlock);
      return ((u_int )newDirent);

/*************************** CFFS_DIRECTORY_SETNAME ************************/

   } else if (action == CFFS_DIRECTORY_SETNAME) {
     //     char *name = (char *) trup(param2); /* XXX */
     char name[256];

      u_int namelen = param3;
      dinode_t *dirDInode = translate_address (&arg->ref_param4);

      /* XXX -- I thought we only reserved < 128 bytes for names? */
      if (namelen > 256) {
	printf ("sys_fsupdate_directory (%d): name too long (%d)\n", action, namelen);
	return (-E_INVAL);
      }

      if (cffsd_copyin (client_eid, param2, (uint )name, namelen) == -1)
	return (-E_INVAL);

      if ((dirDInode == NULL) || (dirDInode->dinodeNum == 0)) {
         return (-E_INVAL);
      }

	/* Note: param5 is fsdev in this case, which should be discoverable */
	/* from the dinode's identity...                                    */
      if ((dirent->type != 0) && (!isNameUnique(param5, dirDInode, name, namelen))) {
         printf ("sys_fsupdate_directory (%d): non-unique name specified (%d)\n", action, namelen);
         return (-E_INVAL);
      }

      bcopy(name, dirent->name, namelen);
      //      dirent->name[namelen] = (char) 0;
      dirent->nameLen = namelen;

/*************************** CFFS_DIRECTORY_MERGEENTRY ************************/

   } else if (action == CFFS_DIRECTORY_MERGEENTRY) {
      embdirent_t *next = NULL;
      int maxleft = CFFS_EMBDIR_SECTOR_SPACEFORNAMES - (param1 % CFFS_EMBDIR_SECTOR_SIZE);
      int extraspace = 0;

      while ((extraspace < param2) && (extraspace < maxleft)) {
         next = (embdirent_t *) ((char *)dirent + dirent->entryLen);
         if (next->type != 0) {
            break;
         }
         extraspace += next->entryLen;
      }

      if (extraspace != param2) {
         printf ("sys_fsupdate_directory (%d): mismatched extraspaces (%d != %d) (maxleft %d)\n", action, extraspace, param2, maxleft);
         printf ("next %p, next->entryLen %d\n", next, ((next) ? (next->entryLen) : -1));
         return (-E_INVAL);
      }

      dirent->entryLen += extraspace;
      next = (embdirent_t *) ((char *)dirent + dirent->entryLen);
      if ((u_int) next % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
         next->preventryLen = dirent->entryLen;
      }

/*************************** CFFS_DIRECTORY_SHIFTENTRY ************************/

   } else if (action == CFFS_DIRECTORY_SHIFTENTRY) {

	/********* Not currently supported! ************/
      assert (0);

#if 0
      embdirent_t *tmp;
      int extraspace;

      if ((-param2 > (param1 % CFFS_EMBDIR_SECTOR_SIZE)) || ((param2 + (param1 % CFFS_EMBDIR_SECTOR_SIZE)) >= (CFFS_EMBDIR_SECTOR_SPACEFORNAMES - SIZEOF_EMBDIRENT_T))) {
         printf ("sys_fsupdate_directory (%d): sizes don't work (currOff %d, move %d)\n", action, (param1 % BLOCK_SIZE), param2);
         return (-E_INVAL);
      }

      if (param2 <= 0) {
         tmp = (embdirent_t *) ((char *)dirent - dirent->preventryLen);
         extraspace = (tmp->type == (char)0) ? tmp->entryLen : tmp->entryLen - embdirentsize(tmp);
         if (extraspace < (-param2)) {
            printf ("sys_fsupdate_directory (%d): not enough extra space (%d < %d)\n", action, extraspace, param2);
            return (-E_INVAL);
         }
         tmp->entryLen -= (-param2);
         dirent->preventryLen = tmp->entryLen;
         dirent->entryLen += (-param2);
         bcopy ((char *) dirent, ((char *)dirent - (-param2)), embdirentsize(dirent));
         tmp = (embdirent_t *) ((char *)dirent + dirent->entryLen);
         if ((u_int) tmp % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
            tmp->preventryLen = dirent->entryLen;
         }
         
      } else {
         tmp = (embdirent_t *) ((char *)dirent - dirent->preventryLen);
         extraspace = dirent->entryLen - embdirentsize(dirent);
         if (extraspace < param2) {
            printf ("sys_fsupdate_directory (%d): not enough extra space (%d < %d)\n", action, extraspace, param2);
            return (-E_INVAL);
         }
         tmp->entryLen += param2;
         dirent->preventryLen = tmp->entryLen;
         dirent->entryLen -= param2;
         bcopy ((char *)dirent, ((char *)dirent + param2), embdirentsize(dirent));
         dirent = (embdirent_t *) ((char *)dirent + tmp->entryLen);
         tmp = (embdirent_t *) ((char *)dirent + dirent->entryLen);
         if ((u_int) tmp % CFFS_EMBDIR_SECTOR_SPACEFORNAMES) {
            tmp->preventryLen = dirent->entryLen;
         }
      }
#endif

/*************************** CFFS_DIRECTORY_SETINODENUM ***********************/

   } else if (action == CFFS_DIRECTORY_SETINODENUM) {
      u_int inodeNum = param2;

	/* no restrictions is directory entry is not currently live */
      if (dirent->type == 0) {
         dirent->inodeNum = inodeNum;

	/* this case causes a change in which inode a dirent points to.      */
	/* because the dirent is live, it must always point to a live inode. */
      } else {
         dinode_t *olddinode;
         dinode_t *newdinode;

         if (inodeNum == 0) {
            printf ("sys_fsupdate_directory (%d): can't set inodeNum to 0 is live dirent\n", action);
            return (-E_INVAL);
         }

	 /*
         if ((param3 == 0) || (param4 == 0)) {
            printf ("sys_fsupdate_directory (%d): passed NULL for a cheat variable (%d, %d)\n", action, param3, param4);
            return (-E_INVAL);
         }
	 */
	/* it is a bug in FS code to have valid dirent with 0 for an inodeNum */
         assert (dirent->inodeNum != 0);

	/* this should be retrievable from dirent->inodeNum, which identifies */
	/* the containing disk block -- which must be a directory block.      */
         olddinode = translate_address (&arg->ref_param3);
         if (olddinode->dinodeNum != dirent->inodeNum) {
            printf ("sys_fsupdate_directory (%d): mismatching old inodeNums (%d != %d)\n", action, olddinode->dinodeNum, dirent->inodeNum);
            return (-E_INVAL);
         }

	/* this should be retrievable from inodeNum, which identifies the */
	/* containing disk block -- which must be a directory block.     */
         newdinode = translate_address (&arg->ref_param4);
         if (newdinode->dinodeNum != inodeNum) {
            printf ("sys_fsupdate_directory (%d): mismatching old inodeNums (%d != %d)\n", action, newdinode->dinodeNum, inodeNum);
            return (-E_INVAL);
         }

         if ((olddinode->type != newdinode->type) && ((cffs_dinode_isDir(olddinode)) || (cffs_dinode_isDir(newdinode)))) {
            printf ("sys_fsupdate_directory (%d): can't interchange a directory and non-directory (%x != %x)\n", action, olddinode->type, newdinode->type);
            return (-E_INVAL);
         }

		/* this rule helps get us around an ordering requirement!! */
		/* (i.e., if we handle ordering in some other way, it can  */
		/* go away...                                              */
         if (newdinode->linkCount < (cffs_dinode_isDir(newdinode) ? 2 : 1)) {
            printf ("sys_fsupdate_directory (%d): can't link up to disowned inode\n", action);
            return (-E_INVAL);
         }

	/* the big stickler here is that we need to be able to check whether */
	/* the caller has the ability to look at newdinode.  In particular,  */
	/* this requires execute permission on a directory that already      */
	/* contains an existing link.                                        */

         if ((olddinode) && (cffs_dinode_isDir(olddinode))) {
            /* must check that directory is actually empty... */
         }

	/* GROK - I'm pretty sure I've designed away all need for ordering here */
         olddinode->linkCount--;
         newdinode->linkCount++;
         dirent->inodeNum = inodeNum;
         dirent->type = newdinode->type >> 8;
      }

/**************************** CFFS_DIRECTORY_SETTYPE **************************/

   } else if (action == CFFS_DIRECTORY_SETTYPE) {
      u_int type = param2;
      dinode_t *dinode;
      dinode_t *parentDinode;

	/* that was easy... */
      if (dirent->type == type) {
         return (0);
      }

	/* GROK - this can probably be loosened.  In particular, only */
	/* directories are special the law at this point...           */
      if ((dirent->type != 0) && (type != 0)) {
         printf ("sys_fsupdate_directory (%d): trying to change a valid type (%d -> %d)\n", action, dirent->type, type);
         return (-E_INVAL);
      }

      if ((type != 0) && (dirent->inodeNum == 0)) {
         printf ("sys_fsupdate_directory (%d): trying to make an invalid dirent live (ino %x)\n", action, dirent->inodeNum);
         return (-E_INVAL);
      }

	/* this should be retrievable from dirent->inodeNum, which identifies */
	/* the containing disk block -- which must be a directory block.      */
      dinode = translate_address (&arg->ref_param3);


      if ((dinode->dinodeNum != 0) && (dinode->dinodeNum != dirent->inodeNum)) {
         printf ("sys_fsupdate_directory (%d): mismatching inodeNums (%d != %d)\n", action, dinode->dinodeNum, dirent->inodeNum);
         return (-E_INVAL);
      }

	/* this should be retrievable from dirent (which is in some directory */
	/* block on which a protection check must be done anyway...)          */
      parentDinode = translate_address(&arg->ref_param4);

      /* sanity check on parentDinode?? */
      if ((parentDinode == NULL) || (parentDinode->dinodeNum == 0)) {
         return (-E_INVAL);
      }

	/* this is link addition -- dirent will be made valid */
      if (type != 0) {
	/* Note: param5 is fsdev in this case, which should be discoverable */
	/* from the dinode's identity...                                    */

         if (!isNameUnique(param5, parentDinode, dirent->name, dirent->nameLen)) {
            printf ("sys_fsupdate_directory (%d): non-unique name specified (%s, %d)\n", action, dirent->name, dirent->nameLen);
            return (-E_INVAL);
         }
		/* new link to existing file */
         if (dinode->dinodeNum != 0) {

		/* this rule helps get us around an ordering requirement!! */
		/* (i.e., if we handle ordering in some other way, it can  */
		/* go away...                                              */
            if (dinode->linkCount < (cffs_dinode_isDir(dinode) ? 2 : 1)) {
               printf ("sys_fsupdate_directory (%d): can't link up to disowned inode\n", action);
               return (-E_INVAL);
            }

            if (cffs_dinode_isDir(dinode)) {
               printf ("sys_fsupdate_directory (%d): can't set link directory\n", action);
               return (-E_INVAL);
            }

            if (dinode->type != (type << 8)) {
               printf ("sys_fsupdate_directory (%d): trying to set type incorrectly for desired inode (%x != %x)\n", action, dinode->type, type);
               return (-E_INVAL);
            }
            
	/* the big stickler here is that we need to be able to check whether */
	/* the caller has the ability to look at newdinode.  In particular,  */
	/* this requires execute permission on a directory that already      */
	/* contains an existing link.                                        */

		/* new file creation */
         } else {
            if (((u_int)dirent / CFFS_EMBDIR_SECTOR_SIZE) != ((u_int)dinode / CFFS_EMBDIR_SECTOR_SIZE)) {
               printf ("sys_fsupdate_directory (%d): can't create new file linked across sector boundary\n", action);
               return (-E_INVAL);
            }

            bzero ((char *)dinode, sizeof(dinode_t));
            dinode->memory_sanity = CFFS_DINODE_MEMORYSANITY;
            dinode->dinodeNum = dirent->inodeNum;
            dinode->dirNum = parentDinode->dinodeNum;
            dinode->type = type << 8;
            if (cffs_dinode_isDir(dinode)) {
               dinode->linkCount++;
               parentDinode->linkCount++;
            }
         }
         dinode->linkCount++;
         dirent->type = type;

	/* this is link removal -- dirent will be made invalid */
      } else {
         if (cffs_dinode_isDir(dinode)) {
            /* must check that directory is actually empty... */
         }

	/* again, I think we can skip any ordering dependencies... */
         if (cffs_dinode_isDir(dinode)) {
            assert (dinode->dirNum == parentDinode->dinodeNum);
            parentDinode->linkCount--;
         }

         dirent->type = (char) 0;
         dinode->linkCount--;
      }

   } else {
      printf ("sys_fsupdate_directory: unknown action (%d) requested\n", action);
   }

   return (0);
}


// int cffsd_fsupdate_renamedir (u_int sn, int action, struct embdirent *direntTo, struct embdirent *direntFrom, struct dinode *dinode, struct dinode *parentTo, struct dinode *parentFrom, struct dinode *dyingDinode)
int serv_fsupdate_renamedir (u_int client_eid, int action, struct cffsd_fsupdate_renamedir_arg *arg)
{
   return (-E_INVAL);
}


// int cffsd_fsupdate_setptr (u_int sn, int action, struct dinode *dinode, char *allocMap, u_int *ptrloc, u_int val)
int serv_fsupdate_setptr (u_int client_eid, int action, struct cffsd_fsupdate_setptr_arg *arg)
{
  struct dinode *dinode = NULL;	/* to avoid compiler warning */
  char *allocMap = NULL;	/* to avoid compiler warning */
  u_int *ptrloc = NULL;		/* to avoid compiler warning */
  cffs_t *sb;
  u_int val = arg->val;

   int blkno = val % BLOCK_SIZE;

   debug_request = 4;

   sb = translate_address (&arg->sb);
   if (action != CFFS_SETPTR_EXTERNALALLOC && action != CFFS_SETPTR_EXTERNALFREE)
     dinode = translate_address (&arg->dinode);
   if (action != CFFS_SETPTR_NULLIFY)
     allocMap = translate_address (&arg->allocmap);
   if (action != CFFS_SETPTR_EXTERNALALLOC && action != CFFS_SETPTR_EXTERNALFREE)
     ptrloc = translate_address (&arg->ptrloc);
   
/*
printf ("sys_fsupdate_setptr: action %d, dinode %p, ptrloc %p, *ptrloc %d, val %d\n", action, dinode, ptrloc, *ptrloc, val);
*/

   if (((dinode == NULL) && (action != CFFS_SETPTR_EXTERNALALLOC && action != CFFS_SETPTR_EXTERNALFREE)) || 
       ((ptrloc == NULL) && (action != CFFS_SETPTR_EXTERNALALLOC && action != CFFS_SETPTR_EXTERNALFREE)) || 
       ((allocMap == NULL) && (action != CFFS_SETPTR_NULLIFY)) ||
       sb == NULL) {
      kprintf ("sys_fsupdate_setptr: bad address (%p %p %p %p, action %d)\n", sb, dinode, ptrloc, allocMap, action);
      return (-E_INVAL);
   }

   /* missing: protection checks on writing block pointer fields... */

   if (action == CFFS_SETPTR_ALLOCATE) {
	/* Second part of && allows fsck to use this to verify/set allocMap */
      if ((val == 0) || ((*ptrloc != 0) && (*ptrloc != val))) {
         kprintf ("sys_fsupdate_setptr (%d): bad value pair (%d -> %d)\n", action, *ptrloc, val);
         return (-E_INVAL);
      }
      if (sb->quota && sb->numalloced+1 >sb->quota) {
	return -E_QUOTA;
      }
   } else if (action == CFFS_SETPTR_DEALLOCATE) {
      if ((val != 0) || (*ptrloc == 0)) {
         kprintf ("sys_fsupdate_setptr (%d): bad value pair (%d -> %d)\n", action, *ptrloc, val);
         return (-E_INVAL);
      }
   } else if (action == CFFS_SETPTR_REPLACE) {
      if ((val == 0) || (*ptrloc == 0)) {
         kprintf ("sys_fsupdate_setptr (%d): bad value pair (%d -> %d)\n", action, *ptrloc, val);
         return (-E_INVAL);
      }
      if (((*ptrloc)/BLOCK_SIZE) != (val / BLOCK_SIZE)) {
         kprintf ("sys_fsupdate_setptr (%d): long-range REPLACEs not yet valid (%d -> %d)\n", action, *ptrloc, val);
         return (-E_INVAL);
      }
   } else if (action == CFFS_SETPTR_NULLIFY) {
      val = 0;
   } else if (action == CFFS_SETPTR_EXTERNALALLOC) {
     if (allocMap[blkno] == CFFS_ALLOCMAP_INUSE)
       return -E_INVAL;
     allocMap[blkno] = CFFS_ALLOCMAP_INUSE;
     return 0;
   } else if (action == CFFS_SETPTR_EXTERNALFREE) {
     if (allocMap[blkno] == CFFS_ALLOCMAP_FREE)
       return -E_INVAL;
     allocMap[blkno] = (char )CFFS_ALLOCMAP_FREE;
     return 0;
   } else {
      kprintf ("sys_fsupdate_setptr: unknown action requested (%d)\n", action);
      return (-E_INVAL);
   }

   /* missing: check that value falls within range of specified allocMap   */
   /* block.  Of course, XN will automatically provide the right block, so */
   /* no need to waste effort here...                                      */
   if ((action == CFFS_SETPTR_ALLOCATE) || (action == CFFS_SETPTR_REPLACE)) {
      if (allocMap[blkno] == CFFS_ALLOCMAP_INUSE) {
         printf ("sys_fsupdate_setptr (%d): desired block (%d) is taken\n", action, val);
         return (-E_EXISTS); 
      }
      allocMap[blkno] = (char) CFFS_ALLOCMAP_INUSE;
   }

   if ((action == CFFS_SETPTR_DEALLOCATE) || (action == CFFS_SETPTR_REPLACE)) {
	/* bug in protected stuff if block isn't marked taken */
      int blkno2 = (*ptrloc) % BLOCK_SIZE;
      if (allocMap[blkno2] != (char )CFFS_ALLOCMAP_INUSE) {
	printf ("sys_fsupdate_setptr (%d): trying to free free block\n", action);
	return -E_INVAL;
      }
      allocMap[blkno2] = (char) CFFS_ALLOCMAP_FREE;
   }

   *ptrloc = val;

   return (0);
}


int serv_fsupdate_initAllocMap (u_int client_eid, struct cffsd_fsupdate_initAllocMap_arg *arg)
{
  char *allocMap;
  int deadcnt = arg->deadcnt;
  int count = arg->count;
  int ret;
  int i, j;

  debug_request = 5;
   
   /* check to make sure we're not dealing with more 
      freemap blocks that we can handle in our
      ipc marshalling code */

   if (count >= CFFSD_MAX_INITALLOC_SZ) return -E_INVAL;

   /* free it in case it was already in use */
   ret = sys_micropart_free (arg->part, CAP_ROOT);
   if (ret < 0) {
     kprintf ("cffsd: sys_micropart_free returned %d\n", ret);
     return -E_INVAL;
   }

   ret = sys_micropart_init (arg->part, CAP_ROOT);
   if (ret < 0) {
     kprintf ("cffsd: sys_micropart_init returned %d\n", ret);
     return -E_INVAL;
   }

   ret = sys_micropart_bootfs (arg->part, CAP_ROOT, FSID);
   if (ret < 0) {
     kprintf ("cffsd: sys_micropart_bootfs returned %d\n", ret);
     return -E_INVAL;
   }

   /* go ahead and allocate the first micropart */
   ret = sys_micropart_alloc (arg->part, 0, CAP_ROOT, FSID);
   if (ret < 0) {
     kprintf ("cffsd: sys_micropart_alloc returned %d\n", ret);
     return -E_INVAL;
   }

   for (i = 0; i < count; i++) {
     allocMap = translate_address (&arg->allocmap[i]);
     if (!allocMap) return -E_INVAL;

     /* we allocated the first micropart above so go ahead
	and open up that part of the freemap */

     if (i < FREEMAP_PAGES_PER_MICROPART) {
       for (j = 0; j < BLOCK_SIZE; j++) {
	 allocMap[j] = CFFS_ALLOCMAP_FREE;
       }
       if (i == 0) {
	 /* the boot code, micropart metadata, etc are all 
	    in the first 100 blocks, so mark that in use */
	 for (j=0; j<100; j++) {
	   allocMap[j] = CFFS_ALLOCMAP_INUSE;
	 }
	 /* this marks the blocks the alloc maps themsevles
	    are in as already allocated  */
	 for (j=0; j<deadcnt; j++) {
	   allocMap[j] = CFFS_ALLOCMAP_INUSE;
	 }
       }
     } else {
       for (j = 0; j < BLOCK_SIZE; j++) {
	 allocMap[j] = CFFS_ALLOCMAP_NOTCFFS;
       }
     }
   }

   return (0);
}

int serv_fsupdate_allocMicropart (u_int client_eid, struct cffsd_fsupdate_allocMicropart_arg *arg)
{
  char *alloc_map;
  int count = arg->count;
  int i,j;
  int ret;

  debug_request = 6;

  if (count >= CFFSD_MAX_ALLOCMICROPART_SZ) return -E_INVAL;

  /* XXX -- should really get a ref to the superblock too
     and make sure that we're opening up the right allocmap blocks
     for the micropart we're allocating. */

  ret = sys_micropart_alloc (arg->part, arg->micropart, CAP_ROOT, FSID);
  if (ret < 0) {
    return ret;
  }

  for (i = 0; i < count; i++) {
    alloc_map = translate_address (&arg->allocmap[i]);
    if (!alloc_map) return -E_INVAL;
    for (j = 0; j < BLOCK_SIZE; j++) {
      alloc_map[j] = CFFS_ALLOCMAP_FREE;
    }
  }
  return 0;
}

int serv_boot (u_int client_eid, u_int part) {
  int ret;

  ret = sys_micropart_load (part, CAP_ROOT);
  if (ret == -E_INVAL) return 0; /* XXX */
  if (ret < 0) {
    return -E_INVAL;
  }

  ret = sys_micropart_bootfs (part, CAP_ROOT, FSID);
  if (ret == -E_EXISTS) return ret; /* preserve so client knows already booted */
  if (ret < 0) {
    kprintf ("cffsd: sys_micropart_bootfs returned %d\n", ret);
    return -E_INVAL;
  }

  return 0;
}  

/* handle incoming ipc requests */
int cffsd_request (int demux_id, int type, int action, int arg_ptr, uint caller_eid) {
  int ret;
  struct cffsd_fsupdate_superblock_arg sb_arg;
  struct cffsd_fsupdate_dinode_arg di_arg;
  struct cffsd_fsupdate_directory_arg dir_arg;
  struct cffsd_fsupdate_setptr_arg ptr_arg;
  struct cffsd_fsupdate_initAllocMap_arg map_arg;
  struct cffsd_fsupdate_allocMicropart_arg micropart_arg;

  switch (type) {
  case CFFSD_FSUPDATE_SUPERBLOCK:
    if (cffsd_copyin (caller_eid, arg_ptr, (uint )&sb_arg, sizeof (sb_arg)) == -1)
      return -E_INVAL;
    ret = serv_fsupdate_superblock (caller_eid, action, &sb_arg);
    break;
  case CFFSD_FSUPDATE_DINODE:
    if (cffsd_copyin (caller_eid, arg_ptr, (uint )&di_arg, sizeof (di_arg)) == -1)
      return -E_INVAL;
    ret = serv_fsupdate_dinode (caller_eid, action, &di_arg);
    break;
  case CFFSD_FSUPDATE_DIRECTORY:
    if (cffsd_copyin (caller_eid, arg_ptr, (uint )&dir_arg, sizeof (dir_arg)) == -1)
      return -E_INVAL;
    ret = serv_fsupdate_directory (caller_eid, action, &dir_arg);
    break;
  case CFFSD_FSUPDATE_SETPTR:
    if (cffsd_copyin (caller_eid, arg_ptr, (uint )&ptr_arg, sizeof (ptr_arg)) == -1)
      return -E_INVAL;
    ret = serv_fsupdate_setptr (caller_eid, action, &ptr_arg);
    break;
  case CFFSD_FSUPDATE_INITALLOCMAP:
    if (cffsd_copyin (caller_eid, arg_ptr, (uint )&map_arg, sizeof (map_arg)) == -1)
      return -E_INVAL;
    ret = serv_fsupdate_initAllocMap (caller_eid, &map_arg);
    break;
  case CFFSD_FSUPDATE_ALLOCMICROPART:
    if (cffsd_copyin (caller_eid, arg_ptr, (uint )&micropart_arg, sizeof (micropart_arg)) == -1)
      return -E_INVAL;
    ret = serv_fsupdate_allocMicropart (caller_eid, &micropart_arg);
    break;
  case CFFSD_BOOT:
    ret = serv_boot (caller_eid, action);
    break;
  default:
    ret = -1;
    printf ("cffsd: received bogus request %d from %d\n", type, caller_eid);
  }

  if (ret != 0) {
    if ((type != CFFSD_FSUPDATE_DIRECTORY && action != CFFS_DIRECTORY_SPLITENTRY) &&
	(type != CFFSD_BOOT) &&
	(type != CFFSD_FSUPDATE_ALLOCMICROPART) &&
	(type != CFFSD_FSUPDATE_SETPTR && (action != CFFS_SETPTR_EXTERNALALLOC &&
					   action != CFFS_SETPTR_EXTERNALFREE &&
					   action != CFFS_SETPTR_ALLOCATE))) { 

      kprintf ("cffsd: returning error value %d operation %d command %d\n", ret, type, action);
    }
  }

  return ret;
}

#ifdef STANDALONE
int main () {
#else
int cffsd_main () {
#endif
  int ret;

  printf ("cffsd: starting\n");

  ret = nameserver_remove (NS_NM_CFFSD);
  if (ret != NS_OK && ret != NS_INVAL) {
    printf ("cffsd: got bad response when trying to remove prior registration\n");
  }

  ret = nameserver_register (NS_NM_CFFSD, __envid);
  if (ret != NS_OK) {
    printf ("cffsd: couldn't register with ipc nameservice\n");
  }

  ipcdemux_register (IPC_CFFSD, cffsd_request, 5);

  EnterCritical ();

  UAREA.u_status = U_SLEEP;
  yield (-1);

  assert (0);

  return 0;
}
  

