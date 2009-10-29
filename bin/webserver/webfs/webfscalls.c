
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


/* WebFS -- Web Server Filesystem (companion to webserver) */

#include "alfs/alfs.h"
#include "alfs/alfs_buffer.h"
#include "alfs/alfs_inode.h"
#include "alfs/alfs_path.h"
#include "alfs/alfs_alloc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "webfs.h"
#include "webfs_rdwr.h"
#include "webfs_mime.h"
#include "webfs_header.h"

/* Use the two extra inode fields for headerlength and extrareadlen */

#define alfs_inode_getHeaderlen(a)	alfs_inode_getExtrafield1(a)
#define alfs_inode_getExtrareadlen(a)	alfs_inode_getExtrafield2(a)
#define alfs_inode_setHeaderlen(a,b)	alfs_inode_setExtrafield1(a,b)
#define alfs_inode_setExtrareadlen(a,b)	alfs_inode_setExtrafield2(a,b)

/*
static int time (int *blah) {
   return (0);
}
*/

mime_t *MimeTypes = NULL;
int MimeTypeCount = 0;

mime_t *MimeEncodings = NULL;
int MimeEncodingCount = 0;

char *DefaultMimeType = "test/plain";

static void webfs_config_mimetypes (char *filename, mime_t **types, int *countPtr);
static unsigned int webfs_mime_info (char *name, int namelen);
static void webfs_construct_header (inode_t *inode);
static void webfs_compute_inetsums (inode_t *inode);
/*#define RELOCATE*/
#ifdef RELOCATE
static void webfs_relocate_includes (inode_t *inode);
#endif


/* create a file system on a disk */

void webfs_initFS (char *devname, int size)
{
   uint superblkno;
   uint devno = atoi(devname);
   char devname2[20];
   int fd;
   FILE *fp;

   sprintf (devname2, "/webfs%d", devno);
   fd = open (devname2, (O_CREAT|O_EXCL|O_WRONLY), 0755);
   if (fd < 0) {
      printf ("could not create webfs descriptor file named \"%s\"\n", devname2);
      perror ("");
      exit(0);
   }

   superblkno = alfs_initFS (devno, size, alfs_makeCap());

   fp = fdopen (fd, "w");
   assert (fp != NULL);
   fprintf (fp, "ALFS file system definition\n");
   fprintf (fp, "devno: %d\n", devno);
   fprintf (fp, "superblkno: %d\n", superblkno);
   fclose (fp);
}


/* mount an on-disk file system */

void webfs_mountFS (char *devname)
{
   uint devno, superblkno;
   int ret, val;
   char devname2[20];
   FILE *fp;

   devno = atoi(devname);
   sprintf (devname2, "/webfs%d", devno);
   fp = fopen (devname2, "r");
   assert (fp != NULL);
   fscanf (fp, "ALFS file system definition\n");
   ret = fscanf (fp, "devno: %d\n", &val);
   assert ((ret == 1) && (val == devno));
   ret = fscanf (fp, "superblkno: %d\n", &val);
   assert ((ret == 1) && (val > 0));
   superblkno = val;

   alfs_mountFS (devno, superblkno);

   webfs_config_mimetypes ("mime.types", &MimeTypes, &MimeTypeCount);	
   webfs_config_mimetypes ("encoding.types", &MimeEncodings, &MimeEncodingCount);

{
extern inode_t *alfs_currentRootInode;
printf ("root: inodeNum %d, directptr0 %d\n", alfs_currentRootInode->inodeNum, alfs_currentRootInode->dinode->directBlocks[0]);
}

}


void webfs_unmountFS () {
   alfs_unmountFS();
}


int webfs_open (char *path, int flags, umask_t mode) {
   int ret = alfs_open (path, flags, mode);

   if (ret > 0) {
      inode_t *inode = (inode_t *) ret;
      char *name = &path[(strlen(path)-1)];
      while ((name != path) && (name[0] != '/')) {
         name--;
      }
      if (name[0] == '/') {
         name++;
      }
      alfs_inode_setType (inode, (alfs_inode_getType(inode) | webfs_mime_info (name, strlen(name))));
/*
printf ("file named %s has been given type %x\n", name, alfs_inode_getType(inode));
*/
   }

   return (ret);
}


