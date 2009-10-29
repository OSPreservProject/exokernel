
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

#include "scsi_port.h"

/*	$NetBSD: sd.c,v 1.100 1996/05/14 10:38:47 leo Exp $	*/

/*
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Originally written by Julian Elischer (julian@dialix.oz.au)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@dialix.oz.au) Sept 1992
 */

/* #define MEASURE_DISK_TIMES	/* Use PC cycle counters on each request */

#ifdef __ExoPC__

#define __DISK_MODULE__

#include <xok/defs.h>
#include <xok/buf.h>
#include <xok/disk.h>
#include <xok/bc.h>
#include <xok/env.h>
#include <xok/scheduler.h>
#include <xok/printf.h>
#include <xok_include/errno.h>
#include "scsi_all.h"
#include "scsi_disk.h"
#include "scsiconf.h"
#include <xok/pctr.h>
#include <xok/sd.h>
#include <xok/sysinfo.h>

#if 0
#include <disk/exodisk.h>
extern uint16 edc_hash_key[]; /* needed within next include */
#include <disk/exodisk_functions.h>
#endif
#else
#include <xok/types.h>
#include <xok/param.h>
#include <xok/systm.h>
#include <xok/kernel.h>
#include <xok/file.h>
#include <xok/stat.h>
#include <xok/ioctl.h>
#include <xok/buf.h>
#include <xok/uio.h>
#include <xok/malloc.h>
#include <xok/errno.h>
#include <xok/device.h>
#include <xok/disklabel.h>
#include <xok/disk.h>
#include <xok/proc.h>
#include <xok/conf.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#endif

#define	SDOUTSTANDING	4
#define	SDRETRIES	4

#ifdef __ExoPC__
#ifndef howmany
#define	howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif
#else
#define	SDUNIT(dev)			DISKUNIT(dev)
#define	SDPART(dev)			DISKPART(dev)
#define	MAKESDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define	SDLABELDEV(dev)	(MAKESDDEV(major(dev), SDUNIT(dev), RAW_PART))
#endif

#if 0
struct sd_softc {
#ifdef __ExoPC__
	int sc_devno;
	u_int size, secsize;
#else
	struct device sc_dev;
	struct disk sc_dk;
#endif

	int flags;
#define	SDF_LOCKED	0x01
#define	SDF_WANTED	0x02
#ifndef __ExoPC__
#define	SDF_WLABEL	0x04		/* label is writable */
#define	SDF_LABELLING	0x08		/* writing label */
#endif
#define	SDF_ANCIENT	0x10		/* disk is ancient; for minphys */
	struct scsi_link *sc_link;	/* contains our targ, lun, etc. */
	struct disk_parms {
		u_char heads;		/* number of heads */
		u_short cyls;		/* number of cylinders */
		u_char sectors;		/* number of sectors/track */
		int blksize;		/* number of bytes/sector */
		u_long disksize;	/* total number sectors */
	} params;
#ifdef __ExoPC__
        struct  buf *sched_queue;       /* queue of pending operations */
        struct  buf *sched_next;	/* next pending op to schedule */
        daddr_t sched_prevblkno;	/* b_blkno of last scheduled op */
#else
	struct buf buf_queue;
#endif
};
#endif

#ifdef __ExoPC__
void	sdattach __P((struct scsibus_attach_args *sa));
#else
int	sdmatch __P((struct device *, void *, void *));
void	sdattach __P((struct device *, struct device *, void *));
#endif
int	sdlock __P((struct sd_softc *));
void	sdunlock __P((struct sd_softc *));
#ifdef __ExoPC__
void    sdstrategy __P((struct buf *));
#endif
void	sdminphys __P((struct buf *));
#ifndef __ExoPC__
void	sdgetdisklabel __P((struct sd_softc *));
#endif
void	sdstart __P((void *, int));
int	sddone __P((struct scsi_xfer *, int));
int	sd_reassign_blocks __P((struct sd_softc *, u_long));
int	sd_get_parms __P((struct sd_softc *, int));

#ifdef __ExoPC__
#if 0
#define SD_MAXDEVS 8
static struct sd_softc sd_devs[SD_MAXDEVS];
static int sd_ndevs;
#endif
#else
struct cfattach sd_ca = {
	sizeof(struct sd_softc), sdmatch, sdattach
};

struct cfdriver sd_cd = {
	NULL, "sd", DV_DISK
};

struct dkdriver sddkdriver = { sdstrategy };
#endif

struct scsi_device sd_switch = {
	NULL,			/* Use default error handler */
	sdstart,		/* have a queue, served by this */
	NULL,			/* have no async handler */
	sddone,			/* deal with stats at interrupt time */
};

#ifndef __ExoPC__
struct scsi_inquiry_pattern sd_patterns[] = {
	{T_DIRECT, T_FIXED,
	 "",         "",                 ""},
	{T_DIRECT, T_REMOV,
	 "",         "",                 ""},
	{T_OPTICAL, T_FIXED,
	 "",         "",                 ""},
	{T_OPTICAL, T_REMOV,
	 "",         "",                 ""},
};

int
sdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct scsibus_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    (caddr_t)sd_patterns, sizeof(sd_patterns)/sizeof(sd_patterns[0]),
	    sizeof(sd_patterns[0]), &priority);
	return (priority);
}
#endif

#if 0
extern struct ed_info ed_info[];
#endif

