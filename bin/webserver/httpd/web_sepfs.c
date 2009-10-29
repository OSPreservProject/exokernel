
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


#include <stdio.h>
#include "web_general.h"
#include "web_fs.h"

#ifdef EXOPC
#include <xok/sys_ucall.h>
#endif
#include <xok/disk.h>
#define MAKEDISKDEV(a,b,c)	(b)
#define disk_poll()		getpid()

#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#if 1
#define DPRINTF(a,b)	if ((a) < 0) { printf b; }
#else
#define DPRINTF(a,b)
#endif

#include <exos/bufcache.h>

#ifdef EXOPC
#include <exos/vm-layout.h>
#else
#define PAGESIZ 4096
typedef unsigned int	cap_t;
#endif

int web_fs_diskreqs_outstanding = 0;

/* file system structure */

typedef struct {
    cap_t       fscap;
    u_int32_t	rootDInodeNum;
    int		size;
    char        space[(BLOCK_SIZE - sizeof(cap_t) - sizeof(u_int32_t) - sizeof(int))];
} alfs_t;

struct dinodeMapEntry_t {
    block_num_t dinodeBlockAddr;        /* address of the dinode block */
    unsigned int freeDInodes;           /* free map for dinodes in block */
};
typedef struct dinodeMapEntry_t dinodeMapEntry_t;

#define ALFS_DINODE_SIZE                128
#define ALFS_DINODES_PER_BLOCK  (BLOCK_SIZE / ALFS_DINODE_SIZE)
#define ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK    (BLOCK_SIZE / sizeof(dinodeMapEntry_t))
#define ALFS_DINODEMAPBLOCKS    1
#define ALFS_MAX_DINODECOUNT    (ALFS_DINODEMAPBLOCKS * ALFS_DINODES_PER_BLOCK * ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK)

static dinodeMapEntry_t web_fs_dinodeMap[ (ALFS_DINODEMAPBLOCKS * ALFS_DINODEBLOCKS_PER_DINODEMAPBLOCK) ];

#define MAX_FILE_SIZE ((NUM_DIRECT + NUM_SINGLE_INDIRECT)*BLOCK_SIZE)
#define fileOffsetToBlockOffset(off) ((off)%BLOCK_SIZE)


/* on disk directory entry layout information */

        /* NOTE: SECTOR_SIZE (below) must evenly divide BLOCK_SIZE (external) */
        /*       that is, assert ((SS <= BS) && ((BS % SS) == 0));            */

#define ALFS_DIR_SECTOR_SIZE    DEV_BSIZE     /* directory block "sector" size */

        /* NOTE: SECTOR_SIZE (above) must exceed the maximum directory entry */
        /*       size, which includes the maximum name size (below), the     */
        /*       basic dirent size and any directory entry assumptions (eg,  */
        /*       inclusion of a blank space at end of name, rounding up to   */
        /*       convenient size, etc...)                                    */

#define ALFS_DIR_MAX_FILENAME_LENGTH    255     /* max file name */

struct dirent {
     u_int32_t  inodeNum;       /* inode number of directory entry */
     u_int16_t  entryLen;       /* length of directory entry (in bytes) */
     u_int8_t   status;         /* status info for the directory entry */
     u_int8_t   nameLen;        /* length of link name */
     char       name[4];        /* link name */
};

typedef struct dirent dirent_t;

        /* NOTE: SIZEOF_DIRENT_T (below) must be >= sizeof(dirent_t) */

#define SIZEOF_DIRENT_T 12


block_num_t alfs_FSdev = 23;
dinode_t *alfs_currentRootInode = NULL;
static bufferCache_t *web_fs_localCache = NULL;

buffer_t *web_fs_waitee = NULL;

#define ALLOCATE_MEMORY_PAGE() alloc_phys_page();
#define DEALLOCATE_MEMORY_PAGE(ptr) free_phys_page(ptr);

static buffer_t *sys_buffer_handleMiss (block_num_t dev, block_num_t inodeNum, int block, int flags);
static void sys_buffer_flushEntry (buffer_t *entry);
static void sys_buffer_writeBack (buffer_t *entry, int async);

