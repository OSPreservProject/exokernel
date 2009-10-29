
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

/* System V shared memory support.
   
   Routines for creating, attaching (mapping), detaching (unmapping), and
   destroying regions of shared memory. 
   
   Exports:
   
    shmgets
    shmat
    shmdt
    shmctl 

   Known Bugs:
  
    Process that creates a segment must be the one that deallocate's it
    since aegis cannot currently do displaced deallocation.

    Are you supposed to be able to do a shmget on a segment
    that is marked for deletion? SunOS appears to allow this.

    When we share pages, we always give the person write access to
    the pages (and assume that we have write access).

*/

#include <exos/callcount.h>
#include <exos/conf.h>
#include <xok/defs.h>
#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <exos/mallocs.h>
#include <exos/shm.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <exos/process.h>
#include <exos/cap.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

/* parameters */
#define SHMSIZE 2048		/* in kilobytes -- keep a multiple of 4 */
#define SHMMNI 32		/* number of segments per system */
#define SHMSEG 32		/* number of segments per process */
#define SHMMIN 1		/* min segment size */
#define SHMMAX (SHMSIZE * 1024)	/* max segment size */
#define SHMPAGES (SHMMAX / 4096) /* number physical pages per segment */

struct SegmentInfo {
  Pte PhysicalPages[SHMPAGES]; /* pages for this region */
  int NumberPages;		/* number of pages in PhysicalPages */
  int ReferenceCount;		/* number of attaches */
  int ScheduledForDeletion;	/* true if last process to detach should delete */
  key_t Key;			/* "name" of ths segment */
};
typedef struct SegmentInfo SegmentInfo;

/* remember where we have segments mapped */

#define IPC_EMPTYSEG -2

static struct LocalSegmentInfo {
  char *Virtual;		/* where the segment is mapped in */
  int Segment;			/* which segment it is */
} LocalSegmentInfo[SHMSEG];;


/* prototypes for internal functions */
static int GetNamedSegment (key_t Key);
static int GetFreeSegment ();
static int AllocateSegment (int SegmentID, int Size, key_t Key);
static int AllocateSegmentMemory (int SegmentID, char *va);
static int AddToLocalSegmentInfo (char *Virtual, int Segment);
static int RemoveFromLocalSegmentInfo (char *Virtual);
static int DeallocateSegment (SegmentInfo *Seg);
static int MapSegmentInfo (u_int k, int envid, int execonly);

static int IncShmRefCounts (u_int k, int envid, int pid);

/* called once for the entire system */
int InitializeSysVShmSys () {
  int Segment;
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  

  /* allocate memory for the GlobalSegmentInfo */
  if (__vm_alloc_region(GLOBAL_SEGMENT_TABLE, sizeof (SegmentInfo) * SHMMNI,
			CAP_WORLD, PG_U | PG_W | PG_P | PG_SHARED) < 0) 
    return -1;

  StaticAssert(sizeof (SegmentInfo) * SHMMNI <= GLOBAL_SEGMENT_TABLE_SZ);
  /* mark all the segments as free */
  for (Segment = 0; Segment < SHMMNI; Segment++) {
    GlobalSegmentInfo[Segment].Key = IPC_EMPTYSEG;
  }

  return (0);
}

/* called when this process starts */

void SysVShmExecInit () {
  int i;
     
  for (i = 0; i < SHMSEG; i++) {
    LocalSegmentInfo[i].Virtual = (char *)0;
  }

  OnExec (MapSegmentInfo);
  OnFork (IncShmRefCounts);
}

/* When we exec a new process we have to explicity share the global
   segment info so that the new process will then be able to get
   access to the other shared segments. */
static int MapSegmentInfo (u_int k, int envid, int execonly) {
  if (__vm_share_region(GLOBAL_SEGMENT_TABLE, sizeof (SegmentInfo) * SHMMNI,
			CAP_WORLD, k, envid, GLOBAL_SEGMENT_TABLE) != 0)
    return -1;

  return (0);
}

/* called at end of process execution to release any segments
   we have attached */

void CleanupSysVShm () {
  int i;

  /* go through our LocalSegmentInfo table and detach anything
     we have attached */
     
  for (i = 0; i < SHMSEG; i++) {
    if (LocalSegmentInfo[i].Virtual != (char *)0) {
      assert (shmdt (LocalSegmentInfo[i].Virtual) == 0);
    }
  }
}

/* Create a shared segment, but do not map it it. Key is either IPC_PRIVATE
   or it specifies the name of a new segment. */

