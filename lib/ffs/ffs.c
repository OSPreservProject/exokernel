#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <xok/sysinfo.h>
#include "ffs_internal.h"

int
ffs_init (u_int disk, u_int e, u_int j, u_int k, int rdonly)
{
  u_int i, size, blks;
  char *base, *space;
  int *lp;

  if (! ffs_space && ! (ffs_space = malloc(8192)))
    return -1;

  /* open the disk */
  if (!(diskblocks = openDisk(disk, e, j, k)))
    return -2;

  /* get the superblock */
  if (! (fs = (struct fs *) malloc(SBSIZE)))
    return -3;

  if (readBlock(SBOFF >> 9/*__sysinfo.si_disks[disk].d_bshift*/, SBSIZE, fs))
  {
    free(fs);
    return -4;
  }

  if (fs->fs_magic != FS_MAGIC
      || fs->fs_bsize > MAXBSIZE
      || fs->fs_bsize < sizeof(struct fs))
  {
    free(fs);
    return -5;
  }

  if (! fs->fs_clean)
  {
    free(fs);
    return -6;
  }

  strcpy(fs->fs_fsmnt, "exopc/ffs");
  if (rdonly)
    fs->fs_ronly = 1;
  else
  {
    fs->fs_ronly = 0;
    fs->fs_clean = 0;
    if (writeBlock(SBOFF >> 9, SBSIZE, fs))
    {
      free(fs);
      return -8;
    }
  }

  /* get the cyl grp and cluster summary areas */
  size = fs->fs_cssize;
  blks = howmany(size, fs->fs_fsize);
  if (fs->fs_contigsumsize > 0)
    size += fs->fs_ncg * sizeof(int32_t);
  base = space = malloc((u_long)size);
  for (i = 0; i < blks; i += fs->fs_frag)
  {
    size = fs->fs_bsize;
    if (i + fs->fs_frag > blks)
      size = (blks - i) * fs->fs_fsize;
    if (readBlock(fsbtodb(fs, fs->fs_csaddr + i), size, space))
    {
      free(base);
      free(fs);
      return -7;
    }
    fs->fs_csp[fragstoblks(fs, i)] = (struct csum *)space;
    space += size;
  }
  if (fs->fs_contigsumsize > 0)
  {
    fs->fs_maxcluster = lp = (int32_t *)space;
    for (i = 0; i < fs->fs_ncg; i++)
      *lp++ = fs->fs_contigsumsize;
  }

  return 0;
}

static int
ffs_cgupdate (void)
{
  int blks, size, i, allerror = 0, error;
  caddr_t space;

  blks = howmany(fs->fs_cssize, fs->fs_fsize);
  space = (caddr_t)fs->fs_csp[0];
  for (i = 0; i < blks; i += fs->fs_frag)
  {
    size = fs->fs_bsize;
    if (i + fs->fs_frag > blks)
      size = (blks - i) * fs->fs_fsize;
    memcpy(ffs_space, space, size);
    error = writeBlock(fsbtodb(fs, fs->fs_csaddr + i), size, ffs_space);
    if (error && !allerror)
      allerror = error;
    space += size;
  }
  return (allerror);
}

int
ffs_shutdown (void)
{
  int i;

  for (i = 0; i < MAX_FDS; i++)
  {
    if (fds[i].used)
    {
      if (!fs->fs_ronly && fds[i].in.i_flag & IN_MODIFIED)
	ffs_writeinode(&fds[i].in);
      fds[i].used = 0;
    }
  }
  if (!fs->fs_ronly && ffs_cgupdate() == 0)
  {
    fs->fs_clean = FS_ISCLEAN;
    fs->fs_fmod = 0;
    memcpy(ffs_space, fs, fs->fs_sbsize);
    writeBlock(SBOFF >> 9, fs->fs_sbsize, ffs_space);
  }
  closeDisk();
  free(ffs_space);
  ffs_space = 0;
  free(fs->fs_csp[0]);
  free(fs);
  return 0;
}