/* Request queueing and scheduling for SCSI disk drives */
/* Use C-LOOK, implemented with a tail-pointed circle queue */
/* kept in ascending order.  relevant rz_softc structure values: */
/* "sched_queue": the list of pending requests kept as above. */
/* "sched_next": pointer to request whose b_next points to next */
/*               one to schedule (indirection needed for list manage) */
/* "sched_prevblkno": b_blkno for last scheduled request. */

/* This approach puts most of the scheduling computation at insert time, */
/* where (arguably) it belongs so it gets charged to the right process.  */

#if 1		/* C-LOOK scheduling */

void sched_addtoqueue (sc, bp)
struct sd_softc *sc;
struct buf *bp;
{
   struct buf *tmp = sc->sched_queue;  /* points to list tail */
   if (tmp == NULL) {
      bp->b_next = bp;
      sc->sched_queue = bp;
      sc->sched_next = bp;
   } else if (bp->b_blkno >= tmp->b_blkno) {
      bp->b_next = tmp->b_next;
      tmp->b_next = bp;
      sc->sched_queue = bp;
      if ((sc->sched_next == tmp) && ((bp->b_next->b_blkno >= sc->sched_prevblkno) || (bp->b_blkno < sc->sched_prevblkno))) {
         sc->sched_next = bp;
      }
   } else {
      while (bp->b_blkno >= tmp->b_next->b_blkno) {
         tmp = tmp->b_next;
      }
      bp->b_next = tmp->b_next;
      tmp->b_next = bp;
      if ((sc->sched_next == tmp) && (bp->b_blkno < sc->sched_prevblkno) && (bp->b_next->b_blkno >= sc->sched_prevblkno)) {
         sc->sched_next = bp;
      }
   }
   if (bp->b_flags & B_READ) {
      // si->si_disks[bp->b_dev].d_queuedReads++;
      struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);
      d->d_queuedReads++;
   } else {
      // si->si_disks[bp->b_dev].d_queuedWrites++;
      struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);
      d->d_queuedWrites++;
   }
}

struct buf * sched_selectnext (sc)
struct sd_softc *sc;
{
   struct buf *tmp = sc->sched_next;
   struct buf *bp = NULL;
   if (tmp) {
      bp = tmp->b_next;
      if (bp == tmp) {
         sc->sched_queue = NULL;
         sc->sched_next = NULL;
      } else {
         tmp->b_next = bp->b_next;
         if (sc->sched_queue == bp) {
            sc->sched_queue = tmp;
         }
      }
      sc->sched_prevblkno = bp->b_blkno;
      bp->b_next = NULL;
      if (bp->b_flags & B_READ) {
         // si->si_disks[bp->b_dev].d_ongoingReads++;
         struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);
         d->d_ongoingReads++;
      } else {
         // si->si_disks[bp->b_dev].d_ongoingWrites++;
         struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);
         d->d_ongoingWrites++;
      }
   }
   return (bp);
}

#else   /* FCFS scheduling */

void sched_addtoqueue (sc, bp)
struct sd_softc *sc;
struct buf *bp;
{
   struct buf *tmp = sc->sched_queue;
   bp->b_next = NULL;
   if (tmp == NULL) {
      sc->sched_queue = bp;
   } else {
      while (tmp->b_next != NULL) {
         tmp = tmp->b_next;
      }
      tmp->b_next = bp;
   }
}


struct buf * sched_selectnext (sc)
struct sd_softc *sc;
{
   struct buf *bp = sc->sched_queue;
   if (bp) {
      sc->sched_queue = bp->b_next;
      bp->b_next = NULL;
   }
   return (bp);
}

#endif

void print_buf(struct buf *bp) {
  printf("dev: %u, flags: %8x cnt: %u env: %u resid: %d bk: %qu\n",
	 bp->b_dev, 
	 bp->b_flags,
	 bp->b_bcount,
	 bp->b_envid,
	 bp->b_resid,
	 bp->b_blkno);

}


void sched_reqcomplete (sc, bp)
struct sd_softc *sc;
struct buf *bp;
{
  int *resptr = 0;
  struct Env *e = 0;

  if (bp->b_flags & B_SCSICMD) {
    if (bp->b_resptr) {
      *(bp->b_resptr) -= 1;
    }
    disk_buf_free (bp);
    return;
  }

  //print_buf(bp); // to print all disk requests
  if (bp->b_flags & B_READ) {
    struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);
    d->d_ongoingReads--;
    d->d_queuedReads--;
  } else {
    struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);
    d->d_ongoingWrites--;
    d->d_queuedWrites--;
  }

  bp->b_flags |= B_DONE;
  if (! (bp->b_flags & B_KERNEL)) {
    struct buf *bp2;
    struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);

    while (bp->b_flags & B_SCATGATH) {
      if (bp->b_flags & B_BC_REQ)
	bc_diskreq_done (bp->b_dev,
			 ((bp->b_blkno * d->d_bsize) / NBPG),
			 !(bp->b_flags & B_READ), bp->b_memaddr, 0);
      bp2 = bp->b_sgnext;
      disk_buf_free (bp);
      bp = bp2;
    }
    resptr = bp->b_resptr;
    if (bp->b_envid) {
      int r;

      e = env_id2env(bp->b_envid, &r);
    }
    if (bp->b_flags & B_BC_REQ)
      /* XXX - check for big blkno wraparound */
      bc_diskreq_done (bp->b_dev, ((bp->b_blkno * d->d_bsize) / NBPG),
		       ! (bp->b_flags & B_READ), bp->b_memaddr, bp->b_envid);

