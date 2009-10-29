/*
 * Copyright (c) 1996, 1998 University of Utah and the Flux Group.
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
 * Default implementation of the root device node "pseudo-bus".
 */
#include <xok/types.h>
#include <xok/printf.h> 
#include <xok/defs.h>
#include <xok/pmap.h>
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

/* List of device nodes attached to the root bus */
static struct node {
	struct node *next;
	oskit_device_t *dev;
	char pos[OSKIT_BUS_POS_MAX];
} *nodes;

static OSKIT_COMDECL
query(oskit_bus_t *bus, const oskit_iid_t *iid, void **out_ihandle)
{
	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_driver_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_device_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_bus_iid, sizeof(*iid)) == 0) {
		*out_ihandle = bus;
		return 0;
	}
	return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
addref(oskit_bus_t *bus)
{
	return 1;
}

static OSKIT_COMDECL_U
release(oskit_bus_t *bus)
{
	return 1;
}

static OSKIT_COMDECL
getinfo(oskit_bus_t *bus, oskit_devinfo_t *out_info)
{
	out_info->name = "root";
	out_info->description = "Hardware Root";
	out_info->vendor = NULL;
	out_info->author = "Flux Project, University of Utah";
	out_info->version = NULL; /* OSKIT_VERSION or somesuch? */
	return 0;
}

static OSKIT_COMDECL
getdriver(oskit_bus_t *dev, oskit_driver_t **out_driver)
{
	/*
	 * Our driver node happens to be the same COM object
	 * as our device node, making this really easy...
	 */
	*out_driver = (oskit_driver_t*)dev;
	return 0;
}

static OSKIT_COMDECL
getchild(oskit_bus_t *bus, unsigned idx,
	  struct oskit_device **out_fdev, char *out_pos)
{
	struct node *n;

	/* Find the node with this index number */
	for (n = nodes; ; n = n->next) {
		if (n == NULL)
			return OSKIT_E_DEV_NOMORE_CHILDREN;
		if (idx == 0)
			break;
		idx--;
	}

	/* Return a reference to the device attached here */
	*out_fdev = n->dev;
	oskit_device_addref(n->dev);
	strcpy(out_pos, n->pos);
	return 0;
}

static OSKIT_COMDECL
probe(oskit_bus_t *bus)
{
	return OSKIT_E_NOTIMPL;
}

static struct oskit_bus_ops ops = {
	query,
	addref,
	release,
	getinfo,
	getdriver,
	getchild,
	probe
};
static oskit_bus_t bus = { &ops };

struct oskit_bus *
osenv_rootbus_getbus(void)
{
	return &bus;
}

oskit_error_t
osenv_rootbus_addchild(oskit_device_t *dev)
{
	struct node *n, *nn;
	oskit_devinfo_t info;
	oskit_error_t err;
	int pos_n;

	/* XXX check already present */

	/*
	 * Find the device's short name;
	 * we'll use that to generate the position string.
	 */
	err = oskit_device_getinfo(dev, &info);
	if (err)
		return err;

	/* Create a new device node */
	n = osenv_mem_alloc(sizeof(*n), OSENV_PHYS_WIRED, 0);
	if (n == NULL)
		return OSKIT_E_OUTOFMEMORY;
	n->next = nodes; nodes = n;
	n->dev = dev; oskit_device_addref(dev);

	/*
	 * Produce a unique position string for this device node.
	 * If there are multiple nodes with the same name,
	 * tack on an integer to make them unique.
	 */
	assert(OSKIT_DEVNAME_MAX < OSKIT_BUS_POS_MAX);
	assert(strlen(info.name) <= OSKIT_DEVNAME_MAX);
	pos_n = 0;
	strcpy(n->pos, info.name);
	retry:
	for (nn = n->next; nn; nn = nn->next) {
		if (strncmp(n->pos, nn->pos, OSKIT_BUS_POS_MAX) == 0) {
		  sprintf(n->pos, "%s%d", info.name, ++pos_n);
			goto retry;
		}
	}

	return 0;
}

oskit_error_t
osenv_rootbus_remchild(oskit_device_t *dev)
{
	struct node **np, *n;

	for (np = &nodes; (n = *np) != NULL; np = &n->next) {
		/*
		 * XXX Technically this isn't correct COM procedure;
		 * we should be comparing the IUnknown interfaces...
		 */
		if (n->dev == dev) {
			*np = n->next;
			oskit_device_release(n->dev);
			osenv_mem_free(n, OSENV_PHYS_WIRED, sizeof(*n));
			return 0;
		}
	}

	return OSKIT_E_DEV_NOSUCH_CHILD;
}

