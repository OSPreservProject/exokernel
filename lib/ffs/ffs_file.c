#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "ffs_internal.h"

struct fd_info fds[MAX_FDS];

int
ffs_open (const char *name, int flags, int mode)
{
  int fd, error;
  struct inode pin;

  if (!name)
    return (-EINVAL);
  if (fs->fs_ronly && (flags & O_ACCMODE) != O_RDONLY)
    return (-EROFS);
  for (fd = 0; fd < MAX_FDS; fd++)
    if (!fds[fd].used)
      break;
  if (fd == MAX_FDS)
    return (-ENFILE);
  error = ffs_lookup(name, &pin, 0);
  if (error)
  {
    if (!(flags & O_CREAT) || error != ENOENT)
      return (-error);
    if (fs->fs_ronly)
      return (-EROFS);
    error = ffs_lookup(name, &pin, LOOKUP_EMPTY);
    if (error)
      return (-error);
    error = ffs_ialloc(&pin, &fds[fd].in, 0);
    if (error)
      return (-error);
    fds[fd].in.i_mode = IFREG | (mode & 0777);
    fds[fd].in.i_nlink = 1;
    fds[fd].in.i_size = 0;
    fds[fd].in.i_blocks = 0;
    error = ffs_writeinode(&fds[fd].in);
    if (error)
    {
      ffs_free(&pin, fds[fd].in.i_number, 0);
      return (-error);
    }
    error = ffs_insert(strrchr(name, '/')+1, &pin, &fds[fd].in, DT_REG);
    if (error)
    {
      ffs_free(&pin, fds[fd].in.i_number, 0);
      return (-error);
    }
  }
  else if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
    return (-EEXIST);
  else
  {
    error = ffs_getinode(pin.i_ino, &fds[fd].in);
    if (error)
      return (-error);
    if (flags & O_TRUNC)
    {
      if (fs->fs_ronly)
	return (-EROFS);
      error = ffs_clear(&fds[fd].in);
      if (error)
	return (-error);
    }
  }
  if ((fds[fd].in.i_mode & IFMT) == IFDIR)
  {
    if ((flags & O_ACCMODE) != O_RDONLY)
      return (-EISDIR);
  }
  else if ((fds[fd].in.i_mode & IFMT) != IFREG)
    return (-ENXIO);
  fds[fd].used = 1;
  fds[fd].offset = 0;
  fds[fd].flags = flags;
  return fd;
}

int
ffs_close (int fd)
{
  int error;

  if (fd < 0 || fd > MAX_FDS || ! fds[fd].used)
    return EBADF;
  if (fds[fd].in.i_flag & IN_MODIFIED)
  {
    error = ffs_writeinode(&fds[fd].in);
    if (error)
      return (error);
  }
  fds[fd].used = 0;
  return 0;
}

struct inode *
ffs_fstat (int fd)
{
  if (fd < 0 || fd > MAX_FDS || ! fds[fd].used)
    return 0;
  return &fds[fd].in;
}

int
ffs_lseek (int fd, int offset, int whence)
{
  if (fd < 0 || fd > MAX_FDS || ! fds[fd].used)
    return EBADF;
  switch (whence)
  {
  case SEEK_SET:
    if (offset < 0)
      return EINVAL;
    if ((fds[fd].flags & O_ACCMODE) == O_RDONLY)
    {
      if (offset > fds[fd].in.i_size)
	return EINVAL;
    }
    else if (offset > fs->fs_maxfilesize)
      return EFBIG;
    fds[fd].offset = offset;
    break;
  case SEEK_CUR:
    if (((int)fds[fd].offset + offset) < 0)
      return EINVAL;
    if ((fds[fd].flags & O_ACCMODE) == O_RDONLY)
    {
      if (fds[fd].offset + offset > fds[fd].in.i_size)
	return EINVAL;
    }
    else if (fds[fd].offset + offset > fs->fs_maxfilesize)
      return EFBIG;
    fds[fd].offset += offset;
    break;
  case SEEK_END:
    if (((int)fds[fd].in.i_size + offset) < 0)
      return EINVAL;
    if ((fds[fd].flags & O_ACCMODE) == O_RDONLY)
    {
      if (offset > 0)
	return EINVAL;
    }
    else if (fds[fd].in.i_size + offset > fs->fs_maxfilesize)
      return EFBIG;
    fds[fd].offset = fds[fd].in.i_size + offset;
    break;
  default:
    return EINVAL;
  }
  return 0;
}

