
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


#ifndef __CFFS_PROC_H__
#define __CFFS_PROC_H__

#ifdef JJ
#include "exofile.h"
#else
#include <fd/proc.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

#define FILEP_GETINODENUM(filep)	(*((u_int *)(filep)->data))
#define FILEP_SETINODENUM(filep,num)	*((u_int *)(filep)->data) = num;
#define FILEP_GETSUPERBLOCK(filep)      (*(((u_int *)(filep)->data)+1))
#define FILEP_SETSUPERBLOCK(filep,blk)  *(((u_int *)(filep)->data)+1) = blk;

void cffs_cache_init ();

void cffs_cache_shutdown ();

void cffs_protection_init (u_int, int);

int cffs_mount_superblock (int, u_int, int);

int cffs_init_superblock (u_int dev, u_int *superblockno);

int cffs_device_usage (u_int dev, int *count);

int cffs_fs_usage (int dev, u_int sb, int *count);

int cffs_free_fs (int dev, u_int sb);

#define CFFS_CHECK_SETUP(dev) \
			    cffs_cache_init (); \
			    if (global_ftable->inited_disks[dev] == 0) { \
				       cffs_protection_init (dev, 1); \
				       global_ftable->inited_disks[dev] = 1; \
      		            }  else { \
				       cffs_protection_init (dev, 0); \
       			    }

int cffs_open (struct file *dirp, struct file *filp, char *name, int flags, mode_t mode);

int cffs_read (struct file *filp, char *buf, int nbytes, int blocking);

int cffs_write (struct file *filp, char *buf, int nbytes, int blocking);

int cffs_close (struct file *filp);

off_t cffs_lseek (struct file *filp, off_t offset, int whence);

int cffs_lookup (struct file *dirp, const char *name, struct file *filp);

int cffs_release (struct file *filp);

int cffs_getdirentries (struct file *dirp, char *buf, int nbytes);

int cffs_mkdir (struct file *dirp, const char *dirname, mode_t mode);

int cffs_unlink (struct file *filp, const char *name);

int cffs_rmdir (struct file *dirp, const char *name);

int cffs_stat (struct file *filp, struct stat *buf);

int cffs_bmap (struct file *filp, u_int *diskBlockHighP, u_int *diskBlockLowP, 
	       off_t offset, u_int *seqBlocksP);

int cffs_chmod (struct file *filp, mode_t mode);

int cffs_chown (struct file *filp, uid_t owner, gid_t group);

int cffs_utimes (struct file *filp, const struct timeval *times);

int cffs_link (struct file *to, struct file *dirp, const char *name);

int cffs_symlink (struct file *dirp, const char *name, const char *to);

int cffs_readlink (struct file *filp, char *buffer, int buflen);

int cffs_truncate (struct file *filp, off_t length);

int cffs_fsync (struct file *filp);

int cffs_rename (struct file *dirfrom, const char *namefrom,
		 struct file *dirto,   const char *nameto);

int cffs_ioctl (struct file *filp, unsigned int request, char *argp);

void cffs_exithandler ();

int cffs_printdirinfo (struct file *filp);

void cffs_grabRoot (u_int dev, u_int sb_num, struct file *dirp);

void cffs_unmount_superblock (u_int dev, u_int sb_num, int weak_unmount);

#endif /* __CFFS_PROC_H__ */

