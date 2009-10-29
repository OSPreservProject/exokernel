
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


#ifndef __XIO_TCPBUFFER_H__
#define __XIO_TCPBUFFER_H__

#include <sys/types.h>

typedef struct tcpbuffer {
   struct tcpbuffer *next;
   int maxlen;
   int start;		/* send_offset of first byte for reads/writes */
   int offset;		/* current offset in buffer */
   int len;		/* remaining number of bytes in buffer */
   char *data;
} xio_tcpbuf_t;

#define XIO_TCPBUFFER_ALLOCSIZE		4096
#define XIO_TCPBUFFER_DATAPERBUF	(XIO_TCPBUFFER_ALLOCSIZE - sizeof(struct tcpbuffer))

	/* structure containing meta tcpbuffer state */
typedef struct {
   struct tcpbuffer *freelist;
   int bufcount;
   int writebufs;
   int readbufs;
   char * (*pagealloc)(void *info, int len);
} xio_tbinfo_t;

	/* structure containing tcpbuffer listhead info */
typedef struct {
   struct tcpbuffer *buffers;
   int bufcount;
   xio_tbinfo_t *tbinfo;
} xio_tblist_t;


/****** prototypes for functions in xio_tcpbuffer.c *******/

void xio_tcpbuffer_init (xio_tbinfo_t *tbinfo, char * (*page_alloc)(void *info, int len));
int xio_tcpbuffer_putdata (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist, char *buffer, int len, int offset, int prunepoint);
int xio_tcpbuffer_countBufferedData (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist, int prunepoint);
void xio_tcpbuffer_reclaimBuffers (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist);

//int xio_tcpbuffer_gotdata (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist, char *data, int len);
int xio_tcpbuffer_getdata (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist, char *buffer, int len);

xio_tcpbuf_t * xio_tcpbuffer_yankBufPage (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist);
void xio_tcpbuffer_shoveBufPage (xio_tbinfo_t *tbinfo, xio_tcpbuf_t **tblist, xio_tcpbuf_t *buf);
void xio_tcpbuffer_shoveFreeBufPage (xio_tbinfo_t *tbinfo, xio_tcpbuf_t *buf);

#endif  /*  __XIO_TCPBUFFER_H__ */