int webfs_read (int fd, char *buf, int nbytes) {
   inode_t *inode = (inode_t *) fd;
   int ret;

   if (inode == NULL) {
      return (ALFS_EBADF);
   }

   /* make sure we've got the file open for reading */
   if (!(inode->flags & OPT_RDONLY)) {
      return (ALFS_EBADF);
   }

   /* do the read */
   ret = webfs_readBuffer (inode, buf, inode->fileptr, nbytes);
   if (ret < 0) {
      return (ret);
   }

   /* update the file position */
   inode->fileptr += ret;

   return (ret);
}


int webfs_write (int fd, char *buf, int nbytes) {
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
   ret = webfs_writeBuffer (inode, buf, inode->fileptr, nbytes);
   if (ret < 0) {
      return (ret);
   }

   /* update the file pointer */
   inode->fileptr += ret;
   alfs_inode_setModTime (inode, time(NULL));

   return (ret);
}


int webfs_close (int fd) {
   inode_t *inode = (inode_t *) fd;
   assert (inode && inode->dinode);
   alfs_inode_setModTime (inode, time(NULL));
   webfs_construct_header (inode);
   webfs_compute_inetsums (inode);  /* compute as go !! */
#ifdef RELOCATE
   webfs_relocate_includes (inode);
#endif
   return (alfs_close (fd));
}


int webfs_lseek (int fd, offset_t offset, int whence) {
   return (alfs_lseek (fd, offset, whence));
}


int webfs_mkdir (char *path, int mode) {
   return (alfs_mkdir (path, mode));
}


int webfs_unlink (char *path) {
   return (alfs_unlink (path));
}


int webfs_fstat (int fd, struct alfs_stat *buf) {
   return (alfs_fstat (fd, buf));
}


int webfs_stat (char *path, struct alfs_stat *buf) {
   return (alfs_stat (path, buf));
}


int webfs_chmod (char *path, int mode) {
   return (alfs_chmod (path, mode));
}


void webfs_listDirectory (void *curRoot)
{
   alfs_listDirectory (curRoot);
}


static char * webfs_config_nextmimeext (char *line)
{
   char *ptr = line;

   while ((ptr[0] != '\n') && (ptr[0] != (char) 0) && (ptr[0] != ' ')) {
      ptr++;
   }
   while (ptr[0] == ' ') {
      ptr++;
   }
   if ((ptr[0] == '\n') || (ptr[0] == (char) 0)) {
      ptr = NULL;
   }
   return (ptr);
}


void webfs_config_mimetypes (char *filename, mime_t **types, int *countPtr)
{
   char line[256];
   FILE *mimefile;
   char *mimetype;
   int linecount = 0;
   int extcount = 0;
   int extno;
   int typeno;
   int i;

   if ((mimefile = fopen (filename, "r")) == NULL) {
      printf ("Unable to open config file: %s\n", filename);
      exit (0);
   }

   while (fgets (line, 255, mimefile)) {
      linecount++;
      if ((line[0] == '#') || (line[0] == '\n')) {
         continue;
      }
      mimetype = line;
      while ((mimetype = webfs_config_nextmimeext (mimetype)) != NULL) {
         extcount++;
      }
   }
   fseek (mimefile, 0, 0);

   *countPtr = extcount;
   *types = (mime_t *) malloc (extcount * sizeof(mime_t));

   extno = 0;
   typeno = 0;
   for (i=0; i<linecount; i++) {
      if (fgets (line, 255, mimefile) == 0) {
         printf ("webfs_config_mimetypes: Non-matching values for i and linecount: %d != %d\n", i, linecount);
         exit (0);
      }
      if ((line[0] == '#') || (line[0] == '\n')) {
         continue;
      }
      mimetype = line;
      while ((mimetype = webfs_config_nextmimeext (mimetype)) != NULL) {
         if (sscanf (line, "%s", (*types)[extno].type) != 1) {
            printf ("Invalid value for mime type %d: %s\n", extno, line);
            exit (0);
         }
         if (strlen ((*types)[extno].type) >= 48) {
            printf ("Mime type is too large (%d characters): %s\n", strlen((*types)[extno].type), (*types)[extno].type);
            exit (0);
         }
         if (sscanf (mimetype, "%s", (*types)[extno].ext) != 1) {
            printf ("Invalid value for mime ext %d: %s\n", extno, mimetype);
            exit (0);
         }
         if (strlen ((*types)[extno].ext) >= 16) {
            printf ("Mime ext is too large (%d characters): %s\n", strlen((*types)[extno].type), (*types)[extno].ext);
            exit (0);
         }
         (*types)[extno].typeno = typeno;
         extno++;
      }
      typeno++;
   }
   if (extno != extcount) {
      printf ("webfs_config_mimetypes: Non-matching values for extno and extcount: %d != %d\n", extno, extcount);
      exit (0);
   }

   fclose (mimefile);
/*
   for (i=0; i<extcount; i++) {
      printf ("Mime ext %d: %s %s (%d)\n", i, (*types)[i].ext, (*types)[i].type, (*types)[i].typeno);
   }
*/
}


