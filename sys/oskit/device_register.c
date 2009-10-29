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
#include <oskit/dev/dev.h>
#include <oskit/dev/device.h>

oskit_error_t
osenv_device_register(oskit_device_t *device,
		     const struct oskit_guid *iids, unsigned iid_count)
{
	oskit_error_t rc;
	int i;

	/* Register the basic device interface */
	rc = oskit_register(&oskit_device_iid, device);
	if (rc)
		return rc;

	/* Register the other supported interfaces */
	for (i = 0; i < iid_count; i++) {
		oskit_iunknown_t *iu;
		rc = oskit_device_query(device, &iids[i], (void**)&iu);
		if (rc)
			return rc;
		rc = oskit_register(&iids[i], iu);
		if (rc)
			return rc;
		oskit_iunknown_release(iu);
	}

	return 0;
}

oskit_error_t
osenv_device_unregister(oskit_device_t *device,
		       const struct oskit_guid *iids, unsigned iid_count)
{
	oskit_error_t rc;
	int i;

	/* Unregister the basic device interface */
	oskit_unregister(&oskit_device_iid, device);

	/* Unregister the other supported interfaces */
	for (i = 0; i < iid_count; i++) {
		oskit_iunknown_t *iu;
		rc = oskit_device_query(device, &iids[i], (void**)&iu);
		if (rc)
			return rc;
		rc = oskit_unregister(&iids[i], iu);
		if (rc)
			return rc;
		oskit_iunknown_release(iu);
	}

	return 0;
}

oskit_error_t
osenv_device_lookup(const struct oskit_guid *iid, void ***out_interface_array)
{
	return oskit_lookup(iid, out_interface_array);
}

