
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

/* ALFS -- Application-Level Filesystem */

#ifndef __ALFS_H__
#define __ALFS_H__

#include "alfs_general.h"
#include <string.h>
#include <stdlib.h>

#ifndef min
#define min(a,b)	((a) <= (b) ? (a) : (b))
#endif

extern cap2_t alfs_FScap;
extern block_num_t alfs_FSdev;
#define alfs_makeCap()	((int)random())

#define ALFS_MAX_FILESIZE	4194304

#define ALFS_BUFCACHE_SIZE	(128 * BLOCK_SIZE)

/* filesystem operation error values */

#define ERROR -100
#define ALFS_ELENGTH -1
#define ALFS_ENOENT -2
#define ALFS_EACCESS -3
#define ALFS_EPERM -4
#define ALFS_EINUSE -7
#define ALFS_ENFILE -8
#define ALFS_EISDIR -9
#define ALFS_EBADF -10
#define ALFS_EINVAL -11
#define ALFS_EEXIST -17
#define ALFS_ENOTDIR -20
#define ALFS_ENOTEMPTY -66

/* cache server-specific return values */
#define CSBADBLK -5
#define CSNODATA -6

/* XXX -- create a couple of users and groups */

#define PINCKNEY_UID 12144
#define HBRICENO_UID 12230
#define USERS_GID 100
#define PDOS_GID 200

/* file system structure */

typedef struct {
    cap2_t	fscap;
    u_int32_t	rootDInodeNum;
    int		size;
    uint	numblocks;
    uint	freeblks;
    char	space[(BLOCK_SIZE - sizeof(cap2_t) - sizeof(u_int32_t) - sizeof(int))];
} alfs_t;

/* structure returned by fstat */

struct alfs_stat {
     block_num_t st_ino;
     umask_t st_mode;
     _uid_t st_uid;
     _gid_t st_gid;
     int st_size;
     int st_blksize;
};

/* some defines for use with access permissions */

#define AXS_IRUSR 256
#define AXS_IWUSR 128
#define AXS_IXUSR 64
#define AXS_IRGRP 32
#define AXS_IWGRP 16
#define AXS_IXGRP 8
#define AXS_IROTH 4
#define AXS_IWOTH 2
#define AXS_IXOTH 1

/* and for file modes */

#define OPT_RDONLY 1
#define OPT_WRONLY 2
#define OPT_RDWR (OPT_RDONLY | OPT_WRONLY)
#define OPT_CREAT 4

/* for lseek */

typedef int offset_t;
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* some globals that define who we are
   defined in protection.c */

extern _uid_t uid;
extern _gid_t gid;
extern umask_t usermask;
extern cap2_t uidCap, gidCap;

extern cap2_t hbricenoUidCap, pinckneyUidCap;
extern cap2_t usersGidCap, pdosGidCap;

extern alfs_t *alfs_superblock;
extern uint alfs_superblkno;

static inline char * malloc_and_alloc (int len)
{
   char *ptr = malloc(len);
   int off = 0;
   if (ptr == NULL) {
      return (NULL);
   }
   ptr[(len-1)] = 0;
   while (len > 0) {
      ptr[off] = 0;
      off += 4096;
      len -= 4096;
   }
   return (ptr);
}

/* syscalls.c */

uint alfs_initmetablks (uint dev, int size);
uint alfs_initFS (uint dev, int size, cap2_t fscap);
void alfs_mountFS (uint dev, uint superblkno);
void alfs_unmountFS ();
int alfs_open (char *, int, umask_t);
int alfs_read (int fd, char *buf, int nbytes);
int alfs_write (int fd, char *buf, int nbytes);
int alfs_close (int fd);
int alfs_lseek (int fd, offset_t offset, int whence);
int alfs_mkdir (char *, umask_t);
int alfs_unlink (char *);
int alfs_rmdir (char *path);
int alfs_fstat (int fd, struct alfs_stat *buf);
int alfs_stat (char *pathname, struct alfs_stat *buf);
int alfs_chmod (char *path, int mode);
void alfs_listDirectory (void *curRootInode);

#endif  /* __ALFS_H__ */

