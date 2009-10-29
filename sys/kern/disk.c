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

/*
 * ExoPC wrapper on top of SCSI top-level driver
 *
 * Currently most I/O is done through the buffer cache which uses routines
 * in this file to prepare disk requests and initiate them. I/O can bypass
 * the buffer cache and go directly to disk using sys_disk_request.
 *
 */
#define __DISK_MODULE__

#include <xok/sysinfo.h>
#include <xok/pmap.h>
#include <xok/capability.h>
#include <xok/disk.h>
#include <xok/env.h>
#include <xok/kerrno.h>
#include <xok/malloc.h>
#include <xok/printf.h>
#include <xok/pxn.h>
#include <xok/sys_proto.h>
#include <xok/types.h>
#include <xok_include/assert.h>

#ifdef MEASURE_DISK_TIMES
/* Use PC cycle counters on each request */
#include <xok/pctr.h>
pctrval disk_pctr_start;
pctrval disk_pctr_return;
pctrval disk_pctr_done;
#endif

#define READ_MASK	(PG_P|PG_U)
#define WRITE_MASK	(PG_P|PG_W|PG_U)

u_int
disk_sector_sz (u_int dev)
{
  assert (dev < SYSINFO_GET(si_ndisks));
  return (SYSINFO_PTR_AT(si_disks,dev))->d_bsize;
}

u_quad_t
disk_size (u_int dev)
{
  assert (dev < SYSINFO_GET(si_ndisks));
  return (SYSINFO_PTR_AT(si_disks,dev))->d_size;
}

void
disk_giveaway (void)
{
  int ret;
  u_int i;
  struct Xn_xtnt xtnt;
  cap cap;

  for (i = 0; i < SYSINFO_GET(si_ndisks); i++) {
    struct disk* d = SYSINFO_PTR_AT(si_disks, i);

    printf ("Giving extentgroup permissions to disk %u"
	    " (size: %qu sectors, sector size: %u bytes, pages: %qu)\n", 
	    i, d->d_size, d->d_bsize, ((d->d_size * d->d_bsize) / NBPG));

    /* format of a full disk capability is CAP_NM_DISK.partno.micro-partno */
    cap.c_valid = 1;
    cap.c_isptr = 0;
    cap.c_len = 2;
    cap.c_perm = CL_ALL;
    bzero (cap.c_name, sizeof (cap.c_name));
    cap.c_name[0] = CAP_NM_DISK;
    cap.c_name[1] = (unsigned char) i;

    /* set up a pxn for the entire partition */
    SYSINFO_PTR_AT(si_pxn,i)->xa_dev = i;
    SYSINFO_PTR_AT(si_pxn,i)->xa_name = 0;
    ret = pxn_alloc (1, 1, 1, &cap, &cap, SYSINFO_PTR_AT(si_pxn,i));
    if (ret != 0) {
      /* bootup */
      panic ("disk_giveaway: pxn_alloc for disk %u failed (ret %d)\n", i, ret);
    }

    xtnt.xtnt_block = 0;
    xtnt.xtnt_size = d->d_size;

    ret = pxn_kernel_add_xtnt (SYSINFO_PTR_AT(si_pxn,i), &xtnt);
    if (ret != 0) {
      /* bootup */
       panic ("disk_giveaway: pxn_kernel_add_xtnt for disk %u failed "
	      "(ret %d)\n", i, ret);
    }
  }
}

struct buf *
disk_buf_alloc ()                  
{
  return ((struct buf *) malloc (sizeof (struct buf)));
}

void
disk_buf_free (struct buf *bp)
{
  u_int datalen;
  u_int data, next;

  /* unpin pages */
  if (bp->b_flags & B_SCSICMD) {
    struct scsicmd *scsicmd = (struct scsicmd *) bp->b_memaddr;
    data = (u_int) scsicmd->data_addr;
    datalen = scsicmd->datalen;
    free (scsicmd->scsi_cmd);
    free (scsicmd);
  } else {
    data = (u_int)bp->b_memaddr;
    datalen = bp->b_bcount;
  }

  while (datalen > 0) {
    if (bp->b_flags & B_BC_REQ) {
      ppage_unpin (kva2pp (data));
    } else {
      struct Env *e;
      int r;

      e = env_id2env (bp->b_envid, &r);
      assert (e); /* XXX - what if an env with an outstanding request dies? */
      ppage_unpin (ppages_get(PGNO (*env_va2ptep (e, data))));
    }
    /* go to start of next page of data */
    next = (data & ~PGMASK) + NBPG;
    if (next - data >= datalen) break;
    assert (next > data);
    datalen -= next - data;
    data = next;
  }

  if (bp->b_resptr) {
    ppage_unpin (kva2pp ((u_int) bp->b_resptr));
  }

  free (bp);
}

