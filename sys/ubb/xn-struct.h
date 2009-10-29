
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

#ifndef __XN_STRUCT_H__
#define __XN_STRUCT_H__

#include "xn.h"
#include "ubb.h"

/*
 * Specify update to a piece of meta-data.  Semantics:
 *       memcpy((char *)meta + offset, addr, nbytes);
 */
struct xn_m_vec {
        size_t  offset;
        void    *addr;
        size_t  nbytes;
};

/* Specifies how to modify xn. */
struct xn_modify {
        cap_t   cap;
        size_t own_id;          /* id of the udf to run. */
        size_t n;               /* number of elements in mv */
        struct xn_m_vec *mv;    /* base, len, ptr pairs */
};

/* A unit of update. */
struct xn_update {
        db_t db;                /* base - all objects sector are aligned. */
        size_t nelem;           /* number of elements of t. */
        xn_elem_t type;            /* type */
};

/* operation. */
struct xn_op {
        struct xn_update u;     /* the value to write. */
        struct xn_modify m;     /* where to do it. */
}; 


/* Probabily a device constant. */
#define XN_MAX_IO_E        256

struct xn_iov {
        size_t n_io;                    /* number of entries. */
        xn_cnt_t *cnt;                  /* Decremented on every successul io*/

        enum { FORCE_READ, MISS_ERROR } flags;  /* unused. */
        struct xn_ioe {
                db_t db;
                size_t nblocks;
		void *addr;		/* mem to write from/to. */
        } iov[1];
};


#endif /* __XN_STRUCT_H__ */
