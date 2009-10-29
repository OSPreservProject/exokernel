
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

#include "alfs/alfs_general.h"
#include "alfs/alfs.h"
#include "alfs/alfs_buffer.h"
#include "alfs/alfs_inode.h"
#include "alfs/alfs_protection.h"
#include "alfs/alfs_path.h"
#include <assert.h>


/* Following variables store a process's identity. In a real system
   these would be installed by whoever creates us. But, since we
   don't have anything like that yet, we just fake it in initProtection */

_uid_t uid;			/* who I am */
_gid_t gid;
cap2_t uidCap;			/* and how to prove it */
cap2_t gidCap;

static umask_t __umask;			/* default file creation permissions */

/* for our fictional users */
cap2_t hbricenoUidCap, pinckneyUidCap;
cap2_t usersGidCap, pdosGidCap;

/* XXX -- Users with write permission will also get read permission since
   to write to the middle of a block will require reading in the block
   and modifying it before writing it back. This makes it impossible
   to give a user write but not read permission (say for a log that
   anyone can write but only a particular user can read). */

void initProtection () {

     /* create our two users and groups */

     hbricenoUidCap = (cap2_t) 0x12345678;
     pinckneyUidCap = (cap2_t) 0x23456789;

     usersGidCap = (cap2_t) 0x34567890;
     pdosGidCap = (cap2_t) 0x45678901;
}

void setUid (_uid_t newUid, cap2_t newUidCap) {

     uid = newUid;
     uidCap = newUidCap;
}

void setGid (_gid_t newGid, cap2_t newGidCap) {

     gid = newGid;
     gidCap = newGidCap;
}
     
/* switch to a particular user */
void setUser (int who) {
     if (who == PINCKNEY_UID) {
	  setUid (PINCKNEY_UID, pinckneyUidCap);
	  setGid (USERS_GID, usersGidCap);
	  __umask = AXS_IRUSR | AXS_IWUSR | AXS_IRGRP | AXS_IROTH;
     } else {
	  setUid (HBRICENO_UID, hbricenoUidCap);
	  setGid (USERS_GID, usersGidCap);
	  __umask = AXS_IRUSR | AXS_IWUSR | AXS_IRGRP | AXS_IROTH;
     }
}

/* next two functions let us check if we should be able to read or
   write a file given the file's ownership and it's mask.*/

int verifyReadPermission (umask_t mask, _uid_t myUid, _uid_t fileUid, 
		   _gid_t myGid, _gid_t fileGid) {

     if (mask & AXS_IRUSR && myUid == fileUid) {
	  return (1);
     }
     else if (mask & AXS_IRGRP && myGid == fileGid) {
	  return (1);
     }
     else if (mask & AXS_IROTH) {
	  return (1);
     }
     else if (verifyWritePermission (mask, myUid, fileUid, myGid, fileGid)) {
	  return (1);
     } else {
	  return (0);
     }
}

int verifyWritePermission (umask_t mask, _uid_t myUid, _uid_t fileUid, 
		    _gid_t myGid, _gid_t fileGid) {

     if (mask & AXS_IWUSR && myUid == fileUid) {
	  return (1);
     }
     else if (mask & AXS_IWGRP && myGid == fileGid) {
	  return (1);
     }
     else if (mask & AXS_IWOTH) {
	  return (1);
     }
     else
	  return (0);
}

/* change umask for future file creations */

umask_t setUmask (umask_t newMask) {
     umask_t tmp;

     tmp = __umask;
     __umask = newMask;
     return (tmp);
}

/* like chmod */

int changeProtection (char *path, inode_t *rootInode, umask_t mask) {
     inode_t *inode;
     int inodeNum;
     int ret;
     umask_t oldMask;

     /* find the inode we're going to change */
     if ((ret = alfs_path_translateNameToInode (path, rootInode, &inode, BUFFER_WITM)) < 0) {
	  return (ret);
     }
     assert (inode);
     inodeNum = inode->inodeNum;	/* GROK - necessary? */

     /* we have to read the block readonly at first, since we 
	may not have write permission. Once we've verified that
	we have write permission, we can get a new buffer with
	write permission. */

     /* make sure we're the onwer */
     if (alfs_inode_getUid(inode) != uid) {
	  alfs_inode_releaseInode (inode, 0);
	  return (ALFS_EPERM);
     }

     /* update the inode */
     oldMask = alfs_inode_getMask(inode);
     alfs_inode_setMask(inode, mask);

     /* ok, now tell cs about the new permissions */

     /* XXX -- need to fix this to pass in the right group cap */
/* temporary bypass
     ret = cs_setFileInfo (root, uidCap, gidCap, mask);
*/

     if (ret != OK) {
	  alfs_inode_setMask(inode, oldMask);
     }
     alfs_inode_releaseInode (inode, BUFFER_DIRTY);

     return (ret);
}

