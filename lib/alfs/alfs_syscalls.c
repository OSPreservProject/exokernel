
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

#include "alfs/alfs_buffer.h"
#include "alfs/alfs.h"
#include "alfs/alfs_inode.h"
#include "alfs/alfs_path.h"
#include "alfs/alfs_protection.h"
#include "alfs/alfs_rdwr.h"
#include "alfs/alfs_alloc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <machine/param.h>

#include "alfs/alfs_defaultcache.h"

#include "alfs/alfs_diskio.h"

/*
#define ORDERING
#define PRINT_FDCALLS
*/

/* temporary stuff */

cap2_t alfs_FScap;

block_num_t alfs_FSdev = 23;

/* unix-like file i/o interface */

/* our idea of where we currently are and what our root is */
inode_t *alfs_currentDirInode = NULL;
inode_t *alfs_currentRootInode = NULL;
buffer_t *alfs_currentRootBuffer = NULL;
uint alfs_superblkno = 0xFFFFFFFF;

static alfs_t alfs;
alfs_t *alfs_superblock = &alfs;


/* number of blocks needed to define an ALFS file system: superblock,  */
/* allocation maps, directory maps (if any), dinode maps, first inode  */
/* block and root directory block.                                     */

uint alfs_initmetablks (uint dev, int size)
{
   return (1 + alfs_alloc_initmetablks(size) + alfs_path_initmetablks() + alfs_dinode_initmetablks() + 1 + 1);
}


/* create a file system on a disk */

uint alfs_initFS (uint dev, int size, cap2_t fscap)
{
   char buf[BLOCK_SIZE];
   char buf2[BLOCK_SIZE];
   dinode_t *dinode = (dinode_t *) &buf[(2*sizeof(dinode_t))];
   block_num_t superblkno;
   block_num_t blkno;
   block_num_t dirblkno;
   struct buf buft;
   int initmetablks;

   /* check size! */
   alfs_FSdev = dev;

   /* GROK -- yuck! */
   initProtection ();
   setUser (HBRICENO_UID);

   alfs.fscap = fscap;
   alfs.size = size;
   assert (size > 0);

   initmetablks = alfs_initmetablks (dev, size);

   /* in addition to initializing allocation structures, this call to     */
   /* alfs_alloc_initDisk(...) will allocate a contiguous range of blocks */
   /* for the initial ALFS metadata.                                      */

   superblkno = alfs_alloc_initDisk (fscap, alfs_FSdev, size, initmetablks);
   alfs_superblkno = superblkno;
   blkno = superblkno + 1 + alfs_alloc_initmetablks(size);
   blkno = alfs_path_initDisk (fscap, alfs_FSdev, blkno, 2, 2);
   blkno = alfs_dinode_initDisk (fscap, alfs_FSdev, buf, blkno);
/*
printf ("size %d, cap %x, blkno %d\n", alfs.size, alfs.fscap, blkno);
*/
   alfs.rootDInodeNum = dinode->dinodeNum;

   alfs_dinode_setUid (dinode, uid);
   alfs_dinode_setGid (dinode, gid);
   alfs_dinode_setAccTime (dinode, time(NULL));
   alfs_dinode_setModTime (dinode, time(NULL));
   alfs_dinode_setCreTime (dinode, time(NULL));
   alfs_dinode_setType (dinode, INODE_DIRECTORY);
   alfs_dinode_setMask (dinode, (AXS_IXUSR | AXS_IRUSR | AXS_IWUSR | 
				 AXS_IRGRP | AXS_IXGRP |
				 AXS_IROTH | AXS_IXOTH));
   alfs_dinode_setLinkCount (dinode, 2);
   alfs_dinode_setGroupstart (dinode, 0);
   alfs_dinode_setGroupsize (dinode, 0);
   dirblkno = blkno + 1;
   alfs_dinode_setDirect (dinode, 0, dirblkno);
   alfs_path_initDirRaw (dinode, dinode, buf2);
   assert (alfs_alloc_allocBlock (alfs_FSdev, dirblkno) == 0);
   ALFS_DISK_WRITE (&buft, buf2, alfs_FSdev, dirblkno);
   ALFS_DISK_WRITE (&buft, buf, alfs_FSdev, blkno);
   alfs_alloc_unmountDisk (fscap, alfs_FSdev, (superblkno+1));
   ALFS_DISK_WRITE (&buft, ((char *) &alfs), alfs_FSdev, superblkno);
   alfs_superblkno = 0xFFFFFFFF;
   return (superblkno);
}