#ifdef MEASURE_DISK_TIMES
    {
      extern pctrval disk_pctr_start;
      extern pctrval disk_pctr_return;
      extern pctrval disk_pctr_done;
      disk_pctr_done = rdtsc();
      printf ("disk request time: initiation %d, service %d\n",
	      (int)((disk_pctr_return - disk_pctr_start) & 0xFFFFFFFF),
	      (int)((disk_pctr_done - disk_pctr_return) & 0xFFFFFFFF));
    }
#endif

    if (resptr) (*resptr)--;
    disk_buf_free (bp);
  } else if (bp->b_flags & B_SCATGATH) {
    panic ("illegal combination of B_KERNEL and B_SCATGATH\n");
  }
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
#ifdef __ExoPC__
void
sdattach(struct scsibus_attach_args *sa)
{
  struct sd_softc *sd;
  struct disk_parms *dp;

  MP_QUEUELOCK_GET(GQLOCK(SYSINFO_LOCK));
  if (SYSINFO_GET(si_ndevs) == SD_MAXDEVS)
  {
    printf("sd%d: SD_MAXDEVS is only %d!\n", SYSINFO_GET(si_ndevs), SD_MAXDEVS);
    MP_QUEUELOCK_RELEASE(GQLOCK(SYSINFO_LOCK));
    return;
  }
  sd = SYSINFO_PTR_AT(si_devs,SYSINFO_GET(si_ndevs));
  sd->sc_devno = SYSINFO_GET(si_ndevs);
  INC_FIELD(si,Sysinfo,si_ndevs,1); // si->sd_ndevs++;
  MP_QUEUELOCK_RELEASE(GQLOCK(SYSINFO_LOCK));
  dp = &sd->params;

  /*
   * Store information needed to contact our base driver
   */
  sd->sc_link = sa->sa_sc_link;
  sd->sc_link->device = &sd_switch;
  sd->sc_link->device_softc = sd;
  if (sd->sc_link->openings > SDOUTSTANDING)
    sd->sc_link->openings = SDOUTSTANDING;

  /*
   * Note if this device is ancient.  This is used in sdminphys().
   */
  if ((sa->sa_inqbuf->version & SID_ANSII) == 0)
    sd->flags |= SDF_ANCIENT;

  /*
   * Use the subdriver to request information regarding
   * the drive. We cannot use interrupts yet, so the
   * request must specify this.
   */
  printf("sd%d: ", sd->sc_devno);
  if (scsi_start(sd->sc_link, SSS_START,
		 SCSI_AUTOCONF | SCSI_IGNORE_ILLEGAL_REQUEST |
		 SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT) ||
      sd_get_parms(sd, SCSI_AUTOCONF) != 0)
    printf("drive offline\n");
  else
  {
    printf("%ldMB, %d cyl, %d head, %d sec, %d bytes/sec\n",
	   dp->disksize / (1048576 / dp->blksize), dp->cyls,
	   dp->heads, dp->sectors, dp->blksize);
    sd->secsize = dp->blksize;
    sd->size = dp->cyls * dp->heads * dp->sectors;

     if (SYSINFO_GET(si_ndisks) < MAX_DISKS)
     {
       struct disk* d = SYSINFO_PTR_AT(si_disks, SYSINFO_GET(si_ndisks));

       d->d_id = sd->sc_devno;
       d->d_dev = sd->sc_devno;      
       d->d_strategy = sdstrategy;
       d->d_bshift = ffs(sd->secsize) - 1;
       if (sd->secsize != (1 << (d->d_bshift)))
 	panic ("sd%d: block size %d is not power of 2",
 	       sd->sc_devno, sd->secsize);
       d->d_bsize = sd->secsize;
       d->d_bmod = sd->secsize - 1;
       d->d_cyls = dp->cyls;
       d->d_heads = dp->heads;
       d->d_sectors = dp->sectors;
       d->d_size = sd->size;
       d->d_part_off = 0;	
       d->d_readcnt = 0;
       d->d_readsectors = 0;
       d->d_writesectors = 0;
       d->d_queuedReads = 0;
       d->d_ongoingReads = 0;
       d->d_writecnt = 0;
       d->d_queuedWrites = 0;
       d->d_ongoingWrites = 0;
       d->d_intrcnt = 0;

       MP_QUEUELOCK_GET(GQLOCK(SYSINFO_LOCK));
       INC_FIELD(si,Sysinfo,si_ndisks,1); // si->si_ndisks++;
       MP_QUEUELOCK_RELEASE(GQLOCK(SYSINFO_LOCK));
     } else {
        panic ("too many disks");
     }

    sd->sc_link->flags |= SDEV_MEDIA_LOADED;
  }
}


#else
void
sdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sd_softc *sd = (void *)self;
	struct disk_parms *dp = &sd->params;
	struct scsibus_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("sdattach: "));

	/*
	 * Store information needed to contact our base driver
	 */
	sd->sc_link = sc_link;
	sc_link->device = &sd_switch;
	sc_link->device_softc = sd;
	if (sc_link->openings > SDOUTSTANDING)
		sc_link->openings = SDOUTSTANDING;

	/*
	 * Initialize and attach the disk structure.
	 */
	sd->sc_dk.dk_driver = &sddkdriver;
	sd->sc_dk.dk_name = sd->sc_dev.dv_xname;
	disk_attach(&sd->sc_dk);

