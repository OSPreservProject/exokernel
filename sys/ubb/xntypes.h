
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


#ifndef __UBB_XNTYPES_H__
#define __UBB_XNTYPES_H__

#ifdef EXOPC
#define cap_t cap *
#include <xok/types.h>
#include <xok/capability.h>
#else
#include <stdlib.h>
typedef void *cap_t;
#endif

/* Element types: should we collapse with fields?  Why haven't we? */
typedef enum {
	XN_NIL,
	XN_ROOT_TYPE,
	XN_TYPE_TYPE,
	XN_BYTES,
        XN_DB,
#	define xn_user_elem(t) ((t) > XN_DB)
#	define xn_isspecial(t) ((t) == XN_ROOT_TYPE || (t) == XN_TYPE_TYPE)
} xn_elem_t;

enum { XN_ALL = 0, XN_ANY = 0 };

typedef enum {
	XN_SUCCESS = 0,
	XN_TOO_MANY_FIELDS = -1,		/* type exceeds capacity. */
	XN_BOGUS_PTR = -2,			/* bad user. */
	XN_BOGUS_FLAG = -3,
	XN_TOO_MANY_TEMPLATES = -4,		/* fixed sized template dir. */
	XN_BOGUS_TYPE = -5,			/* no template of this name. */
	XN_BOGUS_CAP = -6,			
	XN_CANNOT_ALLOC = -7,
	XN_CANNOT_FREE = -8,
	XN_TYPE_ERROR = -9,
	XN_BLOCK_MISMATCH = -10,
	XN_NOT_IN_CORE = -11,
	XN_OUT_OF_MEMORY = -12,
	XN_HAS_POINTERS = -13,
	XN_REGISTRY_MISS = -14,
	XN_CANNOT_COPY = -15,
	XN_BOGUS_DA = -16,
	XN_REGISTRY_HIT = -17,
	XN_IN_CORE = -18,
	XN_NO_PROXY = -19,
	XN_CANNOT_DELETE = -20,
	XN_TAINTED = -21,
	XN_CANNOT_ACCESS = -22,
	XN_BOGUS_INDEX = -23,
	XN_BOGUS_PAGE	= -24,
	XN_WRITE_FAILED = -25,
	XN_READ_FAILED = -26,
	XN_ILLEGAL_OP = -27,
	XN_BOGUS_FIELD = -28,
	XN_CANNOT_LOCK = -29,
	XN_LOCKED = -30,
	XN_BOGUS_NAME = -31,
	XN_BOGUS_ROOT = -32,
	XN_DUPLICATE = -33,
	XN_NOT_FOUND = -34,
	XN_NOT_CONTIG = -35,
	XN_TOO_SMALL = -36,
	XN_TYPE_MISS = -37,
	XN_BOGUS_CTX = -38,
	XN_CYCLE = -39,
	XN_BOGUS_ENTRY = -40,
	XN_NO_MEMORY = -41,
	XN_DIRTY_BLOCKS = -42,
	XN_END_ERROR = -43,
} xn_err_t;

/* potential xn attributes. */
typedef enum {
 	XN_FORCE_BUFFER_CACHE, 
	XN_UNIQUE_FIELD,  
	XN_MOD_TIME, 
 	XN_VERSION_NUMBER, 
	XN_TEMP_CONSISTENCY, 
	XN_RECONSTRUCT,  
	XN_NO_RECONSTRUCT,
 	XN_TYPED_POINTER, 
	XN_EPHEMERAL, 
	XN_MOVABLE
} xn_attr_t;


/* valid operations on fields.  */
typedef enum {
        /* byte operations. */
        XN_READ = 1 << 16,
        XN_WRITE,

        /* for moving db/xn's between two xnodes */
        XN_MOVE,
        XN_SWAP,
        XN_COPY,

        /* modifying pointers. */
        XN_ALLOC,
        XN_FREE
} xn_op_t;


enum { XN_NOT_NIL = 1 };

/* Used during binding. */
/* The three cases of binding given page to disk block. */
typedef enum { 
        /*
         *      1. Do so with current contents.
         *      2. Do so but require a readin to initialize it.
	 *	3. Bind and zero fill page.
	 * 	4. If page is uninitialized, bind zeros, else
	 * 	bind it.
	 *
 	 * Since (1) requires write permissions; (3) does as well
	 * if the disk block already has valid contents.  Only (2) 
	 * can be done without write permissions, but requires that
 	 * the disk block has been initialized.
         */

	XN_BIND_CONTENTS,
	XN_FORCE_READIN, 
	XN_ZERO_FILL,
	XN_ALLOC_ZERO,

} xn_bind_t;

/* Base types. */
typedef void *xn_t;
typedef long ppn_t;
typedef long db_t;
typedef long da_t; 	/* Disk is byte addressable. */
typedef int xn_cnt_t;

#endif /* __UBB_XNTYPES_H__ */