void
disk_buf_free_chain (struct buf *bp)
{
  struct buf *bp2;

  while (bp) {
    bp2 = bp->b_sgnext;
    disk_buf_free (bp);
    bp = bp2;
  }
}

static struct buf *
bp_copy (struct buf *reqbp)
{
   struct buf *bp = disk_buf_alloc ();

   if (!bp) {
     warn ("bp_copy: could not allocate diskbuf");
     return NULL;
   }

   bp->b_next = NULL;
   bp->b_sgnext = NULL;
   bp->b_flags = reqbp->b_flags &
     (B_READ | B_SCATGATH | B_SCSICMD);
   bp->b_dev = reqbp->b_dev;
   bp->b_blkno = reqbp->b_blkno;
   bp->b_bcount = reqbp->b_bcount;
   bp->b_sgtot = 0;
   bp->b_memaddr = reqbp->b_memaddr;
   bp->b_resptr = NULL;
   bp->b_envid = curenv->env_id;
   bp->b_resid = 0;

   return (bp);
}

static int
pin_and_count_noncontigs (char *addr, u_int datalen)
{
  u_int data, next, ppn, prev_ppn;
  u_int noncontigs = 0;

  data = (u_int) addr;
  prev_ppn = SYSINFO_GET(si_nppages);

  while (datalen > 0) {
    ppn = PGNO (*va2ptep (data));
    if (ppn != (prev_ppn + 1)) {
      noncontigs++;
    }
    ppage_pin (ppages_get(ppn));
    prev_ppn = ppn;
    /* go to start of next page of data */
    next = (data & ~PGMASK) + NBPG;
    if (next - data >= datalen) break;
    datalen -= next - data;
    data = next;
  }

  return (noncontigs);
}

static struct buf *
copy_and_pin (struct buf *reqbp, int datalen, int *noncontigs)
{
  struct buf *bp = bp_copy (reqbp);

  if (bp) {
    *noncontigs = pin_and_count_noncontigs (bp->b_memaddr, datalen);
  }

  return bp;
}

/* pass a raw SCSI command down into the driver.  Must have root capability */
/* to the disk in order to perform this incredibly dangerous (but, in some  */
/* circumstances, incredibly useful) operation...                           */

/* The arguments to the scsi_scsi_cmd(...) call are contained in a struct     */
/* scsicmd structure (see xok/buf.h) which is pointed to by reqbp->b_memaddr. */
/* These arguments include a pointer to the SCSI command descriptor block     */
/* itself and a pointer to a data region (for when info is being moved to or  */
/* from the device).                                                          */

/* XXX - we should use copyin, etc, instead of isreadable_* so that user will
   get pagefaults he can handle transparently */