#if !defined(i386)
	dk_establish(&sd->sc_dk, &sd->sc_dev);		/* XXX */
#endif

	/*
	 * Note if this device is ancient.  This is used in sdminphys().
	 */
	if ((sa->sa_inqbuf->version & SID_ANSII) == 0)
		sd->flags |= SDF_ANCIENT;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	printf("\n");
	printf("%s: ", sd->sc_dev.dv_xname);
	if (scsi_start(sd->sc_link, SSS_START,
	    SCSI_AUTOCONF | SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT) ||
	    sd_get_parms(sd, SCSI_AUTOCONF) != 0)
		printf("drive offline\n");
	else
	        printf("%ldMB, %d cyl, %d head, %d sec, %d bytes/sec\n",
		    dp->disksize / (1048576 / dp->blksize), dp->cyls,
		    dp->heads, dp->sectors, dp->blksize);
}
#endif

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
sdlock(sd)
	struct sd_softc *sd;
{
#ifndef __ExoPC__
	int error;
#endif

	while ((sd->flags & SDF_LOCKED) != 0) {
		sd->flags |= SDF_WANTED;
#ifdef __ExoPC__
		sched_runnext();
#else
		if ((error = tsleep(sd, PRIBIO | PCATCH, "sdlck", 0)) != 0)
			return error;
#endif
	}
	sd->flags |= SDF_LOCKED;
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
sdunlock(sd)
	struct sd_softc *sd;
{

	sd->flags &= ~SDF_LOCKED;
	if ((sd->flags & SDF_WANTED) != 0) {
		sd->flags &= ~SDF_WANTED;
		wakeup(sd);
	}
}

#ifndef __ExoPC__
/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
int
sdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct sd_softc *sd;
	struct scsi_link *sc_link;
	int unit, part;
	int error;

	unit = SDUNIT(dev);
	if (unit >= sd_cd.cd_ndevs)
		return ENXIO;
	sd = sd_cd.cd_devs[unit];
	if (!sd)
		return ENXIO;

	sc_link = sd->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("sdopen: dev=0x%x (unit %d (of %d), partition %d)\n", dev, unit,
	    sd_cd.cd_ndevs, SDPART(dev)));

	if ((error = sdlock(sd)) != 0)
		return error;

	if (sd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		/* Check that it is still responding and ok. */
		error = scsi_test_unit_ready(sc_link,
					     SCSI_IGNORE_ILLEGAL_REQUEST |
					     SCSI_IGNORE_MEDIA_CHANGE |
					     SCSI_IGNORE_NOT_READY);
		if (error)
			goto bad3;

		/* Start the pack spinning if necessary. */
		error = scsi_start(sc_link, SSS_START,
				   SCSI_IGNORE_ILLEGAL_REQUEST |
				   SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT);
		if (error)
			goto bad3;

		sc_link->flags |= SDEV_OPEN;

		/* Lock the pack in. */
		error = scsi_prevent(sc_link, PR_PREVENT,
				     SCSI_IGNORE_ILLEGAL_REQUEST |
				     SCSI_IGNORE_MEDIA_CHANGE);
		if (error)
			goto bad;

		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			sc_link->flags |= SDEV_MEDIA_LOADED;

			/* Load the physical device parameters. */
			if (sd_get_parms(sd, 0) != 0) {
				error = ENXIO;
				goto bad2;
			}
			SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));

			/* Load the partition info if not already loaded. */
			sdgetdisklabel(sd);
			SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel loaded "));
		}
	}

	part = SDPART(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= sd->sc_dk.dk_label->d_npartitions ||
	     sd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		sd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		sd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	sd->sc_dk.dk_openmask = sd->sc_dk.dk_copenmask | sd->sc_dk.dk_bopenmask;

	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	sdunlock(sd);
	return 0;

bad2:
	sc_link->flags &= ~SDEV_MEDIA_LOADED;

bad:
	if (sd->sc_dk.dk_openmask == 0) {
		scsi_prevent(sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
		sc_link->flags &= ~SDEV_OPEN;
	}

bad3:
	sdunlock(sd);
	return error;
}

/*
 * close the device.. only called if we are the LAST occurence of an open
 * device.  Convenient now but usually a pain.
 */
int 
sdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(dev)];
	int part = SDPART(dev);
	int error;

	if ((error = sdlock(sd)) != 0)
		return error;

	switch (fmt) {
	case S_IFCHR:
		sd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		sd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sd->sc_dk.dk_openmask = sd->sc_dk.dk_copenmask | sd->sc_dk.dk_bopenmask;

	if (sd->sc_dk.dk_openmask == 0) {
		/* XXXX Must wait for I/O to complete! */

		scsi_prevent(sd->sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY);
		sd->sc_link->flags &= ~(SDEV_OPEN|SDEV_MEDIA_LOADED);
	}

	sdunlock(sd);
	return 0;
}
#endif /* !__ExoPC__ */

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
void
sdstrategy(bp)
	struct buf *bp;
{
#ifdef __ExoPC__
	struct sd_softc *sd;
        struct disk *sidisk;
#else
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(bp->b_dev)];
#endif
	int s;

	if (!(bp->b_flags & B_SCATGATH)) bp->b_sgtot = bp->b_bcount;