static void ALFS_DISK_READ (struct buf *buf, char *memaddr, uint fsdev, uint blkno);
static void ALFS_DISK_WRITE (struct buf *buf, char *memaddr, uint fsdev, uint blkno);


/* allocate a new buffer */
static bufferHeader_t *newBuf (void *entry)
{
   buffer_t *new = malloc (sizeof (buffer_t));
   assert (new != NULL);
   bzero ((char*)new, sizeof(buffer_t));
   new->buffer = NULL;
   return((bufferHeader_t *) new);
}


/* Find a buffer in the buffer cache. If the buffer doesn't exists,
   allocate it. Tell the user whether the buffer contains valid data
   or not. We acuire the buffer with the desired capability (read only
   or read-write). */

buffer_t *web_fs_buffer_getBlock (block_num_t dev, block_num_t inodeNum, int block, int flags)
{
    buffer_t *entry;

DPRINTF (4, ("web_fs_buffer_getBlock: dev %d, inodeNum %d, block %d, flags %x, ", dev, inodeNum, block, flags));

    assert(inodeNum);
    entry = (buffer_t *) alfs_buffertab_findEntry (web_fs_localCache, dev, inodeNum, block, FIND_ALLOCATE);

    if (entry) {

DPRINTF (4, ("hit\n"));

        if (entry->flags & BUFFER_READING) {
           web_fs_waitee = entry;
        }
     } else {

DPRINTF (4, ("miss\n"));

         /* find a slot in the buffer table to hold the new buffer */
         entry = sys_buffer_handleMiss (dev, inodeNum, block, flags);
     }

     /* remember that someone is using this buffer */
     if (entry) {
        entry->inUse++;

        if (entry->flags & BUFFER_READING) {
           entry = NULL;
        }
     }

     return (entry);
}


/* say we're done with a buffer */

void web_fs_buffer_releaseBlock (buffer_t *entry, int flags) {
     int block = entry->header.block;
     int inodeNum = entry->header.inodeNum;
     assert (entry);

     assert(entry == (buffer_t *)alfs_buffertab_findEntry (web_fs_localCache, entry->header.dev, inodeNum, block, FIND_VALID));

DPRINTF (4, ("web_fs_buffer_releaseBlock: dev %d, inodeNum %d, block %d, flags %x, inUse %d\n", entry->header.dev, inodeNum, block, flags, entry->inUse));

     if (flags & BUFFER_WRITE) {
         sys_buffer_writeBack (entry, (flags & BUFFER_ASYNC));
         entry->flags &= ~BUFFER_DIRTY;
     } else if (flags & BUFFER_DIRTY) {
         entry->flags |= BUFFER_DIRTY;
     }

     entry->inUse--;
     assert (entry->inUse >= 0);

     /* if no one is using the buffer, put it on the free list */
     if (entry->inUse == 0) {
         alfs_buffertab_markFree(web_fs_localCache, (bufferHeader_t *) entry);
     }
}


static void affectBuffer (bufferCache_t *cache, bufferHeader_t *entryHeader, int flags)
{
   buffer_t *entry = (buffer_t *) entryHeader;

DPRINTF (4, ("AFFECTBUFFER: dev %x, inodeNum %d, block %d, flags %x\n", entry->header.dev, entry->header.inodeNum, entry->header.block, flags));

   if ((flags & BUFFER_WRITE) && (entry->flags & BUFFER_DIRTY)) {
      sys_buffer_writeBack (entry, (flags & BUFFER_ASYNC));
      entry->flags &= ~BUFFER_DIRTY;
   }
   if (flags & BUFFER_INVALID) {
      sys_buffer_flushEntry (entry);
      alfs_buffertab_markInvalid (cache, entryHeader);
      entry->buffer = NULL;
      entry->flags &= ~BUFFER_DIRTY;
      entry->header.inodeNum = 0;
   }
}


/* act on all cache entries for a file */

