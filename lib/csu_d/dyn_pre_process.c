
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

#include <exos/dyn_process.h>
#include <exos/vm-layout.h>
#include <exos/vm.h>
#include <exos/conf.h>

#include <fd/procaux.h>

#include <os/osdecl.h>

#include <sys/defs.h>
#include <sys/env.h>
#include <sys/pmap.h>
#include <sys/mmu.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
/* #include <sys/wk.h> */
#include <sys/sysinfo.h>
#include <fd/proc.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <xuser.h>

#include <sls_stub.h>

struct LocalSegmentInfo dyn_LocalSegmentInfo[SHMSEG];
char *dyn_Shared = (char *)DYNAMIC_MAP_REGION;

extern u_int dyn__envid;


int dyn_shmget (key_t Key, int Size, int Shmflg);
char *dyn_shmat (int Segment, char *Virtual, int Shmflg);
static int dyn_AllocateSegmentMemory (int SegmentID, char *va);
static int dyn_AddToLocalSegmentInfo (char *Virtual, int Segment);
void dyn_SysVShmExecInit ();
char *dyn_FindFreeVa (int);
static int dyn_IncShmRefCounts (u_int k, int envid, int pid);
static int dyn_MapSegmentInfo (u_int ke, int envid, int execonly);


proc_info *dyn_ProcInfo = (proc_info *)0;

struct proc_struct *dyn_current;

void PreExecInit () {
  int Segment;
  int Index;

  dyn_SysVShmExecInit ();

  /* ProcInfoInit */
  
  /* I assume that the proc table is always there, so the code to deal with
     Segment == -1 is taken out. So all we have to do is shmget and shmat
     both of which should work.  We don't have to initialize the proc_table
     so that code is removed as well*/
  Segment = dyn_shmget (PROC_INFO_KEY, sizeof (proc_info), 0);
  if ((dyn_ProcInfo = (proc_info *)dyn_shmat (Segment, (char *)0, 0)) == (proc_info * )-1) {
    dyn_ProcInfo = (proc_info *)0; /* so ProcessEnd won't try to use it */
    perror ("ProcInfoInit: Could not attach process table");
    exit(-1); }
 
  /* no children yet */
  for (Index = 0; Index < U_MAXCHILDREN; Index++) {
    UAREA.child_state[Index] = P_EMPTY;
  } 



  dyn_current = (struct proc_struct *) PROC_STRUCT;

}


int dyn_shmget (key_t Key, int Size, int Shmflg) {
  /* crippled version of shmget.  We know that the Key is not IPC_PRIVATE
     and that the Shmflg is 0 so we can dispense with most of the exception
     handling code and segment creation code */
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

char *dyn_shmat (int Segment, char *Virtual, int Shmflg) {
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;
  SegmentInfo *Seg;
  char *VirtualStart;
  int PhysPage;
  
  /* We know that: Segment is valid, allocated and !ScheduledForDeletion
     Virtual == (char *)0
     Shmflg == 0 */

  Seg = &GlobalSegmentInfo[Segment];

  Virtual = dyn_FindFreeVa (Seg->NumberPages * NBPG);
  slskprintf("FindFreeVa yields 0x");
  slskprinth((int)Virtual);
  slskprintf("\n");
  
  VirtualStart = Virtual;
  if (Seg->ReferenceCount == 0) {
    if (dyn_AllocateSegmentMemory (Segment, Virtual) != 0)
      return ((char *)-1);
  }  else {
    /* actually map the pages of the segment into our address space */
    for (PhysPage = 0; PhysPage < Seg->NumberPages; PhysPage++) {
      if (sys_self_insert_pte (0, Seg->PhysicalPages[PhysPage],
			       (unsigned )Virtual) < 0) {
	errno = ENOMEM;
	return ((char *)-1);
      }
    }
  }
  for (PhysPage = 0; PhysPage < Seg->NumberPages; PhysPage++) {
    slskprintf("Page Mapped to 0x");
    slskprinth((int)Virtual);
    slskprintf("\n");
    slskprintf("  0x");
    slskprinth(vpt[PGNO(Virtual)]);
    slskprintf("\n");
    Virtual += NBPG;
  }
  
  
  /* remember that we have this segment mapped in */
  if (dyn_AddToLocalSegmentInfo (VirtualStart, Segment) == -1) {
    errno = EMFILE;
    return ((char *)-1);
  }
  Seg->ReferenceCount++;
  
  return (VirtualStart);
}

static int dyn_AllocateSegmentMemory (int SegmentID, char *va) {
  SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;
  SegmentInfo *Segment = &GlobalSegmentInfo[SegmentID];
  int i;
  int NumPages = Segment->NumberPages;
  int tmp;
  
  /* now allocate enough pages and stick them in the segment */
  for (i = 0; i < NumPages; i++) {
    if ((tmp = sys_self_insert_pte (0, PG_U | PG_W | PG_P | PG_SHARED,
                                    (unsigned )va)) < 0) {
      errno = ENOMEM;
      return (-1);
    }
    Segment->PhysicalPages[i] = vpt[(unsigned )va >> PGSHIFT];
    va += NBPG;
  }
  return (0);
}

/* add the pair (Virtual, Segment) to our local table of which
   segments we have mapped in. */

static int dyn_AddToLocalSegmentInfo (char *Virtual, int Segment) {
  int Current;
  
  for (Current = 0; Current < SHMSEG; Current++) {
    if (dyn_LocalSegmentInfo[Current].Virtual == (char *)0) {
      dyn_LocalSegmentInfo[Current].Virtual = Virtual;
      dyn_LocalSegmentInfo[Current].Segment = Segment;
      return (0);
    }
  }
  
  return (-1);
}

char *dyn_FindFreeVa (int Size) {

  char *OldShared;
  slskprintf("dyn_FindFreeVa\n");
  OldShared = dyn_Shared;
  dyn_Shared += Size;
  /* assert (OldShared < (char *)END_DYNAMIC_MAP_REGION); */
  return (OldShared);
}

void dyn_SysVShmExecInit () {
  int i;
     
  for (i = 0; i < SHMSEG; i++) {
    dyn_LocalSegmentInfo[i].Virtual = (char *)0;
  }
 
}

static int dyn_IncShmRefCounts (u_int k, int envid, int pid) {
     SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
     int CurrentSegment;     

     /* iterate through OUR local segment table and inc the ref count
        on each segment in the global table */
     
     for (CurrentSegment = 0; CurrentSegment < SHMSEG; CurrentSegment++) {
          if (dyn_LocalSegmentInfo[CurrentSegment].Virtual != (char *)0) {
               GlobalSegmentInfo[dyn_LocalSegmentInfo[CurrentSegment].Segment].
                    ReferenceCount++;
          }
     }

     return (0);
}


/* When we exec a new process we have to explicity share the global
   segment info so that the new process will then be able to get
   access to the other shared segments. */

static int dyn_MapSegmentInfo (u_int ke, int envid, int execonly) {
     int NumPages = sizeof (SegmentInfo) * SHMMNI / NBPG + 1;     
     SegmentInfo *GlobalSegmentInfo = (SegmentInfo *)GLOBAL_SEGMENT_TABLE;  
     unsigned Virtual = (unsigned )GlobalSegmentInfo;

     /* printf("mapsegement info\n"); */
     while (NumPages--) {
       if (sys_insert_pte (0, vpt[Virtual >> PGSHIFT], Virtual, 
                           ke, envid) > 0) {
         return (-1);
       }
       Virtual += NBPG;
     }

     return (0);
}
