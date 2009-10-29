
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

//#undef PRINTF_LEVEL
//#define PRINTF_LEVEL 99
#include <xok/ae_recv.h>
#include <exos/net/ae_net.h>
#include <exos/process.h>
#include <exos/osdecl.h>		/* for ADD_UVA */
#include <xok/env.h>
#include <exos/uwk.h>

#include "udp_socket.h"
#include "errno.h"

#include <exos/debug.h>

#ifndef MIN
#define MIN(x,y) ((x < y) ? x : y)
#endif

#define kprintf(format,args...) 
/* really need to isolate these.  this is embarrasing. */

int
fd_udp_read(struct file *filp, char *buffer, int length, int nonblocking) {
  socket_data_p sock;
    
  DPRINTF(CLU_LEVEL,("fd_udp_read\n"));
  demand(filp, bogus filp);
  
  if (length > 1472) {
    kprintf("Warning trying to read upd packet larger than 1472: %d,\nreassembly not supported yet\n",length);
    errno = EMSGSIZE;
    return -1;
  }
  sock = GETSOCKDATA(filp);
  
  if (sock->tosockaddr.sin_port == 0) {
    errno = ENOTCONN;
    return -1;
  }
  return fd_udp_recvfrom(filp,buffer,length,nonblocking,0,0,0);
}