void web_fs_buffer_affectFile (block_num_t dev, block_num_t inodeNum, int flags, int notneg)
{
   alfs_buffertab_affectFile (web_fs_localCache, dev, inodeNum, affectBuffer, flags, notneg);
}


/* tell the cache server we're no longer interested in a block */

static int dumpBlock (void *e) {
     buffer_t *entry = (buffer_t *)e;

DPRINTF (4, ("dumpBlock: inodeNum %d, block %d, isDirty %d\n", entry->header.inodeNum, entry->header.block, (entry->flags & BUFFER_DIRTY)));

     if (entry->flags & BUFFER_DIRTY) {
         sys_buffer_writeBack (entry, 0);
         entry->flags &= ~BUFFER_DIRTY;
     }
     sys_buffer_flushEntry (entry);
     entry->buffer = NULL;
     entry->header.inodeNum = 0;
     return  (OK);
}


/* write out any dirty data in the cache and free all memory */

void web_fs_buffer_shutdownBufferCache () {
    buffertab_shutdown (web_fs_localCache);
}


void web_fs_buffer_printBlock (bufferHeader_t *bufHead)
{
    buffer_t *buffer = (buffer_t *) bufHead;
    printf ("buf %x, dev %x, inodeNum %d, block %d, inUse %d\n", (u_int)buffer, buffer->header.dev, buffer->header.inodeNum, buffer->header.block, buffer->inUse);
}

 
void web_fs_buffer_printCache ()
{
    alfs_buffertab_printContents (web_fs_localCache, web_fs_buffer_printBlock);
}


/* web_fs_path_getNextPathComponent

   identify the leftmost component of a pathname and return it

   path is the pathname
   compLen holds length of the pathname component

   returns ptr to first character of the component
 */

char *web_fs_path_getNextPathComponent (char *path, int *compLen)
{
     char *start;
     int count = 0;

     /* strip leading slashes, if any */
     while ((*path == '/') && (*path != (char )NULL)) {
         path++;
     }
     start = path;

     /* copy over chars to next */
     while ((*path != '/') && (*path != (char )NULL)) {
          path++;
          count++;
     }
     *compLen = count;

     return (start);
}


/**************************** web_fs_lookupname *************************/
/*									*/
/* scan the directory for "name" and return the corresponding inodeNum	*/
/*    value if found.  Otherwise, return -1.				*/
/*									*/
/* Parameters upon entry:						*/
/*    fsdev -- the file system identifier used for buffer acquisition	*/
/*    dirInode -- the inode for the directory				*/
/*    name -- the name being looked up					*/
/*    namelen -- length of the name (in characters)			*/
/*									*/
/* Parameters upon exit:  same as entry.				*/
/*									*/
/* Changes to state: buffer entries for directory blocks may be		*/
/*                   acquired (will be released before exit, with no	*/
/*                   changes made). 					*/
/*									*/
/************************************************************************/

int web_fs_lookupname (block_num_t fsdev, dinode_t *dirInode, char *name, int namelen, int flags) {
     int currPos = 0;
     int dirBlockLength;
     int dirBlockNumber = 0;
     dirent_t *dirent;
     buffer_t *dirBlockBuffer;
     char *dirBlock;
     int i;

     assert (dirInode);

     /* search through the directory looking for the name */

     while (currPos < dirInode->length) {
         dirBlockLength = min((dirInode->length - currPos), BLOCK_SIZE);
         dirBlockBuffer = web_fs_buffer_getBlock (fsdev, dirInode->dinodeNum, dirBlockNumber, flags);
         if (dirBlockBuffer == NULL) {
            return (0);
         }
         dirBlockNumber++;
         dirBlock = dirBlockBuffer->buffer;
         for (i=0; i < dirBlockLength; i += dirent->entryLen) {
             dirent = (dirent_t *) (dirBlock + i);
             if ((namelen == dirent->nameLen) && (name[0] == dirent->name[0]) &&
                 (dirent->inodeNum) && (bcmp(dirent->name, name, dirent->nameLen) == 0)) {
                  /* name found */
                  web_fs_buffer_releaseBlock (dirBlockBuffer, 0);
                  return(dirent->inodeNum);
             }
         }
         web_fs_buffer_releaseBlock (dirBlockBuffer, 0);
         currPos += dirBlockLength;
     }

     return (-1);               /* name not found */
}


