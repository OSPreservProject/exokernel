
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

#ifndef _VIRTUAL_DISK_H_
#define _VIRTUAL_DISK_H_

/* Have a device description to make this cleaner. */
#define XN_SECTOR_SIZE  (512)
#define XN_NSECTORS     (XN_DISK_SIZE/XN_SECTOR_SIZE)
#define XN_BLOCK_SIZE	4096
#define SECTOR_SIZE	512
#define XN_DISK_SIZE    (768 * 1024 * 1024UL)
#define XN_NBLOCKS     (XN_DISK_SIZE/XN_BLOCK_SIZE)

struct db {
#if 0
	/* XXX: At some point we should test using these. */
        int  v;                 /* version number */
        int  free;
        int  initialized;
#endif

        unsigned char sector[XN_BLOCK_SIZE];
};

struct io_vec {
        db_t db;
        void *data;
        size_t nblocks;
};

extern struct disk_info {
        int read;
        int written;
        int read_req;
        int write_req;
} disk_info;


/* virtual-disk.c */
int bwrite(db_t dst, void * src, size_t nblocks, xn_cnt_t *cnt);
int bc_write(db_t dst, size_t nblocks, xn_cnt_t *cnt);
int bc_read(db_t dst, size_t nblocks, xn_cnt_t *cnt);
int bread(void * dst, db_t src, size_t nblocks, xn_cnt_t *cnt);
void disk_init(void);
void disk_shutdown(void);

#define db_to_sec(db) 		((db) * (XN_BLOCK_SIZE / XN_SECTOR_SIZE))
#define da_to_db(da) 		((da) / XN_BLOCK_SIZE)
#define da_to_sec(da) 		db_to_sec(da_to_db(da))

#define da_to_offset(da) 	((da) % XN_BLOCK_SIZE)
#define bytes_to_blocks(x) 	(roundup(x, XN_BLOCK_SIZE) / XN_BLOCK_SIZE)
#define bytes_to_secs(x) 	(roundup(x, XN_SECTOR_SIZE) / XN_SECTOR_SIZE)
#define blocks_to_secs(x) 	((x) * (XN_BLOCK_SIZE / XN_SECTOR_SIZE))
#define da_compose(db, offset) 	((db) * XN_BLOCK_SIZE + (offset))

void xr_post_io(db_t db, size_t nblocks, int writep);

#ifdef KERNEL
#ifndef roundup
#	define roundup(x,n) ((((unsigned long)(x))+((n)-1))&(~((n)-1)))
#endif
#else
#include <sys/param.h>	/* provides roundup */
#endif

#endif /* _VIRTUAL_DISK_H_ */