/* mount an on-disk file system */

void alfs_mountFS_setcache (uint dev, uint superblkno, int cachesize, buffer_t * (*cache_handleMiss)(block_num_t, block_num_t, int, int), void (*cache_flushEntry)(buffer_t *), void (*cache_writeBack)(buffer_t *, int)) {
    struct buf buft;
    int blkno;

    extern buffer_t *alfs_lastBuffer;
/*
printf ("mountFS: dev %d, superblkno %d\n", dev, superblkno);
*/
    alfs_FSdev = dev;
    alfs_superblkno = superblkno;

    ALFS_DISK_READ (&buft, ((char *) &alfs), alfs_FSdev, superblkno);
    alfs_FScap = alfs.fscap;

    alfs_buffer_initBufferCache (8, (cachesize/BLOCK_SIZE), cache_handleMiss, cache_flushEntry, cache_writeBack);
    blkno = alfs_alloc_mountDisk (alfs_FScap, alfs_FSdev, (superblkno+1), alfs.size);
    blkno = alfs_path_mountDisk (alfs_FScap, alfs_FSdev, blkno);
    alfs_dinode_mountDisk (alfs_FScap, alfs_FSdev, blkno);
    initProtection ();
    setUser (HBRICENO_UID);

    alfs_currentRootInode = alfs_inode_getInode (alfs.rootDInodeNum, alfs_FSdev, BUFFER_READ);
    alfs_currentRootBuffer = alfs_lastBuffer;
/*
    printf ("root is %d (%x), direct block ptr %d\n", alfs_currentRootInode->inodeNum, alfs_currentRootInode->inodeNum, alfs_inode_getDirect(alfs_currentRootInode,0));
*/
}


void alfs_mountFS (uint dev, uint superblkno) {
   alfs_mountFS_setcache (dev, superblkno, ALFS_BUFCACHE_SIZE, alfs_default_buffer_handleMiss, alfs_default_buffer_flushEntry, alfs_default_buffer_writeBack);
}


void alfs_unmountFS () {
   struct buf buft;
   int blkno;
   alfs_inode_releaseInode (alfs_currentRootInode, BUFFER_DIRTY);
   alfs_buffer_shutdownBufferCache ();
   assert (alfs_superblkno != 0xFFFFFFFF);
   blkno = alfs_alloc_unmountDisk (alfs_FScap, alfs_FSdev, (alfs_superblkno+1));
   blkno = alfs_path_unmountDisk (alfs_FScap, alfs_FSdev, blkno);
   alfs_dinode_unmountDisk (alfs_FScap, alfs_FSdev, blkno);
   assert (alfs.fscap == alfs_FScap);
   ALFS_DISK_WRITE (&buft, ((char *) &alfs), alfs_FSdev, alfs_superblkno);
   alfs_superblkno = 0xFFFFFFFF;
}


/* alfs_open

   Open a file. All we do is check permissions and set the file pointer and 
   flags.  If OPT_CREAT is specified, we create the file too.

   GROK -- should deal with current working directory vs. root here
*/