int
fd_udp_recvfrom(struct file *filp, void *buffer, int length, int nonblocking,
	     unsigned flags, struct sockaddr *reg_rfrom, int *rfromlen) {
    struct eiu *eiu;
    ringbuf_p r, start;
    socket_data_p sock;
    struct sockaddr_in *rfrom;
    int return_length;
    int badfrag = 0;
    ringbuf_p cur, start2;
    int frags_i = 0, i, j, lowest, lowest_i, offset, begin_o, end_o, copied, sz;
    char *src, *dst;
    ringbuf_p frags[8]; //8192

    rfrom = (struct sockaddr_in *)reg_rfrom;

//    kprintf("(R%d",__envid);

    DPRINTF(CLU_LEVEL,("fd_udp_recvfrom\n"));
    demand(filp, bogus filp);

    sock = GETSOCKDATA(filp);
    /* remove this */
    r = sock->recvfrom.r;
	
    start = sock->recvfrom.r;

    do {
	eiu = (struct eiu *)r->data;
	if (!eiu) {
	    kprintf("recvfrom.data is corrupted, filp: %08x\n",
		   (int)filp);
	    assert(0);
	}

	if (r->poll == 0 && CHECKNB(filp)) {
	    errno = EWOULDBLOCK;
	    return -1;
	}
	wk_waitfor_value_neq ((unsigned int *)&r->poll, 0, UDP_CAP);

	if((ntohs(eiu->ip.fragoffset) & IP_FRG_MF) != 0) {
	    /* do frag stuff */

	    struct eiu *eiu2;
	    int id; /* get ipid, assume other identifying things are same for one socket */

	    frags_i = 0;
kprintf("got frag %08x\n", (int)eiu);

	    eiu2 = (struct eiu *)r->data;
	    if (!eiu2) {
		kprintf("2recvfrom.data is corrupted, filp: %08x\n",
			(int)filp);
		assert(0);
	    }
	    id = eiu2->ip.identification;

kprintf("finding parts for frag %d in ring %d length %d\n", id, sock->recvfrom.ring_id, length);
	    /* find correct frags */
	    start2 = cur = sock->recvfrom.r;
	    do {
eiu2 = (struct eiu *)cur->data;
		if(cur->poll != 0) {
kprintf("entry 0x%08x id %d poll %d offset %d length %d\n", (int)cur, id, cur->poll, (ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF) * 8, ntohs(eiu2->ip.totallength));
		    eiu2 = (struct eiu *)cur->data;
		    if (!eiu2) {
			kprintf("3recvfrom.data is corrupted, filp: %08x\n",
				(int)filp);
			assert(0);
		    }
		    if(eiu2->ip.identification == id) {
//kprintf("add 0x%08x id %d offset %d length %d\n", (int)cur, id, (ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF) * 8, ntohs(eiu2->ip.totallength));
			frags[frags_i++] = cur;
		    }
		} //else
//		    if(eiu2->ip.identification == id)
//kprintf("match, but not there? 0x%08x id %d offset %d length %d\n", (int)cur, id, (ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF) * 8, ntohs(eiu2->ip.totallength));
		cur = cur->next;
	    } while (cur != start2);

	    /* sort frags */
	    for(i = 0; i < frags_i; i++) {
		lowest = 8192;
		lowest_i = -1;
		for(j = i; j < frags_i; j++) {
		    eiu2 = (struct eiu *)frags[j]->data;
		    offset = ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF;
		    if(offset < lowest) {
			lowest = offset;
			lowest_i = j;
			cur = frags[j];
		    }
		}
		if(lowest_i < 0) break; /* no more frags */
		frags[lowest_i] = frags[i];
		frags[i] = cur;
	    }
kprintf("eh? lowest=%d\n", 8*ntohs(((struct eiu *)frags[0]->data)->ip.fragoffset) & IP_FRAGOFF);
	    /* look to see if we have all */
	    for(i = 0; i < (frags_i-1); i++) {
		eiu2 = (struct eiu *)frags[i]->data;
		offset = (ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF) * 8;
		end_o = offset + ntohs(eiu2->ip.totallength) - sizeof(struct ip);
		eiu2 = (struct eiu *)frags[i+1]->data;
		begin_o = (ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF) * 8;
kprintf("end_o %d begin_o %d\n", end_o, begin_o);
		if(end_o != begin_o) // || !(ntohs(eiu2->ip.fragoffset) & IP_FRG_MF))
		    break;
	    }

	    eiu2 = (struct eiu *)frags[i]->data;
	    offset = (ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF) * 8;
	    end_o = offset + ntohs(eiu2->ip.totallength) - sizeof(struct ip);
	    eiu2 = (struct eiu *)frags[0]->data;
//    return_length = MIN(length, ntohs(eiu2->udp.length) - sizeof(struct udp));
	    if(end_o != ntohs(eiu2->udp.length)) { 
		/* if not, move frags to end and bail
		sock->recvfrom.r = sock->recvfrom.r->next;
		r = sock->recvfrom.r;
		badfrag = 1; */
	for(i=0; i<frags_i; i++) {
		frags[i]->poll = 0;
		if(frags[i] == sock->recvfrom.r)
			sock->recvfrom.r = sock->recvfrom.r->next;
	}
kprintf("missing. got %d of %d in %d . eiu = 0x%08x. udp source=0x%08x dest=0x%08x chksum 0x%08x\n", end_o, ntohs(eiu2->udp.length), frags_i, (int)eiu2, ntohs(eiu2->udp.src_port), ntohs(eiu2->udp.dst_port), ntohs(eiu2->udp.cksum));
		r = sock->recvfrom.r;
	if (r->poll == 0 && CHECKNB(filp)) {
	    errno = EWOULDBLOCK;
	    return -1;
	}
	wk_waitfor_value_neq ((unsigned int *)&r->poll, 0, UDP_CAP);
		badfrag = 1;
	    }

kprintf("memcpying badfrag=%d\n", badfrag);
	    if(badfrag != 1) {
		eiu2 = (struct eiu *)frags[0]->data;
		/* copy frags into buffer, return */
		copied = 0;
		for(i = 0; i < frags_i && length - copied > 0; i++) {
		  eiu2 = (struct eiu *)frags[i]->data;
		  offset = (ntohs(eiu2->ip.fragoffset) & IP_FRAGOFF) * 8;
		  src = &eiu2->data[0];
		  dst = (char *)buffer+offset;
		  sz = ntohs(eiu2->ip.totallength) - sizeof(struct ip) - sizeof(struct udp);
		  if(i!=0) {
			dst -= sizeof(struct udp);
			src -= sizeof(struct udp);
			sz += sizeof(struct udp);
		  }
		  if(sz > length - copied)
			sz = length - copied;
		  memcpy(dst, src, sz);
		  copied += sz;
kprintf("copied %d at offset %d to %08x\n", ntohs(eiu2->ip.totallength) - sizeof(struct ip), offset, (int)buffer);
		}
    return_length = MIN(length, ntohs(eiu->udp.length) - sizeof(struct udp));
kprintf("length %d udp %d\n", length, ntohs(eiu->udp.length)-sizeof(struct udp));
kprintf("totallength %d udplength %d\n", return_length, ntohs(eiu->udp.length));
		sys_self_dpf_free_state(UDP_CAP, sock->demux_id, id);
	    }
	} else {
	    /*  pull the message out */
	    memcpy((char *)buffer, &eiu->data[0],
		   MIN(length, ntohs(eiu->udp.length) - sizeof(struct udp)));
	}
    } while(badfrag && (r != start));
    return_length = MIN(length, ntohs(eiu->udp.length) - sizeof(struct udp));

    if (rfrom != (struct sockaddr_in *)0)
    {
	rfrom->sin_family = 2;
	*rfromlen = sizeof(struct sockaddr);
	memcpy((char *)&rfrom->sin_addr,
	       (char *)&eiu->ip.source[0],
	       sizeof eiu->ip.destination);
	rfrom->sin_port = eiu->udp.src_port;
    }
//    kprintf(")");
    if (!(flags & MSG_PEEK)) { /* we don't add new structure */
	r->poll = 0;
	sock->recvfrom.r = r->next;
	for(i=0; i<frags_i; i++) {
		frags[i]->poll = 0;
		if(frags[i] == sock->recvfrom.r)
			sock->recvfrom.r = sock->recvfrom.r->next;
	}
    }
    return return_length;
}

