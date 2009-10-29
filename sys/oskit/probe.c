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
 * This routine repeatedly walks through the device tree
 * and calls all the buses' probe routines
 * until no more devices can be found.
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

#include <oskit/dev/dev.h>
#include <oskit/dev/device.h>
#include <oskit/dev/bus.h>

static int probe(oskit_bus_t *bus)
{
	int found = 0;
	oskit_device_t *dev;
	oskit_bus_t *sub;
	char pos[OSKIT_BUS_POS_MAX+1];
	int i;
	
	printf("doing a probe\n");
	/* Probe this bus */
	if (oskit_bus_probe(bus) > 0)
		found = 1;
	printf("found a bus, %d\n", found);
	/* Find any child buses dangling from this bus */
	for (i = 0; oskit_bus_getchild(bus, i, &dev, pos) == 0; i++) {
		if (oskit_device_query(dev, &oskit_bus_iid, (void**)&sub) == 0)
			probe(sub);
		oskit_device_release(dev);
	}
	/* printf("finished children\n"); */
	/* Release the reference to this bus node */
	oskit_bus_release(bus);
	/* printf("release bus.\n"); */
	return found;
}

void oskit_dev_probe(void)
{
	while (probe(osenv_rootbus_getbus()));
}


