/*
 * Copyright (c) 1997-1998 University of Utah and the Flux Group.
 * All rights reserved.
 * 
 * The following are the copyrights and redistribution conditions that apply
 * to this portion of the OSKit software.  To arrange for alternate terms,
 * contact the University at csl-dist@cs.utah.edu or +1-801-585-3271.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that:
 * 1. These copyright, permission, and disclaimer notices are retained
 *    in all source files and reproduced in accompanying documentation.
 * 2. The name of the University may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 3. Redistributions in any form must be accompanied by information on
 *    how to obtain complete source code both for this software and for any
 *    accompanying software that uses this software.  The source code must
 *    either be included in the distribution or be available at no more than
 *    the cost of distribution plus a nominal fee, and must be freely
 *    redistributable under reasonable conditions.
 * 
 * THIS SOFWARE IS PROVIDED "AS IS" AND WITHOUT ANY WARRANTY, INCLUDING
 * WITHOUT LIMITATION THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THE COPYRIGHT HOLDER DISCLAIMS
 * ALL LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.  NO USE OF THIS SOFTWARE IS AUTHORIZED
 * EXCEPT UNDER THIS DISCLAIMER.
 */
/*
 * Default netio object implementation.
 * Besides providing a (slightly) useful service,
 * this module serves as an example of how to implement a netio object.
 */
#include <xok/types.h>
#include <xok/printf.h> 

#include <xok/defs.h>
#include <xok/pmap.h>
#include <xok_include/assert.h>
#include <xok_include/errno.h>
#include <xok/sysinfo.h>
#include <xok/pkt.h>
#include <xok/bios32.h>
#include <machine/param.h>
#include <machine/endian.h>
#include <xok/ae_recv.h>
#include <xok/sys_proto.h>
#include <xok_include/string.h>
#include <xok/trap.h>
#include <xok_include/string.h>

#include <oskit/io/netio.h>

struct client_netio {
	oskit_netio_t	ioi;		/* COM I/O Interface */
	unsigned 	count;		/* reference count */
	oskit_error_t	(*func)(void *data, oskit_bufio_t *b, oskit_size_t size);
	void		*data;
	void		(*cleanup)(void *data); /* cleanup destructor */
};
typedef struct client_netio client_netio_t;

/*
 * Query a net I/O object for its interfaces.
 * This is extremely easy because we only export one interface
 * (plus its base type, IUnknown).
 */
static OSKIT_COMDECL
net_query(oskit_netio_t *io, const oskit_iid_t *iid, void **out_ihandle)
{
	struct client_netio *po = (struct client_netio *)io;

	assert(po != NULL);
	assert(po->count != 0);

	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_netio_iid, sizeof(*iid)) == 0) {
		*out_ihandle = &po->ioi;
		++po->count;
		return 0;
	}

	*out_ihandle = NULL;
	return OSKIT_E_NOINTERFACE;
}


/*
 * Clone a reference to a netio object
 */
static OSKIT_COMDECL_U
net_addref(oskit_netio_t *io)
{
	struct client_netio *po = (struct client_netio *)io;

	assert(po != NULL);
	assert(po->count != 0);

	return ++po->count;
}


/*
 * Close ("release") a netio object.
 */
static OSKIT_COMDECL_U
net_release(oskit_netio_t *io)
{
	struct client_netio *po = (struct client_netio *)io;
	unsigned newcount;

	assert(po != NULL);
	assert(po->count != 0);

	newcount = po->count - 1;
	if (newcount == 0) {
		if (po->cleanup)
			po->cleanup(po->data);
		free(po);
		return 0;
	}

	return po->count = newcount;
}


/*
 * Receive a packet and forward it on to the callback function.
 */
static OSKIT_COMDECL
net_push(oskit_netio_t *ioi, oskit_bufio_t *b, oskit_size_t pkt_size)
{
	struct client_netio *po = (struct client_netio *)ioi;

	assert(po != NULL);
	assert(po->count != 0);

	return po->func(po->data, b, pkt_size);
}

static 
struct oskit_netio_ops client_netio_ops = {
	net_query,
	net_addref, 
	net_release,
	net_push
};

oskit_netio_t *
oskit_netio_create_cleanup(oskit_error_t (*func)(void *data, oskit_bufio_t *b,
				   oskit_size_t pkt_size),
	      void *data, void (*destructor)(void *data))
{
	struct client_netio *c;

	c = (struct client_netio *)malloc(sizeof(*c));
        if (c == NULL)
                return NULL;

        c->ioi.ops = &client_netio_ops;
        c->count = 1;
	c->func = func;
	c->data = data;
	c->cleanup = destructor;

        return &c->ioi;
}

oskit_netio_t *
oskit_netio_create(oskit_error_t (*func)(void *data, oskit_bufio_t *b,
				   oskit_size_t pkt_size),
	      void *data)
{
	return oskit_netio_create_cleanup(func, data, NULL);
}

