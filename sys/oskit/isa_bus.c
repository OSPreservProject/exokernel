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
 * Basic ISA bus "device driver" for x86 PCs.
 * Implements the ISA bus device node and corresponding COM interface.
 */

/*
 * ExoKernel include files
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


#include <oskit/boolean.h>
#include <oskit/com.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/device.h>
#include <oskit/dev/bus.h>
#include <oskit/dev/membus.h>
#include <oskit/dev/isa.h>
#include <oskit/dev/native.h>
#include <oskit/c/string.h>

/*
 * Since this "driver" only supports one ISA bus in the system,
 * we only need to export one device node object,
 * so everything can be static.
 * Since everything's static, we don't have to do reference counting.
 */

/* List of child devices attached to this bus, in no particular order */
static struct busnode {
	struct busnode *next;
	oskit_u32_t addr;
	oskit_device_t *dev;
} *nodes;


static OSKIT_COMDECL
query(oskit_isabus_t *bus, const oskit_iid_t *iid, void **out_ihandle)
{
	if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_driver_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_device_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_bus_iid, sizeof(*iid)) == 0 ||
	    memcmp(iid, &oskit_isabus_iid, sizeof(*iid)) == 0) {
		*out_ihandle = bus;
		return 0;
	}
	return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
addref(oskit_isabus_t *bus)
{
	return 1;
}

static OSKIT_COMDECL_U
release(oskit_isabus_t *bus)
{
	return 1;
}

static OSKIT_COMDECL
getinfo(oskit_isabus_t *bus, oskit_devinfo_t *out_info)
{
	out_info->name = "isa";
	out_info->description = "Industry Standard Architecture (ISA) Bus";
	out_info->vendor = NULL;
	out_info->author = "Flux Project, University of Utah";
	out_info->version = NULL; /* OSKIT_VERSION or somesuch? */
	return 0;
}

static OSKIT_COMDECL
getdriver(oskit_isabus_t *dev, oskit_driver_t **out_driver)
{
	/*
	 * Our driver node happens to be the same COM object
	 * as our device node, making this really easy...
	 */
	*out_driver = (oskit_driver_t*)dev;
	return 0;
}

/*** Bus device registration ***/

static OSKIT_COMDECL
getchildio(oskit_isabus_t *bus, oskit_u32_t idx, oskit_device_t **out_fdev,
		oskit_u32_t *out_addr)
{
	struct busnode *n;

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
	*out_addr = n->addr;
	return 0;
}

static OSKIT_COMDECL
getchildaddr(oskit_isabus_t *bus, oskit_u32_t idx, oskit_device_t **out_fdev,
		oskit_addr_t *out_addr)
{
	oskit_error_t err;
	oskit_u32_t addr;

	err = getchildio(bus, idx, out_fdev, &addr);
	if (err)
		return err;

	*out_addr = 0;	/* XX supply a memory address if memory-mapped? */
	return 0;
}

static OSKIT_COMDECL
getchild(oskit_isabus_t *bus, oskit_u32_t idx, oskit_device_t **out_fdev,
	  char *out_pos)
{
	oskit_error_t err;
	oskit_u32_t addr;

	err = getchildio(bus, idx, out_fdev, &addr);
	if (err)
		return err;

	sprintf(out_pos, "0x%04x", (unsigned)addr);
	return 0;
}

static OSKIT_COMDECL
addchild(oskit_isabus_t *bus, oskit_u32_t addr, oskit_device_t *dev)
{
	struct busnode *n;
	struct busnode *tmp;

	/* XXX sanity check params */
	/* XXX check for addr collisions */

	/* Create a new device node */
	n = osenv_mem_alloc(sizeof(*n), OSENV_PHYS_WIRED, 0);
	if (n == NULL)
		return OSKIT_E_OUTOFMEMORY;

	n->next = NULL;
	n->dev = dev; oskit_device_addref(dev);
	n->addr = addr;

	/*
	 * We need to add it to the end of the list so items
	 * appear in the order in which they are probed so
	 * people don't get confused.
	 */
	if (!nodes) {
		nodes = n;
		return 0;
	}
	for (tmp = nodes; tmp->next; tmp=tmp->next) ;
	tmp->next = n;

	return 0;
}

static OSKIT_COMDECL
remchild(oskit_isabus_t *bus, oskit_u32_t addr)
{
	struct busnode **np, *n;

	for (np = &nodes; (n = *np) != NULL; np = &n->next) {
		if (n->addr == addr) {
			*np = n->next;
			oskit_device_release(n->dev);
			osenv_mem_free(n, OSENV_PHYS_WIRED, sizeof(*n));
			return 0;
		}
	}

	return OSKIT_E_DEV_NOSUCH_CHILD;
}


/*** ISA bus probing ***/