/* web_fs_path_translateNameToInode

   Convert a pathname to an inode for the file.

   UPON ENTRY:
   path is path to lookup
   startPoint is inode of where to start following path

   UPON RETURN:
   return value contains error status
   startPoint contains inode number corresponding to the name
 */

dinode_t * web_fs_translateNameToInode (char *path, block_num_t fsdev, dinode_t *startInode, int flags) {
     char *next = path;
     int len;
     int inodeNumber = startInode->dinodeNum;
     dinode_t *inode;

     DPRINTF (3, ("web_fs_translateNameToInode: path %s\n", path));

     /* get the name of the first component to lookup */
     if (path[0] != (char) NULL) {
        path = web_fs_path_getNextPathComponent (next, &len);
        next = path + len;
     }

     /* loop through all directory entries looking for the next
        element of our path. */

     while (*path) {

          DPRINTF (4, ("working on path: %s\n", path));

          inode = (inodeNumber == startInode->dinodeNum) ? startInode :
                  web_fs_getDInode (inodeNumber, fsdev, BUFFER_READ);
          assert(inode);

          /* look for the name in the directory */
          inodeNumber = web_fs_lookupname (fsdev, inode, path, len, BUFFER_READ);

          if (inode != startInode) {
              web_fs_releaseDInode (inode, fsdev, 0);
          }

          if (inodeNumber < 0) {
               return (NULL);
          }

          /* get the name of the next component to lookup */
          path = web_fs_path_getNextPathComponent (next, &len);
          next = path + len;
     }

     inode = web_fs_getDInode (inodeNumber, fsdev, ((flags & ~BUFFER_ASYNC) | BUFFER_READ));

     return (inode);
}


void web_fs_dinode_mountDisk (block_num_t fsdev, int blkno)
{
   struct buf buft;
   int i;
   for (i=0; i<ALFS_DINODEMAPBLOCKS; i++) {
      ALFS_DISK_READ (&buft, ((char *) web_fs_dinodeMap + (i * BLOCK_SIZE)), fsdev, (i+blkno));
   }
}


dinode_t *web_fs_getDInode (block_num_t dinodeNum, block_num_t fsdev, int flags)
{
    int dinodeBlockNo = dinodeNum / ALFS_DINODES_PER_BLOCK;
    int dinodeInBlock = dinodeNum % ALFS_DINODES_PER_BLOCK;
    dinodeMapEntry_t *dinodeMapEntry;
    block_num_t dinodeBlockAddr;
    buffer_t *dinodeBlockBuffer;

DPRINTF (4, ("web_fs_getDInode: dinodeNum %d, flags %x\n", dinodeNum, flags));

    assert (dinodeNum > 0);
    assert (dinodeNum < ALFS_MAX_DINODECOUNT);

    dinodeMapEntry = &web_fs_dinodeMap[dinodeBlockNo];
    assert ((dinodeMapEntry->freeDInodes & (1 << dinodeInBlock)) == 0);
    dinodeBlockAddr = dinodeMapEntry->dinodeBlockAddr;

DPRINTF (4, ("web_fs_getDInode midpoint: dinodeBlockNo %d, dinodeBlockAddr %d, dinodeInBlock %d\n", dinodeBlockNo, dinodeBlockAddr, dinodeInBlock));

    dinodeBlockBuffer = web_fs_buffer_getBlock (fsdev, 1, dinodeBlockAddr, flags);
    if (dinodeBlockBuffer == NULL) {
       return (NULL);
    }
    assert (((dinode_t *) dinodeBlockBuffer->buffer + dinodeInBlock)->dinodeNum == dinodeNum);
    return ((dinode_t *) dinodeBlockBuffer->buffer + dinodeInBlock);
}