#ifdef __ExoPC__
        sidisk = SYSINFO_PTR_AT(si_disks, bp->b_dev);
	if (!(bp->b_flags & B_SCSICMD) && !(bp->b_flags & B_ABSOLUTE)) {
	  bp->b_blkno += sidisk->d_part_off;
	  bp->b_dev = sidisk->d_dev;
	}
	sd = SYSINFO_PTR_AT(si_devs,bp->b_dev);
#endif

#if  0
	printf("sdstrategy: %u bytes @ blk %qu, buffer %p\n",
	       bp->b_bcount, bp->b_blkno, bp->b_memaddr);
#endif
	SC_DEBUG(sd->sc_link, SDEV_DB2, ("sdstrategy "));
	SC_DEBUG(sd->sc_link, SDEV_DB1,
	    ("%u bytes @ blk %qu\n", bp->b_bcount, bp->b_blkno));

#ifdef __ExoPC__
   if ((bp->b_flags & B_SCSICMD) == 0) {
	if (bp->b_flags & B_READ) {
	   sidisk->d_readcnt++;
	   sidisk->d_readsectors += bp->b_sgtot / sd->secsize;
	} else {
	   sidisk->d_writecnt++;
	   sidisk->d_writesectors += bp->b_sgtot / sd->secsize;
	}
#endif
	/*
	 * The transfer must be a whole number of blocks.
	 */
#ifdef __ExoPC__
	if ((bp->b_sgtot % sd->secsize) != 0) {
		bp->b_flags = (bp->b_flags & ~B_ERROR) | B_EINVAL;
#else
	if ((bp->b_bcount % sd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
#endif
		goto bad;
	}
#ifdef __ExoPC__
    }
#endif
	/*
	 * If the device has been made invalid, error out
	 */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
#ifdef __ExoPC__
		bp->b_flags = (bp->b_flags & ~B_ERROR) | B_EIO;
#else
		bp->b_error = EIO;
#endif
		goto bad;
	}
	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_sgtot == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
#ifndef __ExoPC__
	if (SDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, &sd->dk_label,
	    bounds_check_with_label(bp, sd->sc_dk.dk_label,
	    (sd->flags & (SDF_WLABEL|SDF_LABELLING)) != 0) <= 0)
		goto done;
#endif

	s = splbio();

	/*
	 * Place it in the queue of disk activities for this disk
	 */
#ifdef __ExoPC__
	sched_addtoqueue(sd, bp);
#else
	disksort(&sd->buf_queue, bp);
#endif

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	sdstart(sd, 0);

	splx(s);
	return;

bad:
#ifndef __ExoPC__
	bp->b_flags |= B_ERROR;
#endif
done:
	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_sgtot;
#ifdef __ExoPC__
	sched_reqcomplete(sd, bp);
#else
	biodone(bp);
#endif
}

/*
 * sdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (sdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * sdstart() is called at splbio from sdstrategy and scsi_done
 */
