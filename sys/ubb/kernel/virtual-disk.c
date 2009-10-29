
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
 * Abstract disk used to verify md correctness on the following properties:
 *	1. Never point to a structure before it has been initialized.
 *		- when the xn pointer is set, we simply query the disk.
 *		- on reconstruction, we check that every pointed to block
 *		is initialized
 *	2. Never reuse a resource before nullifying all previous pointers.
 *		- every allocation tracks the block that points to it;
 *
 *	3. Never reset the old pointer to a resource before a new pointer
 *		has been set.
 *		- when reset a pointer, check the owner of the disk block.
 */
#include <xok/types.h>
#include <xok/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "registry.h"
#include "xn-struct.h"
#include "kernel/virtual-disk.h"
#include "lib/demand.h"
#include <bc.h>
#include <pmap.h>

#define map(v, n, fp) do {					\
	int i;							\
								\
	for(i = 0; i < n; i++)					\
		fp(v+i);					\
} while(0)

static struct db disk[XN_NBLOCKS];

struct disk_info disk_info;

static void disk_read(size_t nblocks) {
	disk_info.read += nblocks;
	disk_info.read_req++;
}

static void disk_write(size_t nblocks) {
	disk_info.written += nblocks;
	disk_info.write_req++;
}

int bwrite(db_t dst, void * src, size_t nblocks, xn_cnt_t *cnt) {
	void writeb(struct db *d) {
		memcpy(d->sector, src, sizeof d->sector);
		src = (char *)src + sizeof d->sector;
	}

	/* printf("disk write [%ld, %ld)\n", dst, dst + nblocks); */
	map(disk+dst, nblocks, writeb);
	if(cnt) *(cnt) -= nblocks;
	disk_write(nblocks);
	
	return 1;
}

int bread(void * dst, db_t src, size_t nblocks, xn_cnt_t *cnt) {
	void readb(struct db *d) {
		memcpy(dst, d->sector, sizeof d->sector);
		dst = (char *)dst + sizeof d->sector;
	}

	/* printf("disk read [%ld, %ld)\n", src, src + nblocks); */
	map(disk+src, nblocks, readb);
	if(cnt) *(cnt) -= nblocks;
	disk_read(nblocks);
	return 1;
}

static int vd_fd;

void reset(int fd) {
	lseek(fd, 0, SEEK_SET);
}

void disk_init(void) {
	if((vd_fd = open("vd", O_RDWR, 0777)) >= 0) {
		printf("reading\n");
		if(read(vd_fd, disk , sizeof disk) != sizeof disk)
			fatal(write failed);
		reset(vd_fd);
	} else if((vd_fd = open("vd", O_CREAT|O_RDWR, 0777)) >= 0) {
		printf("creating\n");
		/* initialize. */
		if(write(vd_fd, disk , sizeof disk) != sizeof disk)
			fatal(write failed);
		reset(vd_fd);
	} else
		fatal(Could not open);
}

void disk_shutdown(void) {
	if(write(vd_fd, disk, sizeof disk) != sizeof disk)
		fatal(write failed);
	close(vd_fd);
}
