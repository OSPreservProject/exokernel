
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


#include "cffs.h"
#include "cffs_buffer.h"
#include "cffs_dinode.h"
#include "cffs_alloc.h"
#include "cffs_embdir.h"
#include "cffs_xntypes.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>

#include <ubb/xn.h>
#include <ubb/kernel/virtual-disk.h>

#ifdef JJ
#define __malloc	malloc
#define __free		free
#else
#include <exos/mallocs.h>	/* for __malloc and such */
#include <xok/kerrno.h>
#endif


	/* GROK - I don't understand what this is doing here... */
dinode_t*
cffs_dinode_create(dinode_t* ino, da_t da) {
  ino->dinodeNum = (db_t)(da/128);
  return ino;
}


void cffs_dinode_initDInode (dinode_t *dinode, u_int dinodeNum, u_int type)
{
   StaticAssert (sizeof(dinode_t) == CFFS_DINODE_SIZE);
#ifndef CFFS_PROTECTED
   bzero ((char *) dinode, CFFS_DINODE_SIZE);
   dinode->memory_sanity = CFFS_DINODE_MEMORYSANITY;
   dinode->type = type;
   dinode->linkCount = (cffs_dinode_isDir(dinode)) ? 2 : 1;
   dinode->dinodeNum = dinodeNum;
#else
   assert (0);
#endif
}

dinode_t *cffs_dinode_getDInode (block_num_t dinodeNum, block_num_t fsdev, int flags, buffer_t **dinodeBlockBufferP)
{
   dinode_t *dinode;
   uint dirBlkno = cffs_inode_blkno(dinodeNum);
   uint dirDinodeNum;

   /*  we don't know the inode number to associate this block with, so
    *  we just use 1 for now. The only other entries using 1 should be
    *  for the allocation maps and superblocks, none of which should
    *  be using the same blocknumber as a dinode so this shouldn't
    *  accidentally reference some other block. 
    */
     
   *dinodeBlockBufferP = cffs_buffer_getBlock (fsdev, 0, 2, dirBlkno, BUFFER_MISSONLY | BUFFER_READ, NULL);
   if (*dinodeBlockBufferP == NULL) {
     kprintf ("blk = %d dinodeNum = %d\n", dirBlkno, dinodeNum);
     assert (0);
   }
   assert (*dinodeBlockBufferP);

   dinode = (dinode_t *) ((*dinodeBlockBufferP)->buffer + cffs_inode_offsetInBlock (dinodeNum));
   if (dinode->dinodeNum != dinodeNum) {
      kprintf ("mismatching dinodeNums: %x != %x (sane %d)\n", dinode->dinodeNum, dinodeNum, cffs_dinode_sanityOK(dinode));
   }
   assert (dinode->dinodeNum == dinodeNum);
   dirDinodeNum = dinode->dirNum;

   /* XXX I believe that if we're dealing with an entry that has
    *  multiple links pointing to it, we do not have a valid parent
    *  dinode num. I'm not sure about this... 
    */

   assert (dirDinodeNum);
   
   /*  we now know what inode number this block should be listed under
    *  (-dirDinodeNum).  We have two cases: either this block already
    *  exists under the correct name or it does not. We try to rename
    *  our tmp buffer to the correct desired name.
    *
    *  If the name didn't exist, rename will do its thing and we'll be
    *  all set. If the name did exist, then rename will return an
    *  error, but we'll still be all set.
    */

   /* XXX (assuming that (*dinodeBlockBufferP)->buffer points to the
    *  buffer of the existing entry which will be trus as long as we
    *  map buffers based on their physical page no).  
    */

   if (cffs_buffer_renameEntry (*dinodeBlockBufferP, fsdev, -dirDinodeNum, dirBlkno) != 0) {
     /* hmmm, we're aliasing something in the cache. Need to be carefull
	in dropping our reference so we don't unmap the buffer (which would unmap
	it for both of us). */

     cffs_buffer_markInvalid (*dinodeBlockBufferP);

     /* now get a reference to the other buffer */
     *dinodeBlockBufferP = cffs_buffer_getBlock (fsdev, 0, -dirDinodeNum, dirBlkno, BUFFER_HITONLY | BUFFER_READ, NULL);
     assert (*dinodeBlockBufferP);
   }

   return (dinode);
}