static unsigned int webfs_mime_info (char *name, int namelen)
{
   char *extstart = &name[(namelen-1)];
   int extlen = 0;
   int mimeinfo = 0;
   int i;

   if (namelen <= 1) {
      return (0);
   }

   while ((extstart != name) && (extstart[0] != '.')) {
      extstart--;
   }
   if (extstart[0] != '.') {
      return (0);
   }

   extlen = namelen - (extstart - name) - 1;
   if (extlen == 0) {
      return (0);
   }

   for (i=0; i<MimeEncodingCount; i++) {
      if (bcmp (&extstart[1], MimeEncodings[i].ext, extlen) == 0) {
         break;
      }
   }
   if (i == MimeEncodingCount) {
      for (i=0; i<MimeTypeCount; i++) {
         if (bcmp (&extstart[1], MimeTypes[i].ext, extlen) == 0) {
/*
printf ("no encoding: i %d, ext %s, typeno %d, val %x\n", i, MimeTypes[i].ext, MimeTypes[i].typeno, ((unsigned)(MimeTypes[i].typeno + 1) << 16));
*/
            return ((MimeTypes[i].typeno + 1) << 16);
         }
      }
      return (0);
   }

   mimeinfo |= (MimeEncodings[i].typeno + 1) << 8;
   extlen = extstart - name;
   extstart--;
   while ((extstart > name) && (extstart[0] != '.')) {
      extstart--;
   }
   extlen = extlen - (extstart - name) - 1;
   if ((extlen > 0) && (extstart[0] == '.')) {
      for (i=0; i<MimeTypeCount; i++) {
         if (bcmp (&extstart[1], MimeTypes[i].ext, extlen) == 0) {
/*
printf ("yes encoding: i %d, mimeinfo %x, ext %s, typeno %d, val %x\n", i, mimeinfo, MimeTypes[i].ext, MimeTypes[i].typeno, ((unsigned)(MimeTypes[i].typeno + 1) << 16));
*/
            return (mimeinfo | (MimeTypes[i].typeno + 1) << 16);
         }
      }
   }
   return (mimeinfo);
}


static void webfs_construct_header (inode_t *inode)
{
   int headerlen = 0;
   buffer_t *blockBuffer;
   char *buf;
   int buflen = 512;
/*
printf ("webfs_construct_header: inode %x, inode->inodeNum %d\n", (uint) inode, inode->inodeNum);
*/
   /* Not writing entire block, so must get old copy */
   blockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, 0, (BUFFER_READ | BUFFER_WITM));
   assert ((blockBuffer) && (blockBuffer->buffer));
   buf = blockBuffer->buffer;

   headerlen += web_header_goodstatus (buf, buflen);
   headerlen += web_header_datezeroed ((buf + headerlen), (buflen - headerlen));
