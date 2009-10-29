
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

#ifndef _NFS_H_

#define _NFS_H_


#if 0

#define NFS_SLACK_SPACE 8192
#define NFS_MAXRDATA 	8192
#define NFS_MAXWDATA 	8192

#else

#define NFS_SLACK_SPACE 1024
#define NFS_MAXRDATA 	1024
#define NFS_MAXWDATA 	1024

#endif

#define NFS_MAXPATHLEN 1023	/* 1024, should be using PATH_MAX */
#define NFS_MAXNAMLEN 255	/* 255,  should be using NAME_MAX */

#define NFS_MAXGROUPS 16
#define NFS_FHSIZE 32
#define NFS_COOKIESIZE 4
#define NFS_FIFO_DEV (-1)
#define NFSMODE_FMT   0170000
#define NFSMODE_BLK   0060000
#define NFSMODE_DIR   0040000
#define NFSMODE_CHR   0020000
#define NFSMODE_FIFO  0010000
#define NFSMODE_REG   0100000
#define NFSMODE_LNK   0120000
#define NFSMODE_SOCK  0140000

enum nfs_stat {
  NFS_OK = 0,
  NFSERR_PERM = 1,
  NFSERR_NOENT = 2,
  NFSERR_IO = 5,
  NFSERR_NXIO = 6,
  NFSERR_ACCES = 13,
  NFSERR_EXIST = 17,
  NFSERR_NODEV = 19,
  NFSERR_NOTDIR = 20,
  NFSERR_ISDIR = 21,
  NFSERR_INVAL = 22,		/* that Sun forgot */
  NFSERR_FBIG = 27,
  NFSERR_NOSPC = 28,
  NFSERR_ROFS = 30,
  NFSERR_NAMETOOLONG = 63,
  NFSERR_NOTEMPTY = 66,
  NFSERR_DQUOT = 69,
  NFSERR_STALE = 70,
  NFSERR_WFLUSH = 99
};

enum nfs_ftype {
  NFNON = 0,
  NFREG = 1,
  NFDIR = 2,
  NFBLK = 3,
  NFCHR = 4,
  NFLNK = 5,
  NFSOCK = 6,
  NFBAD = 7,
  NFFIFO = 8
};

int nfs_stat_to_errno(int stat);

#define NLS_ALL         0x0001
#define NLS_LONG        0x0002



#define NFS_PROGRAM		100003
#define NFS_VERSION		2
#define NFS_PORT		2049
#define NFSPROC_NULL		0
#define NFSPROC_GETATTR		1
#define NFSPROC_SETATTR		2
#define NFSPROC_ROOT		3
#define NFSPROC_LOOKUP		4
#define NFSPROC_READLINK	5
#define NFSPROC_READ		6
#define NFSPROC_WRITECACHE	7
#define NFSPROC_WRITE		8
#define NFSPROC_CREATE		9
#define NFSPROC_REMOVE		10
#define NFSPROC_RENAME		11
#define NFSPROC_LINK		12
#define NFSPROC_SYMLINK		13
#define NFSPROC_MKDIR		14
#define NFSPROC_RMDIR		15
#define NFSPROC_READDIR		16
#define NFSPROC_STATFS		17

struct nfs_fh {
  char data[NFS_FHSIZE];
  struct generic_server *server;
};
typedef struct nfs_fh nfs_fh_def;

#define FHGETDEV(fhandle) (((struct nfs_fh *)fhandle)->server->fakedevice)
#define FHGETRSIZE(fhandle) (((struct nfs_fh *)fhandle)->server->rsize)
#define FHGETWSIZE(fhandle) (((struct nfs_fh *)fhandle)->server->wsize)

struct nfs_time {
  unsigned int seconds;
  unsigned int useconds;
};

struct nfs_fattr {
  enum nfs_ftype type;
  unsigned int mode;
  unsigned int nlink;
  unsigned int uid;
  unsigned int gid;
  unsigned int size;
  unsigned int blocksize;
  unsigned int rdev;
  unsigned int blocks;
  unsigned int fsid;
  unsigned int fileid;
  struct nfs_time atime;
  struct nfs_time mtime;
  struct nfs_time ctime;
};

