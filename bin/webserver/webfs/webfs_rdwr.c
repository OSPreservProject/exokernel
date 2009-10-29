
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
#include "webfs_rdwr.h"
#include <assert.h>
#include <memory.h>
#include <stdio.h>

#define webfs_fileOffsetToBlockOffset(off) ((off)%(BLOCK_SIZE-16))

/* read/write buffers from/to disk blocks */

/* These routines map a buffer of data onto a sequence of disk blocks.
   Given an inode and a buffer, data will either be read from the file
   into the buffer or written from the buffer inot the blocks. In the
   case of a write, the inode may be updated to reflect new blocks, file
   lengths, etc... */

/*  webfs_readBuffer 

    inode is the file's inode
    buffer is pointer to where to put data
    offset is number of bytes into file to start reading from
    length is the number of bytes to read

    returns ALFS_ELENGTH if offset+length would read past end of file
    else returns OK.

 */

int webfs_readBuffer (inode_t *inode, char *buffer, unsigned offset, unsigned length)
{
   block_num_t currentBlock;
   buffer_t *blockBuffer;
   int amountToCopy;
   int blockOffset;
   int totlength;
/*
printf ("webfs_readBuffer: inodeNum %d, filesize %d, offset %d, length %d\n", inode->inodeNum, alfs_inode_getLength(inode), offset, length);
*/
   /* GROK -- this represents a change visible to users!!! */
   /* see if we're trying to read from beyond end of file */
   if (offset >= alfs_inode_getLength(inode)) {
      return (ALFS_ELENGTH);
   }

   if (offset+length > alfs_inode_getLength(inode)) {
      length = alfs_inode_getLength(inode) - offset;
   }
   totlength = length;

   /* adjust offset to avoid first 512 bytes - reserved for HTTP header */

   offset += 512;

   /* ok, now handle reading the remaining blocks in the file */

   while (length) {

      /* figure out where were doing this reading thing from and to */
      blockOffset = webfs_fileOffsetToBlockOffset (offset); /*don't do each time*/
      amountToCopy = min (length, (BLOCK_SIZE-16)-blockOffset);
      currentBlock = offset / (BLOCK_SIZE-16);

      /* get a block's worth of data and copy it to the user's buffer */
      /* GROK -- for holes in a file, this wastes time with an extra  */
      /* copy of the block of zeros -- probably not worth optimizing  */
      /* this case.                                                   */

      blockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, currentBlock, BUFFER_READ);
      assert (blockBuffer && blockBuffer->buffer);
      memcpy (buffer, (blockBuffer->buffer + blockOffset), amountToCopy);
      alfs_buffer_releaseBlock (blockBuffer, 0);

      buffer += amountToCopy;
      offset += amountToCopy;
      length -= amountToCopy;
   }

   return totlength;
}

/* webfs_writeBuffer 

   inode is the file's inode
   buffer is pointer to data to write
   offset is number of bytes into file to start writing at
   length is the number of bytes to write

   returns ALFS_ELENGTH if offset+length would read past end of file
   else returns OK.
 */

int webfs_writeBuffer (inode_t *inode, char *buffer, unsigned offset, unsigned length)
{
   block_num_t currentBlock;
   buffer_t *blockBuffer;
   int amountToWrite;
   int blockOffset;
   int totlength = length;
/*
printf ("webfs_writeBuffer: inodeNum %d, filesize %d, offset %d, length %d\n", inode->inodeNum, alfs_inode_getLength (inode), offset, length);
*/
   /* make sure we're not trying to write too big of a file */
   if (offset+length > MAX_FILE_SIZE) {
      return ALFS_ELENGTH;
   }

   /* update the file length if we're appending any data */
   if (alfs_inode_getLength(inode) < offset+length) {
      alfs_inode_setLength(inode, (offset+length)); 
/* GROK -- how do this now??
      inodeBuffer->flags |= BUFFER_DIRTY;
*/
   }

   /* adjust offset to avoid first 512 bytes - reserved for HTTP header */

   offset += 512;

   /* ok, now handle writing the remaining blocks in the file */

   while (length) {

      currentBlock = offset / (BLOCK_SIZE-16);
      blockOffset = webfs_fileOffsetToBlockOffset (offset); /*don't do each time*/
      amountToWrite = min (length, (BLOCK_SIZE-16)-blockOffset);

      /* GROK -- for file extensions, allocation is not done here!!!!  */
      /* Space may not be available when the write-backs occurs later! */
      if (amountToWrite < (BLOCK_SIZE-16) || blockOffset != 0) {
         /* Not writing entire block, so must get old copy */
         blockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, currentBlock, (BUFFER_READ | BUFFER_WITM));
      } else {
         /* just get old copy if it exists -- will replace otherwise */
         blockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, currentBlock, (BUFFER_GET | BUFFER_WITM));
      }

      assert (blockBuffer);
      assert (blockBuffer->buffer);	       
      memcpy ((blockBuffer->buffer + blockOffset), buffer, amountToWrite);
      alfs_buffer_releaseBlock (blockBuffer, BUFFER_DIRTY);

      buffer += amountToWrite;
      offset += amountToWrite;
      length -= amountToWrite;
   }

   return totlength;
}