void 
sdstart(v, flags)
	register void *v;
	int flags;
{
	register struct sd_softc *sd = v;
	register struct	scsi_link *sc_link = sd->sc_link;
	struct buf *bp = 0;
#ifndef __ExoPC__
	struct buf *dp;
#endif
	struct scsi_rw_big cmd_big;
	struct scsi_rw cmd_small;
	struct scsi_generic *cmdp;
	int blkno, nblks, cmdlen, error;
#ifndef __ExoPC__
	struct partition *p;
#endif

	SC_DEBUG(sc_link, SDEV_DB2, ("sdstart "));
	/*
	 * Check if the device has room for another command
	 */
	while (sc_link->openings > 0) {
		/*
		 * there is excess capacity, but a special waits
		 * It'll need the adapter as soon as we clear out of the
		 * way and let it run (user level wait).
		 */
		if (sc_link->flags & SDEV_WAITING) {
			sc_link->flags &= ~SDEV_WAITING;
			wakeup((caddr_t)sc_link);
			return;
		}

		/*
		 * See if there is a buf with work for us to do..
		 */
#ifdef __ExoPC__
		if ((bp = sched_selectnext(sd)) == NULL)
		  return;
#else
		dp = &sd->buf_queue;
		if ((bp = dp->b_actf) == NULL)	/* yes, an assign */
			return;
		dp->b_actf = bp->b_actf;
#endif

		/*
		 * If the device has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-opened
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
#ifdef __ExoPC__
			bp->b_flags = (bp->b_flags & ~B_ERROR) | B_EIO;
			sched_reqcomplete(sd, bp);
#else
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			biodone(bp);
#endif
			continue;
		}

#ifdef __ExoPC__
	    if (bp->b_flags & B_SCSICMD) {
		/* raw scsi command -- just pass it down */
		struct scsicmd *scsicmd = (struct scsicmd *) bp->b_memaddr;
		error = scsi_scsi_cmd (sc_link, scsicmd->scsi_cmd,
			   scsicmd->cmdlen, scsicmd->data_addr,
			   scsicmd->datalen, scsicmd->retries, scsicmd->timeout,
			   bp, (flags | scsicmd->flags));
	    } else {
#endif
		/*
		 * We have a buf, now we should make a command
		 *
		 * First, translate the block to absolute and put it in terms
		 * of the logical blocksize of the device.
		 */
		blkno =
#ifdef __ExoPC__
		    bp->b_blkno;
#else
		    bp->b_blkno / (sd->secsize / DEV_BSIZE);
#endif
#ifndef __ExoPC__
		if (SDPART(bp->b_dev) != RAW_PART) {
		     p = &sd->sc_dk.dk_label->d_partitions[SDPART(bp->b_dev)];
		     blkno += p->p_offset;
		}
#endif
#ifdef __ExoPC__
		nblks = howmany(bp->b_sgtot, sd->secsize);
#else
		nblks = howmany(bp->b_bcount, sd->sc_dk.dk_label->d_secsize);
#endif

		/*
		 *  Fill out the scsi command.  If the transfer will
		 *  fit in a "small" cdb, use it.
		 */
		if (((blkno & 0x1fffff) == blkno) &&
		    ((nblks & 0xff) == nblks)) {
			/*
			 * We can fit in a small cdb.
			 */
			bzero(&cmd_small, sizeof(cmd_small));
			cmd_small.opcode = (bp->b_flags & B_READ) ?
			    READ_COMMAND : WRITE_COMMAND;
			_lto3b(blkno, cmd_small.addr);
			cmd_small.length = nblks & 0xff;
			cmdlen = sizeof(cmd_small);
			cmdp = (struct scsi_generic *)&cmd_small;
		} else {
			/*
			 * Need a large cdb.
			 */
			bzero(&cmd_big, sizeof(cmd_big));
			cmd_big.opcode = (bp->b_flags & B_READ) ?
			    READ_BIG : WRITE_BIG;
			_lto4b(blkno, cmd_big.addr);
			_lto2b(nblks, cmd_big.length);
			cmdlen = sizeof(cmd_big);
			cmdp = (struct scsi_generic *)&cmd_big;
		}

#ifndef __ExoPC__
		/* Instrumentation. */
		disk_busy(&sd->sc_dk);
#endif

		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		error = scsi_scsi_cmd(sc_link, cmdp, cmdlen,
#ifdef __ExoPC__
		    bp->b_memaddr, bp->b_sgtot,
#else
		    (u_char *)bp->b_data, bp->b_bcount,
#endif
		    SDRETRIES, 10000, bp, flags |
#ifdef __ExoPC__
		    ((bp->b_flags & B_KERNEL) ? SCSI_POLL : 0) |
#endif
		    ((bp->b_flags & B_READ) ? SCSI_DATA_IN : SCSI_DATA_OUT));

#ifdef __ExoPC__
	    /* end of SCSI command vs. not */
	    }
#endif

		if (error)
#ifdef __ExoPC__
			printf("sd%d: not queued, error %d\n",
			    sd->sc_devno, error);
#else
			printf("%s: not queued, error %d\n",
			    sd->sc_dev.dv_xname, error);
#endif
	}
}


int
sddone(xs, complete)
	struct scsi_xfer *xs;
	int complete;
{
	struct sd_softc *sd = xs->sc_link->device_softc;

	if (complete && (xs->bp != NULL))
#ifdef __ExoPC__
	{
		struct buf *bp;
		bp = xs->bp;
		sched_reqcomplete(sd, bp);
	}
#else
		disk_unbusy(&sd->sc_dk, (xs->bp->b_bcount - xs->bp->b_resid));
#endif

	return (0);
}

void
sdminphys(bp)
	struct buf *bp;
{
#ifdef __ExoPC__
        struct disk* d = SYSINFO_PTR_AT(si_disks, bp->b_dev);
	struct sd_softc *sd = SYSINFO_PTR_AT(si_devs,d->d_dev);
#else
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(bp->b_dev)];
#endif
	long max;

	/*
	 * If the device is ancient, we want to make sure that
	 * the transfer fits into a 6-byte cdb.
	 *
	 * XXX Note that the SCSI-I spec says that 256-block transfers
	 * are allowed in a 6-byte read/write, and are specified
	 * by settng the "length" to 0.  However, we're conservative
	 * here, allowing only 255-block transfers in case an
	 * ancient device gets confused by length == 0.  A length of 0
	 * in a 10-byte read/write actually means 0 blocks.
	 */
	if (sd->flags & SDF_ANCIENT) {
#ifdef __ExoPC__
		max = sd->secsize * 0xff;
#else
		max = sd->sc_dk.dk_label->d_secsize * 0xff;
#endif

		if (bp->b_sgtot > max)
			bp->b_sgtot = max;
	}

	(*sd->sc_link->adapter->scsi_minphys)(bp);
}

#ifndef __ExoPC__
int
sdread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(sdstrategy, NULL, dev, B_READ, sdminphys, uio));
}

int
sdwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(sdstrategy, NULL, dev, B_WRITE, sdminphys, uio));
}

/*
 * Perform special action on behalf of the user
 * Knows about the internals of this device
 */