static int
sys_disk_scsicmd (u_int sn, u_int k, struct buf *reqbp)
{
  struct buf *bp;
  struct scsicmd *scsicmd = (struct scsicmd *) reqbp->b_memaddr;
  struct scsicmd *scsicmd2;
  int noncontigs;
  struct disk *di;

  /* must have root capability for system to do a raw SCSI command!!   */
  /* XXX -- later, if desired, deeper checking of validity can reduce  */
  /* this restriction...                                               */
  if (k >= curenv->env_clen || ! curenv->env_clist[k].c_valid) {
    warn ("sys_disk_scsicmd: bad capability number %u\n", k);
    return (-E_CAP_INVALID);
  }
  if (! cap_isroot(&curenv->env_clist[k])) {
    warn ("sys_disk_scsicmd: cap %u is not root capability for system\n", k);
    return (-E_CAP_INSUFF);
  }

  /* must be able to read the reqbp ... */
  if (! (isreadable_varange ((u_int) reqbp, sizeof (struct buf)))) {
    warn ("sys_disk_scsicmd: bad reqbp (%p)", reqbp);
    return (-E_FAULT);
  }

  /* Should be a SCSICMD */
  if (! (reqbp->b_flags & B_SCSICMD)) {
    warn ("sys_disk_scsicmd: not a B_SCSICMD\n");
    return (-E_INVAL);
  }

  /* Must be proper environment */
  if (reqbp->b_envid != curenv->env_id) {
    warn ("sys_disk_scsicmd: bad envid\n");
    return (-E_INVAL);
  }

  /* no scatter/gather support for raw SCSI commands */
  if (reqbp->b_flags & B_SCATGATH) {
    warn ("sys_disk_scsicmd: B_SCATGATH not allowed with B_SCSICMD\n");
    return (-E_INVAL);
  }

  /* can't send request to non-existent disk... */
  if (reqbp->b_dev >= SYSINFO_GET(si_ndevs)) {
    warn ("sys_disk_scsicmd: there is no disk %u in system\n", reqbp->b_dev);
    return (-E_NOT_FOUND);
  }

  /* check that everything is readable */
  if (! isreadable_varange ((u_int) reqbp->b_memaddr,
			    sizeof (struct scsicmd))) {
    warn ("sys_disk_scsicmd: SCSI command description is not readable\n");
    return (-E_FAULT);
  }

  if (! isreadable_varange ((u_int) scsicmd->scsi_cmd, scsicmd->cmdlen) ) {
    warn ("sys_disk_scsicmd: SCSI command itself is not readable\n");
    return (-E_FAULT);
  }

  if (! isreadable_varange ((u_int)scsicmd->data_addr, scsicmd->datalen) ) {
    warn ("sys_disk_scsicmd: data area for SCSI command is not readable\n");
    return (-E_FAULT);
  }

  /* length of SCSI command must not be greater than B_SCSICMD_MAXLEN */
  if (scsicmd->cmdlen > B_SCSICMD_MAXLEN) {
    /* XXX - why do we compare scsicmd->cmdlen, but we print out
       reqbp->b_bcount? */
    warn ("sys_disk_scsicmd: specified SCSI command too large (%d > %d)\n",
	  reqbp->b_bcount, B_SCSICMD_MAXLEN);
    return (-E_INVAL);
  }

  /* copy the SCSI command to avoid sharing it with app */
  bp = bp_copy (reqbp);
  if (bp == NULL) {
    warn ("sys_disk_scsicmd: kernel malloc for bp failed\n");
    return (-E_NO_MEM);
  }
  bp->b_memaddr = malloc (sizeof (struct scsicmd));
  if (bp->b_memaddr == NULL) {
    warn ("sys_disk_scsicmd: kernel malloc for scsicmd failed\n");
    free (bp);
    return (-E_NO_MEM);
  }
  scsicmd2 = (struct scsicmd *) bp->b_memaddr;
  bcopy (scsicmd, scsicmd2, sizeof (struct scsicmd));
  scsicmd2->scsi_cmd = (struct scsi_generic *) malloc (scsicmd->cmdlen);
  if (scsicmd2->scsi_cmd == NULL) {
    warn ("sys_disk_scsicmd: second kernel malloc failed\n");
    free (bp->b_memaddr);
    free (bp);
    return (-E_NO_MEM);
  }
  bcopy (scsicmd->scsi_cmd, scsicmd2->scsi_cmd, scsicmd->cmdlen);
  scsicmd2->bp = bp;
  bp->b_resid = scsicmd->datalen;
  bp->b_resptr = (int *) pa2kva (va2pa (reqbp->b_resptr));

  /* pin down the app pages that will later be used by the driver */
  ppage_pin (kva2pp ((u_int) bp->b_resptr));
  noncontigs = pin_and_count_noncontigs (scsicmd2->data_addr,
					 scsicmd2->datalen);
  if (noncontigs >= DISK_MAX_SCATTER) {
    warn ("sys_disk_scsicmd: will require too many scatter/gather entries "
	  "(%d)", noncontigs);
    disk_buf_free (bp);
    return (-E_TOO_BIG);
  }

  /* XXX */
  /* call down to the low-level driver.  GROK -- since the partition stuff   */
  /* creates and abstract disk that is separate from the real one, a hack    */
  /* is needed to get the actual disk strategy routine for raw SCSI commands */
  /* This is fine as long as all disks actually go to the same strategy      */
  /* routine.                                                                */
   di = SYSINFO_PTR_AT(si_disks,0);
   di->d_strategy (bp);

   return (0);
}

