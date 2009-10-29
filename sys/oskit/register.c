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
 * Generic COM interface registration module.
 * Basically maintains a list of COM interface pointers
 * for a particular interface GUID (IID).
 * Given an IID, you can find all the registered COM interfaces with that IID.
 * Interfaces will be returned in the order in which they were registered.
 * It's harmless to register an interface multiple times;
 * only a single entry in the table will be retained.
 * Currently just does everything with simple lists;
 * if we end up having lots of objects, we may need smarter algorithms.
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

#include <oskit/com.h>

/* One of these nodes represents each registered COM interface (object) */
struct objnode {
	struct objnode *next;
	oskit_iunknown_t *intf;
};

/* We keep one iidnode for each unique IID we see */
struct iidnode {
	struct iidnode *next;
	oskit_guid_t iid;
	struct objnode *objs;
	int objcount;
};

static struct iidnode *iids;


oskit_error_t
oskit_register(const struct oskit_guid *iid, void *interface)
{
	oskit_iunknown_t *iu = (oskit_iunknown_t*)interface;
	struct iidnode *in;
	struct objnode *on, **onp;

	/* Find or create the appropriate iidnode */
	for (in = iids; ; in = in->next) {
		if (in == NULL) {
			in = malloc(sizeof(*in));
			if (in == NULL)
				return OSKIT_E_OUTOFMEMORY;
			in->iid = *iid;
			in->objs = NULL;
			in->objcount = 0;
			in->next = iids;
			iids = in;
			break;
		}
		if (memcmp(&in->iid, iid, sizeof(*iid)) == 0)
			break;
	}

	/* Make sure this interface isn't already registered */
	for (onp = &in->objs; *onp; onp = &(*onp)->next) {
		if ((*onp)->intf == interface)
			return 0;
	}

	/* Create a new objnode for this interface */
	on = malloc(sizeof(*on));
	if (on == NULL)
		return OSKIT_E_OUTOFMEMORY;
	on->next = NULL;
	on->intf = iu;	oskit_iunknown_addref(iu);
	*onp = on;
	in->objcount++;

	return 0;
}

oskit_error_t
oskit_unregister(const struct oskit_guid *iid, void *interface)
{
	struct iidnode *in;
	struct objnode *on, **onp;

	/* Find the appropriate iidnode */
	for (in = iids; ; in = in->next) {
		if (in == NULL)
			return OSKIT_E_INVALIDARG;
		if (memcmp(&in->iid, iid, sizeof(*iid)) == 0)
			break;
	}

	/* Find and remove the objnode */
	for (onp = &in->objs; ; onp = &on->next) {
		on = *onp;
		if (on == NULL)
			return OSKIT_E_INVALIDARG;
		if (on->intf == interface)
			break;
	}
	*onp = on->next;
	oskit_iunknown_release(on->intf);
	free(on);
	in->objcount--;

	return 0;
}

oskit_error_t
oskit_lookup(const oskit_guid_t *iid,
	    void ***out_interface_array)
{
	struct iidnode *in;
	struct objnode *on;
	void **arr;
	int i;

	/* Find the appropriate iidnode */
	for (in = iids; ; in = in->next) {
		if (in == NULL) {
			*out_interface_array = NULL;
			return 0;
		}
		if (memcmp(&in->iid, iid, sizeof(*iid)) == 0)
			break;
	}
	if (in->objcount == 0) {
		*out_interface_array = NULL;
		return 0;
	}

	/* Allocate an array to hold the interface pointers to return */
	arr = malloc(sizeof(*arr)*in->objcount);
	if (arr == NULL)
		return OSKIT_E_OUTOFMEMORY;

	/* Fill it in */
	for (i = 0, on = in->objs; i < in->objcount; i++, on = on->next) {
		assert(on != NULL);
		arr[i] = on->intf;
		oskit_iunknown_addref(on->intf);
	}
	assert(on == NULL);

	*out_interface_array = arr;
	return in->objcount;
}

oskit_error_t
oskit_lookup_first(const oskit_guid_t *iid, void **out_intf)
{
	struct iidnode *in;
	oskit_iunknown_t *intf;

	/* Find the appropriate iidnode */
	for (in = iids; ; in = in->next) {
		if (in == NULL) {
			*out_intf = NULL;
			return 0;
		}
		if (memcmp(&in->iid, iid, sizeof(*iid)) == 0)
			break;
	}
	if (in->objcount == 0) {
		*out_intf = NULL;
		return 0;
	}

	*out_intf = intf = in->objs->intf;
	oskit_iunknown_addref(intf);
	return 0;
}