static OSKIT_COMDECL
probe(oskit_isabus_t *bus)
{
	oskit_isa_driver_t **drivers;
	unsigned count, i, found;
	oskit_error_t rc;

	/* Find the set of registered drivers for ISA bus devices */
	rc = osenv_driver_lookup(&oskit_isa_driver_iid, (void***)&drivers);
	if (OSKIT_FAILED(rc))
		return rc;
	count = rc;

	/* Call each driver's probe routine in turn, in priority order */
	found = 0;
	for (i = 0; i < count; i++) {
		rc = oskit_isa_driver_probe(drivers[i], bus);
		if (OSKIT_SUCCEEDED(rc))
			found += rc;
		oskit_isa_driver_release(drivers[i]);
	}

	osenv_mem_free(drivers, OSENV_AUTO_SIZE, 0);

	return found;
}


/*** Operation table for the ISA bus device */

static struct oskit_isabus_ops ops = {
	query,
	addref,
	release,
	getinfo,
	getdriver,
	getchild,
	probe,
	getchildaddr,
	getchildio
};
static oskit_isabus_t bus = { &ops };

/*** Plug & Play ISA ***/
#if 0	/* XXX first cut, based on old P&P docs - didn't work */

#include <oskit/x86/far_ptr.h>
#include <oskit/x86/proc_reg.h>
#include <oskit/x86/pc/bios32.h>

#define ACFG_SCD_SIZE_32	0xB481
#define ACFG_SCD_READ_32	0xB482
#define ACFG_SCD_WRITE_32	0xB483

static void *escd;
static unsigned escd_size;

/* Check for a Plug & Play BIOS with ESCD in NVRAM */
static void pnp_check()
{
	unsigned acfg_start_pa, acfg_length, acfg_entry;
	void *acfg_start_va;
	struct far_pointer_32 acfg_vector;
	unsigned nvram_pa, nvram_size;
	void *nvram_va;
	unsigned char retcode;

	/* Find the BIOS32 autoconfiguration service */
	if (oskit_bios32_find_service(BIOS32_SERVICE_PCI, &acfg_start_pa,
				     &acfg_length, &acfg_entry))
		return;

	/* Map the autoconfiguration service entrypoint */
	if (osenv_mem_map_phys(acfg_start_pa, acfg_length, &acfg_start_va, 0))
		return;

	/* Check the presence and type of ESCD NVRAM storage */
	acfg_vector.seg = get_cs();
	acfg_vector.ofs = (unsigned)acfg_start_va + acfg_entry;
	asm volatile("
		lcall	(%4)
		jc	1f
		xorb	%%ah,%%ah
	1:"	: "=a" (retcode),
		  "=b" (nvram_size),
		  "=c" (escd_size),
		  "=D" (nvram_pa)
		: "r" (&acfg_vector),
		  "a" (ACFG_SCD_SIZE_32)
		: "ebx");
	if (retcode)
		return;

	if (nvram_pa) {
		/* Memory-mapped NVRAM - we need to map it */
		nvram_size &= 0xffff;
		if (nvram_size == 0)
			nvram_size = 0x10000;
		escd_size &= 0xffff;
		if (escd_size == 0)
			escd_size = 0x10000;
		if (osenv_mem_map_phys(nvram_pa, nvram_size, &nvram_va, 0))
			return;
	}

	/* Allocate a buffer to hold the ESCD */
	escd = osenv_mem_alloc(escd_size, 0, 0);
	if (escd == NULL)
		return;

	/* Retrieve it */
	asm volatile("
		lcall	(%1)
		jc	1f
		xorb	%%ah,%%ah
	1:"	: "=a" (retcode)
		: "r" (&acfg_vector),
		  "a" (ACFG_SCD_READ_32),
		  "c" (escd_size),
		  "S" (nvram_va),
		  "D" (escd));
	if (retcode)
		return;

	hexdumpb(0, escd, escd_size);
}
#endif

oskit_error_t
osenv_isabus_init(void)
{
	static oskit_bool_t initialized = FALSE;
	oskit_error_t err;

	/*
	 * mseu, this is a temporary stopping point to make
	 * sure it is called on boot up.
	 */
	/*	panic("isabus_init called"); */

	if (initialized)
		return 0;

	err = osenv_rootbus_addchild((oskit_device_t*)&bus);
	if (err)
		return err;

	/*pnp_check();*/

	initialized = TRUE;
	return 0;
}

oskit_isabus_t *osenv_isabus_getbus(void)
{
	/* XXX */ osenv_isabus_init();

	return &bus;
}

oskit_error_t
osenv_isabus_addchild(oskit_u32_t addr, oskit_device_t *dev)
{
	oskit_error_t err;

	err = osenv_isabus_init();
	if (err)
		return err;

	return addchild(&bus, addr, dev);
}

oskit_error_t
osenv_isabus_remchild(oskit_u32_t addr)
{
	return remchild(&bus, addr);
}