int cffs_dinode_releaseDInode (dinode_t *dinode, buffer_t *sb_buf, int flags, buffer_t *dinodeBlockBuffer)
{
  cffs_t *sb = (cffs_t *)sb_buf->buffer;
/*
printf("cffs_dinode_releaseDInode: dinodeNum %d, flags %x, linkCount %d\n", dinode->dinodeNum, flags, dinode->linkCount);
*/
    assert (dinodeBlockBuffer);
    if ((cffs_dinode_getOpenCount(dinode) == 0) && (cffs_dinode_getLinkCount(dinode) <= ((dinode->type & S_IFDIR) ? 1 : 0))) {
/*
printf ("Deallocating dinode %x\n", dinode->dinodeNum);
*/
       if (cffs_dinode_isDir(dinode)) {
		/* do something with live directory blocks before truncing */
		/* (recall that a dir block can be live if there is a      */
		/* cross-directory link to an embedded inode, due to hard  */
		/* links or rename)                                        */
          cffs_embdir_emptyDir (sb_buf, dinode);
       }
#if defined(XN) || defined(CFFS_PROTECTED)
       cffs_dinode_truncDInode (dinode, sb_buf, 0);
       cffs_buffer_affectFile (sb->fsdev, dinode->dinodeNum, BUFFER_INVALID, 1);
       cffs_buffer_affectFile (sb->fsdev, -dinode->dinodeNum, BUFFER_INVALID, 1);
#ifdef CFFS_PROTECTED
#ifdef CFFSD
       cffsd_fsupdate_dinode (CFFS_DINODE_DELAYEDFREE, dinode, 0, 0, 0);
#else
       sys_fsupdate_dinode (CFFS_DINODE_DELAYEDFREE, dinode, 0, 0, 0);
#endif /* CFFSD */
#else
       dinode->dinodeNum = 0;
       dinode->length = 0;
#endif /* CFFS_PROTECTED */
       cffs_buffer_releaseBlock (dinodeBlockBuffer, ((cffs_softupdates) ? (flags | BUFFER_DIRTY) : (flags | BUFFER_WRITE)));
#else /* !defined (XN) && !defined (CFFS_PROTECTED) */
       dinode_t tmpDInode = *dinode;
	/* This frees it! */
       dinode->dinodeNum = 0;
       dinode->length = 0;
       cffs_dinode_clearBlockPointers (dinode, 0);
       cffs_buffer_releaseBlock (dinodeBlockBuffer, ((cffs_softupdates) ? (flags | BUFFER_DIRTY) : (flags | BUFFER_WRITE)));
       cffs_buffer_affectFile (fsdev, tmpDInode.dinodeNum, BUFFER_INVALID, 1);
       cffs_dinode_truncDInode (&tmpDInode, fsdev, 0);
#endif /* defined (XN) || defined(CFFS_PROTECTED) */
    } else {
       cffs_buffer_releaseBlock (dinodeBlockBuffer, flags);
    }
    return (OK);
}


void cffs_dinode_writeDInode (dinode_t *dinode, block_num_t fsdev, buffer_t *dinodeBlockBuffer)
{
/*
printf("cffs_dinode_writeDInode: dinodeNum %d, linkCount %d\n", dinode->dinodeNum, dinode->linkCount);
*/
   cffs_buffer_writeBlock (dinodeBlockBuffer, BUFFER_WRITE);
}


static int cffs_dinode_indirTrunc (buffer_t *sb_buf, dinode_t *dinode, block_num_t indirblock, int indirlevel, int numblocks)
{
   int blockFrees = 0;
   buffer_t *indirectBlockBuffer;
   int *indirectBlock;
   int i;
   int ret;
   int blksperptr = 1;
   int partialpoint;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
#ifdef XN
   int isDir = cffs_dinode_isDir(dinode);
   da_t ida;
#else
#ifdef CFFS_PROTECTED
   da_t ida;
#endif
#endif

   assert (numblocks >= 0);

   for (i=1; i<indirlevel; i++) {
      blksperptr *= NUM_PTRS_IN_SINGLEINDIR;
   }
   partialpoint = (numblocks) ? (numblocks / blksperptr) : -1;

   if (indirblock == 0) {
      return (0);
   }

   indirectBlockBuffer = cffs_buffer_getBlock (sb->fsdev, 0, dinode->dinodeNum, -indirblock, (BUFFER_READ | BUFFER_WITM), NULL);

#if !defined(XN) && !defined(CFFS_PROTECTED)
   if (numblocks == 0) {
      indirectBlock = (int *) indirectBlockBuffer->buffer;
   } else {
      indirectBlock = (int *) __malloc (BLOCK_SIZE);
      bcopy (indirectBlockBuffer->buffer, (char *)indirectBlock, BLOCK_SIZE);
      for (i=(NUM_PTRS_IN_SINGLEINDIR-1); i>partialpoint; i--) {
         ((int *) indirectBlockBuffer->buffer)[i] = 0;
      }
      cffs_buffer_releaseBlock (indirectBlockBuffer, ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE));
   }
