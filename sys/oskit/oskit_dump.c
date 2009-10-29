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
 * Dump a summary of the fdev hardware tree.
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

#include <oskit/com.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/device.h>
#include <oskit/dev/bus.h>

void oskit_dump_drivers(void)
{
	oskit_error_t rc;
	oskit_driver_t **drivers;
	int i, count;

	osenv_log(OSENV_LOG_INFO, "Currently registered device drivers:\n");

	rc = osenv_driver_lookup(&oskit_driver_iid, (void***)&drivers);
	if (OSKIT_FAILED(rc))
		return;
	count = rc;

	for (i = 0; i < count; i++) {
		oskit_devinfo_t info;
		rc = oskit_driver_getinfo(drivers[i], &info);
		if (OSKIT_SUCCEEDED(rc)) {
			osenv_log(OSENV_LOG_INFO, "  %-16s%s\n", info.name,
				info.description ? info.description : 0);
		}
		oskit_driver_release(drivers[i]);
	}

	osenv_mem_free(drivers, OSENV_AUTO_SIZE, 0);
}

#define LINE_LEN	80
#define INDENT		2
#define INDENT_MAX	20
#define DESCR_POS	30

/*
 * fix strncpy.
 */
static void dumpbus(oskit_bus_t *bus, int indent)
{
	oskit_device_t *dev;
	oskit_devinfo_t info;
	oskit_bus_t *sub;
	char pos[OSKIT_BUS_POS_MAX+1];
	char buf[LINE_LEN];
	int i, j;

	if (indent < INDENT_MAX)
		indent += INDENT;

	for (i = 0; bus->ops->getchild(bus, i, &dev, pos) == 0; i++) {

		/* Create an info line for this device */
		memset(buf, ' ', LINE_LEN-1);
		buf[LINE_LEN-1] = 0;
		memcpy(buf+indent, pos, j = strlen(pos));

		/* Add some more descriptive info to the info line */
		if (dev->ops->getinfo(dev, &info) == 0) {
			buf[indent+j] = '.';
			memcpy(buf+indent+j+1, info.name,
				strlen(info.name));

			strncpy(buf+DESCR_POS, info.description,
				LINE_LEN-DESCR_POS-1);
		}

		/* Output the line */
		osenv_log(OSENV_LOG_INFO, "%s\n", buf);

		/* If this is a bus, descend into it */
		if (dev->ops->query(dev, &oskit_bus_iid, (void**)&sub) == 0)
			dumpbus(sub, indent);

		/* Release the reference to this device node */
		dev->ops->release(dev);
	}

	/* Release the reference to this bus node */
	bus->ops->release(bus);
}

void oskit_dump_devices(void)
{
	osenv_log(OSENV_LOG_INFO, "Current hardware tree:\n");
	dumpbus(osenv_rootbus_getbus(), 0);
}