int web_fs_releaseDInode (dinode_t *dinode, block_num_t fsdev, int flags)
{
    int dinodeNum = dinode->dinodeNum;
    int dinodeBlockNo = dinodeNum / ALFS_DINODES_PER_BLOCK;
    int dinodeInBlock = dinodeNum % ALFS_DINODES_PER_BLOCK;
    dinodeMapEntry_t *dinodeMapEntry;
    buffer_t *dinodeBlockBuffer;
    block_num_t dinodeBlockAddr;

DPRINTF (4, ("web_fs_releaseDInode: dinodeNum %d, flags %x, refCount %d\n", dinodeNum, flags, dinode->refCount));

    assert (dinodeNum > 0);
    assert (dinodeNum < ALFS_MAX_DINODECOUNT);

    dinodeMapEntry = &web_fs_dinodeMap[dinodeBlockNo];
    assert ((dinodeMapEntry->freeDInodes & (1 << dinodeInBlock)) == 0);
    dinodeBlockAddr = dinodeMapEntry->dinodeBlockAddr;

    dinodeBlockBuffer = web_fs_buffer_getBlock (fsdev, 1, dinodeBlockAddr, BUFFER_READ);
    assert (dinodeBlockBuffer);
    web_fs_buffer_releaseBlock (dinodeBlockBuffer, 0);
    web_fs_buffer_releaseBlock (dinodeBlockBuffer, flags);
    return (OK);
}


/* web_fs_offsetToBlock

   convert an offset into a file into a block number, given an dinode.
   This should only be called for offset < MAX_FILE_SIZE

   dinode is the buffer containing the dinode
   offset is the offset into file

   returns block number or 0 if attempting to read from hole in file

 */

block_num_t web_fs_offsetToBlock (dinode_t *dinode, block_num_t fsdev, unsigned offset, int flags) {
     buffer_t *indirectBlock;
     int *indirectPtrs;
     block_num_t blockNum;

DPRINTF (4, ("web_fs_offsetToBlock: dinodeNum %d, offset %d\n", dinode->dinodeNum, offset));

     assert (offset < MAX_FILE_SIZE);

     /* see if the offset is mapped to a direct block */
     if (offset < BLOCK_SIZE*NUM_DIRECT) {
          int directBlockNumber = offset / BLOCK_SIZE;
          blockNum = dinode->directBlocks[directBlockNumber];
     } else {
         /* have to read indirect block. */
          if (dinode->singleIndirect == 0) {
              return (0);       /* messy -- not allocated and we're reading */
          } else {
              /* read in indirect block */
              indirectBlock = web_fs_buffer_getBlock (fsdev, dinode->dinodeNum, -dinode->singleIndirect, (flags | BUFFER_READ));
              if (indirectBlock == NULL) {
                 return (0xFFFFFFFF);
              }
          }

          /* ok, lookup block in indirect block */

          offset -= BLOCK_SIZE * NUM_DIRECT;
          indirectPtrs = (int *) indirectBlock->buffer;
          blockNum = indirectPtrs[(offset / BLOCK_SIZE)];
          web_fs_buffer_releaseBlock (indirectBlock, 0);
     }

DPRINTF (4, ("web_fs_offsetToBlock return: blockNum %d\n", blockNum));

    return (blockNum);
}


/* temporarily located here, while in transition */

/* get a new buffer for a block */

static int web_fs_inode_getNumContig(dinode_t *inode, uint dev, int block)
{
   int i;
   int diskBlock = web_fs_offsetToBlock (inode, dev, (block*BLOCK_SIZE), 0);
   for (i=1; i<16; i++) {
      int db = web_fs_offsetToBlock (inode, dev, ((block+i)*BLOCK_SIZE), 0);
      if (db != (diskBlock + i)) {
         return (i-1);
      }
   }
   return (16);
}