/*
 * sys_disk_mbr
 *
 * Read/Write the master boot record of a disk.
 *
 * The mbr contains the bootstrap code that the BIOS
 * loads on startup. This is always in sector 0
 * of the disk being booted. The mbr also contains the
 * partition table for the disk.
 *
 */
int
sys_disk_mbr (u_int sn, int write, u_int dev, int k, char *buffer, int *resptr)
{
  cap c;
  int ret;
  struct buf *diskbuf;

  /* get the capability */
  if ((ret = env_getcap (curenv, k, &c)) < 0)
    return ret;

  /* make sure the root cap was passed in */
  if (!cap_isroot (&c))
    return -E_CAP_INSUFF;

  /* verify the dev */
  if (dev >= SYSINFO_GET(si_ndisks))
    return -E_NOT_FOUND;

  /* check and translate the buffers we were given */
  if ((((u_int) resptr) % sizeof(u_int)) || 
      !(isvawriteable (resptr))) {
    warn ("sys_disk_mrb: bad resptr (%p)", resptr);
    return (-E_FAULT);
  }
  ppage_pin (pa2pp ((va2pa (resptr))));
  resptr = (int *) pa2kva (va2pa (resptr));

  if (write) {
    if (! (iswriteable_varange ((u_int)buffer, 512))) {
      warn ("sys_disk_mbr: bad buffer (%p)", buffer);
      return (-E_FAULT);
    }
  } else {
    if (! (isreadable_varange ((u_int)buffer, 512))) {
      warn ("sys_disk_mbr: bad buffer (%p)", buffer);
      return (-E_FAULT);
    }
  }

  /* get a disk req buffer and fill it in */
  diskbuf = disk_buf_alloc ();
  if (!diskbuf)
    return -E_NO_MEM;

  diskbuf->b_next = NULL;
  diskbuf->b_sgnext = NULL;
  diskbuf->b_dev = dev;
  diskbuf->b_blkno = 0;
  diskbuf->b_bcount = 512;	/* only want to read the first sector */
  diskbuf->b_sgtot = 512;
  diskbuf->b_memaddr = buffer;
  diskbuf->b_envid = curenv->env_id;
  diskbuf->b_resid = 0;
  diskbuf->b_resptr = resptr;
  diskbuf->b_flags = B_ABSOLUTE;		/* bypass partitions table */
  if (write) {
    diskbuf->b_flags |= B_WRITE;
  } else {
    diskbuf->b_flags |= B_READ;
  }

  /* pin it in case the user frees it before the request completes.
     This will be unpinned when sched_reqcomplete is called which
     in turn calls disk_buf_free which calls ppage_unpin. */

  ppage_pin (pa2pp ((va2pa (buffer))));

  /* start the request */
  (SYSINFO_PTR_AT(si_disks,dev))->d_strategy (diskbuf);

  return 0;
}

/*
 * sys_disk_request
 *
 * Disk I/O without going through the buffer cache.
 *
 * xn_user is the name of a pxn that grants access to the disk
 * reqbp is a list of scatter/gather requests
 * k is which capability in the env should be checked
 *
 * permission is granted to perform the operation if:
 * 1) the blocks in reqbp are covered by the pxn
 * 2) the capability gives access to the pxn
 *
 */