int
sdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(dev)];
	int error;

	SC_DEBUG(sd->sc_link, SDEV_DB2, ("sdioctl 0x%lx ", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) == 0)
		return EIO;

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = *(sd->sc_dk.dk_label);
		return 0;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = sd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &sd->sc_dk.dk_label->d_partitions[SDPART(dev)];
		return 0;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = sdlock(sd)) != 0)
			return error;
		sd->flags |= SDF_LABELLING;

		error = setdisklabel(sd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*sd->sc_dk.dk_openmask : */0,
		    sd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(SDLABELDEV(dev),
				    sdstrategy, sd->sc_dk.dk_label,
				    sd->sc_dk.dk_cpulabel);
		}

		sd->flags &= ~SDF_LABELLING;
		sdunlock(sd);
		return error;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *)addr)
			sd->flags |= SDF_WLABEL;
		else
			sd->flags &= ~SDF_WLABEL;
		return 0;

	case DIOCLOCK:
		return scsi_prevent(sd->sc_link,
		    (*(int *)addr) ? PR_PREVENT : PR_ALLOW, 0);

	case DIOCEJECT:
		return ((sd->sc_link->flags & SDEV_REMOVABLE) == 0 ? ENOTTY :
		    scsi_start(sd->sc_link, SSS_STOP|SSS_LOEJ, 0));

	default:
		if (SDPART(dev) != RAW_PART)
			return ENOTTY;
		return scsi_do_ioctl(sd->sc_link, dev, cmd, addr, flag, p);
	}

#ifdef DIAGNOSTIC
	panic("sdioctl: impossible");
#endif
}
#endif /* !__ExoPC__ */

#ifndef __ExoPC__
/*
 * Load the label information on the named device
 */
void
sdgetdisklabel(sd)
	struct sd_softc *sd;
{
	struct disklabel *lp = sd->sc_dk.dk_label;
	char *errstring;

	bzero(lp, sizeof(struct disklabel));
	bzero(sd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	lp->d_secsize = sd->params.blksize;
	lp->d_ntracks = sd->params.heads;
	lp->d_nsectors = sd->params.sectors;
	lp->d_ncylinders = sd->params.cyls;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it (?) */
	}

	strncpy(lp->d_typename, "SCSI disk", 16);
	lp->d_type = DTYPE_SCSI;
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = sd->params.disksize;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/*
	 * Call the generic disklabel extraction routine
	 */
	errstring = readdisklabel(MAKESDDEV(0, sd->sc_dev.dv_unit, RAW_PART),
				  sdstrategy, lp, sd->sc_dk.dk_cpulabel);
	if (errstring) {
		printf("%s: %s\n", sd->sc_dev.dv_xname, errstring);
		return;
	}
}
#endif /* !__ExoPC__ */

/*
 * Tell the device to map out a defective block
 */
int
sd_reassign_blocks(sd, blkno)
	struct sd_softc *sd;
	u_long blkno;
{
	struct scsi_reassign_blocks scsi_cmd;
	struct scsi_reassign_blocks_data rbdata;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&rbdata, sizeof(rbdata));
	scsi_cmd.opcode = REASSIGN_BLOCKS;

	_lto2b(sizeof(rbdata.defect_descriptor[0]), rbdata.length);
	_lto4b(blkno, rbdata.defect_descriptor[0].dlbaddr);

	return scsi_scsi_cmd(sd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)&rbdata, sizeof(rbdata), SDRETRIES,
	    5000, NULL, SCSI_DATA_OUT);
}

/*
 * Get the scsi driver to send a full inquiry to the * device and use the
 * results to fill out the disk parameter structure.
 */
int
sd_get_parms(sd, flags)
	struct sd_softc *sd;
	int flags;
{
	struct disk_parms *dp = &sd->params;
	struct scsi_mode_sense scsi_cmd;
	struct scsi_mode_sense_data {
		struct scsi_mode_header header;
		struct scsi_blk_desc blk_desc;
		union disk_pages pages;
	} scsi_sense;
	u_long sectors;

	/*
	 * do a "mode sense page 4"
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SENSE;
	scsi_cmd.page = 4;
	scsi_cmd.length = 0x20;
	/*
	 * If the command worked, use the results to fill out
	 * the parameter structure
	 */
	if (scsi_scsi_cmd(sd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)&scsi_sense, sizeof(scsi_sense),
	    SDRETRIES, 6000, NULL, flags | SCSI_DATA_IN) != 0) {
#ifdef __ExoPC__
		printf("sd%d: could not mode sense (4)", sd->sc_devno);
#else
		printf("%s: could not mode sense (4)", sd->sc_dev.dv_xname);
#endif
	fake_it:
		printf("; using fictitious geometry\n");
		/*
		 * use adaptec standard fictitious geometry
		 * this depends on which controller (e.g. 1542C is
		 * different. but we have to put SOMETHING here..)
		 */
		sectors = scsi_size(sd->sc_link, flags);
		dp->heads = 64;
		dp->sectors = 32;
		dp->cyls = sectors / (64 * 32);
		dp->blksize = 512;
		dp->disksize = sectors;
	} else {
		SC_DEBUG(sd->sc_link, SDEV_DB3,
		    ("%d cyls, %d heads, %d precomp, %d red_write, %d land_zone\n",
		    _3btol(scsi_sense.pages.rigid_geometry.ncyl),
		    scsi_sense.pages.rigid_geometry.nheads,
		    _2btol(scsi_sense.pages.rigid_geometry.st_cyl_wp),
		    _2btol(scsi_sense.pages.rigid_geometry.st_cyl_rwc),
		    _2btol(scsi_sense.pages.rigid_geometry.land_zone)));

		/*
		 * KLUDGE!! (for zone recorded disks)
		 * give a number of sectors so that sec * trks * cyls
		 * is <= disk_size
		 * can lead to wasted space! THINK ABOUT THIS !
		 */
		dp->heads = scsi_sense.pages.rigid_geometry.nheads;
		dp->cyls = _3btol(scsi_sense.pages.rigid_geometry.ncyl);
		dp->blksize = _3btol(scsi_sense.blk_desc.blklen);

		if (dp->heads == 0 || dp->cyls == 0) {
#ifdef __ExoPC__
			printf("sd%d: mode sense (4) returned nonsense",
			    sd->sc_devno);
#else
			printf("%s: mode sense (4) returned nonsense",
			    sd->sc_dev.dv_xname);
#endif
			goto fake_it;
		}

		if (dp->blksize == 0)
			dp->blksize = 512;

		sectors = scsi_size(sd->sc_link, flags);
		dp->disksize = sectors;
		sectors /= (dp->heads * dp->cyls);
		dp->sectors = sectors;	/* XXX dubious on SCSI */
	}

	return 0;
}