static buffer_t *sys_buffer_handleMiss (block_num_t dev, block_num_t inodeNum, int block, int flags) {
     dinode_t *inode = NULL;
     buffer_t *entry = NULL;
     block_num_t diskBlock = 0;
     struct bc_entry *bc_entry;
     int allocated = 0;
     int rangesize = 1;
     int rangestart;
/*
printf("alfs_buffer_handleMiss: buf %p, dev %d, inodeNum %d, block %d, flags %x\n", entry, dev, inodeNum, block, flags);
*/
     entry = (buffer_t *)alfs_buffertab_getEmptyEntry (web_fs_localCache, dev, inodeNum, block);
     if (entry == NULL) {
         web_fs_buffer_printCache ();
     }
     assert (entry);

/*
     assert (entry->buffer == NULL);
*/
     assert (block != -1);
     entry->bc_entry = NULL;
     entry->flags = 0;  /* not BUFFER_RDONLY */

     entry->buf.b_resid = -1;

     entry->header.dev = dev;
     entry->header.inodeNum = inodeNum;
     entry->header.block = block;
     entry->inUse = 0;

     if ((block < 0) || (inodeNum == 1)) {
        assert (block);
        if (block < 0) {
           diskBlock = -block;
        } else {
           diskBlock = block;
        }

     } else {   /* file block */
        buffer_t *entry2;
        inode = web_fs_getDInode (inodeNum, dev, BUFFER_READ);
        diskBlock = web_fs_offsetToBlock (inode, dev, (block * BLOCK_SIZE), flags);
        /* GROK -- sledge hammer that breaks clean interfaces yet again */
      if ((entry2 = (buffer_t *)alfs_buffertab_findEntry (web_fs_localCache, dev, 1, diskBlock, FIND_ALLOCATE)) != NULL) {
assert (0);
#if 0
         entry = entry2;
         assert (inode->type == INODE_DIRECTORY);
         assert (entry->bc_entry->buf_blk == entry->header.diskBlock);
/*
         kprintf ("%d: rename: ino %x, blkno %d ---> ino %x, blkno %d (entry %p)\n", getpid(), entry->header.inodeNum, entry->header.block, inodeNum, block, entry);
*/
         alfs_buffertab_renameEntry (alfs_localCache, (bufferHeader_t *)entry, dev, inodeNum, block);
         assert (alfs_buffertab_findEntry (alfs_localCache, dev, 1, diskBlock, FIND_VALID) == NULL);
         assert ((entry->header.inodeNum == inodeNum) && (entry->header.block ==
 block));
         web_fs_releaseDInode (inode, dev, 0);
#endif
         return (entry);
      }

     }

     entry->header.diskBlock = diskBlock;

default_buffer_handleMiss_retry:

     if (diskBlock == 0) {
        /* diskBlock will be 0 only if this is a hole in the file.  Just    */
        /* fill is with zeroes.                                             */
        /* GROK -- this currently can cause a consistency problem if cached */
        /* locally.                                                         */

        kprintf ("fix the locally cached hole problem in ALFS, bozo\n");
        assert (0);
        bzero (entry->buffer, BLOCK_SIZE);

   } else if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) != NULL) {
        /* found the desired block in the bufcache */

      entry->bc_entry = bc_entry;
      entry->buffer = exos_bufcache_map (bc_entry, dev, diskBlock, 1);

      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }
      assert (bc_entry->buf_blk == diskBlock);

//kprintf ("found it (diskBlock %d), now wait for it (%p, ppn %d)\n", diskBlock, entry->buffer, PGNO(vpt[PGNO(entry->buffer)]));

      exos_bufcache_waitforio (bc_entry);

        /* Make sure we don't get old crap (if allocating new). */
      if ((allocated) && ((flags & BUFFER_READ) || (inode == NULL) || (inode->type == INODE_DIRECTORY))) {
         bzero (entry->buffer, BLOCK_SIZE);
      }

   } else if ((allocated) || !(flags & BUFFER_READ)) {
        /* Newly allocated block or block whose contents will be completely */
        /* overwritten.  No disk read is needed; instead, insert it into    */
        /* cache, and bzero it if contents will be used (ie, READ).         */

      int zerofill = (flags & BUFFER_READ) || (inode == NULL) || (inode->type == INODE_DIRECTORY);
      entry->buffer = (char *) exos_bufcache_alloc (dev, diskBlock, zerofill, 1,
 0);
      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }
      if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) == NULL) {
         kprintf ("lookup failed after `successful' alloc\n");
         assert (0);
      }
      entry->bc_entry = bc_entry;

