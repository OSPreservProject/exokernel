/*
 * Copyright (c) 1996-1998 University of Utah and the Flux Group.
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
 * I/O port allocation/deallocation.
 * 0 bit indicates unallocated; 1 is allocated
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

#include <oskit/machine/types.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/isa.h>

#ifndef NBPW
#define NBPW	32
#endif

/* Will need to fix for different ranges; this works on a PC */
#define NPORTS	65536

static unsigned port_bitmap[NPORTS / NBPW];

oskit_bool_t
osenv_io_avail(oskit_addr_t port, oskit_size_t size)
{
	unsigned i, j, bit;

#ifdef DEBUG
	/* I'm paranoid here. oskit_addr_t & oskit_size_t are unsigned */
	if ((port >= NPORTS) || (size >= NPORTS) || (port + size >= NPORTS)) {
#else
	if (port + size >= NPORTS) {
#endif
		osenv_log(OSENV_LOG_WARNING,
			 "%s:%d: osenv_io_avail: bad port range [0x%x-0x%x]\n", 
			 __FILE__, __LINE__, port, port+size);
		return 0;	/* can't be free */
	}

	i = port / NBPW;
	bit = 1 << (port % NBPW);
	for (j = 0; j < size; j++) {
		if (port_bitmap[i] & bit)
			return (0);
		bit <<= 1;
		if (bit == 0) {
			bit = 1;
			i++;
		}
	}
	return (1);	/* is free */
}

oskit_error_t
osenv_io_alloc(oskit_addr_t port, oskit_size_t size)
{
	unsigned i, j, bit;

	if (!osenv_io_avail(port, size)) {
		osenv_log(OSENV_LOG_WARNING,
			 "%s:%d: osenv_io_alloc: bad port range [0x%x-0x%x]\n",
			__FILE__, __LINE__, port, port+size);
		/* what error codes should this use? */
		return -1;
	}
	i = port / NBPW;
	bit = 1 << (port % NBPW);
	for (j = 0; j < size; j++) {
		port_bitmap[i] |= bit;
		bit <<= 1;
		if (bit == 0) {
			bit = 1;
			i++;
		}
	}
	return 0;
}

void
osenv_io_free(oskit_addr_t port, oskit_size_t size)
{
	unsigned i, j, bit;

#ifdef DEBUG
	if ((port >= NPORTS) || (size >= NPORTS) || (port + size >= NPORTS)) {
#else
	if (port + size >= NPORTS) {
#endif
		osenv_log(OSENV_LOG_WARNING,
			"%s:%d: osenv_io_free: bad port range [0x%x-0x%x]\n",
			 __FILE__, __LINE__, port, port+size);
		return;
	}
	i = port / NBPW;
	bit = 1 << (port % NBPW);
	for (j = 0; j < size; j++) {
		if ((port_bitmap[i] & bit) == 0) {
			osenv_log(OSENV_LOG_WARNING,
				"%s:%d: osenv_io_free: port 0x%x not allocated\n",
				__FILE__, __LINE__, port /* +j */);
		}
		port_bitmap[i] &= ~bit;
		bit <<= 1;
		if (bit == 0) {
			bit = 1;
			i++;
		}
	}
}