int
sys_disk_request (u_int sn, struct Xn_name *xn_user, struct buf *reqbp,
		  u_int k)
{
  struct Xn_name xn;
  struct Xn_xtnt xtnt;
  struct Pxn *pxn;
  cap c;
  int ret;
  int access;
  struct disk *di;
  int *resptr = NULL;
  u_int bcount = 0;
  struct buf *bp, *segbp, *nsegbp;
  int noncontigs = 0, nctemp;

#ifdef MEASURE_DISK_TIMES
  disk_pctr_start = rdtsc();
#endif

  /* XXX - use PFM or copyin instead of isreadable_* */

  /* bypass for direct scsi commands */
  if (reqbp->b_flags & B_SCSICMD) {
    return sys_disk_scsicmd (sn, k, reqbp);
  }

  /* get the capability */
  if ((ret = env_getcap (curenv, k, &c)) < 0)
    return ret;

  /* and the pxn */
  copyin (xn_user, &xn, sizeof (xn));
  if (! (pxn = lookup_pxn (&xn))) {
    warn ("sys_disk_request: no pxn found");
    return (-E_NOT_FOUND);
  }

  /* XXX - do we need to check that this is a physical disk? */
  /* get a refernce to the disk unit for this command */
  di = SYSINFO_PTR_AT(si_disks,xn.xa_dev);

  /* Iterate over the request list checking:
     -- if the request is transfering data to/from
     memory that this user can read/write.
     -- if the pxn and capability specified give
     access to these blocks */
  for (segbp = reqbp; ; segbp = (struct buf *) segbp->b_sgnext) {
    if (! (isreadable_varange ((u_int)segbp, sizeof(struct buf)))) {
      warn ("sys_disk_request: bad reqbp (%p)", segbp);
      return (-E_FAULT);
    }

    if (segbp->b_flags & B_READ) {
      access = ACL_R;
    } else {
      access = ACL_W;
    }
    
    xtnt.xtnt_block = segbp->b_blkno;
    xtnt.xtnt_size = segbp->b_bcount / di->d_bsize;
    bcount += segbp->b_bcount;

    if (! pxn_authorizes_xtnt (pxn, &c, &xtnt, access, &ret)) {
      warn ("sys_disk_request: pxn/cap does not grant access to block(s)");
      return ret;
    }

    if (! ((reqbp->b_flags & B_READ) ? iswriteable_varange
	   : isreadable_varange) ((u_int) segbp->b_memaddr, segbp->b_bcount)) {
      warn ("sys_disk_request: bad b_memaddr: %p (b_bcount %d)",
	    segbp->b_memaddr, segbp->b_bcount);
      return (-E_FAULT);
    }

    if (! (segbp->b_flags & B_SCATGATH)) {
      if (segbp->b_resptr) {
	resptr = segbp->b_resptr;
	if ((((u_int) resptr) % sizeof(u_int)) || 
	    !(isvawriteable (resptr))) {
	  warn ("sys_disk_request: bad resptr (%p)", resptr);
	  return (-E_FAULT);
	}
	resptr = (int *) pa2kva (va2pa (resptr));
      }
      break;
    }
  }

  if ((reqbp->b_flags & B_SCATGATH) && bcount != reqbp->b_sgtot) {
    warn ("sys_disk_request: invalid scatter/gather, with total (%u) unequal "
	  "to sum of parts (%u)", reqbp->b_sgtot, bcount);
    return (-E_INVAL);
  }

  /* are we done before we've started? */
  if (bcount == 0) {
    if (resptr)
      (*resptr)--;
    return (0);
  }

  if (bcount & di->d_bmod) {
    warn ("sys_disk_request: bad bcount %u", bcount);
    return (-E_INVAL);
  }

  /* copy request into kernel buffer */
  segbp = reqbp;
  nsegbp = NULL;
  reqbp = NULL;
  do {
    segbp->b_dev = di->d_id;
    bp = copy_and_pin(segbp, segbp->b_bcount, &nctemp);
    if (!bp) {
      warn ("sys_disk_request: could not copy_and_pin");
      /* XXX - cleanup before returning */
      return (-E_NO_MEM);
    }
    noncontigs += nctemp;
    if (nsegbp) nsegbp->b_sgnext = bp;
    if (!reqbp) reqbp = bp;
    if (noncontigs >= DISK_MAX_SCATTER) {
      warn ("sys_disk_request: would require too many scatter/gather entries "
	    "(%d)", noncontigs);
      /* XXX - cleanup before returning */
      return (-E_INVAL);
    }
    nsegbp = bp;
    segbp = segbp->b_sgnext;
  } while (nsegbp->b_flags & B_SCATGATH);

  nsegbp->b_resptr = resptr;

  if (resptr) ppage_pin (kva2pp((u_int) resptr));

  /* call appropriate strategy routine */
  di->d_strategy (reqbp);

#ifdef MEASURE_DISK_TIMES
  disk_pctr_return = rdtsc();
#endif

  return (0);
}

