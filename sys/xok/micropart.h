/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

#ifndef _XOK_MICROPART_H_
#define _XOK_MICROPART_H_

#define MICROPART_METADATA_PAGE 20	/* leave about 80 sectors 
					   at start of part untouched
					   for mbr etc */

#include <xok/pxn.h>

#ifdef KERNEL

#include <xok_include/assert.h>

#endif

struct FSID {
  cap fs_cap;			/* cap that you must have to be this fs */
  u32 fs_id;			/* unique id for this fs */
};

struct Micropart_header {
  u64 h_signature;		/* is this a valid micropart or not? */
#define MICROPART_SIG 0xabcdef0987654321LL
  u32 h_num_microparts;		/* number of microparts in this partition */
  u32 h_micropart_sz;		/* number of device sectors per micropart */
  struct FSID *h_freemap;	/* array of h_num_microparts length */
};

struct Micropart {
  struct Micropart_header *mp_header;	/* freemap of who owns each micro-part */
  int mp_loaded;		/* zero if invalid */
  struct bc_entry *mp_bc;	/* bc entry that holds the freemap */
};

#endif /* _XOK_MICROPART_H_ */
