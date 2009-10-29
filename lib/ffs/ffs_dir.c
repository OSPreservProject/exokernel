#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "ffs_internal.h"

struct dir *
ffs_opendir (const char *dirname)
{
  struct inode in;
  struct dir *dir;

  if (!dirname)
    return 0;
  if (ffs_lookup(dirname, &in, 0) || ffs_getinode(in.i_ino, &in))
    return 0;
  if ((in.i_mode & IFMT) != IFDIR)
    return 0;
  if (!(dir = (struct dir*) malloc(sizeof(struct dir))))
    return 0;
  dir->in = in;
  dir->offset = 0;
  dir->block = 0;
  dir->ain.i_number = 0;
  return dir;
}

struct dirent *
ffs_readdir (struct dir *dir)
{
  int error;
  struct dirent *d;
  daddr_t blk;

  if (!dir || dir->offset == dir->in.i_size)
    return 0;
  if (! (dir->offset % (fs->fs_bsize - 1)))
  {
    error = ffs_bmap(&dir->in, lblkno(fs, dir->block), &blk);
    if (error)
      return 0;
    if (readBlock(blk, fs->fs_bsize, &dir->buf))
      return 0;
  }
  do
  {
    if (dir->offset == dir->in.i_size)
      return 0;
    d = (struct dirent *)(&dir->buf[dir->offset]);
    if (dir->offset + d->d_reclen > fs->fs_bsize)
      printf("bad things about to happen\n");
    dir->offset += d->d_reclen;
    if (! d->d_reclen)
    {
      printf("ffs_readdir: d->d_reclen=0!\n");
      return 0;
    }
    if (! (dir->offset % (fs->fs_bsize - 1)))
      dir->block++;
  } while (! d->d_fileno);
  return d;
}

int
ffs_closedir (struct dir *dirp)
{
  if (!dirp)
    return EINVAL;
  free(dirp);
  return 0;
}

int
ffs_mkdir (const char *path, int mode)
{
  struct inode pip, ip;
  struct dirent *d;
  daddr_t block;
  int error;

  if (fs->fs_ronly)
    return EROFS;

  /* First make sure it doesn't already exist, and find parent directory */
  error = ffs_lookup(path, &pip, LOOKUP_EMPTY);
  if (error)
    return (error);
  if (pip.i_nlink >= LINK_MAX)
    return EMLINK;

  /* Allocate an inode */
  error = ffs_ialloc(&pip, &ip, IFDIR);
  if (error)
    return (error);
  ip.i_mode = IFDIR | (mode & 0777);
  ip.i_nlink = 2;
  ip.i_size = 0;
  ip.i_blocks = 0;

  /* Allocate a block, and fill it in */
  error = ffs_alloc(&ip, 0, fs->fs_fsize, &block);
  if (error)
  {
    ffs_free(&ip, ip.i_number, IFDIR);
    return (error);
  }
  ip.i_db[0] = block;
  memset(ffs_space, 0, fs->fs_fsize);
  d = (struct dirent *)ffs_space;
  d->d_fileno = ip.i_number;
  d->d_reclen = 12;
  d->d_type = DT_DIR;
  d->d_namlen = 1;
  strcpy(d->d_name, ".");
  d = (struct dirent *)(ffs_space + 12);
  d->d_fileno = pip.i_number;
  d->d_reclen = DIRBLKSIZ - 12;
  d->d_type = DT_DIR;
  d->d_namlen = 2;
  strcpy(d->d_name, "..");
  ip.i_size = 512;
  if (writeBlock(fsbtodb(fs, block), fs->fs_fsize, ffs_space))
  {
    error = EIO;
    goto failed;
  }

  if (ffs_writeinode(&ip))
  {
    error = EIO;
    goto failed;
  }
  error = ffs_insert(strrchr(path, '/')+1, &pip, &ip, DT_DIR);
  if (error)
    goto failed;
  pip.i_nlink++;
  if (ffs_writeinode(&pip))
  {
    error = EIO;
    goto failed;
  }
  return (0);
failed:
  ffs_blkfree(&ip, block, fs->fs_fsize);
  ffs_free(&ip, ip.i_number, IFDIR);
  return (error);
}

int
ffs_rmdir (const char *path)
{
  struct inode dn, in;
  int error;

  if (fs->fs_ronly)
    return EROFS;

  if ((error = ffs_lookup(path, &dn, 0)) != 0 ||
      (error = ffs_getinode(dn.i_ino, &in)) != 0)
    return error;
  if ((in.i_mode & IFMT) != IFDIR)
    return ENOTDIR;

  /* Don't delete / */
  if (in.i_number == ROOTINO)
    return EINVAL;

  /* Make sure it's empty */
  if (in.i_nlink != 2)
    return ENOTEMPTY;
  error = ffs_dirempty(&in);
  if (error)
    return error;

  /* Delete reference to directory first; BSD fsck will
     reattach in lost+found if we crash in the middle */
  error = ffs_delete(strrchr(path, '/')+1, &dn);
  if (error)
    return error;
  dn.i_nlink--;
  ffs_writeinode(&dn);

  /* Get rid of blocks */
  ffs_clear(&in);
  memset(&in.i_din, 0, sizeof(in.i_din));
  ffs_writeinode(&in);
  ffs_free(&dn, in.i_number, IFDIR);
  return 0;
}