#ifndef __ExoPC__
int
sdsize(dev)
	dev_t dev;
{
	struct sd_softc *sd;
	int part;
	int size;

	if (sdopen(dev, 0, S_IFBLK, NULL) != 0)
		return -1;
	sd = sd_cd.cd_devs[SDUNIT(dev)];
	part = SDPART(dev);
	if (sd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = sd->sc_dk.dk_label->d_partitions[part].p_size;
	if (sdclose(dev, 0, S_IFBLK, NULL) != 0)
		return -1;
	return size;
}
#endif /* !__ExoPC__ */

#ifndef __ExoPC__
#ifndef __BDEVSW_DUMP_OLD_TYPE
/* #define SD_DUMP_NOT_TRUSTED if you just want to watch */
static struct scsi_xfer sx;
static int sddoingadump;

/*
 * dump all of physical memory into the partition specified, starting
 * at offset 'dumplo' into the partition.
 */
int
sddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	struct sd_softc *sd;	/* disk unit to do the I/O */
	struct disklabel *lp;	/* disk's disklabel */
	int	unit, part;
	int	sectorsize;	/* size of a disk sector */
	int	nsects;		/* number of sectors in partition */
	int	sectoff;	/* sector offset of partition */
	int	totwrt;		/* total number of sectors left to write */
	int	nwrt;		/* current number of sectors to write */
	struct scsi_rw_big cmd;	/* write command */
	struct scsi_xfer *xs;	/* ... convenience */
	int	retval;

	/* Check if recursive dump; if so, punt. */
	if (sddoingadump)
		return EFAULT;

	/* Mark as active early. */
	sddoingadump = 1;

	unit = SDUNIT(dev);	/* Decompose unit & partition. */
	part = SDPART(dev);

	/* Check for acceptable drive number. */
	if (unit >= sd_cd.cd_ndevs || (sd = sd_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	/* Make sure it was initialized. */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) != SDEV_MEDIA_LOADED)
		return ENXIO;

	/* Convert to disk sectors.  Request must be a multiple of size. */
	lp = sd->sc_dk.dk_label;
	sectorsize = lp->d_secsize;
	if ((size % sectorsize) != 0)
		return EFAULT;
	totwrt = size / sectorsize;
	blkno = dbtob(blkno) / sectorsize;	/* blkno in DEV_BSIZE units */

	nsects = lp->d_partitions[part].p_size;
	sectoff = lp->d_partitions[part].p_offset;

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + totwrt) > nsects))
		return EINVAL;

	/* Offset block number to start of partition. */
	blkno += sectoff;

	xs = &sx;

	while (totwrt > 0) {
		nwrt = totwrt;		/* XXX */
#ifndef	SD_DUMP_NOT_TRUSTED
		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.opcode = WRITE_BIG;
		_lto4b(blkno, cmd.addr);
		_lto2b(nwrt, cmd.length);
		/*
		 * Fill out the scsi_xfer structure
		 *    Note: we cannot sleep as we may be an interrupt
		 * don't use scsi_scsi_cmd() as it may want
		 * to wait for an xs.
		 */
		bzero(xs, sizeof(sx));
		xs->flags |= SCSI_AUTOCONF | INUSE | SCSI_DATA_OUT;
		xs->sc_link = sd->sc_link;
		xs->retries = SDRETRIES;
		xs->timeout = 10000;	/* 10000 millisecs for a disk ! */
		xs->cmd = (struct scsi_generic *)&cmd;
		xs->cmdlen = sizeof(cmd);
		xs->resid = nwrt * sectorsize;
		xs->error = XS_NOERROR;
		xs->bp = 0;
		xs->data = va;
		xs->datalen = nwrt * sectorsize;

		/*
		 * Pass all this info to the scsi driver.
		 */
		retval = (*(sd->sc_link->adapter->scsi_cmd)) (xs);
		if (retval != COMPLETE)
			return ENXIO;
#else	/* SD_DUMP_NOT_TRUSTED */
		/* Let's just talk about this first... */
		printf("sd%d: dump addr 0x%x, blk %d\n", unit, va, blkno);
		delay(500 * 1000);	/* half a second */
#endif	/* SD_DUMP_NOT_TRUSTED */

		/* update block count */
		totwrt -= nwrt;
		blkno += nwrt;
		va += sectorsize * nwrt;
	}
	sddoingadump = 0;
	return 0;
}
#else	/* __BDEVSW_DUMP_NEW_TYPE */
int
sddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}
#endif	/* __BDEVSW_DUMP_NEW_TYPE */
#endif /* !__ExoPC__ */

#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

