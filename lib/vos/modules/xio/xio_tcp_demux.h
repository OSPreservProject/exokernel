
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


#ifndef __XIO_TCP_DEMUX_H__
#define __XIO_TCP_DEMUX_H__

#include "xio_tcp.h"
#include "xok/queue.h"

#define XIO_TCP_DEMUX_HASHSIZE	(1024/sizeof(void *))

#define	XIO_TCP_DEMUX_HASH(srctcp, dsttcp, hashsize) \
	((((uint16) (srctcp) + (uint16) (dsttcp)) + \
	 (((uint16) (srctcp) + (uint16) (dsttcp)) >> 8)) \
		& ((hashsize)-1))

LIST_HEAD(tcb_list, tcb);

typedef struct {
   struct tcb_list listenlist;
   int numhashed;
   uint hashsize;
   struct tcb_list hashtable[XIO_TCP_DEMUX_HASHSIZE];
} xio_dmxinfo_t;

#define xio_dmxinfo_size(hashsize) \
	(sizeof (xio_dmxinfo_t) - \
	 (XIO_TCP_DEMUX_HASHSIZE * sizeof (struct tcb_list)) + \
	 (((hashsize) ? (hashsize) : XIO_TCP_DEMUX_HASHSIZE) * sizeof (struct tcb_list)))


static inline int xio_tcp_demux_isconn (struct tcb *tcb)
{
  return (tcb->hashed);
}


void xio_tcp_demux_init (xio_dmxinfo_t *dmxinfo, uint hashsize);

int xio_tcp_demux_addconn (xio_dmxinfo_t *dmxinfo, struct tcb *tcb, int chkuniq);
void xio_tcp_demux_removeconn (xio_dmxinfo_t *dmxinfo, struct tcb *tcb);
struct tcb * xio_tcp_demux_searchconns (xio_dmxinfo_t *dmxinfo, char *buf);

int xio_tcp_demux_addlisten (xio_dmxinfo_t *dmxinfo, struct tcb *tcb, int chkuniq);
void xio_tcp_demux_removelisten (xio_dmxinfo_t *dmxinfo, struct tcb *tcb);
struct tcb * xio_tcp_demux_searchlistens (xio_dmxinfo_t *dmxinfo, char *buf);

#endif  /* __XIO_TCP_DEMUX_H__ */