int
shmget (key_t Key, int Size, int Shmflg)
{
  int Segment;
     
  OSCALLENTER(OSCALL_shmget);
  /* Look for a segment named Key. If Key == IPC_PRIVATE, we create a new
     segment. If the segment doesn't exist and IPC_CREAT is specified
     the segment will be created. If the segment does exist and IPC_EXCL
     and IPC_CREAT are specified, an error will be returned. Otherwise,
     the segment id of the segment will be returned. */

  /* handle IPC_PRIVATE here for simplicity */
  if (Key == IPC_PRIVATE) {
    /* find a segment to allocate */
    Segment = GetFreeSegment ();
    if (Segment == -1) {
      errno = ENOSPC;
      OSCALLEXIT(OSCALL_shmget);
      return (-1);
    }
    /* allocate memory for segment. Sets errno on failure. */
    if (AllocateSegment (Segment, Size, Key) == -1) {
      OSCALLEXIT(OSCALL_shmget);
      return (-1);
    }
  } else {
    /* see if we can find this segment */
    Segment = GetNamedSegment (Key);

    if (Segment == -1) {
      /* if it's not there see if we should create it */
      if (Shmflg & IPC_CREAT) {
	/* find a segment to allocate */
	Segment = GetFreeSegment ();
	if (Segment == -1) {
	  errno = ENOSPC;
	  OSCALLEXIT(OSCALL_shmget);
	  return (-1);
	}
	/* allocate memory for segment. Sets errno on failure. */
	if (AllocateSegment (Segment, Size, Key) == -1) {
	  OSCALLEXIT(OSCALL_shmget);
	  return (-1);
	}	       
      } else {
	/* entry not found and not creating it */
	errno = ENOENT;
	OSCALLEXIT(OSCALL_shmget);
	return (-1);
      }
    } else {
      /* barf if IPC_CREAT and IPC_EXCL */
      if ((Shmflg & IPC_CREAT) && (Shmflg & IPC_EXCL)) {
	errno = EEXIST;
	OSCALLEXIT(OSCALL_shmget);
	return (-1);
      }
      /* otherwise we're set */
    }
  }

  OSCALLEXIT(OSCALL_shmget);
  return (Segment);
}

/* lookup the segment named Key in the global segment table. Return it's index
   if it exists and -1 otherwise. */

static int GetNamedSegment (key_t Key) {
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;
  int Segment;
     
  for (Segment = 0; Segment < SHMMNI; Segment++) {
    if (GlobalSegmentInfo[Segment].Key == Key && 
	!GlobalSegmentInfo[Segment].ScheduledForDeletion) {
      return (Segment);
    }
  }
     
  return (-1);
}

/* find a free slot in the segment table and return it's index or -1 if
   there aren't any free slots */

static int GetFreeSegment () {
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
  int Segment;
     
  for (Segment = 0; Segment < SHMMNI; Segment++) {
    if (GlobalSegmentInfo[Segment].Key == IPC_EMPTYSEG) {
      return (Segment);
    }
  }
     
  return (-1);
}

/* Fork causes us to get a mapping of all currently mapped shared
   segments. Need to inc the ref counts for these. */

static int IncShmRefCounts (u_int k, int envid, int pid) {
     SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
     int CurrentSegment;     

     /* iterate through OUR local segment table and inc the ref count
	on each segment in the global table */
     
     for (CurrentSegment = 0; CurrentSegment < SHMSEG; CurrentSegment++) {
	  if (LocalSegmentInfo[CurrentSegment].Virtual != (char *)0) {
	    GlobalSegmentInfo[LocalSegmentInfo[CurrentSegment].Segment].
	      ReferenceCount++;
	  }
     }

     return (0);
}

/* Fill in SegmentInfo structure. */

static int AllocateSegment (int SegmentID, int Size, key_t Key) {
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
  SegmentInfo *Segment = &GlobalSegmentInfo[SegmentID];
  int NumPages;
     
  /* check size of request */
  if (Size < SHMMIN || Size > SHMMAX) {
    errno = EINVAL;
    return (-1);
  }
  NumPages = PGNO (PGROUNDUP (Size));
     
  Segment->ReferenceCount = 0;
  Segment->ScheduledForDeletion = 0;
  Segment->Key = Key;
  Segment->NumberPages = NumPages;

  return (0);
}

/* Allocate the memory for a segment and actually map it into our
   address space. This is done by the first process to attach
   the segment and is an artifact of xok not allowing memory
   to be allocated without being mapped. */

static int AllocateSegmentMemory (int SegmentID, char *va) {
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
  SegmentInfo *Segment = &GlobalSegmentInfo[SegmentID];
  int i;
  int NumPages = Segment->NumberPages;

  /* now allocate enough pages and stick them in the segment */
  if (__vm_alloc_region((u_int)va, Segment->NumberPages*NBPG, CAP_WORLD,
			PG_U | PG_W | PG_P | PG_SHARED) < 0) {
    errno = ENOMEM;
    return (-1);
  }

  for (i = 0; i < NumPages; i++)
    Segment->PhysicalPages[i] = vpt[PGNO(va) + i];
  
  return (0);
}  

/* attach (map) a segment to our address space. The segment must have
   already been created with shmget. */

