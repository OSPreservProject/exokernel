
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

/*
 * UDF-system interface.  Includes type and access control interfaces. 
 *
 * Access control is performed by type-specific methods.
 *
 * If a udf is associated with a byte range, you can only
 * modify it through the kernel.
 *
 * Todo:
 * 	- allow mutiple capabilities.
 *  	- ensure that some pieces of data can only be used by methods.
 * 	- ensure that data can only be read if you had the parent's
 * 	access rights.
 */
#ifndef __UBB_H__
#define __UBB_H__
#include "xn.h"
#include "mv.h"
#include <xok/types.h>
#include <xok/printf.h>
#include <xok/malloc.h>
#include "ubb-inst.h"

/* Random restrictions. */
#define UDF_EXT_MAX	8
#define UDF_INST_MAX	46
#define UDF_MAX_SEG	1

/* UDF_PARENT + int is the which of the parent pointers to use. */
enum { UDF_SELF, UDF_PARENT };

/* 
 * The 6 different access. 
 *
 * In a sense, byte read and write are overloaded: higher level operations
 * will likily map down to these simple ones (e.g., listing a directory)
 * and the method will have to differentiate using the (offset, nbytes).
 * We should allow client-specified data to be sent as well.
 */
typedef enum { 
	UDF_BYTE_READ, 
	UDF_BYTE_WRITE, 

	UDF_ALLOC, 
	UDF_DEALLOC, 
	UDF_READ, 
	UDF_WRITE,

	UDF_END
} u_access_t;

struct udf_inst;
struct xr;

typedef struct xn_op xn_op;
typedef struct xn_update xn_update;
typedef struct xn_modify xn_modify;
typedef struct xn_m_vec xn_m_vec;

/* 
 * Currently is just a pointer to our parent.   How to allow
 * these to be accumulated?  Currently we just add this
 * by default: parents of children that do not have capabilities
 * are ``sticky'' and follow them down the path. 
 * 
 * Selected using UDF_PARENT in the interpreted code.
 */
typedef struct udf_ctx {
	da_t segs[1];
} udf_ctx;

/* [lb, ub) implies access. */
typedef struct udf_ext {
	size_t n;
	/* Should be in bits, right? */
	struct udf_bl { size_t lb, ub; } v[UDF_EXT_MAX];
} udf_ext;

/* Function; can be either a real UDF or a method. */
typedef struct udf_fun { 
	/* Vector of instructions. */
	struct udf_inst insts[UDF_INST_MAX];
	size_t 		ninst;

	/* type signature. */
	size_t 					nargs;
	enum { UBB_CAP = 1, UBB_INT, UBB_SET } 	result_t;
} udf_fun;

/* Checkpoint of meta data. */
typedef struct udf_ckpt {
        struct xn_m_vec *m;
        int n;
} udf_ckpt;

/*
 *  The rules: 
 *	0. If class == UDF_BASE_TYPE then we are done.
 *
 *	1. If class == UFD_UNION, then we run the given type UDF, which
 *	returns the type integer that this node really is.  The type
 *	must be one of the types given in the u list.  Each of these
 *	types *must* be the same size, and all must reserve the same
 *	read set used by the type UDF (as with the other UDF read sets
 *	this one cannot overlap with others).  The type function, when
 *	its field is zero must yield a valid type or XN_NIL.
 *
 *	2. If class == UDF_STRUCT, then we apply these rules to each
 *	type recursively.  Currently, a struct must be equal to 4096 
 * 	bytes in size.
 *
 *  The way these are used: 
 *	given a disk address, we get the type of its associated disk
 *	block.  This type will be either a base type or some composition.
 *	If it is a composition, then we compute what the type within it
 *	that has this offset and return it and the offset within the page.
 *	We can then go about our merry way.
 */
typedef enum { UDF_BASE_TYPE, UDF_STRUCT, UDF_UNION } udf_class_t;

#define UDF_MAX_TYPES	48

/* Build a structure from existing types. */
typedef struct udf_struct {
	int ntypes;
	xn_elem_t 	ty[UDF_MAX_TYPES];	
	struct udf_type *t[UDF_MAX_TYPES];	
} udf_struct;

typedef struct udf_base_type {
	/* Memory that can be accessed without interfering with UDF's.   */
	struct udf_ext		raw_write;
	struct udf_ext		raw_read;

	/* Procedure used read blocks owned by the user.  */
	struct udf_fun  owns;
	size_t		nowns;

	/* Given an index, what byte range does it own. */
	struct udf_fun 	read_f;

	 /* 
	  * Given an operation and capability, does the process have the given
	  * access? 
	  */

	/* Type signature: int (meta_t, cap_t, size_t base, size_t nbytes) */
	struct udf_fun 	byte_access;
	/* Type signature: int (meta_t, cap_t, size_t index) */
	struct udf_fun 	block_access;
} udf_base_type;

/* UDF type. */
typedef struct udf_type {
	/* human readable string; no guarentees about contents. */
	char 		type_name[80];	
	size_t 		type;		/* Integer name. */
	size_t 		nbytes;		/* Type size. XXX should be variable. */
	udf_class_t	class;		/* Type class. */
	unsigned	active;		/* Has this been finished? */
	unsigned  	is_sticky;	/* Should parent be propogated to us? */

	/* These two fields must be identical for all types in a union. */
	struct udf_fun	get_type;	/* UDF to get type. */
	struct udf_ext	type_access;	/* Its read set. */

	union {
		udf_base_type 	t;
		udf_struct	s;	/* composition. */
		udf_struct	u;	/* legal selection */
	} u;
} udf_type;


#endif