//kprintf ("allocate for zeros: %p (ppn %d), inodeNum %x, block %d, diskBlock %d\n", entry->buffer, PGNO(vpt[PGNO(entry->buffer)]), inodeNum, block, diskBlock);

   } else if (flags & BUFFER_READ) {
        /* gonna do a disk read... */

//kprintf("diskread: inodeNum %x, block %d, diskBlock %d\n", inodeNum, block, diskBlock);

        /* first, determine boundaries for disk read (remember: clustering */
        /* and large disk I/Os are good things!)                           */

      rangestart = diskBlock;

#ifndef NOSCATTER
      if (inode != NULL) {
         if ((block == 0) && (inode->groupsize > 1)) {
            rangesize = inode->groupsize;
            rangestart = inode->groupstart;
         } else {
            rangesize = 1 + web_fs_inode_getNumContig(inode, dev, block);
         }
         rangesize = min (16, rangesize);
      }
#endif
      assert ((diskBlock >= rangestart) && (diskBlock < (rangestart + rangesize)));

        /* figure out the two end points and then generate request sequence... */
      {
      int firstblock = diskBlock;
      int lastblock = diskBlock;
      while (((firstblock-1) >= rangestart) && (exos_bufcache_lookup(dev, (firstblock-1)) == NULL)) {
         firstblock--;
      }
      while (((lastblock+1) < (rangestart + rangesize)) && (exos_bufcache_lookup(dev, (lastblock+1)) == NULL)) {
         lastblock++;
      }

//kprintf ("Using read clustering: inodeNum %x, block %d, contig %d\n", inodeNum, block, (lastblock - firstblock + 1));

        /* now, initiate the actual disk read */

      //alfs_diskreads++;
      exos_bufcache_initfill (dev, firstblock, (lastblock-firstblock+1), &entry->buf.b_resid);
      if ((bc_entry = exos_bufcache_lookup (dev, diskBlock)) == NULL) {
         kprintf ("Didn't really initiate request\n");
         assert (0);
      }
      entry->bc_entry = bc_entry;
      entry->buffer = exos_bufcache_map (bc_entry, dev, diskBlock, 1);
      if (entry->buffer == NULL) {
         goto default_buffer_handleMiss_retry;
      }

      exos_bufcache_waitforio (bc_entry);
      }

   } else {
      kprintf ("alfs_defaultcache_handleMiss: this case shouldn't happen...\n");
      assert (0);
   }

   if (inode != NULL) {
      web_fs_releaseDInode (inode, dev, 0);
   }

   if (allocated) {
#ifdef USE_BC_DIRTY
      if (!entry->bc_entry->buf_dirty) {
         int ret = exos_bufcache_setDirty (dev, diskBlock, 1);
         assert (ret == 0);
      }
#else
      entry->flags |= BUFFER_DIRTY;
#endif
   }

     return (entry);
}


static void sys_buffer_flushEntry (buffer_t *entry)
{
   /* and unmap the block from our address space */
   exos_bufcache_unmap (entry->header.dev, entry->header.diskBlock, entry->buffer);
   entry->buffer = NULL;
}


/* NOTE: this has not been set up to really work with the webserver!!! */

static void sys_buffer_writeBack (buffer_t *entry, int async)
{
assert (0);
/*
printf("web_fs_buffer_writeBack: inodeNum %d, block %d, diskBlock %d\n", entry->header.inodeNum, entry->header.block, entry->header.diskBlock);
*/
}


