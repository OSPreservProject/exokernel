
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


#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#ifdef EXOPC
#include <xok/mmu.h>	/* for NBPG */
#include <exos/osdecl.h> /* for min and max */
#else
#define kprintf printf
#define NBPG	4096
#ifndef min
#define min(a,b)	(((a)<(b)) ? (a) : (b))
#endif
#endif

#include "xio_tcpbuffer.h"


void xio_tcpbuffer_init (xio_tbinfo_t *tbinfo, char * (*pagealloc)(void *info, int len))
{
   tbinfo->freelist = NULL;
   tbinfo->bufcount = 0;
   tbinfo->readbufs = 0;
   tbinfo->writebufs = 0;
   tbinfo->pagealloc = pagealloc;
}


int xio_tcpbuffer_putdata (xio_tbinfo_t *tbinfo, xio_tcpbuf_t * *tblist, char *buffer, int len, int offset, int prunepoint)
{
   xio_tcpbuf_t *tmp = *tblist;
   int donelen = 0;
/*
kprintf ("xio_tcpbuffer_putdata: len %d, start %d, prunepoint %d\n", len, offset, prunepoint);
*/

	/* remove sent&acked buffers from the outbuffer list */
   while ((tmp) && ((int)(tmp->start + tmp->offset + tmp->len) <= prunepoint)) {
      *tblist = tmp->next;
      tmp->next = tbinfo->freelist;
      tbinfo->freelist = tmp;
      tmp = *tblist;
      tbinfo->writebufs--;
   }

   if (len <= 0) {
      return (0);
   }

	/* find the tail */
   while ((tmp) && (tmp->next)) {
      tmp = tmp->next;
   }

if (! ((len == 0) || (tmp == NULL) || (tmp->start < offset))) {
   kprintf ("tmp %p, tmp->start %d, tmp->len %d, tmp->offset %d\n", tmp, tmp->start, tmp->len, tmp->offset);
}
   assert ((len == 0) || (tmp == NULL) || (tmp->start < offset));

if (! ((len == 0) || (tmp == NULL) || ((tmp->start + tmp->offset + tmp->len) == offset)) ) {
   kprintf ("tmp %p, tmp->start %d, tmp->len %d, tmp->offset %d, offset %d, len %d\n", tmp, tmp->start, tmp->len, tmp->offset, offset, len);
}
   assert ((len == 0) || (tmp == NULL) || ((tmp->start + tmp->offset + tmp->len) == offset));

	/* add data to last buffer, if space */
   if ((tmp) && (tmp->maxlen > (tmp->offset + tmp->len))) {
      int tmplen = tmp->maxlen - tmp->offset - tmp->len;
      assert (offset == (tmp->start + tmp->offset + tmp->len));
      bcopy (buffer, &tmp->data[(tmp->offset + tmp->len)], min(len, tmplen));
      tmp->len += min (len, tmplen);
      offset += min (len, tmplen);
      donelen += min (len, tmplen);
      len -= min (len, tmplen);
   }

	/* (re-)alloc and fill more buffers and append to list */
   while (len > 0) {
      xio_tcpbuf_t *new = tbinfo->freelist;
      if (new == NULL) {
         new = (xio_tcpbuf_t *) tbinfo->pagealloc (tbinfo, XIO_TCPBUFFER_ALLOCSIZE);
         tbinfo->bufcount++;
      } else {
         tbinfo->freelist = new->next;
      }
      assert (new != NULL);
      tbinfo->writebufs++;
      new->next = NULL;
      new->start = offset;
      new->offset = 0;
      new->maxlen = XIO_TCPBUFFER_ALLOCSIZE - sizeof (xio_tcpbuf_t);
      new->data = (char *) new + sizeof (xio_tcpbuf_t);
      new->len = min (new->maxlen, len);
      bcopy (&buffer[donelen], new->data, new->len);
      offset += new->len;
      donelen += new->len;
      len -= new->len;
      if (tmp) {
         tmp->next = new;
      } else {
         *tblist = new;
      }
      tmp = new;
   }
/*
kprintf ("done: donelen %d\n", donelen);
*/
   return (donelen);
}