void *
shmat(int Segment, const void *Virtual, int Shmflg)
{
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
  SegmentInfo *Seg;
  char *VirtualStart;
     
  OSCALLENTER(OSCALL_shmat);
  /* check parameters */
  if (Segment < 0 || Segment >= SHMMNI) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_shmat);
    return ((char *)-1);
  }

  Seg = &GlobalSegmentInfo[Segment];  
     
  /* make sure the named segment has been allocated */
  if (Seg->Key == IPC_EMPTYSEG || Seg->ScheduledForDeletion) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_shmat);
    return ((char *)-1);
  }

  /* caller didn't give us an address, so we should get one */
  if (Virtual == (char *)0) {
    Virtual = __malloc (Seg->NumberPages * NBPG);
  }
  VirtualStart = (void*)Virtual;

  /* if we're the first process to attach, need to allocate
     the mem. Otherwise, just map it in. */

  if (Seg->ReferenceCount == 0) {
    if (AllocateSegmentMemory (Segment, (void*)Virtual) != 0) {
      OSCALLEXIT(OSCALL_shmat);
      return ((char *)-1);
    }
  } else {
    Pte *ptes;
    int i;
    u_int num_completed;

    /* actually map the pages of the segment into our address space */
    if ((ptes = alloca(sizeof(Pte) * Seg->NumberPages)) == NULL) {
      errno = ENOMEM;
      OSCALLEXIT(OSCALL_shmat);
      return ((char *)-1);
    }

    for (i = Seg->NumberPages - 1; i >= 0; i--)
      ptes[i] = Seg->PhysicalPages[i];

    num_completed = 0;
    if (_exos_self_insert_pte_range(CAP_WORLD, ptes, Seg->NumberPages,
				    (u_int)Virtual, &num_completed, 0,
				    NULL) < 0) {
      errno = ENOMEM;
      OSCALLEXIT(OSCALL_shmat);
      return ((char *)-1);
    }
  }

  /* remember that we have this segment mapped in */
  if (AddToLocalSegmentInfo (VirtualStart, Segment) == -1) {
    errno = EMFILE;
    OSCALLEXIT(OSCALL_shmat);
    return ((char *)-1);
  }
  Seg->ReferenceCount++;
     
  OSCALLEXIT(OSCALL_shmat);
  return (VirtualStart);
}

/* add the pair (Virtual, Segment) to our local table of which
   segments we have mapped in. */

static int AddToLocalSegmentInfo (char *Virtual, int Segment) {
  int Current;
     
  for (Current = 0; Current < SHMSEG; Current++) {
    if (LocalSegmentInfo[Current].Virtual == (char *)0) {
      LocalSegmentInfo[Current].Virtual = Virtual;
      LocalSegmentInfo[Current].Segment = Segment;
      return (0);
    }
  }
     
  return (-1);
}

/* detach (unmap) a shared segment. The segment must have already
   been attached by shmat */

int
shmdt (const void *Virtual)
{
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
  SegmentInfo *Seg;
  int Segment;
     
  OSCALLENTER(OSCALL_shmdt);
  /* lookup the virtual address in our LocalSegmentInfo table
     to figure out if it's mapped and if so what it's segment id
     is. If it's found, it's removed. */
     
  Segment = RemoveFromLocalSegmentInfo ((void*)Virtual);
  if (Segment == -1) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_shmdt);
    return (-1);
  }
     
  /* unmap the memory */
  Seg = &GlobalSegmentInfo[Segment];
  assert(__vm_free_region((u_int)Virtual, Seg->NumberPages*NBPG, CAP_WORLD)
	 == 0);
     
  /* update the segment info */
  Seg->ReferenceCount--;
  assert (Seg->ReferenceCount >= 0);
  if (Seg->ScheduledForDeletion && Seg->ReferenceCount == 0) {
    DeallocateSegment (Seg);
  }
     
  OSCALLEXIT(OSCALL_shmdt);
  return (0);
}

/* remove the pair (Virtual, Segment) from our local table.
   Return the segment id if we find it, or -1. */

static int RemoveFromLocalSegmentInfo (char *Virtual) {
  int Current;
     
  for (Current = 0; Current < SHMSEG; Current++) {
    if (LocalSegmentInfo[Current].Virtual == Virtual) {
      LocalSegmentInfo[Current].Virtual = (char *)0;
      return (LocalSegmentInfo[Current].Segment);
    }
  }
     
  return (-1);
}

static int DeallocateSegment (SegmentInfo *Seg) {
  /* we're the last ones to use it, so deallocate it */
  Seg->Key = IPC_EMPTYSEG;
     
  return (0);
}

/* the ioctl of shared memory. We only support the IPC_RMID command
   right now. */

int
shmctl (int Segment, int Cmd, struct shmid_ds *Buf)
{
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
  SegmentInfo *Seg;
     
  OSCALLENTER(OSCALL_shmctl);
  /* check parameters */
  if (Segment < 0 || Segment >= SHMMNI) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_shmctl);
    return (-1);
  }
     
  Seg = &GlobalSegmentInfo[Segment];
  if (Seg->Key == IPC_EMPTYSEG || Seg->ScheduledForDeletion) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_shmctl);
    return (-1);
  }
     
  /* and actually do the operation */
  switch (Cmd) {
  case IPC_RMID: {
    Seg->ScheduledForDeletion = 1;
    if (Seg->ReferenceCount == 0) {
      DeallocateSegment (Seg);
    }
    break;
  }
  default:
    errno = EINVAL;
    OSCALLEXIT(OSCALL_shmctl);
    return (-1);
  }
     
  OSCALLEXIT(OSCALL_shmctl);
  return (0);
}