int alfs_open (char *path, int flags, umask_t mode) {
     inode_t *inode;
     inode_t *dirInode;
     int ret;

#ifdef PRINT_FDCALLS
printf("alfs_open: %s\n", path);
#endif

     /* first thing to do is figure out if we're creating a file or not */
     if (flags & OPT_CREAT) {
	  char *name;
          int nameLen;
          void *dirent;
          buffer_t *buffer;

	  /* ok, get the last component from the path which is the name
	     we're creating. */

          ret = alfs_path_traversePath (path, alfs_currentRootInode, &dirInode, &name, &nameLen, 0);
          if (ret != OK) {
             return (ret);
          }
          assert (dirInode);

          /* verify that we can write to the parent directory and get the
             proper write capability if we can. */

          if (!verifyWritePermission (alfs_inode_getMask(dirInode), uid, alfs_inode_getUid(dirInode), gid, alfs_inode_getGid(dirInode))) {
              alfs_inode_releaseInode (dirInode, 0);
              return (ALFS_EACCESS);              /* no write permission */
          }

          /* construct an "in-core" inode structure for the new file */

          inode = alfs_inode_makeInode (alfs_FSdev);

          /* prepare the directory for the new entry and check for collision */

          ret = alfs_path_prepareEntry (dirInode, name, nameLen, inode, &dirent, &buffer, 0, 1);
          if (ret != 0) {
/*
              alfs_inode_setLinkCount(inode, 0);
              alfs_inode_releaseInode (inode, BUFFER_INVALID);
*/
printf("File exists, current number %d\n", ret);
assert(0);
/*
              fileInodeBlock = ret;
*/
          } else {
              alfs_inode_setUid (inode, uid);
              alfs_inode_setGid (inode, gid);
              alfs_inode_setAccTime (inode, time(NULL));
              alfs_inode_setModTime (inode, time(NULL));
              alfs_inode_setCreTime (inode, time(NULL));
              alfs_inode_setMask (inode, mode);
              alfs_inode_setType (inode, INODE_FILE);
              alfs_inode_setGroupstart (inode, 0);
              alfs_inode_setGroupsize (inode, 0);
              alfs_inode_setLinkCount (inode, 1);
#ifdef ORDERING
              alfs_path_fillEntry (dirInode, dirent, inode, (char) 1, buffer, 1, 0);
#else
              alfs_path_fillEntry (dirInode, dirent, inode, (char) 1, buffer, 0, 0);
#endif
          }
			/* assume that inode has changed in an important way */
          alfs_inode_releaseInode (dirInode, BUFFER_DIRTY);
          inode->flags |= OPT_CREAT;
     } else {
	  /* lookup the file's inode */
/*
printf ("going to translate Name to Inode\n");
*/
	  ret = alfs_path_translateNameToInode (path, alfs_currentRootInode, &inode, (BUFFER_WITM | BUFFER_READ));
/*
printf ("back from translate Name to Inode: ret %d\n", ret);
*/
	  if (ret != OK) {
	       return (ret);
          }

          assert (inode);
     }
/*
printf("sys_open: path %s ==> inodeNum %d (%x)\n", path, inode->inodeNum, inode->inodeNum);
*/
     
     /* make sure this is really a data file and not a dir */
     if (alfs_inode_getType(inode) == INODE_DIRECTORY) {
	  alfs_inode_releaseInode (inode, 0);
	  return (ALFS_EISDIR);
     }

     /* check access permissions */
     if (flags & OPT_RDONLY) {
	  if (!verifyReadPermission (alfs_inode_getMask(inode), uid, alfs_inode_getUid(inode), gid, alfs_inode_getGid(inode))) {
printf ("read permission check failed\n");
	       return (ALFS_EACCESS);
	  }
	  inode->flags |= OPT_RDONLY;
     }
     if (flags & OPT_WRONLY) {
	  if (!verifyWritePermission (alfs_inode_getMask(inode), uid, alfs_inode_getUid(inode), gid, alfs_inode_getGid(inode))){
printf ("write permission check failed\n");
	       return (ALFS_EACCESS);
	  }
	  inode->flags |= OPT_WRONLY;
     }

     /* set file pointer to start of file */
     inode->fileptr = 0;
/*
printf ("exit alfs_open: alfs_currentRootInode %d, iuid %d, igid %d\n", alfs_currentRootInode->inodeNum, alfs_inode_getUid(alfs_currentRootInode), alfs_inode_getGid(alfs_currentRootInode));
*/
     return ((int) inode);
}