/*
   headerlen += web_header_date ((buf + headerlen), (buflen - headerlen));
*/
   headerlen += web_header_server ((buf + headerlen), (buflen - headerlen));
   headerlen += web_header_lastmodified ((buf + headerlen), (buflen - headerlen), alfs_inode_getModTime(inode));
   headerlen += web_header_contenttype ((buf + headerlen), (buflen - headerlen), ((alfs_inode_getType(inode) >> 16) - 1));
	/* if inode->contentencoding, then add said mime info */
   if (alfs_inode_getType(inode) & 0x0000FF00) {
      headerlen += web_header_contentencoding ((buf + headerlen), (buflen - headerlen), (((alfs_inode_getType(inode) & 0x0000FF00) >> 8)) - 1);
   }
   headerlen += web_header_contentlength ((buf + headerlen), (buflen - headerlen), alfs_inode_getLength(inode), ((headerlen+1)&0x1));
   headerlen += web_header_end ((buf + headerlen), (buflen - headerlen));

   if (headerlen >= buflen) {
      printf ("Can't fit header and pathname into buf (%d >= %d)\n", headerlen, buflen);
      exit (0);
   }
/*
printf ("webfs_construct_header: buf %x, header length %d, space left %d\n", (unsigned) buf, headerlen, (buflen - headerlen));
buf[headerlen] = (char) 0;
printf ("%s", buf);
*/
   alfs_inode_setHeaderlen(inode, headerlen);
   alfs_buffer_releaseBlock (blockBuffer, BUFFER_DIRTY);
}


/*
 * Compute Internet Checksum for "count" bytes beginning at location "addr".
 * Taken from RFC 1071.
 */

static long webfs_inet_sum(unsigned short *addr, int count, long start, int last)
{
    register long sum = start;

    /* An unrolled loop */
    while ( count > 15 ) {
       sum += * (unsigned short *) &addr[0];
       sum += * (unsigned short *) &addr[1];
       sum += * (unsigned short *) &addr[2];
       sum += * (unsigned short *) &addr[3];
       sum += * (unsigned short *) &addr[4];
       sum += * (unsigned short *) &addr[5];
       sum += * (unsigned short *) &addr[6];
       sum += * (unsigned short *) &addr[7];
       addr += 8;
       count -= 16;
    }

    /*  This is the inner loop */
    while( count > 1 )  {
        sum += * (unsigned short *) addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if(count > 0)
        sum += * (unsigned char *) addr;

    if (last) {
       /*  Fold 32-bit sum to 16 bits */
       while (sum>>16) {
          sum = (sum & 0xffff) + (sum >> 16);
       }
       return (~sum & 0xffff);
    }
    return sum;
}


static void webfs_compute_inetsums (inode_t *inode)
{
   buffer_t *blockBuffer;
   int blkno = 0;
   int physoff;
   int logoff;
   int sum;
   int sumno = 4;
   int sumpart;
   int length = alfs_inode_getLength(inode);
/*
printf ("webfs_compute_inetsums: inode %x, inode->inodeNum %d\n", (uint) inode, inode->inodeNum);
*/
   blockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, 0, (BUFFER_READ | BUFFER_WITM));
   assert ((blockBuffer) && (blockBuffer->buffer));
   sum = webfs_inet_sum ((unsigned short *) blockBuffer->buffer, alfs_inode_getHeaderlen(inode), 0, 0);
   sumpart = alfs_inode_getHeaderlen(inode);
   logoff = 0;
   physoff = 512;
   do {
      int sumlen = min ((1460-sumpart), (min((length-logoff), ((BLOCK_SIZE-16)-physoff))));
      sum = webfs_inet_sum ((unsigned short *) &blockBuffer->buffer[physoff], sumlen, sum, 0);
      sumpart += sumlen;
      physoff += sumlen;
      logoff += sumlen;
      if ((sumpart == 1460) || (logoff == length)) {
         *((int *)&blockBuffer->buffer[(BLOCK_SIZE-(sumno*sizeof(int)))]) = sum;
         sum = 0;
         sumpart = 0;
         sumno--;
         if (logoff == length) {
            break;
         }
      }
/*
printf ("sumpart %d, physoff %d, logoff %d, sumno %d, sumlen %d, length %d\n", sumpart, physoff, logoff, sumno, sumlen, length);
*/
      
      if (physoff == (BLOCK_SIZE-16)) {
         assert ((sumno == 2) || (sumno == 1));
         physoff = 0;
         while (sumno > 0) {
            *((int *)&blockBuffer->buffer[(BLOCK_SIZE-(sumno*sizeof(int)))]) = -1;
            sumno--;
         }
         sumno = 4;
         blkno++;
         alfs_buffer_releaseBlock (blockBuffer, BUFFER_DIRTY);
         blockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, blkno, (BUFFER_READ | BUFFER_WITM));
         assert ((blockBuffer) && (blockBuffer->buffer));
         
      }
   } while (logoff < length);
   while (sumno > 0) {
      *((int *)&blockBuffer->buffer[(BLOCK_SIZE-(sumno*sizeof(int)))]) = -1;
      sumno--;
   }
   alfs_buffer_releaseBlock (blockBuffer, BUFFER_DIRTY);
}

