
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

/* Root catalogue. */
#ifndef __ROOT_CATALOGUE_H__
#define __ROOT_CATALOGUE_H__

#include <ubb/xn-struct.h>
#include <ubb/kernel/virtual-disk.h>

/* Hard-coded for simplicity.  XXX. */
#define R_NSLOTS  ((XN_BLOCK_SIZE - 16) / sizeof(struct root_entry))
#define R_TYPE (XN_DB + 1)	/* First available type. */
#define R_NBLOCKS 1		/* single disk block. */

/* root catalogue disk block. */
extern db_t r_db;

/* Indexed by name. */
struct root_catalogue {
	size_t nelem;		/* number of active roots. */
	da_t	da;		/* disk address */

	struct root_entry { 
		/*
		 * Name of file, unique but *not* 0 terminated
	 	 * (can store aribtrary data in it)
		 */
		unsigned char name[80];	
		cap_t c;		/* capability for this entry. */
		db_t	db;		/* disk block of root. */
		size_t  nelem;		/* number of elements of T. */
		xn_elem_t t;		/* type of root */
		unsigned is_type;	/* has integer type name if so. */
	} roots[R_NSLOTS];
};

int root_install(char *name, db_t *db, size_t nelem, xn_elem_t t, cap_t c);
int root_mount(char *name, cap_t c);
int root_delete(char *name, cap_t c);
void root_init(int new_p);

#endif /* __ROOT_CATALOGUE_H__ */