int
ffs_read (int fd, void *buffer, u_int nbytes)
{
  int bytesdone = 0, num, len, off, error = 0;

  if (fd < 0 || fd > MAX_FDS || ! fds[fd].used)
    return (-EBADF);
  if ((fds[fd].flags & O_ACCMODE) == O_WRONLY)
    return (-EBADF);
  if (nbytes + fds[fd].offset > fds[fd].in.i_size)
    nbytes = fds[fd].in.i_size - fds[fd].offset;
  while (nbytes)
  {
    error = ffs_bmap(&fds[fd].in, lblkno(fs, fds[fd].offset), &num);
    if (error)
      break;

    off = blkoff(fs, fds[fd].offset);
    len = fs->fs_bsize - off;
    if (len > nbytes)
      len = nbytes;

    if (!num)
      memset(buffer, 0, len);
    else
    {
      if (readBlock(num, fs->fs_bsize, ffs_space))
      {
	error = EIO;
	break;
      }
      memcpy(buffer, ffs_space + off, len);
    }
    nbytes -= len;
    bytesdone += len;
    buffer += len;
    fds[fd].offset += len;
  }
  if (error && !bytesdone)
    return (-error);
  return bytesdone;
}

int
ffs_write (int fd, void *buffer, u_int nbytes)
{
  int bytesdone = 0, num, lbn, len, off, error = 0;

  if (fd < 0 || fd > MAX_FDS || ! fds[fd].used)
    return (-EBADF);
  if ((fds[fd].flags & O_ACCMODE) == O_RDONLY)
    return (-EBADF);
  if (fds[fd].flags & O_APPEND)
    fds[fd].offset = fds[fd].in.i_size;
  while (nbytes)
  {
    lbn = lblkno(fs, fds[fd].offset);
    off = blkoff(fs, fds[fd].offset);
    len = fs->fs_bsize - off;
    if (len > nbytes)
      len = nbytes;

    error = ffs_balloc(&fds[fd].in, lbn, off + len, (fs->fs_bsize>len), &num);
    if (error)
      break;

    memcpy(ffs_space + off, buffer, len);
    if (writeBlock(num, blksize(fs, &fds[fd].in, lbn), ffs_space))
    {
      error = EIO;
      break;
    }
    nbytes -= len;
    bytesdone += len;
    buffer += len;
    fds[fd].offset += len;
  }
  if (error && !bytesdone)
    return (-error);
  if (fds[fd].offset > fds[fd].in.i_size)
  {
    fds[fd].in.i_size = fds[fd].offset;
    fds[fd].in.i_flag |= IN_MODIFIED;
  }
  return bytesdone;
}

int
ffs_unlink (const char *path)
{
  struct inode dn, in;
  int error, i;

  if (fs->fs_ronly)
    return EROFS;
  error = ffs_lookup(path, &dn, 0);
  if (error)
    return error;
  error = ffs_getinode(dn.i_ino, &in);
  if (error)
    return error;
  for (i = 0; i < MAX_FDS; i++)
  {
    if (fds[i].used && fds[i].in.i_number == dn.i_ino)
      return EBUSY;
  }
  if ((in.i_mode & IFMT) != IFREG)
    return ENODEV;

  /* Delete reference first */
  error = ffs_delete(strrchr(path, '/')+1, &dn);
  if (error)
    return error;

  /* Get rid of blocks */
  ffs_clear(&in);

  /* Destroy inode */
  memset(&in.i_din, 0, sizeof(in.i_din));
  ffs_writeinode(&in);
  ffs_free(&dn, in.i_number, 0);
  return 0;
}