/* alfs_read

   Read from a file. Check that the file has been opened and that
   we have read permission. Updates the file pointer.
*/

int alfs_read (int fd, char *buf, int nbytes) {
     inode_t *inode = (inode_t *) fd;
     int ret;
/*
printf ("enter alfs_read: alfs_currentRootInode %d (%p), iuid %d, igid %d\n", alfs_currentRootInode->inodeNum, alfs_currentRootInode->dinode, alfs_inode_getUid(alfs_currentRootInode), alfs_inode_getGid(alfs_currentRootInode));
*/

     if (inode == NULL) {
	return (ALFS_EBADF);
     }

     /* make sure we've got the file open for reading */
     if (!(inode->flags & OPT_RDONLY)) {
	return (ALFS_EBADF);
     }

     /* do the read */
     ret = alfs_readBuffer (inode, buf, inode->fileptr, nbytes);
     if (ret < 0) {
	return (ret);
     }

     /* update the file position */
     inode->fileptr += ret;

     return (ret);
}


/* alfs_write

   Write data to a file. Check that the file has been opened and that
   we have write permission. Updates the file pointer. 
*/

int alfs_write (int fd, char *buf, int nbytes) {
     inode_t *inode = (inode_t *) fd;
     int ret;

     if (inode == NULL) {
        return (ALFS_EBADF);
     }

     /* make sure we've got the file open for writing */
     if (!(inode->flags & OPT_WRONLY)) {
        return (ALFS_EBADF);
     }

     /* do the write */
     ret = alfs_writeBuffer (inode, buf, inode->fileptr, nbytes);
     if (ret < 0) {
        return (ret);
     }

     /* update the file pointer */
     inode->fileptr += ret;
     alfs_inode_setModTime (inode, time(NULL));

     return (ret);
}


/* alfs_close

   Close a file. Just mark the desriptor as no-longer valid.
 */

int alfs_close (int fd) {
     inode_t *inode = (inode_t *) fd;

     if (inode == NULL) {
        return (ALFS_EBADF);
     }

#ifdef PRINT_FDCALLS
printf ("alfs_close: inodeNum %x, size %d, flags %x (blk[0] %d)\n", inode->inodeNum, alfs_inode_getLength (inode), inode->flags, inode->dinode->directBlocks[0]);
#endif

     alfs_inode_setAccTime (inode, time(NULL));
     if (inode->flags != OPT_RDONLY) {
        alfs_inode_setModTime (inode, time(NULL));
     }
     alfs_inode_releaseInode (inode, BUFFER_DIRTY);

     return (OK);
}


/* alfs_lseek

   Adjust the file pointer.
 */

int alfs_lseek (int fd, offset_t offset, int whence) {
     inode_t *inode = (inode_t *) fd;
     offset_t new;

     if (inode == NULL) {
        return (ALFS_EBADF);
     }

     switch (whence) {
	case SEEK_SET: new = offset; break;
	case SEEK_CUR: new = inode->fileptr + offset; break;
	case SEEK_END: new = alfs_inode_getLength(inode) + offset;
	default: return (ALFS_EINVAL);
     }

     if (new < 0) {
        return (ALFS_EINVAL);
     }
     inode->fileptr = new;

     return (inode->fileptr);
}


/* alfs_mkdir

 Create an empty directory and link it into it's parent.
 */

