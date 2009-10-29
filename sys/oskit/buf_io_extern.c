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
 * A bufio implementation maintaining an external buffer
 */

/* xok includes */
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
#include <oskit/io/bufio.h>
#include <oskit/dev/dev.h>

struct bufio_extern {
	oskit_bufio_t	ioi;		/* COM I/O Interface */
	unsigned 	count;		/* reference count */
	void		*buf;		/* the buffer */
	oskit_size_t	size;		/* size of buffer */
	void		(*free)(void *cookie, void *buf);	/* free */
	void		*cookie;
};
typedef struct bufio_extern bufio_extern_t;


/* 
 * Query a buffer I/O object for its interfaces.
 */
static OSKIT_COMDECL
bufio_query(oskit_bufio_t *io, const oskit_iid_t *iid, void **out_ihandle) 
{
	struct bufio_extern *b = (struct bufio_extern *)io;

	assert(b != NULL);
	assert(b->count != 0);

	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_bufio_iid, sizeof(*iid)) == 0) {
		*out_ihandle = &b->ioi;
		++b->count;
		
		return 0;
	}

	*out_ihandle = NULL;
	return OSKIT_E_NOINTERFACE;
}

/*
 * Clone a reference to a device's block I/O interface.
 */
static OSKIT_COMDECL_U
bufio_addref(oskit_bufio_t *io)
{
	struct bufio_extern *b = (struct bufio_extern *)io;

	assert(b != NULL);
	assert(b->count != 0);

	return ++b->count;
}

static OSKIT_COMDECL_U
bufio_release(oskit_bufio_t *io)
{
	struct bufio_extern *b = (struct bufio_extern *)io;
	unsigned newcount;

	assert(b != NULL);
	assert(b->count != 0);

	if ((newcount = --b->count) == 0) {
		if (b->free)
			b->free(b->cookie, b->buf);

		/* maybe we should use safe free? */
		osenv_mem_free(b, OSENV_PHYS_WIRED | OSENV_AUTO_SIZE, 0);
		/*		sfree(b, sizeof *b);*/
	}

	return newcount;
}

/*
 * Return the block size of this blkio object - always 1.
 */
static OSKIT_COMDECL_U
bufio_getblocksize(oskit_bufio_t *io)
{
	return 1;
}

/*
 * Copy data from a user buffer into kernel's address space.
 */
static OSKIT_COMDECL
bufio_read(oskit_bufio_t *io, void *dest, oskit_off_t offset, 
	oskit_size_t count, oskit_size_t *out_actual)
{
	struct bufio_extern *b = (struct bufio_extern *)io;

	assert(b != NULL);
	assert(b->count != 0);

	if (offset >= b->size)
		return OSKIT_EINVAL;
	if (offset + count > b->size)
		count = b->size - offset;

	memcpy(dest, b->buf + offset, count);

	*out_actual = count;
	return 0;
}

/*
 * Copy data from kernel address space to a user buffer.
 */
static OSKIT_COMDECL
bufio_write(oskit_bufio_t *io, const void *src, oskit_off_t offset,
	    oskit_size_t count, oskit_size_t *out_actual)
{
	struct bufio_extern *b = (struct bufio_extern *)io;

	assert(b != NULL);
	assert(b->count != 0);

	if (offset >= b->size)
		return OSKIT_EINVAL;
	if (offset + count > b->size)
		count = b->size - offset;

	memcpy(b->buf + offset, src, count);

	*out_actual = count;
	return 0;
}

/*
 * Return the size of this buffer.
 */
static OSKIT_COMDECL
bufio_getsize(oskit_bufio_t *io, oskit_off_t *out_size)
{
	struct bufio_extern *b = (struct bufio_extern *)io;

	assert(b != NULL);
	assert(b->count != 0);

	*out_size = b->size;
	return 0;
}

static OSKIT_COMDECL
bufio_setsize(oskit_bufio_t *io, oskit_off_t size)
{
	struct bufio_extern *b = (struct bufio_extern *)io;

	assert(b != NULL);
	assert(b->count != 0);

	return OSKIT_E_NOTIMPL;
}


static OSKIT_COMDECL
bufio_map(oskit_bufio_t *io, void **out_addr,
	  oskit_off_t offset, oskit_size_t count)
{
  /*struct bufio_extern *b = (struct bufio_extern *)io;*/
	struct bufio_extern *b;
	//	printf("entered map\n");
	b = (struct bufio_extern *)io;
	assert(b != NULL);
	assert(b->count != 0);
	assert(out_addr != NULL);
	//	printf("bufio_map called io=%x\n", (unsigned) io);
	//	printf("map out_adder = %x\n", (unsigned) out_addr);
	if (offset + count > b->size)
		return OSKIT_EINVAL;

	*out_addr = b->buf + offset; 

	return 0;
}

/* 
 * XXX These should probably do checking on their inputs so we could 
 * catch bugs in the code that calls them.
 */

static OSKIT_COMDECL
bufio_unmap(oskit_bufio_t *io, 
	void *addr, oskit_off_t offset, oskit_size_t count)
{
	return 0;
}


static OSKIT_COMDECL 
bufio_wire(oskit_bufio_t *io, oskit_addr_t *out_physaddr,
	   oskit_off_t offset, oskit_size_t count)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL 
bufio_unwire(oskit_bufio_t *io, oskit_addr_t phys_addr,
	     oskit_off_t offset, oskit_size_t count)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
bufio_copy(oskit_bufio_t *io, oskit_off_t offset, oskit_size_t count,
	   oskit_bufio_t **out_io)
{
	return OSKIT_E_NOTIMPL;
}


static struct oskit_bufio_ops bio_ops = {
	bufio_query, bufio_addref, bufio_release,
	bufio_getblocksize,
	bufio_read, bufio_write,
	bufio_getsize, bufio_setsize,
	bufio_map, bufio_unmap,
	bufio_wire, bufio_unwire,
	bufio_copy
};


oskit_bufio_t *
oskit_create_extern_bufio(void *buf, oskit_size_t size,
	void (*free)(void *cookie, void *buf), void *cookie) 
{
	bufio_extern_t *b;

	/* maybe we should use safe malloc? */
	//	printf("inside create)bufio\n");
	b = (struct bufio_extern *) osenv_mem_alloc(sizeof(*b), 
															  OSENV_PHYS_WIRED | OSENV_AUTO_SIZE, 
															  0);
	/*	b = (struct bufio_extern *)smalloc(sizeof(*b));*/
	//	printf("inside create bufio b=%x\n", (unsigned) b);
	if (b == NULL)
		return NULL;
	
	//	printf("accessing b\n");
	b->ioi.ops = &bio_ops;
	b->count = 1;
	b->size = size;
	b->buf = buf;
	b->cookie = cookie;
	b->free = free;
	//	printf("leaving create_extern_bufio\n");
	return &b->ioi;
}




