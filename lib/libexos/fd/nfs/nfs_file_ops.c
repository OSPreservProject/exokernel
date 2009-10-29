
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

#include <fd/proc.h>
#include "nfs_rpc.h"
#include "nfs_struct.h"
#include "nfs_proc.h"

#define NFSREADCACHING
#define NFSWRITECACHING
#define NFSNAMECACHINGLOOKUP
#define NFSNAMECACHINGOPEN
#define NFSGETDIRCACHING

struct file_ops nfs_file_ops = {
#ifdef NFSNAMECACHINGOPEN
    nfs_open_cached,		/* open with caching */
#else
    nfs_open,  		        /* open without caching */
#endif
    nfs_lseek,			/* lseek */
#ifdef NFSREADCACHING
    nfs_read_new,		/* read with caching*/
#else
    nfs_read,		        /* read */
#endif
#ifdef NFSWRITECACHING
    nfs_write_new,		/* write with caching of full blocks */
#else
    nfs_write,     	        /* write */
#endif
    nfs_select,			/* select */
    NULL,			/* select_pred */
    NULL,			/* ioclt */
    nfs_close0,			/* close */
    nfs_close,			/* close */
#ifdef NFSNAMECACHINGLOOKUP
    nfs_lookup_cached,		/* lookup */
#else
    nfs_lookup,		        /* lookup */
#endif
    nfs_link,			/* link */
    nfs_symlink,		/* symlink */
    nfs_unlink,			/* unlink */
    nfs_mkdir,			/* mkdir */
    nfs_rmdir,			/* rmdir */
    NULL,			/* mknod */
    nfs_rename,			/* rename */
    nfs_readlink,		/* readlink */
    NULL,			/* follow_link */
    nfs_truncate,		/* truncate */
    NULL,			/* dup */
    nfs_release,	        /* release */
    nfs_acquire,		/* acquire */
    NULL,			/* bind */
    NULL,			/* connect */
    NULL,			/* filepair */
    NULL,			/* accept */
    NULL,			/* getname */
    NULL,			/* listen */
    NULL,			/* sendto */
    NULL, 		        /* recvfrom */
    NULL,			/* shutdown */
    NULL,			/* setsockopt */
    NULL,			/* getsockopt */
    NULL,			/* fcntl */
    NULL,			/* mount */
    NULL,			/* unmount */
    nfs_chmod,			/* chmod */
    nfs_chown,			/* chown */
    nfs_stat,			/* stat */
#ifdef NFSGETDIRCACHING
    nfs_getdirentries_cached,	/* readdir caching readdir entries */
#else
    nfs_getdirentries,		/* readdir */
#endif
    nfs_utimes,			/* utimes */
    nfs_bmap,			/* bmap */
    nfs_fsync			/* fsync */
};