int alfs_mkdir (char *path, int mode) {
     char *name;
     int nameLen;
     inode_t *dirInode;
     int ret;
     inode_t fakeInode;
     inode_t *inode;
     void *dirent;
     buffer_t *buffer;

#ifdef PRINT_FDCALLS
printf ("alfs_mkdir: %s\n", path);
#endif

     ret = alfs_path_traversePath (path, alfs_currentRootInode, &dirInode, &name, &nameLen, 0);
     if (ret != OK) {
        return (ret);
     }
     assert (dirInode);

     if (alfs_inode_getType (dirInode) != INODE_DIRECTORY) {
        alfs_inode_releaseInode (dirInode, 0);
        return (ALFS_ENOTDIR);
     }

     /* verify that we can write to the parent directory and get the
        proper write capability if we can. */

     if (!verifyWritePermission (alfs_inode_getMask(dirInode), uid, alfs_inode_getUid(dirInode), gid, alfs_inode_getGid(dirInode))) {
        alfs_inode_releaseInode (dirInode, 0);
        return (ALFS_EACCESS);              /* no write permission */
     }

     /* link in the new directory */
/*
     ret = alfs_path_addLink (dirInode, name, nameLen, &inode, (char) 2, 0);
*/
     fakeInode.dinode = NULL;
     fakeInode.inodeNum = 0;
     ret = alfs_path_prepareEntry (dirInode, name, nameLen, &fakeInode, &dirent, &buffer, 0, 0);
     if (ret == 0) {

         /* create the directory itself and get a reference to it */
         inode = alfs_inode_makeInode (alfs_FSdev);
         assert (inode);
         inode->dinode = fakeInode.dinode;
         assert (inode->dinode);
         inode->inodeNum = fakeInode.inodeNum;

         alfs_inode_setUid (inode, uid);
         alfs_inode_setGid (inode, gid);
         alfs_inode_setAccTime (inode, time(NULL));
         alfs_inode_setModTime (inode, time(NULL));
         alfs_inode_setCreTime (inode, time(NULL));
         alfs_inode_setMask (inode, mode);
         alfs_inode_setLinkCount (inode, 2);
         alfs_inode_setType (inode, INODE_DIRECTORY);
         alfs_inode_setGroupstart (inode, 0);
         alfs_inode_setGroupsize (inode, 0);
         alfs_path_fillEntry (dirInode, dirent, inode, (char) 2, buffer, 1, 0);
         alfs_path_initDir (inode, dirInode, 0);
         alfs_inode_releaseInode (inode, BUFFER_DIRTY);
         alfs_inode_setLinkCount (dirInode, (alfs_inode_getLinkCount(dirInode)+1));
/*
     } else {
         alfs_inode_setLinkCount(inode, 0);
         alfs_inode_releaseInode (inode, BUFFER_INVALID);
*/
     } else {
         ret = ALFS_EEXIST;
     }
     alfs_inode_setModTime (dirInode, time(NULL));
     alfs_inode_releaseInode (dirInode, BUFFER_DIRTY);
     return  (ret);
}


/* alfs_unlink

   Remove anything. It slices, it dices, it deletes files. Hell, it'll
   even delete a directory 
*/

