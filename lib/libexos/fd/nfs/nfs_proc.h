
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

#ifndef _NFS_PROC_H_
#define _NFS_PROC_H_

#include "fd/proc.h"
#include "errno.h"
#include <exos/debug.h>



int
nfs_init(void);


int
nfs_read(struct file *filp, char *buffer, int nbyte, int blocking);

int
nfs_write(struct file *filp, char *buffer, int nbyte, int blocking);

int
nfs_read_new(struct file *filp, char *buffer, int nbyte, int blocking);

int
nfs_write_new(struct file *filp, char *buffer, int nbyte, int blocking);

int 
nfs_close(struct file * filp);
int 
nfs_close0(struct file * filp, int fd);
int 
nfs_sync(struct file * filp);

int 
nfs_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode);

int 
nfs_open_cached(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode);

int 
nfs_lookup(struct file *dirp, const char *name, struct file *filp);

int 
nfs_lookup_cached(struct file *dirp, const char *name, struct file *filp);

int 
nfs_release(struct file *filp);
int 
nfs_acquire(struct file *filp);

int 
nfs_release_nocache(struct file *filp);

off_t 
nfs_lseek(struct file *filp, off_t offset, int whence);

int
nfs_chmod(struct file *filp, mode_t mode);

int
nfs_truncate(struct file *filp, off_t length);

int
nfs_chown(struct file *filp, uid_t owner, gid_t group);

int 
nfs_link(struct file *filp, struct file *dir_filp, 
	   const char *name);

int 
nfs_select(struct file *filp, int);

int 
nfs_mkdir(struct file *dir_filp, const char *name, mode_t mode);

int 
nfs_rmdir(struct file *dir_filp, const char *name);

int 
nfs_readlink(struct file *filp, char *buf, int bufsize);

int
nfs_getdirentries(struct file *filp, char *buffer, int nbytes);

int
nfs_getdirentries_cached(struct file *filp, char *buffer, int nbytes);

int 
nfs_rename(struct file *filpfrom, const char *namefrom, 
	   struct file *filpto,   const char *nameto);

int 
nfs_stat(struct file *filp, struct stat *buf);

int 
nfs_utimes(struct file *filp, const struct timeval *times);

int 
nfs_symlink(struct file *dir_filp, const char *name, const char *to);

int 
nfs_unlink(struct file *filp, const char *name);

int
nfs_bmap(struct file *filp, u_int *diskBlockHighP, u_int *diskBlockLowP, off_t offset, u_int *seqBlocksP);

int 
nfs_mount_root(char *hostname, char *path);

int 
nfs_fsync(struct file * filp);

int
nfs_bmap_read (int, u_quad_t);

#endif 