#else
   indirectBlock = (int *) indirectBlockBuffer->buffer;
   ida = indirectBlockBuffer->header.diskBlock * BLOCK_SIZE;
#endif

   for (i=(NUM_PTRS_IN_SINGLEINDIR-1); i>partialpoint; i--) {
      if (indirectBlock[i] != 0) {
         if (indirlevel > 1) {
#ifdef XN
            ret = sys_xn_insert_attr(ida,
                                     cffs_indirect_op(i,
                                                      indirectBlock[i],
                                                      indirectBlock[i],
                                                      indirlevel-1, isDir));
#endif

            blockFrees += cffs_dinode_indirTrunc (sb_buf, dinode, indirectBlock[i], (indirlevel-1), 0);
         }
#if !defined(XN) && !defined(CFFS_PROTECTED)
         ret = cffs_alloc_deallocBlock (sb->fsdev, indirectBlock[i]);
         assert (ret == 0);
         indirectBlock[i] = 0;
#elif defined(XN)
         ret = sys_xn_free(ida, cffs_indirect_op(i, indirectBlock[i], 0,
                                                  indirlevel-1, isDir), 0);
#else
         ret = cffs_alloc_protWrapper (CFFS_SETPTR_DEALLOCATE, sb_buf, dinode, &indirectBlock[i], 0);
#endif

         if (ret != 0) {
            kprintf ("failed to deallocate: ret %d, ptrval %d, ptrloc %p\n", ret, indirectBlock[i], &indirectBlock[i]);
         }
         assert (ret == 0);
         blockFrees++;
      }
   }

   if ((partialpoint >= 0) && (indirlevel > 1)) {
      assert ((numblocks - (partialpoint * blksperptr)) > 0);
#ifdef XN
      ret = sys_xn_insert_attr(ida, cffs_indirect_op(i,
                                                     indirectBlock[i],
                                                     indirectBlock[i],
                                                     indirlevel-1, isDir));
#endif
      blockFrees += cffs_dinode_indirTrunc (sb_buf, dinode, indirectBlock[i], (indirlevel-1), (numblocks - (partialpoint * blksperptr)));
   }

#if !defined(XN) && !defined(CFFS_PROTECTED)
   if (numblocks == 0) {
      cffs_buffer_releaseBlock (indirectBlockBuffer, BUFFER_INVALID);
   } else {
      __free (indirectBlock);
   }
#else
   if (numblocks == 0) {
     cffs_buffer_releaseBlock (indirectBlockBuffer, BUFFER_INVALID);
   } else {
     cffs_buffer_releaseBlock (indirectBlockBuffer, BUFFER_WRITE);
   }
#endif

   blockFrees++;

   return (blockFrees);
}