int xio_tcpbuffer_countBufferedData (xio_tbinfo_t *tbinfo, xio_tcpbuf_t * *tblist, int prunepoint)
{
   int totallen = 0;
   xio_tcpbuf_t *tmp = *tblist;

   while (tmp) {
      if (prunepoint < (int)(tmp->start + tmp->offset + tmp->len)) {
         if (prunepoint > (int)tmp->start) {
            totallen += tmp->start + tmp->offset + tmp->len - prunepoint;
         } else {
            totallen += tmp->len;
         }
         tmp = tmp->next;
      } else {
if ( ! (tmp == *tblist)) {
   kprintf ("tmp %p, outbufs %p, tmp->start %d, tmp->offset %d, tmp->len %d, out->len %d, out->off %d\n", tmp, *tblist, tmp->start, tmp->offset, tmp->len, (*tblist)->offset, (*tblist)->len);
}
         assert (tmp == *tblist);
         *tblist = tmp->next;
         tmp->next = tbinfo->freelist;
         tbinfo->freelist = tmp;
         tmp = *tblist;
         tbinfo->writebufs--;
      }
   }
/*
kprintf ("xio_tcpbuffer_countBufferedData: %d (acked_offset %d)\n", totallen, prunepoint);
*/
   return (totallen);
}


void xio_tcpbuffer_reclaimBuffers (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist)
{
   xio_tcpbuf_t *tmp;

   if (tblist) {
      while ((tmp = *tblist) != NULL) {
         *tblist = tmp->next;
         tmp->next = tbinfo->freelist;
         tbinfo->freelist = tmp;
         tbinfo->writebufs--;
      }

      //assert (tbinfo->writebufs == 0);
   }
}


int xio_tcpbuffer_getdata (xio_tbinfo_t *tbinfo, xio_tcpbuf_t * *tblist, char *buffer, int len)
{
   int donelen = 0;
   xio_tcpbuf_t *tmp;
/*
kprintf ("xio_tcpbuffer_getdata: len %d\n", len);
*/

   while ((len > 0) && (tmp = *tblist)) {
      int tmplen = min (len, tmp->len);
      bcopy (&tmp->data[tmp->offset], &buffer[donelen], tmplen);
      len -= tmplen;
      donelen += tmplen;
      tmp->len -= tmplen;
      tmp->offset += tmplen;
      if (tmp->len == 0) {
         *tblist = tmp->next;
         tmp->next = tbinfo->freelist;
         tbinfo->freelist = tmp;
         tbinfo->readbufs--;
      } else {
         assert (len == 0);
      }
   }

   return (donelen);
}


/* stuff included to assist in movement of buffered data... */

xio_tcpbuf_t * xio_tcpbuffer_yankBufPage (xio_tbinfo_t *tbinfo, xio_tcpbuf_t * *tblist)
{
   xio_tcpbuf_t *buf = *tblist;

   if (buf) {
      *tblist = buf->next;
//kprintf ("yankBufPage: buf %p, start %d, offset %d, len %d\n", buf, buf->start, buf->offset, buf->len);
      tbinfo->bufcount--;
      tbinfo->writebufs--;
   }

   return (buf);
}


void xio_tcpbuffer_shoveBufPage (xio_tbinfo_t *tbinfo, xio_tcpbuf_t * *tblist, xio_tcpbuf_t *buf)
{
   xio_tcpbuf_t *tmp = *tblist;

//kprintf ("shoveBufPage: tmp %p, buf %p, start %d, offset %d, len %d\n", tmp, buf, buf->start, buf->offset, buf->len);

   tbinfo->bufcount++;
   tbinfo->writebufs++;
   buf->next = NULL;
   buf->data = (char *)buf + ((uint)(buf->data) % NBPG);
   if (tmp == NULL) {
      *tblist = buf;
   } else {
      while (tmp->next) {
         assert ((tmp->start + tmp->offset + tmp->len) <= (buf->start + buf->offset));
         tmp = tmp->next;
      }
      assert ((tmp->start + tmp->offset + tmp->len) == (buf->start + buf->offset));
      tmp->next = buf;
   }
}


void xio_tcpbuffer_shoveFreeBufPage (xio_tbinfo_t *tbinfo, xio_tcpbuf_t *buf)
{
   buf->data = (char *) buf + sizeof (xio_tcpbuf_t);
   buf->next = tbinfo->freelist;
   tbinfo->freelist = buf;
   tbinfo->bufcount++;
}

