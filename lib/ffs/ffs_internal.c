#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "ffs_internal.h"

/***
 *
 * Internal variables
 *
 ******/

u_int diskblocks;
struct fs *fs;
char *ffs_space;
u_long nextgennumber;

/***
 *
 * Internal functions
 *
 ******/

int
ffs_getdinode (ino_t inum, struct dinode *dip)
{
  /*printf(" << getinode(%d)\n", inum);*/
  if (readBlock(fsbtodb(fs,ino_to_fsba(fs, inum)), fs->fs_bsize, ffs_space))
    return 1;
  *dip = (struct dinode)*((struct dinode*)ffs_space + ino_to_fsbo(fs,inum));
  return 0;
}

int
ffs_getinode (ino_t inum, struct inode *ip)
{
  /*printf(" << getinode(%d)\n", inum);*/
  if (readBlock(fsbtodb(fs,ino_to_fsba(fs, inum)), fs->fs_bsize, ffs_space))
    return EIO;
  ip->i_din = (struct dinode)*((struct dinode*)ffs_space
			       + ino_to_fsbo(fs,inum));
  ip->i_flag = 0;
  ip->i_number = inum;
  ip->i_fs = fs;
  return 0;
}

int
ffs_writeinode (struct inode *ip)
{
  /*printf(" << writeinode(%d)\n", ip->i_number);*/
  if (readBlock(fsbtodb(fs,ino_to_fsba(fs, ip->i_number)),
		fs->fs_bsize, ffs_space))
    return EIO;
  (struct dinode)*((struct dinode*)ffs_space + ino_to_fsbo(fs,ip->i_number))
    = ip->i_din;
  if (writeBlock(fsbtodb(fs,ino_to_fsba(fs, ip->i_number)),
		 fs->fs_bsize, ffs_space))
    return EIO;
  return 0;
}

int
ffs_blkatoff (struct inode *ip, int offset, char **p, char **buf)
{
  int f = 0, error;
  daddr_t block;

  error = ffs_bmap(ip, lblkno(fs, offset), &block);
  if (error)
    return error;
  if (! *buf)
  {
    f = 1;
    *buf = malloc(fs->fs_bsize);
    if (! *buf)
      return ENOMEM;
  }
  if (block < 0)
    memset(*buf, 0, fs->fs_bsize);
  else
  {
    if (readBlock(block, fs->fs_bsize, *buf))
    {
      if (f)
      {
	free(*buf);
	*buf = 0;
      }
      return EIO;
    }
  }
  *p = *buf + blkoff(fs, offset);
  return 0;
}

int
ffs_wblkatoff (struct inode *ip, int offset, char *buf)
{
  daddr_t block;
  int error;

  error = ffs_bmap(ip, lblkno(fs, offset), &block);
  if (error)
    return error;
  /* should check for block == -1 and allocate... use balloc? */
  if (writeBlock(block, fs->fs_bsize, buf))
    return EIO;
  return 0;
}

void
ffs_brelse (char *buf)
{
  free(buf);
}

int
ffs_lookup (const char *path, struct inode *dp, int flags)
{
  const char *ptr, *name;
  char *dir, *buf = 0;
  int len, off, needed, count, offset, error;
  struct dirent *d;

  /* this shouldn't be necessary, but gcc gets confused */
  off = needed = count = offset = 0;

  if (!path || !*path)
    return EINVAL;
  else if (*path == '/')
  {
    dp->i_ino = ROOTINO;
    dp->i_number = 0;
    path++;
  }
  else if (dp->i_number)
    dp->i_ino = dp->i_number;
  else
    return EINVAL;

  dp->i_count = 0;
  while (path && *path)
  {
    if (!(ptr = strchr(path, '/')))
    {
      if (!*path)
      {
	error = EINVAL;
	goto bad;
      }
      len = strlen(path);
      name = path;
      path = 0;
      if (flags & LOOKUP_EMPTY)
      {
	needed = roundup(len+1+8, 4);
	count = 0;
      }
    }
    else
    {
      len = ptr - path;
      name = path;
      path = ptr + 1;
      if (!len)
	continue;
    }

    if (dp->i_number != dp->i_ino)
    {
      dp->i_number = dp->i_ino;
      error = ffs_getdinode(dp->i_number, &dp->i_din);
      if (error)
	goto bad;
      if ((dp->i_din.di_mode & IFMT) != IFDIR)
      {	
	error = ENOTDIR;
	goto bad;
      }
    }

    for (dp->i_offset = 0;
	 dp->i_offset < dp->i_size;
	 dp->i_offset += d->d_reclen, off += d->d_reclen)
    {
      if (! (dp->i_offset & (fs->fs_bsize - 1)))
      {
	error = ffs_blkatoff(dp, dp->i_offset, &dir, &buf);
	if (error)
	  goto bad;
	off = 0;
      }
      if (! (dp->i_offset & DIRBLKMSK))
	dp->i_count = 0;
      else
	dp->i_count = dp->i_reclen;
      d = (struct dirent *)(dir + off);
      dp->i_reclen = d->d_reclen;
      if (d->d_fileno && d->d_namlen == len && !memcmp(d->d_name, name, len))
	goto found;
      if (needed)
	if ((! d->d_fileno) && d->d_reclen >= needed)
	{
	  count = d->d_reclen;
	  offset = dp->i_offset;
	  needed = 0;
	}
	else if ((d->d_reclen - 8 - d->d_namlen) > needed)
	{
	  count = d->d_reclen - roundup(d->d_namlen+1+8, 4);
	  offset = dp->i_offset;
	  needed = 0;
	}
    }
    if ((!path || !*path) && (flags & LOOKUP_EMPTY))
    {
      dp->i_count = count;
      if (count)
	dp->i_offset = offset;
      goto good;
    }
    error = ENOENT;
    goto bad;
  found:
    dp->i_ino = d->d_fileno;
  }
  if (flags & LOOKUP_EMPTY)
  {
    error = EEXIST;
    goto bad;
  }
good:
  dp->i_flag = 0;
  dp->i_dev = 0;
  dp->i_fs = fs;
  error = 0;
bad:
  if (buf)
    ffs_brelse(buf);
  return (error);
}

