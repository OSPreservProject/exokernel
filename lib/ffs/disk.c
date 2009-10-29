#include <xuser.h>
#include <stdlib.h>
#include <xok/disk.h>
#include <sys/types.h>
#include <xok/sysinfo.h>

#include "ffs.h"

#include "disk.h"

int disk = -1;
u_int e, j, k, disksize, bshift;

int openDisk(u_int ndisk, u_int ne, u_int nj, u_int nk)
{
  if (ndisk >= __sysinfo.si_ndisks)
    return 0;
  disk = ndisk;
  e = ne;
  j = nj;
  k = nk;
  disksize = __sysinfo.si_disks[disk].d_size;
  bshift = __sysinfo.si_disks[disk].d_bshift;
  return disksize;
}

int readBlock(u_int offset, u_int len, void *block)
{
  struct buf b;

  if (disk == -1)
    return -1;
  if (offset+(len>>bshift) >= disksize)
    return -1;

  b.b_flags = B_READ;
  b.b_dev = disk;
  b.b_blkno = offset;
  b.b_bcount = len;
  b.b_memaddr = block;
  b.b_resid = 0;
  b.b_resptr = &b.b_resid;
  if (sys_disk_request(e, j, k, &b)) {
    return -1;
  }
  /* GROK - hack'd way to force waiting for completion */
  while ((b.b_resid == 0) && (ffs_fstat(-1) == 0));
  return 0;
}

int writeBlock(u_int offset, u_int len, void *block)
{
  struct buf b;

  if (disk == -1)
    return -1;
  if (offset+(len>>bshift) >= disksize)
    return -1;

  b.b_flags = B_WRITE;
  b.b_dev = disk;
  b.b_blkno = offset;
  b.b_bcount = len;
  b.b_memaddr = block;
  b.b_resid = 0;
  b.b_resptr = &b.b_resid;
  if (sys_disk_request(e, j, k, &b)) {
    return -1;
  }
  /* GROK - hack'd way to force waiting for completion */
  while ((b.b_resid == 0) && (ffs_fstat(-1) == 0));
  return 0;
}

int bread(u_int dev, u_int blk, u_int len, struct buf **bp)
{
  if (disk == -1)
    return -1;
  if (blk+(len>>bshift) >= disksize)
    return -1;

  if (!(*bp = malloc(sizeof (struct buf))))
    return -1;
  if (!((*bp)->b_memaddr = malloc(len)))
  {
    free(*bp);
    *bp = 0;
    return -1;
  }
  (*bp)->b_flags = B_READ;
  (*bp)->b_dev = disk;
  (*bp)->b_blkno = blk;
  (*bp)->b_bcount = len;
  (*bp)->b_resid = 0;
  (*bp)->b_resptr = &(*bp)->b_resid;
  if (sys_disk_request(e, j, k, *bp))
  {
    free((*bp)->b_memaddr);
    free(*bp);
    *bp = 0;
    return -1;
  }
  /* GROK - hack'd way to force waiting for completion */
  while (((*bp)->b_resid == 0) && (ffs_fstat(-1) == 0));
  return 0;
}

void bdwrite(struct buf *bp)
{
  if (disk == -1 || !bp)
    return;

  bp->b_flags = B_WRITE;
  bp->b_resid = 0;
  bp->b_resptr = &bp->b_resid;
  sys_disk_request(e, j, k, bp);
  /* GROK - hack'd way to force waiting for completion */
  while ((bp->b_resid == 0) && (ffs_fstat(-1) == 0));
  brelse(bp);
}

void brelse(struct buf *bp)
{
  if (bp)
  {
    free (bp->b_memaddr);
    free (bp);
  }
}

void closeDisk(void)
{
  disk = -1;
}