int
disk_prepare_bc_request (u_int devno, u_quad_t blkno, void *vaddr, u_int flags,
			 int *resptr, struct buf **headbpp)
{
  struct buf *bp;

  /* XXX - test for big blkno wraparound */
  if ((devno >= SYSINFO_GET(si_ndisks)) ||
      (((blkno * NBPG) / (SYSINFO_PTR_AT(si_disks,devno))->d_bsize) >=
       (SYSINFO_PTR_AT(si_disks,devno))->d_size)) {
    warn ("disk_prepare_bc_request: invalid devno (%u) or blkno (%qu)",
	  devno, blkno);
    return (-E_INVAL);
  }

  if (headbpp == NULL) {
    warn ("disk_prepare_bc_request: headbpp == NULL");
    return (-E_INVAL);
  }

  bp = disk_buf_alloc();
  if (!bp)
    return (-E_NO_MEM);

  bp->b_next = NULL;
  bp->b_sgnext = NULL;
  bp->b_flags = flags;
  bp->b_dev = devno;
  bp->b_blkno = (blkno * NBPG) / (SYSINFO_PTR_AT(si_disks,devno))->d_bsize;
  bp->b_bcount = NBPG;
  bp->b_sgtot = 0;
  bp->b_memaddr = vaddr;
  bp->b_envid = curenv->env_id;
  bp->b_resid = 0;
  bp->b_resptr = NULL;

  ppage_pin (kva2pp((u_int) vaddr));

  if (*headbpp) {
    struct buf *tmpbp = *headbpp;

    while (tmpbp->b_flags & B_SCATGATH) {
      tmpbp = (struct buf *) tmpbp->b_sgnext;
      if (tmpbp == NULL) {
	warn ("disk_prepare_bc_request: bad scatter/gather list");
	ppage_unpin (kva2pp((u_int) vaddr));
	free (bp);
	return (-E_INVAL);
      }
    }

    /* XXX - test for big blkno wraparound */
    if (bp->b_blkno != (tmpbp->b_blkno +
			(tmpbp->b_bcount / 
			 (SYSINFO_PTR_AT(si_disks,bp->b_dev))->d_bsize))) {
      warn ("disk_prepare_bc_request: noncontiguous requests "
	    "(prevblk %qu, size %u, curblk %qu) can't be merged",
	    tmpbp->b_blkno, tmpbp->b_bcount, bp->b_blkno);
      ppage_unpin (kva2pp((u_int) vaddr));
      free (bp);
      return (-E_INVAL);
    }

    (*headbpp)->b_sgtot += NBPG;
    tmpbp->b_flags |= B_SCATGATH;
    tmpbp->b_sgnext = bp;
  } else {
    *headbpp = bp;
    bp->b_sgtot = NBPG;
  }

  if (resptr) {
    ppage_pin (kva2pp(((u_int) resptr)));
  }
  bp->b_resptr = resptr;

  return (0);
}

int
disk_bc_request (struct buf *bp)
{
  if ((bp == NULL) || (bp->b_dev >= SYSINFO_GET(si_ndisks))) {
    warn ("disk_bc_request: invalid arguments (buf %p, devno %u)", bp,
	  (bp ? bp->b_dev : 0));
    return -E_INVAL;
  }

  if ((bp->b_blkno % 8) || (bp->b_bcount % 4096) || (bp->b_sgtot % 4096)) {
    warn ("disk_bc_request: unexpected disk request (envid %u, blkno %qu, "
	  "bcount %u, sgtot %u)\n", curenv->env_id, bp->b_blkno, bp->b_bcount,
	  bp->b_sgtot);
    return -E_INVAL;
  }

  bp->b_resid = NBPG;

  /* call appropriate strategy routine */
  (SYSINFO_PTR_AT(si_disks,bp->b_dev))->d_strategy (bp);

  return 0;
}

/* XXX - this is a mess */
/* returns 1 if there was an interrupt processed and 0 otherwise */
int
disk_pollforintrs (u_int devno)
{
  extern int ncr_intr(u_int irq);

  /* XXX - GROK -- for now, just bank on only having one controller! */
  assert (SYSINFO_GET(si_scsictlr_cnt) == 1);
  return (ncr_intr (SYSINFO_GET_AT(si_scsictlr_irqs,0)));
}

#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