void cffs_dinode_truncDInode (dinode_t *dinode, buffer_t *sb_buf, off_t length)
{
   int i;
   int ret;
   int blockFrees = 0;
   int numblocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
#ifdef XN
   int isDir = cffs_dinode_isDir(dinode);
#endif
   int error;

/*
printf ("Truncing DInode: dinodeNum %x, OpenCount %d, linkCount %d, length %qd\n", dinode->dinodeNum, dinode->openCount, dinode->linkCount, dinode->length);
*/

	/* if not trunc'ing to block boundary, clear remainder of last block */
   if (length % BLOCK_SIZE) {
      int offset = (int) (length % BLOCK_SIZE);
      buffer_t *buffer;

      block_num_t blkno;
      error = 0;
      blkno = cffs_dinode_offsetToBlock (dinode, sb_buf, length, NULL, &error);
      if (error)
	kprintf ("cffs_dinode_truncDinode got error in cffs_buffer_getBlock\n");
      if (blkno) {
  	 error = 0;
         buffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, dinode->dinodeNum, blkno, (BUFFER_READ|BUFFER_WITM), &error);
	 if (error)
	   kprintf ("cffs_dinode_truncDInode got error in cffs_buffer_getBlock\n");
         assert ((buffer) && (buffer->buffer));
         bzero (buffer->buffer + offset, (u_int)(BLOCK_SIZE - offset));
         cffs_buffer_releaseBlock (buffer, BUFFER_DIRTY);
      }
   }

   for (i=NUM_INDIRECT; i > 0; i--) {
      if (numblocks >= cffs_dinode_firstPtr(i)) {
#ifdef XN
         sys_xn_insert_attr(cffs_dinode_da(dinode),
                            cffs_inode_op(NUM_DIRECT+i-1,
                                          dinode->indirectBlocks[i-1],
                                          dinode->indirectBlocks[i-1],
                                          i, isDir));
#endif
         cffs_dinode_indirTrunc (sb_buf, dinode, dinode->indirectBlocks[(i-1)], i, (numblocks - cffs_dinode_firstPtr(i)));
         break;
      } else if (dinode->indirectBlocks[(i-1)] != 0) {
         blockFrees += cffs_dinode_indirTrunc (sb_buf, dinode, dinode->indirectBlocks[(i-1)], i, 0);
#if !defined(XN) && !defined(CFFS_PROTECTED)
         ret = cffs_alloc_deallocBlock (sb_buf, dinode->indirectBlocks[(i-1)]);
         assert (ret == 0);
         dinode->indirectBlocks[(i-1)] = 0;
#elif defined(XN)
         ret = sys_xn_free(cffs_dinode_da(dinode),
                            cffs_inode_op(NUM_DIRECT+i-1,
                                          dinode->indirectBlocks[i-1], 0,
                                          i, isDir), 0);
#else
         ret = cffs_alloc_protWrapper (CFFS_SETPTR_DEALLOCATE, sb_buf, dinode, &dinode->indirectBlocks[(i-1)], 0);
#endif
         assert (ret == 0);
      }
   }

   for (i=numblocks; i<NUM_DIRECT; i++) {
      if (dinode->directBlocks[i] != 0) {
#if !defined(XN) && !defined(CFFS_PROTECTED)
         ret = cffs_alloc_deallocBlock (sb_buf, dinode->directBlocks[i]);
         dinode->directBlocks[i] = 0;
#elif defined(XN)
         ret = sys_xn_free(cffs_dinode_da(dinode),
                            cffs_inode_op(i, dinode->directBlocks[i], 0,
                                          0, isDir), 0);
         if (ret == XN_CANNOT_DELETE) {
		/* GROK: this is actually dangerous, since we can't tell  */
		/* the difference between an acceptible scenario and a    */
		/* bogus one... perhaps XN oought to actually be fixed... */
            //kprintf ("XN_CANNOT_DELETE, but let it go since XN doesn't support refcnts...\n");
            cffs_dinode_zeroPtr (dinode, sb_buf, i);
            ret = 0;
         }

         if (ret != 0) {
            kprintf ("dealloc failed: ret %d, i %d, blkno %d, isDir %d (isEmpty %d)\n", ret, i, dinode->directBlocks[i], isDir, cffs_embdir_isEmpty(sb_buf, dinode));
         }

#else
         ret = cffs_alloc_protWrapper (CFFS_SETPTR_DEALLOCATE, sb_buf, dinode, &dinode->directBlocks[i], 0);
         if (ret != 0) {
            kprintf ("dealloc failed: ret %d, i %d, blkno %d, (isEmpty %d)\n", ret, i, dinode->directBlocks[i], cffs_embdir_isEmpty(sb_buf, dinode));
         }

#endif
         assert (ret == 0);
         blockFrees++;
      }
   }

   cffs_dinode_setLength (dinode, length);
/*
   printf("cffs_dinode_truncDInode done: dinodeNum %x, blockFrees %d\n", dinode->dinodeNum, blockFrees);
*/
}