int
ffs_insert (const char *name, struct inode *dp, struct inode *ip, int type)
{
  char *dir, *buf = 0;
  struct dirent *d;
  int reclen, error;

  if ((dp->i_mode & IFMT) != IFDIR)
    return (ENOTDIR);

  if (dp->i_count)
  {
    error = ffs_blkatoff(dp, dp->i_offset, &dir, &buf);
    if (error)
      return (error);
    d = (struct dirent *)dir;
    if (d->d_fileno)
    {
      reclen = d->d_reclen;
      d->d_reclen = roundup(d->d_namlen+1+8, 4);
      reclen -= d->d_reclen;
      d = (struct dirent *)(dir + d->d_reclen);
      d->d_reclen = reclen;
    }
    d->d_fileno = ip->i_number;
    d->d_type = type;
    d->d_namlen = strlen(name);
    strcpy(d->d_name, name);
    error = ffs_wblkatoff(dp, dp->i_offset, buf);
    ffs_brelse(buf);
    return (error);
  }

  /* lookup didn't find space, so we need to extend the directory */
  /* XXX */
  return (ENOSYS);
}

int
ffs_delete (const char *name, struct inode *dp)
{
  struct dirent *d;
  char *buf = 0;
  int error;

  if ((dp->i_mode & IFMT) != IFDIR)
    return (ENOTDIR);

  if (! dp->i_count)
  {
    /* first entry in block: set d_ino to zero */
    error = ffs_blkatoff(dp, dp->i_offset, (char**)&d, &buf);
    if (error)
      return (error);
    d->d_fileno = 0;
    error = ffs_wblkatoff(dp, dp->i_offset, buf);
    ffs_brelse(buf);
    return (error);
  }

  error = ffs_blkatoff(dp, dp->i_offset - dp->i_count, (char**)&d, &buf);
  if (error)
    return (error);
  d->d_reclen += dp->i_reclen;
  error = ffs_wblkatoff(dp, dp->i_offset - dp->i_count, buf);
  ffs_brelse(buf);
  return (error);
}

int
ffs_dirempty (struct inode *ip)
{
  int offset, off, error = 0;
  struct dirent *d;
  char *dir, *buf = 0;

  for (offset = 0, off = 0;
       offset < ip->i_size;
       offset += d->d_reclen, off += d->d_reclen)
  {
    if (! (offset & (fs->fs_bsize - 1)))
    {
      error = ffs_blkatoff(ip, offset, &dir, &buf);
      if (error)
	break;
      off = 0;
    }
    d = (struct dirent *)(dir + off);
    if (d->d_fileno && strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
    {
      error = ENOTEMPTY;
      break;
    }
  }
  if (buf)
    ffs_brelse(buf);
  return (error);
}

int
ffs_clear (struct inode *ip)
{
  int i, last, error;
  daddr_t block;

  last = lblkno(fs, ip->i_size);
  if (last >= NDADDR + NINDIR(fs))
    return ENOSYS;
  for (i = 0; i < last; i++)
  {
    error = ffs_bmap(ip, i, &block);
    if (error)
      return error;
    if (block == -1)
      continue;
    ffs_blkfree(ip, dbtofsb(fs, block), fs->fs_bsize);
  }
  error = ffs_bmap(ip, i, &block);
  if (error)
    return error;
  if (block > 0)
    ffs_blkfree(ip, dbtofsb(fs, block), blksize(fs, ip, i));
  if (ip->i_ib[0])
    ffs_blkfree(ip, ip->i_ib[0], fs->fs_bsize);
  /* We can't handle deleting really big files yet :( */
  ip->i_size = 0;
  ip->i_flags |= IN_MODIFIED;
  return 0;
}

int
ffs_clearblk (daddr_t db, int len)
{
  /* can't use ffs_space here */
  char buf[8192];
  memset(buf, 0, 8192);
  printf(" << clearblk(%d,%d)\n",db,len);
  if (writeBlock(db, len, buf))
    return EIO;
  return 0;
}
