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
 * This provides the osenv interfaces for access to PCI
 * configuration space.
 */

#include <oskit/dev/dev.h>

/*
 * These are the machine-dependant routines.
 * May either program configuration cycles directly or call into
 * BIOS32 (on x86 anyway).
 */
int pci_config_read(char bus, char device, char function, char port,
	unsigned *data);

int pci_config_write(char bus, char device, char function, char port,
	unsigned data);

int pci_config_init();


int
osenv_pci_config_read(char bus, char device, char function, char port,
	unsigned *data)
{
	osenv_assert((port & 0x3) == 0);

	return pci_config_read(bus, device, function, port, data);
}


int
osenv_pci_config_write(char bus, char device, char function, char port,
	unsigned data)
{
	osenv_assert((port & 0x3) == 0);
	return pci_config_write(bus, device, function, port, data);
}


/*
 * This returns zero on success, non-zero on failure (ie, no PCI).
 */
int
osenv_pci_config_init()
{
	return pci_config_init();
}