static void ALFS_DISK_READ (struct buf *buf, char *memaddr, uint fsdev, uint blkno)
{
   struct bc_entry *bc_entry;

restart:

   if ((bc_entry = exos_bufcache_lookup(fsdev, blkno)) != NULL) {
      char *contents = exos_bufcache_map (bc_entry, fsdev, blkno, 0);
      if (contents == NULL) {
         goto restart;
      }
      exos_bufcache_waitforio (bc_entry);
      bcopy (contents, memaddr, BLOCK_SIZE);
      exos_bufcache_unmap (fsdev, blkno, contents);
   } else {
      exos_bufcache_initfill (fsdev, blkno, 1, NULL);
      goto restart;
   }
}


static void ALFS_DISK_WRITE (struct buf *buf, char *memaddr, uint fsdev, uint blkno)
{
   struct bc_entry *bc_entry;

restart:

   if ((bc_entry = exos_bufcache_lookup(fsdev, blkno)) != NULL) {
      char *contents = exos_bufcache_map (bc_entry, fsdev, blkno, 1);
      if (contents == NULL) {
         goto restart;
      }
      exos_bufcache_waitforio (bc_entry);
      bcopy (memaddr, contents, BLOCK_SIZE);
      exos_bufcache_setDirty (fsdev, blkno, 1);
      exos_bufcache_initwrite (fsdev, blkno, 1, NULL);
      exos_bufcache_unmap (fsdev, blkno, contents);
   } else {
      exos_bufcache_alloc (fsdev, blkno, 0, 1, 0);
      goto restart;
   }
}


/* just check if done (for now) */

int web_fs_isdone (buffer_t *waitee)
{
   assert (waitee != NULL);
   assert (waitee->inUse > 0);

   if (waitee->buf.b_resid == 0) {
      return (0);
   }

DPRINTF (4, ("web_fs_isdone: request is completed. inodeNum %d, blkno %d, resid %d, flags %x\n", waitee->header.inodeNum, waitee->header.block, waitee->buf.b_resid, (uint)waitee->buf.b_flags));

   if (waitee->flags & (BUFFER_WRITING | BUFFER_READING)) {
      waitee->flags &= ~(BUFFER_WRITING | BUFFER_READING);
      web_fs_diskreqs_outstanding--;
      assert (web_fs_diskreqs_outstanding >= 0);
   }
   web_fs_buffer_releaseBlock (waitee, 0);

   return (1);
}


block_num_t web_fs_alloc_mountDisk (block_num_t blkno, int size)
{
   int blockcount = (size+BLOCK_SIZE-1) / BLOCK_SIZE;
   /* don't bother reading in allocationMap, since no allocation will be done */
   return (blkno + ((blockcount+BLOCK_SIZE-1)/BLOCK_SIZE));
}


/* mount an on-disk file system */

void mountFS (char *devname2)
{
   struct buf buft;
   alfs_t alfs;
   int blkno;
   uint devno, superblkno;
   char devname[20];
   FILE *fp;
   int ret, val;

printf ("mountFS: devname %s\n", devname);

   devno = atoi(devname2);
   sprintf (devname, "/webfs%d", devno);
   fp = fopen (devname, "r");
   assert (fp != NULL);
   fscanf (fp, "ALFS file system definition\n");
   ret = fscanf (fp, "devno: %d\n", &val);
   assert ((ret == 1) && (val == devno));
   ret = fscanf (fp, "superblkno: %d\n", &val);
   assert ((ret == 1) && (val > 0));
   superblkno = val;

   alfs_FSdev = devno;

   ALFS_DISK_READ (&buft, ((char *) &alfs), alfs_FSdev, superblkno);

   assert (sizeof(dinode_t) == ALFS_DINODE_SIZE);

   web_fs_localCache = buffertab_init (8, 1024, newBuf, dumpBlock);
   blkno = web_fs_alloc_mountDisk ((superblkno+1), alfs.size);
printf ("blkno %d, size %d\n", blkno, alfs.size);
   web_fs_dinode_mountDisk (alfs_FSdev, blkno);

printf ("about to get root inode\n");
   alfs_currentRootInode = web_fs_getDInode (alfs.rootDInodeNum, alfs_FSdev, BUFFER_READ);
   printf ("root is %d, direct block ptr %d\n", alfs_currentRootInode->dinodeNum, alfs_currentRootInode->directBlocks[0]);
}