struct nfs_sattr {
  unsigned int mode;
  unsigned int uid;
  unsigned int gid;
  unsigned int size;
  struct nfs_time atime;
  struct nfs_time mtime;
};

struct nfs_entry {
  unsigned int fileid;
  char name[NFS_MAXNAMLEN];	/* changed this to be allocated */
  int cookie;			/* statically  */
  int eof;
};

struct nfs_fsinfo {
  unsigned int tsize;
  unsigned int bsize;
  unsigned int blocks;
  unsigned int bfree;
  unsigned int bavail;
};

/*
 *  linux/include/linux/nfs_fs.h
 *
 *  Copyright (C) 1992  Rick Sladkey
 *
 *  OS-specific nfs filesystem definitions and declarations
 * The readdir cache size controls how many directory entries are cached.
 * Its size is limited by the number of nfs_entry structures that can fit
 * in one 4096-byte page, currently 256.
 */

#define NFS_READDIR_CACHE_SIZE		64

#define NFS_MAX_FILE_IO_BUFFER_SIZE	4096
#define NFS_DEF_FILE_IO_BUFFER_SIZE	1024

/*
 * The upper limit on timeouts for the exponential backoff algorithm
 * in hundreth seconds.
 */

#define NFS_MAX_RPC_TIMEOUT		600

/*
 * Size of the lookup cache in units of number of entries cached.
 * It is better not to make this too large although the optimum
 * depends on a usage and environment.
 */




/* linux/fs/nfs/proc.c */

extern int nfs_proc_getattr(struct nfs_fh *fhandle,
			    struct nfs_fattr *fattr);

extern int nfs_proc_setattr(struct nfs_fh *fhandle,
			    struct nfs_sattr *sattr, struct nfs_fattr *fattr);

extern int nfs_proc_lookup(struct nfs_fh *dir,
			   const char *name, struct nfs_fh *fhandle,
			   struct nfs_fattr *fattr);

extern int nfs_proc_readlink(struct nfs_fh *fhandle,
			     char *res);

/* aliased to parallizing */
extern int nfs_proc_read(struct nfs_fh *fhandle,
			 int offset, int count, char *data,
			 struct nfs_fattr *fattr);

/* not parallizing */
extern int nfs_proc_read_np(struct nfs_fh *fhandle,
			 int offset, int count, char *data,
			 struct nfs_fattr *fattr);

/* parallizing */
extern int nfs_proc_read_p(struct nfs_fh *fhandle,
			 int offset, int count, char *data,
			 struct nfs_fattr *fattr);

extern int nfs_proc_write(struct nfs_fh *fhandle,
			  int offset, int count, char *data,
			  struct nfs_fattr *fattr);

struct ae_recv;
extern int nfs_proc_writev(struct nfs_fh *fhandle,
			  int offset, int count, struct ae_recv *r,
			  struct nfs_fattr *fattr);

extern int nfs_proc_create(struct nfs_fh *dir,
			   const char *name, struct nfs_sattr *sattr,
			   struct nfs_fh *fhandle, struct nfs_fattr *fattr);

extern int nfs_proc_remove(struct nfs_fh *dir,
			   const char *name);

extern int nfs_proc_rename(struct nfs_fh *old_dir, const char *old_name,
			   struct nfs_fh *new_dir, const char *new_name);

extern int nfs_proc_link(struct nfs_fh *fhandle,
			 struct nfs_fh *dir, const char *name);

extern int nfs_proc_symlink(struct nfs_fh *dir, const char *name, 
			    const char *path, 
			    struct nfs_sattr *sattr);

extern int nfs_proc_mkdir(struct nfs_fh *dir,
			  const char *name, struct nfs_sattr *sattr,
			  struct nfs_fh *fhandle, struct nfs_fattr *fattr);

extern int nfs_proc_rmdir(struct nfs_fh *dir,
			  const char *name);

extern int nfs_proc_readdir(struct nfs_fh *fhandle,
			    int cookie, int count, struct nfs_entry *entry);

extern int nfs_proc_statfs(struct nfs_fh *fhandle,
			   struct nfs_fsinfo *res);

int nnamei(char *path, 
	   struct nfs_fh *new_fhandle, 
	   struct nfs_fattr *new_fattr);

/* for debugging purposes */

void print_fattr (struct nfs_fattr *fattr);


#endif