u_int  
cffs_dinode_recurseIndirBlock(dinode_t *dinode, buffer_t *sb_buf,
                              int toplevel, int slot, da_t parent,
                              block_num_t *ptrloc, int indirlevel, int blkno,
                              int *allocate, int readflags, int writeflags,
                              u_int lblkno,
                              u_int (*blkfunc) (dinode_t *, buffer_t *,
                                                int, int, da_t,
                                                block_num_t *, int, int *,
                                                u_int, u_int, int *),
                              u_int param,
			      int *error)           
{
   block_num_t indirblkno = *ptrloc;
   buffer_t *indirblock;
   block_num_t *indirPtrs;
   int allocated = 0;
   u_int retval;
   cffs_t *sb = (cffs_t *)sb_buf->buffer;
#ifdef XN
   int isDir = cffs_dinode_isDir(dinode);
#endif

//kprintf ("enter recurse: dinodeNum %x, slot %d (%d), indirlevel %d, blkno %d\n", dinode->dinodeNum, slot, toplevel, indirlevel, blkno);

   if (indirblkno == 0) {
      if (allocate == NULL) {
         return (0);
      }
#if !defined(XN) && !defined(CFFS_PROTECTED)
      indirblkno = cffs_allocBlock (sb_buf, dinode, -1, -1);
#else
#ifdef XN
      {int ret = XN_CANNOT_ALLOC;
      while (ret == XN_CANNOT_ALLOC) {
         indirblkno = cffs_allocBlock (sb_buf, dinode, -1, -1);
         if (toplevel) {
            //kprintf("Allocating into the inode: slot %d, indirblkno %d, indirlevel %d, isDir %d\n", slot, indirblkno, indirlevel, isDir);
            ret = sys_xn_alloc(parent,
                               cffs_inode_op(slot, 0, indirblkno,
                                             indirlevel, isDir), 0);
//kprintf ("sys_xn_alloc done\n");
         } else {
            //kprintf("Allocating into an indirect block: slot %d, indirblkno %d, indirlevel %d, isDir %d\n", slot, indirblkno, indirlevel, isDir);
            ret = sys_xn_alloc(parent,
                               cffs_indirect_op(slot, 0, indirblkno,
                                                indirlevel, isDir), 0);
//kprintf ("sys_xn_alloc done\n");
         }
#else	/* !def XN */
      {int ret = -E_EXISTS;
      while (ret == -E_EXISTS) {
         indirblkno = cffs_allocBlock (sb_buf, dinode, -1, -1);
         ret = cffs_alloc_protWrapper (CFFS_SETPTR_ALLOCATE, sb_buf, dinode, ptrloc, indirblkno);
#endif /* XN */
         if (ret != 0) {
            kprintf ("allocation failed: ret %d, indirblkno %d\n", ret, indirblkno);
	    if (errno == EDQUOT) {
	      *error = -1;
	      return 0;
	    }
         }
      }
      assert (ret == 0);
      }
#endif /* !XN && !CFFS_PROTECTED */
      allocated = 1;
      (*allocate)++;
#if !defined(XN) && !defined(CFFS_PROTECTED)
      indirblock = cffs_buffer_getBlock (sb->fsdev, 0, dinode->dinodeNum, -indirblkno, (BUFFER_GET|BUFFER_WITM), error);
      if (error && *error == -1)
	return 0;
      }
      bzero (indirblock->buffer, BLOCK_SIZE);
#else
#ifdef CFFS_PROTECTED
      indirblock = cffs_buffer_getBlock (sb->fsdev, 0, dinode->dinodeNum, -indirblkno, (BUFFER_GET|BUFFER_WITM), error);
      if (error && *error == -1) {
	return 0;
      }

#ifdef CFFSD
      {int ret = cffsd_fsupdate_dinode (CFFS_DINODE_INITINDIRBLOCK, (dinode_t *)indirblock->buffer, 0, 0, 0);
#else
      {int ret = sys_fsupdate_dinode (CFFS_DINODE_INITINDIRBLOCK, (dinode_t *)indirblock->buffer, 0, 0, 0);
#endif /* CFFSD */
      assert (ret == 0);
      }
#else
      indirblock = cffs_buffer_getBlock (sb->fsdev, 0, dinode->dinodeNum, -indirblkno, (BUFFER_READ|BUFFER_WITM), NULL);
	/* GROK -- don't assume for now: bypassing XN for read_and_insert */
      bzero (indirblock->buffer, BLOCK_SIZE);
      /* System zeros typed pages -JJJ*/
#endif
#endif

	/* indirblkno is not zero */
   } else {
#ifdef XN
      int ret;
//kprintf("not allocating: toplevel %d, slot %d, indirblkno %d, indirlevel %d, isDir %d\n", toplevel, slot, indirblkno, indirlevel, isDir);
//kprintf ("*ptrloc %d, parent %ld, blkfunc %p\n", *ptrloc, parent, blkfunc);
//kprintf ("ptrloc %p, dinode %p\n", ptrloc, dinode);
      if (toplevel) {
        ret = sys_xn_insert_attr(parent, cffs_inode_op(slot,
                                                   indirblkno, indirblkno,
                                                   indirlevel, isDir));
      } else {
        ret = sys_xn_insert_attr(parent, cffs_indirect_op(slot,
                                                   indirblkno, indirblkno,
                                                   indirlevel, isDir));
      }
      assert (ret == 0);
#endif
      indirblock = cffs_buffer_getBlock (sb->fsdev, 0, dinode->dinodeNum, -indirblkno, (BUFFER_READ|BUFFER_WITM|readflags), NULL);
      if (indirblock == NULL) {
         return (0);
      }
   }

   indirPtrs = (block_num_t *) indirblock->buffer;

   if (indirlevel > 1) {
      int blksperptr = cffs_dinode_blksPerIndirPtr (indirlevel);
      int ptrno = blkno / blksperptr;
      retval = cffs_dinode_recurseIndirBlock(dinode, sb_buf,
                               0, ptrno, ((da_t)indirblkno*(da_t)XN_BLOCK_SIZE),
                               &indirPtrs[ptrno], (indirlevel-1),
                               (blkno - (ptrno * blksperptr)),
                               allocate, readflags, writeflags, lblkno,
                               blkfunc, param, error);

   } else {
      assert (blkfunc != NULL);
      retval = blkfunc (dinode, sb_buf,
                        0, blkno, indirblkno * BLOCK_SIZE,
                        &indirPtrs[blkno], blkno, allocate,
                        ((param == 0) ? indirblock->header.diskBlock : param),
			lblkno, error);
   }

   if (error && *error == -1)
     return 0;

   if (allocated) {
      cffs_buffer_releaseBlock (indirblock, (writeflags | ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE)));
#if !defined(XN) && !defined(CFFS_PROTECTED)
      *ptrloc = indirblkno;
#endif
   } else {
      cffs_buffer_releaseBlock (indirblock, (((allocate) && (*allocate)) ? (BUFFER_DIRTY|writeflags) : 0));
   }

   return (retval);
}


static u_int lblktodblk (dinode_t *dinode, buffer_t *sb_buf,
                         int toplevel, int slot, da_t parent,
                         block_num_t *ptrloc, int ptrno,
                         int *allocate, u_int param, u_int lblkno, int *error)
{
#ifdef XN
   int isDir = cffs_dinode_isDir(dinode);
   int ret = XN_CANNOT_ALLOC;
   block_num_t blockNum;
#else
#ifdef CFFS_PROTECTED
   int ret = -E_EXISTS;
   block_num_t blockNum;
#endif
#endif

   if ((*ptrloc == 0) && (allocate)) {
#if 0
     cffs_t *sb = (cffs_t *)sb_buf->buffer;
#endif
     assert (error != NULL);
//kprintf("allocating: toplevel %d, slot %d, isDir %d\n", toplevel, slot, isDir);
#if !defined(XN) && !defined(CFFS_PROTECTED)
      *ptrloc = cffs_allocBlock (sb_buf, dinode, lblkno, ((ptrno > 0) ? ptrloc[-1] : param));
#else
#ifdef XN
      while (ret == XN_CANNOT_ALLOC) {
         blockNum = cffs_allocBlock (sb_buf, dinode, lblkno, ((ptrno > 0) ? ptrloc[-1] : param));
        if (toplevel) {
           ret = sys_xn_alloc(parent,
                              cffs_inode_op(slot, 0, blockNum, 0, isDir), 0);
        } else {
           ret = sys_xn_alloc(parent,
                              cffs_indirect_op(slot, 0, blockNum, 0, isDir), 0);
        }
        if (ret == XN_LOCKED) {
           kprintf ("I/O ongoing when allocating?  Wait a bit and retry allocation\n");
		/* GROK -- this should really be a "waitforio", but we don't */
		/* have a pointer to the problem buffer_t here.  Sigh.       */
           usleep (10000);
           ret = XN_CANNOT_ALLOC;
           continue;
        }
#else	/* !XN && CFFS_PROTECTED */
      while (ret == -E_EXISTS) {
         blockNum = cffs_allocBlock (sb_buf, dinode, lblkno, ((ptrno > 0) ? ptrloc[-1] : param));
         ret = cffs_alloc_protWrapper (CFFS_SETPTR_ALLOCATE, sb_buf, dinode, ptrloc, blockNum);
#endif /* XN */
         if (ret != 0) {
            kprintf ("allocation failed: ret %d, blockNum %d\n", ret, blockNum);
	    if (errno == EDQUOT) {
	      *error = -1;
	      return 0;
	    }
         }
      }
      assert (ret == 0);
#endif /* XN && CFFS_PROTECTED */
#if 0
      if (cffs_dinode_isDir(dinode)) {
         extern void cffs_embdir_initDirBlock (char *dirblock);
	 buffer_t *dirBlockBuffer;
	 block_num_t physicalDirBlockNumber = cffs_dinode_offsetToBlock (dinode, NULL, lblkno * BLOCK_SIZE, NULL, NULL);
	 assert (physicalDirBlockNumber);
         dirBlockBuffer = cffs_buffer_getBlock (sb->fsdev, sb->blk, -dinode->dinodeNum, physicalDirBlockNumber, (BUFFER_GET | BUFFER_WITM), error);
	 if (dirBlockBuffer == NULL) {
	   kprintf ("inodeNum %d physicalDirBlockNumber %d\n", dinode->dinodeNum, physicalDirBlockNumber);
	 }
	 assert (dirBlockBuffer);
	 if (error && *error) return 0;
		/* initialize the new directory block */
         cffs_embdir_initDirBlock (dirBlockBuffer->buffer); 
         cffs_buffer_releaseBlock (dirBlockBuffer, (BUFFER_INVALID | ((cffs_softupdates) ? BUFFER_DIRTY : BUFFER_WRITE)));
      }
#endif
      (*allocate)++;

#ifdef XN
   } else {
     blockNum = *ptrloc;
//kprintf("not allocating: toplevel %d, slot %d, blockNum %d, isDir %d\n", toplevel, slot, blockNum, isDir);
     if (toplevel) {
        ret = sys_xn_insert_attr(parent,
                                 cffs_inode_op(slot, blockNum, blockNum,
                                              0, isDir));
     } else {
        ret = sys_xn_insert_attr(parent,
                                 cffs_indirect_op(slot, blockNum, blockNum,
                                                 0, isDir));
     }
     assert (ret == 0);
#endif
   }

   return (*ptrloc);
}


/* cffs_dinode_offsetToBlock

   convert an offset into a file into a block number, given a dinode.
   This should only be called for offset < MAX_FILE_SIZE

   returns block number or 0 if attempting to read from hole in file 

 */

block_num_t cffs_dinode_offsetToBlock (dinode_t *dinode, buffer_t *sb_buf, off_t offset, int *allocate, int *error) {
   block_num_t blockNum;
   int allocHint = -1;
   int ptrno = offset / BLOCK_SIZE;

//kprintf("cffs_dinode_offsetToBlock: dinodeNum %x, offset %d, allocate %x\n", dinode->dinodeNum, (u_int)offset, (u_int) allocate);

   assert (offset < MAX_FILE_SIZE);

   if (allocate != NULL) {
/*
      allocHint = *allocate;
*/
      *allocate = 0;
   }

   /* see if the offset is mapped to a direct block */
   if (offset < BLOCK_SIZE*NUM_DIRECT) {
      blockNum = lblktodblk (dinode, sb_buf, 1, ptrno, cffs_dinode_da(dinode), & dinode->directBlocks[ptrno], ptrno, allocate, allocHint, ptrno, error);
      if (error && *error == -1)
	return 0;

   } else {
      /* have to read indirect block. May have to allocate it first. */
      int indirlevel = cffs_dinode_indirlevel (ptrno);
      blockNum = cffs_dinode_recurseIndirBlock (dinode, sb_buf, 1,
                 NUM_DIRECT + indirlevel-1, cffs_dinode_da(dinode),
                 &dinode->indirectBlocks[(indirlevel-1)], indirlevel,
                 cffs_dinode_ptrinIndirBlock(ptrno, indirlevel), allocate, 0, 0,
                 ptrno, lblktodblk, 0, error);
   }

//kprintf("cffs_dinode_offsetToBlock return: blockNum %d, *allocate %d\n", blockNum, *allocate);

   return (blockNum);
}


static u_int countNumContig (dinode_t *dinode, buffer_t *sb_buf,
                             int toplevel, int slot, da_t parent,
                             block_num_t *ptrloc, int ptrno,
                             int *allocate, u_int param, u_int lblkno, int *error)
{
   int i;
   block_num_t diskblock = ptrloc[0];
   int contig = 0;

   assert (error == NULL);

   for (i=1; i<(param-ptrno); i++) {
      if (ptrloc[i] != (diskblock + i)) {
         return (contig);
      }
      contig++;
   }

   return (contig);
}


int cffs_dinode_getNumContig (dinode_t *dinode, buffer_t *sb_buf, block_num_t blkno)
{
   int contig;
/*
printf("cffs_dinode_getNumContig: dinodeNum %x, blkno %d\n", dinode->dinodeNum, blkno);
*/
   if (blkno < NUM_DIRECT) {
      contig = countNumContig (dinode, sb_buf, 0,0,0, &dinode->directBlocks[blkno], blkno, NULL, NUM_DIRECT, blkno, NULL);

   } else {
      int indirlevel = cffs_dinode_indirlevel (blkno);

	/* use HITONLY since this is only for performance and there is no     */
	/* reason to therefore risk a miss-cycle (plus prob. not a perf. win) */

      contig = cffs_dinode_recurseIndirBlock (dinode, sb_buf, 1,
               NUM_DIRECT + indirlevel - 1, cffs_dinode_da(dinode),
               &dinode->indirectBlocks[(indirlevel-1)], indirlevel,
               cffs_dinode_ptrinIndirBlock(blkno, indirlevel), NULL,
               BUFFER_HITONLY, 0, blkno, countNumContig, NUM_PTRS_IN_SINGLEINDIR, NULL);
   }
/*
printf("cffs_dinode_getNumContig return: contig %d\n", contig);
*/
   return (contig);
}


static u_int addPtr (dinode_t *dinode, buffer_t *sb_buf,
                     int toplevel, int slot, da_t parent,
                     block_num_t *ptrloc, int ptrno,
                     int *allocate, u_int param, u_int lblkno, int *error)
{
   int i;

   assert (ptrno == 0);
   assert (allocate != NULL);
   assert (error == NULL);

   for (i=0; i<NUM_PTRS_IN_SINGLEINDIR; i++) {
      if (ptrloc[i] == 0) {
         ptrloc[i] = param;
         (*allocate)++;
         return (i+1);
      }
   }

   return (0);
}


void cffs_dinode_transferBlock (dinode_t *dinode, buffer_t *sb_buf, block_num_t blockNum)
{
   int i, j;
   int allocated = 0;
   u_int retval;

   for (i=0; i<NUM_DIRECT; i++) {
      if (dinode->directBlocks[i] == 0) {

	cffs_dinode_setDirect (dinode, i, blockNum);
         if (dinode->length <= (i * BLOCK_SIZE)) {
	   cffs_dinode_setLength (dinode, i * BLOCK_SIZE);
         }
         return;
      }
   }

assert (0);

   for (i=1; i<NUM_INDIRECT; i++) {
      for (j=0; j<cffs_dinode_blksPerIndirPtr(i); j++) {
		/* GROK -- the arithmetic is wrong here!!! */
         u_int lblkno = cffs_dinode_firstPtr(i) + (j * NUM_PTRS_IN_SINGLEINDIR);
         retval = cffs_dinode_recurseIndirBlock (dinode, sb_buf, 1,
                  NUM_INDIRECT + i - 1, cffs_dinode_da(dinode),
                  &dinode->indirectBlocks[(i-1)], i, j * NUM_PTRS_IN_SINGLEINDIR,
                  &allocated, 0, 0, lblkno, addPtr, blockNum, NULL);
         if (retval) {
            off_t len = BLOCK_SIZE * (cffs_dinode_firstPtr(i) + (j * NUM_PTRS_IN_SINGLEINDIR) + (retval-1));
            if (dinode->length <= len) {
               dinode->length = len;
            }
            return;
         }
      }
   }
}


static u_int zeroPtr (dinode_t *dinode, buffer_t *sb_buf,
                      int toplevel, int slot, da_t parent,
                      block_num_t *ptrloc, int ptrno,
                      int *allocate, u_int param, u_int lblkno, int *error)
{
  assert (error == NULL);

#ifdef CFFS_PROTECTED
#ifdef CFFSD
   cffsd_fsupdate_setptr (CFFS_SETPTR_NULLIFY, (cffs_t *)(sb_buf->buffer), dinode, NULL, ptrloc, 0);
#else
   sys_fsupdate_setptr (CFFS_SETPTR_NULLIFY, (cffs_t *)(sb_buf->buffer), dinode, NULL, ptrloc, 0);
#endif /* CFFSD */
   assert (*ptrloc == 0);
#else
   *ptrloc = 0;
#endif
   return (1);
}


void cffs_dinode_zeroPtr (dinode_t *dinode, buffer_t *sb_buf, int ptrno)
{
   if (ptrno < NUM_DIRECT) {
/*
printf ("zeroPtr (direct): dinodeNum %x, ptrno %d, oldval %d\n", dinode->dinodeNum, ptrno, dinode->directBlocks[ptrno]);
*/
#ifdef CFFS_PROTECTED
#ifdef CFFSD
      cffsd_fsupdate_setptr (CFFS_SETPTR_NULLIFY, (cffs_t *)(sb_buf->buffer), dinode, NULL, &dinode->directBlocks[ptrno], 0);
#else
      sys_fsupdate_setptr (CFFS_SETPTR_NULLIFY, (cffs_t *)(sb_buf->buffer), dinode, NULL, &dinode->directBlocks[ptrno], 0);
#endif /* CFFSD */
#else
      dinode->directBlocks[ptrno] = 0;
#endif
      assert (dinode->directBlocks[ptrno] == 0);
   } else {
      int indirlevel = cffs_dinode_indirlevel (ptrno);
      cffs_dinode_recurseIndirBlock (dinode, sb_buf, 1,
                  NUM_DIRECT + indirlevel - 1, cffs_dinode_da(dinode),
                  &dinode->indirectBlocks[(indirlevel-1)], indirlevel,
                  cffs_dinode_ptrinIndirBlock(ptrno, indirlevel), NULL, 0, 0,
                  ptrno, zeroPtr, 0, NULL);
   }
}