int alfs_unlink (char *path) {
    char *name;
    int nameLen;
    int ret;
    inode_t *dirInode;
    inode_t *inode;
    int flags = BUFFER_DIRTY;
    void *dirent;
    buffer_t *buffer;
    int dotdot = 0;
/*
printf ("alfs_unlink: %s\n", path);
*/
    ret = alfs_path_traversePath (path, alfs_currentRootInode, &dirInode, &name, &nameLen, 0);
    if (ret != OK) {
        return (ret);
    }
    assert (dirInode != NULL);

   if ((dirInode == alfs_currentRootInode) && (nameLen == 0)) {
      printf ("alfs_unlink: cannot remove root directory\n");
      return (ALFS_EINVAL);
   }

    /* verify that we can write to the parent directory and get the
       proper write capability if we can. */

    if (!verifyWritePermission (alfs_inode_getMask(dirInode), uid, alfs_inode_getUid(dirInode), gid, alfs_inode_getGid(dirInode))) {
        alfs_inode_releaseInode (dirInode, 0);
        return (ALFS_EACCESS);      /* no write permission */
    }

    inode = alfs_path_grabEntry (dirInode, name, nameLen, &dirent, &buffer);
    if (inode == NULL) {
        alfs_inode_releaseInode (dirInode, 0);
        return (ALFS_ENOENT);
    }

    if (alfs_inode_getType(inode) == INODE_DIRECTORY) {
       if (alfs_path_isEmpty(inode, &dotdot) == 0) {
          alfs_inode_releaseInode (dirInode, 0);
          alfs_inode_releaseInode (inode, 0);
          return (ALFS_ENOTEMPTY);
       }
    }

    assert (alfs_inode_getLinkCount(inode) > 0);
    alfs_inode_setLinkCount(inode, (alfs_inode_getLinkCount(inode) - 1));
    if ((alfs_inode_getLinkCount(inode) == 0) || ((alfs_inode_getType(inode) == INODE_DIRECTORY) && (alfs_inode_getLinkCount(inode) == 1))) {
       /* GROK -- is this the right thing to do with cache blocks */
       alfs_buffer_affectFile (alfs_FSdev, inode->inodeNum, (BUFFER_INVALID), 1);
       alfs_inode_truncInode (inode, 0);
       flags |= BUFFER_INVALID;
       if (dotdot) {
          assert (dotdot == dirInode->inodeNum);
          alfs_inode_setLinkCount (dirInode, (alfs_inode_getLinkCount(dirInode)-1));
       }
    }

#ifdef ORDERING
    alfs_path_removeEntry (dirInode, name, nameLen, dirent, inode, buffer, BUFFER_WRITE);
#else
    alfs_path_removeEntry (dirInode, name, nameLen, dirent, inode, buffer, BUFFER_DIRTY);
#endif

    alfs_inode_setModTime (dirInode, time(NULL));
    alfs_inode_releaseInode (dirInode, BUFFER_DIRTY);
#ifdef ORDERING
    alfs_inode_releaseInode (inode, (flags | BUFFER_WRITE));
#else
    alfs_inode_releaseInode (inode, (flags | BUFFER_DIRTY));
#endif
     
    return (OK);
}


/* Not quite the right behavior (should work for directories only), but not */
/* worth repreating the code in the short term */

int alfs_rmdir (char *path)
{
   return (alfs_unlink(path));
}


static void fillStatBuf (inode_t *inode, struct alfs_stat *buf) {

     assert (inode);

     buf->st_ino = inode->inodeNum;
     buf->st_mode = alfs_inode_getMask(inode);
     buf->st_uid = alfs_inode_getUid(inode);
     buf->st_gid = alfs_inode_getGid(inode);
     buf->st_size = alfs_inode_getLength(inode);
     buf->st_blksize = BLOCK_SIZE;
}


/* alfs_fstat 

   Return info about an open file.
*/

int alfs_fstat (int fd, struct alfs_stat *buf)
{
     inode_t *inode = (inode_t *) fd;

     if (inode == NULL) {
        return (ALFS_EBADF);
     }

     fillStatBuf (inode, buf);

     return (OK);
}


/* alfs_stat

   Return info about a filename.
 */

int alfs_stat (char *path, struct alfs_stat *buf) {
     inode_t *inode;
     int ret;
/*
printf ("alfs_stat: path %s, buf %p\n", path, buf);
*/
     ret = alfs_path_translateNameToInode (path, alfs_currentRootInode, &inode, 0);
     if (ret != OK) {
printf ("exit sys_stat: ret %d\n", ret);
        return (ret);
     }
     assert (inode);

     fillStatBuf (inode, buf);

     alfs_inode_releaseInode (inode, 0);
/*
printf ("exit alfs_stat: ret %d\n", OK);
*/
     return (OK);
}


/* alfs_chmod 

   Change the file permissions on a file. Must be the owner of the
   file to do this.

   XXX -- this doesn't follow unix semantics exactly: if a file
   is opened and then the file's permissions are changed the
   process with the open file see's the new permissions and not
   the old permissions.
*/

int alfs_chmod (char *path, int mode) {
     return (changeProtection (path, alfs_currentRootInode, mode));
}


void alfs_listDirectory (void *curRootInode)
{
   if (curRootInode == NULL) {
      curRootInode = (void *) alfs_currentRootInode;
   }
   alfs_path_listDirectory ((inode_t *) curRootInode);
}

