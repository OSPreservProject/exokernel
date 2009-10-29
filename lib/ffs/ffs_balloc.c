/*	$NetBSD: ffs_balloc.c,v 1.3 1996/02/09 22:22:21 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ffs_balloc.c	8.4 (Berkeley) 9/23/93
 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "ffs_internal.h"

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ffs_balloc(ip, bn, size, flags, rbn)
	register struct inode *ip;
	register daddr_t bn;
	int size;
	int flags;
	daddr_t *rbn;
{
	register struct fs *fs;
	register daddr_t nb;
	struct indir indirs[NIADDR + 2];
	daddr_t newb, lbn, *bap, pref, addr;
	int osize, nsize, num, i, error;

	if (bn < 0)
		return (EFBIG);
	fs = ip->i_fs;
	lbn = bn;

	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */
	nb = lblkno(fs, ip->i_size);
	if (nb < NDADDR && nb < bn) {
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			error = readBlock(fsbtodb(fs, ip->i_db[nb]),
					  osize, ffs_space);
			if (error)
				return (EIO);
			error = ffs_realloccg(ip, nb,
				ffs_blkpref(ip, nb, (int)nb, &ip->i_db[0]),
				osize, (int)fs->fs_bsize, &newb);
			if (error)
				return (error);
			ip->i_size = (nb + 1) * fs->fs_bsize;
			ip->i_db[nb] = newb;
			ip->i_flag |= IN_CHANGE | IN_UPDATE | IN_MODIFIED;
			error = writeBlock(fsbtodb(fs, newb),
					   osize, ffs_space);
			if (error)
				return (EIO);
		}
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = ip->i_db[bn];
		if (nb != 0 && ip->i_size >= (bn + 1) * fs->fs_bsize) {
			error = readBlock(fsbtodb(fs, nb),
					  fs->fs_bsize, ffs_space);
			if (error)
				return (EIO);
			*rbn = fsbtodb(fs, nb);
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
				error = readBlock(fsbtodb(fs, nb),
						  osize, ffs_space);
				if (error)
					return (EIO);
				*rbn = fsbtodb(fs, nb);
				return (0);
			} else {
				error = readBlock(fsbtodb(fs, nb),
						  osize, ffs_space);
				if (error)
					return (error);
				error = ffs_realloccg(ip, bn,
				    ffs_blkpref(ip, bn, (int)bn, &ip->i_db[0]),
				    osize, nsize, &newb);
				if (error)
					return (error);
				if (newb != nb)
				{
					error = writeBlock(fsbtodb(fs, newb),
							   nsize, ffs_space);
					if (error)
						return (error);
				}
			}
		} else {
			if (ip->i_size < (bn + 1) * fs->fs_bsize)
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			error = ffs_alloc(ip,
			    ffs_blkpref(ip, bn, (int)bn, &ip->i_db[0]),
			    nsize, &newb);
			if (error)
				return (error);
			if (flags)
				memset(ffs_space, 0, nsize);
		}
		ip->i_db[bn] = newb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE | IN_MODIFIED;
		*rbn = fsbtodb(fs, newb);
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ffs_getlbns(bn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ffs_balloc: ufs_bmaparray returned indirect block\n");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = ip->i_ib[indirs[0].in_off];
	if (nb == 0) {
		pref = ffs_blkpref(ip, lbn, 0, (daddr_t *)0);
	        error = ffs_alloc(ip, pref, (int)fs->fs_bsize, &newb);
		if (error)
			return (error);
		nb = newb;
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		error = ffs_clearblk(fsbtodb(fs, newb), fs->fs_bsize);
		if (error) {
			ffs_blkfree(ip, nb, fs->fs_bsize);
			return (error);
		}
		ip->i_ib[indirs[0].in_off] = newb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE | IN_MODIFIED;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	addr = ip->i_ib[indirs[0].in_off];
	for (i = 1;;) {
		error = readBlock(fsbtodb(fs, addr), fs->fs_bsize, ffs_space);
		if (error)
			return (error);
		bap = (daddr_t *)ffs_space;
		nb = bap[indirs[i].in_off];
		if (i == num)
			break;
		i += 1;
		if (nb != 0)
		{
			addr = nb;
			continue;
		}
		if (pref == 0)
			pref = ffs_blkpref(ip, lbn, 0, (daddr_t *)0);
		error = ffs_alloc(ip, pref, (int)fs->fs_bsize, &newb);
		if (error)
			return (error);
		nb = newb;
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		error = ffs_clearblk(fsbtodb(fs, newb), fs->fs_bsize);
		if (error) {
			ffs_blkfree(ip, nb, fs->fs_bsize);
			return (error);
		}
		bap[indirs[i - 1].in_off] = nb;
		writeBlock(fsbtodb(fs, addr), fs->fs_bsize, ffs_space);
		addr = nb;
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		pref = ffs_blkpref(ip, lbn, indirs[i].in_off, &bap[0]);
		error = ffs_alloc(ip, pref, (int)fs->fs_bsize, &newb);
		if (error)
			return (error);
		nb = newb;
		bap[indirs[i].in_off] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		writeBlock(fsbtodb(fs, addr), fs->fs_bsize, ffs_space);
		if (flags)
			memset(ffs_space, 0, fs->fs_bsize);
		*rbn = fsbtodb(fs, addr);
		return (0);
	}
	if (flags) {
		error = readBlock(fsbtodb(fs, nb), fs->fs_bsize, ffs_space);
		if (error)
			return (error);
	}
	*rbn = fsbtodb(fs, nb);
	return (0);
}