#ifdef RELOCATE

#define HACKED_INCLUDES	7

void webfs_relocate_includes (inode_t *inode)
{
   inode_t *inodes[HACKED_INCLUDES];
   buffer_t *blockBuffer;
   buffer_t *blockBuffer2;
   char path[80];
   int start;
   int count;
   int ret;
   int i;

   extern char *alfs_allocationMap;
   extern inode_t *alfs_currentRootInode;

   if ((alfs_inode_getType(inode) >> 16) != 13) {
      alfs_inode_setExtrareadlen (inode, 0);
      return;
   }

printf ("webfs_relocate_includes\n");

   blockBuffer = alfs_buffer_getBlock (inode->fsdev, inode->inodeNum, 0, (BUFFER_READ | BUFFER_WITM));
   assert ((blockBuffer) && (blockBuffer->buffer));

   for (i=0; i<HACKED_INCLUDES; i++) {
      bcopy (&blockBuffer->buffer[(512+(i*53))], path, 52);
      path[52] = (char) 0;
printf ("going after path: %s\n", path);
      ret = alfs_path_translateNameToInode (path, alfs_currentRootInode, &inodes[i], BUFFER_WITM);
      assert (ret == OK);
   }

printf ("going after some free space\n");

   start = alfs_inode_getDirect(inode,0);
   count = 0;
   while (count < (HACKED_INCLUDES + 1)) {
      if ((start + count) >= alfs_blockcount) {
         start = 0;
         count = 0;
      }
      if (!alfs_allocationMap[(start+count)]) {
         count++;
      } else {
         start = start + count + 1;
         count = 0;
      }
   }

printf ("found %d free blocks starting at %d\n", count, start);

   for (i=0; i<count; i++) {
      assert (alfs_alloc_allocBlock (start+i) == 0);
   }

   blockBuffer->header.diskBlock = start;
   assert (alfs_inode_getDirect(inode,0) != 0);
   assert (alfs_alloc_deallocBlock (alfs_inode_getDirect(inode,0)) == 0);
   alfs_inode_setDirect (inode, 0, start);

   for (i=1; i<=HACKED_INCLUDES; i++) {
      blockBuffer2 = alfs_buffer_getBlock (inodes[(i-1)]->fsdev, inodes[(i-1)]->inodeNum, 0, (BUFFER_READ | BUFFER_WITM));
      assert ((blockBuffer2) && (blockBuffer2->buffer));
      blockBuffer2->header.diskBlock = start + i;
      assert (alfs_inode_getDirect(inodes[(i-1)],0) != 0);
      assert (alfs_alloc_deallocBlock (alfs_inode_getDirect(inodes[(i-1)],0)) == 0);
      alfs_inode_setDirect (inodes[(i-1)], 0, (start+i));
      alfs_buffer_releaseBlock (blockBuffer2, BUFFER_DIRTY);
      alfs_inode_releaseInode (inodes[(i-1)], BUFFER_DIRTY);
   }

   alfs_inode_setExtrareadlen (inode, HACKED_INCLUDES);
   alfs_buffer_releaseBlock (blockBuffer, BUFFER_DIRTY);
}
#endif /* RELOCATE */

